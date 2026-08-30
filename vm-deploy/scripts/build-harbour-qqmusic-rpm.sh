#!/usr/bin/env bash
set -euo pipefail

umask 0022
export LC_ALL=C
export TZ=UTC
export RPM_BUILD_NCPUS=1

PROJECT_ROOT="${PROJECT_ROOT:?PROJECT_ROOT must be set}"
OUTPUT_DIR="${OUTPUT_DIR:-$PROJECT_ROOT/artifacts/qqmusic-rpm}"
PACKAGE_DIR="$PROJECT_ROOT/packaging/apps/harbour-qqmusic"
TARGET="${TARGET:-SailfishOS-5.1.0.11-aarch64}"
SDK_CHROOT="${SDK_CHROOT:-/opt/sailfish-hadk/platform/sdks/sfossdk/sdk-chroot}"
SDK_USER="${SDK_USER:-builder}"
BUILD_LABEL="${QQMUSIC_BUILD_LABEL:?QQMUSIC_BUILD_LABEL must be set}"
VERSION_MANIFEST="$PACKAGE_DIR/version.json"
BUILD_IDENTITY="$PROJECT_ROOT/.qqmusic-build-identity.json"
VERSION_PARSER="$PROJECT_ROOT/vm-deploy/scripts/qqmusic-version.py"
STAGE_VERIFIER="$PROJECT_ROOT/vm-deploy/scripts/stage-harbour-qqmusic-source.sh"

fail() {
    echo "QQ Music RPM build failed: $*" >&2
    exit 1
}

case "$PROJECT_ROOT" in
    /root/bonito-deploy/work/qqmusic-native-*) ;;
    *) fail "PROJECT_ROOT must be a run-specific VM-local QQ Music root" ;;
