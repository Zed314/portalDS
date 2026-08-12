/*
 * Level file reader - arm9/source/game/room.c and arm9/source/game/levelinfo.c.
 *
 * This is the only code in the game that parses data the player did not
 * produce: levels are downloaded from the project's webpage, so every count,
 * offset and string in a .map and its sidecar .ini is hostile until proven
 * otherwise. These tests feed the reader files a level editor would never
 * write and assert that it neither corrupts memory nor believes them.
 *
 * The memory-safety half of that is enforced by the sanitizers rather than by
 * an assertion, because both regressions these cover were a write past the
 * end of a fixed buffer - something a test that merely *ran* would not notice.
 * Against the unfixed code:
 *
 *  - the entity count tests die on UBSan's bounds check, "index 64 out of
 *    bounds for type 's16 [64]'" at the entityTargetArray write in
 *    readEntity();
 *  - the title and author tests die in glibc's fortified strcpy, "buffer
 *    overflow detected".
 *
 * Both are aborts, which -fno-sanitize-recover=all makes fatal. Run with
 * SANITIZE=0 and the count tests still fail on their assertions, but the
 * string ones become a coin toss - so this suite is worth much less without
 * the sanitizers than the others are.
 *
 * See tests/host/level_fixture.h for what stands in for the entity pools.
 */

#include <unistd.h>

#include "unity.h"
#include "level_fixture.h"

/* --- building level files ------------------------------------------------ */

/*
 * Entity records, in the layout writeEntity() in editor/io.c produces: a type
 * tag, the position, a direction, then a per-type payload. The payloads are
 * spelled out one function per type below rather than shared, because that is
 * how the format is actually specified - positionally, with no
 * self-description - and a helper that unified them would hide exactly the
 * kind of drift these tests exist to catch.
 */
enum
{
	ENTITY_CATCHER = 0, ENTITY_LAUNCHER = 1, ENTITY_TIMED_BUTTON = 2,
	ENTITY_PRESSURE_BUTTON = 3, ENTITY_TURRET = 4, ENTITY_CUBE = 5,
	ENTITY_COMPANION_CUBE = 6, ENTITY_DISPENSER = 7, ENTITY_GRID = 8,
	ENTITY_PLATFORM = 9, ENTITY_DOOR = 10, ENTITY_LIGHT = 11,
	ENTITY_PLATFORM_TARGET = 12, ENTITY_WALLDOOR_START = 13,
	ENTITY_WALLDOOR_EXIT = 14
};

static void putPrefix(FILE* f, u8 type, u8 dir)
{
	const vect3D editorPos = vect(1, 2, 3); /* the editor-space position, which the game reader discards */
	fwrite(&type, sizeof(u8), 1, f);
	fwrite(&editorPos, sizeof(vect3D), 1, f);
	fwrite(&dir, sizeof(u8), 1, f);
}

static void putVect(FILE* f, vect3D v)   { fwrite(&v, sizeof(vect3D), 1, f); }
static void putU8(FILE* f, u8 v)         { fwrite(&v, sizeof(u8), 1, f); }
static void putS16(FILE* f, s16 v)       { fwrite(&v, sizeof(s16), 1, f); }
static void putInt32(FILE* f, int32 v)   { fwrite(&v, sizeof(int32), 1, f); }

static void putLight(FILE* f, vect3D pos)
{
	putPrefix(f, ENTITY_LIGHT, 0);
	putVect(f, pos);
}

static void putCatcher(FILE* f, vect3D pos, u8 dir, s16 target)
{
	putPrefix(f, ENTITY_CATCHER, dir);
	putVect(f, pos);
	putS16(f, target);
}

static void putLauncher(FILE* f, vect3D pos, u8 dir)
{
	putPrefix(f, ENTITY_LAUNCHER, dir);
	putVect(f, pos);
}

static void putTimedButton(FILE* f, vect3D pos, u8 orientation, s16 target)
{
	putPrefix(f, ENTITY_TIMED_BUTTON, 0);
	putVect(f, pos);
	putU8(f, orientation);
	putS16(f, target);
}

static void putPressureButton(FILE* f, vect3D pos, s16 target)
{
	putPrefix(f, ENTITY_PRESSURE_BUTTON, 0);
	putVect(f, pos);
	putS16(f, target);
}

static void putTurret(FILE* f, vect3D pos, u8 facing)
{
	putPrefix(f, ENTITY_TURRET, 0);
	putVect(f, pos);
	putU8(f, facing);
}

