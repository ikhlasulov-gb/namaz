//! UI-independent state for the Namaz Linux port.
//!
//! The C++ Qt Widgets frontend talks to this crate through the small C ABI at
//! the bottom of the file.  Complex state is serialized as JSON; the frontend
//! parses it with QJsonDocument.  This keeps the FFI surface tiny and stable
//! while all prayer-time and calendar logic stays in Rust.

#![allow(clippy::not_unsafe_ptr_arg_deref)]
#![allow(clippy::collapsible_match)]

mod audio;
mod cities;

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::sync::OnceLock;

use chrono::{
    Datelike, Duration as ChronoDuration, FixedOffset, NaiveDate, NaiveDateTime, Offset, TimeZone,
    Timelike, Utc,
};
use prayer_core::{get_method, get_north_qibla, get_prayer_times, Date, Location, Method};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};

const MONTHS_EN: [&str; 12] = [
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December",
];

const HIJRI_MONTHS: [&str; 12] = [
    "Muharram",
    "Safar",
    "Rabi' I",
    "Rabi' II",
    "Jumada I",
    "Jumada II",
    "Rajab",
    "Sha'ban",
    "Ramadan",
    "Shawwal",
    "Dhu al-Qi'dah",
    "Dhu al-Hijjah",
];

const PRAYER_LABELS: [&str; 6] = ["Fajr", "Shuruq", "Dhuhr", "Asr", "Maghrib", "Ishaa"];

fn all_cities() -> &'static [cities::City] {
    static CITIES: OnceLock<Vec<cities::City>> = OnceLock::new();
    CITIES.get_or_init(cities::load)
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    pub city: String,
    pub province: String,
    pub latitude: f64,
    pub longitude: f64,
    pub timezone: f64,
    pub timezone_name: String,
    pub dst_mode: i32,
    pub dst_manual: i32,
    pub method: i32,
    pub mathhab: i32,
    pub round: i32,
    pub extreme: i32,
    pub fajr_angle: f64,
    pub ishaa_angle: f64,
    pub adjust: [i32; 6],
    pub dhuhr_minutes: i32,
    pub maghrib_minutes: i32,
    pub athan_enabled: bool,
    pub notifications_enabled: bool,
    pub athan_visual_main: bool,
    pub athan_visual_flash: bool,
    pub hijri_offset: i32,
    pub always_on_top: bool,
    pub hide_on_close: bool,
    pub launch_minimized: bool,
    pub launch_on_startup: bool,
    pub locale: String,
    pub sounds: [String; 6],
    pub athan_modes: [i32; 6],
    pub sound_files: [String; 6],
    pub athan_folder: String,
    pub athan_folders: [String; 6],
    pub volume: i32,
    pub startup_sound: String,
    pub startup_mode: i32,
    pub startup_file: String,
    pub startup_folder: String,
    pub dua_after_athan: bool,
    pub stop_audio_on_prayer: bool,
    pub reminders_before: [i32; 6],
    pub reminders_after: [i32; 6],
    pub reminders_before_on: [bool; 6],
    pub reminders_after_on: [bool; 6],
    pub reminders_before_fixed: [i32; 6],
    pub reminders_fixed_on: [bool; 6],
    pub custom_sound_folder: String,
    pub iqama: [i32; 6],
    pub iqama_on: [bool; 6],
    pub reminder_visual_notification: bool,
    pub reminder_visual_main: bool,
    pub reminder_visual_flash: bool,
    pub reminder_audio_mode: i32,
    pub reminder_sound: String,
    pub reminder_folder: String,
    pub qibla_clockwise: bool,
    pub lock_on_pray_now: bool,
    pub screensaver_on_pray_now: bool,
    pub show_splash: bool,
    pub dbus_enabled: bool,
    pub dbus_interval: i32,
    pub masjid_screen: i32,
    pub show_qiblah: bool,
    pub hijri_calendar: bool,
}

fn default_sounds() -> [String; 6] {
    [
        "ADHAN".into(),
        String::new(), // Shuruq is sunrise, not an athan.
        "ADHAN".into(),
        "ADHAN".into(),
        "ADHAN".into(),
        "ADHAN".into(),
    ]
}

fn default_startup_sound() -> String {
    "BISMILLAH".into()
}

impl Default for Config {
    fn default() -> Self {
        Self {
            city: "Palembang | (ID25), Indonesia".into(),
            province: String::new(),
            latitude: -2.9055,
            longitude: 104.7612,
            timezone: 7.0,
            timezone_name: String::new(),
            dst_mode: 0,
            dst_manual: 0,
            method: 1,
            mathhab: 1,
            round: 2,
            extreme: 5,
            fajr_angle: 20.0,
            ishaa_angle: 18.0,
            adjust: [0; 6],
            dhuhr_minutes: 2,
            maghrib_minutes: 2,
            athan_enabled: true,
            notifications_enabled: true,
            athan_visual_main: false,
            athan_visual_flash: false,
            hijri_offset: 0,
            always_on_top: true,
            hide_on_close: false,
            launch_minimized: false,
            launch_on_startup: false,
            locale: "system".into(),
            sounds: default_sounds(),
            athan_modes: [1, 0, 1, 1, 1, 1],
            sound_files: Default::default(),
            athan_folder: String::new(),
            athan_folders: Default::default(),
            volume: 90,
            startup_sound: default_startup_sound(),
            startup_mode: 1,
            startup_file: String::new(),
            startup_folder: String::new(),
            dua_after_athan: false,
            stop_audio_on_prayer: false,
            reminders_before: [0; 6],
            reminders_after: [0; 6],
            reminders_before_on: [false; 6],
            reminders_after_on: [false; 6],
            reminders_before_fixed: [0; 6],
            reminders_fixed_on: [false; 6],
            custom_sound_folder: String::new(),
            iqama: [0; 6],
            iqama_on: [false; 6],
            reminder_visual_notification: true,
            reminder_visual_main: false,
            reminder_visual_flash: false,
            reminder_audio_mode: 0,
            reminder_sound: String::new(),
            reminder_folder: String::new(),
            qibla_clockwise: true,
            lock_on_pray_now: false,
            screensaver_on_pray_now: false,
            show_splash: false,
            dbus_enabled: true,
            dbus_interval: 60,
            masjid_screen: 0,
            show_qiblah: true,
            hijri_calendar: true,
        }
    }
}

/// Resolve the configuration file the same way the desktop does.
///
/// `XDG_CONFIG_HOME` is what Flatpak redirects to the per-application,
/// persistent `~/.var/app/<id>/config`.  Ignoring it made the Flatpak build
/// write its settings into an ephemeral `$HOME/.config` tmpfs, so every
/// preference was lost on exit.
fn config_path_in(
    xdg_config_home: Option<&std::path::Path>,
    home: Option<&std::path::Path>,
) -> std::path::PathBuf {
    if let Some(dir) = xdg_config_home {
        if dir.is_absolute() {
            return dir.join("namaz").join("config.toml");
        }
    }
    home.unwrap_or_else(|| std::path::Path::new("."))
        .join(".config")
        .join("namaz")
        .join("config.toml")
}

fn home_dir() -> Option<std::path::PathBuf> {
    std::env::var_os("HOME").map(std::path::PathBuf::from)
}

fn config_path() -> std::path::PathBuf {
    let xdg = std::env::var_os("XDG_CONFIG_HOME").map(std::path::PathBuf::from);
    config_path_in(xdg.as_deref(), home_dir().as_deref())
}

/// Configuration files written by the old working title.  They are copied
/// once, then the user continues with the `namaz` directory.
fn old_config_paths() -> Vec<std::path::PathBuf> {
    let mut paths = Vec::new();
    if let Some(dir) = std::env::var_os("XDG_CONFIG_HOME").map(std::path::PathBuf::from) {
        if dir.is_absolute() {
            paths.push(dir.join("salaat-time").join("config.toml"));
        }
    }
    if let Some(home) = home_dir() {
        paths.push(home.join(".config").join("salaat-time").join("config.toml"));
    }
    paths
}

/// Keep settings written by older versions meaningful after a schema change.
fn migrate_config(cfg: &mut Config) {
    // The first UI used 1..4 for the extreme-latitude combo, but those values
    // are different ITL methods.  Map the old labels to the methods they
    // actually meant; the next save writes the canonical values.
    cfg.extreme = match cfg.extreme {
        1 => 10, // Middle of the night, always
        2 => 11, // Middle of the night, invalid only
        3 => 6,  // One-seventh of the night, always
        4 => 7,  // One-seventh of the night, invalid only
        other => other,
    };
    // There is no athan or iqama at sunrise.  Old defaults pointed at a
    // nonexistent SHURUQ.wav and could schedule an iqama after sunrise.
    if cfg.sounds.get(1).map(|s| s == "SHURUQ").unwrap_or(false) {
        cfg.sounds[1].clear();
        if cfg.athan_modes.get(1).copied() == Some(1) {
            cfg.athan_modes[1] = 0;
        }
    }
    cfg.iqama[1] = 0;
    cfg.iqama_on[1] = false;

    // The bundled recordings changed when the project became fully free.
    // Keep old configurations usable by pointing legacy names at the files
    // that are actually shipped.
    for sound in &mut cfg.sounds {
        if matches!(
            sound.as_str(),
            "ALAQSA" | "CAIRO1" | "CAIRO2" | "DAMASCUS" | "FAJRMADINA" | "FAJRMAKKAH" | "MAKKAH"
        ) {
            *sound = "ADHAN".to_string();
        } else if sound.starts_with("BISMILLAH") {
            *sound = "BISMILLAH".to_string();
        } else if matches!(
            sound.as_str(),
            "ALFATIHA"
                | "LUQMAN"
                | "ALQAMAR"
                | "ALALA"
                | "ADDUHA"
                | "ASSARH"
                | "ALASR"
                | "ALMAUN"
                | "ALKAWTAR"
                | "ALKAFIRUNE"
                | "ALIHLAS"
                | "ALFALAQ"
                | "ANNAS"
                | "ALBAQARAH_155_157"
                | "ALBAQARAH_177"
                | "ALBAQARAH_238"
                | "AYAATALKURSI"
                | "ALBAQARAH_285_286"
                | "ALIMRAN"
                | "ALANAM"
                | "HUD"
                | "ALKAHF"
                | "ANNAML"
                | "ALAHZAB"
                | "ALHADID"
                | "ALALAQ"
        ) {
            *sound = "ALIHLAS".to_string();
        }
    }
    if cfg.startup_sound.starts_with("BISMILLAH") {
        cfg.startup_sound = "BISMILLAH".to_string();
    }
}

fn load_config_from(path: &std::path::Path) -> Config {
    let text = match std::fs::read_to_string(path) {
        Ok(text) => text,
        Err(err) => {
            if err.kind() != std::io::ErrorKind::NotFound {
                eprintln!("config: cannot read {}: {err}", path.display());
            }
            return Config::default();
        }
    };
    match toml::from_str::<Config>(&text) {
        Ok(mut cfg) => {
            migrate_config(&mut cfg);
            cfg
        }
        Err(err) => {
            eprintln!("config: cannot parse {}: {err}", path.display());
            // Keep the broken file for the user instead of silently deleting
            // it, then start from defaults so the application still runs.
            let backup = path.with_extension("toml.broken");
            let _ = std::fs::copy(path, &backup);
            Config::default()
        }
    }
}

fn load_config() -> Config {
    let path = config_path();
    // Carry settings over once from the old working title.
    if !path.exists() {
        for legacy in old_config_paths() {
            if legacy != path && legacy.exists() {
                if let Some(parent) = path.parent() {
                    let _ = std::fs::create_dir_all(parent);
                }
                let _ = std::fs::copy(&legacy, &path);
                break;
            }
        }
    }
    load_config_from(&path)
}

