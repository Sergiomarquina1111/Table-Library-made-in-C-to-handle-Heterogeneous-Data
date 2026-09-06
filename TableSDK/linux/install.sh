#!/usr/bin/env bash
# Installs table.h and libtable.a system-wide (or to a custom prefix).
# Usage: sudo ./linux/install.sh [prefix]     (default prefix: /usr/local)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${1:-/usr/local}"

LIB_SRC="$SCRIPT_DIR/dist/linux/libtable.a"
HEADER_SRC="$SCRIPT_DIR/include/table.h"

if [ ! -f "$LIB_SRC" ]; then
    echo "ERROR: $LIB_SRC not found." >&2
    echo "Run scripts/build_linux.sh first." >&2
    exit 1
fi

install -d "$PREFIX/include" "$PREFIX/lib"
install -m 644 "$HEADER_SRC" "$PREFIX/include/table.h"
install -m 644 "$LIB_SRC" "$PREFIX/lib/libtable.a"

echo "Installed:"
echo "  $PREFIX/include/table.h"
echo "  $PREFIX/lib/libtable.a"
echo ""
echo "Usage in your project:"
echo "  #include <table.h>"
echo "  g++ your_program.cpp -L$PREFIX/lib -ltable -o your_program"
