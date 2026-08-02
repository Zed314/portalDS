/*
 * Unit tests for rigid body setup and geometry - arm7/source/OBB.c.
 *
 * This is the part of the solver with checkable answers: a box's corners, its
 * bounding box, its inertia tensor, and the torque a force produces. Nothing
 * here integrates anything; see test_solver.c for that and test_collision.c
 * for contact generation.
 *
 * The engine keeps its bodies in a global pool rather than handing them out,
 * so every test starts from physicsReset() and works on objects[] directly.
 */

#include <stdlib.h>
#include <string.h>

#include "../host/physics_fixture.h"

/*
 * Defined in OBB.c but not declared in OBB.h - they are internal helpers that
 * happen to have external linkage. Declared here rather than exported, so the
 * tests can reach them without widening the engine's public surface.
 */
bool collideAABB(vect3D o1, vect3D s1, vect3D o2, vect3D s2);
vect3D projectPointAABB(vect3D size, vect3D p, vect3D *n);
void calculateOBBEnergy(OBB_struct *o);
void copyOBB(OBB_struct *o1, OBB_struct *o2);

void setUp(void) { physicsReset(); }
void tearDown(void) { physicsReset(); }

static OBB_struct *makeCube(vect3D pos)
{
    return createOBB(0, TEST_CUBE_SIZE, pos, TEST_CUBE_MASS, inttof32(1), 0);
}

/* --- initialisation ------------------------------------------------------ */

static void test_init_leaves_the_body_at_rest(void)
{
    OBB_struct *o = makeCube(vect(0, inttof32(5), 0));

    TEST_ASSERT_TRUE(o->used);
    TEST_ASSERT_FALSE(o->sleep);
    TEST_ASSERT_EQUAL_UINT8(0, o->numContactPoints);
    ASSERT_VECT_WITHIN(0, 0, inttof32(5), 0, o->position);
    ASSERT_VECT_WITHIN(0, 0, 0, 0, o->velocity);
    ASSERT_VECT_WITHIN(0, 0, 0, 0, o->angularVelocity);
    ASSERT_VECT_WITHIN(0, 0, 0, 0, o->angularMomentum);
    ASSERT_VECT_WITHIN(0, 0, 0, 0, o->forces);
    ASSERT_VECT_WITHIN(0, 0, 0, 0, o->moment);
    TEST_ASSERT_EQUAL_UINT32(0, o->energy);

    /* Every body shares the one global contact scratch buffer. */
    TEST_ASSERT_EQUAL_PTR(contactPoints, o->contactPoints);
}

static void test_a_recycled_slot_does_not_inherit_the_previous_body(void)
{
    /*
     * The ARM9 reuses body ids, and createOBB overwrites its slot without
     * checking, so every piece of dynamic state has to be cleared. The one
     * that matters is counter - it is the number of calm frames behind the
     * sleep heuristic, so a body spawned into the slot of one that had settled
     * would otherwise fall asleep almost immediately and hang in mid-air.
     */
    OBB_struct *first = makeCube(vect(0, 0, 0));
    first->counter = SLEEPTIMETHRESHOLD;
    first->portaled = true;
    first->groundID = 4;
    first->sleep = true;

    OBB_struct *second = makeCube(vect(0, inttof32(10), 0));

    TEST_ASSERT_EQUAL_PTR(first, second); /* same slot, as the ARM9 would */
    TEST_ASSERT_EQUAL_UINT16(0, second->counter);
    TEST_ASSERT_FALSE(second->portaled);
    TEST_ASSERT_FALSE(second->sleep);
    TEST_ASSERT_EQUAL_INT16(-1, second->groundID);
}

static void test_init_obbs_frees_every_slot(void)
{
    for (int i = 0; i < NUMOBJECTS; i++)
        createOBB(i, TEST_CUBE_SIZE, vect(0, 0, 0), TEST_CUBE_MASS, inttof32(1), 0);
    for (int i = 0; i < NUMOBJECTS; i++)
        TEST_ASSERT_TRUE(objects[i].used);

    initOBBs();

    for (int i = 0; i < NUMOBJECTS; i++)
        TEST_ASSERT_FALSE(objects[i].used);
}

