#!/usr/bin/env python3
"""Write the hash catalogue for one Jazz Jackrabbit DATA directory.

The catalogue contains metadata only; no game files are copied. It mirrors the
browser packer's top-level-file, uppercase-name and ignored-extension rules.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

SKIP_EXT = (".EXE", ".COM", ".BAT", ".DOC", ".TXT", ".ICO", ".PIF", ".DIZ", ".CFG", ".BAK")
REQUIRED = ("PANEL.000", "FONTMN1.0FN", "MENU.000", "SOUNDS.000")


def catalogue(folder: Path) -> list[dict[str, object]]:
    files: list[dict[str, object]] = []
    for path in sorted(folder.iterdir(), key=lambda p: p.name.upper()):
        if not path.is_file():
            continue
        name = path.name.upper()
        if name.endswith(SKIP_EXT) or len(name) >= 24:
            continue
        data = path.read_bytes()
        files.append({
            "id": name.lower().replace(".", "-"),
            "filename": name,
            "bytes": len(data),
            "sha1": hashlib.sha1(data).hexdigest(),
        })
    return files


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path, help="DOS game data directory")
    parser.add_argument("--out", type=Path, required=True, help="catalogue JSON path")
    args = parser.parse_args()
    if not args.folder.is_dir():
        raise SystemExit(f"not a directory: {args.folder}")

    files = catalogue(args.folder)
    names = {str(entry["filename"]) for entry in files}
    missing = sorted(set(REQUIRED) - names)
    if missing:
        raise SystemExit("missing packer-required game files: " + ", ".join(missing))

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(files, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.out}: {len(files)} files, {sum(int(x['bytes']) for x in files)} bytes")


if __name__ == "__main__":
    main()
