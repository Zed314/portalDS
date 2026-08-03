/*
 * Whole-room geometry and the rectangle half of the level format -
 * arm9/source/game/room.c.
 *
 * test_levelfile covers the entity section of a level file; this covers what
 * surrounds it. Two different kinds of thing live here:
 *
 *  - the geometry helpers (orientVector, invertRectangle, roomOriginSize,
 *    roomResetOrigin). These are pure functions over a rectangle list and are
 *    what insertRoom leans on to merge a second room in at an offset and an
 *    orientation - which is how multi-room levels are assembled out of
 *    separately edited pieces. An error here does not crash, it puts a room
 *    somewhere slightly wrong, which is exactly the kind of thing that is
 *    easier to catch with an assertion than by eye.
 *
 *  - the rectangle readers, which take their counts from the file with no
 *    upper bound at all. Those tests pin what actually happens rather than
 *    what should - see the comments on each.
 *
 * See tests/host/level_fixture.h for what stands in for the room geometry
 * these build into.
 */

#include "unity.h"
#include "level_fixture.h"

static room_struct room;

void setUp(void)
{
	levelFixtureReset();
	memset(&room, 0, sizeof(room));
}

void tearDown(void)
{
	levelFixtureFreeRectangles(&room);
}

static void assertVect(vect3D expected, vect3D actual)
{
	TEST_ASSERT_EQUAL_INT32(expected.x, actual.x);
	TEST_ASSERT_EQUAL_INT32(expected.y, actual.y);
	TEST_ASSERT_EQUAL_INT32(expected.z, actual.z);
}

/* --- orientVector -------------------------------------------------------- */

/*
 * The four orientations a room can be inserted at. Only 0, 1 and 4 rotate;
 * everything else is the identity, which is what places a room the way it was
 * drawn.
 */

static void test_orientation_zero_is_a_quarter_turn(void)
{
	assertVect(vect(-30, 20, 10), orientVector(vect(10, 20, 30), 0));
}

static void test_orientation_one_is_the_opposite_quarter_turn(void)
{
	assertVect(vect(30, 20, -10), orientVector(vect(10, 20, 30), 1));
}

static void test_orientation_four_mirrors_z(void)
{
	assertVect(vect(10, 20, -30), orientVector(vect(10, 20, 30), 4));
}

static void test_every_other_orientation_is_the_identity(void)
{
	const u8 others[] = { 2, 3, 5, 6, 200, 255 };

	for(unsigned i=0;i<sizeof(others)/sizeof(others[0]);i++)
	{
		assertVect(vect(10, 20, 30), orientVector(vect(10, 20, 30), others[i]));
	}
}

static void test_the_quarter_turns_are_inverses_of_each_other(void)
{
	const vect3D v = vect(7, -11, 13);
	assertVect(v, orientVector(orientVector(v, 0), 1));
	assertVect(v, orientVector(orientVector(v, 1), 0));
}

static void test_the_mirror_is_its_own_inverse(void)
{
	const vect3D v = vect(7, -11, 13);
	assertVect(v, orientVector(orientVector(v, 4), 4));
}

static void test_orientation_never_touches_height(void)
{
	/* y is up, and no room orientation is allowed to tip a room over. */
	for(u8 k=0;k<8;k++)
	{
		TEST_ASSERT_EQUAL_INT32(20, orientVector(vect(10, 20, 30), k).y);
	}
}

/* --- invertRectangle ----------------------------------------------------- */

/*
 * Rotating a rectangle can leave it facing inwards, with a negative extent
 * running back from its origin. invertRectangle moves the origin to the far
 * corner and flips the sign, which is the same rectangle wound the other way.
 * It works on whichever of x and z is non-zero - a rectangle is flat, so only
 * one of them ever is.
 */

static rectangle_struct rectangleAt(vect3D position, vect3D size)
{
	rectangle_struct rec;
	memset(&rec, 0, sizeof(rec));
	rec.position = position;
	rec.size = size;
	return rec;
}

