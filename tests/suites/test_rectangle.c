/*
 * Rectangle geometry and the lightmap atlas packer - arm9/source/editor/rectangle.c.
 *
 * This file needs nothing from the game but malloc, which makes it the most
 * testable thing in the ARM9 tree and, until now, one of the least tested.
 * Three separate jobs live in it, and all three matter at runtime rather than
 * only when editing:
 *
 *  - **closest point on a rectangle** - what the player's collision resolves
 *    against, every frame, for every nearby surface;
 *  - **ray against a rectangle** - what the portal gun and the gravity gun
 *    aim with;
 *  - **the maximal rectangle finder and the atlas packer** - what turns a
 *    room of blocks into a small number of large faces, and what fits every
 *    face's lightmap patch into one texture.
 *
 * The packer gets property tests rather than golden positions. A bin packer
 * has no single right answer - any placement that fits is correct - so what
 * is asserted is what would actually be wrong: a patch outside the atlas, or
 * two patches overlapping. Overlap is the one that matters, because it does
 * not crash. It silently lights two surfaces from the same pixels.
 */

#include "unity.h"
#include "level_fixture.h"

/*
 * rectangle.h exposes only the two packRectangles wrappers. The tree itself is
 * one level down and has external linkage, and the tests reach it directly for
 * the two cases the wrappers cannot express: whether a set was placed at all
 * (packRectangles discards the bool) and packing into a non-square atlas.
 */
void initTree(tree_struct* t, short w, short h);
bool insertRectangles(tree_struct* t, rectangle2DList_struct* l);
void freeTree(tree_struct* t);

void setUp(void) {}
void tearDown(void) {}

static void assertVect(vect3D expected, vect3D actual)
{
	TEST_ASSERT_EQUAL_INT32(expected.x, actual.x);
	TEST_ASSERT_EQUAL_INT32(expected.y, actual.y);
	TEST_ASSERT_EQUAL_INT32(expected.z, actual.z);
}

/* --- getClosestPointRectangle -------------------------------------------- */

/*
 * The rectangle is given as an origin and an extent with one zero component -
 * the axis it is flat along. The query point is projected onto its plane and
 * then clamped to its edges.
 *
 * A floor 4096 units square at the origin is the fixture for most of these:
 * flat in y, extending along +x and +z.
 */
#define FLOOR_ORIGIN vect(0, 0, 0)
#define FLOOR_SIZE   vect(4096, 0, 4096)

static void test_a_point_above_the_floor_projects_straight_down(void)
{
	const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(1024, 8192, 2048));
	assertVect(vect(1024, 0, 2048), p);
}

static void test_a_point_below_the_floor_projects_straight_up(void)
{
	const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(1024, -8192, 2048));
	assertVect(vect(1024, 0, 2048), p);
}

static void test_a_point_in_the_plane_is_returned_unchanged(void)
{
	const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(1024, 0, 2048));
	assertVect(vect(1024, 0, 2048), p);
}

static void test_a_point_past_the_near_x_edge_clamps_to_it(void)
{
	const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(-5000, 100, 2048));
	assertVect(vect(0, 0, 2048), p);
}

static void test_a_point_past_the_far_x_edge_clamps_to_it(void)
{
	const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(9000, 100, 2048));
	assertVect(vect(4096, 0, 2048), p);
}

static void test_a_point_past_the_near_z_edge_clamps_to_it(void)
{
	const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(2048, 100, -5000));
	assertVect(vect(2048, 0, 0), p);
}

static void test_a_point_past_the_far_z_edge_clamps_to_it(void)
{
	const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(2048, 100, 9000));
	assertVect(vect(2048, 0, 4096), p);
}

static void test_a_point_past_a_corner_clamps_to_that_corner(void)
{
	/* Diagonally outside: both axes have to clamp, not just the first one
	 * tested. */
	const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(-5000, 100, -5000));
	assertVect(vect(0, 0, 0), p);

	const vect3D q = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(-5000, 100, 9000));
	assertVect(vect(0, 0, 4096), q);
}

