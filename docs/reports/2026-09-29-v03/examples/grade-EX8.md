# ARexx examples run EX8

PkgManager `8345cec7d65a7fb9889974cd7a92d1cbbc58936f217c21f668fadc900d2d594b`.

| example | verdict | output |
|---|---|---|
| isinstalled-before | ok | `soliton is not installed` |
| install | ok | `job 1: installing soliton / soliton is installed` |
| isinstalled-after | ok | `soliton is installed, version 2.2` |
| isinstalled-unknown | ok | `cannot tell: NOTFOUND nosuch is neither in the catalogue nor installed` |
| show | ok | `PkgManager shows cls` |
| watch-cancel | ok | `job 2 running  / job 2 running download 0% / job 2 running download 0% / job 2 running download 1% / cancel requested / job 2 running download 1% / job 2 running download 1% / job 2 running download 1` |
| install-watch | ok | `job 3 running  / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract` |
| upgrade | ok | `job 4: confirm or cancel the upgrade of soliton in PkgManager / soliton upgraded` |

Everything holds.
