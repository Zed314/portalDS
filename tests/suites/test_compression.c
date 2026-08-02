/*
 * Unit tests for the 16 bit RLE codec in arm9/source/compression.c.
 *
 * This is the codec every saved level goes through, so a regression here
 * corrupts map files rather than just looking wrong on screen - and it is
 * pure data in, data out, which makes it the single best candidate in the
 * codebase for host testing.
 *
 * The property that matters is the round trip: decompress(compress(x)) == x
 * for every shape of input, including the ones the format handles specially -
 * runs longer than the 0x82 cap, literal stretches longer than the 0x80 cap,
 * a checkerboard that RLE expands rather than shrinks, and the empty input.
 *
 * Unit convention: srcS/dstS are counts of u16 elements, not bytes (see the
 * caller in editor/io.c). The return value of compressRLE is likewise a count
 * of u16 elements.
 */

#include "common/general.h"
#include "common/compress.h"

#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/*
 * Compresses src, decompresses it again and asserts the result matches.
 * Returns the compressed size in u16 elements so individual tests can also
 * assert something about how well it compressed.
 */
static uint32_t assert_round_trips(const u16 *src, uint32_t count)
{
    u16 *packed = NULL;
    u16 *src_copy = malloc(count ? count * sizeof(u16) : sizeof(u16));
    TEST_ASSERT_NOT_NULL(src_copy);
    memcpy(src_copy, src, count * sizeof(u16));

    uint32_t packedCount = compressRLE(&packed, src_copy, count);
    TEST_ASSERT_NOT_NULL_MESSAGE(packed, "compressRLE returned no buffer");
    TEST_ASSERT_GREATER_THAN_UINT32(0, packedCount);

    u16 *out = calloc(count ? count : 1, sizeof(u16));
    TEST_ASSERT_NOT_NULL(out);

    uint32_t written = decompressRLE(out, packed, count);
    TEST_ASSERT_EQUAL_UINT32(count, written);
    TEST_ASSERT_EQUAL_HEX16_ARRAY(src, out, count);

    /* The source must not have been modified in place. */
    TEST_ASSERT_EQUAL_HEX16_ARRAY(src, src_copy, count);

    free(out);
    free(packed);
    free(src_copy);
    return packedCount;
}

/* --- guards ------------------------------------------------------------- */

static void test_compress_rejects_null_arguments(void)
{
    u16 *dst = NULL;
    u16 src[4] = {1, 2, 3, 4};

    TEST_ASSERT_EQUAL_UINT32(0, compressRLE(NULL, src, 4));
    TEST_ASSERT_EQUAL_UINT32(0, compressRLE(&dst, NULL, 4));
    TEST_ASSERT_NULL(dst);
}

static void test_decompress_rejects_null_arguments(void)
{
    u16 dst[4];
    u16 src[8] = {0};

    TEST_ASSERT_EQUAL_UINT32(0, decompressRLE(NULL, src, 4));
    TEST_ASSERT_EQUAL_UINT32(0, decompressRLE(dst, NULL, 4));
}

/* --- shapes of input ---------------------------------------------------- */

static void test_empty_input(void)
{
    /* An empty room array is legal; the codec must produce a stream that
     * decompresses back to nothing rather than reading srcD[0] off the end. */
    u16 src[1] = {0};
    u16 *packed = NULL;

    uint32_t packedCount = compressRLE(&packed, src, 0);

    TEST_ASSERT_NOT_NULL(packed);
    TEST_ASSERT_GREATER_THAN_UINT32(0, packedCount);
    free(packed);
}

static void test_single_element(void)
{
    u16 src[1] = {0xBEEF};
    assert_round_trips(src, 1);
}

static void test_all_identical_compresses_well(void)
{
    u16 src[4096];
    for (int i = 0; i < 4096; i++)
        src[i] = 0x0042;

    uint32_t packedCount = assert_round_trips(src, 4096);

    /* A single repeated value is the best case; if this ever stops being far
     * smaller than the input, the run encoder has broken. */
    TEST_ASSERT_LESS_THAN_UINT32(4096 / 4, packedCount);
}

static void test_run_longer_than_the_encoder_cap(void)
{
    /* A run is capped at 0x82 elements per block, so 1000 identical values
     * must be split across several blocks and stitched back together. */
    u16 src[1000];
    for (int i = 0; i < 1000; i++)
        src[i] = 0x1234;

    assert_round_trips(src, 1000);
}