static void test_the_result_is_always_on_the_rectangle(void)
{
	/*
	 * The property the collision code actually depends on: whatever is asked
	 * for, what comes back is a point of the rectangle. Swept over a grid
	 * that reaches well outside it on both axes.
	 */
	for(int32 x=-8192;x<=12288;x+=1024)
	{
		for(int32 z=-8192;z<=12288;z+=1024)
		{
			const vect3D p = getClosestPointRectangle(FLOOR_ORIGIN, FLOOR_SIZE, vect(x, 4096, z));

			TEST_ASSERT_EQUAL_INT32_MESSAGE(0, p.y, "left the plane of the rectangle");
			TEST_ASSERT_TRUE_MESSAGE(p.x>=0 && p.x<=4096, "outside the rectangle along x");
			TEST_ASSERT_TRUE_MESSAGE(p.z>=0 && p.z<=4096, "outside the rectangle along z");
		}
	}
}

static void test_a_wall_flat_in_z_behaves_the_same_way(void)
{
	/* The other orientation a rectangle can have: extending along x and y
	 * with no depth. */
	const vect3D origin = vect(0, 0, 1000);
	const vect3D size = vect(4096, 4096, 0);

	assertVect(vect(1024, 2048, 1000),
		getClosestPointRectangle(origin, size, vect(1024, 2048, 9999)));
	assertVect(vect(0, 2048, 1000),
		getClosestPointRectangle(origin, size, vect(-5000, 2048, 9999)));
}

static void test_the_offset_of_the_rectangle_is_respected(void)
{
	const vect3D origin = vect(10000, 500, 20000);
	const vect3D size = vect(4096, 0, 4096);

	assertVect(vect(10000, 500, 20000),
		getClosestPointRectangle(origin, size, vect(0, 0, 0)));
	assertVect(vect(11024, 500, 22048),
		getClosestPointRectangle(origin, size, vect(11024, 9999, 22048)));
}

/* --- collideLineConvertedRectangle --------------------------------------- */

/*
 * A ray against a rectangle already in world units - the "converted" in the
 * name. The normal is given separately because the caller has it to hand.
 * The distance limit d is in the same units as the ray direction, which is a
 * unit vector, so d is a length.
 */

static void test_a_ray_straight_at_a_floor_hits_it(void)
{
	vect3D hit;
	const bool r = collideLineConvertedRectangle(
		vect(0, inttof32(1), 0),      /* normal: up */
		FLOOR_ORIGIN, FLOOR_SIZE,
		vect(2048, 8192, 2048),       /* origin: above the middle */
		vect(0, -inttof32(1), 0),     /* direction: down */
		inttof32(100), NULL, &hit);

	TEST_ASSERT_TRUE(r);
	assertVect(vect(2048, 0, 2048), hit);
}

static void test_a_ray_aimed_outside_the_extent_misses(void)
{
	vect3D hit;
	const bool r = collideLineConvertedRectangle(
		vect(0, inttof32(1), 0), FLOOR_ORIGIN, FLOOR_SIZE,
		vect(9000, 8192, 2048),       /* past the far x edge */
		vect(0, -inttof32(1), 0),
		inttof32(100), NULL, &hit);

	TEST_ASSERT_FALSE(r);
}

static void test_a_ray_parallel_to_the_plane_misses(void)
{
	const bool r = collideLineConvertedRectangle(
		vect(0, inttof32(1), 0), FLOOR_ORIGIN, FLOOR_SIZE,
		vect(2048, 8192, 2048),
		vect(inttof32(1), 0, 0),      /* travelling along the surface */
		inttof32(100), NULL, NULL);

	TEST_ASSERT_FALSE(r);
}

static void test_a_ray_pointing_away_misses(void)
{
	/* The intersection is behind the ray's origin, which is a miss rather
	 * than a hit at a negative distance. */
	const bool r = collideLineConvertedRectangle(
		vect(0, inttof32(1), 0), FLOOR_ORIGIN, FLOOR_SIZE,
		vect(2048, 8192, 2048),
		vect(0, inttof32(1), 0),      /* up, away from the floor below */
		inttof32(100), NULL, NULL);

	TEST_ASSERT_FALSE(r);
}

static void test_a_hit_beyond_the_range_limit_misses(void)
{
	/* Same ray as the hit case, but the gun does not reach that far. */
	const bool r = collideLineConvertedRectangle(
		vect(0, inttof32(1), 0), FLOOR_ORIGIN, FLOOR_SIZE,
		vect(2048, inttof32(50), 2048),
		vect(0, -inttof32(1), 0),
		inttof32(10), NULL, NULL);

	TEST_ASSERT_FALSE(r);
}

