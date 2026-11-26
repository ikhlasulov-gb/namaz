# Namaz — guide for AI agents

This file is for a future AI developer or coding agent working on Namaz.  It
documents the project invariants, the build and test flow, and the release
process.

> This file is public.  Never put a private key, a token, a passphrase, a
> personal file path or any other machine-specific detail into it.
> Maintainer-only secrets and access notes belong outside the repository.

## GitHub access

- Repository: `https://github.com/ikhlasulov/namaz`
- Remote: `origin` → the same URL (HTTPS)
- Account: `ikhlasulov`
- CLI: `gh` (authenticate with `gh auth login` if needed)

Check access with:

```bash
gh auth status
git ls-remote --heads origin main
```

Never print the token, write it into the repository, or add it to a workflow
file.  GitHub Actions receives it automatically as `GITHUB_TOKEN`.

## Release signing

The Flatpak repository is signed in CI with the repository secrets
`FLATPAK_GPG_PRIVATE_KEY` and `FLATPAK_GPG_KEY_ID`.  The private key is never
stored in this repository.

If the key is compromised, revoke it, generate a new key, update the secrets
and tell users to re-add the remote.  Never silently replace the key.

## Build and test

```bash
cargo fmt --all -- --check
cargo clippy --workspace --lib --bins -- -D warnings
cargo test -p prayer-core -p namaz-core
cmake -S ui -B ui/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -G Ninja
cmake --build ui/build -j"$(nproc)"
QT_QPA_PLATFORM=offscreen ctest --test-dir ui/build --output-on-failure
./scripts/ui-check.sh
./scripts/diff-itl.sh 1500
```

The helper scripts run locally by default.  Set `NAMAZ_CONTAINER` to run them
inside an existing Podman container.

Build artifacts are ignored: `target/`, `ui/build/`, `ui/build-release/`,
`dist/`, `.flatpak-builder/`, `build-appimage/`, `.cache/`.  Do not commit
them.

### Flatpak

```bash
flatpak run --share=network org.flatpak.Builder \
  --state-dir="$HOME/namaz-flatpak-state" --disable-rofiles-fuse \
  --user --install --force-clean --repo="$HOME/namaz-flatpak-repo" \
  "$HOME/namaz-flatpak-build" packaging/io.github.ikhlasulov.Namaz.yml
```

The manifest uses vendored Cargo sources (`packaging/cargo-sources.json`) and
needs no network during the build.  Regenerate that file after `Cargo.lock`
changes:

```bash
python3 flatpak-cargo-generator.py Cargo.lock -o packaging/cargo-sources.json
```

### Portable and AppImage

```bash
./scripts/build-portable.sh ui/build-release/namaz \
  ui/build-release/cargo/release/libnamaz_core.so
./scripts/build-appimage.sh ui/build-release/namaz \
  ui/build-release/cargo/release/libnamaz_core.so
```

The AppImage script downloads architecture-specific `linuxdeploy` tools into
`.cache/linuxdeploy` and sets `NO_STRIP=1` because the bundled strip cannot
read RELR sections.  CI smoke-tests the AppImage under Xvfb.

## Release flow

1. Add a `## [X.Y.Z]` section to `CHANGELOG.md`.
2. From a clean checkout run:

   ```bash
   ./scripts/release.sh X.Y.Z
   ```

3. `scripts/release.sh` updates `VERSION`, `Cargo.toml`, `Cargo.lock` and the
   AppStream metadata, commits, creates `vX.Y.Z` and pushes the branch and tag.
4. `.github/workflows/release.yml` builds Flatpak (x86_64 + aarch64), portable
   archives, AppImages, signs the OSTree repo, publishes the GitHub release
   and deploys GitHub Pages.
5. Verify:

   ```bash
   gh release view vX.Y.Z -R ikhlasulov/namaz
   curl -sI https://ikhlasulov.github.io/namaz/namaz.flatpakrepo
   flatpak remote-ls --user namaz
   flatpak install --user -y --reinstall namaz io.github.ikhlasulov.Namaz
   flatpak run io.github.ikhlasulov.Namaz --version
   ```

