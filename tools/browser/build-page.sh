#!/usr/bin/env bash
# Assemble the static GWRG WASM converter page. The firmware build supplies Jazz.bin.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SITE="${1:-$ROOT/tools/browser/site}"
BIN="${2:-$ROOT/Jazz.bin}"
WASM="${3:-$ROOT/tools/extractor/build/jazz-assets.wasm}"

if [[ ! -f "$WASM" ]]; then
  echo "GWRG processor WASM is missing; run tools/extractor/build.sh first" >&2
  exit 1
fi
if [[ ! -f "$BIN" ]]; then
  echo "homebrew binary is missing: $BIN" >&2
  exit 1
fi

rm -rf "$SITE"
mkdir -p "$SITE"
cp "$ROOT/tools/browser/"{index.html,app.js,worker.js,pack.mjs,style.css} "$SITE/"
cp "$WASM" "$SITE/jazz-assets.wasm"
cp "$BIN" "$SITE/Jazz.bin"
touch "$SITE/.nojekyll"
echo "Pages site assembled at $SITE"
