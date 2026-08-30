#!/usr/bin/env python3
"""Generate the deterministic Sailfish launcher icon set."""

import argparse
import shutil
import sys
import tempfile
from pathlib import Path

try:
    from PIL import Image
except ImportError as error:
    raise SystemExit("Pillow is required to generate QQ Music icons") from error


MASTER_SIZE = 1024
MASTER_NAME = "harbour-qqmusic-master.png"
ICON_SIZES = (86, 108, 128, 172)
ICON_NAME = "harbour-qqmusic.png"


def load_master(root):
    path = root / MASTER_NAME
    with Image.open(path) as source:
        if (source.format != "PNG" or source.mode != "RGBA"
                or source.size != (MASTER_SIZE, MASTER_SIZE)):
            raise ValueError(f"invalid RGBA master: {path}")
        image = source.copy()
    if not image.getchannel("A").getbbox():
        raise ValueError(f"empty master alpha channel: {path}")
    return image


def icon_path(root, size):
    return root / "generated" / f"{size}x{size}" / ICON_NAME


def generate(root):
    master = load_master(root)
    for size in ICON_SIZES:
        output = icon_path(root, size)
        output.parent.mkdir(parents=True, exist_ok=True)
        resized = master.resize((size, size), Image.Resampling.LANCZOS)
        resized.save(output, format="PNG", optimize=False, compress_level=9)


def validate(root):
    load_master(root)
    expected = {icon_path(root, size) for size in ICON_SIZES}
    actual = set((root / "generated").rglob("*.png"))
    if actual != expected:
        missing = sorted(str(path) for path in expected - actual)
        extra = sorted(str(path) for path in actual - expected)
        raise ValueError(f"unexpected icon set; missing={missing}, extra={extra}")
    if list(root.rglob("*.svg")):
        raise ValueError("SVG launcher icons are not allowed")
    if any("listen1" in path.name.lower() for path in root.rglob("*.*")):
        raise ValueError("legacy Listen1 icon detected")

    for size in ICON_SIZES:
        path = icon_path(root, size)
        with Image.open(path) as source:
            source_mode = source.mode
            image = source.convert("RGBA")
        if source_mode != "RGBA" or image.size != (size, size):
            raise ValueError(f"invalid RGBA dimensions: {path}")
        if not image.getchannel("A").getbbox():
            raise ValueError(f"empty alpha channel: {path}")
        colors = image.getcolors(maxcolors=size * size)
        if colors is not None and len(colors) < 64:
            raise ValueError(f"icon color detail is unexpectedly low: {path}")


def check(checked_root):
    validate(checked_root)
    with tempfile.TemporaryDirectory(prefix="qqmusic-icons-") as directory:
        generated_root = Path(directory)
        shutil.copyfile(checked_root / MASTER_NAME,
                        generated_root / MASTER_NAME)
        generate(generated_root)
        validate(generated_root)
        for size in ICON_SIZES:
            expected = icon_path(checked_root, size).read_bytes()
            actual = icon_path(generated_root, size).read_bytes()
            if actual != expected:
                raise ValueError(f"icon is not reproducible at {size}x{size}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true",
                        help="regenerate in private storage and byte-compare")
    parser.add_argument("--output", type=Path,
                        help="override the icons directory")
    arguments = parser.parse_args()
    root = arguments.output or Path(__file__).resolve().parent
    try:
        if arguments.check:
            check(root)
        else:
            generate(root)
            validate(root)
    except (OSError, ValueError) as error:
        print(f"generate_icons.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