fn save_config_to(cfg: &Config, path: &std::path::Path) -> bool {
    if let Some(parent) = path.parent() {
        if std::fs::create_dir_all(parent).is_err() {
            return false;
        }
    }
    let Ok(text) = toml::to_string_pretty(cfg) else {
        return false;
    };
    // Write atomically so an interrupted save cannot destroy the config.
    let temporary = path.with_extension("toml.tmp");
    if std::fs::write(&temporary, text).is_err() {
        return false;
    }
    if std::fs::rename(&temporary, path).is_err() {
        let _ = std::fs::remove_file(&temporary);
        return false;
    }
    true
}

fn save_config(cfg: &Config) -> bool {
    save_config_to(cfg, &config_path())
}

fn app_method(index: i32, cfg: &Config) -> Method {
    let mut m = get_method(0);
    m.extreme = cfg.extreme;
    m.mathhab = cfg.mathhab;
    m.round = cfg.round;
    match index {
        0 => {
            m.fajr_ang = cfg.fajr_angle;
            m.ishaa_ang = cfg.ishaa_angle;
        }
        1 => {
            m.fajr_ang = 19.5;
            m.ishaa_ang = 17.5;
        }
        2 => {
            m.fajr_ang = 18.0;
            m.ishaa_ang = 18.0;
        }
        3 => {
            m.fajr_ang = 18.0;
            m.ishaa_ang = 18.0;
            m.mathhab = 2;
        }
        4 => {
            m.fajr_ang = 15.0;
            m.ishaa_ang = 15.0;
        }
        5 => {
            m.fajr_ang = 18.0;
            m.ishaa_ang = 17.0;
        }
        6 => {
            m.fajr_ang = 15.0;
            m.ishaa_ang = 0.0;
            m.ishaa_inv = 90;
        }
        7 => {
            m.fajr_ang = 19.0;
            m.ishaa_ang = 0.0;
            m.ishaa_inv = 90;
        }
        8 => {
            m.fajr_ang = 19.5;
            m.ishaa_ang = 17.5;
        }
        _ => {}
    }
    m
}

fn timezone(cfg: &Config) -> Option<chrono_tz::Tz> {
    if cfg.timezone_name.trim().is_empty() {
        return None;
    }
    cfg.timezone_name.parse::<chrono_tz::Tz>().ok()
}

fn effective_offset(cfg: &Config, date: NaiveDate) -> f64 {
    if cfg.dst_mode == 1 {
        if let Some(tz) = timezone(cfg) {
            // Use the offset in effect in the middle of the day, not at
            // 00:00: on a DST transition day the whole day's prayer times
            // must use the new offset (transitions happen at night).
            if let Some(noon) = date.and_hms_opt(12, 0, 0) {
                let local = tz.offset_from_local_datetime(&noon);
                if let Some(offset) = local.earliest().or_else(|| local.latest()) {
                    return offset.fix().local_minus_utc() as f64 / 3600.0;
                }
            }
        }
    }
    cfg.timezone
        + if cfg.dst_mode == 2 {
            cfg.dst_manual.clamp(0, 1) as f64
        } else {
            0.0
        }
}

fn local_now(cfg: &Config) -> chrono::DateTime<FixedOffset> {
    let utc = Utc::now();
    if cfg.dst_mode == 1 {
        if let Some(tz) = timezone(cfg) {
            return utc.with_timezone(&tz).fixed_offset();
        }
    }
    let seconds = (effective_offset(cfg, utc.date_naive()) * 3600.0).round() as i32;
    let offset = FixedOffset::east_opt(seconds)
        .unwrap_or_else(|| FixedOffset::east_opt(0).expect("UTC offset"));
    utc.with_timezone(&offset)
}

fn location(cfg: &Config, date: NaiveDate) -> Location {
    Location {
        degree_lat: cfg.latitude,
        degree_long: cfg.longitude,
        gmt_diff: effective_offset(cfg, date),
        dst: 0,
        sea_level: 0.0,
        pressure: 1010.0,
        temperature: 10.0,
    }
}

fn method(cfg: &Config) -> Method {
    let mut conf = app_method(cfg.method, cfg);
    conf.offset = 1;
    for i in 0..6 {
        conf.off_list[i] = cfg.adjust[i] as f64;
    }
    conf.off_list[2] += cfg.dhuhr_minutes as f64;
    conf.off_list[4] += cfg.maghrib_minutes as f64;
    conf
}

fn format_original_time(seconds: u32) -> String {
    let seconds = seconds % (24 * 3600);
    let h = seconds / 3600;
    let m = (seconds % 3600) / 60;
    let s = seconds % 60;
    let suffix = if h < 12 { "AM" } else { "PM" };
    let hour = match h % 12 {
        0 => 12,
        value => value,
    };
    format!("{hour}:{m:02}:{s:02} {suffix}")
}

/// Format a compass bearing (clockwise from true north) as cardinal text.
fn qibla_cardinal(bearing: f64) -> String {
    let bearing = bearing.rem_euclid(360.0);
    if bearing < 90.0 {
        format!("North {bearing:.2}° East")
    } else if bearing < 180.0 {
        format!("East {:.2}° South", bearing - 90.0)
    } else if bearing < 270.0 {
        format!("South {:.2}° West", bearing - 180.0)
    } else {
        format!("West {:.2}° North", 360.0 - bearing)
    }
}

fn format_date_long(date: NaiveDate) -> String {
    format!(
        "{}, {} {}, {}",
        date.format("%A"),
        MONTHS_EN[(date.month() - 1) as usize],
        date.day(),
        date.year()
    )
}

fn is_russian(cfg: &Config) -> bool {
    if cfg.locale.starts_with("ru") {
        return true;
    }
    if cfg.locale == "system" {
        // POSIX precedence: LC_ALL overrides LC_MESSAGES, which overrides LANG.
        let lang = ["LC_ALL", "LC_MESSAGES", "LANG"]
            .iter()
            .filter_map(|key| std::env::var(key).ok())
            .find(|value| !value.is_empty())
            .unwrap_or_default();
        return lang.starts_with("ru");
    }
    false
}

fn localized_prayer(cfg: &Config, index: usize) -> String {
    if is_russian(cfg) {
        ["Фаджр", "Шурук", "Зухр", "Аср", "Магриб", "Иша"]
            .get(index)
            .unwrap_or(&"")
            .to_string()
    } else {
        PRAYER_LABELS.get(index).unwrap_or(&"").to_string()
    }
}

fn reminder_title(cfg: &Config, key: &str) -> String {
    if is_russian(cfg) {
        match key {
            "reminder" => "Напоминание Namaz".to_string(),
            _ => "Namaz".to_string(),
        }
    } else {
        match key {
            "reminder" => "Namaz Reminder".to_string(),
            _ => "Namaz".to_string(),
        }
    }
}

fn push_reminder_event(
    events: &mut Vec<Value>,
    title: &str,
    body: String,
    visual: bool,
    notifications_enabled: bool,
) {
    if visual && notifications_enabled {
        let _ = notify_rust::Notification::new()
            .appname("Namaz")
            .icon("io.github.ikhlasulov_gb.Namaz")
            .summary(title)
            .body(&body)
            .show();
    }
    events.push(json!({
        "type": "reminder",
        "title": title,
        "body": body,
    }));
}

/// How late an event may still be delivered (e.g. after the computer sleeps).
/// The UI ticks once a second, but a resumed session or a busy machine may be
/// minutes behind; 15 minutes still catches up after a short suspend without
/// replaying a stale athan hours later.
const EVENT_GRACE_SECONDS: i64 = 900;

fn event_time(date: NaiveDate, seconds: i64) -> NaiveDateTime {
    date.and_hms_opt(0, 0, 0)
        .unwrap_or_else(|| date.and_hms_opt(12, 0, 0).expect("valid noon"))
        + ChronoDuration::seconds(seconds)
}

/// Absolute (not time-of-day) due check.  This is what makes events that
/// cross midnight, and events missed while the machine was asleep, reliable.
fn due_at(now: NaiveDateTime, at: NaiveDateTime, grace_seconds: i64) -> bool {
    let diff = (now - at).num_seconds();
    diff >= 0 && diff <= grace_seconds.max(0)
}

/// Index of the prayer that is currently in effect.
///
/// Between midnight and Fajr the previous day's Ishaa is still the current
/// prayer, hence the default 5.  When Ishaa is unavailable (polar day or
/// night) there is no current prayer at all.
fn current_prayer_index(secs: &[Option<u32>; 6], now_sec: i64) -> i32 {
    let mut index: i32 = if secs[5].is_some() { 5 } else { -1 };
    for i in [0usize, 2, 3, 4, 5] {
        if let Some(s) = secs[i] {
            if (s as i64) <= now_sec {
                index = i as i32;
            }
        }
    }
    index
}

fn hijri_for(date: NaiveDate, offset: i32) -> (i32, u32, u32) {
    let shifted = date + ChronoDuration::days(offset as i64);
    let g = hijridate::Gregorian::new(shifted.year(), shifted.month() as u8, shifted.day() as u8);
    match g {
        Ok(g) => {
            let h = g.to_hijri();
            (h.year(), u32::from(h.month()), u32::from(h.day()))
        }
        Err(_) => (0, 0, 0),
    }
}

fn hijri_month_bounds(year: i32, month: u32) -> Option<(NaiveDate, u32)> {
    let h = hijridate::Hijri::new(year, month as u8, 1).ok()?;
    let g = h.to_gregorian();
    Some((
        NaiveDate::from_ymd_opt(g.year(), u32::from(g.month()), u32::from(g.day()))?,
        u32::from(h.month_length()),
    ))
}

fn hijri_text(date: NaiveDate, offset: i32) -> String {
    let (hy, hm, hd) = hijri_for(date, offset);
    let month = HIJRI_MONTHS[(hm.saturating_sub(1) as usize).min(11)];
    format!("{}, {} {}, {}", date.format("%A"), month, hd, hy)
}

fn month_cells(start: NaiveDate, days: u32, selected: NaiveDate, today: NaiveDate) -> Vec<Value> {
    let first_wd = start.weekday().num_days_from_sunday() as i32;
    let weeks = ((first_wd + days as i32 + 6) / 7).max(4);
    let mut cells = Vec::with_capacity((weeks * 7) as usize);
    for i in 0..weeks * 7 {
        let day = i - first_wd + 1;
        let in_month = day >= 1 && day <= days as i32;
        let date = if in_month {
            Some(start + ChronoDuration::days((day - 1) as i64))
        } else {
            None
        };
        cells.push(json!({
            "day": if in_month { day } else { 0 },
            "inMonth": in_month,
            "selected": date.map(|d| d == selected).unwrap_or(false),
            "today": date.map(|d| d == today).unwrap_or(false),
            "iso": date.map(|d| d.format("%Y-%m-%d").to_string()).unwrap_or_default(),
        }));
    }
    cells
}

fn days_in_month(year: i32, month: u32) -> u32 {
    let (ny, nm) = if month == 12 {
        (year + 1, 1)
    } else {
        (year, month + 1)
    };
    NaiveDate::from_ymd_opt(ny, nm, 1)
        .and_then(|next| {
            NaiveDate::from_ymd_opt(year, month, 1)
                .map(|first| next.signed_duration_since(first).num_days() as u32)
        })
        .unwrap_or(30)
}

fn days_in_year(year: i32) -> u32 {
    NaiveDate::from_ymd_opt(year, 12, 31)
        .map(|date| date.ordinal())
        .unwrap_or(365)
}

