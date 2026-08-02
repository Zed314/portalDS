/*
 * Unit tests for the solver itself - impulse response, integration, the sleep
 * heuristic and portal transport in arm7/source/OBB.c.
 *
 * A fixed point impulse solver has no closed form to compare against, so these
 * assert the properties that have to hold whatever the arithmetic does:
 *
 *  - an impulse against a surface removes velocity into it and never adds
 *    energy, because the coefficient of restitution is below 1;
 *  - a body resting on a floor stays on the floor, however many frames pass -
 *    the failure this guards against is a cube sinking through the world;
 *  - the orientation matrix stays a rotation forever, which is only true
 *    because fixMatrix() re-orthonormalises it every step;
 *  - a body that stops moving eventually sleeps, and one that is disturbed
 *    wakes up.
 *
 * updateOBBs() is the real per-frame entry point, so most of these drive it
 * rather than poking the internals.
 */

#include <stdlib.h>
#include <string.h>

#include "../host/physics_fixture.h"

/* Defined in OBB.c, not declared in OBB.h. */
void applyOBBImpulsePlane(OBB_struct *o, u8 pID);
void checkOBBCollisions(OBB_struct *o, bool sleep);

void setUp(void) { physicsReset(); }
void tearDown(void) { physicsReset(); }

static OBB_struct *makeCube(vect3D pos)
{
    return createOBB(0, TEST_CUBE_SIZE, pos, TEST_CUBE_MASS, inttof32(1), 0);
}

/* A wide floor at y=0, plus a grid so the broadphase can find it. */
static void makeGround(void)
{
    const int32 h = inttof32(16);
    createAAR(0, vect(-h, 0, -h), vect(h * 2, 0, h * 2), vect(0, ONE, 0));
    generateGrid(NULL);
}

/* Puts one upward contact under a body, as if it were resting on a floor. */
static void addFloorContact(OBB_struct *o, vect3D point)
{
    o->contactPoints[o->numContactPoints].point = point;
    o->contactPoints[o->numContactPoints].normal = vect(0, ONE, 0);
    o->contactPoints[o->numContactPoints].penetration = 0;
    o->contactPoints[o->numContactPoints].target = NULL;
    o->contactPoints[o->numContactPoints].type = AARCOLLISION;
    o->numContactPoints++;
}

/* --- impulse response ----------------------------------------------------- */

static void test_an_impulse_reverses_motion_into_the_surface(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));
    o->velocity = vect(0, -inttof32(4), 0); /* falling */
    o->numContactPoints = 0;
    addFloorContact(o, vect(0, -inttof32(1), 0));

    applyOBBImpulsePlane(o, 0);

    /* It must no longer be heading into the floor. */
    TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, o->velocity.y);
}

static void test_an_impulse_does_not_add_energy(void)
{
    /* Restitution is 0.2, so a bounce comes back much slower than it arrived.
     * If this ever exceeds the incoming speed the simulation gains energy and
     * a stack of cubes explodes. */
    OBB_struct *o = makeCube(vect(0, 0, 0));
    const int32 incoming = inttof32(4);
    o->velocity = vect(0, -incoming, 0);
    o->numContactPoints = 0;
    addFloorContact(o, vect(0, -inttof32(1), 0));

    applyOBBImpulsePlane(o, 0);

    TEST_ASSERT_LESS_THAN_INT32(incoming, abs(o->velocity.y));
}

static void test_an_impulse_ignores_a_body_already_moving_away(void)
{
    /* iN is clamped at zero, so a contact that is already separating must not
     * yank the body back down. */
    OBB_struct *o = makeCube(vect(0, 0, 0));
    o->velocity = vect(0, inttof32(3), 0); /* rising */
    o->numContactPoints = 0;
    addFloorContact(o, vect(0, -inttof32(1), 0));

    applyOBBImpulsePlane(o, 0);

    ASSERT_F32_WITHIN(TOL_FEW, inttof32(3), o->velocity.y);
}

