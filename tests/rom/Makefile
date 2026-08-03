# Builds fifotest.nds - the FIFO round trip test ROM.
#
# The ROM is the real, unmodified arm7.elf paired with a test ARM9 binary, so
# what runs on the far side of the FIFO is the shipped physics engine and the
# shipped command decoder. Only the ARM9 half is test code.
#
# Needs the BlocksDS toolchain, exactly like the game does. Run it through
# ./docker-build.sh if you would rather not install one:
#
#   ./docker-build.sh -f tests/rom/Makefile
#
# or just use tests/rom/run.sh, which builds and runs in one step.

BLOCKSDS	?= /opt/blocksds/core

ROOT		:= ../..
ROM		:= fifotest.nds

# The test ARM9 binary is built with the game's own Makefile.arm9, pointed at
# this directory's sources. Command line variables override the ones set inside
# it, so nothing there needs changing to support a second binary.
TEST9_NAME	:= test9
TEST9_ELF	:= $(ROOT)/build/$(TEST9_NAME).elf
ARM7_ELF	:= $(ROOT)/build/arm7.elf

NDSTOOL		:= $(BLOCKSDS)/tools/ndstool/ndstool
DLDIPATCH	:= $(BLOCKSDS)/tools/dldipatch/dldipatch

.PHONY: all clean arm7 test9

all: $(ROM)

clean:
	rm -f $(ROM)
	rm -rf $(ROOT)/build/$(TEST9_NAME) $(TEST9_ELF) \
	       $(ROOT)/build/$(TEST9_NAME).map $(ROOT)/build/$(TEST9_NAME).dump

arm7:
	$(MAKE) -C $(ROOT) -f Makefile.arm7 --no-print-directory

test9:
	$(MAKE) -C $(ROOT) -f Makefile.arm9 --no-print-directory \
		NAME=$(TEST9_NAME) \
		SOURCEDIRS=tests/rom/source \
		INCLUDEDIRS=tests/rom/include \
		AUDIODIRS=

$(ROM): arm7 test9
	@echo "  NDSTOOL $@"
	@$(NDSTOOL) -c $@ -7 $(ARM7_ELF) -9 $(TEST9_ELF) \
		-b $(ROOT)/logo.png "FIFO tests;portalDS;test ROM" > /dev/null
