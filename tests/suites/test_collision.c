/*
 * Unit tests for contact generation - arm7/source/AAR.c and the box-box
 * narrow phase in arm7/source/OBB.c.
 *
 * The static world is a soup of one-sided axis aligned rectangles, binned into
 * a broadphase grid over the XZ plane. Two properties matter most and both are
 * asserted below:
 *
 *  - a rectangle only generates contacts for a body that actually straddles
 *    it, within its extent - too eager and boxes catch on nothing, too lazy
 *    and they fall through the floor;
 *  - the grid is only an optimisation, so gathering contacts through it must
 *    produce exactly what testing every rectangle by brute force would.
 *
 * The second is the one that would catch a broadphase that quietly drops a
 * cell, which is the failure mode you would otherwise only notice as a cube
 * falling through one particular corner of one particular test chamber.
 */

#include <stdlib.h>
#include <string.h>

#include "../host/physics_fixture.h"

/* Defined in AAR.c, used by the engine internally, not declared in AAR.h. */
void getOBBNodes(grid_struct *g, OBB_struct *o, u16 *x, u16 *X, u16 *z, u16 *Z);
void freeGrid(grid_struct *g);

void setUp(void) { physicsReset(); }
void tearDown(void) { physicsReset(); }

/* A floor spanning [-half,half] on x and z, at height y, facing up. */
static AAR_struct *makeFloor(u16 id, int32 y, int32 half)
{
    return createAAR(id, vect(-half, y, -half), vect(half * 2, 0, half * 2), vect(0, ONE, 0));
}

static OBB_struct *makeCube(vect3D pos)
{
    return createOBB(0, TEST_CUBE_SIZE, pos, TEST_CUBE_MASS, inttof32(1), 0);
}

/* --- rectangle pool ------------------------------------------------------- */

static void test_create_and_disable_a_rectangle(void)
{
    AAR_struct *a = makeFloor(0, 0, inttof32(10));

    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_TRUE(a->used);
    ASSERT_VECT_WITHIN(0, -inttof32(10), 0, -inttof32(10), a->position);

    toggleAAR(0);
    TEST_ASSERT_FALSE(a->used);
    toggleAAR(0);
    TEST_ASSERT_TRUE(a->used);
}

static void test_create_does_not_overwrite_a_live_slot(void)
{
    makeFloor(0, 0, inttof32(10));

    /* The slot is taken, so this must be refused rather than silently
     * clobbering geometry the level still references. */
    TEST_ASSERT_NULL(createAAR(0, vect(0, 0, 0), vect(ONE, 0, ONE), vect(0, ONE, 0)));
}

static void test_init_aars_frees_every_slot(void)
{
    makeFloor(0, 0, inttof32(10));
    makeFloor(1, inttof32(4), inttof32(10));

    initAARs();

    TEST_ASSERT_NOT_NULL(makeFloor(0, 0, inttof32(10)));
    TEST_ASSERT_NOT_NULL(makeFloor(1, 0, inttof32(10)));
}

/* --- one rectangle against one body --------------------------------------- */

static u8 contactsAgainst(AAR_struct *a, OBB_struct *o)
{
    vect3D v[8];
    o->numContactPoints = 0;
    getOBBVertices(o, v);
    AAROBBContacts(a, o, v, false);
    return o->numContactPoints;
}

static void test_a_box_straddling_the_floor_generates_contacts(void)
{
    AAR_struct *floor = makeFloor(0, 0, inttof32(10));
    /* Half height is 1, so a centre at y=0.5 leaves the bottom face below the
     * floor plane and the top above it. */
    OBB_struct *o = makeCube(vect(0, F32(0.5), 0));

    u8 n = contactsAgainst(floor, o);

    /* Exactly the four vertical edges cross the plane. */
    TEST_ASSERT_EQUAL_UINT8(4, n);
    for (int i = 0; i < n; i++)
    {
        TEST_ASSERT_EQUAL_INT(AARCOLLISION, o->contactPoints[i].type);
        ASSERT_VECT_WITHIN(0, 0, ONE, 0, o->contactPoints[i].normal);
        TEST_ASSERT_NULL(o->contactPoints[i].target);
        /* Contacts sit on the floor plane. */
        ASSERT_F32_WITHIN(TOL_FEW, 0, o->contactPoints[i].point.y);
    }
}

