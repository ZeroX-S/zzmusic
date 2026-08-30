#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C
export TZ=UTC
umask 0077

fail() {
    echo "QQ Music source staging failed: $*" >&2
    exit 1
}

validate_root() {
    local root="$1" kind="$2" sdk_user="${3:-builder}"
    case "$kind:$root" in
        canonical:/root/bonito-deploy/work/qqmusic-native-*) ;;
        sdk:/home/"$sdk_user"/qqmusic-native-*-sdk) ;;
        *) fail "invalid run-specific VM-local QQ Music root" ;;
    esac
    case "$root" in
        /media/psf/*) fail "/media/psf is import-only" ;;
    esac
}

verify_root() {
    local root="$1" kind="$2" sdk_user="${3:-builder}"
    local manifest identity version_parser version_manifest
    local manifest_sha actual_paths expected_paths
    validate_root "$root" "$kind" "$sdk_user"
    manifest="$root/.qqmusic-source-manifest.sha256"
    identity="$root/.qqmusic-build-identity.json"
    version_parser="$root/vm-deploy/scripts/qqmusic-version.py"
    version_manifest="$root/packaging/apps/harbour-qqmusic/version.json"
    test -s "$manifest" || fail "source manifest is missing"
    test -s "$identity" || fail "build identity is missing"
    test -x "$version_parser" || fail "version parser is missing"
    test -s "$version_manifest" || fail "version manifest is missing"
    (
        cd "$root"
        sha256sum -c .qqmusic-source-manifest.sha256 >/dev/null
    ) || fail "staged source hash mismatch"

    expected_paths="$(mktemp)"
    actual_paths="$(mktemp)"
    awk '{ print substr($0, 67) }' "$manifest" | sort > "$expected_paths"
    (
        cd "$root"
        find . -type f ! -name '.qqmusic-source-manifest.sha256' \
            ! -name '.qqmusic-build-identity.json' \
            ! -path './artifacts/*' -printf '%P\n' | sort
    ) > "$actual_paths"
    cmp "$expected_paths" "$actual_paths" >/dev/null ||
        fail "staged root contains files outside the manifest"
    rm -f -- "$expected_paths" "$actual_paths"

    manifest_sha="$(sha256sum "$manifest" | awk '{print $1}')"
    eval "$("$version_parser" identity "$version_manifest" "$identity")"
    test "$manifest_sha" = "$QQMUSIC_SOURCE_MANIFEST_SHA256" ||
        fail "identity references a different source manifest"
    echo "qqmusic_source_stage=verified|run=$QQMUSIC_RUN_ID|source=$QQMUSIC_SOURCE_COMMIT|manifest=$manifest_sha|root=$root"
}

mode="${1:-stage}"
case "$mode" in
    --verify)
        test "$#" -eq 2 || fail "usage: $0 --verify PROJECT_ROOT"
        verify_root "$2" canonical
        exit 0
        ;;
    --verify-sdk)
        test "$#" -eq 3 || fail "usage: $0 --verify-sdk SDK_ROOT SDK_USER"
        verify_root "$2" sdk "$3"
        exit 0
        ;;
    stage)
        test "$#" -eq 1 || fail "usage: $0 stage"
        ;;
    *) fail "usage: $0 stage | --verify PROJECT_ROOT | --verify-sdk SDK_ROOT SDK_USER" ;;
esac

run_id="${QQMUSIC_RUN_ID:?QQMUSIC_RUN_ID must be set}"
source_commit="${QQMUSIC_SOURCE_COMMIT:?QQMUSIC_SOURCE_COMMIT must be set}"
import_root="${IMPORT_ROOT:-/media/psf/bonito-port/.worktrees/qqmusic-native}"
destination="${DEST_ROOT:-/root/bonito-deploy/work/qqmusic-native-$run_id}"
sdk_user="${SDK_USER:-builder}"
sdk_view="${SDK_VIEW_ROOT:-/home/$sdk_user/qqmusic-native-$run_id-sdk}"
allowlist="$import_root/vm-deploy/qqmusic-source-allowlist.txt"
version_parser="$import_root/vm-deploy/scripts/qqmusic-version.py"
version_manifest="$import_root/packaging/apps/harbour-qqmusic/version.json"

case "$run_id" in
    [0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]T[0-9][0-9][0-9][0-9][0-9][0-9]Z) ;;
    *) fail "QQMUSIC_RUN_ID must be a UTC basic timestamp" ;;
esac
case "$source_commit" in
    [0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]) ;;
    *) fail "QQMUSIC_SOURCE_COMMIT must be a full lowercase commit ID" ;;
esac
case "$import_root" in
    /media/psf/*) ;;
    *) fail "IMPORT_ROOT must be below /media/psf" ;;
esac
case "$sdk_user" in
    ''|*[!A-Za-z0-9_-]*) fail "SDK_USER is unsafe" ;;
esac
getent passwd "$sdk_user" >/dev/null || fail "SDK user does not exist"
validate_root "$destination" canonical
validate_root "$sdk_view" sdk "$sdk_user"
test ! -e "$destination" || fail "destination already exists: $destination"
test ! -e "$sdk_view" || fail "SDK view already exists: $sdk_view"
test -s "$allowlist" || fail "source allowlist is missing"
test -x "$version_parser" || fail "version parser is missing"
test -s "$version_manifest" || fail "version manifest is missing"
test "$(sort -u "$allowlist" | wc -l)" = "$(wc -l < "$allowlist")" ||
    fail "source allowlist is not unique"
cmp <(sort "$allowlist") "$allowlist" >/dev/null ||
    fail "source allowlist is not sorted"

temporary="${destination}.tmp.$$"
sdk_temporary="${sdk_view}.tmp.$$"
test ! -e "$temporary" || fail "temporary destination already exists"
test ! -e "$sdk_temporary" || fail "temporary SDK view already exists"
install -d -m0755 "$temporary"
cleanup() {
    status=$?
    if test -d "$temporary"; then
        rm -rf -- "$temporary"
    fi
    if test -d "$sdk_temporary"; then
        rm -rf -- "$sdk_temporary"
    fi
    exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

manifest="$temporary/.qqmusic-source-manifest.sha256"
: > "$manifest"
while IFS= read -r relative || test -n "$relative"; do
    test -n "$relative" || fail "blank allowlist entry"
    case "$relative" in
        /*|../*|*/../*|*/..|.codex-device-shots/*|artifacts/*|logs/*|*.rpm)
            fail "unsafe allowlist entry"
            ;;
    esac
    source_file="$import_root/$relative"
    target_file="$temporary/$relative"
    test -f "$source_file" || fail "allowlisted source is missing: $relative"
    install -d -m0755 "${target_file%/*}"
    cp -p "$source_file" "$target_file"
    source_sha="$(sha256sum "$source_file" | awk '{print $1}')"
    target_sha="$(sha256sum "$target_file" | awk '{print $1}')"
    test "$source_sha" = "$target_sha" || fail "copy mismatch: $relative"
    printf '%s  %s\n' "$target_sha" "$relative" >> "$manifest"
done < "$allowlist"

manifest_sha="$(sha256sum "$manifest" | awk '{print $1}')"
"$version_parser" create "$version_manifest" \
    --run-id "$run_id" \
    --source-commit "$source_commit" \
    --source-manifest-sha256 "$manifest_sha" \
    > "$temporary/.qqmusic-build-identity.json"
chmod 0644 "$manifest" "$temporary/.qqmusic-build-identity.json"
mv "$temporary" "$destination"
install -d -m0755 -o "$sdk_user" -g "$sdk_user" "$sdk_temporary"
cp -a "$destination/." "$sdk_temporary/"
chown -R "$sdk_user:$sdk_user" "$sdk_temporary"
mv "$sdk_temporary" "$sdk_view"
trap - EXIT HUP INT TERM
verify_root "$destination" canonical
verify_root "$sdk_view" sdk "$sdk_user"
canonical_manifest_sha="$(sha256sum "$destination/.qqmusic-source-manifest.sha256" | awk '{print $1}')"
sdk_manifest_sha="$(sha256sum "$sdk_view/.qqmusic-source-manifest.sha256" | awk '{print $1}')"
test "$canonical_manifest_sha" = "$sdk_manifest_sha" ||
    fail "canonical and SDK source manifests differ"
echo "qqmusic_source_stage=ready|run=$run_id|canonical=$destination|sdk_view=$sdk_view|manifest=$canonical_manifest_sha"
