#!/bin/sh
set -eu

suite="${1:-all}"
shift || true
build_jobs="${QQMUSIC_TEST_BUILD_JOBS:-1}"

case "$build_jobs" in
    ''|*[!0-9]*)
        echo "QQMUSIC_TEST_BUILD_JOBS must be an integer from 1 to 8" >&2
        exit 2
        ;;
esac
if test "$build_jobs" -lt 1 || test "$build_jobs" -gt 8; then
    echo "QQMUSIC_TEST_BUILD_JOBS must be an integer from 1 to 8" >&2
    exit 2
fi

case "$suite" in
    auto|qml|all) ;;
    *)
        echo "Usage: $0 [auto|qml|all] [--test tst_filename]" >&2
        exit 2
        ;;
esac

test_name=
if test "${1:-}" = --test; then
    test_name="${2:?missing test name}"
    shift 2
fi
if test "$#" -ne 0; then
    echo "Usage: $0 [auto|qml|all] [--test tst_filename]" >&2
    exit 2
fi
if test -n "$test_name"; then
    case "$test_name" in
        tst_*) ;;
        *)
            echo "--test must name one tst_* selector" >&2
            exit 2
            ;;
    esac
    case "$test_name" in
        *[!A-Za-z0-9_-]*)
            echo "--test must be a safe tst_* basename" >&2
            exit 2
            ;;
    esac
fi
case "$suite" in
    all)
        if test -n "$test_name"; then
            echo "--test is not supported by all" >&2
            exit 2
        fi
        ;;
esac

sdk_chroot="${SDK_CHROOT:-/opt/sailfish-hadk/platform/sdks/sfossdk/sdk-chroot}"
sdk_user="${SDK_USER:-builder}"
test -x "$sdk_chroot" || {
    echo "Missing Sailfish SDK chroot" >&2
    exit 1
}
getent passwd "$sdk_user" >/dev/null || {
    echo "Missing Sailfish SDK user: $sdk_user" >&2
    exit 1
}
target="${TARGET:?TARGET must be set}"

project_root="${PROJECT_ROOT:?PROJECT_ROOT must be set}"
case "$project_root" in
    /root/bonito-deploy/work/qqmusic-native-*) ;;
    *)
        echo "PROJECT_ROOT must be a run-specific VM-local QQ Music root" >&2
        exit 2
        ;;
esac
app_root="$project_root/packaging/apps/harbour-qqmusic"
auto_project="$app_root/tests/auto/auto.pro"
stage_verifier="$project_root/vm-deploy/scripts/stage-harbour-qqmusic-source.sh"
version_parser="$project_root/vm-deploy/scripts/qqmusic-version.py"
version_manifest="$app_root/version.json"
build_identity="$project_root/.qqmusic-build-identity.json"

test -x "$stage_verifier" || {
    echo "Missing staged-source verifier: $stage_verifier" >&2
    exit 1
}
"$stage_verifier" --verify "$project_root"
eval "$("$version_parser" identity "$version_manifest" "$build_identity")"
sdk_project_root="/home/$sdk_user/qqmusic-native-$QQMUSIC_RUN_ID-sdk"
"$stage_verifier" --verify-sdk "$sdk_project_root" "$sdk_user"
sdk_app_root="/parentroot$sdk_project_root/packaging/apps/harbour-qqmusic"
sdk_auto_project="$sdk_app_root/tests/auto/auto.pro"

test -f "$app_root/harbour-qqmusic.pro" || {
    echo "Missing harbour-qqmusic project: $app_root/harbour-qqmusic.pro" >&2
    exit 1
}

work_parent="/home/$sdk_user"

if work="$(mktemp -d "$work_parent/harbour-qqmusic-tests.XXXXXX" 2>/dev/null)"; then
    :
elif test -x /usr/bin/python3; then
    work="$(QQMUSIC_WORK_PARENT="$work_parent" /usr/bin/python3 - <<'PY'
import os
import tempfile

print(tempfile.mkdtemp(prefix="harbour-qqmusic-tests.",
                       dir=os.environ["QQMUSIC_WORK_PARENT"]))
PY
)"
else
    echo "Unable to create private build directory" >&2
    exit 1
