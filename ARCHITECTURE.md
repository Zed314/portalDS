portalDS architecture
=====================

This is a map of how the program fits together, for someone who has cloned the
repository and wants to know where things are before changing them. It is
deliberately about the *seams* - the decisions that span more than one file and
are therefore not visible from any single one. Individual files carry their own
Doxygen comments explaining what they do; this explains why they are shaped
that way.

The short version: portalDS runs Portal on hardware with a 67MHz CPU, one 3D
engine, two screens and 4MB of RAM. Almost every unusual thing in this codebase
is a consequence of one of those numbers.


The three constraints
---------------------

**One 3D engine, two screens.** The DS renders 3D to one screen at a time. Both
screens showing 3D means rendering alternately and freezing each result into
VRAM with the display capture unit. Everything on screen therefore updates at
30Hz, not 60.

**A portal view is another render of the room.** Two open portals mean up to
three passes over the same geometry per displayed frame, on a CPU that struggles
with one. This is why so much of the codebase is culling, pre-baked display
lists and spatial grids - and why portal views are refreshed on a rota rather
than every frame.

**Two processors, one of them idle.** The ARM7 exists to run sound and touch
input, and is otherwise doing nothing. So the rigid body physics engine was
moved onto it, and the two halves talk over the hardware FIFO. That split is
the single most invasive architectural decision in the project.


The two processors
------------------

```
        ARM9  (game)                          ARM7  (physics)
   ┌───────────────────────┐             ┌───────────────────────┐
   │ rendering             │             │ rigid body solver     │
   │ level logic, entities │             │ contact generation    │
   │ player movement       │  FIFO_USER_08 →  broadphase grid    │
   │ input, editor, menus  │             │ moving platforms      │
   │                       │ ← FIFO_USER_01..07                  │
   │ mirror of every body  │             │ the bodies themselves │
   └───────────────────────┘             └───────────────────────┘
```

The ARM7 owns the simulation. The ARM9 owns everything else and keeps a
read-only mirror of the results in `arm9/source/PI9.c` - the `objects[]` and
`aaRectangles[]` arrays there are shadows, updated once a frame from what comes
back over the FIFO, never authoritative.

**The protocol** is defined in `common/include/PIC.h`, the one header both CPUs
compile. A command is a header word on `FIFO_USER_08` packing an opcode and an
object id, followed by a fixed number of argument words:

```
 31                    6 5        0
+-----------------------+----------+
|        object id      |  opcode  |
+-----------------------+----------+
```

Replies come back on `FIFO_USER_01..07`: a header word carrying the object id,
the ground it is resting on and a teleported flag; three words of position; then
three words packing six of the nine orientation elements, two to a word. The
third column is not sent at all - the ARM9 reconstructs it as the cross product
of the other two, saving a word per body per frame.

`arm9/source/PI9.c` holds every sender; `arm7/source/PI7.c` holds the decoder.
They are the two halves of one contract and neither includes the other.

**Things worth knowing about the split:**

- *The ARM7 runs five solver substeps per displayed frame but transmits once.*
  That is what makes stacked cubes stable without costing FIFO bandwidth. See
  `mainLoop()` in `arm7/source/main.c`.
- *The player is not a rigid body.* It gets its own swept-sphere collision on
  the ARM9 (`arm9/source/game/physics.c`), because a first-person camera driven
  by an impulse solver feels wrong. Cubes are rigid bodies; the player is not.
- *Trigonometry happens on the ARM9.* `createBox` sends a sine and cosine, not
  an angle.
- *There is no backpressure.* Every sender ignores what `fifoSendValue32()`
  returns, and because the decoder reads a fixed argument count per opcode, one
  dropped word desynchronises the stream permanently. What prevents that in
  practice is a single `swiWaitForVBlank()` every eight rectangles in
  `transferRectangles()`. See `tests/rom/README.md`.
- *The parts of portal maths both CPUs need* - `warpVector`, `warpMatrix`,
  `computePortalPlane` - live in `PIC.h` so the two sides stay bit-identical.
  Bodies cross portals on the ARM7; the player crosses on the ARM9; they must
  agree.


The ARM9 program
----------------

The program is always in exactly one of three states - menu, game, editor -
each a `state_struct` of four callbacks (`arm9/include/engine/state.h`).
`main()` runs them in a fixed cycle:

```c
while(1)
{
    currentState->init();          // build the world
    while(currentState->used)
        currentState->frame();     // one frame
    currentState->kill();          // tear it down
    applyState();                  // switch to the pending state
}
```

Switching is always deferred: `changeState()` only records where to go next and
drops out of the frame loop, so no state is ever destroyed while its own code is
still on the stack. Game logic can call it from anywhere.

Each state also owns its allocations. `applyState()` resets the tracker in
`arm9/source/memory.c`, and `freeState()` releases everything the state took, in
reverse order so the heap top comes back down instead of fragmenting. A mode can
be sloppy about freeing during its lifetime without hurting the next one.

