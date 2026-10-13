# QQ Music Device Acceptance

Target: Google Pixel 3a XL (`bonito`), Sailfish OS 5.2.0.16

Candidate: use the exact NEVRA, path, byte size, and SHA-256 from the current
run evidence. Never substitute a historical fixed filename or the newest RPM.

## Preconditions

- [ ] Record run ID, device boot ID, OS release, connection transport, and
      foreground application.
- [ ] Record installed `harbour-qqmusic` identity or confirm it is absent.
- [ ] Confirm no `harbour-qqmusic` or browser-engine process is running.
- [ ] Confirm the candidate passed host G1, the complete manifest-verified VM
      Qt 5 suite, two clean deterministic builds, and both RPM verifiers.
- [ ] Verify the local candidate NEVRA, architecture, byte size, payload, and
      SHA-256 against the single build identity.
- [ ] Keep the last accepted RPM available for rollback.

## Install Transaction

- [ ] Query the official repository for any missing Sailfish Secrets dependency.
- [ ] Dry-run dependency installation and reject removals, downgrades, or
      unrelated package changes.
- [ ] Transfer the exact candidate and verify the same SHA-256 on the phone.
- [ ] Require the ordinary dependency-aware RPM transaction test to pass. Do not
      use `--nodeps`.
- [ ] Install the candidate and verify the exact installed NEVRA.
- [ ] Require `rpm -V harbour-qqmusic` to produce no differences.
- [ ] Confirm the package owns only its reviewed application files and contains
      one executable, `/usr/bin/harbour-qqmusic`.
- [ ] Confirm the installed ELF links only to reviewed Qt 5/Sailfish libraries.

## Launch And Lifecycle

- [ ] Launch as `defaultuser` through
      `/usr/bin/sailjail -p harbour-qqmusic.desktop /usr/bin/harbour-qqmusic`.
- [ ] Record the single app PID and `/proc/<pid>/stat` start time before input.
- [ ] Confirm QQ Music is foreground before every gesture.
- [ ] Background and foreground the app twice; require the same PID/start time
      and coherent playback/UI state.
- [ ] Close the app through the normal shell path and confirm no app process
      remains.

## Public Browsing

- [ ] Home recommendations, categories, playlists, and charts reach terminal
      content, empty, cached-offline, or error states without endless loading.
- [ ] Selecting another category replaces stale rows and preserves navigation.
- [ ] Chart detail shows the matching title, period, artwork, and tracks.
- [ ] Track and playlist searches preserve their query and append one page at a
      time without duplicate rows.
- [ ] Playlist detail matches the selected item and exposes no provider other
      than QQ Music.
- [ ] My Music shows a bounded signed-out state and the single-process QR sign-in
      action when no existing session is restored.

## Native QR Login

- [ ] Starting sign-in advances fresh `opening` and `qr` markers in the same app
      PID and exposes one bounded QR image with nonzero byte size and dimensions.
- [ ] The same attempt advances bounded `mqtt_connected` and `mqtt_subscribed`
      markers before accepting scan events; no HTTP long-poll request is used.
- [ ] QR presentation contains no browser/helper process and no Qt 6/WebEngine
      library mapping.
- [ ] Cancel is available only before candidate validation; cancelling advances
      the `cancelled` marker, removes the QR, and preserves any active session.
- [ ] An unscanned QR reaches the bounded `timeout` state and can start a fresh
      generation without reusing the old challenge.
- [ ] QR presentation waits for `mqtt_subscribed`; an unscanned reconnect hides
      the QR until the new subscription is acknowledged.
- [ ] A connection with no MQTT ping response reaches a bounded retry/failure;
      repeated brief successful reconnects do not reset the attempt retry limit.
- [ ] A scan just before QR expiry gets its full exchange deadline, and late
      packets after cancellation or candidate submission do not alter the UI.
- [ ] After a Secrets daemon restart/cold start, candidate storage can request
      system unlock and commits only after storage reports success.
- [ ] A user-authorized scan advances `scanned`, `validating`, and `success` in
      order, with a candidate-submitted marker and a SessionStore activation
      marker from the same attempt.
- [ ] Candidate validation or Secrets write failure reaches `error`, clears only
      attempt-owned data, and preserves the previously active session.
- [ ] After success, a normal app restart restores the same reviewed session;
      sign-out still removes only account state.
- [ ] QR and account screenshots stay private and local. Do not print, OCR, or
      remotely analyze their contents.

## Playback And Integration

- [ ] Starting a playable row updates the compact player, full player, cover,
      MPRIS metadata, and lock-screen metadata consistently.
- [ ] Opening the full player emits its sanitized `page_open` marker, exposes
      portrait artwork/lyrics and the landscape two-column layout, and keeps
      transport and queue actions reachable while artwork is still loading.
- [ ] Artwork loading advances a sanitized `artwork_state` marker, shows bounded
      loading feedback, decodes asynchronously, and produces no Lipstick
      unresponsive event while foreground input remains responsive.
- [ ] Lyrics advance through explicit loading, content, empty, and error states;
      each terminal state is bounded and leaves playback controls responsive.
- [ ] Play, pause, previous, next, seek, repeat, queue selection, remove, and
      clear each mutate state once.
- [ ] On Sailfish OS 5.1, pausing after measurable progress preserves both the
      MPRIS position and the visible duration for at least three seconds; resume
      continues from the preserved position rather than from the beginning.
- [ ] On Sailfish OS 5.2, the same pause/resume probe advances without a media
      reload or an application PID change.
- [ ] Lyrics advance with playback; translations appear only when available.
- [ ] Missing lyrics and unavailable tracks terminate without an infinite retry.
- [ ] Headset removal pauses playback and does not restart it on reconnect.
- [ ] MPRIS exposes one service while running and removes it after exit.
- [ ] `Seek` and `SetPosition` each emit one matching `Seeked` signal.
- [ ] Playback failures show sanitized text and never expose a playback URL.

## Settings And Session Boundary

- [ ] Queue-resume and cache-limit settings persist across a normal restart.
- [ ] Clear cache uses remorse, updates usage, and does not alter an existing
      Secrets session.
- [ ] If an existing session restores successfully, account playlists load
      independently and sign-out clears only account state.
- [ ] Expired-session removal is explicit and leaves public browsing usable.
- [ ] No password field, Cookie editor, account write, download, browser window,
      helper UI, or second login process appears.

## Layout And Language

- [ ] Normal pages fit portrait and both landscape orientations without clipped
      text, overlapping controls, or unreachable actions.
- [ ] English has no unintended Chinese labels.
- [ ] Simplified Chinese has no empty translations or unintended English labels.
- [ ] Offline, timeout, malformed response, rate-limit, and authentication
      expiry states have bounded retry or cleanup behavior.

## Evidence And Cleanup

- [ ] Use a unique device run ID and one bounded journal interval.
- [ ] For capture, force `SCREENSHOT_AUTO_DEPLOY_HELPERS=0`.
- [ ] Validate PNG signature, dimensions, byte size, and SHA-256 locally.
- [ ] Require a handling marker plus a state or output invariant for every
      interaction; a screenshot or injected gesture alone is not a pass.
- [ ] Re-run `rpm -V harbour-qqmusic` after testing.
- [ ] Stop only the test-owned app process and leave Lipstick, networking, SSH,
      and unrelated services running.
- [ ] Record a terminal `sailfish_test=ready|...`, `failed|...`, or `blocked|...`
      result with the exact NEVRA and run ID.

Never include Cookie values, complete URLs, playback URLs, raw service responses,
account identifiers, or Secrets payloads in logs or evidence records.
