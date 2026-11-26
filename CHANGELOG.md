# Changelog

All notable changes to Namaz are documented here.  The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [1.1.12] - 2026-11-26

### Changed

- The project moved to the ikhlasulov account.  The Flatpak and desktop
  application ID is now `io.github.ikhlasulov.Namaz` (previously
  `io.github.ikhlasulov_gb.Namaz`), so the Flatpak remote has to be added
  again once.  Repository, Pages and update-check URLs now point to
  github.com/ikhlasulov.  The Flatpak repository is signed with a new key
  (`AA761593D3FB540FEE04EBB0C1AFFFEA2652299E`).

## [1.1.11] - 2026-11-23

### Fixed

- Desktop notifications no longer fail for a whole session when Namaz starts
  before the notification daemon.  The autostarted application used to check
  for `org.freedesktop.Notifications` once at startup; when it came up before
  plasmashell (or plasmashell restarted later in the session), every prayer
  reminder was silently diverted to the in-window banner on the hidden window
  for the rest of the run.  The notification interface is now acquired lazily
  on each reminder beat and reconnects automatically after the daemon appears
  or restarts; the fallback banner is only used for the beat that actually
  failed.

## [1.1.10] - 2026-11-23

### Fixed

- The tray icon no longer blinks forever.  The blink used to be re-armed every
  time a repeating journal reminder was posted again, so it could keep going
  for the whole prayer window and did not stop when the prayer was confirmed or
  skipped.  The blink now starts once when the reminder first appears, stops as
  soon as the reminder is resolved or withdrawn, and follows the "Flash the
  tray icon" preference in Preferences → Journal (the fallback banner used to
  flash even when that option was off).

## [1.1.9] - 2026-11-22

### Added

- Journal reminders now repeat while the prayer is still due.  Every five
  minutes (configurable in Preferences → Journal) the reminder is posted again
  with a short two-tone beep, up to twelve times per prayer window.  A popup
  closed with the cross, swallowed by Do-Not-Disturb or lost while the screen
  was off therefore comes back instead of disappearing for the rest of the day.
  The adhan audio itself is never replayed.

### Fixed

- A pending journal reminder can no longer be lost.  The reminder no longer
  depends on a one-shot "already shown" flag: the record stays active for as
  long as its prayer window is open, the flag is only set after the
  notification daemon accepts the message, the silent 30-minute drop of a
  reminder held back during the adhan is gone, and the prayer that is currently
  due is announced on startup even if an older build marked it as shown.

## [1.1.8] - 2026-11-21

### Added

- The journal editor offers a new "Восполнен" (made up) status.  Marking a
  missed or skipped prayer as made up shows that it was later performed as
  qada without changing the prayer it belonged to; the qada debt treats such
  a record as neutral — the miss and its compensation cancel each other out,
  exactly like a separate make-up record does.

### Fixed

- Quitting from the system-tray context menu no longer risks leaving a
  Namaz process behind on older Qt builds.  The quit lock used by
  plasma-integration (KDE bug 484405, Qt < 6.7.2) is now disabled, so every
  exit path ends with the explicit `QCoreApplication::quit()`.

## [1.1.7] - 2026-11-21

### Fixed

- Prayer reminders no longer pile up in the notification area.  A new
  countdown, athan or journal reminder now replaces the previous notification
  from both the core and the tracker, so after a long absence only one message
  is shown instead of one per past prayer.

## [1.1.6] - 2026-11-20

### Fixed

- Prayer-time notifications are no longer lost when Namaz runs under KDE
  Plasma while a fullscreen window (a game or video) is focused or
  Do-Not-Disturb is enabled: Plasma 6 suppresses normal-urgency popups in
  those situations.  Both the journal Confirm/Skip reminders and the core's
  plain notifications are now posted with critical urgency, which Plasma
  delivers in both cases, matching their alarm-like nature (the adhan audio
  already plays regardless of the notification).

