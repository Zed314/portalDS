/*
 * Unit tests for the 3x3 matrix code in arm7/source/math.c.
 *
 * These matrices are only ever rotations, and the invariants worth testing
 * are the ones the physics engine actually relies on: that the transpose is
 * the inverse, that rotating by an angle and then by its negation is a no-op,
 * and above all that fixMatrix() pulls a drifted matrix back to orthonormal -
 * without it, integrating an orientation makes boxes visibly shear after a
 * few seconds of simulation.
 *
 * Layout reminder: row-major in a flat 9 element array, so (row,col) is at
 * m[col+row*3].
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nds.h>

#include "../../arm7/include/math.h"
#include "../host/f32_test.h"

void setUp(void) {}
void tearDown(void) {}

static void identity(int32_t *m)
{
    memset(m, 0, 9 * sizeof(int32_t));
    m[0] = m[4] = m[8] = ONE;
}

static void assert_matrix_within(int32_t tol, const int32_t *expected, const int32_t *actual)
{
    for (int i = 0; i < 9; i++)
    {
        char msg[32];
        snprintf(msg, sizeof(msg), "element %d", i);
        TEST_ASSERT_INT32_WITHIN_MESSAGE(tol, expected[i], actual[i], msg);
    }
}

/* Checks m is a rotation: columns are unit length and mutually orthogonal. */
static void assert_orthonormal(int32_t tol, const int32_t *m)
{
    vect3D col[3] = {
        vect(m[0], m[3], m[6]),
        vect(m[1], m[4], m[7]),
        vect(m[2], m[5], m[8]),
    };

    for (int i = 0; i < 3; i++)
    {
        ASSERT_F32_WITHIN(tol, ONE, dotProduct(col[i], col[i]));
        for (int j = i + 1; j < 3; j++)
            ASSERT_F32_WITHIN(tol, 0, dotProduct(col[i], col[j]));
    }
}

/* --- transpose ---------------------------------------------------------- */

