const REQUIRED = ["PANEL.000", "FONTMN1.0FN", "MENU.000", "SOUNDS.000"];
const SKIP_EXT = [".EXE", ".COM", ".BAT", ".DOC", ".TXT", ".ICO", ".PIF", ".DIZ", ".CFG", ".BAK"];
const MAGIC = new TextEncoder().encode("GWJJ01\0\0");
const ENTRY_NAME_LEN = 24;

export async function collectGameFiles(fileList) {
  const files = new Map();
  let ignored = 0;
  for (const file of fileList) {
    const path = file.webkitRelativePath || file.name;
    // The DOS packer reads the selected directory's immediate files.
    if (path.split("/").length > 2) { ignored++; continue; }
    const name = file.name.toUpperCase();
    if (SKIP_EXT.some(ext => name.endsWith(ext)) || name.length >= ENTRY_NAME_LEN) {
      ignored++;
      continue;
    }
    if (!/^[\x00-\x7f]+$/.test(name)) throw new Error(`Unsupported non-ASCII filename: ${file.name}`);
    if (files.has(name)) throw new Error(`Two files have the same DOS filename after case conversion: ${name}`);
    files.set(name, new Uint8Array(await file.arrayBuffer()));
  }
  const missing = REQUIRED.filter(name => !files.has(name));
  if (missing.length) throw new Error(`Missing required Jazz files: ${missing.join(", ")}. Choose the folder containing the DOS game data.`);
  return { files, ignored };
}

export function makePack(files) {
  const entries = [...files.entries()].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0);
  const headerSize = 16 + entries.length * (ENTRY_NAME_LEN + 8);
  let offset = align4(headerSize);
  const packedEntries = entries.map(([name, bytes]) => {
    const entry = { name, bytes, offset };
    offset = align4(offset + bytes.byteLength);
    return entry;
  });
  if (offset > 0xffffffff) throw new Error("The game pack exceeds the supported 4 GiB size.");
  const out = new Uint8Array(offset);
  out.set(MAGIC, 0);
  const view = new DataView(out.buffer);
  view.setUint32(8, entries.length, true);
  view.setUint32(12, offset, true);
  packedEntries.forEach(({ name, bytes, offset: pos }, i) => {
    const nameBytes = new TextEncoder().encode(name);
    out.set(nameBytes, 16 + i * (ENTRY_NAME_LEN + 8));
    view.setUint32(16 + i * (ENTRY_NAME_LEN + 8) + ENTRY_NAME_LEN, pos, true);
    view.setUint32(16 + i * (ENTRY_NAME_LEN + 8) + ENTRY_NAME_LEN + 4, bytes.byteLength, true);
    out.set(bytes, pos);
  });
  return out;
}

export function makeCardZip(homebrew, gamePack) {
  const files = [
    ["homebrews/Jazz.bin", new Uint8Array(homebrew)],
    ["homebrews/Jazz.jj", gamePack],
  ];
  const encoder = new TextEncoder();
  const prepared = files.map(([name, data]) => ({
    nameBytes: encoder.encode(name), data, crc: crc32(data),
  }));
  const centralSize = prepared.reduce((n, file) => n + 46 + file.nameBytes.length, 0);
  const localSize = prepared.reduce((n, file) => n + 30 + file.nameBytes.length + file.data.length, 0);
  const result = new Uint8Array(localSize + centralSize + 22);
  let localOffset = 0;
  for (const file of prepared) {
    const { nameBytes, data, crc } = file;
    const lv = new DataView(result.buffer, localOffset, 30 + nameBytes.length);
    lv.setUint32(0, 0x04034b50, true);
    lv.setUint16(4, 20, true);
    lv.setUint16(6, 0x0800, true); // UTF-8 names
    lv.setUint16(8, 0, true); // stored without compression
    lv.setUint16(12, 0x21, true); // 1980-01-01
    lv.setUint32(14, crc, true);
    lv.setUint32(18, data.length, true);
    lv.setUint32(22, data.length, true);
    lv.setUint16(26, nameBytes.length, true);
    result.set(nameBytes, localOffset + 30);
    result.set(data, localOffset + 30 + nameBytes.length);
    file.localOffset = localOffset;
    localOffset += 30 + nameBytes.length + data.length;
  }

  const centralOffset = localOffset;
  for (const { nameBytes, data, crc, localOffset: start } of prepared) {
    const cv = new DataView(result.buffer, localOffset, 46 + nameBytes.length);
    cv.setUint32(0, 0x02014b50, true);
    cv.setUint16(4, 20, true);
    cv.setUint16(6, 20, true);
    cv.setUint16(8, 0x0800, true);
    cv.setUint16(10, 0, true);
    cv.setUint16(14, 0x21, true); // 1980-01-01
    cv.setUint32(16, crc, true);
    cv.setUint32(20, data.length, true);
    cv.setUint32(24, data.length, true);
    cv.setUint16(28, nameBytes.length, true);
    cv.setUint32(42, start, true);
    result.set(nameBytes, localOffset + 46);
    localOffset += 46 + nameBytes.length;
  }
  const ev = new DataView(result.buffer, localOffset, 22);
  ev.setUint32(0, 0x06054b50, true);
  ev.setUint16(8, files.length, true);
  ev.setUint16(10, files.length, true);
  ev.setUint32(12, centralSize, true);
  ev.setUint32(16, centralOffset, true);
  return result;
}

function align4(value) { return (value + 3) & ~3; }

function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}