static void putCube(FILE* f, u8 type, vect3D pos, s16 target)
{
	putPrefix(f, type, 0);
	putVect(f, pos);
	putS16(f, target);
}

static void putDispenser(FILE* f, vect3D pos, s16 target)
{
	putPrefix(f, ENTITY_DISPENSER, 0);
	putVect(f, pos);
	putS16(f, target);
}

static void putGrid(FILE* f, vect3D pos, u8 dir, int32 length)
{
	putPrefix(f, ENTITY_GRID, dir);
	putInt32(f, length); /* note: the length comes before the position here */
	putVect(f, pos);
}

static void putPlatform(FILE* f, vect3D orig, vect3D dest, s16 target)
{
	putPrefix(f, ENTITY_PLATFORM, 0);
	putVect(f, orig);
	putVect(f, dest);
	putS16(f, target);
}

static void putDoor(FILE* f, vect3D pos, u8 orientation)
{
	putPrefix(f, ENTITY_DOOR, 0);
	putVect(f, pos);
	putU8(f, orientation);
}

static void putPlatformTarget(FILE* f, s16 target)
{
	putPrefix(f, ENTITY_PLATFORM_TARGET, 0);
	putS16(f, target);
}

static void putWallDoor(FILE* f, u8 type, vect3D pos, u8 orientation)
{
	putPrefix(f, type, orientation);
	putVect(f, pos);
	putU8(f, orientation);
}

/* Opens a section, leaving room for the count, which finishEntities patches
 * in once the caller has written as many records as it wanted. */
static FILE* beginEntities(void)
{
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL_MESSAGE(f, "could not create a temporary file");
	const u16 placeholder = 0;
	fwrite(&placeholder, sizeof(u16), 1, f);
	return f;
}

static void finishEntities(FILE* f, u16 count)
{
	fseek(f, 0, SEEK_SET);
	fwrite(&count, sizeof(u16), 1, f);
	rewind(f);
}

/* A section holding exactly one entity, for the per-type tests. */
#define ONE_ENTITY(writerCall) do { \
		FILE* f = beginEntities(); \
		writerCall; \
		finishEntities(f, 1); \
		readEntities(f); \
		fclose(f); \
	} while(0)

static void writeLightEntity(FILE* f, int32 x, int32 y, int32 z)
{
	putLight(f, vect(x, y, z));
}

/*
 * An entity section: a u16 count followed by that many records. claimed and
 * actual are separate so a test can write a header that lies about how much
 * follows it, which is the shape of a hand edited or truncated file.
 */
static FILE* entitySection(u16 claimed, int actual)
{
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL_MESSAGE(f, "could not create a temporary file");

	fwrite(&claimed, sizeof(u16), 1, f);
	for(int i=0;i<actual;i++)writeLightEntity(f, i*4096, 4096, i*8192);

	rewind(f);
	return f;
}

/* A path under /tmp unique to this process, for the tests that need a real
 * filename rather than a stream. */
static const char* tempPath(void)
{
	static char path[64];
	snprintf(path, sizeof(path), "/tmp/portalds_test_levelfile_%d.ini", (int)getpid());
	return path;
}

static void writeTextFile(const char* path, const char* contents)
{
	FILE* f = fopen(path, "w");
	TEST_ASSERT_NOT_NULL_MESSAGE(f, "could not create the temporary ini");
	fputs(contents, f);
	fclose(f);
}

/* --- fixture ------------------------------------------------------------- */

void setUp(void)    { levelFixtureReset(); }
void tearDown(void) { unlink(tempPath()); }

/* --- entity counts ------------------------------------------------------- */

static void test_a_normal_entity_section_is_read_whole(void)
{
	FILE* f = entitySection(5, 5);
	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(5, levelFixtureCounts.entities);
	TEST_ASSERT_EQUAL_INT(5, levelFixtureCounts.lights);
}

static void test_a_full_entity_section_is_read_whole(void)
{
	/* Exactly the pool size must still be accepted in full - an off by one in
	 * the cap would show up here and nowhere else. */
	FILE* f = entitySection(NUMENTITIES, NUMENTITIES);
	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(NUMENTITIES, levelFixtureCounts.entities);
}

static void test_an_empty_entity_section_creates_nothing(void)
{
	FILE* f = entitySection(0, 0);
	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(0, levelFixtureCounts.entities);
}

static void test_a_count_past_the_pool_is_capped(void)
{
	/* The regression: the wiring tables in room.c are NUMENTITIES long and
	 * nothing checked the count against them, so the 65th entity wrote past
	 * entityTargetArray. ASan is what fails this if the cap goes away. */
	FILE* f = entitySection(NUMENTITIES+1, NUMENTITIES+1);
	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(NUMENTITIES, levelFixtureCounts.entities);
}

