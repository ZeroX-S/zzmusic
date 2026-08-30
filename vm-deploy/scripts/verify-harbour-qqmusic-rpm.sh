#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C
export TZ=UTC

PROJECT_ROOT="${PROJECT_ROOT:?PROJECT_ROOT must be set}"
RPM="${QQMUSIC_RPM:?QQMUSIC_RPM must identify the exact run-scoped artifact}"
PACKAGE_DIR="$PROJECT_ROOT/packaging/apps/harbour-qqmusic"
VERSION_MANIFEST="$PACKAGE_DIR/version.json"
BUILD_IDENTITY="$PROJECT_ROOT/.qqmusic-build-identity.json"
VERSION_PARSER="$PROJECT_ROOT/vm-deploy/scripts/qqmusic-version.py"
STAGE_VERIFIER="$PROJECT_ROOT/vm-deploy/scripts/stage-harbour-qqmusic-source.sh"
VERIFY_OUTPUT_DIR="${QQMUSIC_VERIFY_OUTPUT_DIR:-$PROJECT_ROOT/artifacts/verification}"

fail() {
    echo "QQ Music RPM verification failed: $*" >&2
    exit 1
}

case "$PROJECT_ROOT" in
    /root/bonito-deploy/work/qqmusic-native-*) ;;
    *) fail "PROJECT_ROOT must be a run-specific VM-local QQ Music root" ;;