## [1.1.5] - 2026-11-20

### Fixed

- The journal reminder with Confirm and Skip is now shown even when Namaz
  starts or resumes after the adhan has already begun, as long as the prayer
  window is still open.  Each window raises its own reminder once, the mark
  is persisted so a restart does not repeat it, and closed windows stay
  silent.  The adhan audio keeps the strict five-second rule.  The
  notification also asks the daemon to keep it on screen until it is
  confirmed, skipped or closed.

## [1.1.4] - 2026-11-20

### Fixed

- Journal reminders are no longer silently disabled for the whole day when
  Namaz starts after being closed overnight (or keeps running past midnight).
  The new day used to be filled in as history, which marked every reminder as
  already shown while the tracker replaced the core's plain notification.  A
  tracker file written by an affected build is repaired on the first tick, so
  the reminders that are still ahead today are delivered.

## [1.1.3] - 2026-11-19

### Fixed

- The Journal's Today page now follows the clock while it is open.  When a
  prayer time starts, its Confirm and Skip buttons become available without
  switching to History and back.
- When an adhan is playing, the journal reminder with Confirm and Skip waits
  until the recitation has finished instead of appearing (and often
  disappearing) at the start of the adhan.

## [1.1.2] - 2026-11-19

### Fixed

- A missed prayer no longer raises its own "Missed: …" notification.  The
  journal still records the miss, but the notification area keeps only the
  reminder for the upcoming prayer.
- Quitting Namaz now closes the desktop notifications posted by the core as
  well (prayer times, reminders and iqama), so no stale notification is left
  on screen after the application has exited.

## [1.1.1] - 2026-11-18

### Fixed

- The journal shows Witr at the beginning of the last third of the night
  instead of repeating Ishaa's time.
- Witr is opt-in in the journal: it is tracked and listed only when "Track
  Witr in the journal" (or one of its reminders) is enabled.
- A new prayer reminder replaces the previous one instead of stacking, and
  the prayer whose window just ended is reported as missed in the same
  notification.  A missed prayer without a following prayer gets its own
  notification at the deadline.
- Journal notifications are dismissed when the application quits, so no
  message with dead Confirm and Skip buttons is left behind.

## [1.1.0] - 2026-11-18

### Added

- The prayer journal ("Журнал"), a fourth tab next to Day, Month and Year
  (Ctrl+4).  It tracks the five obligatory prayers and Witr: today's list
  with Confirm/Skip, the automatic `missed` mark at the end of every prayer
  window, a qada debt counter (missed +1, make-up −1, skip neutral),
  searchable history with editing, deletion, auto-pruning and CSV export.
- Witr is computed by the Rust core as the last third of the night
  (Maghrib → Fajr of the next day) and is exposed in the day JSON together
  with absolute `epoch` values for every prayer time.
- Journal notifications with Confirm/Skip buttons over
  `org.freedesktop.Notifications`.  When the tracker is enabled they replace
  the plain core notification for the tracked prayers; the adhan and the
  reminder sounds stay unchanged.  Without a notification daemon the window
  shows its own Confirm/Skip banner.
- Tray actions "Confirm Next Prayer" and "Open Journal", and a "Journal"
  page in Preferences with per-prayer notifications, auto-confirm, the
  starting qada debt, the qada reminder threshold, the history retention
  period, CSV export and a data reset.

### Changed

- The journal UI is plain Breeze Qt, like the rest of the application: the
  Today rows are ordinary widgets without custom frames, per-prayer colours
  or icons, and the in-tab settings sub-page is gone — all journal options
  live on the Journal page in Preferences.

## [1.0.14] - 2026-11-17

### Added

- A calculation method for Indonesia and Singapore.  Kemenag (Kementerian
  Agama Republik Indonesia) and MUIS (Majlis Ugama Islam Singapura) both use
  Fajr 20° and Ishaa 18°, so one selection covers both countries.

