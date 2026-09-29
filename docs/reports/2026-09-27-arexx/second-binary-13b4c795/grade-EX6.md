# ARexx examples run EX6

PkgManager `13b4c7954a242d71de21a508aa4987b626356dc729d47f347ef15ffa5a2fb849`.

| example | verdict | output |
|---|---|---|
| isinstalled-before | ok | `soliton is not installed` |
| install | ok | `job 1: installing soliton / soliton is installed` |
| isinstalled-after | ok | `soliton is installed, version 2.2` |
| isinstalled-unknown | ok | `cannot tell: NOTFOUND nosuch is neither in the catalogue nor installed` |
| show | ok | `PkgManager shows cls` |
| watch-cancel | ok | `job 2 running  / job 2 running download 0% / job 2 running download 0% / job 2 running download 0% / cancel requested / job 2 running download 0% / job 2 running download 0% / job 2 running download 0` |
| install-watch | ok | `job 3 running  / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract` |
| upgrade | ok | `job 4: confirm or cancel the upgrade of soliton in PkgManager / soliton upgraded` |

Everything holds.
