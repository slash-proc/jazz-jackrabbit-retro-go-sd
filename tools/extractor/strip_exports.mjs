// Emscripten's standalone linker exposes its libc and XMP symbols by default.
// Keep the callable surface at the GWRG ABI boundary before publishing.
import { readFileSync, writeFileSync } from "node:fs";

const path = process.argv[2];
if (!path) throw new Error("usage: strip_exports.mjs module.wasm");
const bytes = new Uint8Array(readFileSync(path));
let offset = 8;
const sections = [bytes.subarray(0, 8)];
const wanted = new Set([
  "abi_version", "alloc", "input_clear", "input_add", "run", "run_begin", "run_step",
  "stage_count", "stage_index", "stage_name_ptr", "stage_name_len", "output_count",
  "output_name_ptr", "output_name_len", "output_ptr", "output_len", "error_ptr",
  "error_len", "warnings_ptr", "warnings_len", "memory", "__data_end", "__heap_base",
]);
let found = new Set();
while (offset < bytes.length) {
  const start = offset;
  const id = bytes[offset++];
  const size = readU32(bytes, () => offset, n => { offset = n; });
  const end = offset + size;
  if (id !== 7) {
    sections.push(bytes.subarray(start, end));
    offset = end;
    continue;
  }
  let cursor = offset;
  const count = readU32(bytes, () => cursor, n => { cursor = n; });
  const kept = [];
  for (let i = 0; i < count; i++) {
    const nameSize = readU32(bytes, () => cursor, n => { cursor = n; });
    const name = new TextDecoder("utf-8", { fatal: true }).decode(bytes.subarray(cursor, cursor + nameSize));
    cursor += nameSize;
    const kind = bytes[cursor++];
    const indexStart = cursor;
    readU32(bytes, () => cursor, n => { cursor = n; });
    const entry = bytes.subarray(indexStart - nameSize - lebLen(nameSize) - 1, cursor);
    if (wanted.has(name)) {
      if (found.has(name)) throw new Error(`duplicate export ${name}`);
      found.add(name);
      kept.push(entry);
    }
  }
  if (cursor !== end) throw new Error("malformed export section");
  const payload = concat([encodeU32(kept.length), ...kept]);
  sections.push(concat([Uint8Array.of(7), encodeU32(payload.length), payload]));
  offset = end;
}
for (const name of wanted) if (name !== "__data_end" && name !== "__heap_base" && !found.has(name)) throw new Error(`missing ABI export ${name}`);
writeFileSync(path, concat(sections));
console.log(`export surface reduced to ${found.size} ABI names: ${path}`);

function readU32(b, get, set) {
  let i = get(), out = 0, shift = 0;
  for (let n = 0; n < 5; n++) {
    const x = b[i++]; if (x === undefined) throw new Error("truncated LEB128");
    out |= (x & 127) << shift; shift += 7;
    if (!(x & 128)) { set(i); return out >>> 0; }
  }
  throw new Error("invalid LEB128");
}
function lebLen(n) { return encodeU32(n).length; }
function encodeU32(n) {
  const out = [];
  do { let b=n&127; n>>>=7; if(n)b|=128; out.push(b); } while(n);
  return Uint8Array.from(out);
}
function concat(parts) {
  const size = parts.reduce((n, p) => n + p.length, 0), out = new Uint8Array(size);
  let at=0; for(const p of parts){out.set(p,at);at+=p.length;} return out;
}
