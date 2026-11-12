//! Athan playback.  Uses rodio on top of cpal/PipeWire/ALSA.
//!
//! A dedicated worker thread keeps the output stream alive and makes
//! "Stop Audio" reliable even while a long recitation is playing.  Audio is
//! queued as a playlist so consecutive files play without a gap.
//!
//! rodio is built with its `tracing` feature on purpose: without it, every
//! ALSA stream error is printed to stderr directly from the audio thread, so a
//! single dead device floods the terminal with identical lines.  Namaz has no
//! tracing subscriber, so those events are dropped and the watchdog below
//! reports the failure once.

use std::fs::File;
use std::io::BufReader;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::mpsc::{self, RecvTimeoutError, Sender};
use std::sync::{Mutex, OnceLock};
use std::thread;
use std::time::{Duration, Instant};

use rodio::{Decoder, OutputStream, OutputStreamHandle, Sink};

/// Device loss (Bluetooth headset unplugged, PipeWire restarted, resume from
/// suspend) surfaces as a stream error: cpal keeps polling the dead ALSA
/// descriptor and the sink stops draining.  Without a watchdog the playback
/// flag would stay set forever, the current recitation would be lost silently
/// and the UI would keep showing "playing".  Watch the playback position and
/// tear the stream down once it stops moving; the next Play reopens the
/// device.
const STALL_TIMEOUT: Duration = Duration::from_secs(3);

enum Command {
    Play(Vec<PathBuf>, f32),
    Stop,
    SetVolume(f32),
}

static TX: OnceLock<Mutex<Sender<Command>>> = OnceLock::new();
static WAVE_DIR: OnceLock<PathBuf> = OnceLock::new();
static PLAYING: AtomicBool = AtomicBool::new(false);
static VOLUME: AtomicU32 = AtomicU32::new(90);

/// Audio shipped with Namaz.  The public build includes one adhan, one
/// Bismillah and one short surah; users can add their own recordings through
/// the preferences dialog.
pub const ATHAN_SOUNDS: &[&str] = &["ADHAN"];
pub const QURAN_SOUNDS: &[&str] = &["ALIHLAS"];
pub const IQAMA_SOUNDS: &[&str] = &[];
pub const BISMILLAH_SOUNDS: &[&str] = &["BISMILLAH"];

pub const AUDIO_EXTENSIONS: &[&str] = &["wav", "ogg", "mp3", "flac"];

fn wave_dir() -> PathBuf {
    WAVE_DIR.get_or_init(resolve_wave_dir).clone()
}

fn resolve_wave_dir() -> PathBuf {
    if let Some(dir) = std::env::var_os("NAMAZ_WAVE_DIR") {
        return PathBuf::from(dir);
    }
    // Keep working with installations created by the old working title.
    if let Some(dir) = std::env::var_os("SALAAT_TIME_WAVE_DIR") {
        return PathBuf::from(dir);
    }
    if let Some(home) = std::env::var_os("HOME") {
        for relative in [".local/share/namaz/wave", ".local/share/salaat-time/wave"] {
            let p = PathBuf::from(&home).join(relative);
            if p.is_dir() {
                return p;
            }
        }
    }
    for absolute in ["/app/share/namaz/wave", "/app/share/salaat-time/wave"] {
        if Path::new(absolute).is_dir() {
            return PathBuf::from(absolute);
        }
    }

    // Portable/development layouts: look next to the executable first, then
    // relative to the manifest, then the current working directory.
    let mut candidates = Vec::new();
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            candidates.push(dir.join("../share/namaz/wave"));
            candidates.push(dir.join("share/namaz/wave"));
            candidates.push(dir.join("assets/audio"));
            candidates.push(dir.join("assets/salaattime-exe/wave"));
        }
    }
    let manifest = Path::new(env!("CARGO_MANIFEST_DIR"));
    candidates.push(manifest.join("../../assets/audio"));
    candidates.push(manifest.join("../../assets/salaattime-exe/wave"));
    if let Ok(cwd) = std::env::current_dir() {
        candidates.push(cwd.join("assets/audio"));
        candidates.push(cwd.join("assets/salaattime-exe/wave"));
        candidates.push(cwd.join("share/namaz/wave"));
    }
    for candidate in candidates {
        if candidate.is_dir() {
            return candidate;
        }
    }
    // Development fallback: the bundled recordings live next to the project.
    manifest.join("../../assets/audio")
}

