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

After every release:

1. Check the VirusTotal analysis linked in the release notes. Defender may
   flag the new unsigned build with an ML heuristic (`!ml`); this is
   expected for a new unsigned file and usually fades.
2. If it is flagged, submit the release DLL to Microsoft as a false
   positive: https://www.microsoft.com/en-us/wdsi/filesubmission. State
   that the file is the CI build of this repository, and link the release.
   Turnaround is typically days.