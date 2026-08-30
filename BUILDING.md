# Building ZZ Music

## Supported build environment

The accepted baseline is built in a Linux Sailfish SDK/VM with:

- target **SailfishOS-5.1.0.11-aarch64**;
- Qt **5.6.3**;
- qmake, make, lrelease, mb2, rpmbuild and standard RPM tools;
- CMake, Ninja and CTest for the native helper test suite.

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
vm-deploy/scripts/stage-harbour-qqmusic-source.sh
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
"$project_root/vm-deploy/scripts/build-harbour-qqmusic-rpm.sh"

PROJECT_ROOT="$project_root" \
QQMUSIC_RPM="$(find "$project_root/artifacts/zzmusic-rpm" -type f -name '*.rpm' -print -quit)" \
"$project_root/vm-deploy/scripts/verify-harbour-qqmusic-rpm.sh"
~~~

Do not select an artifact merely because it is newest. Always record and verify its exact NEVRA, byte size, payload and SHA-256.

## Accepted public baseline

~~~text
Run ID: 20260830T024809Z
Source manifest SHA-256: 7585b5755902fd746869ed3efe7ca7e15804cda728cef9ac7fdedffaa4abc774
NEVRA: zzmusic-1.0.0-25.bonito.dev.20260830t024809z.7585b5755902.aarch64
Bytes: 451843
SHA-256: 3f0ba8356b2d53097bee308ecd31021c38c86d97aab3115c04c3b26f3011d3ef
~~~

Only one clean build was used for this accepted device candidate; this release does not claim bit-for-bit deterministic reproduction.
