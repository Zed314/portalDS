/*
 * Portal placement - the overlap rule in arm9/source/game/portals.c.
 *
 * portalToPortalIntersection() decides whether a candidate portal may be
 * placed, given where the other one already is. Its name reads like a
 * question about geometry but its answer is the caller's: true means "go
 * ahead". player.c uses it as
 *
 *     if(isPortalOnWall(...) && portalToPortalIntersection(por, other_por))
 *
 * so a false stops the shot from placing anything.
 *
 * @par Why this suite exists
 * The rule this replaces was switched off in e3fc39c, "provisional fix for
 * level 5 portals not appearing" - it refused placements the game needed. Its
 * thresholds were about a tenth of a portal too generous and its axis branches
 * misread two portals that both face along z. Turning a placement rule back on
 * without pinning down exactly which positions it refuses is how that happens
 * twice, so the boundary cases below are the point of the whole file: each
 * axis is checked at the last position that is refused and the first that is
 * allowed.
 *
 * @par Geometry
 * A portal on a wall facing +z has @ref PORTALSIZEX (341) of half width along
 * x and @ref PORTALSIZEY (682) of half height along y - 682 by 1364 overall.
 * Two of them clear each other once their centres are a full width or a full
 * height apart, so the thresholds are 682 on x and 1364 on y.
 */

#include "unity.h"
#include "level_fixture.h"

#define WALL_HALF_WIDTH  (PORTALSIZEX*2) /* 682: centres must differ by this on x */
#define WALL_HALF_HEIGHT (PORTALSIZEY*2) /* 1364: ...or by this on y */

static portal_struct a, b;

/*
 * Places a portal by hand rather than through movePortal(), which would drag
 * in the display list builder. computePortalPlane() derives the second tangent
 * exactly as the game does.
 */
static void place(portal_struct* p, vect3D position, vect3D normal, vect3D tangent)
{
	memset(p, 0, sizeof(*p));
	p->position = position;
	p->normal = normal;
	p->plane[0] = tangent;
	p->used = true;
	computePortalPlane(p);
}

/* The common case: both portals on the same wall, which faces +z. Its tangents
 * are x across and y up. */
static void placeOnBackWall(portal_struct* p, int32 x, int32 y)
{
	place(p, vect(x, y, 0),                 /* somewhere on the plane z = 0 */
	         vect(0, 0, inttof32(1)),       /* facing +z, into the room */
	         vect(inttof32(1), 0, 0));      /* first tangent across the wall */
}

void setUp(void)
{
	levelFixtureReset();
	placeOnBackWall(&a, 0, 0);
	placeOnBackWall(&b, 0, 0);
}

void tearDown(void) {}

/* --- the same spot ------------------------------------------------------- */

static void test_a_portal_cannot_be_placed_on_top_of_the_other(void)
{
	/* The thing this rule exists for: both shots at the same point. */
	TEST_ASSERT_FALSE_MESSAGE(portalToPortalIntersection(&a, &b),
		"two portals at the same position were allowed to overlap");
}

static void test_a_portal_barely_offset_still_overlaps(void)
{
	placeOnBackWall(&b, 64, 64);
	TEST_ASSERT_FALSE(portalToPortalIntersection(&a, &b));
}

static void test_the_rule_is_symmetric(void)
{
	placeOnBackWall(&b, 200, 300);
	TEST_ASSERT_EQUAL_MESSAGE(portalToPortalIntersection(&a, &b),
	                          portalToPortalIntersection(&b, &a),
	                          "which portal is asked about should not matter");
}

/* --- the boundary, across the wall --------------------------------------- */

static void test_side_by_side_portals_just_touching_are_allowed(void)
{
	/* Exactly a full width apart: edge to edge, no overlap. */
	placeOnBackWall(&b, WALL_HALF_WIDTH, 0);
	TEST_ASSERT_TRUE_MESSAGE(portalToPortalIntersection(&a, &b),
		"portals set edge to edge across the wall should be allowed");
}

static void test_side_by_side_portals_one_unit_closer_are_refused(void)
{
	placeOnBackWall(&b, WALL_HALF_WIDTH-1, 0);
	TEST_ASSERT_FALSE(portalToPortalIntersection(&a, &b));
}

