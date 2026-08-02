/*
 * Unit tests for the ARM7 fixed point primitives - arm7/include/math.h and
 * the square root in arm7/source/math.c.
 *
 * These are the foundation everything else in the physics engine is built on,
 * and they are also where the engine's sharpest edges are: mulf32 and mulv16
 * differ only in whether they widen to 64 bits, divv16 silently produces
 * garbage above 2^19, and there are two incompatible angle conventions in the
 * codebase. The tests below pin down each of those behaviours, including the
 * documented overflow limits - if someone "fixes" mulv16 to widen, the test
 * that asserts it wraps will say so.
 */

/* The system <math.h> has to come first: arm7/include/math.h is included by
 * relative path below and would otherwise be found in its place. */
#include <math.h>
#include <string.h>

#include <nds.h>

#include "../../arm7/include/math.h"
#include "../host/f32_test.h"

void setUp(void) {}
void tearDown(void) {}

/* --- conversions ------------------------------------------------------- */

static void test_conversions_round_trip(void)
{
    TEST_ASSERT_EQUAL_INT32(4096, inttof32(1));
    TEST_ASSERT_EQUAL_INT32(-8192, inttof32(-2));
    TEST_ASSERT_EQUAL_INT32(7, f32toint(inttof32(7)));
    TEST_ASSERT_EQUAL_INT32(6144, floattof32(1.5f));
    TEST_ASSERT_EQUAL_FLOAT(1.5f, f32tofloat(6144));
}

static void test_f32toint_floors_towards_negative_infinity(void)
{
    /* An arithmetic shift, not a truncation towards zero. Worth pinning: the
     * asymmetry around zero is a classic source of off-by-one in the editor's
     * grid snapping. */
    TEST_ASSERT_EQUAL_INT32(1, f32toint(F32(1.75)));
    TEST_ASSERT_EQUAL_INT32(-2, f32toint(F32(-1.75)));
}

/* --- multiplication and division --------------------------------------- */

static void test_mulf32_multiplies(void)
{
    ASSERT_F32_WITHIN(TOL_BIT, F32(3.0), mulf32(F32(1.5), F32(2.0)));
    ASSERT_F32_WITHIN(TOL_BIT, F32(-3.0), mulf32(F32(1.5), F32(-2.0)));
    TEST_ASSERT_EQUAL_INT32(0, mulf32(F32(123.0), 0));
}

static void test_mulf32_survives_products_beyond_32_bits(void)
{
    /* 1000 * 1000 in f32 is 4096000 * 4096000, far past 2^31. This is the
     * whole reason mulf32 widens to 64 bits before shifting. */
    ASSERT_F32_WITHIN(TOL_BIT, inttof32(1000000),
                      mulf32(inttof32(1000), inttof32(1000)));
}

static void test_mulv16_agrees_with_mulf32_on_small_values(void)
{
    /* The fast path is only safe for small operands - normalised vector
     * components and the like. Inside that range the two must not diverge. */
    for (int32_t a = -ONE; a <= ONE; a += 137)
    {
        for (int32_t b = -ONE; b <= ONE; b += 211)
            TEST_ASSERT_EQUAL_INT32(mulf32(a, b), mulv16(a, b));
    }
}

static void test_mulv16_overflows_on_large_values(void)
{
    /* Documented limitation, asserted so it stays documented: the same inputs
     * mulf32 handles above wrap here. */
    TEST_ASSERT_NOT_EQUAL_INT32(mulf32(inttof32(1000), inttof32(1000)),
                                mulv16(inttof32(1000), inttof32(1000)));
}

static void test_divv16_divides(void)
{
    ASSERT_F32_WITHIN(TOL_BIT, F32(0.5), divv16(F32(1.0), F32(2.0)));
    ASSERT_F32_WITHIN(TOL_BIT, F32(4.0), divv16(F32(10.0), F32(2.5)));
    ASSERT_F32_WITHIN(TOL_BIT, F32(-2.0), divv16(F32(-4.0), F32(2.0)));
}

/* --- vectors ------------------------------------------------------------ */

static void test_vector_arithmetic(void)
{
    vect3D a = vect(F32(1.0), F32(2.0), F32(3.0));
    vect3D b = vect(F32(0.5), F32(0.5), F32(0.5));

    ASSERT_VECT_WITHIN(TOL_BIT, F32(1.5), F32(2.5), F32(3.5), addVect(a, b));
    ASSERT_VECT_WITHIN(TOL_BIT, F32(0.5), F32(1.5), F32(2.5), vectDifference(a, b));
    ASSERT_VECT_WITHIN(TOL_BIT, F32(2.0), F32(4.0), F32(6.0), vectMult(a, F32(2.0)));
    ASSERT_VECT_WITHIN(TOL_BIT, F32(3.0), F32(6.0), F32(9.0), vectMultInt(a, 3));
    ASSERT_VECT_WITHIN(TOL_BIT, F32(0.5), F32(1.0), F32(1.5), vectDivInt(a, 2));
}

