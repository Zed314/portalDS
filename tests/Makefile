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

# Include path notes:
#  - tests/host must come first so its nds.h and common/general.h shadow the
#    toolchain and ARM9 originals;
#  - arm7/include is deliberately NOT on the path for the test files
#    themselves. It contains a math.h that would be found in place of the
#    system one that Unity includes; the suites reach the engine header by
#    relative path instead.
I_HOST    := -I$(TESTDIR)host
I_UNITY   := -I$(TESTDIR)unity
I_ARM7    := -I$(ROOT)/arm7/include -I$(ROOT)/common/include
I_ARM9    := -I$(ROOT)/arm9/include

SUITES    := fixed_math matrix compression
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

# --- game sources under test ---------------------------------------------

$(BUILD)/arm7_math.o: $(ROOT)/arm7/source/math.c $(THISFILE) | $(BUILD)
	$(CC) $(GAMEFLAGS) $(I_HOST) $(I_ARM7) -c $< -o $@

$(BUILD)/compression.o: $(ROOT)/arm9/source/compression.c $(THISFILE) | $(BUILD)
	$(CC) $(GAMEFLAGS) $(I_HOST) $(I_ARM9) -c $< -o $@

# --- suites ---------------------------------------------------------------

$(BUILD)/test_fixed_math.o: $(TESTDIR)suites/test_fixed_math.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(I_HOST) $(I_UNITY) -c $< -o $@

$(BUILD)/test_matrix.o: $(TESTDIR)suites/test_matrix.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(I_HOST) $(I_UNITY) -c $< -o $@

$(BUILD)/test_compression.o: $(TESTDIR)suites/test_compression.c $(THISFILE) | $(BUILD)
	$(CC) $(TESTFLAGS) $(I_HOST) $(I_UNITY) $(I_ARM9) -c $< -o $@

MATHOBJS  := $(BUILD)/unity.o $(BUILD)/arm_primitives.o $(BUILD)/arm7_math.o

$(BUILD)/test_fixed_math: $(BUILD)/test_fixed_math.o $(MATHOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_matrix: $(BUILD)/test_matrix.o $(MATHOBJS)
	$(CC) $(BASEFLAGS) $^ -o $@ -lm

$(BUILD)/test_compression: $(BUILD)/test_compression.o $(BUILD)/unity.o $(BUILD)/compression.o
	$(CC) $(BASEFLAGS) $^ -o $@