Do not move a published tag.  The early `v1.0.0` tag was moved while the
project was still being set up; future releases must be immutable.  If a
release is broken, publish the next patch version.

## Behaviour invariants (do not regress)

- **No stale adhan.** `EVENT_GRACE_SECONDS` in `crates/core/src/lib.rs` is
  deliberately `5` seconds.  Athan, iqama and reminders must only fire close
  to their scheduled time.  Do not restore the old 15-minute catch-up: it
  played the adhan when the app started ten minutes late or the machine woke
  from sleep.  The guard tests are `tick_plays_athan_on_time` and
  `tick_does_not_replay_stale_athan`.
- **Journal reminders never get lost.** The tracker reports the prayer that is
  currently due through `PrayerTracker::activeReminder()`; the UI decides how
  often to repeat it with `ReminderRepeat` (`repeat_minutes`, default 5, and
  `repeat_limit`, default 12, in `tracker.json`).  Repeats are deliberate:
  a popup closed with the cross, suppressed by Do-Not-Disturb or lost while the
  screen was off must come back, with a short ping from
  `namaz_core_play_reminder_ping()`.  Only the visual reminder repeats — the
  adhan audio must never be replayed, and `ping()` stays silent while anything
  is playing.  `notified` is informational and is set only after the
  notification daemon accepts the message, so it must not gate reminders.
  The tray blink is a one-shot visual hint: `flashTray()` ignores a blink that
  is already running, so repeats cannot extend it, and `stopReminderFlash()`
  ends it as soon as the reminder is confirmed, skipped or withdrawn.  The
  journal reminder flashes only on its first beat and only when
  `reminder_visual_flash` is on; the athan and core reminders keep their own
  bounded blink.
  Guard tests: `activeReminderTracksTheOpenWindow`,
  `lateStartInsideWindowRepeatsUntilConfirmed`, `reminderRepeatCadence`.
- **Flatpak icon theme.** The manifest must install
  `/usr/share/icons/hicolor/index.theme` as
  `/app/share/icons/hicolor/index.theme`.  Without it Qt cannot resolve
  `QIcon::fromTheme("io.github.ikhlasulov.Namaz")`, which leaves the window
  and tray icon empty.  `Ui::appIcon()` also falls back to the embedded PNG
  when a theme icon has no pixmaps.
- **Spaces in the project path.** The build must work from directories whose
  names contain spaces and non-ASCII characters.  `ui/CMakeLists.txt`
  generates `namaz_ru.qm` with a direct `lrelease` command instead of deferred
  `qt_add_translations()`.  Keep at least one test build in such a path.

## Project invariants

- Application ID: `io.github.ikhlasulov.Namaz`
- Binary: `namaz`
- Desktop file / icon name: `io.github.ikhlasulov.Namaz`
- D-Bus service: `io.github.ikhlasulov.Namaz`, object `/Namaz`
- Config: `$XDG_CONFIG_HOME/namaz/config.toml`
- Audio env: `NAMAZ_WAVE_DIR`
- Version: `VERSION` is the single source of truth; tag is `vX.Y.Z`
- Licence: GPL-3.0-or-later
- Free assets only:
  - audio: Quran recitation (Bismillah, Al-Ikhlas) by Mishary Rashid Alafasy, adhan by Ahmad bin Abdul-Aziz Al-Nufais (no licence stated)
  - cities: GeoNames `cities5000` (`assets/cities.json`, CC BY 4.0)
  - no Salaat Time executables, DLLs, WAVs, screenshots or city XML

## Useful URLs

- Repository: https://github.com/ikhlasulov/namaz
- Pages: https://ikhlasulov.github.io/namaz/
- Flatpak remote: https://ikhlasulov.github.io/namaz/namaz.flatpakrepo
- Releases: https://github.com/ikhlasulov/namaz/releases
