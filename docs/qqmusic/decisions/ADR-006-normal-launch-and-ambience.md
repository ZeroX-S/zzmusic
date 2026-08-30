# ADR-006: Normal Launch And Ambience

Status: Accepted, 2026-08-23

## Decision

Production presentation is defined by desktop/Sailjail/invoker activation of
the Qt 5 Silica `ApplicationWindow`. That path owns Ambience, foreground, Cover,
and lifecycle behavior.

A manually reconstructed shell environment is diagnostic only. Its observed
opaque white background does not authorize a QML background workaround unless
the same failure reproduces through normal desktop activation.

Every device gesture requires a separate foreground assertion. Deployment and
launch begin only after source, test, package, identity, and checksum gates pass.