static void test_a_wildly_oversized_count_is_capped(void)
{
	FILE* f = entitySection(5000, 5000);
	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(NUMENTITIES, levelFixtureCounts.entities);
}

static void test_the_largest_representable_count_is_capped(void)
{
	/* 65535 is the worst a u16 count can say. It also used to be the case
	 * that the index was truncated to a u8 on the way into readEntity(), so
	 * past 256 the writes wrapped around and landed back at the start of the
	 * tables - corruption that ASan cannot see. Capping is what stops both. */
	FILE* f = entitySection(65535, 300);
	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(NUMENTITIES, levelFixtureCounts.entities);
}

static void test_a_count_with_no_entities_behind_it_is_survivable(void)
{
	/* A truncated file: the header promises entities that are not there.
	 * fread fails at EOF and leaves the destination untouched, so the reader
	 * builds whatever the uninitialised bytes decode to - the point of the
	 * test is that it stays inside its tables while doing it. */
	FILE* f = entitySection(NUMENTITIES+50, 0);
	readEntities(f);
	fclose(f);

	TEST_ASSERT_LESS_OR_EQUAL_INT(NUMENTITIES, levelFixtureCounts.entities);
}

static void test_a_truncated_count_creates_nothing(void)
{
	/* Not even the two byte count is present. This used to read cnt
	 * uninitialised and loop that many times. */
	FILE* f = tmpfile();
	TEST_ASSERT_NOT_NULL(f);
	const u8 half = 0x40;
	fwrite(&half, sizeof(u8), 1, f); /* one byte of a two byte count */
	rewind(f);

	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(0, levelFixtureCounts.entities);
}

static void test_capping_leaves_the_tables_beyond_the_cap_alone(void)
{
	/* The cap is a bound on the tables, not just on the loop: nothing past
	 * the last slot may have been written, and the last slot itself must
	 * still have been. */
	FILE* f = entitySection(5000, 5000);
	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT16(-1, entityTargetArray[NUMENTITIES-1]);
}

static void test_a_null_stream_is_ignored(void)
{
	readEntities(NULL);
	TEST_ASSERT_EQUAL_INT(0, levelFixtureCounts.entities);
}


/* --- entity payloads ----------------------------------------------------- */

/*
 * One test per entity type, checking that the reader takes the right values
 * out of the right places. These are the other half of the format: the count
 * tests above check the reader does not overrun its tables, these check it
 * agrees with editor/io.c about what a record contains. The file header in
 * room.c is explicit that the two must be kept in step by hand, and that a
 * mismatch "silently corrupts every entity after it".
 */

static void assertCreated(int index, createdKind_type kind)
{
	TEST_ASSERT_GREATER_THAN_INT_MESSAGE(index, levelFixtureCreatedCount, "fewer entities created than expected");
	TEST_ASSERT_EQUAL_INT_MESSAGE(kind, levelFixtureCreated[index].kind, "wrong kind of entity created");
}

static void assertPosition(int index, vect3D expected)
{
	TEST_ASSERT_EQUAL_INT32(expected.x, levelFixtureCreated[index].pos.x);
	TEST_ASSERT_EQUAL_INT32(expected.y, levelFixtureCreated[index].pos.y);
	TEST_ASSERT_EQUAL_INT32(expected.z, levelFixtureCreated[index].pos.z);
}

static void test_a_catcher_carries_its_position_and_direction(void)
{
	const vect3D pos = vect(4096, 8192, -12288);

	ONE_ENTITY(putCatcher(f, pos, 3, -1));

	assertCreated(0, CREATED_ENERGY_DEVICE);
	assertPosition(0, pos);
	TEST_ASSERT_EQUAL_INT32(3, levelFixtureCreated[0].param);
	/* The device type doubles as the entity tag: 0 is a catcher. */
	TEST_ASSERT_FALSE(levelFixtureCreated[0].flag);
}

static void test_a_launcher_is_the_same_record_with_the_other_type(void)
{
	const vect3D pos = vect(-4096, 0, 4096);

	ONE_ENTITY(putLauncher(f, pos, 1));

	assertCreated(0, CREATED_ENERGY_DEVICE);
	assertPosition(0, pos);
	TEST_ASSERT_EQUAL_INT32(1, levelFixtureCreated[0].param);
	TEST_ASSERT_TRUE(levelFixtureCreated[0].flag);
}

