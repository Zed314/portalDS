#!/bin/sh
# Builds the FRAME_PROFILING ROM and boots it under a DS emulator, headless.
#
#   tests/profile/run.sh              build the profiling ROM and run it
#   tests/profile/run.sh --no-build   run the portalDS.nds that is already there
#
# The profiling build boots straight into the first chamber and places the
# portal pair by itself (see profilerAutoShoot in game.c), so a successful run
# proves the whole chain: boot, filesystem, level load, physics settling,
# portal placement, and the portal render pipeline actually producing visible
# portals. That last step is the pass condition - the profiler reports portal
# used/seen flags every ~4 seconds of game time, and this script fails unless
# it sees reports with both portals placed AND both being rendered.
#
# The frame profile itself is printed for the log (and the GitHub job summary
# when GITHUB_STEP_SUMMARY is set) but deliberately not asserted on: emulator
# timing is approximate and runner speed varies, so numbers are information,
# not a gate.
#
# Results get out the same way tests/rom/run.sh gets its results out: DeSmuME
# prints a warning for every write to an undefined I/O register, value
# included, and the profiler doubles its reports as tagged u32 writes to one
# such address (see PROFILER_MMIO_SINK in profiler.c). The emulator never
# exits on its own and is killed on the timeout; a run that produced no
# reports at all is a failure, not a pass.
#
# NOTE: the build step runs "clean" first - DEFINES changes do not invalidate
# the incremental build - so a local run throws away your build cache.

set -eu

HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PROJECT="$(CDPATH= cd -- "$HERE/../.." && pwd)"

EMU_IMAGE="${TEST_ROM_EMU_IMAGE:-ubuntu:24.04}"
SECONDS_TIMEOUT="${SECONDS_TIMEOUT:-360}"

if [ "${1-}" != "--no-build" ]; then
	echo "== building the profiling ROM (clean first: DEFINES changes are not tracked)"
	"$PROJECT/docker-build.sh" clean
	"$PROJECT/docker-build.sh" 'DEFINES=-DPICOLIBC_LONG_LONG_PRINTF_SCANF -DFRAME_PROFILING'
fi

if [ ! -f "$PROJECT/portalDS.nds" ]; then
	echo "no ROM at portalDS.nds" >&2
	exit 1
fi

echo "== running under DeSmuME for up to ${SECONDS_TIMEOUT}s"

LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

# Only the profiler sink writes are kept; the emulator's other chatter is
# heavy and irrelevant here.
docker run --rm \
	-v "$PROJECT:/project" \
	-w /project \
	"$EMU_IMAGE" \
	sh -c "
		apt-get update >/dev/null 2>&1
		DEBIAN_FRONTEND=noninteractive apt-get install -y desmume xvfb >/dev/null 2>&1
		timeout ${SECONDS_TIMEOUT} xvfb-run -a stdbuf -oL -eL /usr/games/desmume-cli \
			--disable-sound --disable-limiter \
			portalDS.nds 2>&1 | grep --line-buffered '04FFFC00h'
		true
	" > "$LOG" || true

python3 - "$LOG" <<'EOF'
import os, re, sys
from collections import defaultdict

CODES={0:"cpy",1:"pp",2:"upd",3:"sub",4:"phy",5:"busy",6:"max"}
FRAME=16715  # microseconds in one vblank period

# phase 0: no portals, 1: partially placed or unseen, 2: both placed and seen
phase=0
buckets=[defaultdict(list),defaultdict(list),defaultdict(list)]
state_words=0

for line in open(sys.argv[1]):
    m=re.search(r'04FFFC00h = ([0-9A-F]{8})h',line)
    if not m: continue
    v=int(m.group(1),16); tag=v>>24
    if tag==0xFF:
        if (v>>20)&0xF==0xB:  # portal state, emitted at the head of each report
            state_words+=1
            used=v&3; seen=(v>>2)&3
            phase=2 if (used==3 and seen==3) else (1 if used else 0)
        continue
    half=(tag>>4)&1; code=CODES.get(tag&0xF)
    if code: buckets[phase][(half,code)].append(v&0xFFFFFF)

both_seen=len(buckets[2][(0,'busy')])
print(f"profiler reports: {state_words} total, {both_seen} with both portals placed and rendering")

if state_words==0:
    print("FAILED: no profiler reports at all - the ROM did not boot or the "
          "side channel stopped working in this emulator build.",file=sys.stderr)
    sys.exit(1)
if both_seen<3:
    print("FAILED: the portal pair never came up: portals were not placed, or "
          "were placed but never rendered (render2 does no work for an unseen "
          "portal).",file=sys.stderr)
    sys.exit(1)

def table():
    rows=[]
    rows.append(("half","section","no portals","both portals seen"))
    for half in (0,1):
        hname="A (main scene)" if half==0 else "B (portal view)"
        for c in ["cpy","pp","upd","sub","phy","busy"]:
            cells=[]
            for b in (buckets[0],buckets[2]):
                vals=b.get((half,c))
                if vals and len(vals)>1: vals=vals[1:]  # first window straddles the transition
                cells.append(f"{sum(vals)/len(vals):.0f}us" if vals else "-")
            if cells==["-","-"]: continue
            if c=="busy":
                cells=[f"{cells[i]} ({sum(b[(half,'busy')][1:] or b[(half,'busy')])/max(1,len(b[(half,'busy')][1:] or b[(half,'busy')]))*100/FRAME:.0f}%)"
                       for i,b in enumerate((buckets[0],buckets[2]))]
            rows.append((hname,c,cells[0],cells[1]))
    return rows

rows=table()
w=[max(len(r[i]) for r in rows) for i in range(4)]
for r in rows:
    print("  ".join(r[i].ljust(w[i]) for i in range(4)))

summary=os.environ.get("GITHUB_STEP_SUMMARY")
if summary:
    with open(summary,"a") as f:
        f.write("## Portal pipeline frame profile (DeSmuME, indicative timing)\n\n")
        f.write("|"+"|".join(rows[0])+"|\n")
        f.write("|"+"|".join("-" for _ in rows[0])+"|\n")
        for r in rows[1:]:
            f.write("|"+"|".join(r)+"|\n")
        f.write(f"\n{both_seen} reports with both portals placed and rendering.\n")

print("PASSED: both portals placed and rendering")
EOF