Each of the three modes has an umbrella header that includes its whole world in
dependency order (`game/game_main.h`, `editor/editor_main.h`, `menu/menu_main.h`)
and an `_ex.h` header exposing only the four entry points to everyone else.
There are essentially no forward declarations in this codebase, so **the include
order in those umbrella headers is the dependency graph.**


The frame, and how portals are drawn
------------------------------------

This is the part of the codebase that is hardest to reconstruct by reading, and
the part most likely to surprise you. `gameFrame()` in `arm9/source/game/game.c`
alternates between two phases, one per hardware frame, so the game runs at 30Hz:

```
phase 0                                  phase 1
──────────────────────────────────       ──────────────────────────────────
postProcess1()  scan main view for       postProcess2()  composite main view
                portal key colours,                      + portal captures
                recording run bounds                      into the visible BG
render1()       submit the main view     render2()        submit ONE portal view
                                         listenPI9()      read ARM7 results
                                         updateOBBs()
── vblank ──                             ── vblank ──
copy VRAM_C → portal viewPoint           copy VRAM_C → mainScreen
arm capture                              arm capture
```

The trick that makes it affordable is **colour-key compositing**. The main view
does not render the portals' contents. It draws each portal's clipped elliptical
outline filled with a flat reserved colour - yellow `RGB15(31,31,0)` for portal
1, cyan `RGB15(0,31,31)` for portal 2. Then:

1. `postProcess` (hand-written ARM, `arm9/source/game/postprocess.s`) sweeps the
   captured main view looking for runs of those two colours, and writes the run
   boundaries into a stack of pointers;
2. `postProcess2()` walks that stack and DMAs each run either from the main view
   or from the corresponding portal's captured view, straight into the visible
   background.

So a portal's contents are pasted in per scanline segment, and the expensive
work - clipping the portal shape - is done by the rasteriser for free while it
fills the outline.

Two consequences fall out of this that are easy to trip over:

- **Portal visibility is discovered, not computed.** `orangeSeen` and `blueSeen`
  are set by `postProcess2()` according to whether it actually found those
  colours in the frame. Nothing does a frustum test against the portals; the
  renderer finds out a portal is on screen by seeing its key colour.
- **Only one portal view is re-rendered per cycle.** `render2()` alternates when
  both are visible, so with two portals on screen each view refreshes at 15Hz
  while the main view stays at 30Hz. The stale capture is reused in between.

`updatePortalCamera()` warps the player's camera through to the far portal -
position and each column of the orientation matrix - and `drawPortalRoom()`
renders from there into that portal's own pre-culled display list.

The other two states do not use this path at all. The menu uses
`arm9/source/dual3D.c`, which alternates the 3D engine between the two *screens*
and shows each captured result as a bitmap background - the same capture
hardware, a different purpose. `d3dScreen` tells drawing code which screen is
being rendered right now, and a good deal of code branches on it. The editor is
simpler still: 3D on the top screen only, with a plain 2D background underneath
for the touch interface.


The world
---------

**Three coordinate systems**, and keeping them straight is most of the battle:

| | | |
|---|---|---|
| tile | integers | what the editor and the level format use; `TILESIZE` (384) across, `HEIGHTUNIT` (192) tall |
| world | f32 (20.12) | what physics and gameplay use; `convertVect()` maps from tiles and lands on tile *centres* |
| view | world × `SCALEFACT` | scaled so a room fits the geometry engine's fixed point range |

All maths is 20.12 fixed point. Nothing is floating point anywhere, on either
CPU.

**A room** (`arm9/include/game/room.h`) is a list of textured rectangles plus
entities plus lighting. It is not a mesh - the level is axis-aligned rectangles
throughout, which is what makes the collision world cheap enough to simulate and
the level format compact enough to store.

**The grid.** `generateRoomGrid()` bins rectangles into `CELLSIZE`-tile cubes and
caches each cell's three nearest lights. Everything performance-sensitive goes
through it: ray casts for the portal gun, dynamic object lighting, and culling.
The ARM7 builds its own independent broadphase grid over the same rectangles
when it receives `PI_MAKEGRID`.

**Display lists.** `generateRoomDisplayList()` bakes a room into a hardware
command buffer, culled against a given viewpoint and normal. Each portal gets
its own, which is precisely what makes drawing the room three times a frame
possible - the alternative, re-submitting geometry each pass, does not fit in
the budget.


Level data
----------

```
  editor  ──writeMapEditor──▶  .map file  ──newReadMap──▶  room_struct
                                                              │
                              ┌───────────────────────────────┤
                              ▼                               ▼
                    transferRectangles()            generateRoomDisplayList()
                    (→ ARM7 collision world)        (→ what gets drawn)
```

