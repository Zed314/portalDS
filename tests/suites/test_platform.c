/*
 * Unit tests for moving platforms - arm7/source/platform.c.
 *
 * A platform is a small upward-facing rectangle that travels along a fixed run
 * and carries whatever is standing on it. Two behaviours matter and both are
 * easy to get subtly wrong:
 *
 *  - arrival. updatePlatform() decides it has reached an end by testing the
 *    sign of the dot product between the remaining distance and the velocity,
 *    so it works whichever way the run points - but it also means a platform
 *    that never quite overshoots would run forever;
 *  - carrying. A body resting on an active platform is displaced by the
 *    platform's own motion before its physics runs, which is what makes it
 *    ride along instead of being left behind or flung by an impulse.
 *
 * Note the scale: PLATFORMSIZE puts the collision surface at 0.75 units either
 * side of the centre, so the cubes here are deliberately smaller than the ones
 * in the other suites. A one unit cube's vertical edges fall outside the
 * platform entirely and it would rest on nothing.
 */

#include <stdlib.h>
#include <string.h>

#include "../host/physics_fixture.h"

/* Defined in platform.c; platform.h only declares the plural form. */
void updatePlatform(platform_struct *pf);

void setUp(void) { physicsReset(); }
void tearDown(void) { physicsReset(); }

/* Half extents that actually fit on a platform. */
#define SMALL_CUBE vect(F32(0.5), F32(0.5), F32(0.5))

/* The collision surface is centred on the platform and this far out. */
#define PLATFORM_HALF (PLATFORMSIZE * 4)

static OBB_struct *makeSmallCube(vect3D pos)
{
    return createOBB(0, SMALL_CUBE, pos, inttof32(1), inttof32(1), 0);
}

/* Runs one body against every platform, as AARsOBBContacts would. */
static void collideWithPlatforms(OBB_struct *o)
{
    vect3D v[8];
    o->numContactPoints = 0;
    getOBBVertices(o, v);
    collideOBBPlatforms(o, v);
}

/* --- creation ------------------------------------------------------------- */

static void test_init_frees_every_slot(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(1), 0), false);
    createPlatform(3, vect(0, 0, 0), vect(0, inttof32(1), 0), false);
    TEST_ASSERT_TRUE(platform[0].used);
    TEST_ASSERT_TRUE(platform[3].used);

    initPlatforms();

    for (int i = 0; i < NUMPLATFORMS; i++)
        TEST_ASSERT_FALSE(platform[i].used);
}

static void test_a_new_platform_starts_parked_at_its_origin(void)
{
    const vect3D orig = vect(inttof32(2), inttof32(1), inttof32(3));
    const vect3D dest = vect(inttof32(2), inttof32(4), inttof32(3));

    createPlatform(0, orig, dest, false);

    TEST_ASSERT_TRUE(platform[0].used);
    TEST_ASSERT_FALSE_MESSAGE(platform[0].active, "a new platform should not be moving");
    TEST_ASSERT_TRUE(platform[0].direction);
    ASSERT_VECT_WITHIN(0, orig.x, orig.y, orig.z, platform[0].position);
    ASSERT_VECT_WITHIN(0, orig.x, orig.y, orig.z, platform[0].origin);
    ASSERT_VECT_WITHIN(0, dest.x, dest.y, dest.z, platform[0].destination);
}

static void test_the_collision_surface_is_centred_on_the_platform(void)
{
    const vect3D orig = vect(inttof32(2), inttof32(1), inttof32(3));

    createPlatform(0, orig, vect(inttof32(2), inttof32(4), inttof32(3)), false);

    TEST_ASSERT_TRUE(platform[0].AAR.used);
    /* Minimum corner is half a platform back on x and z, level on y. */
    ASSERT_VECT_WITHIN(0, orig.x - PLATFORM_HALF, orig.y, orig.z - PLATFORM_HALF,
                       platform[0].AAR.position);
    ASSERT_VECT_WITHIN(0, PLATFORM_HALF * 2, 0, PLATFORM_HALF * 2, platform[0].AAR.size);
    /* Flat and facing up, or nothing would ever stand on it. */
    ASSERT_VECT_WITHIN(0, 0, ONE, 0, platform[0].AAR.normal);
}

