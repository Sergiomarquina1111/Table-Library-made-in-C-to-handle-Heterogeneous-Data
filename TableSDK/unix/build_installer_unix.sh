#!/usr/bin/env bash
# Builds installer_unix (Linux/macOS) using FLTK.
# Requires: a C++ compiler, and FLTK dev files (fltk-config on PATH).
#   Debian/Ubuntu: apt install libfltk1.3-dev
#   macOS (Homebrew): brew install fltk
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v fltk-config >/dev/null 2>&1; then
    echo "ERROR: fltk-config not found on PATH. Install FLTK's dev package first." >&2
    exit 1
fi

echo "Regenerating embedded_payload.h from ../source/table.h and ../source/table.c ..."
python3 generate_embedded_payload.py

echo "Compiling installer_unix (statically linked against FLTK, so the result" \
     "is one self-contained binary - no libfltk runtime package needed on the" \
     "machine that runs it) ..."
CXX="${CXX:-c++}"
"$CXX" -O2 -std=c++17 -static-libgcc -static-libstdc++ \
    installer_unix.cpp -o installer_unix \
    $(fltk-config --cxxflags) \
    $(fltk-config --ldstaticflags --use-images)

echo "Built ./installer_unix"