static void test_inverse_inertia_is_diagonal_and_symmetric_for_a_cube(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));

    /* A cube has the same moment of inertia about all three axes. */
    TEST_ASSERT_EQUAL_INT32(o->invInertiaMatrix[0], o->invInertiaMatrix[4]);
    TEST_ASSERT_EQUAL_INT32(o->invInertiaMatrix[4], o->invInertiaMatrix[8]);
    TEST_ASSERT_GREATER_THAN_INT32(0, o->invInertiaMatrix[0]);

    /* Everything off the diagonal must be zero - the tensor is expressed in
     * the box's own frame, where the axes are the principal axes. */
    const int off[] = {1, 2, 3, 5, 6, 7};
    for (size_t i = 0; i < sizeof(off) / sizeof(off[0]); i++)
        TEST_ASSERT_EQUAL_INT32_MESSAGE(0, o->invInertiaMatrix[off[i]], "off-diagonal");
}

static void test_a_longer_box_is_harder_to_spin_about_its_short_axis(void)
{
    /* Stretched along x: inertia about y and z grows (so the inverse shrinks),
     * while inertia about x, which only depends on y and z, is unchanged. */
    OBB_struct *cube = createOBB(0, vect(inttof32(1), inttof32(1), inttof32(1)),
                                 vect(0, 0, 0), TEST_CUBE_MASS, inttof32(1), 0);
    int32 cubeXX = cube->invInertiaMatrix[0];
    int32 cubeYY = cube->invInertiaMatrix[4];

    OBB_struct *rod = createOBB(1, vect(inttof32(4), inttof32(1), inttof32(1)),
                                vect(0, 0, 0), TEST_CUBE_MASS, inttof32(1), 0);

    TEST_ASSERT_EQUAL_INT32(cubeXX, rod->invInertiaMatrix[0]);
    TEST_ASSERT_LESS_THAN_INT32(cubeYY, rod->invInertiaMatrix[4]);
    TEST_ASSERT_LESS_THAN_INT32(cubeYY, rod->invInertiaMatrix[8]);
}

static void test_a_heavier_body_is_harder_to_spin(void)
{
    OBB_struct *light = createOBB(0, TEST_CUBE_SIZE, vect(0, 0, 0), inttof32(1), inttof32(1), 0);
    int32 lightXX = light->invInertiaMatrix[0];

    OBB_struct *heavy = createOBB(1, TEST_CUBE_SIZE, vect(0, 0, 0), inttof32(4), inttof32(1), 0);

    TEST_ASSERT_LESS_THAN_INT32(lightXX, heavy->invInertiaMatrix[0]);
}

static void test_transformation_matrix_of_zero_yaw_is_the_identity(void)
{
    int32 m[9];
    initTransformationMatrix(m, inttof32(1), 0);

    const int32 expected[9] = {ONE, 0, 0, 0, ONE, 0, 0, 0, ONE};
    for (int i = 0; i < 9; i++)
        TEST_ASSERT_EQUAL_INT32(expected[i], m[i]);
}

static void test_transformation_matrix_of_a_yaw_is_a_rotation(void)
{
    /* 45 degrees: cos == sin == sqrt(2)/2. */
    const int32 c = F32(0.70710678), s = F32(0.70710678);
    int32 m[9];
    initTransformationMatrix(m, c, s);

    vect3D col[3] = {
        vect(m[0], m[3], m[6]),
        vect(m[1], m[4], m[7]),
        vect(m[2], m[5], m[8]),
    };
    for (int i = 0; i < 3; i++)
    {
        ASSERT_F32_WITHIN(TOL_FEW, ONE, dotProduct(col[i], col[i]));
        for (int j = i + 1; j < 3; j++)
            ASSERT_F32_WITHIN(TOL_FEW, 0, dotProduct(col[i], col[j]));
    }
}

/* --- vertices and bounds -------------------------------------------------- */

/*
 * Asserts that the eight vertices are exactly the eight (+/-sx, +/-sy, +/-sz)
 * corners around p, each appearing once. Order is deliberately not asserted -
 * it is an implementation detail of getVertices - but completeness is.
 */
static void assertCornerSet(const vect3D *v, vect3D p, vect3D s)
{
    bool seen[8] = {false};
    for (int i = 0; i < 8; i++)
    {
        vect3D d = vectDifference(v[i], p);
        TEST_ASSERT_EQUAL_INT32_MESSAGE(s.x, abs(d.x), "corner x offset");
        TEST_ASSERT_EQUAL_INT32_MESSAGE(s.y, abs(d.y), "corner y offset");
        TEST_ASSERT_EQUAL_INT32_MESSAGE(s.z, abs(d.z), "corner z offset");

        int idx = (d.x > 0 ? 1 : 0) | (d.y > 0 ? 2 : 0) | (d.z > 0 ? 4 : 0);
        TEST_ASSERT_FALSE_MESSAGE(seen[idx], "duplicate corner");
        seen[idx] = true;
    }
    for (int i = 0; i < 8; i++)
        TEST_ASSERT_TRUE_MESSAGE(seen[i], "missing corner");
}