fn find_audio_file(dir: &Path, name: &str) -> Option<PathBuf> {
    let direct = dir.join(name);
    if direct.is_file() {
        return Some(direct);
    }
    for extension in AUDIO_EXTENSIONS {
        let candidate = dir.join(format!("{name}.{extension}"));
        if candidate.is_file() {
            return Some(candidate);
        }
    }
    None
}

fn legacy_alias(name: &str) -> Option<&'static str> {
    match name {
        // Recordings from the old Salaat Time installation.
        "ALAQSA" | "CAIRO1" | "CAIRO2" | "DAMASCUS" | "FAJRMADINA" | "FAJRMAKKAH" | "MAKKAH" => {
            Some("ADHAN")
        }
        "BISMILLAH0" | "BISMILLAH1" | "BISMILLAH2" | "BISMILLAH3" | "BISMILLAH4" => {
            Some("BISMILLAH")
        }
        "ALFATIHA" | "LUQMAN" | "ALQAMAR" | "ALALA" | "ADDUHA" | "ASSARH" | "ALASR" | "ALMAUN"
        | "ALKAWTAR" | "ALKAFIRUNE" | "ALIHLAS" | "ALFALAQ" | "ANNAS" | "ALBAQARAH_155_157"
        | "ALBAQARAH_177" | "ALBAQARAH_238" | "AYAATALKURSI" | "ALBAQARAH_285_286" | "ALIMRAN"
        | "ALANAM" | "HUD" | "ALKAHF" | "ANNAML" | "ALAHZAB" | "ALHADID" | "ALALAQ" => {
            Some("ALIHLAS")
        }
        _ => None,
    }
}

pub fn sound_path(name: &str) -> Option<PathBuf> {
    let dir = wave_dir();
    if let Some(path) = find_audio_file(&dir, name) {
        return Some(path);
    }
    legacy_alias(name).and_then(|alias| find_audio_file(&dir, alias))
}

pub fn sound_available(name: &str) -> bool {
    sound_path(name).is_some()
}

fn is_audio_file(path: &Path) -> bool {
    path.extension()
        .and_then(|extension| extension.to_str())
        .map(|extension| {
            AUDIO_EXTENSIONS
                .iter()
                .any(|candidate| candidate.eq_ignore_ascii_case(extension))
        })
        .unwrap_or(false)
}