fn hijri_iso(date: NaiveDate, offset: i32) -> String {
    let (year, month, day) = hijri_for(date, offset);
    if year == 0 {
        String::new()
    } else {
        format!("{year:04}-{month:02}-{day:02}")
    }
}

fn csv_quote(value: &str) -> String {
    format!("\"{}\"", value.replace('"', "\"\""))
}

pub struct Core {
    pub cfg: Config,
    pub selected: NaiveDate,
    pub view: i32,
    seen_events: std::collections::HashSet<String>,
    last_date: NaiveDate,
}

impl Core {
    pub fn new() -> Self {
        let cfg = load_config();
        let today = local_now(&cfg).date_naive();
        Self {
            cfg,
            selected: today,
            view: 0,
            seen_events: std::collections::HashSet::new(),
            last_date: today,
        }
    }

    fn now(&self) -> chrono::DateTime<FixedOffset> {
        local_now(&self.cfg)
    }

    pub fn day_json(&self) -> Value {
        let now = self.now();
        let today = now.date_naive();
        let selected = self.selected;
        let cfg = &self.cfg;
        let loc = location(cfg, selected);
        let conf = method(cfg);
        let date = Date::new(
            selected.year(),
            selected.month() as i32,
            selected.day() as i32,
        );
        let times = get_prayer_times(&loc, &conf, &date);
        let secs: [Option<u32>; 6] = [
            times[0].seconds_of_day(),
            times[1].seconds_of_day(),
            times[2].seconds_of_day(),
            times[3].seconds_of_day(),
            times[4].seconds_of_day(),
            times[5].seconds_of_day(),
        ];

        let mut time_values = Vec::with_capacity(6);
        for seconds in secs {
            time_values.push(json!({
                "text": seconds.map(format_original_time).unwrap_or_else(|| "N/A".into()),
                "short": seconds.map(|s| format!("{:02}:{:02}", s / 3600, (s % 3600) / 60))
                    .unwrap_or_else(|| "N/A".into()),
                "seconds": seconds,
            }));
        }

        let now_sec = now.time().num_seconds_from_midnight() as i64;
        let is_today = selected == today;
        let mut next_at: Option<(i32, i64, String, NaiveDateTime)> = None;
        if is_today {
            for i in [0usize, 2, 3, 4, 5] {
                if let Some(s) = secs[i] {
                    if (s as i64) > now_sec {
                        next_at = Some((
                            i as i32,
                            s as i64,
                            PRAYER_LABELS[i].to_string(),
                            event_time(selected, s as i64),
                        ));
                        break;
                    }
                }
            }
            if next_at.is_none() {
                // Tomorrow's Fajr may be on the other side of a DST change,
                // so compute it from that date instead of reusing today's
                // Location.
                let next_date = selected + ChronoDuration::days(1);
                let next_times = get_prayer_times(
                    &location(cfg, next_date),
                    &conf,
                    &Date::new(
                        next_date.year(),
                        next_date.month() as i32,
                        next_date.day() as i32,
                    ),
                );
                if let Some(seconds) = next_times[0].seconds_of_day() {
                    next_at = Some((
                        0,
                        seconds as i64,
                        "Fajr".to_string(),
                        event_time(next_date, seconds as i64),
                    ));
                }
            }
        }
        let (next_idx, next_sec, next_name, next_diff) = match next_at {
            // Absolute difference, so the countdown stays correct when the
            // next prayer is on the other side of a DST transition.
            Some((index, seconds, name, at)) => {
                (index, seconds, name, (at - now.naive_local()).num_seconds())
            }
            None => (-1, 0, String::new(), -1),
        };

        let current_idx = if is_today {
            current_prayer_index(&secs, now_sec)
        } else {
            -1
        };
        let prev_sec = if current_idx >= 0 {
            secs[current_idx as usize].unwrap_or(0) as i64
        } else {
            0
        };
        let total = (next_sec - prev_sec).rem_euclid(24 * 3600).max(1);
        let elapsed = (now_sec - prev_sec).rem_euclid(24 * 3600);
        let remaining = if is_today && next_idx >= 0 {
            1.0 - (elapsed as f64 / total as f64).clamp(0.0, 1.0)
        } else {
            0.0
        };

        let q = get_north_qibla(&loc);
        // ITL returns a signed angle where positive values point west of true
        // north.  The compass bearing (clockwise from north) is 360 - q.
        // The JSON always carries the compass bearing; the UI chooses whether
        // to render it as a number or as cardinal text.
        let compass = (360.0 - q).rem_euclid(360.0);
        let bearing = compass;
        let current_name = if current_idx >= 0 {
            PRAYER_LABELS[current_idx as usize]
        } else {
            ""
        };
        let window_title = if current_idx >= 0 {
            format!("Namaz - Current Prayer is {current_name}")
        } else {
            "Namaz".to_string()
        };
        let (hijri_year, hijri_month, hijri_day) = hijri_for(selected, cfg.hijri_offset);
        let hijri = hijri_text(selected, cfg.hijri_offset);
        let date_text = format_date_long(selected);
        let date_line = if cfg.hijri_calendar {
            format!("{date_text} / {hijri}")
        } else {
            date_text.clone()
        };

        json!({
            "ok": true,
            "date": {
                "iso": selected.format("%Y-%m-%d").to_string(),
                "text": date_text,
                "hijri": hijri,
                "hijriYear": hijri_year,
                "hijriMonth": hijri_month,
                "hijriDay": hijri_day,
                "showHijri": cfg.hijri_calendar,
                "line": date_line,
                "friday": selected.weekday() == chrono::Weekday::Fri,
                "today": is_today,
            },
            "now": {
                "text": format_original_time(now_sec as u32),
                "seconds": now_sec,
            },
            "times": time_values,
            "next": {
                "index": next_idx,
                "name": next_name,
                "seconds": if is_today && next_idx >= 0 { next_sec } else { -1 },
                "time": if is_today && next_idx >= 0 { format_original_time(next_sec as u32) } else { String::new() },
                "countdown": if is_today && next_idx >= 0 { format!("{}h{:02}m{:02}s", next_diff / 3600, (next_diff % 3600) / 60, next_diff % 60) } else { String::new() },
                "remaining": if is_today && next_idx >= 0 { next_diff } else { -1 },
                "progress": if is_today && next_idx >= 0 { remaining } else { 0.0 },
            },
            "current": {
                "index": current_idx,
                "name": current_name,
                "windowTitle": window_title,
            },
            "qibla": {
                "bearing": bearing,
                "text": format!("{bearing:.2}°"),
                "compass": compass,
                "compassText": format!("{compass:.2}°"),
                // Cardinal text is always expressed as a compass bearing so
                // it stays correct when the numeric representation changes.
                "cardinal": qibla_cardinal(compass),
                "clockwise": cfg.qibla_clockwise,
            },
            "iqama": cfg.iqama,
            "iqamaOn": cfg.iqama_on,
            "city": cfg.city.replace(" | ", ", "),
            "showQiblah": cfg.show_qiblah,
            "view": self.view,
        })
    }

    pub fn month_json(&self) -> Value {
        let selected = self.selected;
        let today = self.now().date_naive();
        let (hy, hm, _) = hijri_for(selected, self.cfg.hijri_offset);
        let start =
            NaiveDate::from_ymd_opt(selected.year(), selected.month(), 1).expect("valid month");
        let cells_g = month_cells(
            start,
            days_in_month(selected.year(), selected.month()),
            selected,
            today,
        );
        let (cells_h, title_h) = if let Some((hstart_actual, hdays)) = hijri_month_bounds(hy, hm) {
            // `hijri_for` shifts the Gregorian date by hijri_offset, so the
            // grid must shift the month start back by the same amount.
            let hstart = hstart_actual - ChronoDuration::days(self.cfg.hijri_offset as i64);
            (
                month_cells(hstart, hdays, selected, today),
                format!(
                    "{} {}",
                    HIJRI_MONTHS[(hm.saturating_sub(1) as usize).min(11)],
                    hy
                ),
            )
        } else {
            (cells_g.clone(), String::new())
        };
        json!({
            "titleG": format!("{} {}", MONTHS_EN[(selected.month() - 1) as usize], selected.year()),
            "titleH": title_h,
            "selectedLabel": selected.format("%b %-d").to_string(),
            "selectedIso": selected.format("%Y-%m-%d").to_string(),
            "month": selected.month(),
            "year": selected.year(),
            "hijriYear": hy,
            "hijriMonth": hm,
            "cellsG": cells_g,
            "cellsH": cells_h,
        })
    }

    pub fn year_json(&self) -> Value {
        let selected = self.selected;
        let today = self.now().date_naive();
        let conf = method(&self.cfg);
        let mut rows = Vec::with_capacity(365);
        for i in 0..365 {
            let d = selected + ChronoDuration::days(i);
            let date = Date::new(d.year(), d.month() as i32, d.day() as i32);
            let loc = location(&self.cfg, d);
            let t = get_prayer_times(&loc, &conf, &date);
            let mut row = json!({
                "date": d.format("%b %-d").to_string(),
                "iso": d.format("%Y-%m-%d").to_string(),
                "current": d == today,
            });
            for (idx, name) in PRAYER_LABELS.iter().enumerate() {
                row[name.to_lowercase()] = json!(t[idx]
                    .seconds_of_day()
                    .map(|s| format!("{:02}:{:02}", s / 3600, (s % 3600) / 60))
                    .unwrap_or_else(|| "N/A".into()));
            }
            rows.push(row);
        }
        json!({
            "title": format!(
                "{} - {}",
                format_date_long(selected),
                format_date_long(selected + ChronoDuration::days(364))
            ),
            "startIso": selected.format("%Y-%m-%d").to_string(),
            // The table has 365 rows: start .. start+364.  Keep the displayed
            // end date in sync with the last row.
            "endIso": (selected + ChronoDuration::days(364)).format("%Y-%m-%d").to_string(),
            "rows": rows,
        })
    }

    /// Called once per second by the frontend.  Fires athan/reminders exactly
    /// once when their second is crossed.  The returned list describes the
    /// events so the UI can show the main window or flash the tray icon.
    pub fn tick(&mut self) -> Value {
        self.tick_at(self.now())
    }

