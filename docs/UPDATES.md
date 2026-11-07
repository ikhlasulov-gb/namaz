# Updates

## In-app check

`ui/src/UpdateChecker.cpp` performs one GET request to:

```text
https://api.github.com/repos/ikhlasulov-gb/namaz/releases/latest
```

- Automatic check: at most once every 24 hours, 15 seconds after startup.
- Manual check: `Help → Check for Updates…`.
- The result is cached in QSettings (`updates/lastCheck`).
- “Skip this version” stores `updates/skippedVersion`.
- No identifiers, config values or telemetry are sent.
- `NAMAZ_UPDATE_URL` can override the endpoint for testing.

## Flatpak

Flatpak updates are delivered by the signed OSTree repository on GitHub Pages:

```bash
flatpak update --user io.github.ikhlasulov_gb.Namaz
```

The in-app dialog detects Flatpak and offers to copy the command.  Software
centres (GNOME Software, KDE Discover) show the same update when the remote is
added.

## AppImage

When running from an AppImage, the update dialog offers **Download and
install**:

1. The new AppImage is downloaded from the GitHub release.
2. If `SHA256SUMS` is available, the SHA-256 is verified.
3. The current AppImage is replaced atomically; the old file is removed after
   a successful rename.
4. The user is offered a restart.

If the AppImage is not writable, the new file is saved to the Downloads
directory instead.

## Portable archive

The update dialog links to the release page.  Replace the extracted directory
with the new archive; settings live in `~/.config/namaz` and are preserved.

## Release flow

1. Update `CHANGELOG.md` for the new version.
2. Run `scripts/release.sh 1.1.0` inside the development container.
3. The script bumps `VERSION`, `Cargo.toml`, AppStream metadata, commits,
   creates `v1.1.0` and pushes.
4. The release workflow runs tests, builds Flatpak/portable/AppImage for both
   architectures, signs the repository, publishes the GitHub release, and
   deploys GitHub Pages.