static void test_velocity_points_from_origin_to_destination(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);
    TEST_ASSERT_GREATER_THAN_INT32(0, platform[0].velocity.y);
    TEST_ASSERT_EQUAL_INT32(0, platform[0].velocity.x);

    createPlatform(1, vect(0, inttof32(4), 0), vect(0, 0, 0), false);
    TEST_ASSERT_LESS_THAN_INT32(0, platform[1].velocity.y);
}

/* --- movement ------------------------------------------------------------- */

static void test_an_inactive_platform_does_not_move(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);
    const vect3D before = platform[0].position;

    for (int i = 0; i < 50; i++)
        updatePlatforms();

    ASSERT_VECT_WITHIN(0, before.x, before.y, before.z, platform[0].position);
}

static void test_an_active_platform_travels_towards_its_destination(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);
    togglePlatform(0, true);

    for (int i = 0; i < 50; i++)
        updatePlatforms();

    TEST_ASSERT_GREATER_THAN_INT32(0, platform[0].position.y);
    TEST_ASSERT_LESS_THAN_INT32(inttof32(4), platform[0].position.y);
}

static void test_a_one_shot_platform_stops_on_arrival(void)
{
    const vect3D dest = vect(0, F32(0.25), 0);
    createPlatform(0, vect(0, 0, 0), dest, false);
    togglePlatform(0, true);

    for (int i = 0; i < 400; i++)
        updatePlatforms();

    TEST_ASSERT_FALSE_MESSAGE(platform[0].active, "a one shot platform should stop");
    ASSERT_VECT_WITHIN(0, 0, 0, 0, platform[0].velocity);
    /* Past the destination, never short of it - see the overshoot test below. */
    TEST_ASSERT_GREATER_OR_EQUAL_INT32_MESSAGE(dest.y, platform[0].position.y,
                                               "should not stop short of the destination");
}

static void test_arrival_overshoots_because_the_dot_product_truncates(void)
{
    /*
     * Documents a real inaccuracy rather than hiding it in a loose tolerance.
     *
     * Arrival is detected with dotProduct(position - destination, velocity) > 0,
     * and dotProduct shifts its 64 bit product down by 12. A platform travels at
     * 8 f32 units per step, so that product only reaches 1 once the remaining
     * distance is 512 units - which means the platform sails a full 512 units
     * (0.125 world units) past its destination before it notices, every time,
     * on every run.
     *
     * That is about a sixth of the platform's own half width. It is pinned here
     * rather than fixed because fixing it moves where every platform in every
     * shipped level comes to rest, which is a level design decision and not a
     * test's to make.
     */
    const vect3D dest = vect(0, F32(0.25), 0);
    createPlatform(0, vect(0, 0, 0), dest, false);
    togglePlatform(0, true);

    int steps = 0;
    while (platform[0].active && steps < 2000)
    {
        updatePlatforms();
        steps++;
    }

    TEST_ASSERT_FALSE_MESSAGE(platform[0].active, "the platform never arrived");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(512, platform[0].position.y - dest.y,
                                    "overshoot changed - see the comment above");
}

static void test_a_back_and_forth_platform_reverses_at_the_far_end(void)
{
    const vect3D dest = vect(0, F32(0.25), 0);
    createPlatform(0, vect(0, 0, 0), dest, true);
    togglePlatform(0, true);

    for (int i = 0; i < 400; i++)
        updatePlatforms();

    /* Still going, now heading back. */
    TEST_ASSERT_TRUE_MESSAGE(platform[0].active, "a back and forth platform should keep going");
    TEST_ASSERT_FALSE_MESSAGE(platform[0].direction, "it should have turned around");
    TEST_ASSERT_LESS_THAN_INT32(0, platform[0].velocity.y);
}

