portalDS (aka Aperture Science DS)
=======

https://web.archive.org/web/20140728200937/http://smealum.net/ASDS/

This is the source code for portalDS, a homebrew adaptation of Valve Software's Portal for the Nintendo DS. Most of the code for this was written by myself, smea ( http://smealum.net , https://twitter.com/smealum ), and the graphics were made by Lobo ( http://infectuous.net ). You can email me at smealum@gmail.com; see my webpage for other means of contacting me.

While reading this, please keep in mind that this includes a bunch of legacy code that may date back as far as 2007; most of this probably isn't a good example of "good practice". Likewise, keep in mind that a number of features were rushed in the two weeks right before the neoflash competition ended in August 2013; a lot of these should probably be rewritten. (see git log to see what i'm referring to)

Compiling this fork requires BlocksDS; see https://blocksds.skylyrac.net/ for more information on setting everything up.

If you'd rather not install a toolchain, the project can be built with Docker using the official BlocksDS image:

    docker build --output out .     # produces out/portalDS.nds

Or, for incremental builds straight into the working tree (the ROM and build/ end up owned by your user):

    ./docker-build.sh               # same as "make", produces ./portalDS.nds
    ./docker-build.sh clean
    ./docker-build.sh dldipatch
    ./docker-build.sh docs          # generate the API reference, see below
    ./docker-build.sh sh            # interactive shell inside the toolchain

Set `BLOCKSDS_IMAGE` to pin a specific toolchain version; it defaults to `skylyrac/blocksds:slim-latest`.

Tests
-----

There is a host-side unit test suite covering the parts of the codebase that
are pure logic - the ARM7 fixed point maths, the 3x3 matrix code, the rigid
body physics engine, and the RLE codec every saved level goes through:

    make test                       # needs only a C compiler
    ./docker-build.sh test          # or run them in a container instead

It needs nothing but a C compiler: no BlocksDS, no DS. The sources under test
are compiled natively against a small stand-in for `<nds.h>` and run under
AddressSanitizer and UndefinedBehaviorSanitizer. The framework is
[Unity](https://github.com/ThrowTheSwitch/Unity), vendored under
`tests/unity/`.

Note that the Docker route does *not* use the BlocksDS image: these tests are
not cross compiled, so what they need is a host compiler, which a cross
toolchain does not carry. It pulls a plain Ubuntu image instead; set
`TEST_IMAGE` to use your own. Arguments are passed through to make, so
`./docker-build.sh test SANITIZE=0` works.

See `tests/README.md` for what is covered, what is not (anything touching the
hardware, and the hand written ARM assembly), and how to add a suite.

Documentation
-------------

The sources carry Doxygen comments: a file-level overview at the top of every
file explaining what it does and how it fits together, plus documentation for
each public type and function in the headers. If you are new to the codebase,
the umbrella headers are the place to start — they lay out each half of the
program in dependency order:

- `common/include/PIC.h` — how the two CPUs split the work and talk to each other
- `arm7/include/stdafx.h` — the ARM7 physics engine
- `arm9/include/common/general.h` — the ARM9 as a whole
- `arm9/include/game/game_main.h` — playing a test chamber
- `arm9/include/editor/editor_main.h` — the level editor
- `arm9/include/menu/menu_main.h` — the front end

To build a browsable HTML reference from those comments:

    make docs                       # needs doxygen installed locally
    ./docker-build.sh docs          # or run doxygen in a container instead

Either writes `docs/api/html/index.html`. Note that doxygen is *not* part of
the BlocksDS toolchain image, so the Docker route pulls a small Alpine image
and installs it there; set `DOXYGEN_IMAGE` to use your own image instead.

Continuous integration
----------------------

`.github/workflows/ci.yml` runs on every push and pull request. It runs the
unit tests, builds the ROM both ways the README describes
(`./docker-build.sh` and `docker build --output`), and generates the
documentation, failing if doxygen reports any warning outside the third-party
`iniparser`/`dictionary` files. The ROM and the HTML reference are attached to
each run as artifacts.

This is pretty much the game's final version. It's not quite complete feature-wise but I have no plans on continuing it.
The code is provided "as-is" (whatever that entails), and can be freely used so long as it's not for commercial purposes and that proper credit is given to the original author.
I'd love to hear about what people do with this, if anything, so shoot me an email if you feel like using some (or all) of the code ! I'll also try to answer any questions you may have. Here's a short write-up to accompany the source code : https://web.archive.org/web/20140210005514/smealum.net/?page_id=326

Test chamber video : http://www.youtube.com/watch?v=TlXYyjhOYQU

Level editor video : http://www.youtube.com/watch?v=V2OwozMNJFE

Acknowledgements :

- the code for the rigid body dynamics engine is heavily based on chris hecker's articles : http://chrishecker.com/Rigid_Body_Dynamics
- the code for loading ini files was written by N. Devillard
- the code used for level data compression was taken from GRIT
- the code used for loading MD2 models and PCX files was written by David HENRY
- Output function for No$gba debug messages written by Peter Schraut (www.console-dev.de)
- this project uses libnds
- xmem.c was written by SunDEV

Please let me know if you think I missed anything.

=======

Controls

ABXY/D-pad for movement

Touch screen for camera control

Touch screen button is for switching portal color

L/R for shooting portals

This may change. Currently shooting portals can double as a use/gravity gun button.

