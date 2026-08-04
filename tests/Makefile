# Host unit tests for portalDS.
#
# These do not use the BlocksDS toolchain: they compile the game's own sources
# with the host compiler and run them natively, so `make test` needs nothing
# more than gcc. See tests/README.md for what that does and does not cover.
#
#   make            build and run every suite
#   make build      build only
#   make SANITIZE=0 skip ASan/UBSan (they are on by default)
#   make clean

CC        ?= cc
RM        := rm -rf

# Objects depend on this file: the compiler flags below (the sanitizer set in
# particular) change what the tests actually check, so editing them has to
# force a rebuild.
THISFILE  := $(abspath $(lastword $(MAKEFILE_LIST)))
TESTDIR   := $(dir $(THISFILE))
ROOT      := $(TESTDIR)..
BUILD     := $(TESTDIR)build

# ASan matters here more than usual: the codec under test does its own manual
# buffer arithmetic, and a four byte overrun is exactly the kind of thing that
# is invisible on a DS and fatal on a map file.
#
# Two UBSan checks are switched off, because the engine relies on the
# behaviour they flag and gcc defines both the way the ARM hardware does:
#  - shift-base: inttof32() left shifts negative values, which is formally
#    undefined in C but is an arithmetic shift here and on the DS;
#  - signed-integer-overflow: mulv16() is documented to wrap on large inputs
#    and there is a test asserting exactly that.
# -fwrapv makes the second one defined rather than merely unreported.
SANITIZE  ?= 1
ifeq ($(SANITIZE),1)
SANFLAGS  := -fsanitize=address,undefined \
             -fno-sanitize=shift-base,signed-integer-overflow \
             -fno-sanitize-recover=all -fno-omit-frame-pointer
else
SANFLAGS  :=
endif

BASEFLAGS := -std=gnu11 -g -O1 -fwrapv $(SANFLAGS)

# The game sources predate most of these warnings and are not the thing under
# test, so they are built without -Wextra to keep the output readable.
GAMEFLAGS := $(BASEFLAGS) -Wall
TESTFLAGS := $(BASEFLAGS) -Wall -Wextra

# The ARM9 sources need two more turned off, and both are worth knowing about
# rather than only silencing:
#  - unused-result: the level reader ignores the return of nearly every fread,
#    so a truncated file leaves it decoding whatever was already in the
#    destination. That is real and unfixed; readEntities() is the one place it
#    is checked, because a bogus entity count was doing damage past the end of
#    a table rather than merely producing a nonsense entity. Fixing the rest is
#    a bigger change than the one these tests were written for.
#  - restrict: iniparser_load carves line, section, key, tmp and val out of one
#    allocation and then sprintf()s two of them into a third. gcc cannot see
#    that the slices do not overlap. Also real, also upstream's.
# Without these, 25 lines of warning bury the test results on every run.
GAME9FLAGS := $(GAMEFLAGS) -Wno-unused-result -Wno-restrict

# Include path notes:
#  - tests/host must come first so its nds.h and common/general.h shadow the
#    toolchain and ARM9 originals;
#  - arm7/include is deliberately NOT on the path for the test files
#    themselves. It contains a math.h that would be found in place of the
#    system one that Unity includes; the suites reach the engine header by
#    relative path instead.
I_HOST    := -I$(TESTDIR)host
I_UNITY   := -I$(TESTDIR)unity
I_COMMON  := -I$(ROOT)/common/include
I_ARM7    := -I$(ROOT)/arm7/include $(I_COMMON)
I_ARM9    := -I$(ROOT)/arm9/include $(I_COMMON)

# The real ARM7 build gets these from ds_arm7.specs. ARM7 in particular
# selects a different computePortalPlane() in common/include/PIC.h, so without
# it the tests would be exercising the ARM9's version of the portal maths.
D_ARM7    := -DARM7 -D__NDS__

