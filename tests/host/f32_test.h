/*
 * Assertion helpers for 20.12 fixed point, shared by the test suites.
 *
 * Every quantity in the engine is an int32 where 4096 is 1.0, so an exact
 * comparison is only reasonable for results that are exact by construction
 * (a transpose, a dot product of axis vectors). Anything that goes through a
 * square root, a trig call or a chain of >>12 truncations lands a few units
 * off, and the tolerances below are the vocabulary for saying how far off is
 * acceptable.
 */

#ifndef PORTALDS_TEST_F32_H
#define PORTALDS_TEST_F32_H

#include "unity.h"

/** 1.0 in 20.12 fixed point. */
#define ONE (1 << 12)

/** Writes a fixed point literal readably: F32(1.5) is 6144. */
#define F32(x) ((int32_t)((x) * (double)ONE))

/** A single least significant bit: the error one >>12 truncation can leave. */
#define TOL_BIT 1

/** A few bits: results of a handful of chained fixed point operations. */
#define TOL_FEW 8

/** ~0.5%: results that go through the square root or the trig tables. */
#define TOL_COARSE 24

#define ASSERT_F32_WITHIN(tol, expected, actual) \
    TEST_ASSERT_INT32_WITHIN_MESSAGE((tol), (expected), (actual), #actual)

#define ASSERT_VECT_WITHIN(tol, ex, ey, ez, v)                         \
    do {                                                               \
        vect3D assert_v_ = (v);                                        \
        TEST_ASSERT_INT32_WITHIN_MESSAGE((tol), (ex), assert_v_.x, #v ".x"); \
        TEST_ASSERT_INT32_WITHIN_MESSAGE((tol), (ey), assert_v_.y, #v ".y"); \
        TEST_ASSERT_INT32_WITHIN_MESSAGE((tol), (ez), assert_v_.z, #v ".z"); \
    } while (0)

#endif
