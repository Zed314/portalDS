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
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;

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

#endif