esac
case "$OUTPUT_DIR" in
    "$PROJECT_ROOT"/artifacts/*) ;;
    *) fail "OUTPUT_DIR must be run-specific and below PROJECT_ROOT/artifacts" ;;
esac
case "$BUILD_LABEL" in
    ''|*[!A-Za-z0-9_-]*) fail "QQMUSIC_BUILD_LABEL is unsafe" ;;
esac
case "$PROJECT_ROOT$OUTPUT_DIR$TARGET$SDK_USER$BUILD_LABEL" in
    *[!A-Za-z0-9_./-]*) fail "build paths and identifiers contain unsupported characters" ;;
esac

for tool in find install python3 readelf rpmbuild rpm sha256sum strip touch; do
    command -v "$tool" >/dev/null || fail "missing build tool: $tool"
done
test -x "$STAGE_VERIFIER" || fail "missing staged-source verifier"
"$STAGE_VERIFIER" --verify "$PROJECT_ROOT"
eval "$("$VERSION_PARSER" identity "$VERSION_MANIFEST" "$BUILD_IDENTITY")"
SDK_SOURCE_ROOT="${SDK_SOURCE_ROOT:-/home/$SDK_USER/qqmusic-native-$QQMUSIC_RUN_ID-sdk}"
"$STAGE_VERIFIER" --verify-sdk "$SDK_SOURCE_ROOT" "$SDK_USER"
export SOURCE_DATE_EPOCH
test -x "$SDK_CHROOT" || fail "missing Sailfish SDK chroot: $SDK_CHROOT"
test -f "$PACKAGE_DIR/harbour-qqmusic.pro" || fail "missing application project"
test -f "$PACKAGE_DIR/harbour-qqmusic.spec" || fail "missing RPM spec"
test -f "$PACKAGE_DIR/harbour-qqmusic.desktop" || fail "missing desktop entry"

work="$(mktemp -d /tmp/harbour-qqmusic-rpm.XXXXXX)"
main_build="/home/$SDK_USER/harbour-qqmusic-rpm-$QQMUSIC_RUN_ID-$BUILD_LABEL"
stage="$work/stage"
topdir="$work/rpmbuild"
cleanup() {
    status=$?
    if test "${KEEP_WORK:-0}" = 1; then
        echo "Preserved RPM work directory: $work"
        echo "Preserved main build directory: $main_build"
    else
        rm -rf -- "$work"
        rm -rf -- "$main_build"
    fi
    exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

test ! -e "$main_build" || fail "main build directory already exists: $main_build"
install -d -m0755 -o "$SDK_USER" -g "$SDK_USER" "$main_build"
install -d -m0755 "$stage/qml" "$stage/icons" \
    "$stage/licenses" "$topdir"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS} \
    "$OUTPUT_DIR"

sdk_package_dir="/parentroot$SDK_SOURCE_ROOT/packaging/apps/harbour-qqmusic"
main_flags="-O2 -g0 -fPIC -fPIE -fno-ident -ffile-prefix-map=$sdk_package_dir=/usr/src/harbour-qqmusic-$QQMUSIC_VERSION -ffile-prefix-map=$main_build=/usr/src/harbour-qqmusic-build"
main_ldflags="-pie -Wl,-z,relro,-z,now,--build-id=none"
printf -v sdk_command '%s' "set -euo pipefail
export LC_ALL=C TZ=UTC SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH
cd $main_build
mb2 -t $TARGET build-init
mb2 -t $TARGET build-shell env CFLAGS='$main_flags' CXXFLAGS='$main_flags' LDFLAGS='$main_ldflags' qmake '$sdk_package_dir/src/app.pro' 'QMAKE_CFLAGS_RELEASE=$main_flags' 'QMAKE_CXXFLAGS_RELEASE=$main_flags' 'QMAKE_LFLAGS_RELEASE=$main_ldflags'
mb2 -t $TARGET build-shell make -j1
mb2 -t $TARGET build-shell lrelease '$sdk_package_dir/translations/harbour-qqmusic-zh_CN.ts' -qm '$main_build/harbour-qqmusic-zh_CN.qm'
test -x '$main_build/harbour-qqmusic'
test -s '$main_build/harbour-qqmusic-zh_CN.qm'"
"$SDK_CHROOT" -u "$SDK_USER" -m all /bin/bash -lc "$sdk_command"

install -m0755 "$main_build/harbour-qqmusic" "$stage/harbour-qqmusic"
install -m0644 "$main_build/harbour-qqmusic-zh_CN.qm" \
    "$stage/harbour-qqmusic-zh_CN.qm"
install -m0644 "$PACKAGE_DIR/harbour-qqmusic.desktop" \
    "$stage/harbour-qqmusic.desktop"
cp -a "$PACKAGE_DIR/qml/." "$stage/qml/"
for size in 86 108 128 172; do
    install -D -m0644 \
        "$PACKAGE_DIR/icons/generated/${size}x${size}/harbour-qqmusic.png" \
        "$stage/icons/${size}x${size}/harbour-qqmusic.png"
done
for notice in LICENSE LICENSE.listen1 QT-LICENSES.md UPSTREAM.md COMMUNITY-NOTICE.md; do
    install -m0644 "$PACKAGE_DIR/$notice" "$stage/licenses/$notice"
done

main_elf="$stage/harbour-qqmusic"
strip --strip-unneeded --remove-section=.note.gnu.build-id \
    --remove-section=.comment "$main_elf"
readelf -h "$main_elf" | grep -Fq AArch64 || fail "main ELF is not AArch64"
readelf -h "$main_elf" | grep -Fq 'Position-Independent Executable' ||
    fail "main ELF is not PIE"
readelf -l "$main_elf" | grep -Fq GNU_RELRO || fail "main ELF lacks RELRO"
readelf -d "$main_elf" | grep -Eq 'BIND_NOW|FLAGS.*NOW' ||
    fail "main ELF lacks immediate binding"
! readelf -n "$main_elf" | grep -Fq 'Build ID:' || fail "main ELF contains a build ID"
! readelf -S "$main_elf" | grep -Fq '.comment' || fail "main ELF contains .comment"

find "$stage" -type d -exec chmod 0755 {} +
find "$stage" -type f ! -name harbour-qqmusic -exec chmod 0644 {} +
find "$stage" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +

rpm_name="$QQMUSIC_NAME-$QQMUSIC_VERSION-$QQMUSIC_RELEASE.$QQMUSIC_ARCH.rpm"
test ! -e "$OUTPUT_DIR/$rpm_name" || fail "output RPM already exists: $OUTPUT_DIR/$rpm_name"
rpmbuild -bb \
    --define "_topdir $topdir" \
    --define "_buildhost bonito-build" \
    --define "use_source_date_epoch_as_buildtime 1" \
    --define "clamp_mtime_to_source_date_epoch 1" \
    --define "_build_id_links none" \
    --define "stagingdir $stage" \
    --define "qqmusic_name $QQMUSIC_NAME" \
    --define "qqmusic_version $QQMUSIC_VERSION" \
    --define "qqmusic_release $QQMUSIC_RELEASE" \
    "$PACKAGE_DIR/harbour-qqmusic.spec"

rpm_path="$(find "$topdir/RPMS" -type f \
    -name "$rpm_name" -print -quit)"
test -n "$rpm_path" || fail "rpmbuild did not create the expected RPM"
install -m0644 "$rpm_path" "$OUTPUT_DIR/$(basename "$rpm_path")"
final_rpm="$OUTPUT_DIR/$rpm_name"
rpm -K --nosignature "$final_rpm"
nevra="$(rpm -qp --qf '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}' "$final_rpm")"
test "$nevra" = "$QQMUSIC_NAME-$QQMUSIC_VERSION-$QQMUSIC_RELEASE.$QQMUSIC_ARCH" ||
    fail "built RPM identity mismatch"
sha256="$(sha256sum "$final_rpm" | awk '{print $1}')"
bytes="$(stat -c '%s' "$final_rpm")"
echo "qqmusic_rpm_build=ready|run=$QQMUSIC_RUN_ID|nevra=$nevra|rpm=$final_rpm|bytes=$bytes|sha256=$sha256|source_manifest=$QQMUSIC_SOURCE_MANIFEST_SHA256|build=$BUILD_LABEL"
