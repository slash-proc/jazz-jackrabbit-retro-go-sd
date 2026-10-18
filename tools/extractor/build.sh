#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE="${EMSDK_IMAGE:-emscripten/emsdk:3.1.64}"
OUT="$ROOT/tools/extractor/build/jazz-assets.wasm"

mkdir -p "$(dirname "$OUT")"
# Make flag changes take effect too; Make does not track compiler arguments.
rm -rf "$ROOT/harness/build/wasm-processor"
if ! docker run --rm --user "$(id -u):$(id -g)" \
  -v "$ROOT:/work" -w /work "$IMAGE" make -C harness processor-raw \
  >"$ROOT/tools/extractor/build/build.log" 2>&1; then
  tail -40 "$ROOT/tools/extractor/build/build.log" >&2
  exit 1
fi
# Docker's user-namespace mapping can leave this file owned by `nobody` even
# though the mounted directory belongs to the caller. Re-copy it so the Node
# export filter can update it without modifying the shared-directory owner.
RAW="$OUT.raw"
mv "$OUT" "$RAW"
cp "$RAW" "$OUT"
rm "$RAW"
node "$ROOT/tools/extractor/strip_exports.mjs" "$OUT"
docker run --rm --user "$(id -u):$(id -g)" \
  -v "$ROOT:/work" -w /work "$IMAGE" \
  /emsdk/upstream/bin/wasm-opt -O2 --enable-exception-handling --enable-multivalue \
    --enable-reference-types --translate-to-exnref --remove-unused-module-elements \
    tools/extractor/build/jazz-assets.wasm -o tools/extractor/build/jazz-assets.opt.wasm
mv "$ROOT/tools/extractor/build/jazz-assets.opt.wasm" "$OUT"
node "$ROOT/tools/extractor/verify.mjs" "$OUT"
