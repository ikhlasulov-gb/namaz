#!/usr/bin/env bash
# Prepare and push a release.  Run from a clean git checkout after adding the
# new version's section to CHANGELOG.md.
#
# Usage: scripts/release.sh 1.1.0
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-}"
if [[ ! "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "usage: scripts/release.sh MAJOR.MINOR.PATCH" >&2
    exit 2
fi

cd "$ROOT"
git rev-parse --is-inside-work-tree >/dev/null
if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "working tree is not clean; commit or stash your changes first" >&2
    exit 1
fi
if ! grep -q "^## \[$VERSION\]" CHANGELOG.md; then
    echo "CHANGELOG.md has no '## [$VERSION]' section" >&2
    echo "add release notes first, then run this script again" >&2
    exit 1
fi

printf '%s\n' "$VERSION" > VERSION

python3 - "$VERSION" <<'PY'
from pathlib import Path
import sys
version = sys.argv[1]
path = Path("Cargo.toml")
text = path.read_text()
marker = "[workspace.package]"
head, sep, tail = text.partition(marker)
if not sep:
    raise SystemExit("Cargo.toml has no [workspace.package]")
lines = tail.splitlines()
for index, line in enumerate(lines):
    if line.startswith("version = "):
        lines[index] = f'version = "{version}"'
        break
else:
    raise SystemExit("Cargo.toml has no workspace version")
path.write_text(head + sep + "\n".join(lines) + "\n")
PY

cargo check --workspace --quiet

python3 - "$VERSION" <<'PY'
from datetime import date
from pathlib import Path
import sys
version = sys.argv[1]
path = Path("packaging/io.github.ikhlasulov_gb.Namaz.metainfo.xml")
text = path.read_text()
if f'version="{version}"' in text:
    raise SystemExit(0)
entry = (
    f'    <release version="{version}" date="{date.today().isoformat()}">\n'
    "      <description>\n"
    "        <p>See CHANGELOG.md for the full list of changes.</p>\n"
    "      </description>\n"
    "    </release>\n"
)
if "  <releases>\n" not in text:
    raise SystemExit("metainfo has no <releases> block")
path.write_text(text.replace("  <releases>\n", "  <releases>\n" + entry, 1))
PY

git add VERSION Cargo.toml Cargo.lock CHANGELOG.md \
    packaging/io.github.ikhlasulov_gb.Namaz.metainfo.xml
git commit -m "chore(release): v$VERSION"
git tag -a "v$VERSION" -m "Namaz $VERSION"

git push origin HEAD
git push origin "v$VERSION"

echo "pushed v$VERSION; the release workflow will build and publish it"
