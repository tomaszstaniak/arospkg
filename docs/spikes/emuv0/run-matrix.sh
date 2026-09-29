#!/bin/sh
# The EmuV0 matrix: each case from a fresh boot, because a failing case
# can leave the machine unable to take commands.
#
#   docs/spikes/emuv0/run-matrix.sh STAGEDIR RESULTSDIR SHOTDIR CASE...
#
# STAGEDIR holds matrix/*.script, ev/ (the programs) and emu112/ (the 1.12
# archive, unpacked). A case whose program is a window gets 40 s, then a
# screenshot, then "alive": a file written by the Shell -- if the machine
# no longer takes commands, that file is missing.
set -e
STAGE=$1; OUT=$2; SHOTS=$3; shift 3
mkdir -p "$SHOTS"
. "$(cd "$(dirname "$0")/../../.." && pwd)/tests/arexx/pool-lib.sh"
for R in "$@"; do
  boot_staged "$STAGE"
  t "Copy RAM:rxt/ev RAM:ev ALL QUIET" 8
  t "Execute RAM:rxt/case-$R.script" 20
  sleep 40; shot $R
  t "Echo >RESULTS:$R-alive.txt alive" 5
  t "Status >>RESULTS:$R-alive.txt" 5
  shot $R-after
  ./vm.sh stop $M >/dev/null 2>&1 || true
  stopped
  echo "case $R done $(date +%T)"
done
./vm.sh collect $M "$OUT" >/dev/null
echo "collected $OUT"