static void test_a_ray_through_a_corner_of_the_extent(void)
{
	/* The near corner is inside, the far corner is not: the extent test is
	 * inclusive at the origin and exclusive at the far edge. */
	vect3D hit;
	TEST_ASSERT_TRUE(collideLineConvertedRectangle(
		vect(0, inttof32(1), 0), FLOOR_ORIGIN, FLOOR_SIZE,
		vect(0, 8192, 0), vect(0, -inttof32(1), 0), inttof32(100), NULL, &hit));

	TEST_ASSERT_FALSE(collideLineConvertedRectangle(
		vect(0, inttof32(1), 0), FLOOR_ORIGIN, FLOOR_SIZE,
		vect(4096, 8192, 4096), vect(0, -inttof32(1), 0), inttof32(100), NULL, NULL));
}

static void test_the_reported_distance_is_filled_in(void)
{
	int32 k = 0;
	collideLineConvertedRectangle(
		vect(0, inttof32(1), 0), FLOOR_ORIGIN, FLOOR_SIZE,
		vect(2048, inttof32(2), 2048), vect(0, -inttof32(1), 0),
		inttof32(100), &k, NULL);

	/* Two units up, travelling at one unit per step, plus the one unit bias
	 * the function adds so the caller lands just clear of the surface. */
	TEST_ASSERT_INT32_WITHIN(2, inttof32(2), k);
}

static void test_a_ray_against_a_wall_flat_in_x(void)
{
	vect3D hit;
	const bool r = collideLineConvertedRectangle(
		vect(inttof32(1), 0, 0),
		vect(1000, 0, 0), vect(0, 4096, 4096),
		vect(9000, 2048, 2048), vect(-inttof32(1), 0, 0),
		inttof32(100), NULL, &hit);

	TEST_ASSERT_TRUE(r);
	assertVect(vect(1000, 2048, 2048), hit);
}

/* --- getMaxRectangle and fillRectangle ----------------------------------- */

/*
 * The block mesher's inner loop. getMaxRectangle finds the largest solid
 * rectangle of cells whose value has any bit of the mask set; fillRectangle
 * clears that mask over a region. Together they turn a grid of set cells into
 * a small number of large faces, one call at a time.
 *
 * Grids are written out as strings so the shape is visible in the test.
 */
#define GRID_MASK 1

static u8* gridFromRows(const char* const* rows, int w, int h)
{
	u8* data = malloc((size_t)w*h);
	TEST_ASSERT_NOT_NULL(data);
	for(int y=0;y<h;y++)
		for(int x=0;x<w;x++)
			data[x+y*w] = (rows[y][x]=='#') ? GRID_MASK : 0;
	return data;
}

static void test_a_fully_set_grid_is_one_rectangle(void)
{
	static const char* const rows[] = { "####", "####", "####" };
	u8* data = gridFromRows(rows, 4, 3);

	vect2D pos, size;
	getMaxRectangle(data, GRID_MASK, 4, 3, &pos, &size);

	TEST_ASSERT_EQUAL_INT(4*3, size.x*size.y);
	TEST_ASSERT_EQUAL_INT(0, pos.x);
	TEST_ASSERT_EQUAL_INT(0, pos.y);
	free(data);
}

static void test_an_empty_grid_yields_nothing(void)
{
	static const char* const rows[] = { "....", "....", "...." };
	u8* data = gridFromRows(rows, 4, 3);

	vect2D pos, size;
	getMaxRectangle(data, GRID_MASK, 4, 3, &pos, &size);

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, size.x*size.y, "found a rectangle in an empty grid");
	free(data);
}

static void test_a_single_cell_is_found(void)
{
	static const char* const rows[] = { "....", ".#..", "...." };
	u8* data = gridFromRows(rows, 4, 3);

	vect2D pos, size;
	getMaxRectangle(data, GRID_MASK, 4, 3, &pos, &size);

	TEST_ASSERT_EQUAL_INT(1, size.x*size.y);
	TEST_ASSERT_EQUAL_INT(1, pos.x);
	TEST_ASSERT_EQUAL_INT(1, pos.y);
	free(data);
}

static void test_the_largest_rectangle_wins_over_a_longer_thinner_one(void)
{
	/*
	 * A 2x3 block (area 6) next to a 1x4 strip (area 4). The strip is longer
	 * in one dimension, which is exactly what a naive scan would prefer.
	 */
	static const char* const rows[] = {
		"##.#",
		"##.#",
		"##.#",
		"...#",
	};
	u8* data = gridFromRows(rows, 4, 4);

	vect2D pos, size;
	getMaxRectangle(data, GRID_MASK, 4, 4, &pos, &size);

	TEST_ASSERT_EQUAL_INT(6, size.x*size.y);
	free(data);
}