static void test_a_box_clear_of_the_floor_generates_nothing(void)
{
    AAR_struct *floor = makeFloor(0, 0, inttof32(10));
    OBB_struct *o = makeCube(vect(0, inttof32(5), 0));

    TEST_ASSERT_EQUAL_UINT8(0, contactsAgainst(floor, o));
}

static void test_a_box_below_the_floor_generates_nothing(void)
{
    AAR_struct *floor = makeFloor(0, 0, inttof32(10));
    OBB_struct *o = makeCube(vect(0, -inttof32(5), 0));

    TEST_ASSERT_EQUAL_UINT8(0, contactsAgainst(floor, o));
}

static void test_a_box_straddling_the_plane_but_beyond_the_rectangle(void)
{
    /* This is the one that matters: an infinite plane would catch this box,
     * a rectangle must not. The floor stops at x=10; the box is at x=50. */
    AAR_struct *floor = makeFloor(0, 0, inttof32(10));
    OBB_struct *o = makeCube(vect(inttof32(50), F32(0.5), 0));

    TEST_ASSERT_EQUAL_UINT8(0, contactsAgainst(floor, o));
}

static void test_a_disabled_rectangle_generates_nothing(void)
{
    /* How doors stop colliding when they open. */
    AAR_struct *floor = makeFloor(0, 0, inttof32(10));
    OBB_struct *o = makeCube(vect(0, F32(0.5), 0));
    TEST_ASSERT_GREATER_THAN_UINT8(0, contactsAgainst(floor, o));

    toggleAAR(0);

    TEST_ASSERT_EQUAL_UINT8(0, contactsAgainst(floor, o));
}

static void test_walls_on_every_axis_generate_contacts(void)
{
    /* A rectangle is flat along whichever axis its normal points down, and
     * AAROBBContacts has a separate branch per axis. All three must work. */
    const int32 h = inttof32(10);

    AAR_struct *wallX = createAAR(0, vect(0, -h, -h), vect(0, h * 2, h * 2), vect(ONE, 0, 0));
    AAR_struct *wallZ = createAAR(1, vect(-h, -h, 0), vect(h * 2, h * 2, 0), vect(0, 0, ONE));
    AAR_struct *floor = makeFloor(2, 0, h);

    OBB_struct *o = makeCube(vect(0, 0, 0));

    TEST_ASSERT_EQUAL_UINT8(4, contactsAgainst(wallX, o));
    TEST_ASSERT_EQUAL_UINT8(4, contactsAgainst(wallZ, o));
    TEST_ASSERT_EQUAL_UINT8(4, contactsAgainst(floor, o));
}

static void test_contacts_append_rather_than_replace(void)
{
    /* One body collects contacts from many rectangles in a frame, so each
     * rectangle must add to the list, not reset it. */
    AAR_struct *floor = makeFloor(0, 0, inttof32(10));
    AAR_struct *ceiling = createAAR(1, vect(-inttof32(10), 0, -inttof32(10)),
                                    vect(inttof32(20), 0, inttof32(20)), vect(0, -ONE, 0));
    OBB_struct *o = makeCube(vect(0, F32(0.5), 0));

    vect3D v[8];
    o->numContactPoints = 0;
    getOBBVertices(o, v);
    AAROBBContacts(floor, o, v, false);
    u8 afterFirst = o->numContactPoints;
    AAROBBContacts(ceiling, o, v, false);

    TEST_ASSERT_GREATER_THAN_UINT8(afterFirst, o->numContactPoints);
}

static void test_null_arguments_are_tolerated(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));
    vect3D v[8];
    getOBBVertices(o, v);

    TEST_ASSERT_FALSE(AAROBBContacts(NULL, o, v, false));
    TEST_ASSERT_FALSE(AAROBBContacts(&portal[0].guideAAR[0], NULL, v, false));
    AARsOBBContacts(NULL, false);
}

