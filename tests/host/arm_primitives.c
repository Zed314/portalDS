/*
 * Portable stand-ins for the two ARM assembly primitives, for host tests.
 *
 * arm7/source/isqrt32.c and arm7/source/normalize.c are hand written ARM
 * assembly - a branch-free restoring square root and a reciprocal-square-root
 * normalise. Neither can be assembled by the host compiler, and hand
 * translating them to C for the tests would mean the tests were checking the
 * translation rather than the code that ships.
 *
 * So the tests link these reference implementations instead. The consequence
 * is worth stating plainly:
 *
 *   - the assembly routines themselves are NOT under test here; they are only
 *     exercised on real hardware;
 *   - everything layered on top of them is. sqrtv()'s Newton refinement,
 *     fixMatrix(), magnitude(), initPlane() and so on are the real shipped
 *     code, running against a square root that is correct by construction.
 *
 * Because the reference and the assembly round differently in the last bit or
 * two, any test whose result flows through these must assert with a tolerance
 * (TEST_ASSERT_INT32_WITHIN) rather than an exact value.
 */

#include <nds.h>

#define ONE_F32 (1 << 12) /* 1.0 in 20.12 fixed point */

/* Exact floor(sqrt(x)) by restoring shift-and-subtract; no floating point, so
 * there is no double-rounding to argue about. Matches what the assembly
 * computes, bit for bit, by definition of the algorithm. */
uint32_t isqrt_asm(uint32_t x)
{
    uint32_t rem = 0, root = 0;
    for (int i = 0; i < 16; i++)
    {
        root <<= 1;
        rem = (rem << 2) | (x >> 30);
        x <<= 2;
        if (root < rem)
        {
            rem -= root | 1;
            root += 2;
        }
    }
    return root >> 1;
}

/* Scales a 20.12 vector in place to unit length (4096). Divides by the length
 * rather than multiplying by a reciprocal, which is slower but exact to the
 * last integer division. */
void normalize_arm7(int32_t *a)
{
    uint64_t squared = (int64_t)a[0] * a[0]
                     + (int64_t)a[1] * a[1]
                     + (int64_t)a[2] * a[2];
    if (squared == 0)
        return;

    /* Length in f32: sqrt(squared >> 12) taken at 20.12 scale, i.e.
     * sqrt(squared) with the 12 bit shift folded in. */
    uint64_t s = squared;
    uint32_t shift = 0;
    while (s > 0xFFFFFFFFull) { s >>= 2; shift++; }
    uint32_t len = isqrt_asm((uint32_t)s) << shift;
    if (len == 0)
        return;

    for (int i = 0; i < 3; i++)
    {
        /* Multiply rather than shift: a[i] is signed and shifting a negative
         * value left is undefined behaviour the sanitizers will (rightly)
         * flag in code that, unlike the engine's own, has no reason to. */
        int64_t scaled = ((int64_t)a[i] * ONE_F32) / (int32_t)len;
        a[i] = (int32_t)scaled;
    }
}
