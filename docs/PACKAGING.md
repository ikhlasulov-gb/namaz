# Packaging

## Flatpak

The manifest is `packaging/io.github.ikhlasulov_gb.Namaz.yml`.  Cargo
dependencies are pinned in `packaging/cargo-sources.json`; regenerate that file
after changing `Cargo.lock`:

```bash
python3 flatpak-cargo-generator.py Cargo.lock -o packaging/cargo-sources.json
```

Build locally with `org.flatpak.Builder`:

```bash
flatpak run org.flatpak.Builder --user --install --force-clean \
  --repo=repo build packaging/io.github.ikhlasulov_gb.Namaz.yml
flatpak run io.github.ikhlasulov_gb.Namaz --version
```

The release workflow builds x86_64 and aarch64, signs the OSTree commits with
the `FLATPAK_GPG_KEY_ID` secret and publishes the merged repository to GitHub
Pages.  Users add it once:

```bash
flatpak remote-add --if-not-exists --user namaz \
  https://ikhlasulov-gb.github.io/namaz/namaz.flatpakrepo
flatpak install --user namaz io.github.ikhlasulov_gb.Namaz
```

## Portable archive

```bash
./scripts/build-portable.sh ui/build-release/namaz \
  ui/build-release/cargo/release/libnamaz_core.so
```

The output is `dist/namaz-<version>-linux-<arch>.tar.gz`.  It contains the
binary, `libnamaz_core.so`, audio, icons, desktop entry and `run.sh`.  A system
Qt 6 runtime is required.

## AppImage

```bash
./scripts/build-appimage.sh ui/build-release/namaz \
  ui/build-release/cargo/release/libnamaz_core.so
```

The script downloads `linuxdeploy` and its Qt plugin into `.cache/linuxdeploy`,
then produces `dist/Namaz-<version>-<arch>.AppImage`.  `NO_STRIP=1` is set
because the bundled strip cannot read RELR sections produced by newer
toolchains.

## System installation

Distribution packages can use the CMake install rules instead of carrying a
patch:

```bash
cmake -S ui -B ui/build-release -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build ui/build-release -j4
DESTDIR="$PWD/dist/root" cmake --install ui/build-release --prefix /usr
```

The install step provides:

| Path | Content |
| --- | --- |
| `bin/namaz` | the Qt binary |
| `<libdir>/libnamaz_core.so` | the Rust core library |
| `share/applications/io.github.ikhlasulov_gb.Namaz.desktop` | desktop entry |
| `share/metainfo/io.github.ikhlasulov_gb.Namaz.metainfo.xml` | AppStream data |
| `share/icons/hicolor/*/apps/io.github.ikhlasulov_gb.Namaz.png` | 16–512 px icons |
| `share/icons/hicolor/scalable/apps/io.github.ikhlasulov_gb.Namaz.svg` | scalable icon |
| `share/namaz/wave/*.ogg` | bundled CC0 recordings |
| `share/namaz/audio.LICENSE`, `share/namaz/cities.LICENSE` | asset licences |
| `share/licenses/namaz/LICENSE` | GPL text |

`CMAKE_INSTALL_LIBDIR` is respected and the install RPATH is derived from it,
so `lib64` distributions work unchanged.  Packagers that forbid RPATH can pass
`-DCMAKE_SKIP_INSTALL_RPATH=ON` and rely on the dynamic loader configuration.

The audio directory is found through `<prefix>/share/namaz/wave`, so an install
under `/usr` needs no `NAMAZ_WAVE_DIR`.  Use that variable only for a
non-standard layout.

## Offline and vendored builds

`cargo` honours `CARGO_NET_OFFLINE=true`; in addition the CMake build accepts
`-DNAMAZ_CARGO_OFFLINE=ON`, which passes `--offline` to cargo explicitly.  Run
`cargo vendor` (or unpack a vendor archive) and point cargo at it with a
`.cargo/config.toml` or `CARGO_HOME` before configuring:

```bash
cmake -S ui -B ui/build-offline -G Ninja -DNAMAZ_CARGO_OFFLINE=ON
```

## Versioning

`VERSION` is the single source of truth.  CMake and the release script read it.
The release workflow refuses to publish a tag whose name does not match
`VERSION` (`v1.2.3` → `1.2.3`).  `Cargo.toml` and AppStream metadata are kept
in sync by `scripts/release.sh`.

## Icons

`packaging/icons/` contains every hicolor size plus the scalable SVG and the
Windows `.ico`.  The Flatpak manifest, portable script and `install-user.sh`
install the whole set.  The desktop file uses
`io.github.ikhlasulov_gb.Namaz` as the icon name.