/* --- broadphase ----------------------------------------------------------- */

/*
 * Builds a grid-sized room: a floor, four walls and a shelf, spread widely
 * enough that the broadphase has several cells to choose between.
 *
 * The rectangle pool is static inside AAR.c, so the only handles on the room's
 * geometry are the pointers createAAR hands back. They are kept here so the
 * brute force comparison below can walk the same rectangles the grid indexes.
 */
#define ROOM_RECTS (6)
static AAR_struct *roomRects[ROOM_RECTS];

static void buildRoom(void)
{
    const int32 h = inttof32(8);
    roomRects[0] = makeFloor(0, 0, h);
    roomRects[1] = createAAR(1, vect(-h, 0, -h), vect(0, h, h * 2), vect(ONE, 0, 0));
    roomRects[2] = createAAR(2, vect(h, 0, -h), vect(0, h, h * 2), vect(-ONE, 0, 0));
    roomRects[3] = createAAR(3, vect(-h, 0, -h), vect(h * 2, h, 0), vect(0, 0, ONE));
    roomRects[4] = createAAR(4, vect(-h, 0, h), vect(h * 2, h, 0), vect(0, 0, -ONE));
    /* A shelf partway up, to give the grid something non-uniform. */
    roomRects[5] = createAAR(5, vect(-inttof32(2), inttof32(3), -inttof32(2)),
                             vect(inttof32(4), 0, inttof32(4)), vect(0, ONE, 0));
    for (int i = 0; i < ROOM_RECTS; i++)
        TEST_ASSERT_NOT_NULL(roomRects[i]);

    generateGrid(NULL);
}

static int compareVects(const void *a, const void *b)
{
    const vect3D *va = a, *vb = b;
    if (va->x != vb->x) return va->x < vb->x ? -1 : 1;
    if (va->y != vb->y) return va->y < vb->y ? -1 : 1;
    if (va->z != vb->z) return va->z < vb->z ? -1 : 1;
    return 0;
}

static void test_grid_finds_the_floor_and_records_it_as_ground(void)
{
    buildRoom();
    OBB_struct *o = makeCube(vect(0, F32(0.5), 0));

    o->numContactPoints = 0;
    AARsOBBContacts(o, false);

    TEST_ASSERT_GREATER_THAN_UINT8(0, o->numContactPoints);
    /* groundID is what the ARM9 uses to make a body ride a moving platform. */
    TEST_ASSERT_EQUAL_INT16(0, o->groundID);
}

static void test_a_box_in_mid_air_rests_on_nothing(void)
{
    buildRoom();
    OBB_struct *o = makeCube(vect(0, inttof32(6), 0));
    vect3D v[8];
    getOBBVertices(o, v);

    o->numContactPoints = 0;
    AARsOBBContacts(o, false);

    TEST_ASSERT_EQUAL_UINT8(0, o->numContactPoints);
    TEST_ASSERT_EQUAL_INT16(-1, o->groundID);
}