static void test_side_by_side_works_in_both_directions(void)
{
	placeOnBackWall(&b, -WALL_HALF_WIDTH, 0);
	TEST_ASSERT_TRUE(portalToPortalIntersection(&a, &b));

	placeOnBackWall(&b, -(WALL_HALF_WIDTH-1), 0);
	TEST_ASSERT_FALSE(portalToPortalIntersection(&a, &b));
}

/* --- the boundary, up the wall ------------------------------------------- */

static void test_stacked_portals_just_touching_are_allowed(void)
{
	placeOnBackWall(&b, 0, WALL_HALF_HEIGHT);
	TEST_ASSERT_TRUE_MESSAGE(portalToPortalIntersection(&a, &b),
		"portals stacked one above the other should be allowed");
}

static void test_stacked_portals_one_unit_closer_are_refused(void)
{
	placeOnBackWall(&b, 0, WALL_HALF_HEIGHT-1);
	TEST_ASSERT_FALSE(portalToPortalIntersection(&a, &b));
}

static void test_clearing_either_axis_is_enough(void)
{
	/* Separating axes: clear on x while still close on y is fine. */
	placeOnBackWall(&b, WALL_HALF_WIDTH, 100);
	TEST_ASSERT_TRUE(portalToPortalIntersection(&a, &b));

	placeOnBackWall(&b, 100, WALL_HALF_HEIGHT);
	TEST_ASSERT_TRUE(portalToPortalIntersection(&a, &b));
}

/* --- different surfaces -------------------------------------------------- */

static void test_portals_on_perpendicular_walls_never_conflict(void)
{
	/* A floor and a wall meeting at a corner can put both portals at nearly
	 * the same point without either covering the other. */
	place(&b, vect(0, 0, 0), vect(0, inttof32(1), 0), vect(inttof32(1), 0, 0));

	TEST_ASSERT_TRUE_MESSAGE(portalToPortalIntersection(&a, &b),
		"portals on walls at right angles were treated as overlapping");
}

static void test_portals_on_facing_walls_do_not_conflict(void)
{
	/* Parallel planes, but one across the room from the other. */
	placeOnBackWall(&b, 0, 0);
	b.position = vect(0, 0, inttof32(8));

	TEST_ASSERT_TRUE_MESSAGE(portalToPortalIntersection(&a, &b),
		"portals on opposite walls of a room were treated as overlapping");
}

static void test_portals_back_to_back_in_one_plane_do_conflict(void)
{
	/* Anti-parallel normals with no gap: the same spot, approached from
	 * either side. The cross product is zero for these too, which is why the
	 * plane test uses it rather than comparing normals for equality. */
	placeOnBackWall(&b, 0, 0);
	b.normal = vect(0, 0, -inttof32(1));
	computePortalPlane(&b);

	TEST_ASSERT_FALSE(portalToPortalIntersection(&a, &b));
}

static void test_a_wall_just_behind_another_does_not_conflict(void)
{
	/* Parallel and close, but further apart than the coplanarity tolerance. */
	placeOnBackWall(&b, 0, 0);
	b.position = vect(0, 0, PORTALPLANEEPSILON+1);

	TEST_ASSERT_TRUE(portalToPortalIntersection(&a, &b));
}

/* --- a rotated portal ---------------------------------------------------- */

static void test_a_portal_turned_on_its_side_is_measured_correctly(void)
{
	/*
	 * Nothing stops the two portals having different tangents - the frame
	 * comes from where the player was looking. Turned ninety degrees, the
	 * other portal is 1364 wide and 682 tall, so the thresholds swap: both
	 * axes clear at 341+682.
	 */
	place(&b, vect(0, 0, 0), vect(0, 0, inttof32(1)), vect(0, inttof32(1), 0));

	const int32 mixed = PORTALSIZEX + PORTALSIZEY; /* 1023 */

	b.position = vect(mixed, 0, 0);
	TEST_ASSERT_TRUE_MESSAGE(portalToPortalIntersection(&a, &b), "a turned portal was measured with the wrong extent on x");

	b.position = vect(mixed-1, 0, 0);
	TEST_ASSERT_FALSE(portalToPortalIntersection(&a, &b));

	b.position = vect(0, mixed, 0);
	TEST_ASSERT_TRUE_MESSAGE(portalToPortalIntersection(&a, &b), "a turned portal was measured with the wrong extent on y");

	b.position = vect(0, mixed-1, 0);
	TEST_ASSERT_FALSE(portalToPortalIntersection(&a, &b));
}

