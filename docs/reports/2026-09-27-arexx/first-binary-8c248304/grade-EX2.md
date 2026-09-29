# ARexx examples run EX2

PkgManager `8c248304e439036b2aaec7c1f7a10a6c2f007aedd87fac3271efc5f5bfdbe7ae`.

| example | verdict | output |
|---|---|---|
| isinstalled-before | ok | `soliton is not installed` |
| install | ok | `job 1: installing soliton / soliton is installed` |
| isinstalled-after | ok | `soliton is installed, version 2.2` |
| isinstalled-unknown | ok | `cannot tell: NOTFOUND nosuch is neither in the catalogue nor installed` |
| show | ok | `PkgManager shows cls` |
| watch-cancel | ok | `job 2 running  / job 2 running download 0% / job 2 running download 0% / job 2 running download 0% / cancel requested / job 2 cancelled download 1% / outcome: cancelled CANCELLED` |
| install-watch | ok | `job 3 running  / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract` |
| upgrade | ok | `job 4: confirm or cancel the upgrade of soliton in PkgManager / soliton upgraded` |

Everything holds.
