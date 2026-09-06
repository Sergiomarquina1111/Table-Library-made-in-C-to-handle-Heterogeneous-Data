#!/usr/bin/env bash
# Builds dist/linux/libtable.a from src/table.cpp
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

CXX="${CXX:-g++}"
OUT="dist/linux"
mkdir -p "$OUT"

echo "Compiling with $CXX ..."
"$CXX" -c -O2 -std=c++17 -Wall -Iinclude src/table.cpp -o "$OUT/table.o"

echo "Archiving ..."
ar rcs "$OUT/libtable.a" "$OUT/table.o"
rm -f "$OUT/table.o"

echo "Built $OUT/libtable.a"