/* --- the first shot ------------------------------------------------------ */

static void test_the_first_portal_of_a_pair_is_always_allowed(void)
{
	/* Until the other one has been shot there is nothing to overlap, and the
	 * stale position it still holds must not be treated as an obstacle. */
	b.used = false;
	TEST_ASSERT_TRUE_MESSAGE(portalToPortalIntersection(&a, &b),
		"an unplaced portal blocked the first shot");
}

static void test_null_portals_are_tolerated(void)
{
	TEST_ASSERT_TRUE(portalToPortalIntersection(NULL, &b));
	TEST_ASSERT_TRUE(portalToPortalIntersection(&a, NULL));
	TEST_ASSERT_TRUE(portalToPortalIntersection(NULL, NULL));
}

/* --- standing in a portal ------------------------------------------------ */

/*
 * player_struct::inPortal and ::oldInPortal are an edge detector: updatePlayer
 * plays the enter sound on a false-to-true and the exit sound on a
 * true-to-false. So the pair has to be stable while the player stands still,
 * or the sound retriggers every frame.
 *
 * That is exactly what happened. isPointInPortal() tests only the two in-plane
 * axes, so it answers true for the whole column running through a portal, not
 * just its mouth - and with two portals facing each other, standing in one puts
 * the player inside the other's column too. Both per-portal checks then wrote
 * the same pair of flags, and the second read the first one's answer as though
 * it were the previous frame's.
 */

static player_struct* thePlayer(void) { return getPlayer(); }

/* Portals facing each other down the z axis, which is what a test chamber is
 * usually built out of. */
static void faceEachOther(int32 separation)
{
	place(&portal1, vect(0, 0, 0),          vect(0, 0,  inttof32(1)), vect(inttof32(1), 0, 0));
	place(&portal2, vect(0, 0, separation), vect(0, 0, -inttof32(1)), vect(inttof32(1), 0, 0));
	portal1.targetPortal = &portal2;
	portal2.targetPortal = &portal1;
}

/* Runs the portal update for a while and counts how often the enter/exit edge
 * test in updatePlayer would have fired. */
static int soundTriggersOver(int frames)
{
	player_struct* pl = thePlayer();
	int triggers = 0;

	for(int i=0;i<frames;i++)
	{
		updatePortals();
		if(pl->inPortal != pl->oldInPortal)triggers++;
	}
	return triggers;
}

static void test_standing_still_in_a_portal_triggers_the_sound_once(void)
{
	faceEachOther(inttof32(4));

	/* Just inside portal1's mouth, and therefore also inside portal2's column.
	 * Slightly in front of the plane so the warp test does not fire. */
	thePlayer()->object->position = vect(0, 0, 50);

	const int triggers = soundTriggersOver(60);

	TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(1, triggers,
		"the enter/exit sound retriggered while the player stood still");
	TEST_ASSERT_TRUE_MESSAGE(thePlayer()->inPortal, "standing in a portal should read as being in one");
}

static void test_standing_still_in_the_far_portal_is_also_stable(void)
{
	/* The other way round, because the two checks run in a fixed order and
	 * only one of them is last. */
	faceEachOther(inttof32(4));
	thePlayer()->object->position = vect(0, 0, inttof32(4)-50);

	TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(1, soundTriggersOver(60),
		"the enter/exit sound retriggered while the player stood still");
	TEST_ASSERT_TRUE(thePlayer()->inPortal);
}

static void test_standing_in_the_column_but_not_the_portal_is_stable(void)
{
	/* Lined up with both portals but well clear of either mouth: in neither,
	 * and it should stay that way silently. */
	faceEachOther(inttof32(8));
	thePlayer()->object->position = vect(0, 0, inttof32(4));

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, soundTriggersOver(60), "a sound fired while standing in open space");
	TEST_ASSERT_FALSE(thePlayer()->inPortal);
}