static void test_a_timed_button_converts_its_setting_to_an_angle(void)
{
	/* The file stores a small setting; the reader turns it into (d+2)*8192,
	 * which is the countdown the button is created with. */
	const vect3D pos = vect(0, 0, 0);

	ONE_ENTITY(putTimedButton(f, pos, 5, -1));

	assertCreated(0, CREATED_TIMED_BUTTON);
	TEST_ASSERT_EQUAL_INT32((5+2)*8192, levelFixtureCreated[0].param);
}

static void test_a_pressure_button_carries_its_position(void)
{
	const vect3D pos = vect(1024, 2048, 3072);

	ONE_ENTITY(putPressureButton(f, pos, -1));

	assertCreated(0, CREATED_BIG_BUTTON);
	assertPosition(0, pos);
}

static void test_a_turret_carries_its_facing(void)
{
	const vect3D pos = vect(768, 384, 192);

	ONE_ENTITY(putTurret(f, pos, 2));

	assertCreated(0, CREATED_TURRET);
	assertPosition(0, pos);
	TEST_ASSERT_EQUAL_INT32(2, levelFixtureCreated[0].param);
}

static void test_cubes_are_read_but_not_created(void)
{
	/* Types 5 and 6 have a full record and no create* call - the cubes are
	 * spawned by their dispenser, not by the level. The record still has to be
	 * consumed, which the mixed sequence test below is what really proves. */
	ONE_ENTITY(putCube(f, ENTITY_CUBE, vect(0,0,0), -1));
	TEST_ASSERT_EQUAL_INT(0, levelFixtureCreatedCount);

	levelFixtureReset();
	ONE_ENTITY(putCube(f, ENTITY_COMPANION_CUBE, vect(0,0,0), -1));
	TEST_ASSERT_EQUAL_INT(0, levelFixtureCreatedCount);
}

static void test_a_dispenser_is_created_holding_a_companion_cube(void)
{
	const vect3D pos = vect(4096, 4096, 4096);

	ONE_ENTITY(putDispenser(f, pos, -1));

	assertCreated(0, CREATED_CUBE_DISPENSER);
	assertPosition(0, pos);
	TEST_ASSERT_TRUE(levelFixtureCreated[0].flag);
}

static void test_a_grid_reads_its_length_before_its_position(void)
{
	/* The odd one out: every other type puts the position first. Getting this
	 * backwards would misread the length as a coordinate and shift everything
	 * after it by four bytes. */
	const vect3D pos = vect(8192, 4096, 2048);

	ONE_ENTITY(putGrid(f, pos, 0, 6144));

	assertCreated(0, CREATED_GRID);
	assertPosition(0, pos);
	TEST_ASSERT_EQUAL_INT32(6144, levelFixtureCreated[0].param);
}

static void test_a_grid_takes_its_sign_and_side_from_the_direction(void)
{
	/* dir picks both the sign of the length (odd flips it) and which side the
	 * grid faces (2 and up is the far side). */
	const struct { u8 dir; int32 length; bool side; } cases[] = {
		{ 0, 6144, false }, { 1, -6144, false }, { 2, 6144, true }, { 3, -6144, true },
	};

	for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
	{
		levelFixtureReset();
		ONE_ENTITY(putGrid(f, vect(0,0,0), cases[i].dir, 6144));

		assertCreated(0, CREATED_GRID);
		TEST_ASSERT_EQUAL_INT32(cases[i].length, levelFixtureCreated[0].param);
		TEST_ASSERT_EQUAL(cases[i].side, levelFixtureCreated[0].flag);
	}
}

static void test_a_platform_carries_both_ends_of_its_run(void)
{
	const vect3D orig = vect(0, 0, 0);
	const vect3D dest = vect(0, 16384, 0);

	ONE_ENTITY(putPlatform(f, orig, dest, -1));

	assertCreated(0, CREATED_PLATFORM);
	assertPosition(0, orig);
	TEST_ASSERT_EQUAL_INT32(dest.y, levelFixtureCreated[0].pos2.y);
	TEST_ASSERT_TRUE(levelFixtureCreated[0].flag); /* platforms always run back and forth */
}

static void test_a_door_orientation_is_reduced_to_an_axis(void)
{
	/* The file can hold any of the six face directions; a door only has two
	 * orientations, so the reader takes it modulo 2. */
	const struct { u8 stored; int32 expected; } cases[] = {
		{ 0, 0 }, { 1, 1 }, { 4, 0 }, { 5, 1 },
	};

	for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
	{
		levelFixtureReset();
		ONE_ENTITY(putDoor(f, vect(0,0,0), cases[i].stored));

		assertCreated(0, CREATED_DOOR);
		TEST_ASSERT_EQUAL_INT32(cases[i].expected, levelFixtureCreated[0].param);
	}
}

