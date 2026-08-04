/*
 * Host stand-in for arm9/include/common/general.h, used only by the unit
 * tests.
 *
 * The real umbrella header pulls in the renderer, NitroFS, the texture
 * manager and every other ARM9 subsystem, none of which can exist on the
 * host. The sources that are worth testing on the host - compression.c so far
 * - only ever wanted the integer types and the C library out of it, so this
 * file provides that much. tests/Makefile puts tests/host ahead of
 * arm9/include, so this shadows the real header for the test build only.
 */

#ifndef PORTALDS_TEST_HOST_GENERAL_H
#define PORTALDS_TEST_HOST_GENERAL_H

#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same definitions as arm9/include/common/math.h. */
#define min(a,b) (((a)>(b))?(b):(a))
#define max(a,b) (((a)>(b))?(a):(b))

/*
 * Not behind TEST_ARM9_FULL, unlike the rest of the ARM9 headers below: the
 * real general.h puts this one before common/math.h because fadeIn() reads the
 * brightness setting out of it, so anything reaching math.h needs it too - and
 * the suites that only want the fixed point maths reach math.h directly.
 */
#include "common/settings.h"

/* On the DS this writes to the no$gba debug console. There is nothing to
 * write to here, and a test run should not be printing anyway. */
#define NOGBA(_fmt, _args...) ((void)0)

/*
 * Everything above is all compression.c has ever wanted, and it is what the
 * ARM7 suites get. test_levelfile needs more: the level reader in
 * game/room.c includes game_main.h, which needs the ARM9's real type headers
 * to be present before any of the game headers will parse.
 *
 * Those headers are the genuine article, not stand-ins - the types the level
 * format is made of have to be the ones the game actually uses, or the test
 * would be reading a different file format. Only the hardware vocabulary
 * underneath them is faked, up in nds.h.
 *
 * This is behind a flag rather than unconditional so that the suites which do
 * not need it keep compiling against the same three-include shim they always
 * have; tests/Makefile passes -DTEST_ARM9_FULL only where it is wanted.
 */
#ifdef TEST_ARM9_FULL

#include <stdarg.h>
#include <sys/stat.h>

#include "common/compress.h"
#include "common/iniparser.h"
#include "common/math.h"
#include "common/files.h"
#include "dual3D.h"
#include "common/pcx.h"
#include "common/textures.h"
#include "common/md2.h"
#include "common/font.h"
#include "common/simplegui.h"
#include "common/keyboard.h"
#include "game/displaylist.h"
#include "game/game_ex.h"
#include "menu/menu_ex.h"
#include "editor/editor_ex.h"
#include "engine/state.h"
#include "engine/memory.h"

extern state_struct gameState;
extern state_struct editorState;
extern state_struct menuState;

#endif /* TEST_ARM9_FULL */

#endif
