/*
 * PCX decoding - arm9/source/pcx.c.
 *
 * Every image in the game is an 8 bit palettised PCX, and this is the decoder
 * for all of them. It reads a file into a buffer and walks it with an index,
 * which is exactly the shape of code that reads past the end when the file is
 * not what the header says it is.
 *
 * Unlike the other ARM9 suites this one needs no stand-in for the thing under
 * test: bufferizeFile() is plain stdio, so the decoder is handed real files
 * written to /tmp, byte for byte as a PCX is laid out on disk. Only the
 * cartridge and NitroFS calls around it are stubbed.
 *
 * @par What is being pinned
 * A PCX stride is padded to an even byte count, so bytesPerScanLine is wider
 * than the image for every odd width - and the decoder used to write the whole
 * stride into a row sized to the width, one byte past the end per row. That is
 * the case test_an_odd_width_image_decodes_within_its_buffer covers, and ASan
 * is what fails it if the clamp goes away.
 *
 * The rest are malformed files: truncated pixel data, a missing palette
 * marker, a file too short to hold either the header or the palette, a
 * reversed image window. All of them must come back NULL rather than as a
 * half decoded texture, because NULL is the only thing a caller can check.
 */

#include <unistd.h>

#include "unity.h"
#include "level_fixture.h"

/* --- writing PCX files --------------------------------------------------- */

#define PCX_HEADER_BYTES 128
#define PCX_PALETTE_BYTES 768

typedef struct
{
	u8*    data;
	size_t len, cap;
} blob_struct;

static void blobPut(blob_struct* b, u8 v)
{
	if(b->len==b->cap)
	{
		b->cap = b->cap ? b->cap*2 : 256;
		b->data = realloc(b->data, b->cap);
		TEST_ASSERT_NOT_NULL(b->data);
	}
	b->data[b->len++] = v;
}

static void blobPut16(blob_struct* b, u16 v) { blobPut(b, v&0xFF); blobPut(b, v>>8); }

/*
 * The 128 byte header. width and height are given as a window, which is how
 * PCX stores a size; stride is bytesPerScanLine, which callers set
 * independently so a test can make it disagree with the width.
 */
static void putHeader(blob_struct* b, int xmin, int ymin, int xmax, int ymax,
                      u8 bitsPerPixel, u8 planes, u16 stride, u8 manufacturer)
{
	const size_t start = b->len;

	blobPut(b, manufacturer);
	blobPut(b, 5);              /* version */
	blobPut(b, 1);              /* encoding: RLE */
	blobPut(b, bitsPerPixel);
	blobPut16(b, xmin); blobPut16(b, ymin);
	blobPut16(b, xmax); blobPut16(b, ymax);
	blobPut16(b, 72); blobPut16(b, 72);           /* device resolution */
	for(int i=0;i<48;i++)blobPut(b, (u8)i);       /* EGA palette */
	blobPut(b, 0);              /* reserved */
	blobPut(b, planes);
	blobPut16(b, stride);
	blobPut16(b, 1);            /* palette type */
	blobPut16(b, 0); blobPut16(b, 0);
	while(b->len - start < PCX_HEADER_BYTES)blobPut(b, 0);
}

/* One pixel, RLE encoded. Values at or above 0xC0 have to go out as a run of
 * one, or the decoder would read them as a count. */
static void putPixel(blob_struct* b, u8 v)
{
	if(v>=0xC0)blobPut(b, 0xC1);
	blobPut(b, v);
}

static void putRun(blob_struct* b, u8 count, u8 v)
{
	TEST_ASSERT_TRUE(count>=1 && count<=63);
	blobPut(b, 0xC0|count);
	blobPut(b, v);
}

/* The 0x0C marker and 256 RGB triples that close an 8 bit file. */
static void putPalette(blob_struct* b)
{
	blobPut(b, 0x0C);
	for(int i=0;i<256;i++)
	{
		blobPut(b, (u8)i);          /* r */
		blobPut(b, (u8)(255-i));    /* g */
		blobPut(b, (u8)(i/2));      /* b */
	}
}

static const char* tempDir(void) { return "/tmp"; }