static void test_the_largest_rectangle_of_an_l_shape(void)
{
	static const char* const rows[] = {
		"####",
		"#...",
		"#...",
		"#...",
	};
	u8* data = gridFromRows(rows, 4, 4);

	vect2D pos, size;
	getMaxRectangle(data, GRID_MASK, 4, 4, &pos, &size);

	/* Both arms are four cells long and one wide. */
	TEST_ASSERT_EQUAL_INT(4, size.x*size.y);
	free(data);
}

static void test_filling_a_rectangle_removes_it_from_the_grid(void)
{
	static const char* const rows[] = { "####", "####" };
	u8* data = gridFromRows(rows, 4, 2);

	vect2D pos, size;
	getMaxRectangle(data, GRID_MASK, 4, 2, &pos, &size);
	TEST_ASSERT_EQUAL_INT(8, size.x*size.y);

	fillRectangle(data, 4, 2, &pos, &size, GRID_MASK);

	getMaxRectangle(data, GRID_MASK, 4, 2, &pos, &size);
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, size.x*size.y, "the grid was not cleared");
	free(data);
}

static void test_repeated_find_and_fill_covers_every_set_cell(void)
{
	/*
	 * The property the mesher relies on: alternating getMaxRectangle and
	 * fillRectangle eventually consumes the whole shape, in a finite number
	 * of steps, without ever claiming a cell that was not set.
	 */
	static const char* const rows[] = {
		"##.##",
		"#####",
		".###.",
		"##.##",
	};
	const int w = 5, h = 4;
	u8* data = gridFromRows(rows, w, h);

	int expected = 0;
	for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(data[x+y*w])expected++;

	int covered = 0;
	for(int step=0;step<64;step++)
	{
		vect2D pos, size;
		getMaxRectangle(data, GRID_MASK, w, h, &pos, &size);
		if(size.x*size.y<=0)break;

		/* Every cell it claims must actually have been set. */
		for(int x=pos.x;x<pos.x+size.x;x++)
		{
			for(int y=pos.y;y<pos.y+size.y;y++)
			{
				TEST_ASSERT_TRUE_MESSAGE(x>=0 && x<w && y>=0 && y<h, "claimed a cell outside the grid");
				TEST_ASSERT_TRUE_MESSAGE(data[x+y*w]&GRID_MASK, "claimed a cell that was not set");
			}
		}

		covered += size.x*size.y;
		fillRectangle(data, w, h, &pos, &size, GRID_MASK);
	}

	TEST_ASSERT_EQUAL_INT_MESSAGE(expected, covered, "the shape was not fully covered");
	free(data);
}

/* --- the lightmap atlas packer ------------------------------------------- */

/*
 * Patches are held in a list that keeps itself sorted largest first, then fed
 * to a binary tree packer. Each patch carries a pointer to the
 * lightMapCoordinates_struct the result is written back into, which is what
 * the rest of the lighting code reads.
 */

#define MAX_PATCHES 32

typedef struct
{
	rectangle2DList_struct list;
	lightMapCoordinates_struct coords[MAX_PATCHES];
	int count;
} atlas_struct;

static void atlasInit(atlas_struct* a)
{
	memset(a, 0, sizeof(*a));
	initRectangle2DList(&a->list);
}

static void atlasAdd(atlas_struct* a, int w, int h)
{
	TEST_ASSERT_LESS_THAN_INT(MAX_PATCHES, a->count);

	rectangle2D_struct rec;
	memset(&rec, 0, sizeof(rec));
	rec.size = vect2(w, h);
	rec.real = &a->coords[a->count++];

	insertRectangle2DList(&a->list, rec);
}

/* Walks the packed list, checking every patch is inside the atlas and that no
 * two of them share a pixel. */
static void assertPackingIsSound(atlas_struct* a, int w, int h)
{
	u8* used = calloc((size_t)w*h, 1);
	TEST_ASSERT_NOT_NULL(used);

	for(listCell2D_struct* lc=a->list.first; lc; lc=lc->next)
	{
		const vect2D pos = lc->data.position;
		const vect2D size = lc->data.size;

		TEST_ASSERT_TRUE_MESSAGE(pos.x>=0 && pos.y>=0, "a patch was placed off the atlas");
		TEST_ASSERT_TRUE_MESSAGE(pos.x+size.x<=w && pos.y+size.y<=h, "a patch ran past the edge of the atlas");

		for(int x=pos.x;x<pos.x+size.x;x++)
		{
			for(int y=pos.y;y<pos.y+size.y;y++)
			{
				TEST_ASSERT_FALSE_MESSAGE(used[x+y*w], "two patches overlap in the atlas");
				used[x+y*w] = 1;
			}
		}
	}

	free(used);
}

