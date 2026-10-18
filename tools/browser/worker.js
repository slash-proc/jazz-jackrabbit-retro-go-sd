import { makeCardZip } from "./pack.mjs";

let cancelled = false;
let currentExports;
const wasmUrl = new URL("./jazz-assets.wasm", import.meta.url);

self.onmessage = async ({ data }) => {
  if (data.type === "cancel") { cancelled = true; return; }
  if (data.type !== "convert") return;
  cancelled = false;
  try {
    const files = data.files.filter(file => isImmediateChild(file) && allowed(file.name));
    postMessage({ type: "progress", message: `Reading ${files.length} game files locally…` });
    const wasmResponse = await fetch(wasmUrl);
    if (!wasmResponse.ok) throw new Error("Could not load the Jazz converter module from this page.");
    const wasm = new Uint8Array(await wasmResponse.arrayBuffer());
    const { instance } = await WebAssembly.instantiate(wasm, {});
    const e = instance.exports;
    currentExports = e;
    if (e.abi_version() !== 1) throw new Error("Unsupported converter ABI.");
    e.input_clear();
    for (const file of files) {
      if (cancelled) return postMessage({ type: "cancelled" });
      const bytes = new Uint8Array(await file.arrayBuffer());
      const ptr = e.alloc(bytes.length);
      if (!ptr && bytes.length) throw new Error(`Not enough memory to read ${file.name}.`);
      new Uint8Array(e.memory.buffer, ptr, bytes.length).set(bytes);
      e.input_add(ptr, bytes.length);
    }
    let result = e.run_begin(0);
    if (result !== 0) throw new Error(readString(e.error_ptr(), e.error_len()));
    const count = e.stage_count();
    while ((result = e.run_step()) === 1) {
      if (cancelled) return postMessage({ type: "cancelled" });
      const index = e.stage_index() - 1;
      postMessage({ type: "progress", message: `${index + 1} of ${count}: ${readString(e.stage_name_ptr(index), e.stage_name_len(index))}` });
      // Yield a task so the page can repaint and process a Cancel click.
      await new Promise(resolve => setTimeout(resolve, 0));
    }
    if (result !== 0) {
      const error = readString(e.error_ptr(), e.error_len());
      const warnings = readString(e.warnings_ptr(), e.warnings_len());
      throw new Error(`${error}${warnings ? `\n${warnings}` : ""}`);
    }
    if (cancelled) return postMessage({ type: "cancelled" });
    if (e.output_count() !== 1 || readString(e.output_name_ptr(0), e.output_name_len(0)) !== "jazz-pack") {
      throw new Error("The converter returned an unexpected output.");
    }
    const output = new Uint8Array(e.memory.buffer, e.output_ptr(0), e.output_len(0)).slice();
    const binaryResponse = await fetch(new URL("./Jazz.bin", import.meta.url));
    if (!binaryResponse.ok) throw new Error("Could not load Jazz.bin from this page.");
    const homebrew = await binaryResponse.arrayBuffer();
    const zip = makeCardZip(homebrew, output);
    const warnings = readString(e.warnings_ptr(), e.warnings_len());
    postMessage({ type: "done", zip: zip.buffer, packSize: output.length, fileCount: files.length, warnings }, [zip.buffer]);
  } catch (error) {
    if (cancelled) postMessage({ type: "cancelled" });
    else postMessage({ type: "error", message: error?.message || String(error) });
  }
};

function readString(ptr, len) {
  return new TextDecoder().decode(new Uint8Array(instanceMemory(), ptr, len));
}
function instanceMemory() { return currentExports.memory.buffer; }

function isImmediateChild(file) {
  const path = file.webkitRelativePath || file.name;
  return path.split("/").length <= 2;
}
function allowed(name) {
  const upper = name.toUpperCase();
  if (upper.length >= 24 || /[^\x00-\x7f]/.test(upper)) return false;
  return ![".EXE", ".COM", ".BAT", ".DOC", ".TXT", ".ICO", ".PIF", ".DIZ", ".CFG", ".BAK"].some(ext => upper.endsWith(ext));
}
