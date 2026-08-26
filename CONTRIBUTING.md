# Contributing

## Ground rules

- Commits are GPG-signed and carry the Developer Certificate of Origin.
  Add `Signed-off-by: Your Name <your email>` to every commit message.
- Work on a short-lived branch from `main` and open a pull request.
- Never rewrite history on `main`.

## Before a pull request

- CI must pass: build and behavioural tests on Linux (Wine) and Windows
  (native), CodeQL, ClamAV scan, gitleaks secret scan.
- Every change needs a rationale. Link the change to the issue it fixes.
- If the change alters behaviour, update the test harness (`test_harness.c`)
  to cover it.

## Releasing

Releases are signed tags on `main`. The release DLL is built by CI from the
committed source; never upload a locally built binary.