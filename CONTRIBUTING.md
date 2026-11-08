# Contributing

Thank you for helping improve Namaz.

> AI agents: read [`AGENTS.md`](AGENTS.md) first.  It documents the local
> signing key, release flow and other maintainer-only details.

## Before you start

- Use a recent Rust toolchain (1.92+) and Qt 6.5+.
- Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the project layout.
- For user-visible changes, update `CHANGELOG.md` and, if needed, the README
  screenshots or documentation.

## Development checks

```bash
cargo fmt --all -- --check
cargo clippy --workspace --lib --bins -- -D warnings
cargo test -p prayer-core -p namaz-core
./scripts/ui-check.sh
./scripts/diff-itl.sh 1500
```

`ui-check.sh` and `diff-itl.sh` use the `salaat-dev` Toolbox container when it
exists and fall back to the current environment otherwise.  Set
`NAMAZ_CONTAINER` to use a different container.

## Pull requests

- Keep changes focused and explain the user-visible effect.
- Add or update tests for behaviour changes.
- Do not commit build output (`target/`, `ui/build/`, `dist/`,
  `.flatpak-builder/`, `build-appimage/`).
- Do not add proprietary Salaat Time assets.  The public build must remain
  reproducible from free resources.
- Flatpak changes must build offline with `packaging/cargo-sources.json`.
  Regenerate it with `flatpak-cargo-generator.py Cargo.lock` after changing
  `Cargo.lock`.

## Translations

The Russian translation lives in `ui/translations/namaz_ru.ts`.
After adding or changing `tr()` strings:

```bash
cmake --build ui/build --target update_translations
```

Then translate the new unfinished entries and rebuild.

## Reporting bugs

Include the version (`namaz --version`), distribution, desktop (KDE/GNOME),
display server (Wayland/X11) and whether you use Flatpak, AppImage or a
source build.  A minimal config and steps to reproduce help a lot.