static void test_an_off_centre_impulse_induces_spin(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));
    o->velocity = vect(0, -inttof32(4), 0);
    o->numContactPoints = 0;
    /* Contact under one corner rather than the centre. */
    addFloorContact(o, vect(inttof32(1), -inttof32(1), 0));

    applyOBBImpulsePlane(o, 0);

    TEST_ASSERT_NOT_EQUAL_INT32(0, o->angularMomentum.z);
}

static void test_a_centred_impulse_induces_no_spin(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));
    o->velocity = vect(0, -inttof32(4), 0);
    o->numContactPoints = 0;
    addFloorContact(o, vect(0, -inttof32(1), 0));

    applyOBBImpulsePlane(o, 0);

    ASSERT_VECT_WITHIN(TOL_FEW, 0, 0, 0, o->angularMomentum);
}

static void test_impulse_helpers_tolerate_null_and_bad_indices(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));
    o->numContactPoints = 0;

    applyOBBImpulsePlane(NULL, 0);
    applyOBBImpulsePlane(o, 0);   /* index past numContactPoints */
    applyOBBImpulses(NULL);
    applyOBBImpulses(o);          /* no contacts at all */
}

/* --- integration ---------------------------------------------------------- */

static void test_a_free_body_falls(void)
{
    OBB_struct *o = makeCube(vect(0, inttof32(50), 0));
    const int32 startY = o->position.y;

    for (int i = 0; i < 10; i++)
        updateOBBs();

    TEST_ASSERT_LESS_THAN_INT32(startY, o->position.y);
    TEST_ASSERT_LESS_THAN_INT32(0, o->velocity.y);
}

static void test_a_free_body_accelerates(void)
{
    /* Gravity is a constant force, so speed downwards has to keep growing. */
    OBB_struct *o = makeCube(vect(0, inttof32(50), 0));

    updateOBBs();
    const int32 afterOne = o->velocity.y;
    for (int i = 0; i < 5; i++)
        updateOBBs();

    TEST_ASSERT_LESS_THAN_INT32(afterOne, o->velocity.y);
}

static void test_a_body_does_not_drift_sideways_under_gravity(void)
{
    OBB_struct *o = makeCube(vect(0, inttof32(50), 0));

    for (int i = 0; i < 20; i++)
        updateOBBs();

    ASSERT_F32_WITHIN(TOL_FEW, 0, o->position.x);
    ASSERT_F32_WITHIN(TOL_FEW, 0, o->position.z);
}

static void test_a_body_resting_on_the_floor_does_not_fall_through(void)
{
    /*
     * The headline property of the whole engine. A cube is parked just above
     * the floor and left to settle for a couple of seconds of frames; it must
     * come to rest on the surface and stay there. A regression in contact
     * generation, in the impulse, or in the timestep bisection all show up
     * here as the box quietly leaving through the floor.
     */
    makeGround();
    OBB_struct *o = makeCube(vect(0, F32(1.2), 0));

    for (int frame = 0; frame < 120; frame++)
    {
        updateOBBs();
        /* Half height is 1, so the centre may not sink far below y=1. */
        TEST_ASSERT_GREATER_THAN_INT32_MESSAGE(F32(0.5), o->position.y,
                                               "body sank into the floor");
    }

    /* And it should have settled rather than still be bouncing. */
    TEST_ASSERT_LESS_THAN_INT32(F32(1.5), o->position.y);
}

static void test_a_dropped_body_lands_and_stays(void)
{
    /* Same again, but with a real drop first so the solver has to absorb a
     * genuine impact rather than a body already in contact. Gravity here is
     * gentle - a two unit fall takes a few hundred frames - so this runs long
     * enough for the bounce to damp out and the body to settle. */
    makeGround();
    OBB_struct *o = makeCube(vect(0, inttof32(3), 0));

    for (int frame = 0; frame < 600; frame++)
        updateOBBs();

    /* Resting on the floor: half height is 1, and the solver allows a little
     * sink, so the centre settles just under y=1. */
    ASSERT_F32_WITHIN(F32(0.15), inttof32(1), o->position.y);
    TEST_ASSERT_TRUE_MESSAGE(o->sleep, "a landed body never settled");
}