fn sender() -> &'static Mutex<Sender<Command>> {
    TX.get_or_init(|| {
        let (tx, rx) = mpsc::channel::<Command>();
        thread::spawn(move || {
            // The output device is opened lazily and retried for every Play
            // command.  A missing device at startup must not disable audio for
            // the rest of the session.
            let mut _stream: Option<OutputStream> = None;
            let mut handle: Option<OutputStreamHandle> = None;
            let mut sink: Option<Sink> = None;
            let mut last_position = Duration::ZERO;
            let mut last_progress = Instant::now();
            loop {
                match rx.recv_timeout(Duration::from_millis(250)) {
                    Ok(Command::Play(paths, volume)) => {
                        last_position = Duration::ZERO;
                        last_progress = Instant::now();
                        if let Some(current) = sink.take() {
                            current.stop();
                        }
                        // Re-open the device for every playback.  PipeWire,
                        // Bluetooth and USB devices come and go; a cached
                        // OutputStream would keep writing to a dead device and
                        // stay silent until the application was restarted.
                        _stream = None;
                        // Drop the previous device handle as well; on failure
                        // `handle` stays None and the next Play retries.
                        let _ = handle.take();
                        handle = match OutputStream::try_default() {
                            Ok((stream, stream_handle)) => {
                                _stream = Some(stream);
                                Some(stream_handle)
                            }
                            Err(err) => {
                                eprintln!("audio: no output device: {err}");
                                PLAYING.store(false, Ordering::Relaxed);
                                continue;
                            }
                        };
                        let Some(handle) = handle.as_ref() else {
                            PLAYING.store(false, Ordering::Relaxed);
                            continue;
                        };
                        let Ok(new_sink) = Sink::try_new(handle) else {
                            PLAYING.store(false, Ordering::Relaxed);
                            continue;
                        };
                        new_sink.set_volume(volume.clamp(0.0, 2.0));
                        let mut appended = false;
                        for path in paths {
                            let Ok(file) = File::open(&path) else {
                                continue;
                            };
                            match Decoder::new(BufReader::new(file)) {
                                Ok(source) => {
                                    new_sink.append(source);
                                    appended = true;
                                }
                                Err(err) => {
                                    eprintln!("audio decode {}: {err}", path.display());
                                }
                            }
                        }
                        if appended {
                            sink = Some(new_sink);
                            PLAYING.store(true, Ordering::Relaxed);
                        } else {
                            new_sink.stop();
                            PLAYING.store(false, Ordering::Relaxed);
                        }
                    }
                    Ok(Command::Stop) => {
                        if let Some(current) = sink.take() {
                            current.stop();
                        }
                        PLAYING.store(false, Ordering::Relaxed);
                        // cpal keeps an idle ALSA stream polling its file
                        // descriptor.  Release the device together with the
                        // sink so a device that disappears while nothing is
                        // playing cannot spin in its error callback.
                        handle = None;
                        _stream = None;
                    }
                    Ok(Command::SetVolume(volume)) => {
                        if let Some(current) = sink.as_ref() {
                            current.set_volume(volume.clamp(0.0, 2.0));
                        }
                    }
                    Err(RecvTimeoutError::Timeout) => {
                        let mut finished = false;
                        let mut stalled = false;
                        match sink.as_ref() {
                            None => finished = true,
                            Some(current) if current.empty() => finished = true,
                            Some(current) => {
                                let position = current.get_pos();
                                if position != last_position {
                                    last_position = position;
                                    last_progress = Instant::now();
                                } else if !current.is_paused()
                                    && last_progress.elapsed() >= STALL_TIMEOUT
                                {
                                    stalled = true;
                                }
                            }
                        }
                        if stalled {
                            // rodio's error callback only emits a tracing
                            // event, which Namaz does not subscribe to.  Report
                            // the recovery once instead of on every poll
                            // iteration.
                            eprintln!(
                                "audio: output stream stalled, closing it until the next playback"
                            );
                            sink = None;
                            handle = None;
                            _stream = None;
                            PLAYING.store(false, Ordering::Relaxed);
                        } else if finished {
                            PLAYING.store(false, Ordering::Relaxed);
                            sink = None;
                            // Close the device when playback ends.  cpal's
                            // ALSA worker keeps polling even with an empty
                            // sink, and a sink-only watchdog cannot see a
                            // device that is lost while idle: the worker then
                            // spins on POLLERR forever and burns CPU without
                            // printing anything.  Play reopens the device.
                            handle = None;
                            _stream = None;
                        }
                    }
                    Err(RecvTimeoutError::Disconnected) => break,
                }
            }
        });
        Mutex::new(tx)
    })
}

fn volume_factor(volume: f32) -> f32 {
    let master = VOLUME.load(Ordering::Relaxed) as f32 / 100.0;
    if volume > 0.0 {
        (master * volume).clamp(0.0, 2.0)
    } else {
        master.clamp(0.0, 2.0)
    }
}

