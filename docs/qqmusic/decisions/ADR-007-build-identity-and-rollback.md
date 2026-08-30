# ADR-007: Build Identity And Rollback

Status: Accepted for M1, 2026-08-23

## Decision

`packaging/apps/harbour-qqmusic/version.json` is the sole tracked version input.
A staging transaction combines its release prefix with the UTC run ID and the
first 12 hexadecimal characters of the verified source-manifest SHA-256. The
result is the only allowed development release for that staged source.

The spec requires explicit `qqmusic_version` and `qqmusic_release` macros. The
builder and verifiers consume the generated `.qqmusic-build-identity.json` and
reject fixed-name fallbacks, `/media/psf` build roots, mismatched manifests,
unexpected RPM filenames, or outputs outside the run root.

The canonical root and SDK builder view must have identical source-manifest
hashes. The SDK view exists only to preserve `/root` traversal isolation; it is
not a second import source or a separately versioned candidate.

Two clean builds may share one NEVRA only when they use the same source
manifest, source-date epoch, and build identity. Reproducibility is claimed
only when their exact NEVRA, payload list, byte size, and SHA-256 match.

No M1 artifact is installed. A later device candidate must retain the last
accepted RPM and record an explicit rollback identity before deployment.