esac
case "$RPM" in
    "$PROJECT_ROOT"/artifacts/*) ;;
    *) fail "QQMUSIC_RPM must be below the run-specific PROJECT_ROOT/artifacts" ;;
esac
case "$VERIFY_OUTPUT_DIR" in
    "$PROJECT_ROOT"/artifacts/*) ;;
    *) fail "QQMUSIC_VERIFY_OUTPUT_DIR must be below PROJECT_ROOT/artifacts" ;;
esac

for tool in awk comm cpio diff find grep python3 readelf rpm rpm2cpio sed \
    sha256sum sort stat tr wc; do
    command -v "$tool" >/dev/null || fail "missing verification tool: $tool"
done
test -x "$STAGE_VERIFIER" || fail "missing staged-source verifier"
"$STAGE_VERIFIER" --verify "$PROJECT_ROOT"
eval "$("$VERSION_PARSER" identity "$VERSION_MANIFEST" "$BUILD_IDENTITY")"
python3 -c 'from PIL import Image' >/dev/null 2>&1 ||
    fail "Pillow is required for icon verification"
test -s "$RPM" || fail "missing RPM: $RPM"
expected_nevra="$QQMUSIC_NAME-$QQMUSIC_VERSION-$QQMUSIC_RELEASE.$QQMUSIC_ARCH"
expected_rpm="$expected_nevra.rpm"
test "$(basename "$RPM")" = "$expected_rpm" || fail "RPM filename differs from identity"
test "$(rpm -qp --qf '%{NAME}' "$RPM")" = "$QQMUSIC_NAME" || fail "wrong package name"
test "$(rpm -qp --qf '%{VERSION}' "$RPM")" = "$QQMUSIC_VERSION" || fail "wrong package version"
test "$(rpm -qp --qf '%{RELEASE}' "$RPM")" = "$QQMUSIC_RELEASE" || fail "wrong package release"
test "$(rpm -qp --qf '%{ARCH}' "$RPM")" = "$QQMUSIC_ARCH" || fail "wrong package architecture"
grep -Fq 'digests OK' <<<"$(rpm -K --nosignature "$RPM")" || fail "RPM digest check failed"
test -z "$(rpm -qp --scripts "$RPM")" || fail "RPM contains installation scripts"
rpm -qp --obsoletes "$RPM" | grep -Eq '^listen1-sailfish[[:space:]]*<[[:space:]]*2\.34\.0$' ||
    fail "Listen1 obsoletes contract missing"
rpm -qp --provides "$RPM" | grep -Eq '^listen1-sailfish[[:space:]]*=[[:space:]]*2\.34\.0$' ||
    fail "Listen1 compatibility provide missing"
rpm -qp --obsoletes "$RPM" | grep -Fxq 'harbour-qqmusic' ||
    fail "harbour-qqmusic replacement contract missing"
rpm -qp --provides "$RPM" | grep -Fxq \
    "harbour-qqmusic = $QQMUSIC_VERSION-$QQMUSIC_RELEASE" ||
    fail "harbour-qqmusic compatibility provide missing"

requires="$(rpm -qp --requires "$RPM")"
for requirement in \
    'libsailfishapp >= 1.2.17' \
    'sailfishsilica-qt5 >= 1.2.154' \
    'qt5-qtmultimedia >= 5.6.2' \
    'qt5-qtmultimedia-plugin-mediaservice-gstmediaplayer' \
    'qt5-qtmultimedia-plugin-audio-pulseaudio' \
    'qt5-qtmultimedia-plugin-resourcepolicy-resourceqt' \
    'libsailfishsecretsplugin' \
    'sailfishsecretsdaemon' \
    'sailfishsecretsdaemon-secretsplugins-default' \
    'nemo-qml-plugin-notifications-qt5'; do
    grep -Fxq "$requirement" <<<"$requires" || fail "missing requirement: $requirement"
done
! grep -Eiq 'qt6|webengine' <<<"$requires" ||
    fail "RPM declares a Qt 6 or WebEngine requirement"

work="$(mktemp -d /tmp/harbour-qqmusic-rpm-verify.XXXXXX)"
cleanup() {
    status=$?
    rm -rf -- "$work"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
root="$work/root"
mkdir -p "$root"
(cd "$root" && rpm2cpio "$RPM" | cpio --quiet -idmu)

actual_files="$work/actual-files"
expected_files="$work/expected-files"
rpm -qpl "$RPM" | sort -u > "$actual_files"
{
    printf '%s\n' \
        /usr/bin/harbour-qqmusic \
        /usr/share/applications/harbour-qqmusic.desktop \
        /usr/share/harbour-qqmusic \
        /usr/share/harbour-qqmusic/qml \
        /usr/share/icons/hicolor/86x86/apps/harbour-qqmusic.png \
        /usr/share/icons/hicolor/108x108/apps/harbour-qqmusic.png \
        /usr/share/icons/hicolor/128x128/apps/harbour-qqmusic.png \
        /usr/share/icons/hicolor/172x172/apps/harbour-qqmusic.png \
        "/usr/share/licenses/$QQMUSIC_NAME" \
        "/usr/share/licenses/$QQMUSIC_NAME/LICENSE" \
        "/usr/share/licenses/$QQMUSIC_NAME/LICENSE.listen1" \
        "/usr/share/licenses/$QQMUSIC_NAME/QT-LICENSES.md" \
        "/usr/share/licenses/$QQMUSIC_NAME/UPSTREAM.md" \
        "/usr/share/licenses/$QQMUSIC_NAME/COMMUNITY-NOTICE.md" \
        /usr/share/translations/harbour-qqmusic-zh_CN.qm
    (cd "$PACKAGE_DIR/qml" && find . -mindepth 1 -printf '/usr/share/harbour-qqmusic/qml/%P\n')
} | sort -u > "$expected_files"
if ! diff -u "$expected_files" "$actual_files"; then
    fail "RPM ownership differs from the reviewed file set"
fi

file_metadata="$work/file-metadata"
rpm -qp --qf '[%{FILENAMES}\t%{FILEUSERNAME}\t%{FILEGROUPNAME}\t%{FILEMODES:perms}\n]' \
    "$RPM" > "$file_metadata"
awk -F '\t' '$2 != "root" || $3 != "root" { exit 1 }' "$file_metadata" ||
    fail "RPM payload contains non-root ownership"
grep -Eq '^/usr/bin/harbour-qqmusic[[:space:]]+root[[:space:]]+root[[:space:]]+-rwxr-xr-x$' \
    "$file_metadata" || fail "main ELF mode is not 0755"

desktop="$root/usr/share/applications/harbour-qqmusic.desktop"
expected_display_name='ZZ Music'
for line in \
    "Name=$expected_display_name" \
    "Name[zh_CN]=$expected_display_name" \
    'Exec=/usr/bin/harbour-qqmusic' \
    'Icon=harbour-qqmusic' \
    'X-Nemo-Application-Type=silica-qt5' \
    'OrganizationName=org.bonito.qqmusic' \
    'ApplicationName=harbour-qqmusic' \
    'Permissions=Internet;Audio;Secrets'; do
    grep -Fxq "$line" "$desktop" || fail "desktop entry is missing: $line"
done
! grep -Fq 'Sandboxing=Disabled' "$desktop" || fail "desktop entry disables sandboxing"
test "$(find "$root/usr/share/applications" -type f | wc -l)" -eq 1 ||
    fail "unexpected desktop entry is packaged"

for notice in LICENSE LICENSE.listen1 QT-LICENSES.md UPSTREAM.md COMMUNITY-NOTICE.md; do
    test -s "$root/usr/share/licenses/$QQMUSIC_NAME/$notice" || fail "missing notice: $notice"
done
test -s "$root/usr/share/translations/harbour-qqmusic-zh_CN.qm" || fail "compiled translation missing"
test "$(find "$root/usr/share/harbour-qqmusic/qml" -type f -name '*.qml' | wc -l)" = \
    "$(find "$PACKAGE_DIR/qml" -type f -name '*.qml' | wc -l)" || fail "QML payload is incomplete"

python3 - "$root" "$PACKAGE_DIR" <<'PY'
import hashlib
import sys
from pathlib import Path
from PIL import Image

root = Path(sys.argv[1]) / "usr/share/icons/hicolor"
source_root = Path(sys.argv[2]) / "icons/generated"
for size in (86, 108, 128, 172):
    path = root / f"{size}x{size}" / "apps" / "harbour-qqmusic.png"
    source = source_root / f"{size}x{size}" / "harbour-qqmusic.png"
    if not path.is_file():
        raise SystemExit(f"missing icon: {path}")
    if not source.is_file():
        raise SystemExit(f"missing source icon: {source}")
    if hashlib.sha256(path.read_bytes()).digest() != hashlib.sha256(source.read_bytes()).digest():
        raise SystemExit(f"packaged icon differs from source: {path}")
    with Image.open(path) as image:
        if image.size != (size, size) or image.mode != "RGBA":
            raise SystemExit(f"invalid icon: {path}: {image.size} {image.mode}")
        alpha = image.getchannel("A")
        if alpha.getbbox() is None:
            raise SystemExit(f"fully transparent icon: {path}")
PY

if find "$root" -type f \( -name '*.html' -o -name '*.js' -o -name '*.map' \
        -o -name 'listen1.png' -o -name 'listen1-sailfish.png' \) | grep -q .; then
    fail "web payload, source map, or legacy icon is packaged"
fi
if find "$root" -type f | grep -Eiq 'helper|webengine|qt6'; then
    fail "Qt 6, WebEngine, or helper payload is packaged"
fi
login_qml="$root/usr/share/harbour-qqmusic/qml/pages/MyMusicPage.qml"
for marker in \
    'page.controller.startLogin()' \
    'page.controller.cancelLogin()' \
    'page.controller.loginQrImage' \
    'page.controller.loginQrGeneration'; do
    grep -Fq "$marker" "$login_qml" ||
        fail "native Qt 5 login entry point is missing: $marker"
done
if grep -RaiE 'LoginWindow|QtWebEngine|qqmusic-login-helper|QLocal(Server|Socket)' \
        "$root/usr/share/harbour-qqmusic"; then
    fail "removed login helper or IPC entry point remains in packaged QML"
fi
if grep -RaiE 'music[.]163|ku[g]ou|ku[w]o|mi[g]u|last[f]m|client[_]secret' \
        "$root/usr/share/harbour-qqmusic" "$root/usr/share/applications"; then
    fail "non-QQ provider or credential marker is packaged"
fi

main_elf="$root/usr/bin/harbour-qqmusic"
verify_elf() {
    local elf="$1" label="$2"
    readelf -h "$elf" | grep -Fq AArch64 || fail "$label is not AArch64"
    readelf -h "$elf" | grep -Fq 'Position-Independent Executable' || fail "$label is not PIE"
    readelf -l "$elf" | grep -Fq GNU_RELRO || fail "$label lacks RELRO"
    readelf -d "$elf" | grep -Eq 'BIND_NOW|FLAGS.*NOW' || fail "$label lacks immediate binding"
    ! readelf -n "$elf" | grep -Fq 'Build ID:' || fail "$label contains a build ID"
    ! readelf -S "$elf" | grep -Fq '.comment' || fail "$label contains .comment"
}

verify_needed() {
    local elf="$1" label="$2" allowed="$3" needed unknown
    needed="$(readelf -d "$elf" | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p' | sort -u)"
    unknown="$(comm -23 <(printf '%s\n' "$needed") <(printf '%s\n' "$allowed" | sort -u))"
    test -z "$unknown" || fail "$label has unreviewed NEEDED entries: $(tr '\n' ' ' <<<"$unknown")"
    printf '%s\n' "$needed"
}

verify_versions() {
    local elf="$1" label="$2" max_glibc max_glibcxx max_cxxabi
    max_glibc="$(readelf --version-info "$elf" 2>/dev/null | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)+' | sort -Vu | tail -1 || true)"
    max_glibcxx="$(readelf --version-info "$elf" 2>/dev/null | grep -oE 'GLIBCXX_[0-9]+(\.[0-9]+)+' | sort -Vu | tail -1 || true)"
    max_cxxabi="$(readelf --version-info "$elf" 2>/dev/null | grep -oE 'CXXABI_[0-9]+(\.[0-9]+)+' | sort -Vu | tail -1 || true)"
    test "$(printf '%s\n%s\n' "${max_glibc:-GLIBC_0}" GLIBC_2.41 | sort -V | tail -1)" = GLIBC_2.41 ||
        fail "$label exceeds GLIBC_2.41: $max_glibc"
    test "$(printf '%s\n%s\n' "${max_glibcxx:-GLIBCXX_0}" GLIBCXX_3.4.32 | sort -V | tail -1)" = GLIBCXX_3.4.32 ||
        fail "$label exceeds GLIBCXX_3.4.32: $max_glibcxx"
    test "$(printf '%s\n%s\n' "${max_cxxabi:-CXXABI_0}" CXXABI_1.3.14 | sort -V | tail -1)" = CXXABI_1.3.14 ||
        fail "$label exceeds CXXABI_1.3.14: $max_cxxabi"
    echo "$label ABI maxima: GLIBC=${max_glibc:-none} GLIBCXX=${max_glibcxx:-none} CXXABI=${max_cxxabi:-none}"
}

main_allowed='libQt5Core.so.5
libQt5DBus.so.5
libQt5Gui.so.5
libQt5Multimedia.so.5
libQt5Network.so.5
libQt5Qml.so.5
libQt5Quick.so.5
libGLESv2.so.2
libc.so.6
libdl.so.2
libgcc_s.so.1
libm.so.6
libmdeclarativecache5.so.0
libpthread.so.0
libsailfishapp.so.1
libsailfishsecrets.so.0
libstdc++.so.6'
verify_elf "$main_elf" 'main ELF'
main_needed="$(verify_needed "$main_elf" 'main ELF' "$main_allowed")"
for library in libsailfishapp.so.1 libQt5Quick.so.5 libQt5Multimedia.so.5 \
    libQt5Network.so.5 libsailfishsecrets.so.0; do
    grep -Fxq "$library" <<<"$main_needed" || fail "main ELF does not require $library"
done
! grep -Eq '^libQt6|WebEngine' <<<"$main_needed" || fail "main ELF crosses into Qt 6/WebEngine"
verify_versions "$main_elf" 'main ELF'

install -d -m0755 "$VERIFY_OUTPUT_DIR"
rpm -qpl "$RPM" > "$VERIFY_OUTPUT_DIR/rpm-payload.txt"
rpm -qp --qf '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n' "$RPM" > \
    "$VERIFY_OUTPUT_DIR/rpm-nevra.txt"
sha256="$(sha256sum "$RPM" | awk '{print $1}')"
bytes="$(stat -c '%s' "$RPM")"
printf '%s  %s\n' "$sha256" "$RPM" > "$VERIFY_OUTPUT_DIR/rpm-sha256.txt"
echo "qqmusic_rpm_verify=ready|run=$QQMUSIC_RUN_ID|nevra=$expected_nevra|rpm=$RPM|bytes=$bytes|sha256=$sha256|payload=$VERIFY_OUTPUT_DIR/rpm-payload.txt|source_manifest=$QQMUSIC_SOURCE_MANIFEST_SHA256"