static void test_the_grid_agrees_with_brute_force(void)
{
    /*
     * The grid is only ever an optimisation: for any body, gathering contacts
     * through it must give the same set as testing every rectangle. Checked at
     * a spread of positions, including ones straddling cell boundaries, where
     * a broadphase that rounds a cell index the wrong way would drop a
     * rectangle and let a cube through the floor.
     */
    buildRoom();

    const int32 xs[] = {-inttof32(7), -inttof32(4), 0, F32(4.5), inttof32(7)};
    const int32 ys[] = {F32(0.5), inttof32(3), F32(3.5)};

    for (size_t xi = 0; xi < sizeof(xs) / sizeof(xs[0]); xi++)
    {
        for (size_t yi = 0; yi < sizeof(ys) / sizeof(ys[0]); yi++)
        {
            OBB_struct *o = makeCube(vect(xs[xi], ys[yi], xs[xi]));

            /* Through the grid. */
            vect3D v[8];
            o->numContactPoints = 0;
            getOBBVertices(o, v);
            AARsOBBContacts(o, false);
            u8 gridCount = o->numContactPoints;
            vect3D gridPoints[MAXCONTACTPOINTS];
            for (int i = 0; i < gridCount; i++)
                gridPoints[i] = o->contactPoints[i].point;

            /* Brute force over the rectangles the room is made of. */
            o->numContactPoints = 0;
            getOBBVertices(o, v);
            for (int id = 0; id < ROOM_RECTS; id++)
                AAROBBContacts(roomRects[id], o, v, false);
            u8 bruteCount = o->numContactPoints;
            vect3D brutePoints[MAXCONTACTPOINTS];
            for (int i = 0; i < bruteCount; i++)
                brutePoints[i] = o->contactPoints[i].point;

            char msg[64];
            snprintf(msg, sizeof(msg), "contact count at x=%d y=%d", (int)xs[xi], (int)ys[yi]);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(bruteCount, gridCount, msg);

            qsort(gridPoints, gridCount, sizeof(vect3D), compareVects);
            qsort(brutePoints, bruteCount, sizeof(vect3D), compareVects);
            for (int i = 0; i < gridCount; i++)
                TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&brutePoints[i], &gridPoints[i], sizeof(vect3D), msg);
        }
    }
}

static void test_a_body_far_outside_the_grid_is_handled(void)
{
    /*
     * A body can leave the room - falling out of the world, or shoved through
     * a portal that has since moved. The broadphase converts its bounding box
     * to cell indices without clamping them to the grid, so this is where an
     * out of bounds read would show up. Under ASan a regression here aborts.
     */
    buildRoom();
    OBB_struct *o = makeCube(vect(-inttof32(4000), -inttof32(4000), -inttof32(4000)));
    vect3D v[8];
    getOBBVertices(o, v);

    o->numContactPoints = 0;
    AARsOBBContacts(o, false);

    TEST_ASSERT_EQUAL_UINT8(0, o->numContactPoints);
}

static void test_regenerating_the_grid_repeatedly_is_clean(void)
{
    /* Every level load rebuilds the grid; a leak here is a slow death on a
     * machine with this little RAM. Under ASan's leak checker this fails if
     * the previous grid is not released. */
    for (int i = 0; i < 8; i++)
    {
        initAARs();
        buildRoom();
    }
    TEST_ASSERT_TRUE(true);
}

/* --- box against box ------------------------------------------------------ */

static void test_two_overlapping_boxes_generate_contacts(void)
{
    OBB_struct *a = createOBB(0, TEST_CUBE_SIZE, vect(0, 0, 0), TEST_CUBE_MASS, inttof32(1), 0);
    OBB_struct *b = createOBB(1, TEST_CUBE_SIZE, vect(F32(1.5), 0, 0), TEST_CUBE_MASS, inttof32(1), 0);

    vect3D v[8];
    getOBBVertices(a, v);
    getOBBVertices(b, v);
    a->numContactPoints = 0;

    collideOBBs(a, b);

    TEST_ASSERT_GREATER_THAN_UINT8(0, a->numContactPoints);
    for (int i = 0; i < a->numContactPoints; i++)
        TEST_ASSERT_EQUAL_PTR(b, a->contactPoints[i].target);
}

static void test_two_separated_boxes_generate_nothing(void)
{
    OBB_struct *a = createOBB(0, TEST_CUBE_SIZE, vect(0, 0, 0), TEST_CUBE_MASS, inttof32(1), 0);
    OBB_struct *b = createOBB(1, TEST_CUBE_SIZE, vect(inttof32(20), 0, 0), TEST_CUBE_MASS, inttof32(1), 0);

    vect3D v[8];
    getOBBVertices(a, v);
    getOBBVertices(b, v);
    a->numContactPoints = 0;

    collideOBBs(a, b);

    TEST_ASSERT_EQUAL_UINT8(0, a->numContactPoints);
}

/* --- planes --------------------------------------------------------------- */

