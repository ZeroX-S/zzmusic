# Native 1.0.0-27 validation record

Published as a development prerelease on 2026-10-13. Runtime, resource and test
files are copied from the verified candidate; local engineering instructions,
device logs, account evidence and recovery bundles are excluded.

| Item | Result |
| --- | --- |
| Target | SailfishOS-5.1.0.11-aarch64; Qt 5.6.3; GCC 13.4.0 |
| Complete SDK run | 20261013T104034Z; 25 C++ suites, 482 passed; QML, 44 passed |
| Final controller regression | 34 passed, including same-account and different-account credential renewal |
| QR state machine | 20 passed |
| Login / WebSocket / MQTT protocols | 11 / 11 / 12 passed |
| Session store | 17 passed |
| Live service | Three create/subscribe/cancel/restart cycles with Sailfish target libraries; no scan or stored credentials |
| Packaging | Dedicated and aggregate RPM checks passed; 41 payload entries; Qt 5 only |
| Source checks | Whitespace, shell syntax, screenshot-tool tests, Python 12/12 and secret scan passed |
| Device acceptance | Real scan, Secrets commit/restore and 5.1 hardware pause/resume pending |

The complete SDK run used source manifest
`6df0ed3ae8b1643e088f1a57d422fab916eac5073ffc0813dedea3693869ef31`.
The final run is `20261013T113645Z`, with source manifest
`8ef344bc6f8b476ad3a904095b227ef36496b879aaf6a6e60a9b44a19d858d94`.
Comparison found differences only in the application controller, its tests,
implementation notes and the parallel build option. All headers and other
application files were unchanged. Both changed C++ files were forced to rebuild
for the final 34-test controller run rather than relying on source mtimes.

The Release application-source manifest covers public application files, and
the build-identity attachment records the original integration commit, tested
manifest and public source commit. The public staging allowlist additionally
includes standalone repository documentation; rebuilding therefore creates a
new manifest and NEVRA.

The new behavior preserves the existing session until candidate validation and
Secrets storage succeed. Transient validation failures get one retry; account
rejection and invalid identity/schema still fail closed. Stale old-session
requests cannot expire a renewed session.

Only one clean build produced the published RPM. No deterministic-build or
new real-device login claim is made.

~~~text
NEVRA: zzmusic-1.0.0-27.bonito.dev.20261013t113645z.8ef344bc6f8b.aarch64
Bytes: 456540
SHA-256: 923c3d70a7ea1d3afb10503984343b001051509c134e7d1f74d36ebe3f3e8214
~~~
