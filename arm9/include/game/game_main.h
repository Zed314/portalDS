/**
 * @file game_main.h
 * @brief Umbrella include for the game state - everything needed to play a test chamber.
 *
 * Every file under arm9/source/game/ includes this and nothing else. As with
 * @ref general.h, the include order below is the dependency order.
 *
 * @par How the game is put together
 * Reading roughly outwards from the core:
 *
 *  - **the world** - @ref room.h and @ref map.h. A room is a list of textured
 *    rectangles plus a spatial grid over them; map.h holds the tile-to-world
 *    conversions and the queries (ray casts, cell lookups) everything else
 *    uses.
 *  - **the view** - @ref camera.h (frustum, projection, view matrices) and
 *    @ref polygon.h (the clipping used for portal outlines).
 *  - **the player** - @ref player.h and @ref controls.h, with movement resolved
 *    by @ref physics.h. Note that the player is *not* an ARM7 rigid body: it
 *    gets its own simpler swept-sphere collision.
 *  - **portals** - @ref portals.h, the reason for most of the complexity
 *    elsewhere.
 *  - **entities** - turrets, cubes, doors, buttons, energy balls, platforms,
 *    elevators, emancipation grids and sludge. Each is a small pool with an
 *    init/update/draw trio, wired together through @ref activator.h.
 *  - **presentation** - @ref sfx.h, @ref particles.h, @ref lights.h and
 *    @ref pause.h.
 *
 * Three headers come from the editor rather than the game: @ref material.h,
 * @ref lighting.h and @ref rectangle.h. The two modes share the level format,
 * so they share the types that describe it.
 *
 * @see game_ex.h for the small surface the rest of the program sees.
 */

#ifndef __GAMEMAIN9__
#define __GAMEMAIN9__

// #define DEBUG_GAME /**< Define to enable the on-screen debug console and ARM7 alert printing. */

#include "common/general.h"
#include "editor/material.h"   // surface materials; shared with the editor
#include "game/lights.h"       // light sources
#include "editor/lighting.h"   // lightmap and vertex lighting data; shared with the editor
#include "game/room.h"         // rectangles, rectangle lists, the room struct
#include "game/map.h"          // tile/world conversions, room queries, ray casts
#include "game/physics.h"      // swept sphere collision for the player
#include "game/camera.h"       // frustum, projection and view matrices
#include "game/polygon.h"      // polygon clipping, used for portal outlines
#include "game/player.h"       // the player and the portal gun
#include "game/particles.h"    // particle effects
#include "game/portals.h"      // portal placement and recursive rendering
#include "editor/rectangle.h"  // rectangle helpers; shared with the editor
#include "PI9.h"               // the ARM7 physics bridge
#include "game/turrets.h"      // entities, from here down
#include "game/platform.h"
#include "game/cubes.h"
#include "game/door.h"
#include "game/activator.h"    // the trigger/target wiring shared by all of them
#include "game/energyball.h"
#include "game/bigbutton.h"
#include "game/timedbutton.h"
#include "game/emancipation.h"
#include "game/elevator.h"
#include "game/walldoor.h"
#include "game/sludge.h"
#include "game/sfx.h"          // sound
#include "game/pause.h"        // the pause menu
#include "game/controls.h"     // input mapping

#endif