static void test_the_list_keeps_itself_sorted_largest_first(void)
{
	/* The packer's only heuristic. Inserting out of order must still come
	 * out in order, or the tree fragments and small patches waste the space
	 * a large one needed. */
	atlas_struct a;
	atlasInit(&a);
	atlasAdd(&a, 2, 2);   /* 4 */
	atlasAdd(&a, 8, 8);   /* 64 */
	atlasAdd(&a, 4, 4);   /* 16 */

	int previous = 1<<30;
	int seen = 0;
	for(listCell2D_struct* lc=a.list.first; lc; lc=lc->next)
	{
		const int area = lc->data.size.x*lc->data.size.y;
		TEST_ASSERT_TRUE_MESSAGE(area<=previous, "the list is not sorted largest first");
		previous = area;
		seen++;
	}

	TEST_ASSERT_EQUAL_INT(3, seen);
	TEST_ASSERT_EQUAL_INT(4+64+16, a.list.surface);
	freeRectangle2DList(&a.list);
}

static void test_a_single_patch_is_placed_at_the_origin(void)
{
	atlas_struct a;
	atlasInit(&a);
	atlasAdd(&a, 8, 8);

	packRectangles(&a.list, 32, 32);

	TEST_ASSERT_EQUAL_INT(0, a.list.first->data.position.x);
	TEST_ASSERT_EQUAL_INT(0, a.list.first->data.position.y);
	assertPackingIsSound(&a, 32, 32);
	freeRectangle2DList(&a.list);
}

static void test_patches_that_fit_are_packed_without_overlapping(void)
{
	atlas_struct a;
	atlasInit(&a);
	atlasAdd(&a, 16, 16);
	atlasAdd(&a, 16, 8);
	atlasAdd(&a, 8, 8);
	atlasAdd(&a, 4, 12);
	atlasAdd(&a, 12, 4);

	packRectangles(&a.list, 32, 32);

	assertPackingIsSound(&a, 32, 32);
	freeRectangle2DList(&a.list);
}

static void test_many_small_patches_pack_without_overlapping(void)
{
	atlas_struct a;
	atlasInit(&a);
	for(int i=0;i<MAX_PATCHES;i++)atlasAdd(&a, 4, 4);

	packRectangles(&a.list, 32, 32);

	assertPackingIsSound(&a, 32, 32);
	freeRectangle2DList(&a.list);
}

static void test_the_result_is_written_back_to_the_owner(void)
{
	/* The packed position has to reach the lightMapCoordinates_struct the
	 * patch pointed at - that is the only thing the lighting code reads. */
	atlas_struct a;
	atlasInit(&a);
	atlasAdd(&a, 8, 8);
	atlasAdd(&a, 16, 16);

	packRectangles(&a.list, 32, 32);

	for(listCell2D_struct* lc=a.list.first; lc; lc=lc->next)
	{
		TEST_ASSERT_EQUAL_INT32(lc->data.position.x, lc->data.real->lmPos.x);
		TEST_ASSERT_EQUAL_INT32(lc->data.position.y, lc->data.real->lmPos.y);
	}
	freeRectangle2DList(&a.list);
}

static void test_a_patch_larger_than_the_atlas_cannot_be_placed(void)
{
	atlas_struct a;
	atlasInit(&a);
	atlasAdd(&a, 64, 64);

	tree_struct t;
	initTree(&t, 32, 32);
	const bool r = insertRectangles(&t, &a.list);
	freeTree(&t);

	TEST_ASSERT_FALSE_MESSAGE(r, "a patch bigger than the atlas was reported as placed");
	freeRectangle2DList(&a.list);
}

static void test_a_tall_patch_is_rotated_to_fit_a_wide_gap(void)
{
	/* An 8x24 patch does not fit a 24x8 atlas upright; the packer is allowed
	 * to turn it, and records that it did so the sampler can compensate. */
	atlas_struct a;
	atlasInit(&a);
	atlasAdd(&a, 8, 24);

	tree_struct t;
	initTree(&t, 24, 8);
	const bool r = insertRectangles(&t, &a.list);
	freeTree(&t);

	TEST_ASSERT_TRUE(r);
	TEST_ASSERT_TRUE_MESSAGE(a.list.first->data.rot, "the patch was placed without recording the rotation");
	TEST_ASSERT_TRUE(a.list.first->data.real->rot);
	freeRectangle2DList(&a.list);
}