static void test_vertices_of_an_axis_aligned_box(void)
{
    const vect3D p = vect(inttof32(10), inttof32(20), inttof32(30));
    const vect3D s = vect(inttof32(1), inttof32(2), inttof32(3));
    vect3D v[8];

    getVertices(s, p, vect(ONE, 0, 0), vect(0, ONE, 0), vect(0, 0, ONE), v);

    assertCornerSet(v, p, s);
}

static void test_vertices_are_centred_on_the_body(void)
{
    /* The eight corners of a box average out to its centre, whatever its
     * orientation - a cheap check that the corner signs are balanced. */
    OBB_struct *o = makeCube(vect(inttof32(7), inttof32(-3), inttof32(2)));
    rotateMatrixY(o->transformationMatrix, F32(0.6), false);
    rotateMatrixX(o->transformationMatrix, F32(0.3), false);

    vect3D v[8];
    getOBBVertices(o, v);

    vect3D sum = vect(0, 0, 0);
    for (int i = 0; i < 8; i++)
        sum = addVect(sum, v[i]);

    ASSERT_VECT_WITHIN(TOL_FEW, o->position.x, o->position.y, o->position.z, vectDivInt(sum, 8));
}

static void test_aabb_of_an_axis_aligned_box_is_tight(void)
{
    const vect3D p = vect(0, inttof32(5), 0);
    OBB_struct *o = makeCube(p);
    vect3D v[8];

    getOBBVertices(o, v);

    /* An unrotated cube's bounding box is the cube. */
    ASSERT_VECT_WITHIN(TOL_BIT, p.x - inttof32(1), p.y - inttof32(1), p.z - inttof32(1), o->AABBo);
    ASSERT_VECT_WITHIN(TOL_BIT, inttof32(2), inttof32(2), inttof32(2), o->AABBs);
}

static void test_aabb_contains_every_vertex_when_rotated(void)
{
    OBB_struct *o = makeCube(vect(inttof32(3), inttof32(4), inttof32(5)));
    rotateMatrixY(o->transformationMatrix, F32(0.7), false);
    rotateMatrixZ(o->transformationMatrix, F32(-0.4), false);

    vect3D v[8];
    getOBBVertices(o, v);

    for (int i = 0; i < 8; i++)
    {
        TEST_ASSERT_GREATER_OR_EQUAL_INT32(o->AABBo.x, v[i].x);
        TEST_ASSERT_GREATER_OR_EQUAL_INT32(o->AABBo.y, v[i].y);
        TEST_ASSERT_GREATER_OR_EQUAL_INT32(o->AABBo.z, v[i].z);
        TEST_ASSERT_LESS_OR_EQUAL_INT32(o->AABBo.x + o->AABBs.x, v[i].x);
        TEST_ASSERT_LESS_OR_EQUAL_INT32(o->AABBo.y + o->AABBs.y, v[i].y);
        TEST_ASSERT_LESS_OR_EQUAL_INT32(o->AABBo.z + o->AABBs.z, v[i].z);
    }

    /* A rotated cube needs a strictly bigger box than an unrotated one. */
    TEST_ASSERT_GREATER_THAN_INT32(inttof32(2), o->AABBs.x);
}

static void test_get_vertices_tolerates_null(void)
{
    vect3D v[8];
    getOBBVertices(NULL, v);
    getOBBVertices(&objects[0], NULL);
}

/* --- forces and torque ---------------------------------------------------- */

static void test_force_through_the_centre_of_mass_produces_no_torque(void)
{
    OBB_struct *o = makeCube(vect(inttof32(2), inttof32(2), inttof32(2)));

    applyOBBForce(o, o->position, vect(0, inttof32(10), 0));

    ASSERT_VECT_WITHIN(TOL_BIT, 0, inttof32(10), 0, o->forces);
    ASSERT_VECT_WITHIN(TOL_BIT, 0, 0, 0, o->moment);
}