fn play_paths(paths: Vec<PathBuf>, volume: f32) -> bool {
    if paths.is_empty() {
        return false;
    }
    match sender().lock() {
        Ok(tx) => tx.send(Command::Play(paths, volume_factor(volume))).is_ok(),
        Err(_) => false,
    }
}

/// Play a WAV.  Returns false if the file is absent.
pub fn play_path(path: &Path, volume: f32) -> bool {
    if !path.is_file() {
        return false;
    }
    play_paths(vec![path.to_path_buf()], volume)
}

pub fn play(name: &str, volume: f32) -> bool {
    let Some(path) = sound_path(name) else {
        return false;
    };
    play_path(&path, volume)
}

pub fn play_paths_sequence(paths: &[PathBuf], volume: f32) -> bool {
    play_paths(paths.to_vec(), volume)
}

pub fn play_quran_random(volume: f32, seed: u64) -> bool {
    if QURAN_SOUNDS.is_empty() {
        return false;
    }
    let index = seed as usize % QURAN_SOUNDS.len();
    play(QURAN_SOUNDS[index], volume)
}

pub fn play_iqama(volume: f32, seed: u64) -> bool {
    if IQAMA_SOUNDS.is_empty() {
        return false;
    }
    let index = seed as usize % IQAMA_SOUNDS.len();
    play(IQAMA_SOUNDS[index], volume)
}

pub fn play_bismillah_random(volume: f32, seed: u64) -> bool {
    if BISMILLAH_SOUNDS.is_empty() {
        return false;
    }
    let index = seed as usize % BISMILLAH_SOUNDS.len();
    play(BISMILLAH_SOUNDS[index], volume)
}

pub fn stop() {
    if let Ok(tx) = sender().lock() {
        let _ = tx.send(Command::Stop);
    }
}

pub fn set_volume(percent: i32) {
    let percent = percent.clamp(0, 200) as u32;
    VOLUME.store(percent, Ordering::Relaxed);
    if let Ok(tx) = sender().lock() {
        let _ = tx.send(Command::SetVolume(percent as f32 / 100.0));
    }
}

pub fn volume() -> i32 {
    VOLUME.load(Ordering::Relaxed) as i32
}

pub fn is_playing() -> bool {
    PLAYING.load(Ordering::Relaxed)
}

pub fn available_sounds() -> Vec<String> {
    let mut out = Vec::new();
    if let Ok(dir) = std::fs::read_dir(wave_dir()) {
        for entry in dir.flatten() {
            let path = entry.path();
            if is_audio_file(&path) {
                if let Some(stem) = path.file_stem().and_then(|s| s.to_str()) {
                    if !out.iter().any(|name| name == stem) {
                        out.push(stem.to_string());
                    }
                }
            }
        }
    }
    out.sort();
    out
}

pub fn pick_from_folder(folder: &str) -> Option<PathBuf> {
    if folder.trim().is_empty() {
        return None;
    }
    let mut files: Vec<_> = std::fs::read_dir(folder)
        .ok()?
        .flatten()
        .map(|e| e.path())
        .filter(|p| is_audio_file(p))
        .collect();
    if files.is_empty() {
        return None;
    }
    files.sort();
    let seed = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.subsec_nanos() as usize)
        .unwrap_or(0);
    Some(files[seed % files.len()].clone())
}

pub fn available_athan_sounds() -> Vec<String> {
    ATHAN_SOUNDS
        .iter()
        .filter(|name| sound_path(name).is_some())
        .map(|name| (*name).to_string())
        .collect()
}

pub fn pick_random_athan(seed: u64) -> Option<PathBuf> {
    let names = available_athan_sounds();
    if names.is_empty() {
        return None;
    }
    sound_path(&names[seed as usize % names.len()])
}

pub fn athan_for(_prayer_index: usize) -> &'static str {
    "ADHAN"
}

pub fn play_bismillah() {
    let _ = play("BISMILLAH", 0.7);
}
