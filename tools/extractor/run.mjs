#!/usr/bin/env node
import { readFileSync, writeFileSync, readdirSync } from "node:fs";
import { createHash } from "node:crypto";
import { resolve } from "node:path";

const dataDir = resolve(process.argv[2] || "/home/doug/Games/gog/jazz-jackrabbit/jazz-jackrabbit/data/noarch/game/data");
const outPath = resolve(process.argv[3] || "/tmp/Jazz-processor-output.jj");
const skipExtensions = [".EXE", ".COM", ".BAT", ".DOC", ".TXT", ".ICO", ".PIF", ".DIZ", ".CFG", ".BAK"];
const files = readdirSync(dataDir, { withFileTypes: true })
  .filter(entry => entry.isFile())
  .map(entry => entry.name)
  .filter(name => name.length < 24 && /^[\x00-\x7f]+$/.test(name)
    && !skipExtensions.some(ext => name.toUpperCase().endsWith(ext)));
const wasm = new Uint8Array(readFileSync(new URL("./build/jazz-assets.wasm", import.meta.url)));
const { instance } = await WebAssembly.instantiate(wasm, {});
const e = instance.exports;
if (e.abi_version() !== 1) throw new Error("unexpected processor ABI version");
e.input_clear();
let totalInput = 0;
for (const name of files) {
  const bytes = new Uint8Array(readFileSync(resolve(dataDir, name)));
  totalInput += bytes.length;
  const ptr = e.alloc(bytes.length);
  new Uint8Array(e.memory.buffer, ptr, bytes.length).set(bytes);
  e.input_add(ptr, bytes.length);
}
console.log(`registered ${files.length} files (${totalInput} bytes)`);
let status = e.run_begin(0);
if (status !== 0) fail(status);
const warnings = readString(e.warnings_ptr(), e.warnings_len());
if (warnings) console.warn(warnings);
try {
  while ((status = e.run_step()) === 1) {
    const i = e.stage_index() - 1;
    const name = readString(e.stage_name_ptr(i), e.stage_name_len(i));
    console.log(`${e.stage_index()}/${e.stage_count()}: ${name}`);
  }
} catch (error) {
  throw new Error(`WASM trap: ${readString(e.error_ptr(), e.error_len()) || error.message}\n${error.stack}`);
}
if (status !== 0) fail(status);
if (e.output_count() !== 1 || readString(e.output_name_ptr(0), e.output_name_len(0)) !== "jazz-pack") {
  throw new Error("processor returned an unexpected output set");
}
const output = new Uint8Array(e.memory.buffer, e.output_ptr(0), e.output_len(0));
const hash = createHash("sha256").update(output).digest("hex");
writeFileSync(outPath, output);
console.log(`wrote ${outPath}: ${output.length} bytes sha256=${hash}`);

function readString(ptr, len) {
  return new TextDecoder().decode(new Uint8Array(e.memory.buffer, ptr, len));
}
function fail(status) {
  const message = readString(e.error_ptr(), e.error_len());
  const warnings = readString(e.warnings_ptr(), e.warnings_len());
  throw new Error(`processor failed (${status}): ${message}${warnings ? `\n${warnings}` : ""}`);
}