    /// Testable variant of `tick` with an injected local time.
    pub fn tick_at(&mut self, now: chrono::DateTime<FixedOffset>) -> Value {
        let now_naive = now.naive_local();
        let today = now_naive.date();
        if today != self.last_date {
            let yesterday = (today - ChronoDuration::days(1))
                .format("%Y-%m-%d")
                .to_string();
            let today_key = today.format("%Y-%m-%d").to_string();
            self.seen_events
                .retain(|key| key.starts_with(&yesterday) || key.starts_with(&today_key));
            self.last_date = today;
        }
        let mut events = Vec::new();
        let seed = (now_naive.time().num_seconds_from_midnight() as u64)
            ^ (today.num_days_from_ce() as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15);

        // Check yesterday/today/tomorrow so a reminder just before Fajr, an
        // iqama after Ishaa, or an event missed while suspended is still
        // delivered even when it belongs to an adjacent calendar day.
        for offset in [-1i64, 0, 1] {
            let schedule = today + ChronoDuration::days(offset);
            let date = Date::new(
                schedule.year(),
                schedule.month() as i32,
                schedule.day() as i32,
            );
            let times = get_prayer_times(&location(&self.cfg, schedule), &method(&self.cfg), &date);
            let day_key = schedule.format("%Y-%m-%d").to_string();

            for i in 0usize..6 {
                let Some(seconds) = times[i].seconds_of_day() else {
                    continue;
                };
                let seconds = seconds as i64;

                if self.cfg.reminders_before_on[i] {
                    let minutes = self.cfg.reminders_before[i];
                    if minutes > 0 {
                        let at_seconds = seconds - minutes as i64 * 60;
                        let key = format!("{day_key}:before:{i}:{at_seconds}");
                        if due_at(
                            now_naive,
                            event_time(schedule, at_seconds),
                            EVENT_GRACE_SECONDS,
                        ) && self.seen_events.insert(key)
                        {
                            let prayer = localized_prayer(&self.cfg, i);
                            let body = if is_russian(&self.cfg) {
                                format!("До молитвы {prayer} осталось {minutes} мин.")
                            } else {
                                format!("There remains {minutes} minutes until {prayer}.")
                            };
                            push_reminder_event(
                                &mut events,
                                &reminder_title(&self.cfg, "reminder"),
                                body,
                                self.cfg.reminder_visual_notification,
                                self.cfg.notifications_enabled,
                            );
                            if self.cfg.athan_enabled {
                                self.play_reminder_audio(seed);
                            }
                        }
                    }
                }

                if self.cfg.reminders_fixed_on[i] {
                    let at_seconds = self.cfg.reminders_before_fixed[i] as i64 * 60;
                    let key = format!("{day_key}:fixed:{i}:{at_seconds}");
                    if due_at(
                        now_naive,
                        event_time(schedule, at_seconds),
                        EVENT_GRACE_SECONDS,
                    ) && self.seen_events.insert(key)
                    {
                        let prayer = localized_prayer(&self.cfg, i);
                        let at_text = if is_russian(&self.cfg) {
                            format!(
                                "{:02}:{:02}",
                                (at_seconds / 3600) % 24,
                                (at_seconds % 3600) / 60
                            )
                        } else {
                            format_original_time(at_seconds as u32)
                        };
                        let body = if is_russian(&self.cfg) {
                            format!("Фиксированное напоминание для {prayer} в {at_text}.")
                        } else {
                            format!("Fixed reminder for {prayer} at {at_text}.")
                        };
                        push_reminder_event(
                            &mut events,
                            &reminder_title(&self.cfg, "reminder"),
                            body,
                            self.cfg.reminder_visual_notification,
                            self.cfg.notifications_enabled,
                        );
                        if self.cfg.athan_enabled {
                            self.play_reminder_audio(seed);
                        }
                    }
                }

                if self.cfg.reminders_after_on[i] {
                    let minutes = self.cfg.reminders_after[i];
                    if minutes > 0 {
                        let at_seconds = seconds + minutes as i64 * 60;
                        let key = format!("{day_key}:after:{i}:{at_seconds}");
                        if due_at(
                            now_naive,
                            event_time(schedule, at_seconds),
                            EVENT_GRACE_SECONDS,
                        ) && self.seen_events.insert(key)
                        {
                            let prayer = localized_prayer(&self.cfg, i);
                            let body = if is_russian(&self.cfg) {
                                format!("С момента молитвы {prayer} прошло {minutes} мин.")
                            } else {
                                format!("It has been {minutes} minutes since {prayer}.")
                            };
                            push_reminder_event(
                                &mut events,
                                &reminder_title(&self.cfg, "reminder"),
                                body,
                                self.cfg.reminder_visual_notification,
                                self.cfg.notifications_enabled,
                            );
                            if self.cfg.athan_enabled {
                                self.play_reminder_audio(seed);
                            }
                        }
                    }
                }

                // Sunrise (index 1) has no iqama or athan.
                if i != 1 && self.cfg.iqama_on[i] && self.cfg.iqama[i] > 0 {
                    let at_seconds = seconds + self.cfg.iqama[i] as i64 * 60;
                    let key = format!("{day_key}:iqama:{i}:{at_seconds}");
                    if due_at(
                        now_naive,
                        event_time(schedule, at_seconds),
                        EVENT_GRACE_SECONDS,
                    ) && self.seen_events.insert(key)
                    {
                        let prayer = localized_prayer(&self.cfg, i);
                        let body = if is_russian(&self.cfg) {
                            format!("Икама для молитвы {prayer}.")
                        } else {
                            format!("Iqama for {prayer}.")
                        };
                        push_reminder_event(
                            &mut events,
                            &reminder_title(&self.cfg, "namaz"),
                            body,
                            self.cfg.reminder_visual_notification,
                            self.cfg.notifications_enabled,
                        );
                        if self.cfg.athan_enabled {
                            self.play_reminder_audio(seed);
                        }
                    }
                }

                if i == 1 {
                    continue;
                }
                let key = format!("{day_key}:athan:{i}:{seconds}");
                if due_at(
                    now_naive,
                    event_time(schedule, seconds),
                    EVENT_GRACE_SECONDS,
                ) && self.seen_events.insert(key)
                {
                    if self.cfg.stop_audio_on_prayer {
                        audio::stop();
                    }
                    if self.cfg.athan_enabled {
                        self.play_athan_audio(i);
                    }
                    if self.cfg.notifications_enabled {
                        let _ = notify_rust::Notification::new()
                            .appname("Namaz")
                            .icon("io.github.ikhlasulov_gb.Namaz")
                            .summary("Namaz")
                            .body(&if is_russian(&self.cfg) {
                                format!("Время молитвы {}.", localized_prayer(&self.cfg, i))
                            } else {
                                format!("It is time to pray {}.", PRAYER_LABELS[i])
                            })
                            .show();
                    }
                    events.push(json!({
                        "type": "athan",
                        "prayer": i,
                        "name": PRAYER_LABELS[i],
                    }));
                }
            }
        }

        json!(events)
    }

    fn athan_paths(&self, index: usize, seed: u64) -> Vec<std::path::PathBuf> {
        let mut paths = Vec::new();
        let mode = self.cfg.athan_modes.get(index).copied().unwrap_or(1);
        match mode {
            0 => {
                return paths;
            }
            2 => {
                let path = std::path::PathBuf::from(&self.cfg.sound_files[index]);
                if path.is_file() {
                    paths.push(path);
                }
            }
            3 => {
                let folder = if !self.cfg.athan_folders[index].trim().is_empty() {
                    &self.cfg.athan_folders[index]
                } else if self.cfg.athan_folder.trim().is_empty() {
                    &self.cfg.custom_sound_folder
                } else {
                    &self.cfg.athan_folder
                };
                if let Some(path) = audio::pick_from_folder(folder) {
                    paths.push(path);
                }
            }
            _ => {
                let name = &self.cfg.sounds[index];
                if name == "<random>" {
                    if let Some(path) = audio::pick_random_athan(seed) {
                        paths.push(path);
                    }
                } else if name.is_empty() {
                    // Fall through to the default athan for this prayer.
                } else if name.starts_with('/') {
                    let path = std::path::PathBuf::from(name);
                    if path.is_file() {
                        paths.push(path);
                    }
                } else if let Some(path) = audio::sound_path(name) {
                    paths.push(path);
                }
                if paths.is_empty() {
                    if let Some(path) = audio::sound_path(audio::athan_for(index)) {
                        paths.push(path);
                    }
                }
            }
        }
        // A custom file/folder that is missing must stay silent instead of
        // quietly playing the built-in athan the user did not select.
        if paths.is_empty() {
            return paths;
        }
        if self.cfg.dua_after_athan {
            if let Some(path) = audio::sound_path("DUA") {
                paths.push(path);
            }
        }
        paths
    }

    fn play_athan_audio(&self, index: usize) {
        let now = self.now();
        let seed = (now.time().num_seconds_from_midnight() as u64)
            ^ (now.date_naive().num_days_from_ce() as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15);
        let paths = self.athan_paths(index, seed);
        audio::play_paths_sequence(&paths, 0.0);
    }

    fn play_reminder_audio(&self, seed: u64) {
        match self.cfg.reminder_audio_mode {
            1 => {
                audio::play_iqama(0.0, seed);
            }
            2 => {
                audio::play_bismillah_random(0.0, seed);
            }
            3 => {
                audio::play_quran_random(0.0, seed);
            }
            4 => {
                let path = std::path::Path::new(&self.cfg.reminder_sound);
                audio::play_path(path, 0.0);
            }
            5 => {
                if let Some(path) = audio::pick_from_folder(&self.cfg.reminder_folder) {
                    audio::play_path(&path, 0.0);
                }
            }
            _ => {}
        }
    }

    pub fn startup(&self) {
        if !self.cfg.athan_enabled {
            return;
        }
        let seed = self.now().time().num_seconds_from_midnight() as u64;
        match self.cfg.startup_mode {
            0 => {}
            1 => {
                let name = self.cfg.startup_sound.trim();
                if name.is_empty() {
                    audio::play_bismillah_random(0.7, seed);
                } else {
                    audio::play(name, 0.0);
                }
            }
            2 => {
                audio::play_quran_random(0.0, seed);
            }
            3 => {
                audio::play_path(std::path::Path::new(&self.cfg.startup_file), 0.0);
            }
            4 => {
                if let Some(path) = audio::pick_from_folder(&self.cfg.startup_folder) {
                    audio::play_path(&path, 0.0);
                }
            }
            _ => {
                audio::play_bismillah();
            }
        }
    }

    fn csv_rows(&self, days: impl Iterator<Item = NaiveDate>) -> String {
        let conf = method(&self.cfg);
        let mut out = String::from("Date,Fajr,Shuruq,Dhuhr,Asr,Maghrib,Ishaa\n");
        for d in days {
            let date = Date::new(d.year(), d.month() as i32, d.day() as i32);
            let loc = location(&self.cfg, d);
            let t = get_prayer_times(&loc, &conf, &date);
            out.push_str(&d.format("%Y-%m-%d").to_string());
            for prayer in &t {
                out.push(',');
                match prayer.seconds_of_day() {
                    Some(s) => out.push_str(&format!("{:02}:{:02}", s / 3600, (s % 3600) / 60)),
                    None => out.push_str("N/A"),
                }
            }
            out.push('\n');
        }
        out
    }

    fn csv_hijri_rows(&self, days: impl Iterator<Item = NaiveDate>) -> String {
        let conf = method(&self.cfg);
        let mut out =
            String::from("Hijri Date,Gregorian Date,Fajr,Shuruq,Dhuhr,Asr,Maghrib,Ishaa\n");
        for d in days {
            let date = Date::new(d.year(), d.month() as i32, d.day() as i32);
            let loc = location(&self.cfg, d);
            let t = get_prayer_times(&loc, &conf, &date);
            out.push_str(&hijri_iso(d, self.cfg.hijri_offset));
            out.push(',');
            out.push_str(&d.format("%Y-%m-%d").to_string());
            for prayer in &t {
                out.push(',');
                match prayer.seconds_of_day() {
                    Some(s) => out.push_str(&format!("{:02}:{:02}", s / 3600, (s % 3600) / 60)),
                    None => out.push_str("N/A"),
                }
            }
            out.push('\n');
        }
        out
    }

    fn csv_outlook_rows(&self, days: impl Iterator<Item = NaiveDate>) -> String {
        let conf = method(&self.cfg);
        let location_name = self.cfg.city.replace(" | ", ", ");
        let mut out = String::from(
            "Subject,Start Date,Start Time,End Date,End Time,All day event,\
             Reminder on/off,Reminder Date,Reminder Time,Categories,Description,\
             Location,Private\n",
        );
        for d in days {
            let date = Date::new(d.year(), d.month() as i32, d.day() as i32);
            let loc = location(&self.cfg, d);
            let t = get_prayer_times(&loc, &conf, &date);
            let date_text = d.format("%m/%d/%Y").to_string();
            for (index, prayer) in t.iter().enumerate() {
                let Some(seconds) = prayer.seconds_of_day() else {
                    continue;
                };
                let hour24 = seconds / 3600;
                let minute = (seconds % 3600) / 60;
                let suffix = if hour24 < 12 { "AM" } else { "PM" };
                let hour12 = match hour24 % 12 {
                    0 => 12,
                    hour => hour,
                };
                let time_text = format!("{hour12}:{minute:02} {suffix}");
                out.push_str(&format!(
                    "{},{},{},{},{},False,False,,,{},,{},False\n",
                    csv_quote(PRAYER_LABELS[index]),
                    csv_quote(&date_text),
                    csv_quote(&time_text),
                    csv_quote(&date_text),
                    csv_quote(&time_text),
                    csv_quote("Namaz"),
                    csv_quote(&location_name),
                ));
            }
        }
        out
    }

