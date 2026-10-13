# Release notes

## ZZ Music Native 1.0.0-27

Release date: 2026-10-13. AArch64 development prerelease.

QR login now waits for an acknowledged MQTT subscription before displaying a
code. Old-session failures cannot interrupt candidate validation or invalidate
the newly activated session, including when signing in to the same account again.

- Accept fragmented/coalesced MQTT packets and bounded publications before SUBACK.
- Enforce ping-response deadlines, broker keepalive settings and an attempt-wide reconnect limit.
- Ignore late messages after cancel or candidate submission; give an accepted scan its own exchange timeout.
- Accept valid HTTP 101 upgrade reason phrases while retaining required header and TLS checks.
- Retry transient account-validation failures once with a new operation ID.
- Permit system unlock during Secrets storage; activate only after validation and storage succeed.
- Invalidate old library/search requests when credentials are renewed for the same account.
- Preserve Sailfish 5.1 pause position/duration when the legacy multimedia backend resets; reload only when necessary, retaining the normal 5.2 resume path.
- Complete missing Simplified Chinese login diagnostics and add a bounded parallel RPM build option.

Validation: 25 SDK C++ suites (482 passes), QML (44 passes), final application
controller delta (34 passes), and three live QR creation/subscription/cancel
rounds with the Sailfish target libraries. Dedicated and aggregate RPM checks passed.
The complete suite used the preceding staged snapshot; hash comparison confirms
only the controller, its tests, documentation and build parallelism changed in
the final snapshot. The controller was explicitly rebuilt and retested.

Real account scanning, Secrets commit/restore and Sailfish 5.1 phone playback
acceptance remain pending for this build. It is not a hardware-validated stable release.

~~~text
zzmusic-1.0.0-27.bonito.dev.20261013t113645z.8ef344bc6f8b.aarch64.rpm
456540 bytes
SHA-256 923c3d70a7ea1d3afb10503984343b001051509c134e7d1f74d36ebe3f3e8214
~~~

See [the validation record](docs/releases/native-v1.0.0-27.md) for source identities and test details. Release assets include the RPM, application icons and checksums.

## ZZ Music Native 1.0.0-25

First public Sailfish OS AArch64 development release.

### Product

- Native Qt 5 / Sailfish Silica application with the public product name **ZZ Music**.
- Home recommendations, Daily 30, Guess You Like, discovery categories, top charts, search and My Music.
- Single-process QR login with remote candidate validation and Sailfish Secrets persistence.
- Playback queue, repeat-one, repeat-all and shuffle modes.
- MPRIS integration, multi-action Cover, lyrics, adaptive Cover contrast and bitrate display.
- Asynchronous artwork caching and staged page content to reduce UI stalls.

### Fixes included

- Repaired QR login visibility and candidate validation failures.
- Repaired inconsistent playlist and track search behavior.
- Decoupled discovery content from home recommendation failures.
- Improved page/pager transitions and reduced avoidable synchronous work.
- Preserved playback progress across pause/resume backend resets on Sailfish Qt Multimedia.

### Verification

- Qt 5 compatibility target: **SailfishOS-5.1.0.11-aarch64**.
- Device: AArch64 phone running Sailfish OS **5.2.0.16**.
- Exact RPM verification: payload **41**, Qt 5-only, no Qt 6/WebEngine, clean **rpm -V**.
- Device acceptance: QR login, Secrets commit, authenticated single-track search and pause/resume passed.
- Pause evidence: progress remained stable while paused and advanced after resume without a process restart.

### Artifact

~~~text
zzmusic-1.0.0-25.bonito.dev.20260830t024809z.7585b5755902.aarch64.rpm
451843 bytes
SHA-256 3f0ba8356b2d53097bee308ecd31021c38c86d97aab3115c04c3b26f3011d3ef
~~~

This is a prerelease because upstream QQ Music interfaces may change and broader hardware coverage is still in progress.
