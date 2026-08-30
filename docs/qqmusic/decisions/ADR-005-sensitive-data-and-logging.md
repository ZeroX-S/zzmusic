# ADR-005: Sensitive Data And Logging

Status: Accepted, 2026-08-23

## Decision

Logs and test artifacts may contain run/attempt IDs, states, counts, booleans,
dimensions, generation numbers, reviewed hosts, Cookie names/domains/paths and
stable diagnostic codes.

They must not contain Cookie values or headers, complete URLs or queries,
playback URLs, account identifiers/nicknames, raw responses, or Secrets
payloads.

Source secret gates print only `secret_scan=pass`, or `path|rule_id` on
failure. Account screenshots remain local, untracked, and unavailable to remote
image analysis.
