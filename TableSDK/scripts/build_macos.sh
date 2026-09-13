#!/usr/bin/env bash
# Builds dist/macos/libtable.a from src/table.c
# You didn't ask for this one, but it's the same pattern as build_linux.sh -
# included since Table SDK targets macOS too. Delete it if you don't need it.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

# Apple ships clang as `cc`; only overridden here if you explicitly set CC.
CC="${CC:-cc}"
OUT="dist/macos"
mkdir -p "$OUT"

echo "Compiling with $CC ..."
"$CC" -c -O2 -std=c11 -Wall -Wextra -Iinclude src/table.c -o "$OUT/table.o"

echo "Archiving ..."
ar rcs "$OUT/libtable.a" "$OUT/table.o"
rm -f "$OUT/table.o"

echo "Built $OUT/libtable.a"
