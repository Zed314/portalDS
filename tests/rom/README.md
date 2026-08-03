FIFO round trip tests
=====================

    tests/rom/run.sh                # build the ROM and run it
    tests/rom/run.sh --no-build     # run the ROM that is already built

Everything runs in containers - the BlocksDS image to build, a plain Ubuntu
image with DeSmuME to run - so this needs nothing installed but Docker.

What this is for
----------------

Everything else under `tests/` runs on the host, which works because the code
it covers is pure logic. The one thing that cannot be reached that way is the
contract between the two processors: command encoding, argument order, reply
packing, and whether the ARM7 actually does what the ARM9 asked. Testing that
needs both CPUs running their own binaries over a real FIFO, in real time.

So this is a second ROM. The ARM7 half is the real, unmodified `arm7.elf` -
the shipped physics engine and the shipped command decoder in `PI7.c`. Only the
ARM9 half is test code, and it drives the protocol from the outside.

The driver deliberately re-implements the sender rather than linking `PI9.c`.
A test that shares its encoder with the decoder cannot notice the two drifting
apart, because both move together; writing the encoding out by hand from the
layout documented in `common/include/PIC.h` is what gives the test something
independent to compare against. See `include/fifo_protocol.h`.

This found a real bug on its first complete run: `PI_ADDBOX` read its last
argument word without waiting for it, so a box could be created with a zero
sine and silently face the wrong way. The test named "the last argument word
is waited for" is the regression test for it.

How it is built
---------------

`Makefile` builds a second ARM9 binary using the game's own `Makefile.arm9`,
pointed at this directory by command line variables. Variables given on the
make command line override the ones set inside a makefile, so nothing in the
existing build had to change to support a second binary.

How results get out
-------------------

DeSmuME has no headless mode, no frame limit, and no ordinary way for a ROM to
print anything to the host. Two obvious channels were tried and do not work:

- `nocashMessage()` is not forwarded to stdout in this build;
- writes to an emulated filesystem never reach the host. The ROM side succeeds
  all the way through `fatInitDefault`, `fopen`, `fprintf` and `fclose`; the
  file simply never appears.

What DeSmuME does print is a warning for every write to an undefined I/O
register, including the value written. So the ROM emits one byte per write to
an unused register and `run.sh` reassembles the stream into text. That is the
only emulator-specific thing in the ROM, and it lives in one header
(`include/dsprobe.h`) so a better channel can replace it without touching the
tests.

Two consequences worth knowing:

- **The emulator never exits.** It is killed on a timeout, so a hang and a pass
  would look identical from the outside. The ROM therefore prints a `STATUS`
  line at the end and `run.sh` treats its absence as a failure.
- **The last output can be damaged.** DeSmuME's stdout is block buffered, and
  the X server's shutdown message has been observed landing in the middle of a
  probe line at kill time. `run.sh` line-buffers the emulator to prevent it, the
  ROM prints `STATUS` twice, and the harness requires a strictly formed line -
  a damaged one is skipped, never guessed at, and if none survive that is a
  failure rather than a pass.

Limitations
-----------

Worth being honest about what a pass here is worth:

- DeSmuME 0.9.11 dates from 2015. FIFO timing is exactly the thing these tests
  lean on hardest and exactly the thing an emulator is least likely to match.
  Treat a pass as weaker evidence than a host test's.
- Timing-sensitive tests have to force the ordering they want rather than hope
  for it. The split-send test steps whole frames between words to be sure the
  receiver has caught up first - without that it passes for the wrong reason.
- It is slow. A run is a couple of minutes, most of it emulation, which is why
  it is a separate command rather than part of `make test`.