static void test_a_light_is_created_at_the_standard_intensity(void)
{
	const vect3D pos = vect(2048, 6144, -2048);

	ONE_ENTITY(putLight(f, pos));

	assertCreated(0, CREATED_LIGHT);
	assertPosition(0, pos);
	/* Levels do not store an intensity; every light gets the same one. */
	TEST_ASSERT_EQUAL_INT32(TILESIZE*2*16, levelFixtureCreated[0].param);
}

static void test_a_platform_target_is_read_but_not_created(void)
{
	ONE_ENTITY(putPlatformTarget(f, 3));
	TEST_ASSERT_EQUAL_INT(0, levelFixtureCreatedCount);
}

static void test_the_two_wall_doors_go_to_their_own_slots(void)
{
	FILE* f = beginEntities();
	putWallDoor(f, ENTITY_WALLDOOR_START, vect(4096, 0, 0), 0);
	putWallDoor(f, ENTITY_WALLDOOR_EXIT, vect(-4096, 0, 0), 1);
	finishEntities(f, 2);
	readEntities(f);
	fclose(f);

	assertCreated(0, CREATED_WALLDOOR);
	assertCreated(1, CREATED_WALLDOOR);
	/* The fixture sets flag when the reader passed exitWallDoor rather than
	 * entryWallDoor - the two are separate globals and swapping them would
	 * put the player at the end of the chamber. */
	TEST_ASSERT_FALSE(levelFixtureCreated[0].flag);
	TEST_ASSERT_TRUE(levelFixtureCreated[1].flag);
	TEST_ASSERT_TRUE_MESSAGE(exitWallDoor.override, "the exit door is forced open on load");
}

static void test_the_arrival_door_places_the_player(void)
{
	/* setupWallDoor leaving used set is what sends the reader down the branch
	 * that puts the player on the arrival lift. */
	ONE_ENTITY(putWallDoor(f, ENTITY_WALLDOOR_START, vect(0, 0, 0), 0));

	TEST_ASSERT_TRUE(entryWallDoor.used);
	TEST_ASSERT_NOT_EQUAL_INT32(0, getPlayer()->object->position.y);
}

static void test_a_mixed_sequence_reads_every_record_at_the_right_offset(void)
{
	/*
	 * The strongest thing this suite asserts. Every entity type is written
	 * once, in order, followed by a light at a position nothing else uses.
	 * The format is positional with no self-description, so if the reader
	 * disagrees with the writer about the size of *any* record, the stream
	 * desynchronises and that final light comes out somewhere else - or as
	 * some other kind of entity entirely.
	 *
	 * That is precisely the failure the comment at the top of room.c warns
	 * about, and the reason to add a case here whenever an entity is added.
	 */
	const vect3D sentinel = vect(123456, -654321, 111111);

	FILE* f = beginEntities();
	putCatcher(f,         vect(1,1,1),  0, -1);
	putLauncher(f,        vect(2,2,2),  1);
	putTimedButton(f,     vect(3,3,3),  1, -1);
	putPressureButton(f,  vect(4,4,4),  -1);
	putTurret(f,          vect(5,5,5),  0);
	putCube(f, ENTITY_CUBE,           vect(6,6,6), -1);
	putCube(f, ENTITY_COMPANION_CUBE, vect(7,7,7), -1);
	putDispenser(f,       vect(8,8,8),  -1);
	putGrid(f,            vect(9,9,9),  0, 4096);
	putPlatform(f,        vect(10,10,10), vect(11,11,11), -1);
	putDoor(f,            vect(12,12,12), 0);
	putLight(f,           vect(13,13,13));
	putPlatformTarget(f,  -1);
	putWallDoor(f, ENTITY_WALLDOOR_START, vect(14,14,14), 0);
	putWallDoor(f, ENTITY_WALLDOOR_EXIT,  vect(15,15,15), 1);
	putLight(f, sentinel);
	finishEntities(f, 16);

	readEntities(f);
	fclose(f);

	/* Types 5, 6 and 12 create nothing, so 16 records make 13 entities. */
	static const createdKind_type expected[] = {
		CREATED_ENERGY_DEVICE, CREATED_ENERGY_DEVICE, CREATED_TIMED_BUTTON,
		CREATED_BIG_BUTTON, CREATED_TURRET, CREATED_CUBE_DISPENSER,
		CREATED_GRID, CREATED_PLATFORM, CREATED_DOOR, CREATED_LIGHT,
		CREATED_WALLDOOR, CREATED_WALLDOOR, CREATED_LIGHT
	};
	const int n = (int)(sizeof(expected)/sizeof(expected[0]));

	TEST_ASSERT_EQUAL_INT_MESSAGE(n, levelFixtureCreatedCount,
		"a record size disagrees with editor/io.c - the stream desynchronised");
	for(int i=0;i<n;i++)assertCreated(i, expected[i]);

	assertPosition(n-1, sentinel);
}

