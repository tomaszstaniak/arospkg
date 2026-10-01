# rc1 network acceptance, run rc1g10010102n

- published PkgManager `732ecd9b356c200e3436e5f3bf49240c30b5698050e9aea3c6464eed360070ff`
- fault-injection PkgManager-entropyfail `f81b0e9c1ea3b622d3c1b5cef3fbe9448755b027f446c0d72c39bfcac9a4bba5` (its results are not the release binary's)
- apkg `e4583d6aeb20af0de62b96827cc5c9c43984f4abf5519dbcc7eb12283f2b5aae`

| phase | verdict | pass | fail | what it shows |
|---|---|---|---|---|
| update | PASS | 9 | 0 | Update from the window's worker: the real catalogue over HTTPS |
| archives | PASS | 9 | 0 | install gmore from AROS Archives on an empty cache |
| github | PASS | 9 | 0 | install micropolis from GitHub Releases on an empty cache |
| quitnet | PASS | 2 | 0 | close the window |
| gonenet | PASS | 3 | 0 | window and lock gone |
| tls | PASS | 13 | 0 | expired certificate refused; nothing installed or left behind |
| recover | PASS | 9 | 0 | the next install after the refusal succeeds |
| quittls | PASS | 2 | 0 | close the window |
| gonetls | PASS | 3 | 0 | window and lock gone |
| entfail | PASS | 11 | 0 | forced entropy failure: Update refused, nothing written, window responsive |
| quitent | PASS | 2 | 0 | close the window |
| goneent | PASS | 3 | 0 | window and lock gone |

Every phase passed.
