# Security Policy

## Supported version

Security fixes are currently evaluated against the latest published **native-*** prerelease.

## Reporting a vulnerability

Please use GitHub's private **Report a vulnerability** flow under the repository Security tab. Do not open a public issue containing credentials or account evidence.

Never include:

- Cookie values or Cookie headers;
- account identifiers or nicknames;
- QR image bytes, decoded QR content or login tokens;
- complete URLs, URL queries, playback URLs or raw QQ responses;
- Sailfish Secrets payloads;
- private device screenshots or device logs containing account information.

A useful report contains the affected tag, Sailfish OS version, stable diagnostic code, reproducible non-secret steps and whether the issue occurs before or after session validation.

## Scope

The project accepts reports about in-process QR login, session storage, request scoping, parser robustness, playback authorization and Sailjail boundaries. Password login, Cookie editors, downloads, DRM circumvention, browser engines and secondary login processes are intentionally unsupported.