fi
chown "$sdk_user:$sdk_user" "$work"
chmod 0755 "$work"
cleanup() {
    status=$?
    if test "$status" -ne 0; then
        echo "qqmusic_test_suite=failed|run=$QQMUSIC_RUN_ID|suite=$suite|source_manifest=$QQMUSIC_SOURCE_MANIFEST_SHA256"
    fi
    if test "${KEEP_WORK:-0}" = 1; then
        echo "Preserved build directory: $work"
    else
        rm -rf -- "$work"
    fi
    exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

run_auto() {
    test -f "$auto_project" || {
        echo "Missing auto test project: $auto_project" >&2
        return 1
    }

    test_manifest="$work/auto-tests.list"
    if test -n "$test_name"; then
        printf '%s\n' "$app_root/tests/auto/$test_name.cpp" > "$test_manifest"
    else
        for test_source in "$app_root"/tests/auto/tst_*.cpp; do
            test -f "$test_source" || continue
            printf '%s\n' "$test_source" >> "$test_manifest"
        done
    fi
    test -s "$test_manifest" || {
        echo "No auto tests found" >&2
        return 1
    }

    while IFS= read -r test_source || test -n "$test_source"; do
        test -f "$test_source" || {
            echo "Missing auto test: $test_source" >&2
            return 1
        }
        build_dir="$work/auto/${test_source##*/}"
        test_relative="${test_source#"$app_root"/}"
        sdk_test_source="$sdk_app_root/$test_relative"
        install -d -m0755 -o "$sdk_user" -g "$sdk_user" "$build_dir"
        sdk_command="set -eu
export LC_ALL=C TZ=UTC
cd '$build_dir'
mb2 -t '$target' build-init
mb2 -t '$target' build-shell qmake '$sdk_auto_project' 'TEST_SOURCE=$sdk_test_source' 'DEFINES+=QQMUSIC_SB2_EMULATION'
mb2 -t '$target' build-shell make -j'$build_jobs'
mb2 -t '$target' build-shell './${test_source##*/}'"
        "$sdk_chroot" -u "$sdk_user" -m all /bin/bash -lc "$sdk_command"
    done < "$test_manifest"
}

run_qml() {
    qml_test_dir="$app_root/tests/qml"
    qml_test_manifest="$work/qml-tests.list"
    test -d "$qml_test_dir" || {
        echo "QML test suite is not available: $qml_test_dir" >&2
        return 1
    }
    if test "$suite" = qml && test -n "$test_name"; then
        qml_test_source="$qml_test_dir/$test_name.qml"
        test -f "$qml_test_source" || {
            echo "Missing QML test: $qml_test_source" >&2
            return 1
        }
        printf '%s\n' "$qml_test_source" > "$qml_test_manifest"
    else
        find "$qml_test_dir" -type f -name 'tst_*.qml' | sort > "$qml_test_manifest"
        test -s "$qml_test_manifest" || {
            echo "No QML tests found: $qml_test_dir" >&2
            return 1
        }
    fi
    install -d -m0755 -o "$sdk_user" -g "$sdk_user" "$work/qml"
    sdk_command="set -eu
export LC_ALL=C TZ=UTC
cd '$work/qml'
mb2 -t '$target' build-init"
    "$sdk_chroot" -u "$sdk_user" -m all /bin/bash -lc "$sdk_command"
    while IFS= read -r qml_test_source || test -n "$qml_test_source"; do
        qml_relative="${qml_test_source#"$app_root"/}"
        sdk_qml_test_source="$sdk_app_root/$qml_relative"
        sdk_command="set -eu
export LC_ALL=C TZ=UTC
cd '$work/qml'
mb2 -t '$target' build-shell env QT_QPA_PLATFORM=minimal QT_QUICK_BACKEND=software QT_IM_MODULE=none qmltestrunner -input '$sdk_qml_test_source'"
        "$sdk_chroot" -u "$sdk_user" -m all /bin/bash -lc "$sdk_command"
    done < "$qml_test_manifest"
}

case "$suite" in
    auto) run_auto ;;
    qml) run_qml ;;
    all)
        run_auto
        run_qml
        ;;
esac

echo "qqmusic_test_suite=ready|run=$QQMUSIC_RUN_ID|suite=$suite|source_manifest=$QQMUSIC_SOURCE_MANIFEST_SHA256"
