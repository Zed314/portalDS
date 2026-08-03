portalDS unit tests
===================

    make test                   # from the repository root
    make -C tests               # the same thing
    ./docker-build.sh test      # in a container, if you would rather not
                                # install a compiler

The tests need nothing but a C compiler - no BlocksDS, no DS. They compile the
game's own sources with the host compiler and run them natively.
AddressSanitizer and UndefinedBehaviorSanitizer are on by default; pass
`SANITIZE=0` to turn them off.

The container route deliberately does not use the BlocksDS image. That image
is a *cross* toolchain and has no host compiler at all, which is the same
reason `./docker-build.sh docs` has to fetch its own image: it pulls plain
Ubuntu, installs gcc, runs `make test` against the bind-mounted tree and hands
`tests/build` back to you rather than to root. `TEST_IMAGE` overrides the
image; anything after `test` is passed through to make.

Framework
---------

[Unity](https://github.com/ThrowTheSwitch/Unity) (ThrowTheSwitch, MIT),
vendored under `tests/unity/` at v2.6.0. It is three files with no build
system or package of its own, which keeps the test build as self-contained as
the ROM build, and it has the tolerance-based assertions this codebase needs -
almost nothing here compares equal on the nose once it has been through a
20.12 fixed point pipeline.

To upgrade it, replace the three files from the upstream release; nothing in
`tests/` depends on anything else in Unity.

What is covered
---------------

| Suite | Code under test |
| ----- | --------------- |
| `test_fixed_math` | `arm7/include/math.h` - fixed point conversions, multiplies, vectors, lengths, trig |
| `test_matrix` | `arm7/source/math.c` - 3x3 rotation matrices, `rotateMatrixAxis`, `fixMatrix` |
| `test_compression` | `arm9/source/compression.c` - the 16 bit RLE codec every saved level goes through |
| `test_obb` | `arm7/source/OBB.c` - body setup, inertia, corners, bounding boxes, torque |
| `test_collision` | `arm7/source/AAR.c` + the box-box narrow phase - contact generation and the broadphase grid |
| `test_solver` | `arm7/source/OBB.c` - impulses, integration, sleeping, portal transport |
| `test_platform` | `arm7/source/platform.c` - moving platforms, arrival, and carrying bodies |
| `test_levelfile` | `arm9/source/game/room.c` + `levelinfo.c` - the entity section of a level file, and the level banner |
| `test_room` | `arm9/source/game/room.c` - room geometry, and the rectangle sections of a level file |
| `test_rectangle` | `arm9/source/editor/rectangle.c` - closest point, ray casts, the maximal rectangle finder and the lightmap atlas packer |
| `test_physics` | `arm9/source/game/physics.c` - the player's swept sphere collision, movement and gravity |
| `test_pcx` | `arm9/source/pcx.c` - the image decoder, against malformed files |

These are the parts of the codebase that are pure logic: data in, data out,
no hardware. That is also where the bugs are worst, because a wrong answer in
the fixed point maths shows up as physics that feels subtly off rather than as
a crash, and a wrong answer in the codec corrupts map files.

Note that the physics engine is in that list. It reads like hardware code
because it lives on the ARM7 next to the FIFO, but `OBB.c`, `AAR.c` and
`platform.c` between them make not a single rendering, VRAM, input or FIFO
call - they are fixed point maths over a pool of structs. What kept them
untestable was the global state, not the hardware, and `physicsReset()` in
`tests/host/physics_fixture.c` deals with that.

The physics suites lean on properties rather than golden numbers, because a
fixed point impulse solver has no closed form to compare against:

- a rectangle generates contacts only for a body that really straddles it,
  within its extent;
- gathering contacts through the broadphase grid gives exactly what testing
  every rectangle by brute force would - the grid is an optimisation, not a
  behaviour;
- an impulse never adds energy, so a stack of cubes cannot explode;
- a body resting on a floor is still resting on it hundreds of frames later;
- an orientation matrix is still a rotation after hundreds of integrations,
  which is only true because `fixMatrix()` runs every step;
- a body left standing on a rising platform is still standing on it two
  hundred steps later, having gone up with it.

`test_levelfile` and `test_room` are the odd ones out, and assert two
different things.

The first is that the level reader does not believe the file. Levels are
downloaded from the project's webpage, so a `.map` and its sidecar `.ini` are
the only data in the game the player did not produce - every count, offset and
string in them is hostile until checked. The suites feed the reader files no
editor would write (a count of 65535 entities, a title a kilobyte long, a
section that stops mid-record) and lean on the sanitizers to catch what an
assertion cannot: a write one element past a fixed table looks like a clean
pass unless something is watching the bounds. See the comment at the top of
`test_levelfile` for which sanitizer catches which regression.

The second is that the reader and the writer still agree about the format.
`writeEntity()` in `editor/io.c` and `readEntity()` in `game/room.c` are two
halves of one positional, self-describing-nothing format, and the comment at
the top of `room.c` warns that adding a field to one without the other
"silently corrupts every entity after it". So `test_levelfile` writes one
record of every entity type in sequence, followed by a light at a position
nothing else uses, and checks that light still comes out where it was put -
if any record's size drifts, the stream desynchronises and the sentinel
arrives as something else. Adding an entity type means adding a case there.

`test_rectangle` is the easiest of the ARM9 suites to have written and the
last one anybody got to: `editor/rectangle.c` needs nothing from the game but
`malloc`. Despite the file living under `editor/`, two thirds of it runs every
frame - `getClosestPointRectangle` is what the player's collision resolves
against, and `collideLineConvertedRectangle` is what both guns aim with.

The packer is tested by property rather than by golden positions, because a
bin packer has no single right answer: any placement that fits is correct.
What is asserted is what would actually be wrong - a patch outside the atlas,
or two patches sharing a pixel. Overlap is the one that matters, because it
does not crash; it silently lights two surfaces from the same pixels. The
maximal rectangle finder gets the same treatment: rather than pinning which
rectangle it picks, the test alternates find-and-fill over a shape and checks
it consumes every set cell and never claims one that was not set.

`test_pcx` is the only suite that needs no stand-in for the thing it tests:
`bufferizeFile()` is plain stdio, so `files.c` is linked in for real and the
decoder is handed actual files written to /tmp, byte for byte as a PCX is laid
out on disk. Only the cartridge and NitroFS calls around it are stubbed. Every
image in the game goes through this decoder, and it walks a file buffer with an
index, which is the shape of code that reads past the end when the header lies.

`test_physics` covers the other collision system - the player's, which is not
an ARM7 rigid body but a much simpler move-then-push-out sphere. It is worth
testing for the same reason the solver is: an error does not crash, it makes
movement feel wrong. Two things make it reachable at all. The rectangles it
resolves against come from a grid cell, and the fixture hands it one the test
built, so a test can put exactly one surface in front of the player. And
`getClosestPointRectangle` is linked in for real from `editor/rectangle.c`
rather than stubbed, so what is under test is the resolver and not a model of
it.

A few things are pinned as wrong rather than right, in the same spirit as
`test_platform`'s overshoot. In the level readers: an unknown entity tag
desynchronises the stream rather than being rejected, the rectangle counts
have no upper bound the way the entity count now does, and `roomOriginSize`
seeds its accumulators at 8192 and 0 instead of at the first rectangle. Each is commented with why it is pinned instead of fixed.

`test_physics` started out with two of those too, and both turned out to be
worth fixing rather than pinning - a swept move skipping collision on every
step after the first, and a divide by zero when an object's centre landed
exactly on a surface. The tests that pinned them now assert the fixed
behaviour instead, which is the outcome to aim for: a pin is a placeholder for
a fix, not a substitute for one.

Where a property cannot hold exactly, the inaccuracy is pinned rather than
papered over with a loose tolerance: `test_platform` asserts that a platform
overshoots its destination by exactly 512 units, because `dotProduct()`
truncates and that is what the arrival test can actually detect. Fixing it
would move where every platform in every shipped level comes to rest, which is
a level design decision.

What is NOT covered, and why
----------------------------

Most of the codebase cannot be tested this way and pretending otherwise would
be worse than not trying:

- **Anything touching the hardware** - the renderer, VRAM and texture
  management, the FIFO link between the two CPUs, input, sound, NitroFS. These
  need a DS or an emulator, not a unit test.
- **The ARM assembly routines** - `arm7/source/isqrt32.c` and
  `arm7/source/normalize.c` are hand written ARM and cannot be assembled by
  the host compiler. `tests/host/arm_primitives.c` provides reference C
  implementations so that everything *layered on top of them* can be tested;
  the assembly itself is only exercised on hardware. Anything whose result
  flows through them is asserted with a tolerance, not an exact value.
- **The game and editor logic** - large stateful modules wired into globals
  and hardware. Testable in principle, but only after the state they depend on
  is untangled from the hardware they depend on. `game/room.c` is the one
  exception so far, and it is instructive about the cost: the reader itself
  touches no hardware, but it ends every entity case by calling into a pool
  that does, so it took `tests/host/level_fixture.c` to stand those pools up.
  `game/game.c` is the rule rather than the exception - it is the renderer and
  the state driver, and there is no version of this that reaches it. The level
  title and author moved out to `game/levelinfo.c` for exactly that reason.
- **The FIFO protocol** - `PI7.c` and `PI9.c` are the boundary between the two
  CPUs and genuinely need both of them, so they are covered by a separate test
  ROM run under an emulator instead. See `tests/rom/README.md`.
  `tests/host/physics_fixture.c` stands in for the state `PI7.c` owns, so the
  physics can still be tested here without it.

How the host build works
------------------------

`tests/host/` contains just enough to let real game sources compile natively:

- `nds.h` stands in for libnds. The ARM7 sources want two things from it - the
  legacy integer typedefs and the section attributes - and neither needs
  hardware. Behind `-DTEST_ARM9_FULL` it also supplies the ARM9's vocabulary:
  a few graphics and sound types the ARM9 headers name in prototypes, and the
  20.12 arithmetic, which is real rather than stubbed so that anything
  computing a coordinate computes the right one.
- `common/general.h` stands in for the ARM9 umbrella header, which otherwise
  drags in the entire ARM9. Behind the same flag it pulls in the ARM9's real
  type headers, because the types the level format is made of have to be the
  ones the game actually uses.
- `level_fixture.[ch]` owns the entity pools `game/room.c` builds into, and
  provides `levelFixtureReset()`. Its stubs record what they were handed, in
  order, so a test can ask what a file actually produced rather than only
  whether the run survived - that log is what makes the entity format
  testable. `addRoomRectangle` is the exception and is a real list append,
  because the room geometry helpers walk the list it builds.
- `arm_primitives.c` supplies the two ARM assembly routines described above.
- `f32_test.h` holds the fixed point assertion helpers and the three
  tolerances (`TOL_BIT`, `TOL_FEW`, `TOL_COARSE`) the suites assert with.
- `physics_fixture.[ch]` owns the globals the FIFO layer would normally hold
  and provides `physicsReset()`, which every physics test calls from `setUp()`.

Building the ARM9 sources natively has already paid for itself once. UBSan
flagged a misaligned pointer store in the atlas packer, which turned out to be
`common/pcx.h` ending its packed section with `#pragma pack(4)` rather than
`#pragma pack(pop)` - so every header included after it, which is most of the
ARM9, was packed to four bytes. Invisible on the DS, where four is both the
default and the widest alignment anything needs; undefined behaviour on a host
with eight byte pointers. The fix is push/pop, and the ROM it produces is byte
identical.

`tests/Makefile` puts `tests/host` first on the include path so these shadow
the real headers, and deliberately keeps `arm7/include` off the include path
for the test files themselves - it contains a `math.h` that would be found in
place of the system one.

Adding a suite
--------------

Add `tests/suites/test_<name>.c` with its own `main()` calling `UNITY_BEGIN`,
`RUN_TEST` and `UNITY_END`, then add `<name>` to `SUITES` in `tests/Makefile`
along with the two rules for its object file and binary. If the new suite
needs a game source compiled, add a rule for that too - the include flags
differ per file, which is why they are spelled out rather than pattern
matched.