A `.map` file is a header of section offsets followed by the compressed block
array, the optimised rectangle list, the lightmap atlas and the entities. The
codec is 16-bit RLE, taken from GRIT (`arm9/source/compression.c`).

The editor writes rectangles that `generateOptimizedRectangles()` has merged, so
the geometry the game loads is not the geometry the level was authored as. The
block array is kept alongside it precisely so the editor can reload and re-edit.

**The entity format is positional and has no self-description.** `writeEntity()`
in `arm9/source/editor/io.c` and `readEntity()` in `arm9/source/game/room.c` are
two halves of one format, and adding a field to one without the other silently
corrupts every entity after it in the file. `adaptVector()` converts between the
game's world coordinates and the editor's per-face frames and is applied on both
paths, which is why an asymmetry there is particularly hard to see.

Entities are wired to each other through `arm9/source/game/activator.c`:
activators are stored in the file as indices and resolved back to pointers as
entities are created.


Memory
------

**Main RAM** is 4MB. The state-scoped allocator (`arm9/source/memory.c`) is a
flat table of at most `MAX_MALLOC` (512) pointers; `alloc()` records, and
`freeState()` releases in reverse. Note that `reAlloc()` does *not* preserve
contents - it frees and re-allocates.

**VRAM** is the tighter constraint, and the awkwardness in
`arm9/source/textures.c` all comes from one rule: a bank is either mapped for
the CPU to write or mapped for the 3D engine to read as texture memory, never
both. So textures are bump-allocated into banks while the banks are CPU-visible,
and the addresses computed then are baked into each texture's descriptor for
later use.

The game state's layout:

| Bank | Use |
|---|---|
| A, B | texture memory |
| C | display capture target - the main and portal views land here |
| D | main background: the composited image that is actually displayed |
| H, I | sub-screen background |

Portal view textures are a special case: their pixels are written by the capture
unit rather than the CPU, so they need an allocation at a fixed address that can
be re-described rather than moved when the visible size changes. That is what
`createReservedTextureBufferA5I3()` and `changeTextureSizeA5I3()` are for.


The source tree
---------------

```
common/include/PIC.h     the contract between the two CPUs - start here
arm7/                    the physics engine
  include/stdafx.h         umbrella header; include order is dependency order
  source/OBB.c             rigid bodies, the solver, portal transport
  source/AAR.c             static rectangles, contacts, broadphase grid
  source/platform.c        moving platforms
  source/PI7.c             FIFO command decoder and result transmitter
  source/isqrt32.c,
         normalize.c       hand-written ARM assembly
arm9/                    the game
  include/common/general.h umbrella header for the whole ARM9 side
  source/main.c            entry point, the three states
  source/PI9.c             physics bridge: senders, receiver, mirror
  source/dual3D.c          3D on both screens
  source/textures.c        VRAM allocation and texture upload
  source/memory.c          state-scoped allocation
  source/compression.c     the RLE codec every level goes through
  source/game/             playing a test chamber
    game.c                   the frame and the render passes
    portals.c                placement, rendering, walking through
    map.c                    grid, ray casts, culling, display lists
    room.c                   the level loader
    physics.c                swept-sphere collision for the player
    postprocess.s            the colour-key compositor
  source/editor/           the level editor; io.c is the level writer
  source/menu/             the front end
tests/                   see tests/README.md and tests/rom/README.md
```


Before you change something
---------------------------

A short list of the things that have teeth:

- **The FIFO protocol is duplicated by design.** `PIC.h` documents the layout,
  `PI9.c` encodes it, `PI7.c` decodes it, and `tests/rom/include/fifo_protocol.h`
  writes it out a third time on purpose, so the test can catch the other two
  drifting apart. Changing an opcode means changing all four.
- **The entity format is positional.** Two files, no version field, no length
  prefix. See above.
- **Culling is not an optimisation here, it is the budget.** Anything not
  rejected on the CPU is paid for up to three times per frame.
- **"Down" is a variable.** `normGravityVector` can point along any axis, which
  is why `physics.c` is full of `if(normGravityVector.x)` branches instead of
  assuming Y is up.
- **`createPlatform()` does not bounds-check its id.** It is safe only because
  its one caller applies `id % NUMPLATFORMS`.
- **This is 2007-2013 code and the author says so in the README.** Some of it
  was written in the fortnight before a competition deadline. The documentation
  in this repository tries to describe what the code *does*, including where
  that is unfortunate, rather than what it ought to have done.


Where to read next
------------------

- `common/include/PIC.h` - the CPU split and the FIFO protocol
- `arm7/include/stdafx.h` - the physics engine, in dependency order
- `arm9/include/common/general.h` - the ARM9 as a whole
- `arm9/include/game/game_main.h` - how a test chamber is put together
- `tests/README.md` - what is covered by tests, and what cannot be
- `tests/rom/README.md` - the two-CPU contract tests, and what they turned up