static void test_orientation_stays_a_rotation_over_many_frames(void)
{
    /*
     * Integrating an orientation matrix makes it drift away from orthonormal;
     * fixMatrix() is what pulls it back every step. Without it the columns
     * shear and boxes visibly distort, so this spins a body hard and checks
     * the matrix is still a rotation at the end.
     */
    makeGround();
    OBB_struct *o = makeCube(vect(0, inttof32(4), 0));
    o->angularMomentum = vect(inttof32(2), inttof32(3), -inttof32(1));

    for (int frame = 0; frame < 200; frame++)
        updateOBBs();

    const int32 *m = o->transformationMatrix;
    vect3D col[3] = {
        vect(m[0], m[3], m[6]),
        vect(m[1], m[4], m[7]),
        vect(m[2], m[5], m[8]),
    };
    for (int i = 0; i < 3; i++)
    {
        ASSERT_F32_WITHIN(TOL_COARSE, ONE, dotProduct(col[i], col[i]));
        for (int j = i + 1; j < 3; j++)
            ASSERT_F32_WITHIN(TOL_COARSE, 0, dotProduct(col[i], col[j]));
    }
}

static void test_several_bodies_can_be_simulated_at_once(void)
{
    /* The pool is walked by updateOBBs; make sure a full one steps cleanly and
     * every body stays supported rather than leaking through the floor. */
    makeGround();
    for (int i = 0; i < NUMOBJECTS; i++)
        createOBB(i, TEST_CUBE_SIZE, vect(inttof32(i * 4 - 12), inttof32(4), 0),
                  TEST_CUBE_MASS, inttof32(1), 0);

    for (int frame = 0; frame < 60; frame++)
        updateOBBs();

    for (int i = 0; i < NUMOBJECTS; i++)
    {
        TEST_ASSERT_TRUE(objects[i].used);
        TEST_ASSERT_GREATER_THAN_INT32(F32(0.5), objects[i].position.y);
    }
}

/* --- sleeping ------------------------------------------------------------- */

static void test_a_settled_body_falls_asleep(void)
{
    /* Sleeping is what makes a room full of cubes affordable on the ARM7. A
     * body resting on the floor has to reach it, or the engine burns its
     * budget simulating boxes that are not moving. */
    makeGround();
    OBB_struct *o = makeCube(vect(0, F32(1.05), 0));

    for (int frame = 0; frame < SLEEPTIMETHRESHOLD * 6; frame++)
        updateOBBs();

    TEST_ASSERT_TRUE_MESSAGE(o->sleep, "a body at rest never went to sleep");
}

static void test_a_moving_body_stays_awake(void)
{
    OBB_struct *o = makeCube(vect(0, inttof32(200), 0));

    for (int frame = 0; frame < SLEEPTIMETHRESHOLD * 2; frame++)
        updateOBBs();

    TEST_ASSERT_FALSE_MESSAGE(o->sleep, "a falling body went to sleep");
}

static void test_waking_a_sleeping_body_resumes_simulation(void)
{
    /* wakeOBBs runs whenever a portal moves - a sleeping box may suddenly have
     * nothing under it. */
    makeGround();
    OBB_struct *o = makeCube(vect(0, F32(1.05), 0));
    for (int frame = 0; frame < SLEEPTIMETHRESHOLD * 6; frame++)
        updateOBBs();
    TEST_ASSERT_TRUE(o->sleep);

    wakeOBBs();
    TEST_ASSERT_FALSE(o->sleep);

    const int32 restY = o->position.y;
    for (int frame = 0; frame < 10; frame++)
        updateOBBs();

    /* Awake and still supported: it neither froze nor fell through. */
    TEST_ASSERT_GREATER_THAN_INT32(F32(0.5), o->position.y);
    ASSERT_F32_WITHIN(F32(0.5), restY, o->position.y);
}

