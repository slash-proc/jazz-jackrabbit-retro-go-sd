#!/usr/bin/env python3
"""Record the Python converter's byte-exact oracle for a local game install."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from hash_game_data import catalogue


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path, help="DOS game data directory")
    parser.add_argument("output", type=Path, help="Jazz.jj produced by tools/jj_pack.py")
    parser.add_argument("--out", type=Path, default=Path("reference.json"))
    args = parser.parse_args()

    if not args.output.is_file():
        raise SystemExit(f"converter output not found: {args.output}")
    inputs = catalogue(args.folder)
    data = args.output.read_bytes()
    reference = {
        "inputs": [
            {"name": f["filename"], "sha1": f["sha1"], "bytes": f["bytes"]}
            for f in inputs
        ],
        "flags": 0,
        "outputs": [{
            "name": "Jazz.jj",
            "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        }],
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(reference, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.out}: {len(inputs)} inputs, {len(data)} output bytes")


if __name__ == "__main__":
    main()