static const char* tempName(void)
{
	static char name[64];
	snprintf(name, sizeof(name), "portalds_test_%d.pcx", (int)getpid());
	return name;
}

static const char* tempPath(void)
{
	static char path[128];
	snprintf(path, sizeof(path), "%s/%s", tempDir(), tempName());
	return path;
}

static void writeBlob(blob_struct* b)
{
	FILE* f = fopen(tempPath(), "wb");
	TEST_ASSERT_NOT_NULL_MESSAGE(f, "could not write the temporary pcx");
	if(b->len)TEST_ASSERT_EQUAL_size_t(b->len, fwrite(b->data, 1, b->len, f));
	fclose(f);
}

static void blobFree(blob_struct* b) { free(b->data); b->data=NULL; b->len=b->cap=0; }

/* An ordinary 8 bit image of the given size, every pixel a literal, with the
 * stride PCX would really use: the width rounded up to an even number. */
static void writeImage(int w, int h, u8 (*pixel)(int x, int y))
{
	blob_struct b = {0};
	const u16 stride = (u16)(w + (w & 1));

	putHeader(&b, 0, 0, w-1, h-1, 8, 1, stride, 0x0A);
	for(int y=0;y<h;y++)
	{
		for(int x=0;x<stride;x++)putPixel(&b, x<w ? pixel(x,y) : 0);
	}
	putPalette(&b);

	writeBlob(&b);
	blobFree(&b);
}

static u8 gradient(int x, int y) { return (u8)((x*7 + y*13) & 0xFF); }

static struct gl_texture_t* load(void)
{
	return ReadPCXFile(tempName(), (char*)tempDir());
}

/* --- fixture ------------------------------------------------------------- */

void setUp(void) { levelFixtureReset(); }
void tearDown(void) { unlink(tempPath()); }

/* --- images that are what they claim ------------------------------------- */

static void test_an_even_width_image_decodes(void)
{
	writeImage(8, 4, gradient);

	struct gl_texture_t* t = load();
	TEST_ASSERT_NOT_NULL_MESSAGE(t, "a well formed image failed to load");
	TEST_ASSERT_EQUAL_UINT16(8, t->width);
	TEST_ASSERT_EQUAL_UINT16(4, t->height);
	TEST_ASSERT_EQUAL_INT(8, t->format);

	for(int y=0;y<4;y++)
		for(int x=0;x<8;x++)
			TEST_ASSERT_EQUAL_UINT8_MESSAGE(gradient(x,y), t->texels[x+y*8], "wrong pixel");

	freePCX(t);
}

static void test_an_odd_width_image_decodes_within_its_buffer(void)
{
	/*
	 * The regression. A stride of width+1 means one padding byte per row, and
	 * the decoder used to store it - writing past the end of a buffer sized to
	 * width*height. ASan reports a heap-buffer-overflow if the clamp goes.
	 *
	 * The padding must be dropped rather than shifting the row, so every pixel
	 * has to land where it belongs on every row, not just the first.
	 */
	writeImage(7, 5, gradient);

	struct gl_texture_t* t = load();
	TEST_ASSERT_NOT_NULL(t);
	TEST_ASSERT_EQUAL_UINT16(7, t->width);

	for(int y=0;y<5;y++)
		for(int x=0;x<7;x++)
			TEST_ASSERT_EQUAL_UINT8_MESSAGE(gradient(x,y), t->texels[x+y*7], "a row was shifted by the padding");

	freePCX(t);
}

static void test_a_single_pixel_image_decodes(void)
{
	writeImage(1, 1, gradient);

	struct gl_texture_t* t = load();
	TEST_ASSERT_NOT_NULL(t);
	TEST_ASSERT_EQUAL_UINT16(1, t->width);
	TEST_ASSERT_EQUAL_UINT16(1, t->height);
	TEST_ASSERT_EQUAL_UINT8(gradient(0,0), t->texels[0]);

	freePCX(t);
}

