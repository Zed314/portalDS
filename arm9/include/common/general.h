/**
 * @file general.h
 * @brief Umbrella include pulling in the entire ARM9 side of the game.
 *
 * Almost every ARM9 source file includes this one and nothing else. That is
 * deliberate: the codebase has no forward declarations to speak of, so the
 * include order below *is* the dependency order, and adding a header means
 * adding it here rather than at the point of use.
 *
 * @par Layout of the ARM9 tree
 * The three "_ex" headers are the seams between the three top-level modes of
 * the program, each of which is a @ref state_struct.
 *  - @c game/game_ex.h    - playing a test chamber (see game/game_main.h);
 *  - @c menu/menu_ex.h    - the front end and chapter select;
 *  - @c editor/editor_ex.h - the in-game level editor.
 *
 * Each "_ex" header exposes only the four state entry points plus whatever the
 * other modes genuinely need, so the modes stay largely independent of one
 * another. Everything above them in the list is shared infrastructure:
 * fixed point maths, file IO, texture and VRAM management, model and image
 * loading, fonts, the touch screen GUI and the display list builder.
 *
 * @see arm9/include/engine/state.h for the state machine these plug into.
 */

#ifndef __GENERAL9__
#define __GENERAL9__

#define GAMEVERSION "0.01" /**< Version string shown in the menu. */
#define arrayLength(a) (sizeof((a))/sizeof((a)[0])) /**< @brief Element count of a true array. Do not use on a pointer. */

#include <nds.h>
#include <fat.h>

//libnds's NORMAL_PACK masks x and y but shifts z unmasked, which is undefined
//for a negative z and a -Wshift-negative-value error. Same packing, z masked.
#undef NORMAL_PACK
#define NORMAL_PACK(x,y,z) (u32)(((x) & 0x3FF) | (((y) & 0x3FF) << 10) | (((u32)((z) & 0x3FF)) << 20))
#include <filesystem.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <unistd.h>

// #define FATONLY

// Shared infrastructure, in dependency order.
#include "common/compress.h"    // level data decompression (from GRIT)
#include "common/iniparser.h"   // .ini reader used for level and config files
#include "common/settings.h"    // the player's settings, read from config.ini
#include "common/math.h"        // fixed point vectors and matrices
#include "common/files.h"       // NitroFS / FAT file access
#include "dual3D.h"             // rendering 3D to both screens
#include "common/pcx.h"         // PCX image loader
#include "common/textures.h"    // VRAM bank and texture management
#include "common/md2.h"         // MD2 model loader and animation
#include "common/font.h"        // bitmap text rendering
#include "common/simplegui.h"   // touch screen widgets
#include "common/keyboard.h"    // on-screen keyboard
#include "game/displaylist.h"   // hardware display list construction

// The three top-level modes.
#include "game/game_ex.h"
#include "menu/menu_ex.h"
#include "editor/editor_ex.h"

// Diagnostics and lifetime management.
#include "debug/xmem.h"
#include "engine/state.h"
#include "engine/debug.h"
#include "engine/profiler.h"
#include "engine/memory.h"

#include "McuASAN.h"

extern state_struct gameState;   /**< The playing-a-level state. */
extern state_struct editorState; /**< The level editor state. */
extern state_struct menuState;   /**< The front end state; also where the program starts. */

#endif