/* --- portals -------------------------------------------------------------- */

static void test_a_body_crossing_a_portal_comes_out_of_the_other(void)
{
    /*
     * Both portals face up, one at the origin and one far away. A body
     * dropping through the first has to reappear at the second.
     */
    makePortalPair(vect(0, 0, 0), vect(0, ONE, 0),
                   vect(inttof32(100), 0, 0), vect(0, ONE, 0));

    OBB_struct *o = makeCube(vect(0, F32(0.4), 0));
    o->velocity = vect(0, -inttof32(2), 0);
    updateOBBPortals(o, 0, true); /* record the starting side */

    /* Move it through to the back side and re-test. */
    o->position = vect(0, -F32(0.05), 0);
    updateOBBPortals(o, 0, false);

    TEST_ASSERT_TRUE_MESSAGE(o->portaled, "body did not teleport");
    /* It should now be near the far portal, not the near one. */
    ASSERT_F32_WITHIN(inttof32(2), inttof32(100), o->position.x);
}

static void test_a_body_away_from_the_portal_is_not_teleported(void)
{
    makePortalPair(vect(0, 0, 0), vect(0, ONE, 0),
                   vect(inttof32(100), 0, 0), vect(0, ONE, 0));

    OBB_struct *o = makeCube(vect(inttof32(30), F32(0.4), 0));
    updateOBBPortals(o, 0, true);

    o->position = vect(inttof32(30), -F32(0.05), 0);
    updateOBBPortals(o, 0, false);

    TEST_ASSERT_FALSE(o->portaled);
    ASSERT_F32_WITHIN(TOL_BIT, inttof32(30), o->position.x);
}

static void test_portal_transport_preserves_speed(void)
{
    /* warpVector rotates velocity into the far portal's frame; the magnitude
     * has to survive, or cubes would gain or lose energy every trip. */
    makePortalPair(vect(0, 0, 0), vect(0, ONE, 0),
                   vect(inttof32(100), 0, 0), vect(0, ONE, 0));

    OBB_struct *o = makeCube(vect(0, F32(0.4), 0));
    o->velocity = vect(0, -inttof32(3), 0);
    const int32 speedBefore = magnitude(o->velocity);
    updateOBBPortals(o, 0, true);

    o->position = vect(0, -F32(0.05), 0);
    updateOBBPortals(o, 0, false);

    TEST_ASSERT_TRUE(o->portaled);
    ASSERT_F32_WITHIN(TOL_COARSE, speedBefore, magnitude(o->velocity));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_an_impulse_reverses_motion_into_the_surface);
    RUN_TEST(test_an_impulse_does_not_add_energy);
    RUN_TEST(test_an_impulse_ignores_a_body_already_moving_away);
    RUN_TEST(test_an_off_centre_impulse_induces_spin);
    RUN_TEST(test_a_centred_impulse_induces_no_spin);
    RUN_TEST(test_impulse_helpers_tolerate_null_and_bad_indices);

    RUN_TEST(test_a_free_body_falls);
    RUN_TEST(test_a_free_body_accelerates);
    RUN_TEST(test_a_body_does_not_drift_sideways_under_gravity);
    RUN_TEST(test_a_body_resting_on_the_floor_does_not_fall_through);
    RUN_TEST(test_a_dropped_body_lands_and_stays);
    RUN_TEST(test_orientation_stays_a_rotation_over_many_frames);
    RUN_TEST(test_several_bodies_can_be_simulated_at_once);

    RUN_TEST(test_a_settled_body_falls_asleep);
    RUN_TEST(test_a_moving_body_stays_awake);
    RUN_TEST(test_waking_a_sleeping_body_resumes_simulation);

    RUN_TEST(test_a_body_crossing_a_portal_comes_out_of_the_other);
    RUN_TEST(test_a_body_away_from_the_portal_is_not_teleported);
    RUN_TEST(test_portal_transport_preserves_speed);

    return UNITY_END();
}