static void test_a_back_and_forth_platform_completes_a_full_cycle(void)
{
    const vect3D orig = vect(0, 0, 0);
    createPlatform(0, orig, vect(0, F32(0.25), 0), true);
    togglePlatform(0, true);

    /* Run until it has turned around twice rather than guessing a step count -
     * the leg length depends on the overshoot documented above, so a hard coded
     * count would just be re-encoding that quirk in a second place. */
    int flips = 0;
    bool wasOutbound = platform[0].direction;
    for (int i = 0; i < 4000 && flips < 2; i++)
    {
        updatePlatforms();
        if (platform[0].direction != wasOutbound)
        {
            flips++;
            wasOutbound = platform[0].direction;
        }
    }

    TEST_ASSERT_EQUAL_INT_MESSAGE(2, flips, "the platform never completed a cycle");
    TEST_ASSERT_TRUE(platform[0].active);
    TEST_ASSERT_TRUE_MESSAGE(platform[0].direction, "it should be outbound again");
    /* It turns for home just past the origin, by the same overshoot. */
    TEST_ASSERT_INT32_WITHIN_MESSAGE(F32(0.2), orig.y, platform[0].position.y,
                                     "back near the origin");
}

static void test_the_surface_follows_the_platform_while_it_moves(void)
{
    /* If the rectangle ever lagged behind the platform, bodies would ride an
     * invisible surface somewhere else. */
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), true);
    togglePlatform(0, true);

    for (int i = 0; i < 100; i++)
    {
        updatePlatforms();
        ASSERT_F32_WITHIN(0, platform[0].position.x - PLATFORM_HALF, platform[0].AAR.position.x);
        ASSERT_F32_WITHIN(0, platform[0].position.y, platform[0].AAR.position.y);
        ASSERT_F32_WITHIN(0, platform[0].position.z - PLATFORM_HALF, platform[0].AAR.position.z);
    }
}

static void test_moving_a_platform_takes_its_surface_along(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);

    const vect3D to = vect(inttof32(9), inttof32(2), -inttof32(5));
    movePlatform(0, to);

    ASSERT_VECT_WITHIN(0, to.x, to.y, to.z, platform[0].position);
    ASSERT_VECT_WITHIN(0, to.x - PLATFORM_HALF, to.y, to.z - PLATFORM_HALF,
                       platform[0].AAR.position);
}

static void test_out_of_range_ids_are_ignored(void)
{
    /*
     * movePlatform and togglePlatform take an id straight off the FIFO, so the
     * bounds check is the only thing between a bad word and a stray write.
     *
     * createPlatform has no such check - it indexes the pool with whatever it
     * is given - and is not called here for that reason. It is safe only
     * because its one caller, the PI_ADDPLATFORM case in PI7.c, passes
     * id % NUMPLATFORMS. A second caller that forgets would corrupt memory,
     * which is worth knowing but is not something a test can assert.
     */
    movePlatform(NUMPLATFORMS, vect(inttof32(1), inttof32(1), inttof32(1)));
    movePlatform(200, vect(inttof32(1), inttof32(1), inttof32(1)));
    togglePlatform(NUMPLATFORMS, true);
    togglePlatform(200, true);

    for (int i = 0; i < NUMPLATFORMS; i++)
        TEST_ASSERT_FALSE(platform[i].used);
}

static void test_update_tolerates_null(void)
{
    updatePlatform(NULL);
    collideOBBPlatforms(NULL, NULL);
}

/* --- carrying bodies ------------------------------------------------------ */

static void test_a_body_on_an_active_platform_is_carried(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);
    togglePlatform(0, true);

    /* Straddling the platform surface at y=0. */
    OBB_struct *o = makeSmallCube(vect(0, F32(0.25), 0));
    const s32 before = o->position.y;

    collideWithPlatforms(o);

    TEST_ASSERT_INT32_WITHIN_MESSAGE(2, before + platform[0].velocity.y, o->position.y,
                                     "displaced by the platform's velocity");
}

static void test_carrying_wakes_a_sleeping_body(void)
{
    /* A body asleep on a platform that starts moving has to wake, or it would
     * hang in the air where the platform left it. */
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);
    togglePlatform(0, true);

    OBB_struct *o = makeSmallCube(vect(0, F32(0.25), 0));
    o->sleep = true;
    o->counter = SLEEPTIMETHRESHOLD;

    collideWithPlatforms(o);

    TEST_ASSERT_FALSE_MESSAGE(o->sleep, "a carried body should be awake");
    TEST_ASSERT_EQUAL_UINT16(0, o->counter);
}

static void test_a_body_on_a_stopped_platform_is_not_carried(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);
    /* Left inactive. */

    OBB_struct *o = makeSmallCube(vect(0, F32(0.25), 0));
    const vect3D before = o->position;

    collideWithPlatforms(o);

    ASSERT_VECT_WITHIN(0, before.x, before.y, before.z, o->position);
    /* It should still be resting on it, though. */
    TEST_ASSERT_GREATER_THAN_UINT8(0, o->numContactPoints);
}