## [1.0.13] - 2026-11-16

### Fixed

- The selected date now follows the new day after midnight.  Previously a
  long-running window stayed on the previous date, so the next prayer and its
  countdown disappeared until "Today" was pressed.
- The tray tooltip and its "Next" menu item keep showing the real next prayer
  while another date is selected in the calendar.

## [1.0.12] - 2026-11-13

### Fixed

- A failed ALSA write no longer sends the audio worker into a 100 % CPU loop.
  cpal 0.15.3 retried `writei()` forever when the device returned an error
  (for example after a PipeWire restart or when an ALSA file PCM hits
  `ENOSPC`).  The vendored copy in `third_party/cpal` returns the error,
  backs off, and stops the worker after repeated failures.  `Stop` and
  `Stream::drop()` no longer risk hanging in `join()`, and the playback
  watchdog can detect the stalled sink and release the device.

## [1.0.11] - 2026-11-12

### Fixed

- The output device is released as soon as playback ends or is stopped.
  cpal's ALSA worker keeps polling an open device even with an empty sink, so
  a device lost while idle could spin in its error callback and consume a
  whole CPU core without printing anything.  The next playback reopens the
  device.

## [1.0.10] - 2026-11-11

### Changed

- The bundled recordings are credited to their reciters instead of being
  described as original CC0 recordings: the adhan is by Ahmad bin
  Abdul-Aziz Al-Nufais, the Bismillah and Al-Ikhlas recitation by Mishary
  Rashid Alafasy.  The reciters did not state a licence.

## [1.0.9] - 2026-11-11

### Fixed

- The AppImage build downloads `linuxdeploy` again: `fetch_tool` received its
  arguments in the wrong order, so `curl` was handed a local path instead of
  the release URL and every AppImage job failed.  1.0.8 therefore shipped
  without AppImage packages.

## [1.0.8] - 2026-11-11

### Added

- `cmake --install` now installs a complete layout for distributions: the
  binary, `libnamaz_core.so`, desktop entry, AppStream metadata, every hicolor
  icon size plus the scalable SVG, the bundled recordings and the licence
  files.  `CMAKE_INSTALL_LIBDIR` is respected and the install RPATH is derived
  from it, so `lib64` systems work without a packaging patch.
- `NAMAZ_CARGO_OFFLINE=ON` builds the Rust core with `cargo --offline`, which
  lets packagers build from a source archive with vendored dependencies.

### Changed

- `assets/audio/LICENSE` is now `assets/audio/audio.LICENSE`, matching the
  installed name and `assets/cities.LICENSE`.

### Fixed

- A lost audio device no longer leaves playback stuck.  When the output stream
  stops delivering samples (unplugged headset, PipeWire restart,
  suspend/resume), the worker closes the stream instead of keeping the
  "playing" flag set forever and losing the current adhan silently.
- The flood of `an error occurred on output stream: A backend-specific error
  has occurred: alsa::poll() returned POLLERR` lines is gone: rodio is built
  with its `tracing` feature, so a single device failure no longer prints the
  same message from every poll iteration, and the watchdog reports the failure
  once.

## [1.0.7] - 2026-11-08

### Fixed

- The prayer-times table contour is now a visible one-pixel border derived
  from the palette; the themed frame was invisible against the card in
  Breeze Dark.

## [1.0.6] - 2026-11-08

### Changed

- The prayer-times table now has a subtle one-pixel frame instead of
  blending into the card.
- The "No athan will be played." message is shown on a field-like surface,
  consistent with the combo boxes around it, instead of plain muted text.
- The time-format choices use template patterns (`hh:mm AM/PM`,
  `hh:mm:ss AM/PM`, `HH:mm`, `HH:mm:ss`) instead of a hard-coded example
  time.

## [1.0.5] - 2026-11-08

### Added