static void test_transpose_swaps_rows_and_columns(void)
{
    int32_t m[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    int32_t t[9];

    transposeMatrix33(m, t);

    int32_t expected[9] = {1, 4, 7, 2, 5, 8, 3, 6, 9};
    assert_matrix_within(0, expected, t);
}

static void test_transpose_is_its_own_inverse(void)
{
    int32_t m[9] = {11, -22, 33, 44, 55, -66, 77, 88, 99};
    int32_t t[9], tt[9];

    transposeMatrix33(m, t);
    transposeMatrix33(t, tt);

    assert_matrix_within(0, m, tt);
}

/* --- multiplication ----------------------------------------------------- */

static void test_multiply_by_identity_is_a_no_op(void)
{
    int32_t id[9], m[9], out[9];
    identity(id);
    identity(m);
    rotateMatrixY(m, F32(0.7), false);

    multMatrix33(m, id, out);
    assert_matrix_within(TOL_BIT, m, out);

    multMatrix33(id, m, out);
    assert_matrix_within(TOL_BIT, m, out);
}

static void test_multiply_composes_rotations(void)
{
    /* Rotating 30 degrees twice about Y is the same as rotating 60 degrees. */
    int32_t twice[9], once[9];
    identity(twice);
    identity(once);

    rotateMatrixY(twice, F32(M_PI / 6), false);
    rotateMatrixY(twice, F32(M_PI / 6), false);
    rotateMatrixY(once, F32(M_PI / 3), false);

    assert_matrix_within(TOL_COARSE, once, twice);
}

static void test_rotation_times_its_transpose_is_the_identity(void)
{
    /* This is the property the engine leans on when it builds the world-space
     * inverse inertia tensor: for a rotation, transpose == inverse. */
    int32_t m[9], t[9], out[9], id[9];
    identity(m);
    identity(id);
    rotateMatrixX(m, F32(0.4), false);
    rotateMatrixZ(m, F32(-1.1), false);

    transposeMatrix33(m, t);
    multMatrix33(m, t, out);

    assert_matrix_within(TOL_COARSE, id, out);
}

static void test_add_matrix(void)
{
    int32_t a[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    int32_t b[9] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
    int32_t out[9];

    addMatrix33(a, b, out);

    int32_t expected[9] = {10, 10, 10, 10, 10, 10, 10, 10, 10};
    assert_matrix_within(0, expected, out);
}

/* --- transforming vectors ----------------------------------------------- */

static void test_eval_vector_by_identity_returns_the_vector(void)
{
    int32_t id[9];
    identity(id);

    vect3D v = vect(F32(1.5), F32(-2.25), F32(3.0));
    ASSERT_VECT_WITHIN(TOL_BIT, v.x, v.y, v.z, evalVectMatrix33(id, v));
}

static void test_eval_vector_applies_a_quarter_turn(void)
{
    /* A quarter turn about Z takes +X to +Y (or -Y, depending on the sign
     * convention baked into rotateMatrixZ); either way the vector must land
     * on the Y axis with unit length, and X must vanish. */
    int32_t m[9];
    identity(m);
    rotateMatrixZ(m, F32(M_PI / 2), false);

    vect3D r = evalVectMatrix33(m, vect(ONE, 0, 0));

    ASSERT_F32_WITHIN(TOL_COARSE, 0, r.x);
    ASSERT_F32_WITHIN(TOL_COARSE, 0, r.z);
    TEST_ASSERT_INT32_WITHIN(TOL_COARSE, ONE, abs(r.y));
}

static void test_rotation_preserves_length(void)
{
    int32_t m[9];
    identity(m);
    rotateMatrixX(m, F32(0.9), false);
    rotateMatrixY(m, F32(-0.3), false);

    vect3D v = vect(inttof32(3), inttof32(4), 0);
    ASSERT_F32_WITHIN(TOL_COARSE, magnitude(v), magnitude(evalVectMatrix33(m, v)));
}

/* --- arbitrary axis ----------------------------------------------------- */

static void test_rotate_axis_by_zero_is_the_identity(void)
{
    int32_t m[9], id[9];
    identity(m);
    identity(id);

    rotateMatrixAxis(m, 0, normalize(vect(ONE, ONE, ONE)), false);

    assert_matrix_within(TOL_COARSE, id, m);
}

static void test_rotate_axis_matches_rotate_y_about_the_y_axis(void)
{
    /* rotateMatrixAxis with a = +Y has to agree with the specialised
     * rotateMatrixY; the two are independent implementations of the same
     * rotation and it is easy for one to drift from the other. */
    int32_t byAxis[9], byY[9];
    identity(byAxis);
    identity(byY);

    rotateMatrixAxis(byAxis, F32(0.6), vect(0, ONE, 0), false);
    rotateMatrixY(byY, F32(0.6), false);

    assert_matrix_within(TOL_COARSE, byY, byAxis);
}

static void test_rotate_axis_leaves_its_own_axis_fixed(void)
{
    vect3D axis = normalize(vect(ONE, ONE, 0));
    int32_t m[9];
    identity(m);

    rotateMatrixAxis(m, F32(1.234), axis, false);

    /* A rotation about an axis fixes that axis. */
    ASSERT_VECT_WITHIN(TOL_COARSE, axis.x, axis.y, axis.z, evalVectMatrix33(m, axis));
}

static void test_rotate_axis_produces_a_rotation(void)
{
    int32_t m[9];
    identity(m);
    rotateMatrixAxis(m, F32(2.0), normalize(vect(ONE, -ONE, ONE)), true);

    assert_orthonormal(TOL_COARSE, m);
}

/* --- fixMatrix ---------------------------------------------------------- */

static void test_fix_matrix_leaves_a_clean_rotation_alone(void)
{
    int32_t m[9], before[9];
    identity(m);
    rotateMatrixY(m, F32(0.5), false);
    memcpy(before, m, sizeof(before));

    fixMatrix(m);

    assert_matrix_within(TOL_COARSE, before, m);
}

static void test_fix_matrix_reorthonormalises_a_drifted_matrix(void)
{
    /* Simulates what integrating an orientation does over time: the columns
     * grow, shrink and stop being perpendicular. fixMatrix has to pull them
     * back, which is the only thing standing between the engine and visibly
     * sheared boxes. */
    int32_t m[9] = {
        F32(1.05),  F32(0.04), F32(-0.02),
        F32(-0.03), F32(0.97), F32(0.06),
        F32(0.01),  F32(-0.05), F32(1.02),
    };

    fixMatrix(m);

    assert_orthonormal(TOL_COARSE, m);
    /* Still recognisably the matrix it started as, not some other rotation. */
    ASSERT_F32_WITHIN(F32(0.15), ONE, m[0]);
    ASSERT_F32_WITHIN(F32(0.15), ONE, m[4]);
    ASSERT_F32_WITHIN(F32(0.15), ONE, m[8]);
}

static void test_fix_matrix_tolerates_null(void)
{
    fixMatrix(NULL);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_transpose_swaps_rows_and_columns);
    RUN_TEST(test_transpose_is_its_own_inverse);

    RUN_TEST(test_multiply_by_identity_is_a_no_op);
    RUN_TEST(test_multiply_composes_rotations);
    RUN_TEST(test_rotation_times_its_transpose_is_the_identity);
    RUN_TEST(test_add_matrix);

    RUN_TEST(test_eval_vector_by_identity_returns_the_vector);
    RUN_TEST(test_eval_vector_applies_a_quarter_turn);
    RUN_TEST(test_rotation_preserves_length);

    RUN_TEST(test_rotate_axis_by_zero_is_the_identity);
    RUN_TEST(test_rotate_axis_matches_rotate_y_about_the_y_axis);
    RUN_TEST(test_rotate_axis_leaves_its_own_axis_fixed);
    RUN_TEST(test_rotate_axis_produces_a_rotation);

    RUN_TEST(test_fix_matrix_leaves_a_clean_rotation_alone);
    RUN_TEST(test_fix_matrix_reorthonormalises_a_drifted_matrix);
    RUN_TEST(test_fix_matrix_tolerates_null);

    return UNITY_END();
}