    pub fn csv_month(&self, year: i32, month: u32) -> String {
        let start = NaiveDate::from_ymd_opt(year, month, 1).unwrap_or(self.selected);
        let days = days_in_month(year, month);
        self.csv_rows((0..days).map(move |d| start + ChronoDuration::days(d as i64)))
    }

    pub fn csv_year(&self, year: i32) -> String {
        let start = NaiveDate::from_ymd_opt(year, 1, 1).unwrap_or(self.selected);
        self.csv_rows((0..days_in_year(year)).map(move |d| start + ChronoDuration::days(d as i64)))
    }

    pub fn csv_hijri_month(&self, year: i32, month: u32) -> String {
        let start = NaiveDate::from_ymd_opt(year, month, 1).unwrap_or(self.selected);
        let days = days_in_month(year, month);
        self.csv_hijri_rows((0..days).map(move |d| start + ChronoDuration::days(d as i64)))
    }

    pub fn csv_hijri_year(&self, year: i32) -> String {
        let start = NaiveDate::from_ymd_opt(year, 1, 1).unwrap_or(self.selected);
        self.csv_hijri_rows(
            (0..days_in_year(year)).map(move |d| start + ChronoDuration::days(d as i64)),
        )
    }

    /// CSV for a real Hijri month (not a Gregorian month with a Hijri column).
    pub fn csv_hijri_month_by_hijri(&self, hijri_year: i32, hijri_month: u32) -> String {
        let Some((start_actual, days)) = hijri_month_bounds(hijri_year, hijri_month) else {
            return String::new();
        };
        let start = start_actual - ChronoDuration::days(self.cfg.hijri_offset as i64);
        self.csv_hijri_rows((0..days).map(move |d| start + ChronoDuration::days(d as i64)))
    }

    /// CSV for a complete Hijri year (Muharram 1 through the end of Dhu al-Hijjah).
    pub fn csv_hijri_year_by_hijri(&self, hijri_year: i32) -> String {
        let Some((start_actual, _)) = hijri_month_bounds(hijri_year, 1) else {
            return String::new();
        };
        let Some((last_actual, last_days)) = hijri_month_bounds(hijri_year, 12) else {
            return String::new();
        };
        let offset = ChronoDuration::days(self.cfg.hijri_offset as i64);
        let start = start_actual - offset;
        let end = last_actual + ChronoDuration::days(last_days as i64) - offset;
        let days = (end - start).num_days();
        if days <= 0 {
            return String::new();
        }
        self.csv_hijri_rows((0..days).map(move |d| start + ChronoDuration::days(d)))
    }

    pub fn csv_outlook_month(&self, year: i32, month: u32) -> String {
        let start = NaiveDate::from_ymd_opt(year, month, 1).unwrap_or(self.selected);
        let days = days_in_month(year, month);
        self.csv_outlook_rows((0..days).map(move |d| start + ChronoDuration::days(d as i64)))
    }

    pub fn csv_outlook_year(&self, year: i32) -> String {
        let start = NaiveDate::from_ymd_opt(year, 1, 1).unwrap_or(self.selected);
        self.csv_outlook_rows(
            (0..days_in_year(year)).map(move |d| start + ChronoDuration::days(d as i64)),
        )
    }

    pub fn csv_month_calendar(&self, year: i32, month: u32, calendar: i32) -> String {
        match calendar {
            1 => self.csv_hijri_month(year, month),
            2 => self.csv_outlook_month(year, month),
            _ => self.csv_month(year, month),
        }
    }

    pub fn csv_year_calendar(&self, year: i32, calendar: i32) -> String {
        match calendar {
            1 => self.csv_hijri_year(year),
            2 => self.csv_outlook_year(year),
            _ => self.csv_year(year),
        }
    }

    pub fn set_date(&mut self, date: NaiveDate) {
        self.selected = date;
    }

    pub fn shift_day(&mut self, days: i64) {
        self.selected += ChronoDuration::days(days);
    }

    pub fn shift_month(&mut self, months: i32) {
        let mut year = self.selected.year();
        let mut month = self.selected.month() as i32 + months;
        while month < 1 {
            month += 12;
            year -= 1;
        }
        while month > 12 {
            month -= 12;
            year += 1;
        }
        let day = self.selected.day().min(days_in_month(year, month as u32));
        self.selected = NaiveDate::from_ymd_opt(year, month as u32, day).unwrap_or(self.selected);
    }

    pub fn shift_year(&mut self, years: i32) {
        let year = self.selected.year() + years;
        let month = self.selected.month();
        let day = self.selected.day().min(days_in_month(year, month));
        self.selected = NaiveDate::from_ymd_opt(year, month, day).unwrap_or(self.selected);
    }

    pub fn go_today(&mut self) {
        self.selected = self.now().date_naive();
    }

    pub fn set_view(&mut self, view: i32) {
        self.view = view.clamp(0, 2);
    }

    pub fn save(&self) -> bool {
        save_config(&self.cfg)
    }
}

impl Default for Core {
    fn default() -> Self {
        Self::new()
    }
}

