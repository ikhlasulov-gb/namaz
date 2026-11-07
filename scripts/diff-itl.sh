#!/usr/bin/env bash
# Differential test: Rust prayer engine vs the bundled ITL C sources.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CASES="${1:-1500}"
CONTAINER="${NAMAZ_CONTAINER:-salaat-dev}"

run_checks() {
    gcc -O2 -I "$ROOT/third_party/itl" "$ROOT/scripts/itl_ref.c" \
        "$ROOT/third_party/itl/prayer.c" "$ROOT/third_party/itl/astro.c" \
        -lm -o /tmp/itl_ref
    cargo build -p prayer-core --bin namaz-cli >/dev/null
    python3 "$ROOT/scripts/check_itl.py" /tmp/itl_ref \
        "$ROOT/target/debug/namaz-cli" "$CASES"
}

if command -v podman >/dev/null 2>&1 \
    && podman container exists "$CONTAINER" >/dev/null 2>&1; then
    podman exec -u "$(id -u)" -w "$ROOT" "$CONTAINER" bash -lc \
        "gcc -O2 -I '$ROOT/third_party/itl' '$ROOT/scripts/itl_ref.c' \
            '$ROOT/third_party/itl/prayer.c' '$ROOT/third_party/itl/astro.c' \
            -lm -o /tmp/itl_ref && \
         cargo build -p prayer-core --bin namaz-cli >/dev/null && \
         python3 '$ROOT/scripts/check_itl.py' /tmp/itl_ref \
            '$ROOT/target/debug/namaz-cli' '$CASES'"
else
    cd "$ROOT"
    run_checks
fi
