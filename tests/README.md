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
  which is only true because `fixMatrix()` runs every step.

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
  is untangled from the hardware they depend on.
- **The FIFO protocol** - `PI7.c` and `PI9.c` are the boundary between the two
  CPUs and genuinely need both of them. `tests/host/physics_fixture.c` stands
  in for the state `PI7.c` owns, so the physics can be tested without it.

How the host build works
------------------------

`tests/host/` contains just enough to let real game sources compile natively:

- `nds.h` stands in for libnds. The sources under test want two things from
  it - the legacy integer typedefs and the section attributes - and neither
  needs hardware.
- `common/general.h` stands in for the ARM9 umbrella header, which otherwise
  drags in the entire ARM9.
- `arm_primitives.c` supplies the two ARM assembly routines described above.
- `f32_test.h` holds the fixed point assertion helpers and the three
  tolerances (`TOL_BIT`, `TOL_FEW`, `TOL_COARSE`) the suites assert with.
- `physics_fixture.[ch]` owns the globals the FIFO layer would normally hold
  and provides `physicsReset()`, which every physics test calls from `setUp()`.

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