fn into_c_string(value: Value) -> *mut c_char {
    match CString::new(value.to_string()) {
        Ok(s) => s.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

unsafe fn c_str(ptr: *const c_char) -> Option<String> {
    if ptr.is_null() {
        return None;
    }
    CStr::from_ptr(ptr).to_str().ok().map(str::to_owned)
}

#[no_mangle]
pub extern "C" fn namaz_core_new() -> *mut Core {
    Box::into_raw(Box::new(Core::new()))
}

#[no_mangle]
pub extern "C" fn namaz_core_free(core: *mut Core) {
    if !core.is_null() {
        unsafe { drop(Box::from_raw(core)) };
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_free_string(value: *mut c_char) {
    if !value.is_null() {
        unsafe { drop(CString::from_raw(value)) };
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_day_json(core: *mut Core) -> *mut c_char {
    if core.is_null() {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    into_c_string(core.day_json())
}

#[no_mangle]
pub extern "C" fn namaz_core_month_json(core: *mut Core) -> *mut c_char {
    if core.is_null() {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    into_c_string(core.month_json())
}

#[no_mangle]
pub extern "C" fn namaz_core_year_json(core: *mut Core) -> *mut c_char {
    if core.is_null() {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    into_c_string(core.year_json())
}

#[no_mangle]
pub extern "C" fn namaz_core_csv_month(core: *mut Core, year: i32, month: i32) -> *mut c_char {
    if core.is_null() || !(1..=12).contains(&month) {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    match CString::new(core.csv_month(year, month as u32)) {
        Ok(text) => text.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_csv_year(core: *mut Core, year: i32) -> *mut c_char {
    if core.is_null() {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    match CString::new(core.csv_year(year)) {
        Ok(text) => text.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_csv_month_calendar(
    core: *mut Core,
    year: i32,
    month: i32,
    calendar: i32,
) -> *mut c_char {
    if core.is_null() || !(1..=12).contains(&month) {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    match CString::new(core.csv_month_calendar(year, month as u32, calendar)) {
        Ok(text) => text.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_csv_year_calendar(
    core: *mut Core,
    year: i32,
    calendar: i32,
) -> *mut c_char {
    if core.is_null() {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    match CString::new(core.csv_year_calendar(year, calendar)) {
        Ok(text) => text.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_csv_hijri_month(
    core: *mut Core,
    hijri_year: i32,
    hijri_month: i32,
) -> *mut c_char {
    if core.is_null() || !(1..=12).contains(&hijri_month) {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    match CString::new(core.csv_hijri_month_by_hijri(hijri_year, hijri_month as u32)) {
        Ok(text) => text.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_csv_hijri_year(core: *mut Core, hijri_year: i32) -> *mut c_char {
    if core.is_null() {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    match CString::new(core.csv_hijri_year_by_hijri(hijri_year)) {
        Ok(text) => text.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_tick(core: *mut Core) {
    if !core.is_null() {
        let _ = unsafe { &mut *core }.tick();
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_tick_events(core: *mut Core) -> *mut c_char {
    if core.is_null() {
        return std::ptr::null_mut();
    }
    into_c_string(unsafe { &mut *core }.tick())
}

#[no_mangle]
pub extern "C" fn namaz_core_startup(core: *mut Core) {
    if core.is_null() {
        return;
    }
    unsafe { &*core }.startup();
}

#[no_mangle]
pub extern "C" fn namaz_core_shift_day(core: *mut Core, days: i32) {
    if !core.is_null() {
        unsafe { &mut *core }.shift_day(days as i64);
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_shift_month(core: *mut Core, months: i32) {
    if !core.is_null() {
        unsafe { &mut *core }.shift_month(months);
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_shift_year(core: *mut Core, years: i32) {
    if !core.is_null() {
        unsafe { &mut *core }.shift_year(years);
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_go_today(core: *mut Core) {
    if !core.is_null() {
        unsafe { &mut *core }.go_today();
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_set_date(core: *mut Core, iso_date: *const c_char) {
    if core.is_null() {
        return;
    }
    let Some(text) = (unsafe { c_str(iso_date) }) else {
        return;
    };
    if let Ok(date) = NaiveDate::parse_from_str(&text, "%Y-%m-%d") {
        unsafe { &mut *core }.set_date(date);
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_set_view(core: *mut Core, view: i32) {
    if !core.is_null() {
        unsafe { &mut *core }.set_view(view);
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_set_location(
    core: *mut Core,
    latitude: f64,
    longitude: f64,
    timezone: f64,
    city: *const c_char,
) {
    if core.is_null() {
        return;
    }
    let core = unsafe { &mut *core };
    let old_today = core.now().date_naive();
    let followed_today = core.selected == old_today;
    core.cfg.latitude = latitude;
    core.cfg.longitude = longitude;
    core.cfg.timezone = timezone;
    if let Some(city) = unsafe { c_str(city) } {
        core.cfg.city = city;
    }
    if followed_today {
        core.selected = core.now().date_naive();
        core.last_date = core.selected;
    }
    core.save();
}

#[no_mangle]
pub extern "C" fn namaz_core_set_hijri_offset(core: *mut Core, offset: i32) {
    if !core.is_null() {
        let core = unsafe { &mut *core };
        core.cfg.hijri_offset = offset.clamp(-3, 3);
        core.save();
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_config_json(core: *mut Core) -> *mut c_char {
    if core.is_null() {
        return std::ptr::null_mut();
    }
    let core = unsafe { &*core };
    into_c_string(json!(core.cfg))
}

#[no_mangle]
pub extern "C" fn namaz_core_default_config_json() -> *mut c_char {
    into_c_string(json!(Config::default()))
}

#[no_mangle]
pub extern "C" fn namaz_core_countries_json() -> *mut c_char {
    let mut names: Vec<String> = all_cities().iter().map(|c| c.country.clone()).collect();
    names.sort();
    names.dedup();
    into_c_string(json!(names))
}

#[no_mangle]
pub extern "C" fn namaz_core_country_cities_json(country: *const c_char) -> *mut c_char {
    let Some(country) = (unsafe { c_str(country) }) else {
        return into_c_string(json!([]));
    };
    let values: Vec<Value> = all_cities()
        .iter()
        .filter(|c| c.country == country)
        .map(|c| {
            json!({
                "name": c.name,
                "state": c.state,
                "lat": c.lat,
                "lon": c.lon,
                "tz": c.tz,
                "tz_name": cities::timezone_name(c.lon, c.lat).unwrap_or_default(),
            })
        })
        .collect();
    into_c_string(json!(values))
}

#[no_mangle]
pub extern "C" fn namaz_core_find_city_json(
    country: *const c_char,
    city: *const c_char,
) -> *mut c_char {
    let (Some(country), Some(city)) = (unsafe { c_str(country) }, unsafe { c_str(city) }) else {
        return into_c_string(Value::Null);
    };
    let found = all_cities()
        .iter()
        .find(|c| c.country == country && c.name == city);
    match found {
        Some(c) => into_c_string(json!({
            "name": c.name,
            "state": c.state,
            "lat": c.lat,
            "lon": c.lon,
            "tz": c.tz,
            "tz_name": cities::timezone_name(c.lon, c.lat).unwrap_or_default(),
        })),
        None => into_c_string(Value::Null),
    }
}

fn json_f64(value: &Value, key: &str, current: f64) -> f64 {
    value.get(key).and_then(Value::as_f64).unwrap_or(current)
}

fn json_i32(value: &Value, key: &str, current: i32) -> i32 {
    value
        .get(key)
        .and_then(Value::as_i64)
        .unwrap_or(current as i64) as i32
}

fn json_bool(value: &Value, key: &str, current: bool) -> bool {
    value.get(key).and_then(Value::as_bool).unwrap_or(current)
}

fn json_string(value: &Value, key: &str, current: &str) -> String {
    value
        .get(key)
        .and_then(Value::as_str)
        .unwrap_or(current)
        .to_string()
}

fn json_i32_array(value: &Value, key: &str, current: [i32; 6]) -> [i32; 6] {
    let mut result = current;
    if let Some(items) = value.get(key).and_then(Value::as_array) {
        for (index, item) in items.iter().take(6).enumerate() {
            result[index] = item.as_i64().unwrap_or(result[index] as i64) as i32;
        }
    }
    result
}

fn json_bool_array(value: &Value, key: &str, current: [bool; 6]) -> [bool; 6] {
    let mut result = current;
    if let Some(items) = value.get(key).and_then(Value::as_array) {
        for (index, item) in items.iter().take(6).enumerate() {
            result[index] = item.as_bool().unwrap_or(result[index]);
        }
    }
    result
}

fn json_string_array(value: &Value, key: &str, current: [String; 6]) -> [String; 6] {
    let mut result = current;
    if let Some(items) = value.get(key).and_then(Value::as_array) {
        for (index, item) in items.iter().take(6).enumerate() {
            if let Some(text) = item.as_str() {
                result[index] = text.to_string();
            }
        }
    }
    result
}

#[no_mangle]
pub extern "C" fn namaz_core_apply_settings_json(core: *mut Core, json_ptr: *const c_char) -> i32 {
    if core.is_null() {
        return 0;
    }
    let Some(text) = (unsafe { c_str(json_ptr) }) else {
        return 0;
    };
    let Ok(value) = serde_json::from_str::<Value>(&text) else {
        return 0;
    };
    let core = unsafe { &mut *core };
    let old_today = core.now().date_naive();
    let followed_today = core.selected == old_today;
    core.cfg.latitude = json_f64(&value, "latitude", core.cfg.latitude).clamp(-90.0, 90.0);
    core.cfg.longitude = json_f64(&value, "longitude", core.cfg.longitude).clamp(-180.0, 180.0);
    core.cfg.timezone = json_f64(&value, "timezone", core.cfg.timezone).clamp(-12.0, 14.0);
    core.cfg.timezone_name = json_string(&value, "timezone_name", &core.cfg.timezone_name);
    core.cfg.dst_mode = json_i32(&value, "dst_mode", core.cfg.dst_mode).clamp(0, 2);
    core.cfg.dst_manual = json_i32(&value, "dst_manual", core.cfg.dst_manual).clamp(0, 1);
    core.cfg.city = json_string(&value, "city", &core.cfg.city);
    core.cfg.province = json_string(&value, "province", &core.cfg.province);
    core.cfg.method = json_i32(&value, "method", core.cfg.method).clamp(0, 8);
    core.cfg.mathhab = if json_i32(&value, "mathhab", core.cfg.mathhab) == 2 {
        2
    } else {
        1
    };
    core.cfg.round = json_i32(&value, "round", core.cfg.round).clamp(0, 3);
    core.cfg.extreme = json_i32(&value, "extreme", core.cfg.extreme).clamp(0, 15);
    core.cfg.fajr_angle = json_f64(&value, "fajr_angle", core.cfg.fajr_angle).clamp(0.0, 30.0);
    core.cfg.ishaa_angle = json_f64(&value, "ishaa_angle", core.cfg.ishaa_angle).clamp(0.0, 30.0);
    core.cfg.dhuhr_minutes = json_i32(&value, "dhuhr_minutes", core.cfg.dhuhr_minutes).clamp(0, 60);
    core.cfg.maghrib_minutes =
        json_i32(&value, "maghrib_minutes", core.cfg.maghrib_minutes).clamp(0, 60);
    if let Some(adjust) = value.get("adjust").and_then(Value::as_array) {
        for (i, item) in adjust.iter().take(6).enumerate() {
            core.cfg.adjust[i] = item.as_i64().unwrap_or(0).clamp(-60, 60) as i32;
        }
    }
    if followed_today {
        core.selected = core.now().date_naive();
        core.last_date = core.selected;
    }
    if core.save() {
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_apply_preferences_json(
    core: *mut Core,
    json_ptr: *const c_char,
) -> i32 {
    if core.is_null() {
        return 0;
    }
    let Some(text) = (unsafe { c_str(json_ptr) }) else {
        return 0;
    };
    let Ok(value) = serde_json::from_str::<Value>(&text) else {
        return 0;
    };
    let core = unsafe { &mut *core };
    core.cfg.athan_enabled = json_bool(&value, "athan_enabled", core.cfg.athan_enabled);
    core.cfg.notifications_enabled = json_bool(
        &value,
        "notifications_enabled",
        core.cfg.notifications_enabled,
    );
    core.cfg.athan_visual_main = json_bool(&value, "athan_visual_main", core.cfg.athan_visual_main);
    core.cfg.athan_visual_flash =
        json_bool(&value, "athan_visual_flash", core.cfg.athan_visual_flash);
    core.cfg.always_on_top = json_bool(&value, "always_on_top", core.cfg.always_on_top);
    core.cfg.hide_on_close = json_bool(&value, "hide_on_close", core.cfg.hide_on_close);
    core.cfg.launch_minimized = json_bool(&value, "launch_minimized", core.cfg.launch_minimized);
    core.cfg.launch_on_startup = json_bool(&value, "launch_on_startup", core.cfg.launch_on_startup);
    core.cfg.hijri_calendar = json_bool(&value, "hijri_calendar", core.cfg.hijri_calendar);
    core.cfg.show_qiblah = json_bool(&value, "show_qiblah", core.cfg.show_qiblah);
    core.cfg.hijri_offset = json_i32(&value, "hijri_offset", core.cfg.hijri_offset).clamp(-3, 3);
    core.cfg.locale = json_string(&value, "locale", &core.cfg.locale);
    core.cfg.dbus_enabled = json_bool(&value, "dbus_enabled", core.cfg.dbus_enabled);
    core.cfg.dbus_interval =
        json_i32(&value, "dbus_interval", core.cfg.dbus_interval).clamp(5, 3600);
    core.cfg.masjid_screen =
        json_i32(&value, "masjid_screen", core.cfg.masjid_screen).clamp(-1, 32);
    core.cfg.qibla_clockwise = json_bool(&value, "qibla_clockwise", core.cfg.qibla_clockwise);
    core.cfg.lock_on_pray_now = json_bool(&value, "lock_on_pray_now", core.cfg.lock_on_pray_now);
    core.cfg.screensaver_on_pray_now = json_bool(
        &value,
        "screensaver_on_pray_now",
        core.cfg.screensaver_on_pray_now,
    );
    core.cfg.show_splash = json_bool(&value, "show_splash", core.cfg.show_splash);
    core.cfg.dua_after_athan = json_bool(&value, "dua_after_athan", core.cfg.dua_after_athan);
    core.cfg.stop_audio_on_prayer = json_bool(
        &value,
        "stop_audio_on_prayer",
        core.cfg.stop_audio_on_prayer,
    );
    core.cfg.reminder_visual_notification = json_bool(
        &value,
        "reminder_visual_notification",
        core.cfg.reminder_visual_notification,
    );
    core.cfg.reminder_visual_main = json_bool(
        &value,
        "reminder_visual_main",
        core.cfg.reminder_visual_main,
    );
    core.cfg.reminder_visual_flash = json_bool(
        &value,
        "reminder_visual_flash",
        core.cfg.reminder_visual_flash,
    );
    core.cfg.reminder_audio_mode =
        json_i32(&value, "reminder_audio_mode", core.cfg.reminder_audio_mode).clamp(0, 5);
    core.cfg.reminder_sound = json_string(&value, "reminder_sound", &core.cfg.reminder_sound);
    core.cfg.reminder_folder = json_string(&value, "reminder_folder", &core.cfg.reminder_folder);
    core.cfg.volume = json_i32(&value, "volume", core.cfg.volume).clamp(0, 200);
    audio::set_volume(core.cfg.volume);
    core.cfg.startup_mode = json_i32(&value, "startup_mode", core.cfg.startup_mode).clamp(0, 4);
    core.cfg.startup_sound = json_string(&value, "startup_sound", &core.cfg.startup_sound);
    core.cfg.startup_file = json_string(&value, "startup_file", &core.cfg.startup_file);
    core.cfg.startup_folder = json_string(&value, "startup_folder", &core.cfg.startup_folder);
    core.cfg.custom_sound_folder =
        json_string(&value, "custom_sound_folder", &core.cfg.custom_sound_folder);
    core.cfg.athan_folder = json_string(&value, "athan_folder", &core.cfg.athan_folder);
    core.cfg.athan_folders =
        json_string_array(&value, "athan_folders", core.cfg.athan_folders.clone());
    core.cfg.athan_modes = json_i32_array(&value, "athan_modes", core.cfg.athan_modes);
    core.cfg.sound_files = json_string_array(&value, "sound_files", core.cfg.sound_files.clone());
    core.cfg.reminders_before =
        json_i32_array(&value, "reminders_before", core.cfg.reminders_before);
    core.cfg.reminders_after = json_i32_array(&value, "reminders_after", core.cfg.reminders_after);
    core.cfg.reminders_before_on =
        json_bool_array(&value, "reminders_before_on", core.cfg.reminders_before_on);
    core.cfg.reminders_after_on =
        json_bool_array(&value, "reminders_after_on", core.cfg.reminders_after_on);
    core.cfg.reminders_before_fixed = json_i32_array(
        &value,
        "reminders_before_fixed",
        core.cfg.reminders_before_fixed,
    );
    core.cfg.reminders_fixed_on =
        json_bool_array(&value, "reminders_fixed_on", core.cfg.reminders_fixed_on);
    core.cfg.iqama = json_i32_array(&value, "iqama", core.cfg.iqama);
    core.cfg.iqama_on = json_bool_array(&value, "iqama_on", core.cfg.iqama_on);
    core.cfg.sounds = json_string_array(&value, "sounds", core.cfg.sounds.clone());
    // Sunrise is not a congregational prayer.
    core.cfg.iqama[1] = 0;
    core.cfg.iqama_on[1] = false;
    core.cfg.athan_modes[1] = 0;
    core.cfg.sounds[1].clear();
    if core.save() {
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_set_flag(core: *mut Core, key: *const c_char, value: i32) {
    if core.is_null() {
        return;
    }
    let core = unsafe { &mut *core };
    let Some(key) = (unsafe { c_str(key) }) else {
        return;
    };
    let on = value != 0;
    match key.as_str() {
        "always_on_top" => core.cfg.always_on_top = on,
        "hide_on_close" => core.cfg.hide_on_close = on,
        "launch_minimized" => core.cfg.launch_minimized = on,
        "launch_on_startup" => core.cfg.launch_on_startup = on,
        "hijri_calendar" => core.cfg.hijri_calendar = on,
        "athan_enabled" => core.cfg.athan_enabled = on,
        "notifications_enabled" => core.cfg.notifications_enabled = on,
        "athan_visual_main" => core.cfg.athan_visual_main = on,
        "athan_visual_flash" => core.cfg.athan_visual_flash = on,
        "show_qiblah" => core.cfg.show_qiblah = on,
        "qibla_clockwise" => core.cfg.qibla_clockwise = on,
        "dua_after_athan" => core.cfg.dua_after_athan = on,
        "stop_audio_on_prayer" => core.cfg.stop_audio_on_prayer = on,
        "reminder_visual_notification" => core.cfg.reminder_visual_notification = on,
        "reminder_visual_main" => core.cfg.reminder_visual_main = on,
        "reminder_visual_flash" => core.cfg.reminder_visual_flash = on,
        "lock_on_pray_now" => core.cfg.lock_on_pray_now = on,
        "screensaver_on_pray_now" => core.cfg.screensaver_on_pray_now = on,
        "dbus_enabled" => core.cfg.dbus_enabled = on,
        "show_splash" => core.cfg.show_splash = on,
        _ => return,
    }
    core.save();
}

#[no_mangle]
pub extern "C" fn namaz_core_play_sound(name: *const c_char) -> i32 {
    match unsafe { c_str(name) } {
        Some(name) if !name.trim().is_empty() && audio::play(&name, 0.9) => 1,
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_play_path(path: *const c_char) -> i32 {
    match unsafe { c_str(path) } {
        Some(path) if audio::play_path(std::path::Path::new(&path), 0.9) => 1,
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_play_folder(folder: *const c_char) -> i32 {
    let Some(folder) = (unsafe { c_str(folder) }) else {
        return 0;
    };
    match audio::pick_from_folder(&folder) {
        Some(path) if audio::play_path(&path, 0.9) => 1,
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_play_quran_random() -> i32 {
    let seed = Utc::now().timestamp_subsec_nanos() as u64;
    if audio::play_quran_random(0.9, seed) {
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_play_random_athan() -> i32 {
    let seed = Utc::now().timestamp_subsec_nanos() as u64;
    match audio::pick_random_athan(seed) {
        Some(path) if audio::play_path(&path, 0.9) => 1,
        _ => 0,
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_play_iqama_random() -> i32 {
    let seed = Utc::now().timestamp_subsec_nanos() as u64;
    if audio::play_iqama(0.9, seed) {
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_set_volume(volume: i32) {
    audio::set_volume(volume);
}

#[no_mangle]
pub extern "C" fn namaz_core_volume() -> i32 {
    audio::volume()
}

#[no_mangle]
pub extern "C" fn namaz_core_is_playing() -> i32 {
    if audio::is_playing() {
        1
    } else {
        0
    }
}

#[no_mangle]
pub extern "C" fn namaz_core_stop_audio() {
    audio::stop();
}

#[no_mangle]
pub extern "C" fn namaz_core_available_sounds_json() -> *mut c_char {
    into_c_string(json!(audio::available_sounds()))
}

#[no_mangle]
pub extern "C" fn namaz_core_athan_sounds_json() -> *mut c_char {
    into_c_string(json!(audio::available_athan_sounds()))
}

#[no_mangle]
pub extern "C" fn namaz_core_sound_exists(name: *const c_char) -> i32 {
    match unsafe { c_str(name) } {
        Some(name) if audio::sound_available(&name) => 1,
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_core(cfg: Config, selected: NaiveDate) -> Core {
        Core {
            cfg,
            selected,
            view: 0,
            seen_events: std::collections::HashSet::new(),
            last_date: selected,
        }
    }

    fn berlin_config() -> Config {
        let mut cfg = Config::default();
        cfg.city = "Berlin, DE".into();
        cfg.latitude = 52.52;
        cfg.longitude = 13.405;
        cfg.timezone = 1.0;
        cfg.timezone_name = "Europe/Berlin".into();
        cfg.dst_mode = 1;
        cfg.method = 5;
        cfg
    }

    fn berlin_manual_config() -> Config {
        let mut cfg = berlin_config();
        cfg.timezone_name.clear();
        cfg.dst_mode = 0;
        cfg.timezone = 2.0;
        cfg
    }

    #[test]
    fn day_json_has_all_prayers() {
        let core = Core::new();
        let value = core.day_json();
        assert_eq!(value["times"].as_array().unwrap().len(), 6);
        assert!(value["qibla"]["bearing"].as_f64().is_some());
    }

    #[test]
    fn month_has_cells() {
        let core = Core::new();
        let value = core.month_json();
        assert!(value["cellsG"].as_array().unwrap().len() >= 28);
    }

    #[test]
    fn automatic_dst_matches_manual_offset_in_summer() {
        let date = NaiveDate::from_ymd_opt(2026, 7, 1).unwrap();
        let auto = test_core(berlin_config(), date);
        let manual = test_core(berlin_manual_config(), date);
        let auto_times = auto.day_json()["times"].as_array().unwrap().clone();
        let manual_times = manual.day_json()["times"].as_array().unwrap().clone();
        for (a, m) in auto_times.iter().zip(manual_times.iter()) {
            assert_eq!(a["short"], m["short"]);
        }
    }

    #[test]
    fn automatic_dst_uses_winter_offset() {
        let date = NaiveDate::from_ymd_opt(2026, 11, 7).unwrap();
        let auto = test_core(berlin_config(), date);
        let manual = test_core(berlin_manual_config(), date);
        let auto_times = auto.day_json()["times"].as_array().unwrap().clone();
        let manual_times = manual.day_json()["times"].as_array().unwrap().clone();
        // CET (+1) is one hour earlier than the summer offset (+2).
        assert_ne!(auto_times[0]["short"], manual_times[0]["short"]);
    }

    #[test]
    fn hijri_csv_has_header_and_rows() {
        let core = test_core(
            Config::default(),
            NaiveDate::from_ymd_opt(2026, 11, 7).unwrap(),
        );
        let csv = core.csv_hijri_month(2026, 11);
        let lines: Vec<&str> = csv.lines().collect();
        assert!(lines[0].starts_with("Hijri Date,Gregorian Date"));
        assert_eq!(lines.len(), 31);
        assert!(lines[1].contains("2026-11-01"));
    }

    #[test]
    fn outlook_csv_has_calendar_events() {
        let core = test_core(
            Config::default(),
            NaiveDate::from_ymd_opt(2026, 11, 7).unwrap(),
        );
        let csv = core.csv_outlook_month(2026, 11);
        let lines: Vec<&str> = csv.lines().collect();
        assert!(lines[0].starts_with("Subject,Start Date,Start Time"));
        assert!(lines.len() > 30);
        assert!(lines[1].starts_with("\"Fajr\""));
    }

    #[test]
    fn csv_year_handles_leap_year() {
        let core = test_core(
            Config::default(),
            NaiveDate::from_ymd_opt(2024, 1, 1).unwrap(),
        );
        let csv = core.csv_year(2024);
        assert_eq!(csv.lines().count(), 367);
        let csv = core.csv_year(2025);
        assert_eq!(csv.lines().count(), 366);
    }

    #[test]
    fn qibla_cardinal_uses_compass_bearings() {
        assert_eq!(qibla_cardinal(294.526907), "West 65.47° North");
        assert_eq!(qibla_cardinal(91.691723), "East 1.69° South");
        assert_eq!(qibla_cardinal(118.99), "East 28.99° South");
        assert_eq!(qibla_cardinal(135.0), "East 45.00° South");
        assert_eq!(qibla_cardinal(0.0), "North 0.00° East");
        assert_eq!(qibla_cardinal(180.0), "South 0.00° West");
    }

    #[test]
    fn config_path_honors_xdg_config_home() {
        let xdg = std::path::Path::new("/tmp/xdg-config");
        let home = std::path::Path::new("/home/user");
        assert_eq!(
            config_path_in(Some(xdg), Some(home)),
            xdg.join("namaz").join("config.toml")
        );
        assert_eq!(
            config_path_in(None, Some(home)),
            home.join(".config").join("namaz").join("config.toml")
        );
        // A relative XDG_CONFIG_HOME is invalid per the XDG base directory
        // specification and must fall back to $HOME.
        assert_eq!(
            config_path_in(Some(std::path::Path::new("relative")), Some(home)),
            home.join(".config").join("namaz").join("config.toml")
        );
    }

    #[test]
    fn dst_transition_days_use_the_new_offset() {
        // Europe/Berlin switches to CEST on 2026-03-29 and back to CET on
        // 2026-10-25.  Fajr must use the offset in effect after the change.
        let mut core = test_core(
            berlin_config(),
            NaiveDate::from_ymd_opt(2026, 3, 29).unwrap(),
        );
        let times = core.day_json()["times"].as_array().unwrap().clone();
        let spring = times[0]["seconds"].as_u64().unwrap();
        assert!(
            (4 * 3600 + 30 * 60..5 * 3600).contains(&spring),
            "spring Fajr {spring}"
        );

        core.selected = NaiveDate::from_ymd_opt(2026, 10, 25).unwrap();
        let times = core.day_json()["times"].as_array().unwrap().clone();
        let autumn = times[0]["seconds"].as_u64().unwrap();
        assert!(
            (4 * 3600 + 30 * 60..5 * 3600).contains(&autumn),
            "autumn Fajr {autumn}"
        );
    }

    #[test]
    fn current_prayer_index_handles_missing_ishaa() {
        let normal = [
            Some(100),
            Some(200),
            Some(300),
            Some(400),
            Some(500),
            Some(600),
        ];
        // Before Fajr the previous day's Ishaa is still the current prayer.
        assert_eq!(current_prayer_index(&normal, 50), 5);
        assert_eq!(current_prayer_index(&normal, 350), 2);
        assert_eq!(current_prayer_index(&normal, 700), 5);

        // A polar day with no Ishaa has no current prayer before Dhuhr.
        let polar = [None, None, Some(300), None, None, None];
        assert_eq!(current_prayer_index(&polar, 100), -1);
        assert_eq!(current_prayer_index(&polar, 350), 2);
        assert_eq!(current_prayer_index(&[None; 6], 350), -1);
    }

    #[test]
    fn custom_athan_without_file_stays_silent() {
        let date = NaiveDate::from_ymd_opt(2026, 11, 8).unwrap();
        let mut cfg = Config::default();
        cfg.dua_after_athan = true;
        cfg.athan_modes[0] = 2;
        cfg.sound_files[0] = "/nonexistent/namaz-custom-athan.wav".into();
        let core = test_core(cfg.clone(), date);
        assert!(core.athan_paths(0, 0).is_empty());

        cfg.athan_modes[0] = 3;
        cfg.athan_folders[0] = "/nonexistent/namaz-custom-folder".into();
        let core = test_core(cfg.clone(), date);
        assert!(core.athan_paths(0, 0).is_empty());

        // The selected/default mode still falls back to the built-in athan.
        cfg.athan_modes[0] = 1;
        cfg.sounds[0].clear();
        let core = test_core(cfg, date);
        assert!(!core.athan_paths(0, 0).is_empty());
    }

    #[test]
    fn qibla_bearing_is_a_stable_compass_bearing() {
        let date = NaiveDate::from_ymd_opt(2026, 11, 7).unwrap();
        let mut cfg = Config::default();
        cfg.qibla_clockwise = true;
        let mut core = test_core(cfg, date);
        let clockwise = core.day_json();
        core.cfg.qibla_clockwise = false;
        let counter = core.day_json();
        assert_eq!(clockwise["qibla"]["bearing"], counter["qibla"]["bearing"]);
        assert_eq!(clockwise["qibla"]["compass"], counter["qibla"]["compass"]);
        assert_eq!(clockwise["qibla"]["cardinal"], counter["qibla"]["cardinal"]);
        assert_eq!(clockwise["qibla"]["clockwise"], true);
        assert_eq!(counter["qibla"]["clockwise"], false);
    }

    #[test]
    fn set_date_updates_selected_day() {
        let mut core = test_core(
            Config::default(),
            NaiveDate::from_ymd_opt(2026, 11, 7).unwrap(),
        );
        core.set_date(NaiveDate::from_ymd_opt(2026, 12, 25).unwrap());
        assert_eq!(core.day_json()["date"]["iso"], "2026-12-25");
    }

    #[test]
    fn due_window_uses_absolute_times() {
        let base = NaiveDate::from_ymd_opt(2026, 11, 7)
            .unwrap()
            .and_hms_opt(10, 0, 0)
            .unwrap();
        assert!(due_at(base, base, 2));
        assert!(due_at(base + ChronoDuration::seconds(2), base, 2));
        assert!(!due_at(base + ChronoDuration::seconds(3), base, 2));
        assert!(!due_at(base - ChronoDuration::seconds(1), base, 2));
        // A missing event just before midnight is still found after midnight.
        let late = NaiveDate::from_ymd_opt(2026, 11, 8)
            .unwrap()
            .and_hms_opt(0, 0, 30)
            .unwrap();
        let event = NaiveDate::from_ymd_opt(2026, 11, 7)
            .unwrap()
            .and_hms_opt(23, 59, 45)
            .unwrap();
        assert!(due_at(late, event, 120));
    }

    #[test]
    fn legacy_extreme_values_are_migrated() {
        for (legacy, expected) in [(1, 10), (2, 11), (3, 6), (4, 7), (5, 5), (0, 0), (15, 15)] {
            let mut cfg = Config::default();
            cfg.extreme = legacy;
            cfg.sounds[1] = "SHURUQ".into();
            cfg.athan_modes[1] = 1;
            cfg.iqama[1] = 12;
            cfg.iqama_on[1] = true;
            migrate_config(&mut cfg);
            assert_eq!(cfg.extreme, expected, "legacy extreme {legacy}");
            assert!(cfg.sounds[1].is_empty());
            assert_eq!(cfg.athan_modes[1], 0);
            assert_eq!(cfg.iqama[1], 0);
            assert!(!cfg.iqama_on[1]);
        }
    }

    #[test]
    fn original_method_angles_are_preserved() {
        let cfg = Config::default();
        let egypt = app_method(1, &cfg);
        assert_eq!((egypt.fajr_ang, egypt.ishaa_ang), (19.5, 17.5));
        let isna = app_method(4, &cfg);
        assert_eq!((isna.fajr_ang, isna.ishaa_ang), (15.0, 15.0));
        let mwl = app_method(5, &cfg);
        assert_eq!((mwl.fajr_ang, mwl.ishaa_ang), (18.0, 17.0));
        let umm = app_method(6, &cfg);
        assert_eq!(
            (umm.fajr_ang, umm.ishaa_ang, umm.ishaa_inv),
            (15.0, 0.0, 90)
        );
    }

    #[test]
    fn year_end_matches_last_row() {
        let core = test_core(
            Config::default(),
            NaiveDate::from_ymd_opt(2026, 11, 8).unwrap(),
        );
        let value = core.year_json();
        let rows = value["rows"].as_array().unwrap();
        assert_eq!(rows.len(), 365);
        assert_eq!(value["endIso"], rows.last().unwrap()["iso"]);
    }

    #[test]
    fn polar_day_without_fajr_never_uses_fajr_as_next() {
        let mut cfg = Config::default();
        cfg.latitude = 89.0;
        cfg.longitude = 0.0;
        cfg.timezone = 0.0;
        cfg.timezone_name.clear();
        cfg.dst_mode = 0;
        cfg.extreme = 0;
        let today = local_now(&cfg).date_naive();
        let core = test_core(cfg, today);
        let value = core.day_json();
        assert_eq!(value["times"][0]["seconds"], Value::Null);
        // Dhuhr is still defined during a polar night, so the next prayer may
        // legitimately be Dhuhr.  What must never happen is Fajr being chosen
        // when its time is invalid.
        let next = value["next"]["index"].as_i64().unwrap();
        assert_ne!(next, 0);
        if next < 0 {
            assert_eq!(value["next"]["remaining"], -1);
            assert_eq!(value["next"]["countdown"], "");
        } else {
            assert!(value["next"]["remaining"].as_i64().unwrap() >= 0);
        }
    }

    #[test]
    fn non_today_day_has_no_current_prayer() {
        let core = test_core(
            Config::default(),
            NaiveDate::from_ymd_opt(2000, 1, 1).unwrap(),
        );
        let value = core.day_json();
        assert_eq!(value["current"]["index"], -1);
        assert_eq!(value["current"]["name"], "");
        assert_eq!(value["current"]["windowTitle"], "Namaz");
        assert_eq!(value["next"]["index"], -1);
    }

    #[test]
    fn hijri_grid_respects_offset() {
        for offset in [-1, 0, 1] {
            let mut cfg = Config::default();
            cfg.hijri_offset = offset;
            let selected = NaiveDate::from_ymd_opt(2026, 11, 5).unwrap();
            let core = test_core(cfg, selected);
            let day = core.day_json();
            let month = core.month_json();
            let hijri_day = day["date"]["hijriDay"].as_i64().unwrap();
            let selected_cells: Vec<_> = month["cellsH"]
                .as_array()
                .unwrap()
                .iter()
                .filter(|cell| cell["selected"].as_bool().unwrap_or(false))
                .collect();
            assert_eq!(selected_cells.len(), 1, "offset {offset}");
            assert_eq!(selected_cells[0]["day"].as_i64(), Some(hijri_day));
            assert_eq!(selected_cells[0]["iso"], "2026-11-05");
        }
    }

    #[test]
    fn hijri_csv_uses_a_real_hijri_month() {
        let core = test_core(
            Config::default(),
            NaiveDate::from_ymd_opt(2026, 11, 5).unwrap(),
        );
        let csv = core.csv_hijri_month_by_hijri(1448, 5);
        let lines: Vec<&str> = csv.lines().collect();
        assert!(lines[0].starts_with("Hijri Date,Gregorian Date"));
        assert!(
            lines.len() == 30 || lines.len() == 31,
            "rows: {}",
            lines.len()
        );
        assert!(lines[1].starts_with("1448-05-01"));
        assert!(lines.last().unwrap().starts_with("1448-05-"));
    }

    #[test]
    fn tick_delivers_iqama_after_midnight() {
        let mut cfg = Config::default();
        cfg.city = "Palembang".into();
        cfg.latitude = -2.9055;
        cfg.longitude = 104.7612;
        cfg.timezone = 7.0;
        cfg.athan_enabled = false;
        cfg.notifications_enabled = false;
        cfg.reminder_visual_notification = false;
        cfg.iqama_on = [false; 6];
        cfg.iqama = [0; 6];
        // Make the Ishaa iqama cross midnight.
        let date = NaiveDate::from_ymd_opt(2026, 11, 8).unwrap();
        let times = get_prayer_times(
            &location(&cfg, date),
            &method(&cfg),
            &Date::new(2026, 11, 8),
        );
        let ishaa = times[5].seconds_of_day().unwrap() as i64;
        let minutes = ((24 * 3600 - ishaa) / 60 + 10) as i32;
        cfg.iqama[5] = minutes;
        cfg.iqama_on[5] = true;
        let mut core = test_core(cfg, date);
        let offset = FixedOffset::east_opt(7 * 3600).unwrap();
        let now = offset
            .with_ymd_and_hms(2026, 11, 9, 0, 10, 5)
            .single()
            .unwrap();
        let events = core.tick_at(now);
        let events = events.as_array().unwrap();
        assert_eq!(events.len(), 1, "events: {events:?}");
        assert_eq!(events[0]["type"], "reminder");
        assert!(events[0]["body"].as_str().unwrap().contains("Iqama"));
        // The same event must not fire twice.
        let again = core.tick_at(now + ChronoDuration::seconds(1));
        assert!(again.as_array().unwrap().is_empty());
    }

    #[test]
    fn tick_catches_up_athan_after_sleep() {
        let mut cfg = Config::default();
        cfg.city = "Palembang".into();
        cfg.latitude = -2.9055;
        cfg.longitude = 104.7612;
        cfg.timezone = 7.0;
        cfg.athan_enabled = false;
        cfg.notifications_enabled = false;
        cfg.reminder_visual_notification = false;
        let date = NaiveDate::from_ymd_opt(2026, 11, 8).unwrap();
        let times = get_prayer_times(
            &location(&cfg, date),
            &method(&cfg),
            &Date::new(2026, 11, 8),
        );
        let fajr = times[0].seconds_of_day().unwrap() as i64;
        let mut core = test_core(cfg, date);
        let offset = FixedOffset::east_opt(7 * 3600).unwrap();
        let now = offset
            .with_ymd_and_hms(2026, 11, 8, 0, 0, 0)
            .single()
            .unwrap()
            + ChronoDuration::seconds(fajr + 90);
        let events = core.tick_at(now);
        assert!(events
            .as_array()
            .unwrap()
            .iter()
            .any(|event| event["type"] == "athan"));
    }

    #[test]
    fn athan_sound_list_contains_only_athans() {
        let sounds = audio::available_athan_sounds();
        // The test binary is not installed, so the bundled audio directory is
        // the source of truth.  It must expose the adhan and nothing else.
        if !sounds.is_empty() {
            assert!(sounds.contains(&"ADHAN".to_string()));
        }
        assert!(!sounds.iter().any(|name| name.starts_with("IQAMA")));
        assert!(!sounds.iter().any(|name| name.starts_with("DUA")));
        assert!(!sounds.iter().any(|name| name == "ALIHLAS"));
    }

    #[test]
    fn config_roundtrip_and_corrupt_backup() {
        let dir = std::env::temp_dir().join(format!(
            "namaz-core-test-{}-{}",
            std::process::id(),
            Utc::now().timestamp_nanos_opt().unwrap_or(0)
        ));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("config.toml");
        let mut cfg = Config::default();
        cfg.city = "Roundtrip".into();
        cfg.hijri_offset = 2;
        cfg.extreme = 10;
        assert!(save_config_to(&cfg, &path));
        let loaded = load_config_from(&path);
        assert_eq!(loaded.city, "Roundtrip");
        assert_eq!(loaded.hijri_offset, 2);
        assert_eq!(loaded.extreme, 10);

        std::fs::write(&path, "this is not toml [[[").unwrap();
        let fallback = load_config_from(&path);
        assert_eq!(fallback.city, Config::default().city);
        assert!(path.with_extension("toml.broken").exists());
        let _ = std::fs::remove_dir_all(&dir);
    }
}
