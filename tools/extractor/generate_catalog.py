#!/usr/bin/env python3
"""Generate the hash-to-DOS-name table embedded in the isolated WASM tool."""

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
catalog = json.loads((Path(__file__).parent / "gog-data-files.json").read_text())
rows = []
for item in catalog:
    digest = bytes.fromhex(item["sha1"])
    vals = ", ".join(f"0x{b:02x}" for b in digest)
    name = item["filename"]
    rows.append(f'    {{{{{vals}}}, "{name}"}}')
target = Path(__file__).parent / "catalog.inc"
target.write_text("// Generated from gog-data-files.json; do not edit.\n" +
                   "static const CatalogEntry CATALOG[] = {\n" +
                   ",\n".join(rows) + "\n};\n")
print(f"wrote {target}: {len(rows)} filename variants")