static void test_inverting_a_rectangle_that_runs_along_x(void)
{
	rectangle_struct rec = rectangleAt(vect(100, 0, 500), vect(40, 0, 0));

	invertRectangle(&rec);

	assertVect(vect(140, 0, 500), rec.position);
	assertVect(vect(-40, 0, 0), rec.size);
}

static void test_inverting_a_rectangle_that_runs_along_z(void)
{
	rectangle_struct rec = rectangleAt(vect(100, 0, 500), vect(0, 0, 40));

	invertRectangle(&rec);

	assertVect(vect(100, 0, 540), rec.position);
	assertVect(vect(0, 0, -40), rec.size);
}

static void test_inverting_twice_returns_the_original_rectangle(void)
{
	rectangle_struct rec = rectangleAt(vect(100, 0, 500), vect(40, 0, 0));

	invertRectangle(&rec);
	invertRectangle(&rec);

	assertVect(vect(100, 0, 500), rec.position);
	assertVect(vect(40, 0, 0), rec.size);
}

static void test_inverting_tolerates_null(void)
{
	invertRectangle(NULL);
}

/* --- roomOriginSize and roomResetOrigin ---------------------------------- */

static void test_the_bounds_of_a_single_rectangle(void)
{
	levelFixturePushRectangle(&room, vect(100, 200, 300), vect(40, 0, 60), vect(0, 4096, 0));

	vect3D origin, size;
	roomOriginSize(&room, &origin, &size);

	assertVect(vect(100, 200, 300), origin);
	assertVect(vect(40, 0, 60), size);
}

static void test_the_bounds_span_every_rectangle(void)
{
	levelFixturePushRectangle(&room, vect(100, 200, 300), vect(40, 0, 60), vect(0, 4096, 0));
	levelFixturePushRectangle(&room, vect(50, 100, 400), vect(10, 0, 10), vect(0, 4096, 0));

	vect3D origin, size;
	roomOriginSize(&room, &origin, &size);

	assertVect(vect(50, 100, 300), origin);
	/* x: 140-50, y: 200-100, z: 410-300. */
	assertVect(vect(90, 100, 110), size);
}

static void test_bounds_account_for_rectangles_wound_backwards(void)
{
	/* A negative extent, as invertRectangle produces. The far corner is
	 * behind the origin, and both have to be considered. */
	levelFixturePushRectangle(&room, vect(500, 0, 500), vect(-100, 0, 0), vect(0, 4096, 0));

	vect3D origin, size;
	roomOriginSize(&room, &origin, &size);

	assertVect(vect(400, 0, 500), origin);
	TEST_ASSERT_EQUAL_INT32(100, size.x);
}

static void test_asking_for_only_the_origin_is_allowed(void)
{
	levelFixturePushRectangle(&room, vect(100, 200, 300), vect(40, 0, 60), vect(0, 4096, 0));

	vect3D origin = vect(-1, -1, -1);
	roomOriginSize(&room, &origin, NULL);

	assertVect(vect(100, 200, 300), origin);
}

static void test_bounds_tolerate_null(void)
{
	vect3D origin;
	roomOriginSize(NULL, &origin, NULL);
	roomOriginSize(&room, NULL, NULL);
}

static void test_the_bounds_accumulators_start_seeded(void)
{
	/*
	 * Pinning a limitation, not asserting a virtue. roomOriginSize seeds its
	 * running minimum at 8192 and its running maximum at 0, rather than at
	 * the first rectangle. So a room built entirely beyond 8192 reports an
	 * origin of 8192 - the seed, not anything in the room - and one built
	 * entirely in negative space reports a maximum of 0.
	 *
	 * Harmless for real levels, which the editor lays out from the origin
	 * within those bounds, and load bearing for nothing: this is here so that
	 * anyone who widens the coordinate range finds out what else has to move.
	 */
	levelFixturePushRectangle(&room, vect(20000, 20000, 20000), vect(10, 0, 10), vect(0, 4096, 0));

	vect3D origin, size;
	roomOriginSize(&room, &origin, &size);

	assertVect(vect(8192, 8192, 8192), origin);
	TEST_ASSERT_EQUAL_INT32(20010 - 8192, size.x);
}

