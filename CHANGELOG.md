# Changelog

All notable changes to Namaz are documented here.  The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

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
