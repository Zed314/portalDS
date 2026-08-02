/*
 * Host stand-in for arm9/include/common/general.h, used only by the unit
 * tests.
 *
 * The real umbrella header pulls in the renderer, NitroFS, the texture
 * manager and every other ARM9 subsystem, none of which can exist on the
 * host. The sources that are worth testing on the host - compression.c so far
 * - only ever wanted the integer types and the C library out of it, so this
 * file provides that much. tests/Makefile puts tests/host ahead of
 * arm9/include, so this shadows the real header for the test build only.
 */

#ifndef PORTALDS_TEST_HOST_GENERAL_H
#define PORTALDS_TEST_HOST_GENERAL_H

#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same definitions as arm9/include/common/math.h. */
#define min(a,b) (((a)>(b))?(b):(a))
#define max(a,b) (((a)>(b))?(a):(b))

/* On the DS this writes to the no$gba debug console. There is nothing to
 * write to here, and a test run should not be printing anyway. */
#define NOGBA(_fmt, _args...) ((void)0)

#endif
