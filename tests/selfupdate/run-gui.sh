#!/bin/sh
# The window side of the self-update tests, on a reserved ABIv11 pool
# machine already booted with the stage of make-stage.py and past
# setup.script (see README.md here):
#
#   tests/selfupdate/run-gui.sh RUNID SHOTDIR RECVDIR
#
# Drives PkgManager by coordinates (its main window at 100,42, About in
# the middle of the screen, 1024x768), takes the screenshots and waits for
# each check script's results to reach RECVDIR (tests/tools/recv.py).
set -e
R=$1; SHOTS=$2; RECV=$3
[ -n "$RECV" ] || { echo "usage: $0 RUNID SHOTDIR RECVDIR"; exit 2; }
mkdir -p "$SHOTS"
. "$(dirname "$0")/../arexx/pool-lib.sh"
trap - EXIT; rmdir "$LOCK"
vmenv
wait_for() { i=0; while [ ! -f "$RECV/$R-$1.txt" ] && [ $i -lt 120 ]; do sleep 5; i=$((i+5)); done; echo "$1: $(grep -c PASS "$RECV/$R-$1.txt") PASS, $(grep -c FAIL "$RECV/$R-$1.txt") FAIL"; }
about() { c 829 76; sleep 3; }
check() { c 437 493; sleep 15; }
update() { c 565 493; }
close_about() { c 639 493; sleep 1; }
close_window() { c 112 52; sleep 8; }

# The network first: AROSTCP comes up a while after setup.script starts it.
for try in 1 2 3 4 5 6 7 8 9 10; do
  t "RAM:rxt/putfile RAM:su-setup-done $R-net.txt" 8
  [ -f "$RECV/$R-net.txt" ] && break
  sleep 15
done
[ -f "$RECV/$R-net.txt" ] || { echo "the guest has no network"; exit 1; }

# G1: drawer unpacked by hand, started in its drawer from a Shell; C:apkg asked about.
t "Execute RAM:rxt/gui-g1.script" 30
about; check; shot g1-1-available
update; sleep 12; shot g1-2-c-apkg-question
c 349 424; sleep 20; shot g1-3-progress
sleep 40; shot g1-4-confirm
c 380 375; sleep 50; shot g1-5-restarted-result
c 499 397; sleep 2; about; shot g1-6-about-after; close_about
t "Execute RAM:rxt/gui-g1-check.script" 15; wait_for g1

# G2: a second window from the same installation refuses; then a cancel during the download.
t "Execute RAM:rxt/gui-g2.script" 30
about; check; update; sleep 4; shot g2-0-two-windows-refused
close_about; close_window; close_window
t "Execute RAM:rxt/gui-g2.script" 30
about; check; update; sleep 25; shot g2-1-downloading
c 859 611; sleep 6; shot g2-2-cancelled; close_about; close_window
t "Execute RAM:rxt/gui-g2-check.script" 15; wait_for g2

# G3: the package arospkg, started from its icon, restarted from it.
t "Execute RAM:rxt/gui-g3.script" 60
about; check; shot g3-1-available
update; sleep 45; shot g3-2-confirm
c 380 375; sleep 50; shot g3-3-restarted-result
c 499 397; sleep 2; close_window
t "Execute RAM:rxt/gui-g3-check.script" 25; wait_for g3

# G4: an update stopped between apkg and PkgManager; Repair now at the start.
t "Execute RAM:rxt/gui-g4.script" 60; shot g4-1-repair-offer
c 388 403; sleep 45; shot g4-2-repaired
c 499 397; sleep 2; close_window
t "Execute RAM:rxt/gui-g4-check.script" 20; wait_for g4
