---
title: "Bulk testing of catalogue candidates"
status: specification
created: 2026-09-29
audience: whoever runs the test machines that grow the catalogue
---

# Bulk testing of catalogue candidates

## Goal

Take the candidates in `index/candidates/` (about 150 unreviewed x86_64
packages imported from AROS Archives) through the same test every published
package passed: **installed, started and removed on AROS**. Each candidate
ends in one of three states, with evidence:

| result | what happens |
|---|---|
| **approved** | manifest moves to arospkg-index `manifests/`, the index is regenerated and pushed |
| **excluded** | the candidate gets `excluded = "<reason>"`, stays here, and is not tried again |
| **needs a person** | nothing is published; the reason is recorded for review |

The bar does not change. "Started" means the program opened its window or
screen, or printed its output, and was still running (or finished normally)
at the check. It is not a claim that every feature works. See
`docs/guide/packaging.md`, sections 5 and 6B, for the manual version of
this procedure.

## Inputs

- `arospkg` and `arospkg-index` checked out side by side, both on `main`.
- The `apkg` binary from the 0.3 release archive
  (`apkg` SHA-256 `40c2ba9350bc10e6da294d0cf0675aefb8f3dc86c519fd8c992bb4bf675609e0`).
  Use exactly this binary, and record its hash in every report.
- AROS One 1.3 x86_64 (ABIv11) guests, each started from a fresh copy of
  the same base image, with AROSTCP up and `ENV:SYS/Certificates/ca-bundle.crt`
  present. Six machines can run in parallel; each works on its own batch.
- Network access to AROS Archives from the guests.

## The test machines (Linux host, x86_64)

A Fedora host with 64 GB of RAM runs six guests of 2 GB each with room to
spare. Packages: `qemu-system-x86`, `qemu-img`, `python3`, `lhasa`, `git`.

Each guest is a copy-on-write overlay of one AROS One 1.3 base disk, so
every batch starts from the same clean system:

```
qemu-img create -f qcow2 -F qcow2 -b /srv/aros/aros-one-1.3.qcow2 guest-N.qcow2
```

The command line used for all the published tests (TCG on macOS):

```
qemu-system-x86_64 -name aros-N -machine pc,accel=tcg -cpu qemu64 -smp 2 -m 2048 \
  -hda guest-N.qcow2 \
  -vga std -global VGA.vgamem_mb=64 \
  -netdev user,id=net0 -device e1000,netdev=net0 \
  -usb -device usb-tablet -display none -rtc base=localtime \
  -serial file:serial-N.log \
  -monitor unix:monitor-N.sock,server=on,wait=off \
  -qmp unix:qmp-N.sock,server=on,wait=off \
  -drive file=results-N.img,format=raw,if=ide,index=1 \
  -drive file=input-N.iso,format=raw,if=ide,index=2,media=cdrom,readonly=on -boot c
```

- **KVM** (`-machine pc,accel=kvm -cpu host`) should be much faster, but AROS
  One under KVM has not been tested for this project. Before the first batch,
  boot one guest with KVM and run the 27 published packages through stages
  2 and 3 as a control: they must all pass again. If any does not, use TCG.
- **Input**: a read-only ISO per batch holding `apkg`, `index.json` and the
  scripts (`mkisofs -J -R -o input-N.iso batchdir/`). Copy them to `RAM:`
  in the guest before use.
- **Results over the network** (preferred): in QEMU's user network the
  host is `10.0.2.2` from inside the guest. Run
  `python3 tests/tools/recv.py <dir> <port>` on the host, one port and one
  directory per guest, and send each log from the guest with
  `tests/tools/putfile` (the AROS binary; source `putfile.c`). Files arrive
  while the guest runs, so a crash does not lose what was already sent. Before
  starting a receiver, check that nothing else listens on its port: a leftover
  receiver once took a run's files.
- **Results on a disk** (fallback): a small FAT image per guest
  (`results-N.img`). Read it on the host only while that guest is stopped.
- No project-specific VM tools are needed: everything goes through the QMP
  or monitor socket.
- **Driving**: keys and screenshots go through QMP (`send-key`,
  `screendump`) or the monitor socket. `usb-tablet` is required for the
  pointer. Wait for Workbench before typing, and press Return if the GRUB
  menu waits.
- **Stopping**: after `quit`, wait until the QEMU process has actually gone
  before touching the disk or the results image.

## Stage 1: on the host, no guest needed

For each candidate (`index/candidates/*.toml` with `status = "skeleton"` and
no `excluded`):

1. **Download** with `tools/fetch.py <id>`. A size mismatch with the
   catalogue excludes nothing: record it as *needs a person*.
2. **Hash**: fill `size` and `sha256` from the downloaded file.
3. **ABI**: from the file name. `-v11` means `abi = "v11"`. No marker means
   ABIv1: exclude with `"ABIv1 build; the released client is ABIv11"`.
   `aarch64` candidates: exclude with `"aarch64; not tested"` for now.
4. **Layout**: list the archive (ZIP with Python's `zipfile`, LHA with
   `lha l`). Find:
   - `subdir`: the single top-level drawer. If there are several, or files
     loose at the top level beside a drawer, record *needs a person*.
   - `icon`: the `.info` beside that drawer at the top level, if any.
   Exclude with the reason when:
   - the archive installs into `LIBS:`, `C:`, `Fonts:`, `Devs:` or `SYS:`
     according to its ReadMe or an Installer script
     (`"needs files outside its drawer"`);
   - it is a development package: headers, static libraries, an SDK
     (`"development files, not a program"`);
   - it is a newer upload of an id that is already approved (update the
     approved manifest instead, as a separate review).
