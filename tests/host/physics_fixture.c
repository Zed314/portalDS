/*
 * Host environment for the physics suites: the globals the engine expects the
 * FIFO layer to own, plus the reset helper described in physics_fixture.h.
 *
 * portal[2] normally lives in PI7.c alongside the FIFO command decoder. That
 * file is the boundary with the other CPU and has no business in a host test,
 * so the storage is declared here instead and the tests set it up directly -
 * which is what the FIFO would have done anyway.
 */

#include "physics_fixture.h"

#include <string.h>

portal_struct portal[2];

static unsigned fifoCalls;

void fifoSendValue32(u32 channel, u32 value)
{
    (void)channel;
    (void)value;
    fifoCalls++;
}

unsigned fifoCallCount(void)
{
    return fifoCalls;
}

void physicsReset(void)
{
    /* Order matters: the rectangle pool owns the broadphase grid, and
     * initAARs drops it, so bodies must stop pointing into it first. */
    initOBBs();
    initAARs();
    initPlatforms();

    memset(portal, 0, sizeof(portal));
    portal[0].targetPortal = &portal[1];
    portal[1].targetPortal = &portal[0];

    fifoCalls = 0;
}

void makePortalPair(vect3D pos0, vect3D normal0, vect3D pos1, vect3D normal1)
{
    portal[0].position = pos0;
    portal[0].normal = normal0;
    portal[1].position = pos1;
    portal[1].normal = normal1;

    /* plane[0] is any unit tangent; the engine derives plane[1] from it.
     * Pick one that is not parallel to the normal. */
    portal[0].plane[0] = normal0.y ? vect(inttof32(1), 0, 0) : vect(0, inttof32(1), 0);
    portal[1].plane[0] = normal1.y ? vect(inttof32(1), 0, 0) : vect(0, inttof32(1), 0);

    portal[0].used = true;
    portal[1].used = true;
    portal[0].targetPortal = &portal[1];
    portal[1].targetPortal = &portal[0];

    /* Completes plane[1] and regenerates the guide rectangles. */
    computePortalPlane(&portal[0]);
    computePortalPlane(&portal[1]);
}
