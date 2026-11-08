# Changelog

All notable changes to Namaz are documented here.  The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

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
- CC0 adhan, Bismillah and Al-Ikhlas recordings by Ikhlasulov.
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
