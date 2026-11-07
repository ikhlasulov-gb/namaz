# Architecture

Namaz is split into a UI-independent Rust core and a Qt 6 Widgets frontend.
The C++ side talks to Rust through a small C ABI; complex state is exchanged
as JSON.

```text
crates/prayer-core   pure prayer-time and astronomy engine (ITL port)
crates/core          config, calendars, Hijri, Qibla, audio, reminders, C ABI
ui/src               Qt 6 Widgets: main window, views, dialogs, tray, D-Bus
ui/tests             Qt Test suite
assets/              CC0 audio, GeoNames city database, icons
packaging/           Flatpak, desktop entry, AppStream, Pages landing page
scripts/             build, release, smoke tests
third_party/itl      original LGPL ITL C sources (test reference)
```

## Prayer engine

`crates/prayer-core` is a direct port of the Islamic Times Library.  The
original C sources stay in `third_party/itl` so `scripts/diff-itl.sh` can
compare both implementations over random locations, dates and methods.

## State core

`crates/core` owns:

- configuration in `XDG_CONFIG_HOME/namaz/config.toml` (with migration from
  the old `salaat-time` directory),
- day/month/year JSON consumed by the UI,
- Hijri conversion, Qibla bearing and city lookup,
- audio playback through `rodio` (PipeWire/PulseAudio/ALSA),
- reminders, iqama scheduling and D-Bus notifications.

The C ABI is declared in `ui/src/Core.h` and implemented at the bottom of
`crates/core/src/lib.rs`.  Every returned string must be freed with
`namaz_core_free_string`.

## Frontend

`MainWindow` (`ui/src/Widgets.cpp`) builds the menu, toolbar, tray, shortcuts
and status bar.  `Views.cpp` contains Day/Month/Year views and the Masjid
window; `Dialogs.cpp` contains Prayer Settings and Preferences.  `DBusService`
exports the session-bus API.  `UpdateChecker` performs the optional GitHub
Releases request.

## Configuration and migration

The config directory is `$XDG_CONFIG_HOME/namaz/config.toml` (or
`~/.config/namaz/config.toml`).  On first start the core copies a config from
`$XDG_CONFIG_HOME/salaat-time/config.toml` or
`~/.config/salaat-time/config.toml` and maps legacy audio names to the free
recordings.  QSettings (window geometry, selected tab, update preferences)
also migrates from the old organization/application names.

## Packaging

- Flatpak is the primary distribution and bundles Qt, the app and the core.
- Portable archives and AppImage builds are produced from the same CMake
  release build.
- The Flatpak manifest uses vendored Cargo sources and needs no network during
  the build.