static void test_off_centre_force_produces_the_cross_product_torque(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));
    const vect3D r = vect(inttof32(1), 0, 0);
    const vect3D f = vect(0, inttof32(10), 0);

    applyOBBForce(o, addVect(o->position, r), f);

    ASSERT_VECT_WITHIN(TOL_BIT, 0, inttof32(10), 0, o->forces);
    /* r x f = x_hat x y_hat scaled = +z */
    vect3D expected = vectProduct(r, f);
    ASSERT_VECT_WITHIN(TOL_FEW, expected.x, expected.y, expected.z, o->moment);
    TEST_ASSERT_GREATER_THAN_INT32(0, o->moment.z);
}

static void test_forces_accumulate(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));

    applyOBBForce(o, o->position, vect(inttof32(1), 0, 0));
    applyOBBForce(o, o->position, vect(inttof32(2), 0, 0));
    applyOBBForce(o, o->position, vect(0, inttof32(5), 0));

    ASSERT_VECT_WITHIN(TOL_BIT, inttof32(3), inttof32(5), 0, o->forces);
}

static void test_opposing_off_centre_forces_are_a_pure_couple(void)
{
    /* Equal and opposite forces at opposite corners cancel linearly but add
     * rotationally - the classic way to spin a body without moving it. */
    OBB_struct *o = makeCube(vect(0, 0, 0));
    const vect3D r = vect(inttof32(1), 0, 0);

    applyOBBForce(o, addVect(o->position, r), vect(0, inttof32(10), 0));
    applyOBBForce(o, vectDifference(o->position, r), vect(0, -inttof32(10), 0));

    ASSERT_VECT_WITHIN(TOL_BIT, 0, 0, 0, o->forces);
    TEST_ASSERT_GREATER_THAN_INT32(0, o->moment.z);
}

static void test_apply_force_tolerates_null(void)
{
    applyOBBForce(NULL, vect(0, 0, 0), vect(0, inttof32(1), 0));
}

/* --- energy and the sleep heuristic --------------------------------------- */

static void test_energy_is_zero_at_rest_and_grows_with_speed(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));

    calculateOBBEnergy(o);
    TEST_ASSERT_EQUAL_UINT32(0, o->energy);

    o->velocity = vect(0, F32(0.5), 0);
    calculateOBBEnergy(o);
    u32 slow = o->energy;
    TEST_ASSERT_GREATER_THAN_UINT32(0, slow);

    o->velocity = vect(0, inttof32(2), 0);
    calculateOBBEnergy(o);
    TEST_ASSERT_GREATER_THAN_UINT32(slow, o->energy);
}

static void test_energy_counts_rotation_as_well_as_translation(void)
{
    OBB_struct *o = makeCube(vect(0, 0, 0));

    o->angularVelocity = vect(0, inttof32(2), 0);
    calculateOBBEnergy(o);

    TEST_ASSERT_GREATER_THAN_UINT32(0, o->energy);
}

/* --- assorted helpers ----------------------------------------------------- */

static void test_aabb_overlap(void)
{
    const vect3D size = vect(inttof32(2), inttof32(2), inttof32(2));

    TEST_ASSERT_TRUE(collideAABB(vect(0, 0, 0), size, vect(inttof32(1), inttof32(1), inttof32(1)), size));
    TEST_ASSERT_FALSE(collideAABB(vect(0, 0, 0), size, vect(inttof32(10), 0, 0), size));
    TEST_ASSERT_FALSE(collideAABB(vect(0, 0, 0), size, vect(0, inttof32(10), 0), size));
    TEST_ASSERT_FALSE(collideAABB(vect(0, 0, 0), size, vect(0, 0, inttof32(10)), size));
}

static void test_project_point_onto_box_face(void)
{
    const vect3D size = vect(inttof32(1), inttof32(1), inttof32(1));
    vect3D n;

    /* Well outside on +x: clamped back to the +x face, normal points +x. */
    vect3D p = projectPointAABB(size, vect(inttof32(5), 0, 0), &n);
    TEST_ASSERT_EQUAL_INT32(inttof32(1), p.x);
    TEST_ASSERT_EQUAL_INT32(1, n.x);
    TEST_ASSERT_EQUAL_INT32(0, n.y);
    TEST_ASSERT_EQUAL_INT32(0, n.z);

    /* Outside on -y. */
    p = projectPointAABB(size, vect(0, -inttof32(5), 0), &n);
    TEST_ASSERT_EQUAL_INT32(-inttof32(1), p.y);
    TEST_ASSERT_EQUAL_INT32(-1, n.y);
}

