/*
 * Shared fixture for the physics suites.
 *
 * The engine is built around file-scope state - the rigid body pool, the
 * rectangle pool, the broadphase grid, the two portals - so a test that leaves
 * any of it dirty silently poisons the next one. physicsReset() puts all of it
 * back to cold-boot condition and every suite calls it from setUp().
 *
 * Include order matters here: unity.h (via f32_test.h) has to come first so
 * that the <math.h> it includes is the system one. The engine headers are then
 * pulled in by relative path, which keeps arm7/include off the include path
 * where it would shadow <math.h> with its own.
 */

#ifndef PORTALDS_TEST_PHYSICS_FIXTURE_H
#define PORTALDS_TEST_PHYSICS_FIXTURE_H

#include "f32_test.h"

#include <nds.h>

#include "../../arm7/include/stdafx.h"

/**
 * Resets every piece of engine state: rigid bodies, rectangles, the broadphase
 * grid, platforms and both portals. Safe to call repeatedly.
 */
void physicsReset(void);

/** Number of fifoSendValue32() calls since the last physicsReset(). */
unsigned fifoCallCount(void);

/**
 * Builds a portal pair facing each other, both marked used, with their guide
 * rectangles generated - the state the engine expects once the player has shot
 * both portals.
 *
 * @param pos0 centre of portal 0, whose normal is @p normal0.
 * @param pos1 centre of portal 1, whose normal is @p normal1.
 */
void makePortalPair(vect3D pos0, vect3D normal0, vect3D pos1, vect3D normal1);

/** Half extents used by most tests: a 1x1x1 unit cube. */
#define TEST_CUBE_SIZE vect(inttof32(1), inttof32(1), inttof32(1))

/** Mass used by most tests. */
#define TEST_CUBE_MASS inttof32(1)

#endif
