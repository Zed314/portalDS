portalDS unit tests
===================

    make test           # from the repository root
    make -C tests       # the same thing

The tests need nothing but a C compiler - no BlocksDS, no Docker, no DS. They
compile the game's own sources with the host compiler and run them natively.
AddressSanitizer and UndefinedBehaviorSanitizer are on by default; pass
`SANITIZE=0` to turn them off.

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

These are the parts of the codebase that are pure logic: data in, data out,
no hardware. That is also where the bugs are worst, because a wrong answer in
the fixed point maths shows up as physics that feels subtly off rather than as
a crash, and a wrong answer in the codec corrupts map files.

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
