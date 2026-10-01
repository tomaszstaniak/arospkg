# ARexx acceptance run rc1d1001A

PkgManager `732ecd9b356c200e3436e5f3bf49240c30b5698050e9aea3c6464eed360070ff`, apkg `e4583d6aeb20af0de62b96827cc5c9c43984f4abf5519dbcc7eb12283f2b5aae`.

| phase | pass | fail | |
|---|---:|---:|---|
| env | 4 | 0 | ok |
| parse | 17 | 0 | ok |
| stress | 4 | 0 | ok |
| queries0 | 16 | 0 | ok |
| show | 3 | 0 | ok |
| installstart | 11 | 0 | ok |
| installcheck | 13 | 0 | ok |
| remove | 12 | 0 | ok |
| rapid | 4 | 0 | ok |
| spin | 7 | 0 | ok |
| select | 1 | 0 | ok |
| guibusy | 9 | 0 | ok |
| clilock | 7 | 0 | ok |
| upgradestart | 15 | 0 | ok |
| upgradecheck | 6 | 0 | ok |
| rollbackstart | 2 | 0 | ok |
| rollbackcheck | 5 | 0 | ok |
| cancel | 12 | 0 | ok |
| quitstart | 5 | 0 | ok |
| gone | 3 | 0 | ok |
| collide | 2 | 0 | ok |
| closestart | 2 | 1 | failed |
| gone2 | 1 | 2 | failed |
| norexx | 2 | 0 | ok |
| nomem | 11 | 0 | ok |
| nomem2 | 8 | 0 | ok |
| update | 9 | 0 | ok |
| waitjob- | 0 | 1 | failed |
| waitjob-20 | 2 | 0 | ok |
| waitjob-24 | 2 | 0 | ok |

Guest checks: 191 passed, 3 failed. Host checks on the event logs and the final state: 26 of 29 hold.

Problems:

- closestart: FAIL install-before-close : sent [INSTALL cls] wanted OK got NOPORT [NOPORT PKGMANAGER is not there]
- gone2: FAIL wait-got-final-before-quit : 
- gone2: FAIL job-finished-despite-quit : soliton
- rc1d1001A-waitjob-.txt: no final state reported
- rc1d1001A-waitjob-.txt: answered by nothing
- pm4: the window still works
- pm4: and removed cls
- pm2: the install finished before the window quit