static void test_the_atlas_search_grows_until_everything_fits(void)
{
	/* packRectanglesSize starts at 32x32 and doubles alternately until the
	 * set fits or it runs out of texture sizes. */
	atlas_struct a;
	atlasInit(&a);
	for(int i=0;i<8;i++)atlasAdd(&a, 16, 16);   /* 2048 pixels, so 32x32 cannot do it */

	short w = 0, h = 0;
	const bool r = packRectanglesSize(&a.list, &w, &h);

	TEST_ASSERT_TRUE_MESSAGE(r, "a set that fits in a legal texture was rejected");
	TEST_ASSERT_TRUE_MESSAGE(w*h >= 8*16*16, "the chosen atlas is too small to hold the patches");
	assertPackingIsSound(&a, w, h);
	freeRectangle2DList(&a.list);
}

static void test_the_atlas_search_gives_up_on_an_impossible_set(void)
{
	/* Past 512x256 there is no larger texture to try. */
	atlas_struct a;
	atlasInit(&a);
	for(int i=0;i<MAX_PATCHES;i++)atlasAdd(&a, 200, 200);

	short w = 0, h = 0;
	const bool r = packRectanglesSize(&a.list, &w, &h);

	TEST_ASSERT_FALSE_MESSAGE(r, "an impossible set was reported as packed");
	freeRectangle2DList(&a.list);
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_a_point_above_the_floor_projects_straight_down);
	RUN_TEST(test_a_point_below_the_floor_projects_straight_up);
	RUN_TEST(test_a_point_in_the_plane_is_returned_unchanged);
	RUN_TEST(test_a_point_past_the_near_x_edge_clamps_to_it);
	RUN_TEST(test_a_point_past_the_far_x_edge_clamps_to_it);
	RUN_TEST(test_a_point_past_the_near_z_edge_clamps_to_it);
	RUN_TEST(test_a_point_past_the_far_z_edge_clamps_to_it);
	RUN_TEST(test_a_point_past_a_corner_clamps_to_that_corner);
	RUN_TEST(test_the_result_is_always_on_the_rectangle);
	RUN_TEST(test_a_wall_flat_in_z_behaves_the_same_way);
	RUN_TEST(test_the_offset_of_the_rectangle_is_respected);

	RUN_TEST(test_a_ray_straight_at_a_floor_hits_it);
	RUN_TEST(test_a_ray_aimed_outside_the_extent_misses);
	RUN_TEST(test_a_ray_parallel_to_the_plane_misses);
	RUN_TEST(test_a_ray_pointing_away_misses);
	RUN_TEST(test_a_hit_beyond_the_range_limit_misses);
	RUN_TEST(test_a_ray_through_a_corner_of_the_extent);
	RUN_TEST(test_the_reported_distance_is_filled_in);
	RUN_TEST(test_a_ray_against_a_wall_flat_in_x);

	RUN_TEST(test_a_fully_set_grid_is_one_rectangle);
	RUN_TEST(test_an_empty_grid_yields_nothing);
	RUN_TEST(test_a_single_cell_is_found);
	RUN_TEST(test_the_largest_rectangle_wins_over_a_longer_thinner_one);
	RUN_TEST(test_the_largest_rectangle_of_an_l_shape);
	RUN_TEST(test_filling_a_rectangle_removes_it_from_the_grid);
	RUN_TEST(test_repeated_find_and_fill_covers_every_set_cell);

	RUN_TEST(test_the_list_keeps_itself_sorted_largest_first);
	RUN_TEST(test_a_single_patch_is_placed_at_the_origin);
	RUN_TEST(test_patches_that_fit_are_packed_without_overlapping);
	RUN_TEST(test_many_small_patches_pack_without_overlapping);
	RUN_TEST(test_the_result_is_written_back_to_the_owner);
	RUN_TEST(test_a_patch_larger_than_the_atlas_cannot_be_placed);
	RUN_TEST(test_a_tall_patch_is_rotated_to_fit_a_wide_gap);
	RUN_TEST(test_the_atlas_search_grows_until_everything_fits);
	RUN_TEST(test_the_atlas_search_gives_up_on_an_impossible_set);

	return UNITY_END();
}