static void test_a_run_spanning_a_row_boundary_decodes(void)
{
	/* Runs are not required to stop at the end of a row, so the decoder has
	 * to carry one over - which is why rle_count lives outside the row loop. */
	blob_struct b = {0};
	putHeader(&b, 0, 0, 3, 3, 8, 1, 4, 0x0A);
	putRun(&b, 16, 0x42);   /* the whole 4x4 image in one run */
	putPalette(&b);
	writeBlob(&b);
	blobFree(&b);

	struct gl_texture_t* t = load();
	TEST_ASSERT_NOT_NULL(t);
	for(int i=0;i<16;i++)TEST_ASSERT_EQUAL_UINT8(0x42, t->texels[i]);

	freePCX(t);
}

static void test_the_palette_is_converted(void)
{
	writeImage(4, 2, gradient);

	struct gl_texture_t* t = load();
	TEST_ASSERT_NOT_NULL(t);
	TEST_ASSERT_NOT_NULL(t->palette);
	/* Entry i is written as (i, 255-i, i/2), each dropped to 5 bits. */
	TEST_ASSERT_EQUAL_HEX16(RGB15(10>>3, (255-10)>>3, (10/2)>>3), t->palette[10]);

	freePCX(t);
}

/* --- files that are not ------------------------------------------------- */

static void test_a_missing_file_is_reported(void)
{
	TEST_ASSERT_NULL(ReadPCXFile("portalds_test_does_not_exist.pcx", (char*)tempDir()));
}

static void test_an_empty_file_is_reported(void)
{
	blob_struct b = {0};
	writeBlob(&b);
	blobFree(&b);

	TEST_ASSERT_NULL(load());
}

static void test_a_file_shorter_than_the_header_is_reported(void)
{
	/* The header is memcpy'd out of the buffer in one go, so a file shorter
	 * than 128 bytes used to be read past the end before anything was
	 * validated. */
	blob_struct b = {0};
	for(int i=0;i<40;i++)blobPut(&b, i==0 ? 0x0A : (u8)i);
	writeBlob(&b);
	blobFree(&b);

	TEST_ASSERT_NULL(load());
}

static void test_a_bad_manufacturer_byte_is_reported(void)
{
	blob_struct b = {0};
	putHeader(&b, 0, 0, 3, 3, 8, 1, 4, 0x55);   /* not 0x0A */
	for(int i=0;i<16;i++)putPixel(&b, 1);
	putPalette(&b);
	writeBlob(&b);
	blobFree(&b);

	TEST_ASSERT_NULL(load());
}

static void test_a_missing_palette_marker_is_reported(void)
{
	blob_struct b = {0};
	putHeader(&b, 0, 0, 3, 3, 8, 1, 4, 0x0A);
	for(int i=0;i<16;i++)putPixel(&b, 1);
	blobPut(&b, 0x00);                          /* should be 0x0C */
	for(int i=0;i<PCX_PALETTE_BYTES;i++)blobPut(&b, 0);
	writeBlob(&b);
	blobFree(&b);

	TEST_ASSERT_NULL(load());
}

static void test_a_file_too_short_for_a_palette_is_reported(void)
{
	/* The palette is found by seeking 769 bytes back from the end. A file
	 * barely longer than its header has nowhere to seek to, and the index
	 * used to go negative. */
	blob_struct b = {0};
	putHeader(&b, 0, 0, 3, 3, 8, 1, 4, 0x0A);
	for(int i=0;i<16;i++)putPixel(&b, 1);
	writeBlob(&b);
	blobFree(&b);

	TEST_ASSERT_NULL(load());
}

static void test_pixel_data_that_ends_early_is_reported(void)
{
	/*
	 * The header promises a 400x400 image and the file holds a handful of
	 * pixels. It has to be that much bigger than the data, for two reasons
	 * that both push the same way: the 769 byte palette sits at the end of
	 * every 8 bit PCX, so a decoder running past the pixel data eats that
	 * first; and any palette byte at or above 0xC0 reads as a run count, so
	 * each pair of bytes it swallows can yield up to 63 pixels. Between them
	 * a few hundred bytes of tail can fill a surprisingly large image. Only
	 * one bigger than anything that amplification can produce actually runs
	 * the buffer out, which is what this needs to exercise.
	 */
	blob_struct b = {0};
	putHeader(&b, 0, 0, 399, 399, 8, 1, 400, 0x0A);
	for(int i=0;i<8;i++)putPixel(&b, (u8)i);
	putPalette(&b);
	writeBlob(&b);
	blobFree(&b);

	TEST_ASSERT_NULL_MESSAGE(load(), "a half decoded image was handed back");
}