static void test_an_unknown_entity_type_desynchronises_the_stream(void)
{
	/*
	 * Pinning a real weakness rather than asserting something good. An
	 * unrecognised tag falls through readEntity's default case, which
	 * consumes only the 14 byte prefix and no payload - so every record after
	 * it is misaligned. A level written by a newer editor would not fail
	 * loudly here, it would load as nonsense.
	 *
	 * Nothing is corrupted, which is why this is pinned rather than fixed:
	 * the reader stays inside its tables throughout. Making it stop at the
	 * first unknown tag would be a format decision, not a bug fix.
	 */
	FILE* f = beginEntities();
	putPrefix(f, 200, 0);           /* not a type any editor writes */
	putVect(f, vect(0,0,0));        /* a payload the reader will not consume */
	putLight(f, vect(4096,4096,4096));
	finishEntities(f, 2);

	readEntities(f);
	fclose(f);

	/* The light that followed did not survive as a light. */
	TEST_ASSERT_FALSE(levelFixtureCreatedCount == 1 && levelFixtureCreated[0].kind == CREATED_LIGHT);
}

/* --- trigger wiring ------------------------------------------------------ */

static void test_a_button_is_wired_to_the_door_it_targets(void)
{
	/* Entity 0 is a pressure button naming entity 1; entity 1 is a door.
	 * addEntityTarget resolves that index back into a pointer, which is what
	 * makes the button open the door. */
	FILE* f = beginEntities();
	putPressureButton(f, vect(0,0,0), 1);
	putDoor(f, vect(4096,0,0), 0);
	finishEntities(f, 2);

	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT16(1, entityTargetArray[0]);
	TEST_ASSERT_NOT_NULL_MESSAGE(entityActivatorArray[0], "the button registered no activator");
	TEST_ASSERT_EQUAL_INT_MESSAGE(1, levelFixtureCounts.activatorTargets, "the button was not wired to the door");
}

static void test_an_entity_targeting_nothing_is_wired_to_nothing(void)
{
	FILE* f = beginEntities();
	putPressureButton(f, vect(0,0,0), -1);
	putDoor(f, vect(4096,0,0), 0);
	finishEntities(f, 2);

	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT16(-1, entityTargetArray[0]);
	TEST_ASSERT_EQUAL_INT(0, levelFixtureCounts.activatorTargets);
}

static void test_two_buttons_can_drive_the_same_door(void)
{
	FILE* f = beginEntities();
	putPressureButton(f, vect(0,0,0), 2);
	putPressureButton(f, vect(4096,0,0), 2);
	putDoor(f, vect(8192,0,0), 0);
	finishEntities(f, 3);

	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(2, levelFixtureCounts.activatorTargets);
}

static void test_a_target_index_past_the_pool_wires_nothing(void)
{
	/* The stored index is compared against every entity's own index rather
	 * than used to subscript anything, so a wild value simply matches no one.
	 * Worth pinning: it is the reason a bogus target is harmless where a
	 * bogus count was not. */
	FILE* f = beginEntities();
	putPressureButton(f, vect(0,0,0), 30000);
	putDoor(f, vect(4096,0,0), 0);
	finishEntities(f, 2);

	readEntities(f);
	fclose(f);

	TEST_ASSERT_EQUAL_INT(0, levelFixtureCounts.activatorTargets);
}

/* --- level title and author ---------------------------------------------- */

static void test_title_and_author_are_read_from_the_ini(void)
{
	writeTextFile(tempPath(), "[info]\ntitle = Test Chamber\nauthor = Cave\n");
	readMapInfo((char*)tempPath());

	TEST_ASSERT_EQUAL_STRING("Test Chamber", levelTitle);
	TEST_ASSERT_EQUAL_STRING("by Cave", levelAuthor);
}