static void test_plane_distance_is_signed(void)
{
    plane_struct p;
    initPlane(&p, 0, inttof32(1), 0, 0); /* y = 0, normal up */

    TEST_ASSERT_GREATER_THAN_INT32(0, evaluatePlanePoint(&p, vect(0, inttof32(3), 0)));
    TEST_ASSERT_LESS_THAN_INT32(0, evaluatePlanePoint(&p, vect(0, -inttof32(3), 0)));
    ASSERT_F32_WITHIN(TOL_FEW, 0, evaluatePlanePoint(&p, vect(inttof32(9), 0, inttof32(9))));
    ASSERT_F32_WITHIN(TOL_COARSE, inttof32(3), evaluatePlanePoint(&p, vect(0, inttof32(3), 0)));
}

static void test_plane_is_normalised_on_init(void)
{
    /* Coefficients are scaled arbitrarily by the caller; initPlane divides
     * through by the normal's length so the distance comes out true. */
    plane_struct p;
    initPlane(&p, 0, inttof32(8), 0, 0);

    ASSERT_F32_WITHIN(TOL_COARSE, ONE, magnitude(vect(p.A, p.B, p.C)));
    ASSERT_F32_WITHIN(TOL_COARSE, inttof32(2), evaluatePlanePoint(&p, vect(0, inttof32(2), 0)));
}

static void test_plane_contacts_are_generated_for_a_sunken_box(void)
{
    plane_struct p;
    initPlane(&p, 0, inttof32(1), 0, 0);
    OBB_struct *o = makeCube(vect(0, F32(0.5), 0));

    planeOBBContacts(&p, o);

    /* The four bottom corners are below y=0. */
    TEST_ASSERT_EQUAL_UINT8(4, o->numContactPoints);
    for (int i = 0; i < o->numContactPoints; i++)
        TEST_ASSERT_EQUAL_INT(PLANECOLLISION, o->contactPoints[i].type);
    TEST_ASSERT_GREATER_THAN_UINT16(0, o->maxPenetration);
}

static void test_plane_contacts_are_not_generated_for_a_clear_box(void)
{
    plane_struct p;
    initPlane(&p, 0, inttof32(1), 0, 0);
    OBB_struct *o = makeCube(vect(0, inttof32(5), 0));

    planeOBBContacts(&p, o);

    TEST_ASSERT_EQUAL_UINT8(0, o->numContactPoints);
    TEST_ASSERT_EQUAL_UINT16(0, o->maxPenetration);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_create_and_disable_a_rectangle);
    RUN_TEST(test_create_does_not_overwrite_a_live_slot);
    RUN_TEST(test_init_aars_frees_every_slot);

    RUN_TEST(test_a_box_straddling_the_floor_generates_contacts);
    RUN_TEST(test_a_box_clear_of_the_floor_generates_nothing);
    RUN_TEST(test_a_box_below_the_floor_generates_nothing);
    RUN_TEST(test_a_box_straddling_the_plane_but_beyond_the_rectangle);
    RUN_TEST(test_a_disabled_rectangle_generates_nothing);
    RUN_TEST(test_walls_on_every_axis_generate_contacts);
    RUN_TEST(test_contacts_append_rather_than_replace);
    RUN_TEST(test_null_arguments_are_tolerated);

    RUN_TEST(test_grid_finds_the_floor_and_records_it_as_ground);
    RUN_TEST(test_a_box_in_mid_air_rests_on_nothing);
    RUN_TEST(test_the_grid_agrees_with_brute_force);
    RUN_TEST(test_a_body_far_outside_the_grid_is_handled);
    RUN_TEST(test_regenerating_the_grid_repeatedly_is_clean);

    RUN_TEST(test_two_overlapping_boxes_generate_contacts);
    RUN_TEST(test_two_separated_boxes_generate_nothing);

    RUN_TEST(test_plane_distance_is_signed);
    RUN_TEST(test_plane_is_normalised_on_init);
    RUN_TEST(test_plane_contacts_are_generated_for_a_sunken_box);
    RUN_TEST(test_plane_contacts_are_not_generated_for_a_clear_box);

    return UNITY_END();
}
