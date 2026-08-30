# Release notes

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
