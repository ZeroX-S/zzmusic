# ADR-008: Single-Process Qt 5 QR Login

Status: Accepted, 2026-08-24

## Context

The accepted device baseline is a single Qt 5/Silica process. Qt 6,
WebEngine, the login helper, local IPC, and helper packaging were removed to
keep the runtime aligned with the Sailfish phone libraries. Login is still
required, but restoring any browser/helper path would violate that boundary.

QQ Music's mobile-client login service exposes a fixed QR creation request and
an MQTT 5 subscription over WSS for `scanned`, candidate-credential,
cancellation, timeout, and failure events. The former HTTP long-poll endpoint
returns an incompatible root schema and is not part of the accepted protocol.
A final fixed login exchange converts the short-lived QR candidate into the
reviewed music-session fields.

## Decision

`harbour-qqmusic` performs QR login in its existing Qt 5 process with
`QNetworkAccessManager`, a fixed Qt5Network TLS/WebSocket transport, and an
application-owned bounded MQTT 5 codec. The target's Qt 5.5 WebSockets API
cannot declare the required `mqtt` subprotocol, so the bounded transport owns
only the fixed RFC 6455 client handshake and binary-frame subset required here.

- The implementation permits only compiled-in HTTPS/WSS hosts, paths, methods,
  MQTT packet/property types, request schemas, response sizes, and time limits.
- MQTT uses one attempt-owned connection, fixed CONNECT user properties, one
  exact QR topic, packet ID 1, QoS 0, bounded keepalive, and at most two
  same-host server-reference reconnects. Unknown or malformed frames fail
  closed without logging their payload.
- The UI receives only a validated in-memory PNG data URI plus a small state
  enum. It never receives a QR identifier, token, Cookie value, complete URL,
  or raw response.
- Each attempt has one non-secret `attempt_id` and the states `opening`, `qr`,
  `scanned`, `validating`, `success`, `cancelled`, `timeout`, and `error`.
- Cancellation is available before candidate validation starts. Once a
  candidate reaches `SessionStore`, the existing validation and Secrets write
  complete without cancellation.
- The active session remains unchanged until remote account validation and the
  Sailfish Secrets write both succeed. Failure clears only attempt-owned data.
- Native exchange emits exactly `uin`, `wxuin`, `qm_keyst`, and `qqmusic_key`.
  The broader seven-name `CookiePolicy` remains only for restoring compatible
  pre-existing sessions and is not an output boundary for native login.
- Logs contain state, attempt ID, dimensions, byte counts, generation, counts,
  booleans, and stable diagnostic codes. They never contain QR bytes or text,
  credentials, account identity, raw responses, complete URLs, or URL queries.
- No browser engine, Qt 6 library, child process, local IPC socket, or login
  profile is linked, launched, or packaged.

## Consequences

The QR protocol is now owned and tested as application code. Upstream schema
changes fail closed with a stable diagnostic rather than falling back to a
general web page. Device acceptance must prove QR ready, cancel, timeout, scan,
candidate validation, Secrets commit, restart restore, and zero browser/helper
processes. The QR is a QQ Music mobile-login challenge, but scanner-brand
claims remain server-defined until device events prove the accepted client.
Account screenshots remain private and local.