static void test_a_reversed_image_window_is_reported(void)
{
	/*
	 * The size is the difference between two corners, kept in a u16, so
	 * reversed corners underflow it into a 65000 pixel row.
	 *
	 * Note that this test does not isolate the check that rejects it: without
	 * that check the load still fails, just later and more expensively - the
	 * implied allocation is gigabytes and fails, or the stream runs out part
	 * way through a nonsense image. The check is there to fail fast rather
	 * than to change the answer, so this asserts the answer and not the
	 * route to it.
	 */
	blob_struct b = {0};
	putHeader(&b, 100, 100, 4, 4, 8, 1, 8, 0x0A);
	for(int i=0;i<64;i++)putPixel(&b, 1);
	putPalette(&b);
	writeBlob(&b);
	blobFree(&b);

	TEST_ASSERT_NULL(load());
}

static void test_an_unsupported_depth_is_reported(void)
{
	blob_struct b = {0};
	putHeader(&b, 0, 0, 3, 3, 8, 3, 12, 0x0A);   /* 24 bit: not handled */
	for(int i=0;i<48;i++)putPixel(&b, 1);
	putPalette(&b);
	writeBlob(&b);
	blobFree(&b);

	TEST_ASSERT_NULL(load());
}

static void test_a_stride_narrower_than_the_image_is_survivable(void)
{
	/* Not a file any encoder writes. The decoder gets fewer bytes per row than
	 * the row holds; whatever it does, it must stay inside the buffer. */
	blob_struct b = {0};
	putHeader(&b, 0, 0, 15, 15, 8, 1, 4, 0x0A);
	for(int i=0;i<64;i++)putPixel(&b, (u8)i);
	putPalette(&b);
	writeBlob(&b);
	blobFree(&b);

	struct gl_texture_t* t = load();
	if(t)freePCX(t);
}

/* --- conversion to direct colour ---------------------------------------- */

static void test_converting_to_16_bit_keeps_the_size(void)
{
	writeImage(7, 3, gradient);

	struct gl_texture_t* t = load();
	TEST_ASSERT_NOT_NULL(t);

	convertPCX16Bit(t);

	TEST_ASSERT_EQUAL_INT_MESSAGE(16, t->format, "should have been marked as direct colour");
	TEST_ASSERT_NOT_NULL(t->texels16);
	TEST_ASSERT_NULL_MESSAGE(t->texels, "the indexed data should have been released");
	TEST_ASSERT_EQUAL_UINT16(7, t->width);

	freePCX(t);
}

static void test_converting_tolerates_null(void)
{
	convertPCX16Bit(NULL);
	freePCX(NULL);
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_an_even_width_image_decodes);
	RUN_TEST(test_an_odd_width_image_decodes_within_its_buffer);
	RUN_TEST(test_a_single_pixel_image_decodes);
	RUN_TEST(test_a_run_spanning_a_row_boundary_decodes);
	RUN_TEST(test_the_palette_is_converted);

	RUN_TEST(test_a_missing_file_is_reported);
	RUN_TEST(test_an_empty_file_is_reported);
	RUN_TEST(test_a_file_shorter_than_the_header_is_reported);
	RUN_TEST(test_a_bad_manufacturer_byte_is_reported);
	RUN_TEST(test_a_missing_palette_marker_is_reported);
	RUN_TEST(test_a_file_too_short_for_a_palette_is_reported);
	RUN_TEST(test_pixel_data_that_ends_early_is_reported);
	RUN_TEST(test_a_reversed_image_window_is_reported);
	RUN_TEST(test_an_unsupported_depth_is_reported);
	RUN_TEST(test_a_stride_narrower_than_the_image_is_survivable);

	RUN_TEST(test_converting_to_16_bit_keeps_the_size);
	RUN_TEST(test_converting_tolerates_null);

	return UNITY_END();
}
