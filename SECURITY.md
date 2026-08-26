# Security Policy

## Supported versions

Only the latest release is supported.

## Reporting a vulnerability

This project is a small community mod for an abandoned game. If you find a
security problem in the source code or in the released DLL, open a private
advisory or contact the maintainer through GitHub.

Please include:

- the file and function involved
- what the code does wrong
- a proof of concept if you have one

The repository is open source and the DLL is built by CI from the source on
every push. You can rebuild it yourself and compare against the release
artifact (compare hashes from the CI artifacts).

## Scope

The fix redirects only connections to `110.232.115.186:80` to a local
loopback server. It does not intercept any other traffic. See the test
harness (`test_harness.c`) for the exact behaviour verified by CI.