static void test_project_point_inside_the_box_picks_the_nearest_face(void)
{
    const vect3D size = vect(inttof32(1), inttof32(1), inttof32(1));
    vect3D n;

    /* Just inside the +z face: that is the one to push out through. */
    vect3D p = projectPointAABB(size, vect(0, 0, F32(0.9)), &n);

    TEST_ASSERT_EQUAL_INT32(inttof32(1), p.z);
    TEST_ASSERT_EQUAL_INT32(1, n.z);
}

static void test_copy_obb_duplicates_the_dynamic_state(void)
{
    /* copyOBB is what the timestep bisection rolls back to, so it has to carry
     * every quantity the integrator touches. */
    OBB_struct *o = makeCube(vect(inttof32(1), inttof32(2), inttof32(3)));
    o->velocity = vect(inttof32(4), inttof32(5), inttof32(6));
    o->angularMomentum = vect(inttof32(7), 0, 0);
    rotateMatrixY(o->transformationMatrix, F32(0.5), false);

    OBB_struct copy;
    memset(&copy, 0, sizeof(copy));
    copyOBB(o, &copy);

    ASSERT_VECT_WITHIN(0, o->position.x, o->position.y, o->position.z, copy.position);
    ASSERT_VECT_WITHIN(0, o->velocity.x, o->velocity.y, o->velocity.z, copy.velocity);
    ASSERT_VECT_WITHIN(0, o->angularMomentum.x, o->angularMomentum.y, o->angularMomentum.z,
                       copy.angularMomentum);
    TEST_ASSERT_EQUAL_INT32(o->mass, copy.mass);
    TEST_ASSERT_EQUAL_INT32_ARRAY(o->transformationMatrix, copy.transformationMatrix, 9);
    TEST_ASSERT_EQUAL_INT32_ARRAY(o->invInertiaMatrix, copy.invInertiaMatrix, 9);
}

static void test_wake_obbs_clears_sleep_on_every_body(void)
{
    for (int i = 0; i < 3; i++)
    {
        createOBB(i, TEST_CUBE_SIZE, vect(0, 0, 0), TEST_CUBE_MASS, inttof32(1), 0);
        objects[i].sleep = true;
        objects[i].counter = SLEEPTIMETHRESHOLD;
    }

    wakeOBBs();

    for (int i = 0; i < 3; i++)
    {
        TEST_ASSERT_FALSE(objects[i].sleep);
        TEST_ASSERT_EQUAL_UINT16(0, objects[i].counter);
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_leaves_the_body_at_rest);
    RUN_TEST(test_a_recycled_slot_does_not_inherit_the_previous_body);
    RUN_TEST(test_init_obbs_frees_every_slot);
    RUN_TEST(test_inverse_inertia_is_diagonal_and_symmetric_for_a_cube);
    RUN_TEST(test_a_longer_box_is_harder_to_spin_about_its_short_axis);
    RUN_TEST(test_a_heavier_body_is_harder_to_spin);
    RUN_TEST(test_transformation_matrix_of_zero_yaw_is_the_identity);
    RUN_TEST(test_transformation_matrix_of_a_yaw_is_a_rotation);

    RUN_TEST(test_vertices_of_an_axis_aligned_box);
    RUN_TEST(test_vertices_are_centred_on_the_body);
    RUN_TEST(test_aabb_of_an_axis_aligned_box_is_tight);
    RUN_TEST(test_aabb_contains_every_vertex_when_rotated);
    RUN_TEST(test_get_vertices_tolerates_null);

    RUN_TEST(test_force_through_the_centre_of_mass_produces_no_torque);
    RUN_TEST(test_off_centre_force_produces_the_cross_product_torque);
    RUN_TEST(test_forces_accumulate);
    RUN_TEST(test_opposing_off_centre_forces_are_a_pure_couple);
    RUN_TEST(test_apply_force_tolerates_null);

    RUN_TEST(test_energy_is_zero_at_rest_and_grows_with_speed);
    RUN_TEST(test_energy_counts_rotation_as_well_as_translation);

    RUN_TEST(test_aabb_overlap);
    RUN_TEST(test_project_point_onto_box_face);
    RUN_TEST(test_project_point_inside_the_box_picks_the_nearest_face);
    RUN_TEST(test_copy_obb_duplicates_the_dynamic_state);
    RUN_TEST(test_wake_obbs_clears_sleep_on_every_body);

    return UNITY_END();
}
