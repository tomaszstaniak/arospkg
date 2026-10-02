---
title: "Hide the long redirect URL unless asked"
status: noted, cosmetic (2026-10-02)
---

# Hide the long redirect URL unless asked

`install` prints `  redirect -> <url>` for every hop. GitHub's release
downloads redirect to a signed URL of several hundred characters, which
fills a narrow window before the progress line appears (seen in the joint
presentation test, `docs/reports/2026-10-02-presentation-joint/`). Print the
redirect's host only, and the full URL with a verbose option. Redirected
output changes with it, so the test suites' expectations need the same
change.
