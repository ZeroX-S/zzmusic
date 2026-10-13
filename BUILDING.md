# Building ZZ Music

## Supported build environment

The accepted baseline is built in a Linux Sailfish SDK/VM with:

- target **SailfishOS-5.1.0.11-aarch64**;
- Qt **5.6.3**;
- qmake, make, lrelease, mb2, rpmbuild and standard RPM tools;
- Python 3 and Pillow for package/icon verification.

Tests use the same Qt 5 target. There is no login helper or separate native-host Qt 6 suite.

The application remains Qt 5-only. Qt 6 and WebEngine are outside the project boundary.

## Compile the application

Inside a configured Sailfish SDK build shell:

~~~sh
qmake packaging/apps/harbour-qqmusic/src/app.pro
make -j2
lrelease packaging/apps/harbour-qqmusic/translations/harbour-qqmusic-zh_CN.ts \
  -qm harbour-qqmusic-zh_CN.qm
~~~

The executable target remains **harbour-qqmusic** for Sailfish compatibility; the package and displayed product name are **zzmusic** and **ZZ Music**.

## Manifest-staged tests and RPM

The repository includes the exact staging, test, build and verification tools used for the accepted development RPM. They intentionally require a run-specific VM-local source directory and treat **/media/psf** as import-only.

Assuming the repository is visible to the VM at **/media/psf/zzmusic**:

~~~sh
run_id=$(date -u +%Y%m%dT%H%M%SZ)
source_commit=$(git rev-parse HEAD)

QQMUSIC_RUN_ID="$run_id" \
QQMUSIC_SOURCE_COMMIT="$source_commit" \
IMPORT_ROOT=/media/psf/zzmusic \
vm-deploy/scripts/stage-harbour-qqmusic-source.sh stage
~~~

Run the complete Qt 5 suite:

~~~sh
PROJECT_ROOT="/root/bonito-deploy/work/qqmusic-native-$run_id" \
TARGET=SailfishOS-5.1.0.11-aarch64 \
QQMUSIC_TEST_BUILD_JOBS=2 \
"/root/bonito-deploy/work/qqmusic-native-$run_id/vm-deploy/scripts/run-harbour-qqmusic-tests.sh" all
~~~

Build and verify one run-specific RPM:

~~~sh
project_root="/root/bonito-deploy/work/qqmusic-native-$run_id"

PROJECT_ROOT="$project_root" \
OUTPUT_DIR="$project_root/artifacts/zzmusic-rpm" \
TARGET=SailfishOS-5.1.0.11-aarch64 \
QQMUSIC_BUILD_LABEL=public \
QQMUSIC_BUILD_JOBS=4 \
"$project_root/vm-deploy/scripts/build-harbour-qqmusic-rpm.sh"

eval "$("$project_root/vm-deploy/scripts/qqmusic-version.py" identity \
  "$project_root/packaging/apps/harbour-qqmusic/version.json" \
  "$project_root/.qqmusic-build-identity.json")"
rpm_file="$project_root/artifacts/zzmusic-rpm/$QQMUSIC_NAME-$QQMUSIC_VERSION-$QQMUSIC_RELEASE.$QQMUSIC_ARCH.rpm"

PROJECT_ROOT="$project_root" \
QQMUSIC_RPM="$rpm_file" \
"$project_root/vm-deploy/scripts/verify-harbour-qqmusic-rpm.sh"
~~~

Do not select an artifact merely because it is newest. Always record and verify its exact NEVRA, byte size, payload and SHA-256.

`QQMUSIC_BUILD_JOBS` accepts 1–8 and defaults to 1. Choose a value appropriate to the VM's memory and CPU count.

## Published development build

~~~text
Run ID: 20261013T113645Z
Source manifest SHA-256: 8ef344bc6f8b476ad3a904095b227ef36496b879aaf6a6e60a9b44a19d858d94
NEVRA: zzmusic-1.0.0-27.bonito.dev.20261013t113645z.8ef344bc6f8b.aarch64
Bytes: 456540
SHA-256: 923c3d70a7ea1d3afb10503984343b001051509c134e7d1f74d36ebe3f3e8214
~~~

The application files in this public tree match the tested build inputs, excluding local engineering instructions. Public documentation and the staging allowlist are maintained separately from the private integration workspace. The release includes an application-file SHA-256 manifest and build identity linking the RPM to its public source commit.

One clean build produced this artifact; this release does not claim bit-for-bit deterministic reproduction. A build from this public tree receives a fresh run ID, source-manifest hash and NEVRA. See [the validation record](docs/releases/native-v1.0.0-27.md) for test scope and device acceptance limits.