5. **Requirements**: run `tools/deps.py` on the unpacked drawer. Keep only
   libraries that are not in every AROS; the AROS One 1.3 lists are in
   `docs/spikes/twenty/`. Write them as `[[requires_system]]`. Set
   `depends_checked = true` and `depends = []`.
6. **Fix `id`** if the imported one carries a version
   (`python-2.7.18`, `scummvm-2026.3.0`): the id is the program's name.
   When two candidates are the same program, keep the newer and exclude the
   other with `"older upload of <id>"`.
7. **Validate**: `python3 tools/mkindex.py --manifests <dir with only this
   manifest, status set to approved> --cache .cache/archives --out /tmp/x.json`.
   It must publish it (exit 0, `published 1`). A refusal is fixed or recorded;
   never loosen `mkindex.py`.
8. Write the candidate into the batch's **test index** with
   `status = "approved"` in a scratch copy only (the real file stays a
   skeleton until stage 3).

Batch size: 25 to 30 packages per machine. Keep large packages (over 20 MB)
in batches of their own; the client refuses downloads over 64 MiB.

## Stage 2: on a guest, one batch per machine

Stage `apkg`, the batch's `index.json` and the scripts. From a fresh boot,
in a Shell (default stack is fine for `apkg`):

**A. Install** every package:

```
apkg --root SYS:Batch --index RAM:index.json install <id> >>RAM:a.txt
```

and for each one also `apkg --root SYS:Batch info <id>`. A refused install
records the refusal line.

**B. Start** each installed package. The installed drawer is
`SYS:Batch/<id>`, named by the id, not by the archive's drawer. Use the
method of `tests/trial-b.script`:

- windowed programs: `CD` into the drawer, `Run >NIL: <NIL: <program>`,
  `Wait 15`, record `Status COMMAND=<program>`, take a **screenshot**,
  then `Break <n> C` and `Status` again;
- Shell tools: run with input from `NIL:` and a usage argument, output and
  return code to the log;
- programs whose ReadMe asks for a bigger stack: `Stack <n>` first, as the
  ReadMe says, and write that into the manifest comment;
- after a program that opened its own screen, bring Workbench back
  (Amiga+N) and open a new Shell (Amiga+W) before the next one.

Nothing is typed into a program. The screenshots are taken from the host.

**C. Remove** every package with `apkg --root SYS:Batch remove <id>`, then
`List SYS:Batch ALL` and `apkg --root SYS:Batch list` (must say
`nothing installed`).

Rules that avoid wasted runs (each one cost a run before):

- One driver per machine. Two scripts typing into one guest spoil both.
- Prove the network after boot (a download of a known small file) before
  stage A. A boot without network is restarted, not recorded as failures.
- A crash (Software Failure requester) ends the batch on that machine:
  screenshot it, power-cycle, record the package as **crashed**, and rerun
  the rest of the batch from a fresh boot. Never measure anything after a
  crash in the same boot.
- Every report names the machine, the base image, the `apkg` hash, the
  batch and the time.

## Stage 3: judging and publishing

Per package, from the logs and screenshots:

| evidence | result |
|---|---|
| installed; alive after 15 s with a window or screen on the shot, or a Shell tool printed its usage/output and returned; removed; `nothing installed` | **approved** |
| install refused because a requirement is missing on AROS One | **excluded**: `"needs <library>, not on AROS One 1.3"` |
| crashed at start, or exited at once with an error about the system | **excluded** with what was seen, unless it looks like a test mistake (then *needs a person*) |
| needs data the user must supply (game files), and shows its own message saying so | **approved** only if that message is the program running normally; say so in the manifest comment |
| anything else, or unsure | **needs a person** |

For each **approved** package:

1. Move the manifest from `arospkg/index/candidates/` to
   `arospkg-index/manifests/`, `status = "approved"`, with a comment at the
   top naming the date, the base image, the `apkg` hash, what was started
   and how, and where the evidence is.
2. `installs_on = ["aros-one"]`, `runs_on = ["aros-one"]`,
   `does_not_run_on = ["mainline-x86_64"]` (ABIv11 builds).
3. Put the batch report and the screenshots under
   `arospkg-index/reports/<date>-batch-<n>/`: one README with the table of
   results, the console logs, the screenshots, and the `apkg` hash.

Then, in `arospkg`: `python3 tools/mkindex.py`. It writes
`../arospkg-index/index.json` and must exit 0. If it exits 1, nothing is
published until the reason is fixed. Commit and push arospkg-index (manifests,
index, report). Commit the candidates that moved or were excluded in
arospkg.

## Rules for the text that gets published

- Commit as the project's configured Git identity; check author and
  committer before pushing.
- No `Co-Authored-By` or similar trailers, and no mention of tools used to
  write the text.
- No em dashes in commit messages, manifests or reports.
- Strings in manifests: printable ASCII only, no `"` or `\` (the client does
  not decode JSON escapes; `mkindex.py` refuses them).
- Claims only for what was done: never `runs_on` without a start on that
  system.

## What this does not cover

- Packages that need files outside their drawer. They stay excluded until
  the client supports system components.
- ABIv1 (mainline) and aarch64 packages.
- Checking every feature of a program. The bar is "installs, starts, removes".
