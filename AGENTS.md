# Namaz — guide for AI agents

This file is for a future AI developer or coding agent working on Namaz.  It
documents the local-only pieces (secrets, signing key, build environment) that
must never be committed, plus the project invariants and release flow.

> This file is public.  Never put the private key, a GitHub token, a
> passphrase or any other secret into it.

## Current local paths (maintainer machine)

| What | Path |
| --- | --- |
| Project | `/var/home/ikhlasulov/Data/Моя Информация/Программирование/namaz` |
| Portable builds | `/var/home/ikhlasulov/.apps/namaz/` |
| Private legacy archive | `/var/home/ikhlasulov/Buffer/Fixing/salaat-time-private/` |
| Flatpak signing key | `~/.config/namaz/flatpak-signing/` |
| Secure key backup | `/var/home/ikhlasulov/Data/Моя Информация/Программирование/namaz-secrets/` |
| Local access notes | `~/.config/namaz/ACCESS.md` |
| Dev container | `salaat-dev` (podman/toolbox), user id 1002 |

The project path contains spaces and Cyrillic characters.  The Qt build was
made spaces-safe in `ui/CMakeLists.txt` by generating `namaz_ru.qm` with a
plain `lrelease` command instead of the deferred `qt_add_translations()`
implementation.  Do not reintroduce `qt_add_translations()` without testing a
build directory whose path contains spaces.

## GitHub access

- Repository: `https://github.com/ikhlasulov-gb/namaz`
- Remote: `origin` → the same URL (HTTPS)
- CLI: `~/.local/bin/gh`
- Account: `ikhlasulov-gb`
- Token storage: system keyring.  It survives reboot and was verified from a
  clean environment (`env -i ... gh auth status`).
- Git credential helper:
  `!~/.local/bin/gh auth git-credential`
- Required scopes: `repo`, `workflow` (plus `gist`, `read:org`).

Check access with:

```bash
~/.local/bin/gh auth status
cd "/var/home/ikhlasulov/Data/Моя Информация/Программирование/namaz"
podman exec -w "$PWD" -u 1002 salaat-dev bash -lc 'git ls-remote --heads origin main'
```

Re-authenticate only if the keyring entry is gone:

```bash
~/.local/bin/gh auth login --hostname github.com --git-protocol https --web
~/.local/bin/gh auth refresh -h github.com -s workflow
```

Never print the token, write it into the repository, or add it to a workflow
file.  GitHub Actions receives it automatically as `GITHUB_TOKEN`.

## Flatpak signing key

The signed Flatpak repository is what lets users update with
`flatpak update`.  Losing the private key means existing installations can no
longer trust updates from the same remote without re-adding it.

- Key ID: `7D589F7149CE1619`
- Fingerprint: `775E58E7D9BC2567B2C7FF697D589F7149CE1619`
- UID: `Namaz Flatpak Signing <namaz@ikhlasulov-gb.github.io>`
- Private key: `~/.config/namaz/flatpak-signing/backup/private-key.asc`
- Public key: `~/.config/namaz/flatpak-signing/backup/public-key.asc`
- GPG home: `~/.config/namaz/flatpak-signing/gnupg`
- Secure backup directory (mode 700):
  `/var/home/ikhlasulov/Data/Моя Информация/Программирование/namaz-secrets/`
  - `private-key.asc`
  - `public-key.asc`
  - `revocation-certificate.rev`
  - `flatpak-signing-backup-2026-11-08.tar.gz`
  - `SHA256SUMS`
  - `README.txt`

The public key is embedded in
`https://ikhlasulov-gb.github.io/namaz/namaz.flatpakrepo`.  The private key is
**not** in the repository and must never be added to it.

### GitHub Actions secrets

Already configured on the repository:

- `FLATPAK_GPG_PRIVATE_KEY` — contents of `private-key.asc`
- `FLATPAK_GPG_KEY_ID` — `7D589F7149CE1619`

Set them again only on a new repository:

```bash
~/.local/bin/gh secret set FLATPAK_GPG_PRIVATE_KEY \
  < ~/.config/namaz/flatpak-signing/backup/private-key.asc
~/.local/bin/gh secret set FLATPAK_GPG_KEY_ID --body 7D589F7149CE1619
```

### Restore the key on a new machine

```bash
export GNUPGHOME="$HOME/.config/namaz/flatpak-signing/gnupg"
mkdir -p "$GNUPGHOME"
chmod 700 "$GNUPGHOME"
gpg --batch --import /path/to/namaz-secrets/private-key.asc
gpg --list-secret-keys --keyid-format LONG
```

Verify the fingerprint before using it.  The backup archive was test-imported
into a temporary GNUPGHOME and reported the expected fingerprint
`775E58E7D9BC2567B2C7FF697D589F7149CE1619`.