# The ARM9 counterpart, for test_levelfile only. It switches on the rest of
# tests/host/common/general.h - the real ARM9 type headers, which the level
# format is described in - and the ARM9 half of tests/host/nds.h. It must NOT
# reach the ARM7 build: arm7/include/math.h defines inttof32 and friends too.
D_ARM9    := -DTEST_ARM9_FULL

SUITES    := fixed_math matrix compression obb collision solver platform levelfile room rectangle physics pcx portals settings
BINS      := $(addprefix $(BUILD)/test_,$(SUITES))

.PHONY: all build test clean
all: test

build: $(BINS)

# Runs every suite even if an earlier one fails, then fails the build if any
# did - one run should tell you about all the breakage, not just the first.
test: $(BINS)
	@fail=0; \
	for t in $(BINS); do \
		echo "== $$(basename $$t)"; \
		$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "FAILED"; exit 1; fi; \
	echo "All suites passed."

clean:
	$(RM) $(BUILD)

$(BUILD):
	@mkdir -p $(BUILD)

# --- framework and host support ------------------------------------------

$(BUILD)/unity.o: $(TESTDIR)unity/unity.c $(THISFILE) | $(BUILD)
	$(CC) $(BASEFLAGS) $(I_UNITY) -c $< -o $@

$(BUILD)/arm_primitives.o: $(TESTDIR)host/arm_primitives.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(I_HOST) -c $< -o $@

$(BUILD)/physics_fixture.o: $(TESTDIR)host/physics_fixture.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM7) $(I_HOST) $(I_UNITY) -c $< -o $@

$(BUILD)/level_fixture.o: $(TESTDIR)host/level_fixture.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM9) $(I_HOST) $(I_ARM9) $(I_UNITY) -c $< -o $@

# --- game sources under test ---------------------------------------------

# Every ARM7 source is compiled the same way, so one rule covers them all:
# build/arm7_OBB.o comes from arm7/source/OBB.c.
$(BUILD)/arm7_%.o: $(ROOT)/arm7/source/%.c $(THISFILE) | $(BUILD)
	$(CC) $(GAMEFLAGS) $(D_ARM7) $(I_HOST) $(I_ARM7) -c $< -o $@

$(BUILD)/compression.o: $(ROOT)/arm9/source/compression.c $(THISFILE) | $(BUILD)
	$(CC) $(GAMEFLAGS) $(I_HOST) $(I_ARM9) -c $< -o $@

# The ARM9 sources test_levelfile puts under test. Unlike compression.c these
# need the full ARM9 umbrella, hence D_ARM9. build/arm9_room.o comes from
# arm9/source/game/room.c; the level format types live under game/ and the
# parsers they lean on at the top of arm9/source/.
$(BUILD)/arm9_%.o: $(ROOT)/arm9/source/game/%.c $(THISFILE) | $(BUILD)
	$(CC) $(GAME9FLAGS) $(D_ARM9) $(I_HOST) $(I_ARM9) -c $< -o $@

$(BUILD)/arm9_%.o: $(ROOT)/arm9/source/editor/%.c $(THISFILE) | $(BUILD)
	$(CC) $(GAME9FLAGS) $(D_ARM9) $(I_HOST) $(I_ARM9) -c $< -o $@

$(BUILD)/arm9_%.o: $(ROOT)/arm9/source/%.c $(THISFILE) | $(BUILD)
	$(CC) $(GAME9FLAGS) $(D_ARM9) $(I_HOST) $(I_ARM9) -c $< -o $@

# --- suites ---------------------------------------------------------------

# Suites reach the engine headers by relative path, so they need no include
# flags beyond the shim and the framework. test_compression is the exception
# and has its own rule below; an explicit rule beats this pattern.
$(BUILD)/test_%.o: $(TESTDIR)suites/test_%.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM7) $(I_HOST) $(I_UNITY) -c $< -o $@

$(BUILD)/test_compression.o: $(TESTDIR)suites/test_compression.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

$(BUILD)/test_levelfile.o: $(TESTDIR)suites/test_levelfile.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM9) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

