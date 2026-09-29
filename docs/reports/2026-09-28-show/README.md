# `apkg show` on AROS One, 2026-09-28

*Superseded by `docs/reports/2026-09-29-v03/`. This is the run on the
development build: "runs here" became "compatibility", with the
requirements on a line of their own, and choosing between builds became
stricter before the freeze.*

Pool slot `v11-2` (AROS One 1.3, x86_64, ABIv11), QEMU TCG. Run SH3,
driven by `tests/show/run-show.sh`, decided by `tests/show/showtest.rexx`,
graded by `tests/show/grade-show.sh`: **37 checks, 0 failed.**

| binary | SHA-256 |
|---|---|
| apkg | `45d4d8620774bd9b8a7aa29378f1cb5d0fed6d40b3c4f7e8aeda1f7180fc8412` |
| PkgManager | `cd104736a23e3256be3e1d742d82e6c71a9932c060e2aa0a592d781ccf9d587e` |

`SH3-show-cli.txt` opens with `apkg --version`, which prints the binary's
own hash. `SH3-show-pm.log` opens with the window's. The files are as the
guest wrote them, less the carriage returns Regina's `lineout` adds on AROS.

## What the run covers

The index (`tests/show/make-show-fixtures.py`) holds:
- soliton, at revision 1 or 2, depending on the step;
- cls and xrick;
- `oldthing`, built for ABIv1;
- `armthing`, built for aarch64;
- `futurething`, built for an ABI this client does not know (i386/v0);
- `rescode` twice: its i386/v0 entry comes first, its x86_64/v11 entry second.

| case | checked |
|---|---|
| no index at all | `show` of an unknown package says there is no index and nothing installed |
| not installed | target, "runs here: native" with the reason, download size and URL, SHA-256, requirements probed on the machine, "installed: no"; no source line when the index gives none |
| unknown id | "no such package in the index, and none installed" |
| another ABI | "runs here: no -- built for ABIv1, mainline AROS; this machine runs ABIv11"; requirements not probed, since it cannot run |
| another CPU | "runs here: no -- built for aarch64; this machine is x86_64" |
| an ABI the client does not know | "undetermined", never native |
| two targets for one id | `show rescode` describes the x86_64/v11 entry, says which targets the index has, and probes that entry's requirements. `install rescode` installed that entry, as the registry shows, although the i386 one comes first |
| source and licence | printed when the index gives them |
| installed at another revision | the header is the index's revision; "installed: 2.2 (x86_64), in SYS:PkgShow/soliton, since ..."; "upgrade: available, 2.2 -> 2.2-aros2"; "roll back: no" with the reason |
| the index gone, the package installed | "index: none on this machine", the verdict from the registry, the installed line |
| `info` | unchanged: it still prints the registry entry |
| `search` | hides the four entries that are not for this machine, and says why |
| nothing changed | a listing of the whole root before and after the `show` calls is identical |
| ARexx `INFO ... FIELD runs` | `native`, `no`, `no`, `undetermined` for soliton, oldthing, armthing, futurething; rescode's `abi` is `v11` |
| the window | `sh3-panel.png`: the panel reads "runs here: native", from the same call |

## The ARexx run again

The window changed too: its panel shows the verdict, and ARexx `INFO` finds
packages the way `show` does. The whole ARexx acceptance run was repeated on
this PkgManager as AX8. It passed 194 guest checks with 0 failed, and 29 of
29 host checks on the window's logs (`grade-AX8-arexx-regression.md`,
`stage-ax8.sha256`).

## Found on the way

With two targets for one id in the index, `show` described the native entry
while the requirement probe, `install` and `upgrade` each took whichever came
first. Run SH1 showed rescode's native entry with an empty requirement list,
which was the i386 entry's. All four now choose through
`index_find_variant`, and SH2 and SH3 check both the requirements and the
installed entry.

## Not covered

- Nothing was run on mainline (ABIv1) or aarch64.
- There is no `--json` form of `show` yet.
- The v0.2 CLI suites were not re-run against this `apkg`. Its changes touch
  how install and upgrade pick an entry, so they belong in the 0.3
  regression before a release.
