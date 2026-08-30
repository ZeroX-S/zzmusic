#!/usr/bin/env python3
"""Validate QQ Music version/build identity manifests."""

import argparse
import json
import re
import sys
from pathlib import Path


BASE_KEYS = {
    "schema",
    "name",
    "version",
    "release_prefix",
    "architecture",
    "source_date_epoch",
}
IDENTITY_KEYS = {
    "schema",
    "name",
    "version",
    "release",
    "architecture",
    "source_date_epoch",
    "run_id",
    "source_commit",
    "source_manifest_sha256",
}
SAFE_COMPONENT = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
RUN_ID = re.compile(r"^[0-9]{8}T[0-9]{6}Z$")
HEX_40 = re.compile(r"^[0-9a-f]{40}$")
HEX_64 = re.compile(r"^[0-9a-f]{64}$")


def fail(message):
    raise SystemExit(f"QQ Music version manifest error: {message}")


def load_json(path, expected_keys):
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        fail(f"cannot read {path}: {error}")
    if not isinstance(data, dict) or set(data) != expected_keys:
        fail(f"unexpected keys in {path}")
    return data


def validate_base(path):
    data = load_json(path, BASE_KEYS)
    if data["schema"] != 1:
        fail("unsupported version schema")
    if data["name"] != "zzmusic":
        fail("unexpected package name")
    for key in ("name", "version", "release_prefix", "architecture"):
        if not isinstance(data[key], str) or not SAFE_COMPONENT.fullmatch(data[key]):
            fail(f"unsafe {key}")
    if data["architecture"] != "aarch64":
        fail("unsupported architecture")
    epoch = data["source_date_epoch"]
    if not isinstance(epoch, int) or epoch < 1_700_000_000 or epoch > 4_000_000_000:
        fail("invalid source_date_epoch")
    return data


def expected_release(base, run_id, source_manifest_sha256):
    return ".".join(
        (base["release_prefix"], run_id.lower(), source_manifest_sha256[:12])
    )


def validate_identity(version_path, identity_path):
    base = validate_base(version_path)
    data = load_json(identity_path, IDENTITY_KEYS)
    if data["schema"] != 1:
        fail("unsupported identity schema")
    if not RUN_ID.fullmatch(str(data["run_id"])):
        fail("invalid run_id")
    if not HEX_40.fullmatch(str(data["source_commit"])):
        fail("invalid source_commit")
    if not HEX_64.fullmatch(str(data["source_manifest_sha256"])):
        fail("invalid source_manifest_sha256")
    for key in ("name", "version", "release", "architecture"):
        if not isinstance(data[key], str) or not SAFE_COMPONENT.fullmatch(data[key]):
            fail(f"unsafe identity {key}")
    for key in ("name", "version", "architecture", "source_date_epoch"):
        if data[key] != base[key]:
            fail(f"identity {key} differs from version manifest")
    expected = expected_release(base, data["run_id"], data["source_manifest_sha256"])
    if data["release"] != expected:
        fail("identity release does not match run/source manifest")
    return data


def emit_shell(data):
    mapping = {
        "QQMUSIC_NAME": data["name"],
        "QQMUSIC_VERSION": data["version"],
        "QQMUSIC_ARCH": data["architecture"],
        "SOURCE_DATE_EPOCH": str(data["source_date_epoch"]),
    }
    if "release_prefix" in data:
        mapping["QQMUSIC_RELEASE_PREFIX"] = data["release_prefix"]
    if "release" in data:
        mapping.update(
            {
                "QQMUSIC_RELEASE": data["release"],
                "QQMUSIC_RUN_ID": data["run_id"],
                "QQMUSIC_SOURCE_COMMIT": data["source_commit"],
                "QQMUSIC_SOURCE_MANIFEST_SHA256": data[
                    "source_manifest_sha256"
                ],
            }
        )
    for key, value in mapping.items():
        if not SAFE_COMPONENT.fullmatch(value):
            fail(f"unsafe shell value for {key}")
        print(f"{key}={value}")


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    base_parser = subparsers.add_parser("base")
    base_parser.add_argument("version_manifest")

    create_parser = subparsers.add_parser("create")
    create_parser.add_argument("version_manifest")
    create_parser.add_argument("--run-id", required=True)
    create_parser.add_argument("--source-commit", required=True)
    create_parser.add_argument("--source-manifest-sha256", required=True)

    identity_parser = subparsers.add_parser("identity")
    identity_parser.add_argument("version_manifest")
    identity_parser.add_argument("identity_manifest")

    args = parser.parse_args()
    if args.command == "base":
        emit_shell(validate_base(args.version_manifest))
        return
    if args.command == "identity":
        emit_shell(validate_identity(args.version_manifest, args.identity_manifest))
        return

    base = validate_base(args.version_manifest)
    if not RUN_ID.fullmatch(args.run_id):
        fail("invalid run_id")
    if not HEX_40.fullmatch(args.source_commit):
        fail("invalid source_commit")
    if not HEX_64.fullmatch(args.source_manifest_sha256):
        fail("invalid source_manifest_sha256")
    identity = {
        "schema": 1,
        "name": base["name"],
        "version": base["version"],
        "release": expected_release(base, args.run_id, args.source_manifest_sha256),
        "architecture": base["architecture"],
        "source_date_epoch": base["source_date_epoch"],
        "run_id": args.run_id,
        "source_commit": args.source_commit,
        "source_manifest_sha256": args.source_manifest_sha256,
    }
    json.dump(identity, sys.stdout, sort_keys=True, indent=2)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
