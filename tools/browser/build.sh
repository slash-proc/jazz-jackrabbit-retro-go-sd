#!/usr/bin/env bash
# Build the existing headless OpenJazz renderer for the browser.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$ROOT/tools/browser/build"
make -C "$ROOT/harness" browser