static void test_resetting_the_origin_moves_the_room_to_zero(void)
{
	levelFixturePushRectangle(&room, vect(1000, 2000, 3000), vect(40, 0, 60), vect(0, 4096, 0));
	levelFixturePushRectangle(&room, vect(1100, 2100, 3100), vect(40, 0, 60), vect(0, 4096, 0));

	roomResetOrigin(&room);

	vect3D origin, size;
	roomOriginSize(&room, &origin, &size);

	assertVect(vect(0, 0, 0), origin);
	/* The shape is unchanged, only where it sits. */
	assertVect(vect(140, 100, 160), size);
}

static void test_resetting_the_origin_clears_the_room_position(void)
{
	room.position = vect(500, 600, 700);
	levelFixturePushRectangle(&room, vect(1000, 2000, 3000), vect(40, 0, 60), vect(0, 4096, 0));

	roomResetOrigin(&room);

	assertVect(vect(0, 0, 0), room.position);
}

static void test_resetting_an_empty_room_is_harmless(void)
{
	roomResetOrigin(&room);
	roomResetOrigin(NULL);
}

/* --- the rectangle readers ----------------------------------------------- */

/*
 * A rectangle on disk, as writeRectangle() in editor/io.c produces it:
 * position, size and normal, then whether it is portalable, then a material
 * id the game reader does not use.
 */
static void putRectangle(FILE* f, vect3D position, vect3D size, vect3D normal, bool portalable)
{
	const u16 materialId = 7;
	fwrite(&position, sizeof(vect3D), 1, f);
	fwrite(&size, sizeof(vect3D), 1, f);
	fwrite(&normal, sizeof(vect3D), 1, f);
	fwrite(&portalable, sizeof(bool), 1, f);
	fwrite(&materialId, sizeof(u16), 1, f);
}