static void test_a_body_beside_the_platform_is_not_carried(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);
    togglePlatform(0, true);

    /* Level with the surface but well outside its footprint. */
    OBB_struct *o = makeSmallCube(vect(inttof32(20), F32(0.25), 0));
    const vect3D before = o->position;

    collideWithPlatforms(o);

    ASSERT_VECT_WITHIN(0, before.x, before.y, before.z, o->position);
    TEST_ASSERT_EQUAL_UINT8(0, o->numContactPoints);
}

static void test_a_body_above_the_platform_is_not_carried(void)
{
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(4), 0), false);
    togglePlatform(0, true);

    OBB_struct *o = makeSmallCube(vect(0, inttof32(6), 0));
    const vect3D before = o->position;

    collideWithPlatforms(o);

    ASSERT_VECT_WITHIN(0, before.x, before.y, before.z, o->position);
    TEST_ASSERT_EQUAL_UINT8(0, o->numContactPoints);
}

static void test_only_live_slots_are_considered(void)
{
    /* Nothing is created, so a body sitting where a platform would be must be
     * left alone. */
    OBB_struct *o = makeSmallCube(vect(0, F32(0.25), 0));
    const vect3D before = o->position;

    collideWithPlatforms(o);

    ASSERT_VECT_WITHIN(0, before.x, before.y, before.z, o->position);
    TEST_ASSERT_EQUAL_UINT8(0, o->numContactPoints);
}

static void test_a_body_rides_a_platform_over_many_steps(void)
{
    /*
     * The whole mechanic end to end: a body left on a rising platform should
     * still be on it after a while, having gone up with it rather than being
     * left behind or shoved off.
     */
    createPlatform(0, vect(0, 0, 0), vect(0, inttof32(2), 0), false);
    togglePlatform(0, true);
    OBB_struct *o = makeSmallCube(vect(0, F32(0.25), 0));

    /* 8 f32 units per step, so 200 steps is a rise of about 0.39 units. */
    for (int i = 0; i < 200; i++)
    {
        updatePlatforms();
        collideWithPlatforms(o);
    }

    TEST_ASSERT_GREATER_THAN_INT32_MESSAGE(F32(0.3), platform[0].position.y,
                                           "the platform should have risen");
    /* The body kept its offset above the surface: it rode up. */
    TEST_ASSERT_INT32_WITHIN_MESSAGE(64, platform[0].position.y + F32(0.25), o->position.y,
                                     "the body should have risen with the platform");
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_frees_every_slot);
    RUN_TEST(test_a_new_platform_starts_parked_at_its_origin);
    RUN_TEST(test_the_collision_surface_is_centred_on_the_platform);
    RUN_TEST(test_velocity_points_from_origin_to_destination);

    RUN_TEST(test_an_inactive_platform_does_not_move);
    RUN_TEST(test_an_active_platform_travels_towards_its_destination);
    RUN_TEST(test_a_one_shot_platform_stops_on_arrival);
    RUN_TEST(test_arrival_overshoots_because_the_dot_product_truncates);
    RUN_TEST(test_a_back_and_forth_platform_reverses_at_the_far_end);
    RUN_TEST(test_a_back_and_forth_platform_completes_a_full_cycle);
    RUN_TEST(test_the_surface_follows_the_platform_while_it_moves);
    RUN_TEST(test_moving_a_platform_takes_its_surface_along);
    RUN_TEST(test_out_of_range_ids_are_ignored);
    RUN_TEST(test_update_tolerates_null);

    RUN_TEST(test_a_body_on_an_active_platform_is_carried);
    RUN_TEST(test_carrying_wakes_a_sleeping_body);
    RUN_TEST(test_a_body_on_a_stopped_platform_is_not_carried);
    RUN_TEST(test_a_body_beside_the_platform_is_not_carried);
    RUN_TEST(test_a_body_above_the_platform_is_not_carried);
    RUN_TEST(test_only_live_slots_are_considered);
    RUN_TEST(test_a_body_rides_a_platform_over_many_steps);

    return UNITY_END();
}