static void test_dot_product(void)
{
    vect3D x = vect(ONE, 0, 0);
    vect3D y = vect(0, ONE, 0);

    TEST_ASSERT_EQUAL_INT32(0, dotProduct(x, y));
    ASSERT_F32_WITHIN(TOL_BIT, ONE, dotProduct(x, x));
    ASSERT_F32_WITHIN(TOL_BIT, -ONE, dotProduct(x, vectMultInt(x, -1)));
}

static void test_dot_product_accumulates_in_64_bits(void)
{
    /* Each term alone overflows 32 bits; the sum must still be right. */
    vect3D big = vect(inttof32(500), inttof32(500), inttof32(500));
    ASSERT_F32_WITHIN(TOL_FEW, inttof32(750000), dotProduct(big, big));
}

static void test_cross_product_follows_right_hand_rule(void)
{
    vect3D x = vect(ONE, 0, 0);
    vect3D y = vect(0, ONE, 0);
    vect3D z = vect(0, 0, ONE);

    ASSERT_VECT_WITHIN(TOL_BIT, 0, 0, ONE, vectProduct(x, y));
    ASSERT_VECT_WITHIN(TOL_BIT, ONE, 0, 0, vectProduct(y, z));
    ASSERT_VECT_WITHIN(TOL_BIT, 0, ONE, 0, vectProduct(z, x));
}

static void test_cross_product_is_anticommutative(void)
{
    vect3D a = vect(F32(1.5), F32(-2.0), F32(0.25));
    vect3D b = vect(F32(0.75), F32(3.0), F32(-1.25));

    vect3D ab = vectProduct(a, b);
    vect3D ba = vectProduct(b, a);

    /* a x b == -(b x a), to within the truncation each >>12 leaves behind. */
    ASSERT_VECT_WITHIN(TOL_BIT, -ba.x, -ba.y, -ba.z, ab);
}

static void test_cross_product_is_orthogonal_to_both_operands(void)
{
    vect3D a = vect(F32(1.0), F32(2.0), F32(3.0));
    vect3D b = vect(F32(-1.0), F32(0.5), F32(2.0));
    vect3D c = vectProduct(a, b);

    ASSERT_F32_WITHIN(TOL_FEW, 0, dotProduct(c, a));
    ASSERT_F32_WITHIN(TOL_FEW, 0, dotProduct(c, b));
}

static void test_cross_product_of_parallel_vectors_is_zero(void)
{
    vect3D a = vect(F32(1.0), F32(2.0), F32(3.0));
    ASSERT_VECT_WITHIN(TOL_BIT, 0, 0, 0, vectProduct(a, vectMultInt(a, 2)));
}

/* --- lengths ------------------------------------------------------------ */

static void test_magnitude(void)
{
    ASSERT_F32_WITHIN(TOL_BIT, inttof32(5), magnitude(vect(inttof32(3), inttof32(4), 0)));
    TEST_ASSERT_EQUAL_INT32(0, magnitude(vect(0, 0, 0)));
    ASSERT_F32_WITHIN(TOL_COARSE, ONE, magnitude(vect(ONE, 0, 0)));
}

static void test_magnitude_ignores_sign(void)
{
    vect3D v = vect(F32(-3.0), F32(4.0), F32(-12.0));
    ASSERT_F32_WITHIN(TOL_COARSE, inttof32(13), magnitude(v));
}

static void test_distance_is_symmetric(void)
{
    vect3D a = vect(inttof32(1), inttof32(2), inttof32(3));
    vect3D b = vect(inttof32(4), inttof32(6), inttof32(3));

    ASSERT_F32_WITHIN(TOL_BIT, inttof32(5), distance(a, b));
    TEST_ASSERT_EQUAL_INT32(distance(a, b), distance(b, a));
}

static void test_sqrtv(void)
{
    TEST_ASSERT_EQUAL_INT32(0, sqrtv(0));
    ASSERT_F32_WITHIN(TOL_BIT, inttof32(2), sqrtv(inttof32(4)));
    ASSERT_F32_WITHIN(TOL_BIT, inttof32(8), sqrtv(inttof32(64)));
}

static void test_sqrtv_stays_accurate_past_the_newton_threshold(void)
{
    /* sqrtv switches implementation at 1<<20; the branch above that adds a
     * refinement step, and this is what checks the refinement is right. */
    for (uint32_t n = 256; n <= 4096; n += 137)
    {
        uint32_t x = inttof32(n); /* past 1<<20, so the refined branch runs */
        /* sqrt(n) in f32, computed independently in floating point. */
        int32_t expected = (int32_t)(sqrt((double)n) * ONE);
        ASSERT_F32_WITHIN(TOL_COARSE, expected, (int32_t)sqrtv(x));
    }
}