static void test_a_missing_ini_leaves_the_banner_empty(void)
{
	readMapInfo((char*)"/tmp/portalds_test_levelfile_does_not_exist.ini");

	TEST_ASSERT_EQUAL_STRING("", levelTitle);
	TEST_ASSERT_EQUAL_STRING("", levelAuthor);
}

/*
 * The longest a value can actually be. iniparser_load rejects a whole file
 * the moment one line does not fit in ASCIILINESZ, so the ini parser is the
 * real bound on how much a map can hand to setLevelInfo - roughly a kilobyte,
 * against a 32 byte buffer. Staying comfortably under the limit keeps these
 * tests about the copy rather than about the parser's own cutoff, which
 * test_a_line_longer_than_the_parser_allows_is_rejected covers instead.
 */
#define LONGEST_USABLE_VALUE 900

static void writeInfoIni(const char* key, char fill, int length)
{
	char ini[LONGEST_USABLE_VALUE + 64];
	char value[LONGEST_USABLE_VALUE + 1];

	TEST_ASSERT_LESS_OR_EQUAL_INT(LONGEST_USABLE_VALUE, length);
	memset(value, fill, length);
	value[length] = '\0';
	snprintf(ini, sizeof(ini), "[info]\n%s = %s\n", key, value);
	writeTextFile(tempPath(), ini);
}

static void test_an_overlong_title_is_truncated(void)
{
	/* The regression: strcpy into a 32 byte buffer from a value the ini
	 * parser will happily return 900 bytes of. */
	writeInfoIni("title", 'A', LONGEST_USABLE_VALUE);
	readMapInfo((char*)tempPath());

	TEST_ASSERT_EQUAL_INT(LEVELINFOCHARS-1, (int)strlen(levelTitle));
	TEST_ASSERT_EACH_EQUAL_CHAR('A', levelTitle, LEVELINFOCHARS-1);
}

static void test_an_overlong_author_is_truncated(void)
{
	writeInfoIni("author", 'B', LONGEST_USABLE_VALUE);
	readMapInfo((char*)tempPath());

	TEST_ASSERT_EQUAL_INT(LEVELINFOCHARS-1, (int)strlen(levelAuthor));
	/* The "by " prefix has to survive the truncation, not be cut off by it. */
	TEST_ASSERT_EQUAL_STRING_LEN("by ", levelAuthor, 3);
}

static void test_a_line_longer_than_the_parser_allows_is_rejected(void)
{
	/* Pinning the parser's cutoff, because it is what bounds everything
	 * above: one over-long line and iniparser_load throws the whole file
	 * away, so the banner stays empty rather than holding a partial value. */
	char ini[2048];
	char title[1200];

	memset(title, 'D', sizeof(title)-1);
	title[sizeof(title)-1] = '\0';
	snprintf(ini, sizeof(ini), "[info]\ntitle = %s\n", title);
	writeTextFile(tempPath(), ini);

	readMapInfo((char*)tempPath());

	TEST_ASSERT_EQUAL_STRING("", levelTitle);
}

static void test_a_line_starting_with_a_nul_byte_is_skipped(void)
{
	/*
	 * The parser measured a line with strlen and then indexed line[len-1]
	 * without checking that len was non-zero. A line beginning with a NUL -
	 * which fgets happily reads, since it stops at a newline and not at a
	 * NUL - made that index -1: a read before the buffer, and if the byte
	 * there happened to be a backslash the parser set its continuation
	 * offset to -1 and the next fgets *wrote* before the buffer too.
	 *
	 * ASan is what fails this if the guard goes away; the assertions below
	 * only check that the rest of the file is still parsed around it.
	 */
	FILE* f = fopen(tempPath(), "wb");
	TEST_ASSERT_NOT_NULL(f);
	fputs("[info]\n", f);
	fputc('\0', f);                    /* a line that strlen sees as empty */
	fputs("\ntitle = Survived\n", f);
	fclose(f);

	readMapInfo((char*)tempPath());

	TEST_ASSERT_EQUAL_STRING("Survived", levelTitle);
}

static void test_a_nul_byte_followed_by_a_backslash_is_skipped(void)
{
	/* The variant that used to corrupt rather than merely read out of
	 * bounds: the byte before the buffer being a backslash set the
	 * continuation offset negative. */
	FILE* f = fopen(tempPath(), "wb");
	TEST_ASSERT_NOT_NULL(f);
	fputs("[info]\n", f);
	fputc('\0', f);
	fputs("\\\n", f);
	fputs("title = Survived\n", f);
	fclose(f);

	readMapInfo((char*)tempPath());

	TEST_ASSERT_EQUAL_STRING("Survived", levelTitle);
}

