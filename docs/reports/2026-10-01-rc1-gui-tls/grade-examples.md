# ARexx examples run rc1g10010102e2

PkgManager `732ecd9b356c200e3436e5f3bf49240c30b5698050e9aea3c6464eed360070ff`.

| example | verdict | output |
|---|---|---|
| isinstalled-before | ok | `soliton is not installed` |
| install | ok | `job 1: installing soliton / soliton is installed` |
| isinstalled-after | ok | `soliton is installed, version 2.2` |
| isinstalled-unknown | ok | `cannot tell: NOTFOUND nosuch is neither in the catalogue nor installed` |
| show | ok | `PkgManager shows cls` |
| watch-cancel | ok | `job 2 running  / job 2 running download 0% / job 2 running download 0% / job 2 running download 1% / cancel requested / job 2 running download 2% / job 2 running download 2% / job 2 running download 2` |
| install-watch | ok | `job 3 running  / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract 0% / job 3 running extract` |
| upgrade | ok | `job 4: confirm or cancel the upgrade of soliton in PkgManager / soliton upgraded` |

Everything holds.