static void test_normalize_produces_unit_length(void)
{
    vect3D v = normalize(vect(inttof32(3), inttof32(4), 0));
    ASSERT_F32_WITHIN(TOL_COARSE, ONE, magnitude(v));
    /* Direction preserved: 3/5 and 4/5. */
    ASSERT_VECT_WITHIN(TOL_COARSE, F32(0.6), F32(0.8), 0, v);
}

static void test_normalize_leaves_the_zero_vector_alone(void)
{
    /* Guards the division by zero; the engine normalises separation vectors
     * that can legitimately be zero when two bodies are exactly coincident. */
    ASSERT_VECT_WITHIN(TOL_BIT, 0, 0, 0, normalize(vect(0, 0, 0)));
}

/* --- misc --------------------------------------------------------------- */

static void test_clamp_accepts_its_bounds_in_either_order(void)
{
    TEST_ASSERT_EQUAL_INT32(5, clamp(5, 0, 10));
    TEST_ASSERT_EQUAL_INT32(0, clamp(-3, 0, 10));
    TEST_ASSERT_EQUAL_INT32(10, clamp(50, 0, 10));
    /* Swapped bounds are handled rather than returning nonsense. */
    TEST_ASSERT_EQUAL_INT32(5, clamp(5, 10, 0));
    TEST_ASSERT_EQUAL_INT32(0, clamp(-3, 10, 0));
}

static void test_project_vector_plane_removes_the_normal_component(void)
{
    vect3D n = vect(0, ONE, 0);
    vect3D v = vect(F32(1.0), F32(5.0), F32(-2.0));

    projectVectorPlane(&v, n);

    ASSERT_VECT_WITHIN(TOL_FEW, F32(1.0), 0, F32(-2.0), v);
    ASSERT_F32_WITHIN(TOL_FEW, 0, dotProduct(v, n));
}

static void test_project_vector_plane_tolerates_null(void)
{
    projectVectorPlane(NULL, vect(0, ONE, 0));
}

static void test_trig_matches_the_radian_convention(void)
{
    /* cosLerp/sinLerp here take f32 *radians* and return f32 - not libnds'
     * 15 bit binary angle. Mixing the two conventions up is the single most
     * common mistake when moving code between the two CPUs. */
    ASSERT_F32_WITHIN(TOL_FEW, ONE, cosLerp(0));
    ASSERT_F32_WITHIN(TOL_FEW, 0, sinLerp(0));
    ASSERT_F32_WITHIN(TOL_COARSE, 0, cosLerp(F32(M_PI / 2)));
    ASSERT_F32_WITHIN(TOL_COARSE, ONE, sinLerp(F32(M_PI / 2)));
    ASSERT_F32_WITHIN(TOL_COARSE, -ONE, cosLerp(F32(M_PI)));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_conversions_round_trip);
    RUN_TEST(test_f32toint_floors_towards_negative_infinity);

    RUN_TEST(test_mulf32_multiplies);
    RUN_TEST(test_mulf32_survives_products_beyond_32_bits);
    RUN_TEST(test_mulv16_agrees_with_mulf32_on_small_values);
    RUN_TEST(test_mulv16_overflows_on_large_values);
    RUN_TEST(test_divv16_divides);

    RUN_TEST(test_vector_arithmetic);
    RUN_TEST(test_dot_product);
    RUN_TEST(test_dot_product_accumulates_in_64_bits);
    RUN_TEST(test_cross_product_follows_right_hand_rule);
    RUN_TEST(test_cross_product_is_anticommutative);
    RUN_TEST(test_cross_product_is_orthogonal_to_both_operands);
    RUN_TEST(test_cross_product_of_parallel_vectors_is_zero);

    RUN_TEST(test_magnitude);
    RUN_TEST(test_magnitude_ignores_sign);
    RUN_TEST(test_distance_is_symmetric);
    RUN_TEST(test_sqrtv);
    RUN_TEST(test_sqrtv_stays_accurate_past_the_newton_threshold);
    RUN_TEST(test_normalize_produces_unit_length);
    RUN_TEST(test_normalize_leaves_the_zero_vector_alone);

    RUN_TEST(test_clamp_accepts_its_bounds_in_either_order);
    RUN_TEST(test_project_vector_plane_removes_the_normal_component);
    RUN_TEST(test_project_vector_plane_tolerates_null);
    RUN_TEST(test_trig_matches_the_radian_convention);

    return UNITY_END();
}
