#!/usr/bin/env python3
"""Fail if a Keil project references a file that is not in the repository."""

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROJECTS = [
    ROOT / "MDK-ARM" / "F4project.uvprojx",
    ROOT / "bootloader" / "Bootloader.uvprojx",
]


def main() -> int:
    missing = []
    total = 0
    for proj in PROJECTS:
        tree = ET.parse(proj)
        for node in tree.iter("File"):
            path = node.findtext("FilePath")
            if not path:
                continue
            total += 1
            target = (proj.parent / path.replace("\\", "/")).resolve()
            if not target.exists():
                missing.append(f"{proj.name}: {path}")

    if missing:
        print("missing files:")
        for item in missing:
            print("  " + item)
        return 1

    print(f"ok: {total} referenced files exist")
    return 0


if __name__ == "__main__":
    sys.exit(main())