- Time-format preference: 12-hour or 24-hour clock, with or without seconds
  for the current time (`08:00 PM`, `08:00:33 PM`, `20:00`, `20:00:33`).
  The 12/24-hour choice applies to every displayed time; the seconds choice
  applies to the live clock and the Masjid screen.

## [1.0.4] - 2026-11-08

### Fixed

- The Prayer Settings and Preferences dialogs now open large enough to
  show the tallest page without a vertical scrollbar on a normal desktop
  screen.
- The Apply button starts disabled, becomes enabled only after a change and
  is disabled again after applying. "Restore Defaults" enables it so the
  restored values can be applied.

## [1.0.3] - 2026-11-08

### Fixed

- The "Raka'as" column in the Day view is now centred between the prayer
  name and the time, instead of being pushed against the time column.

## [1.0.2] - 2026-11-08

### Fixed

- The Day view now uses the full card width: the Gregorian and Hijri dates
  share one line, the current prayer and the next-prayer countdown sit on the
  right, and the cards no longer leave a large empty area beside their text.
- The Qiblah compass ring and ticks stay visible in dark colour schemes
  instead of merging into the card background.
- The prayer-times table no longer draws an unexplained line below the last
  row, blends into the card background and is centred vertically.
- The status bar no longer reserves space for the size grip after the method
  label.
- "Manual +1 hour" daylight saving now really adds one hour; the redundant
  0/1-hour spinner that could contradict the selected mode is gone.
- The Masjid-screen box in Preferences no longer leaves an empty area.

### Removed

- "Play Dua after Athan". The bundled adhan already contains the dua and no
  standalone dua recording is shipped, so the option never did anything.
- The duplicate "Bismi Allah" startup selector; the startup mode already
  chooses the built-in Bismillah recording.
- The "Iqama" reminder-audio choice: no iqama recording is shipped and the
  option always stayed silent.
- The twilight-angle fields are disabled unless the Custom method is
  selected, because preset methods ignore them.

## [1.0.1] - 2026-11-08

### Fixed

- The application icon now resolves inside Flatpak.  `/app/share/icons` now
  contains the hicolor `index.theme`, so Qt can load the themed icon, and the
  embedded PNG is used as a fallback when a theme cache is incomplete.
- Athan, iqama and reminders are only delivered within a few seconds of their
  scheduled time.  Starting the application late, or waking the computer after
  the prayer time, no longer plays a stale adhan; a muadhdhin does not start
  ten minutes after the prayer either.
- CMake builds from project paths containing spaces and Cyrillic characters by
  generating the Russian `.qm` file with a direct `lrelease` command.

## [1.0.0] - 2026-11-08

### Added

- First public release of Namaz.
- Day, Month and Year prayer-time views with Gregorian and Hijri calendars.
- Rust prayer engine ported from the Islamic Times Library (ITL).
- Calculation methods, Standard/Hanafi Asr and high-latitude rules.
- Qibla compass with true bearing and cardinal direction.
- Adhan, reminders, iqama, custom audio files and folders.
- System tray, desktop notifications, D-Bus API, XDG autostart and Masjid
  full-screen view.
- PDF and CSV export, including Outlook CSV.
- English and Russian interface.
- Flatpak packaging with a signed OSTree repository, portable archives and
  AppImage builds.
- In-app update check against GitHub Releases with AppImage self-update.
- Bundled adhan, Bismillah and Al-Ikhlas recordings.
- GeoNames `cities5000` city database (CC BY 4.0).

### Changed

- The project is now called **Namaz**, with the application ID
  `io.github.ikhlasulov_gb.Namaz`.
- Settings from the old working title are migrated automatically.
- The default audio set is fully free; missing dua/iqama files no longer
  appear as selectable actions.

### Fixed

- Prayer-time calculations, DST transitions, polar latitudes and Qibla
  quadrant handling from the pre-release work.
- Flatpak config, autostart, CUPS, notifications and tray integration.
