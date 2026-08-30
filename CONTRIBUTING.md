# Contributing

Contributions should preserve the product boundary: Qt 5, Sailfish Silica, Sailjail-native, QQ Music-only and single-process QR login.

Before opening a pull request:

1. Keep unrelated changes out of the patch.
2. Add or update a focused Qt test for behavioral changes.
3. Run shell syntax checks and the relevant host/VM tests.
4. Do not commit RPMs, build output, device screenshots, logs, account data, Cookies, QR payloads or Secrets data.
5. Document the exact Sailfish target used for device-only behavior.

Use issues for non-sensitive bugs and GitHub Security Advisories for anything involving credentials, session storage or privacy.
