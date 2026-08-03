/*
 * Host stand-in for libnds' <nds.h>, used only by the unit tests.
 *
 * The tests compile the game's own sources with the host compiler so the
 * fixed point maths and the level data codec can be exercised without a DS.
 * Those sources include <nds.h> for two things only: libnds' legacy integer
 * typedefs, and the section attributes. Neither needs any hardware, so this
 * header supplies both and nothing else; tests/Makefile puts this directory
 * ahead of the real toolchain on the include path.
 *
 * If a source file you want to test starts needing more than this, that is a
 * signal it is touching hardware and does not belong in a host test.
 */

#ifndef PORTALDS_TEST_HOST_NDS_H
#define PORTALDS_TEST_HOST_NDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* libnds' legacy fixed width names (ndstypes.h). */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

/* 20.12 fixed point. The whole engine is built on this type. */
typedef int32_t f32;

/*
 * Placement attributes. On the DS these move code into instruction TCM or
 * force the ARM instruction set; on the host they are meaningless, and the
 * generated code is the same either way.
 */
#define ARM_CODE
#define THUMB_CODE
#define ITCM_CODE
#define DTCM_DATA
#define DTCM_BSS

/*
 * The FIFO link to the other CPU. The physics engine only ever uses it to
 * report its allocator high water mark, which is telemetry rather than
 * behaviour, so the test build gets a no-op that counts the calls. Anything
 * that needs a real FIFO does not belong in a host test.
 */
#define FIFO_USER_08 8

void fifoSendValue32(u32 channel, u32 value);

/*
 * The ARM9 half of the shim, used only by test_levelfile.
 *
 * The ARM7 sources need nothing past this point, and must not see it: their
 * own arm7/include/math.h defines inttof32 and friends, and two definitions
 * would collide. tests/Makefile passes -DTEST_ARM9_FULL only to the ARM9
 * build, which is also what selects the full umbrella in common/general.h.
 *
 * What is here is far less than you would expect, because the level reader
 * touches no hardware - it is just that game_main.h drags in headers that
 * mention these types in prototypes and in a handful of inline functions. So
 * this is a vocabulary, not an emulation. The one exception is the fixed
 * point arithmetic below, which is real: it is the same 20.12 the DS does, so
 * that anything which does compute a coordinate computes the right one.
 */
#ifdef TEST_ARM9_FULL

/* 20.12 fixed point, matching libnds' arm9/math.h. */
#define inttof32(n)   ((n) << 12)
#define f32toint(n)   ((n) >> 12)
#define floattof32(n) ((int32)((n) * (1 << 12)))
#define inttot16(n)   ((n) << 4)
#define RGB15(r,g,b)  ((r)|((g)<<5)|((b)<<10))

static inline int32 mulf32(int32 a, int32 b)
{
	return (int32)(((int64)a * (int64)b) >> 12);
}

static inline int32 divf32(int32 a, int32 b)
{
	if(!b) return 0; /* the hardware divider returns garbage; do not trap the test */
	return (int32)((((int64)a) << 12) / b);
}

static inline u32 sqrt64(u64 a)
{
	u64 r = 0, b = 1ULL << 62;
	while(b > a) b >>= 2;
	while(b)
	{
		if(a >= r + b) { a -= r + b; r = (r >> 1) + b; }
		else r >>= 1;
		b >>= 2;
	}
	return (u32)r;
}

/* Both operate on int32[3] in place / into a third, as libnds does.
 *
 * normalizef32 takes void* rather than libnds' int32*, because normalize() in
 * common/math.h calls it with a vect3D* down one branch and an int32(*)[3]
 * down the other. Neither matches int32*; the ARM9 build has been quietly
 * warning about that for years. Declaring it laxer here keeps the test build
 * clean without editing game code to suit the tests. */
void normalizef32(void* a);
void crossf32(int32* a, int32* b, int32* result);

/* Screen fades. Real on the DS, nothing here - see common/math.h fadeIn(). */
void setBrightness(int screen, int level);
void swiWaitForVBlank(void);

typedef int16_t v16;  /* 4.12 vertex coordinate */
typedef int16_t t16;  /* 12.4 texture coordinate */
typedef uint16_t rgb; /* 15 bit BGR colour */

typedef enum {
	GL_RGB4=1, GL_RGB16, GL_RGB256, GL_RGB32_A3, GL_RGB8_A5, GL_RGB, GL_RGBA
} GL_TEXTURE_TYPE_ENUM;

typedef enum { SoundFormat_16Bit, SoundFormat_8Bit, SoundFormat_ADPCM } SoundFormat;

typedef struct { u16 rawx, rawy, px, py, z1, z2; } touchPosition;

/*
 * Geometry engine registers. On the DS these are volatile writes to 0x4000000
 * and up; here they are plain variables, so the inline setters in textures.h
 * and the odd GFX_COLOR assignment compile and go nowhere.
 */
extern u32 GFX_PAL_FORMAT, GFX_TEX_FORMAT, GFX_COLOR;

#define POLY_ALPHA(n)   ((n)<<16)
#define POLY_CULL_BACK  0x40
#define POLY_CULL_FRONT 0x80
#define POLY_CULL_NONE  0xC0

void glPolyFmt(u32 params);

#endif /* TEST_ARM9_FULL */

#endif