$(BUILD)/test_room.o: $(TESTDIR)suites/test_room.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM9) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

$(BUILD)/test_rectangle.o: $(TESTDIR)suites/test_rectangle.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM9) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

$(BUILD)/test_physics.o: $(TESTDIR)suites/test_physics.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM9) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

$(BUILD)/test_pcx.o: $(TESTDIR)suites/test_pcx.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM9) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

$(BUILD)/test_portals.o: $(TESTDIR)suites/test_portals.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM9) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

$(BUILD)/test_settings.o: $(TESTDIR)suites/test_settings.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(D_ARM9) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

MATHOBJS  := $(BUILD)/unity.o $(BUILD)/arm_primitives.o $(BUILD)/arm7_math.o

# The whole ARM7 physics engine, minus the FIFO layer that talks to the other
# CPU - physics_fixture.o stands in for the state that layer owns.
PHYSOBJS  := $(MATHOBJS) $(BUILD)/physics_fixture.o \
             $(BUILD)/arm7_OBB.o $(BUILD)/arm7_AAR.o \
             $(BUILD)/arm7_plane.o $(BUILD)/arm7_platform.o \
             $(BUILD)/arm7_PIC7.o

$(BUILD)/test_fixed_math: $(BUILD)/test_fixed_math.o $(MATHOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_matrix: $(BUILD)/test_matrix.o $(MATHOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_compression: $(BUILD)/test_compression.o $(BUILD)/unity.o $(BUILD)/compression.o
	$(CC) $(BASEFLAGS) $^ -o $@

$(BUILD)/test_obb: $(BUILD)/test_obb.o $(PHYSOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_collision: $(BUILD)/test_collision.o $(PHYSOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_solver: $(BUILD)/test_solver.o $(PHYSOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_platform: $(BUILD)/test_platform.o $(PHYSOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

# The level reader, the banner it fills in, and the real ini parser behind
# both - the point of the suite is that the whole path from file to buffer is
# the shipping one. level_fixture.o supplies the entity pools it calls into.
LEVELOBJS := $(BUILD)/unity.o $(BUILD)/level_fixture.o \
             $(BUILD)/arm9_room.o $(BUILD)/arm9_levelinfo.o \
             $(BUILD)/arm9_iniparser.o $(BUILD)/arm9_dictionary.o \
             $(BUILD)/arm9_rectangle.o $(BUILD)/arm9_files.o \
             $(BUILD)/arm9_portals.o $(BUILD)/arm9_PIC.o

$(BUILD)/test_levelfile: $(BUILD)/test_levelfile.o $(LEVELOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_room: $(BUILD)/test_room.o $(LEVELOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_rectangle: $(BUILD)/test_rectangle.o $(LEVELOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

# The player collision resolver. editor/rectangle.c is in LEVELOBJS and is
# linked for real rather than stubbed: getClosestPointRectangle does the actual
# geometry, so stubbing it would leave only a model of the resolver under test.
$(BUILD)/test_physics: $(BUILD)/test_physics.o $(LEVELOBJS) $(BUILD)/arm9_physics.o
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

# The image decoder, with files.c linked in for real: bufferizeFile is plain
# stdio, so the tests hand it actual files rather than a stand-in reader.
$(BUILD)/test_pcx: $(BUILD)/test_pcx.o $(LEVELOBJS) $(BUILD)/arm9_pcx.o
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

# Portal placement; portals.c and PIC.c are both in LEVELOBJS.
$(BUILD)/test_portals: $(BUILD)/test_portals.o $(LEVELOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

# The settings file. iniparser and dictionary are the real ones - the point of
# the suite is the round trip through the format the game actually writes -
# and files.c is here for basePath, which settings.c resolves its path from.
$(BUILD)/test_settings: $(BUILD)/test_settings.o $(LEVELOBJS) $(BUILD)/arm9_settings.o
	$(CC) $(BASEFLAGS) $^ -o $@ -lm