### Manual signing (recovery / local testing)

The release workflow does this automatically.  For a local repository:

```bash
export GNUPGHOME="$HOME/.config/namaz/flatpak-signing/gnupg"
KEYID=7D589F7149CE1619
REPO=/path/to/flatpak-repo

flatpak build-sign --arch=x86_64 --gpg-sign="$KEYID" "$REPO" \
  io.github.ikhlasulov_gb.Namaz master
flatpak build-sign --arch=x86_64 --runtime --gpg-sign="$KEYID" "$REPO" \
  io.github.ikhlasulov_gb.Namaz.Debug master
# Repeat with --arch=aarch64 when that architecture is present.

flatpak build-update-repo --gpg-sign="$KEYID" \
  --generate-static-deltas --prune "$REPO"
```

If the key is compromised, revoke it with
`revocation-certificate.rev`, generate a new key, update the GitHub secrets,
and tell users to re-add the remote.  Never silently replace the key.

## Build and test

Run in the `salaat-dev` container:

```bash
NEW="/var/home/ikhlasulov/Data/Моя Информация/Программирование/namaz"
podman exec -w "$NEW" -u 1002 salaat-dev bash -lc '
  cargo fmt --all -- --check &&
  cargo clippy --workspace --lib --bins -- -D warnings &&
  cargo test -p prayer-core -p namaz-core &&
  cmake -S ui -B ui/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -G Ninja &&
  cmake --build ui/build -j4 &&
  QT_QPA_PLATFORM=offscreen ctest --test-dir ui/build --output-on-failure
'
./scripts/ui-check.sh
./scripts/diff-itl.sh 1500
```

The helper scripts use `salaat-dev` when it exists and otherwise run directly.
Set `NAMAZ_CONTAINER` to use a different container.

Build artifacts are ignored: `target/`, `ui/build/`, `ui/build-release/`,
`dist/`, `.flatpak-builder/`, `build-appimage/`, `.cache/`.  Do not commit
them.

### Flatpak

```bash
cd "/var/home/ikhlasulov/Data/Моя Информация/Программирование/namaz"
flatpak run --share=network org.flatpak.Builder \
  --state-dir="$HOME/namaz-flatpak-state" --disable-rofiles-fuse \
  --user --install --force-clean --repo="$HOME/namaz-flatpak-repo" \
  "$HOME/namaz-flatpak-build" packaging/io.github.ikhlasulov_gb.Namaz.yml
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
   podman exec -w "/var/home/ikhlasulov/Data/Моя Информация/Программирование/namaz" \
     -u 1002 salaat-dev bash -lc './scripts/release.sh X.Y.Z'
   ```

3. `scripts/release.sh` updates `VERSION`, `Cargo.toml`, `Cargo.lock` and the
   AppStream metadata, commits, creates `vX.Y.Z` and pushes the branch and tag.
4. `.github/workflows/release.yml` builds Flatpak (x86_64 + aarch64), portable
   archives, AppImages, signs the OSTree repo, publishes the GitHub release
   and deploys GitHub Pages.
5. Verify:

   ```bash
   ~/.local/bin/gh release view vX.Y.Z -R ikhlasulov-gb/namaz
   curl -sI https://ikhlasulov-gb.github.io/namaz/namaz.flatpakrepo
   flatpak remote-ls --user namaz
   flatpak install --user -y --reinstall namaz io.github.ikhlasulov_gb.Namaz
   flatpak run io.github.ikhlasulov_gb.Namaz --version
   ```

Do not move a published tag.  The early `v1.0.0` tag was moved while the
project was still being set up; future releases must be immutable.  If a
release is broken, publish the next patch version.

## Project invariants

- Application ID: `io.github.ikhlasulov_gb.Namaz`
- Binary: `namaz`
- Desktop file / icon name: `io.github.ikhlasulov_gb.Namaz`
- D-Bus service: `io.github.ikhlasulov_gb.Namaz`, object `/Namaz`
- Config: `$XDG_CONFIG_HOME/namaz/config.toml`
- Audio env: `NAMAZ_WAVE_DIR`
- Version: `VERSION` is the single source of truth; tag is `vX.Y.Z`
- Licence: GPL-3.0-or-later
- Free assets only:
  - audio: CC0 recordings by Ikhlasulov (`assets/audio/`)
  - cities: GeoNames `cities5000` (`assets/cities.json`, CC BY 4.0)
  - no Salaat Time executables, DLLs, WAVs, screenshots or city XML

## Useful URLs

- Repository: https://github.com/ikhlasulov-gb/namaz
- Pages: https://ikhlasulov-gb.github.io/namaz/
- Flatpak remote: https://ikhlasulov-gb.github.io/namaz/namaz.flatpakrepo
- Releases: https://github.com/ikhlasulov-gb/namaz/releases