static void test_literal_stretch_longer_than_the_encoder_cap(void)
{
    /* Literal blocks cap at 0x80 elements. An ascending ramp has no runs at
     * all, so it exercises the literal path and its block splitting. */
    u16 src[500];
    for (int i = 0; i < 500; i++)
        src[i] = (u16)(i * 7 + 1);

    assert_round_trips(src, 500);
}

static void test_checkerboard_expands_but_still_round_trips(void)
{
    /* The pathological case called out in the source comments: alternating
     * values make the "compressed" stream larger than the input. Correctness
     * still has to hold. */
    u16 src[256];
    for (int i = 0; i < 256; i++)
        src[i] = (i & 1) ? 0xFFFF : 0x0000;

    assert_round_trips(src, 256);
}

static void test_runs_alternating_with_literals(void)
{
    /* The realistic shape of level data: slabs of one block type separated by
     * short stretches of detail. Exercises the transitions between the two
     * block kinds, which is where the index arithmetic is hairiest. */
    u16 src[600];
    int n = 0;
    for (int block = 0; block < 20; block++)
    {
        for (int i = 0; i < 20; i++)
            src[n++] = (u16)(0x1000 + block);
        for (int i = 0; i < 10; i++)
        {
            src[n] = (u16)(0x2000 + n);
            n++;
        }
    }
    TEST_ASSERT_EQUAL_INT(600, n);

    assert_round_trips(src, 600);
}

static void test_full_16_bit_values_survive(void)
{
    /* The codec was widened from bytes to u16 for block ids; values above
     * 0xFF must not be truncated to the low byte. */
    u16 src[8] = {0x0000, 0x00FF, 0x0100, 0x8000, 0xFFFF, 0x8000, 0x8000, 0x8000};
    assert_round_trips(src, 8);
}

static void test_boundary_lengths(void)
{
    /* Lengths either side of the 0x80 / 0x82 block caps, run and literal. */
    static const uint32_t lengths[] = {2, 3, 4, 127, 128, 129, 130, 131, 132, 133, 255, 256, 257};

    for (size_t k = 0; k < sizeof(lengths) / sizeof(lengths[0]); k++)
    {
        uint32_t n = lengths[k];
        u16 *run = malloc(n * sizeof(u16));
        u16 *ramp = malloc(n * sizeof(u16));
        TEST_ASSERT_NOT_NULL(run);
        TEST_ASSERT_NOT_NULL(ramp);

        for (uint32_t i = 0; i < n; i++)
        {
            run[i] = 0x5A5A;
            ramp[i] = (u16)(i + 1);
        }

        assert_round_trips(run, n);
        assert_round_trips(ramp, n);

        free(run);
        free(ramp);
    }
}

static void test_pseudorandom_inputs(void)
{
    /* A deterministic fuzz: mixed runs and noise at many lengths. Seeded, so
     * a failure is reproducible. */
    uint32_t seed = 0x12345678u;
    u16 src[977];

    for (int round = 0; round < 40; round++)
    {
        uint32_t n = 1 + (seed % 977);
        for (uint32_t i = 0; i < n; i++)
        {
            seed = seed * 1103515245u + 12345u;
            /* Bias towards repeats so runs actually form. */
            if (i > 0 && (seed >> 28) < 10)
                src[i] = src[i - 1];
            else
                src[i] = (u16)(seed >> 16);
        }
        assert_round_trips(src, n);
        seed = seed * 1103515245u + 12345u;
    }
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_compress_rejects_null_arguments);
    RUN_TEST(test_decompress_rejects_null_arguments);

    RUN_TEST(test_empty_input);
    RUN_TEST(test_single_element);
    RUN_TEST(test_all_identical_compresses_well);
    RUN_TEST(test_run_longer_than_the_encoder_cap);
    RUN_TEST(test_literal_stretch_longer_than_the_encoder_cap);
    RUN_TEST(test_checkerboard_expands_but_still_round_trips);
    RUN_TEST(test_runs_alternating_with_literals);
    RUN_TEST(test_full_16_bit_values_survive);
    RUN_TEST(test_boundary_lengths);
    RUN_TEST(test_pseudorandom_inputs);

    return UNITY_END();
}