static void test_stepping_into_a_portal_triggers_the_sound_once(void)
{
	/* The edge still has to work: the fix must not silence it. */
	faceEachOther(inttof32(8));

	thePlayer()->object->position = vect(0, 0, inttof32(4));
	soundTriggersOver(4);
	TEST_ASSERT_FALSE(thePlayer()->inPortal);

	thePlayer()->object->position = vect(0, 0, 50);
	TEST_ASSERT_EQUAL_INT_MESSAGE(1, soundTriggersOver(1), "stepping into a portal should trigger the sound");
	TEST_ASSERT_TRUE(thePlayer()->inPortal);

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, soundTriggersOver(30), "and then stop");
}

static void test_stepping_out_of_a_portal_triggers_the_sound_once(void)
{
	faceEachOther(inttof32(8));

	thePlayer()->object->position = vect(0, 0, 50);
	soundTriggersOver(4);
	TEST_ASSERT_TRUE(thePlayer()->inPortal);

	thePlayer()->object->position = vect(0, 0, inttof32(4));
	TEST_ASSERT_EQUAL_INT_MESSAGE(1, soundTriggersOver(1), "stepping out of a portal should trigger the sound");
	TEST_ASSERT_FALSE(thePlayer()->inPortal);

	TEST_ASSERT_EQUAL_INT_MESSAGE(0, soundTriggersOver(30), "and then stop");
}

/* --- the colour-to-portal pairing ---------------------------------------- */

static void test_the_orange_shot_fills_portal1_and_the_blue_portal2(void)
{
	/*
	 * portalForColor is the one place the pairing lives: shootPlayerGun
	 * places its shot through it, and the firing sound, the gun tint and the
	 * touch button all assume the same convention - true is orange is
	 * portal1. Pinned by pointer identity, because half of those places
	 * reach for &portal1 by name.
	 */
	TEST_ASSERT_EQUAL_PTR(&portal1, portalForColor(true));
	TEST_ASSERT_EQUAL_PTR(&portal2, portalForColor(false));
}

static void test_init_gives_each_colour_its_own_portal(void)
{
	/*
	 * The pairing above is only right because initPortals paints portal1
	 * orange and portal2 blue, and aims each at the other. A swap here would
	 * leave every portalForColor caller consistent with each other and all
	 * of them wrong, which no other test would notice.
	 */
	initPortals();

	TEST_ASSERT_EQUAL_HEX16(RGB15(31,31,0), portalForColor(true)->color);
	TEST_ASSERT_EQUAL_HEX16(RGB15(0,31,31), portalForColor(false)->color);
	TEST_ASSERT_EQUAL_PTR(&portal2, portal1.targetPortal);
	TEST_ASSERT_EQUAL_PTR(&portal1, portal2.targetPortal);
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_a_portal_cannot_be_placed_on_top_of_the_other);
	RUN_TEST(test_a_portal_barely_offset_still_overlaps);
	RUN_TEST(test_the_rule_is_symmetric);

	RUN_TEST(test_side_by_side_portals_just_touching_are_allowed);
	RUN_TEST(test_side_by_side_portals_one_unit_closer_are_refused);
	RUN_TEST(test_side_by_side_works_in_both_directions);
	RUN_TEST(test_stacked_portals_just_touching_are_allowed);
	RUN_TEST(test_stacked_portals_one_unit_closer_are_refused);
	RUN_TEST(test_clearing_either_axis_is_enough);

	RUN_TEST(test_portals_on_perpendicular_walls_never_conflict);
	RUN_TEST(test_portals_on_facing_walls_do_not_conflict);
	RUN_TEST(test_portals_back_to_back_in_one_plane_do_conflict);
	RUN_TEST(test_a_wall_just_behind_another_does_not_conflict);

	RUN_TEST(test_a_portal_turned_on_its_side_is_measured_correctly);

	RUN_TEST(test_the_first_portal_of_a_pair_is_always_allowed);
	RUN_TEST(test_null_portals_are_tolerated);

	RUN_TEST(test_the_orange_shot_fills_portal1_and_the_blue_portal2);
	RUN_TEST(test_init_gives_each_colour_its_own_portal);

	RUN_TEST(test_standing_still_in_a_portal_triggers_the_sound_once);
	RUN_TEST(test_standing_still_in_the_far_portal_is_also_stable);
	RUN_TEST(test_standing_in_the_column_but_not_the_portal_is_stable);
	RUN_TEST(test_stepping_into_a_portal_triggers_the_sound_once);
	RUN_TEST(test_stepping_out_of_a_portal_triggers_the_sound_once);

	return UNITY_END();
}