static void test_rectangles_are_read_into_the_room(void)
{
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL(f);
	putRectangle(f, vect(100, 200, 300), vect(40, 0, 60), vect(0, 4096, 0), true);
	putRectangle(f, vect(500, 600, 700), vect(10, 0, 20), vect(4096, 0, 0), false);
	rewind(f);

	/* readRectangles takes its count from the room rather than the stream -
	 * newReadMap reads it out of the file and puts it there first. */
	room.rectangles.num = 2;
	readRectangles(&room, f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(2, room.rectangles.num);

	/* The list pushes at the front, so the last one read is at the head. */
	assertVect(vect(500, 600, 700), room.rectangles.first->data.position);
	assertVect(vect(100, 200, 300), room.rectangles.first->next->data.position);
}

static void test_a_rectangles_material_comes_from_its_portalability(void)
{
	/*
	 * The stored material id is read and thrown away: the game picks material
	 * 1 for a portalable surface and 2 for anything else. Pinned because it
	 * is surprising - the field is still in the format, and the editor still
	 * writes a meaningful value into it.
	 */
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL(f);
	putRectangle(f, vect(0,0,0), vect(10,0,10), vect(0,4096,0), true);
	rewind(f);

	room.rectangles.num = 1;
	readRectangles(&room, f);
	fclose(f);

	TEST_ASSERT_NOT_NULL(room.rectangles.first);
	TEST_ASSERT_TRUE(room.rectangles.first->data.portalable);
}

static void test_reading_no_rectangles_leaves_the_room_empty(void)
{
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL(f);
	rewind(f);

	room.rectangles.num = 0;
	readRectangles(&room, f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(0, room.rectangles.num);
	TEST_ASSERT_NULL(room.rectangles.first);
}

static void test_the_rectangle_count_is_not_bounded(void)
{
	/*
	 * Pinning a real weakness. Unlike the entity count, nothing checks the
	 * rectangle count against anything - it is an int straight out of the
	 * file, and every iteration allocates a list cell. The loop below asks
	 * for two hundred rectangles from a file holding three, and gets two
	 * hundred: a hundred and ninety seven of them built from whatever fread
	 * left in the destination when it hit EOF.
	 *
	 * That is memory safe - the list grows, nothing is overrun - which is why
	 * it is pinned rather than fixed here. It is not harmless: a count of a
	 * few million would allocate until the DS ran out, and there is no
	 * natural cap to clamp to the way NUMENTITIES caps entities. Bounding it
	 * properly means deciding what a room's rectangle budget is.
	 */
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL(f);
	for(int i=0;i<3;i++)putRectangle(f, vect(i,i,i), vect(10,0,10), vect(0,4096,0), false);
	rewind(f);

	room.rectangles.num = 200;
	readRectangles(&room, f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(200, room.rectangles.num);
}

static void test_sludge_rectangles_are_read(void)
{
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL(f);
	const int count = 3;
	fwrite(&count, sizeof(int), 1, f);
	for(int i=0;i<count;i++)putRectangle(f, vect(i,0,i), vect(10,0,10), vect(0,4096,0), false);
	rewind(f);

	readSludgeRectangles(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(3, levelFixtureCounts.sludgeRectangles);
}

static void test_a_negative_sludge_count_reads_nothing(void)
{
	/* The count is signed here, unlike the entity count. A negative one is
	 * the loop condition failing immediately rather than anything worse. */
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL(f);
	const int count = -5;
	fwrite(&count, sizeof(int), 1, f);
	rewind(f);

	readSludgeRectangles(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(0, levelFixtureCounts.sludgeRectangles);
}

static void test_the_rectangle_readers_tolerate_null(void)
{
	readRectangles(NULL, NULL);
	readSludgeRectangles(NULL);
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_orientation_zero_is_a_quarter_turn);
	RUN_TEST(test_orientation_one_is_the_opposite_quarter_turn);
	RUN_TEST(test_orientation_four_mirrors_z);
	RUN_TEST(test_every_other_orientation_is_the_identity);
	RUN_TEST(test_the_quarter_turns_are_inverses_of_each_other);
	RUN_TEST(test_the_mirror_is_its_own_inverse);
	RUN_TEST(test_orientation_never_touches_height);

	RUN_TEST(test_inverting_a_rectangle_that_runs_along_x);
	RUN_TEST(test_inverting_a_rectangle_that_runs_along_z);
	RUN_TEST(test_inverting_twice_returns_the_original_rectangle);
	RUN_TEST(test_inverting_tolerates_null);

	RUN_TEST(test_the_bounds_of_a_single_rectangle);
	RUN_TEST(test_the_bounds_span_every_rectangle);
	RUN_TEST(test_bounds_account_for_rectangles_wound_backwards);
	RUN_TEST(test_asking_for_only_the_origin_is_allowed);
	RUN_TEST(test_bounds_tolerate_null);
	RUN_TEST(test_the_bounds_accumulators_start_seeded);
	RUN_TEST(test_resetting_the_origin_moves_the_room_to_zero);
	RUN_TEST(test_resetting_the_origin_clears_the_room_position);
	RUN_TEST(test_resetting_an_empty_room_is_harmless);

	RUN_TEST(test_rectangles_are_read_into_the_room);
	RUN_TEST(test_a_rectangles_material_comes_from_its_portalability);
	RUN_TEST(test_reading_no_rectangles_leaves_the_room_empty);
	RUN_TEST(test_the_rectangle_count_is_not_bounded);
	RUN_TEST(test_sludge_rectangles_are_read);
	RUN_TEST(test_a_negative_sludge_count_reads_nothing);
	RUN_TEST(test_the_rectangle_readers_tolerate_null);

	return UNITY_END();
}