static void test_a_title_that_exactly_fills_the_buffer_is_kept(void)
{
	/* LEVELINFOCHARS-1 characters is the longest that needs no truncation.
	 * A cap that was one too tight would lose the last character here. */
	char ini[128];
	char title[LEVELINFOCHARS];

	memset(title, 'C', sizeof(title)-1);
	title[sizeof(title)-1] = '\0';
	snprintf(ini, sizeof(ini), "[info]\ntitle = %s\n", title);
	writeTextFile(tempPath(), ini);

	readMapInfo((char*)tempPath());

	TEST_ASSERT_EQUAL_STRING(title, levelTitle);
}

static void test_setting_level_info_twice_does_not_keep_the_old_text(void)
{
	setLevelInfo((char*)"A long previous chamber name", (char*)"Somebody");
	setLevelInfo((char*)"Short", (char*)"X");

	TEST_ASSERT_EQUAL_STRING("Short", levelTitle);
	TEST_ASSERT_EQUAL_STRING("by X", levelAuthor);
}

static void test_null_title_and_author_empty_the_banner(void)
{
	setLevelInfo((char*)"Something", (char*)"Someone");
	setLevelInfo(NULL, NULL);

	TEST_ASSERT_EQUAL_STRING("", levelTitle);
	TEST_ASSERT_EQUAL_STRING("", levelAuthor);
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_a_normal_entity_section_is_read_whole);
	RUN_TEST(test_a_full_entity_section_is_read_whole);
	RUN_TEST(test_an_empty_entity_section_creates_nothing);
	RUN_TEST(test_a_count_past_the_pool_is_capped);
	RUN_TEST(test_a_wildly_oversized_count_is_capped);
	RUN_TEST(test_the_largest_representable_count_is_capped);
	RUN_TEST(test_a_count_with_no_entities_behind_it_is_survivable);
	RUN_TEST(test_a_truncated_count_creates_nothing);
	RUN_TEST(test_capping_leaves_the_tables_beyond_the_cap_alone);
	RUN_TEST(test_a_null_stream_is_ignored);

	RUN_TEST(test_a_catcher_carries_its_position_and_direction);
	RUN_TEST(test_a_launcher_is_the_same_record_with_the_other_type);
	RUN_TEST(test_a_timed_button_converts_its_setting_to_an_angle);
	RUN_TEST(test_a_pressure_button_carries_its_position);
	RUN_TEST(test_a_turret_carries_its_facing);
	RUN_TEST(test_cubes_are_read_but_not_created);
	RUN_TEST(test_a_dispenser_is_created_holding_a_companion_cube);
	RUN_TEST(test_a_grid_reads_its_length_before_its_position);
	RUN_TEST(test_a_grid_takes_its_sign_and_side_from_the_direction);
	RUN_TEST(test_a_platform_carries_both_ends_of_its_run);
	RUN_TEST(test_a_door_orientation_is_reduced_to_an_axis);
	RUN_TEST(test_a_light_is_created_at_the_standard_intensity);
	RUN_TEST(test_a_platform_target_is_read_but_not_created);
	RUN_TEST(test_the_two_wall_doors_go_to_their_own_slots);
	RUN_TEST(test_the_arrival_door_places_the_player);
	RUN_TEST(test_a_mixed_sequence_reads_every_record_at_the_right_offset);
	RUN_TEST(test_an_unknown_entity_type_desynchronises_the_stream);

	RUN_TEST(test_a_button_is_wired_to_the_door_it_targets);
	RUN_TEST(test_an_entity_targeting_nothing_is_wired_to_nothing);
	RUN_TEST(test_two_buttons_can_drive_the_same_door);
	RUN_TEST(test_a_target_index_past_the_pool_wires_nothing);

	RUN_TEST(test_title_and_author_are_read_from_the_ini);
	RUN_TEST(test_a_missing_ini_leaves_the_banner_empty);
	RUN_TEST(test_an_overlong_title_is_truncated);
	RUN_TEST(test_an_overlong_author_is_truncated);
	RUN_TEST(test_a_line_longer_than_the_parser_allows_is_rejected);
	RUN_TEST(test_a_line_starting_with_a_nul_byte_is_skipped);
	RUN_TEST(test_a_nul_byte_followed_by_a_backslash_is_skipped);
	RUN_TEST(test_a_title_that_exactly_fills_the_buffer_is_kept);
	RUN_TEST(test_setting_level_info_twice_does_not_keep_the_old_text);
	RUN_TEST(test_null_title_and_author_empty_the_banner);

	return UNITY_END();
}
