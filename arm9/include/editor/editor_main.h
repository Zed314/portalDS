/**
 * @file editor_main.h
 * @brief Umbrella include for the level editor.
 *
 * Every file under arm9/source/editor/ includes this. Note that it pulls in
 * @ref game_main.h first: the editor draws the level with the game's renderer
 * and shares its level format, so it has the whole game available to it.
 *
 * @par How the editor is put together
 *  - **the model** - @ref blocks.h. The editor does *not* edit rectangles
 *    directly. It edits a 3D array of solid/empty blocks, and only converts
 *    that to the game's rectangle soup when saving. That is the central design
 *    decision of the editor and the reason it is usable with a stylus at all.
 *  - **entities** - @ref entity.h, a table-driven description of every
 *    placeable object, so adding one is data rather than code.
 *  - **editing** - @ref selection.h (dragging out a box of blocks) and
 *    @ref roomeditor.h (the camera, the cursor and the frame loop).
 *  - **interface** - @ref interface.h and @ref contextbuttons.h, the toolbar
 *    and the pop-up menus that appear for whatever is selected.
 *  - **output** - @ref io.h writes the level file, which involves converting
 *    blocks to rectangles, packing lightmaps (@ref rectangle.h) and baking the
 *    lighting (@ref lighting.h).
 *
 * @see editor_ex.h for the small surface the rest of the program sees.
 */

#ifndef __EDITORMAIN9__
#define __EDITORMAIN9__

#include "common/general.h"
#include "game/game_main.h"      // the editor renders with the game's renderer
#include "editor/blocks.h"       // the block array, the editor's real model of a level
#include "editor/contextbuttons.h" // pop-up menus for the current selection
#include "editor/entity.h"       // placeable objects, table-driven
#include "editor/selection.h"    // dragging out a range of blocks
#include "editor/roomeditor.h"   // editor camera, cursor and frame loop
#include "editor/material.h"     // surface materials; shared with the game
#include "editor/lighting.h"     // lightmap and vertex lighting data
#include "editor/rectangle.h"    // 2D rectangle packing, for lightmap atlases
#include "editor/io.h"           // reading and writing level files
#include "editor/interface.h"    // the toolbar

#endif
