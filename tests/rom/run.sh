#!/bin/sh
# Builds and runs the FIFO round trip tests under a DS emulator.
#
#   tests/rom/run.sh              build the ROM and run it
#   tests/rom/run.sh --no-build   run the ROM that is already there
#
# Everything happens in containers: the BlocksDS image to build the ROM, and a
# plain Ubuntu image with DeSmuME to run it. Set TEST_ROM_EMU_IMAGE to use your
# own runner image, or SECONDS_TIMEOUT to allow a slower machine more time.
#
# How results get out
# -------------------
# DeSmuME has no headless mode and no way to print from inside the ROM:
# nocashMessage() is not forwarded to stdout, and writes to an emulated
# filesystem are never flushed back to the host - both checked. What it does
# print is a warning for every write to an undefined I/O register, so the ROM
# emits one byte per write to an unused register and this script reassembles
# them. See tests/rom/include/dsprobe.h.
#
# The emulator never exits on its own, so it is killed on a timeout. That means
# a hang and a pass look the same from the outside, which is why the ROM prints
# a "DONE" line and this script treats its absence as a failure.

set -eu

HERE="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PROJECT="$(CDPATH= cd -- "$HERE/../.." && pwd)"
ROM_REL="tests/rom/fifotest.nds"

BLOCKSDS_IMAGE="${BLOCKSDS_IMAGE:-skylyrac/blocksds:slim-latest}"
EMU_IMAGE="${TEST_ROM_EMU_IMAGE:-ubuntu:24.04}"
SECONDS_TIMEOUT="${SECONDS_TIMEOUT:-300}"

if [ "${1-}" != "--no-build" ]; then
	echo "== building $ROM_REL"
	docker run --rm \
		-u "$(id -u):$(id -g)" \
		-v "$PROJECT:/project" \
		-w /project \
		-e HOME=/tmp \
		"$BLOCKSDS_IMAGE" \
		make -C tests/rom BLOCKSDS=/opt/wonderful/thirdparty/blocksds/core
fi

if [ ! -f "$PROJECT/$ROM_REL" ]; then
	echo "no ROM at $ROM_REL" >&2
	exit 1
fi

echo "== running under DeSmuME"

# The emulator's own chatter is heavy, so only the undefined-register writes to
# the probe port are kept. Each carries one byte in the low half; awk turns the
# stream back into text.
RAW="$(docker run --rm \
	-v "$PROJECT:/project" \
	-w /project \
	"$EMU_IMAGE" \
	sh -c "
		apt-get update >/dev/null 2>&1
		DEBIAN_FRONTEND=noninteractive apt-get install -y desmume xvfb >/dev/null 2>&1
		timeout ${SECONDS_TIMEOUT} xvfb-run -a stdbuf -oL -eL /usr/games/desmume-cli \
			--disable-sound --disable-limiter \
			$ROM_REL 2>&1 | grep --line-buffered '04000058h'
		true
	")" || true

OUTPUT="$(printf '%s\n' "$RAW" | sed -n 's/.*= 0000\([0-9A-Fa-f]*\)h.*/\1/p' | awk '
	{
		v = strtonum("0x" $0)
		if (v > 0 && v < 128) printf "%c", v
	}
')"

echo "----------------------------------------"
printf '%s' "$OUTPUT"
echo "----------------------------------------"

# Only a strictly formed STATUS line counts. A line damaged in transit does not
# parse, and is skipped rather than guessed at; if none survive, that is a
# failure and not a pass.
STATUS="$(printf '%s' "$OUTPUT" \
	| grep -E '^STATUS pass=[0-9]+ total=[0-9]+ checks=[0-9]+ failures=[0-9]+$' \
	| head -1 || true)"

if [ -z "$STATUS" ]; then
	echo "FAILED: no intact STATUS line from the ROM. It hung, it crashed, or" >&2
	echo "        the probe channel stopped working in this emulator build." >&2
	exit 1
fi

FAILCOUNT="${STATUS##*failures=}"

if [ "$FAILCOUNT" = "0" ]; then
	echo "PASSED: $STATUS"
else
	echo "FAILED: $STATUS" >&2
	exit 1
fi
