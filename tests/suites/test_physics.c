/*
 * Player collision and movement - arm9/source/game/physics.c.
 *
 * The player is not an ARM7 rigid body. It gets this much simpler
 * move-then-push-out scheme instead, which is what gives a first person game
 * its crisp movement, and it runs every frame for every surface near the
 * player. It is also the only collision code a player can feel directly: the
 * rigid body solver being slightly wrong makes a cube settle oddly, this being
 * slightly wrong makes you fall through a floor.
 *
 * Two things make it testable at all. The rectangles it collides against come
 * from a grid cell, and the fixture hands it one the test built - so a test
 * can put exactly one surface in front of it. And getClosestPointRectangle,
 * which does the real geometry, is linked in for real from editor/rectangle.c
 * rather than stubbed, so what is under test here is the resolver rather than
 * a model of it.
 *
 * @par Room space
 * physics.c works relative to the room's origin, which for a room at (0,0) is
 * still a half tile off the world origin - convertVect() places a tile's
 * centre. roomSpace() below does that conversion so the tests can talk about
 * where a surface is rather than about the offset.
 *
 * @par Numbers that are pinned rather than derived
 * A fixed point resolver has no closed form to check against, so a few
 * constants here are measured from the code rather than predicted: the rest
 * height above a floor is 572, the rest distance from a wall is the collision
 * radius, and terminal velocity is 800. Each is asserted where it is load
 * bearing and explained where it is surprising.
 */

#include "unity.h"
#include "level_fixture.h"

static room_struct room;

/*
 * physics.c owns gravityVector and normGravityVector as globals and
 * changeGravity() mutates them, so they have to go back to "down is -y"
 * between tests or a gravity test would leak into everything after it.
 */
static void resetGravity(void)
{
	changeGravity(vect(0, -inttof32(1), 0), 16);
}

void setUp(void)
{
	levelFixtureReset();
	memset(&room, 0, sizeof(room));
	resetGravity();
}

void tearDown(void)
{
	levelFixtureFreeRectangles(&room);
}

/* Room space to world space, for a room at the origin. */
static vect3D roomSpace(int32 x, int32 y, int32 z)
{
	return vect(x-TILESIZE, y, z-TILESIZE);
}

static physicsObject_struct playerAt(vect3D position)
{
	physicsObject_struct o;
	memset(&o, 0, sizeof(o));
	o.radius = PLAYERRADIUS;
	o.sqRadius = SQPLAYERRADIUS;
	o.position = position;
	return o;
}

/* A floor four tiles square with its near corner on the room origin. */
static rectangle_struct* addFloor(void)
{
	return levelFixtureCellAdd(vect(0,0,0), vect(4,0,4), vect(0,inttof32(1),0));
}

/* A wall in the x=2 tile plane, facing +x. */
static rectangle_struct* addWall(void)
{
	return levelFixtureCellAdd(vect(2,0,0), vect(0,4,4), vect(inttof32(1),0,0));
}

/* The rest height a sphere settles at above a floor, measured from the code.
 * It is where the weighted distance test in checkObjectCollisionCell stops
 * reporting an overlap, not the collision radius - the vertical term is
 * divided by transY, so the sphere rests further off a floor than off a wall. */
#define FLOOR_REST_HEIGHT 572

/* --- resolving against a surface ----------------------------------------- */

static void test_a_sphere_clear_of_the_floor_is_untouched(void)
{
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 4000, 1536));
	const vect3D before = o.position;

	TEST_ASSERT_FALSE(checkObjectCollision(&o, &room));
	TEST_ASSERT_EQUAL_INT32(before.y, o.position.y);
}

static void test_a_sphere_sunk_into_the_floor_is_pushed_up(void)
{
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));
	TEST_ASSERT_GREATER_THAN_INT32_MESSAGE(100, o.position.y, "the sphere was not pushed up out of the floor");
}

static void test_the_push_out_does_not_move_it_sideways(void)
{
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));
	const vect3D before = o.position;

	checkObjectCollision(&o, &room);

	TEST_ASSERT_EQUAL_INT32(before.x, o.position.x);
	TEST_ASSERT_EQUAL_INT32(before.z, o.position.z);
}

static void test_resolving_converges_in_one_step(void)
{
	/*
	 * The invariant the whole scheme rests on: one correction is enough, and
	 * resolving again does not move the sphere any further. If it did, a
	 * player standing still would be nudged every frame - the classic jitter
	 * of a push-out collider.
	 *
	 * Note that the *flag* stays set once at rest, and has to: it is what
	 * o->contact is built from, which is the "am I on the ground" test. So
	 * what settles here is the position, not the return value.
	 */
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));
	const int32 settled = o.position.y;

	for(int i=0;i<8;i++)
	{
		TEST_ASSERT_TRUE_MESSAGE(checkObjectCollision(&o, &room), "resting on a surface must keep reporting contact");
		TEST_ASSERT_EQUAL_INT32_MESSAGE(settled, o.position.y, "the sphere kept moving after it had been resolved");
	}
}

static void test_the_rest_height_is_the_same_from_any_depth(void)
{
	/* However deep it started, it comes to rest in the same place - so
	 * walking onto a floor from any direction puts the camera at one height
	 * rather than at wherever the geometry happened to catch it. */
	const int32 depths[] = { 100, 250, 400, 550 };

	for(unsigned i=0;i<sizeof(depths)/sizeof(depths[0]);i++)
	{
		levelFixtureReset();
		addFloor();
		physicsObject_struct o = playerAt(roomSpace(1536, depths[i], 1536));

		TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));
		TEST_ASSERT_EQUAL_INT32_MESSAGE(FLOOR_REST_HEIGHT, o.position.y, "settled at a different height");
	}
}

static void test_a_hair_above_the_plane_settles_slightly_short(void)
{
	/*
	 * Pinning fixed point noise. From any real penetration the sphere lands
	 * on exactly FLOOR_REST_HEIGHT, but from a few units above the surface
	 * the correction is computed from a very short vector and the division
	 * loses enough that it stops a little low - by up to about twenty units
	 * out of five hundred.
	 *
	 * Harmless in play, because gravity puts the sphere back against the
	 * floor the following frame and it converges from there. Pinned so that
	 * anyone who sees a camera settle a hair low knows this is where it comes
	 * from, and so that a change which makes it worse shows up here.
	 */
	const int32 shallow[] = { 5, 10, 20, 40, 60 };

	for(unsigned i=0;i<sizeof(shallow)/sizeof(shallow[0]);i++)
	{
		levelFixtureReset();
		addFloor();
		physicsObject_struct o = playerAt(roomSpace(1536, shallow[i], 1536));

		TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));
		TEST_ASSERT_INT32_WITHIN_MESSAGE(24, FLOOR_REST_HEIGHT, o.position.y, "settled far from the rest height");
		TEST_ASSERT_LESS_OR_EQUAL_INT32_MESSAGE(FLOOR_REST_HEIGHT, o.position.y, "settled above the rest height");
	}
}

static void test_the_surface_that_was_hit_is_marked(void)
{
	/* touched is what the trigger logic reads to know the player is standing
	 * on something. */
	rectangle_struct* floor = addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	checkObjectCollision(&o, &room);

	TEST_ASSERT_TRUE(floor->touched);
}

static void test_a_surface_that_was_missed_is_not_marked(void)
{
	rectangle_struct* floor = addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 4000, 1536));

	checkObjectCollision(&o, &room);

	TEST_ASSERT_FALSE(floor->touched);
}

static void test_a_non_colliding_surface_is_ignored(void)
{
	/* Faces that exist only to be drawn have collides clear and no ARM7
	 * rectangle behind them. */
	rectangle_struct* floor = addFloor();
	floor->collides = false;

	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));
	const vect3D before = o.position;

	TEST_ASSERT_FALSE(checkObjectCollision(&o, &room));
	TEST_ASSERT_EQUAL_INT32(before.y, o.position.y);
}

static void test_a_sphere_beside_the_floor_does_not_touch_it(void)
{
	/* Level with the floor but well outside its extent. */
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(6000, 100, 1536));

	TEST_ASSERT_FALSE(checkObjectCollision(&o, &room));
}

static void test_a_wall_pushes_horizontally(void)
{
	addWall();
	/* Just inside the wall plane, which is at room space x = 2 tiles. */
	physicsObject_struct o = playerAt(roomSpace(2*TILESIZE*2 - 100, 100, 1536));
	const vect3D before = o.position;

	TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));
	TEST_ASSERT_LESS_THAN_INT32_MESSAGE(before.x, o.position.x, "the sphere was not pushed back off the wall");
	TEST_ASSERT_EQUAL_INT32_MESSAGE(before.y, o.position.y, "a wall should not move the sphere vertically");
}

static void test_a_sphere_rests_a_radius_away_from_a_wall(void)
{
	/*
	 * Horizontally there is no transY weighting, so the resting distance is
	 * the collision radius exactly - unlike the floor, which rests further
	 * out. Worth pinning because the asymmetry is deliberate and not obvious.
	 */
	addWall();
	physicsObject_struct o = playerAt(roomSpace(2*TILESIZE*2 - 100, 100, 1536));

	checkObjectCollision(&o, &room);

	const int32 gap = (2*TILESIZE*2) - (o.position.x + TILESIZE);
	TEST_ASSERT_INT32_WITHIN_MESSAGE(2, PLAYERRADIUS, gap, "did not come to rest a radius off the wall");
}

static void test_a_sphere_exactly_in_the_plane_is_pushed_clear(void)
{
	/*
	 * The centre landing exactly on the closest point of a surface used to
	 * leave the correction with no direction to push along and a zero length
	 * to scale by - a divide by zero, which on the DS meant the hardware
	 * divider handing back whatever it held and the player being displaced by
	 * an arbitrary amount.
	 *
	 * With no motion to back out along, the escape is against gravity: the
	 * sphere comes off the floor by its radius. That is short of the resting
	 * height, so it takes a couple of passes to settle - which is fine, it is
	 * a degenerate case, and settling slowly beats being teleported.
	 */
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 0, 1536));

	TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));
	TEST_ASSERT_EQUAL_INT32_MESSAGE(PLAYERRADIUS, o.position.y, "should have come off the surface by one radius");

	for(int i=0;i<8;i++)checkObjectCollision(&o, &room);
	TEST_ASSERT_EQUAL_INT32_MESSAGE(FLOOR_REST_HEIGHT, o.position.y, "should have settled at the resting height");
}

static void test_a_sphere_in_the_plane_backs_out_the_way_it_came(void)
{
	/*
	 * When the object was moving, the escape direction comes from its motion
	 * rather than from gravity - so walking into a wall and landing exactly on
	 * its plane pushes back the way it came, not upwards.
	 */
	addWall();
	physicsObject_struct o = playerAt(roomSpace(2*TILESIZE*2, 100, 1536));
	o.speed = vect(200, 0, 0);

	TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));

	TEST_ASSERT_EQUAL_INT32_MESSAGE(roomSpace(2*TILESIZE*2, 100, 1536).x - PLAYERRADIUS, o.position.x,
		"should have backed out along the reversed motion");
	TEST_ASSERT_EQUAL_INT32_MESSAGE(100, o.position.y, "gravity should not have been used as the escape here");
}

static void test_a_point_outside_the_grid_collides_with_nothing(void)
{
	/* getCurrentCell returns NULL for a position outside the room, and the
	 * resolver has to cope rather than dereference it. */
	addFloor();
	levelFixtureCellDetach();

	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));
	TEST_ASSERT_FALSE(checkObjectCollision(&o, &room));
}

static void test_a_floor_and_a_wall_are_both_resolved(void)
{
	/* In a corner, both surfaces have to push - resolving only the first
	 * would leave the player inside the other. */
	addFloor();
	addWall();

	physicsObject_struct o = playerAt(roomSpace(2*TILESIZE*2 - 100, 100, 1536));

	TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));
	const vect3D settled = o.position;

	checkObjectCollision(&o, &room);

	TEST_ASSERT_EQUAL_INT32_MESSAGE(settled.x, o.position.x, "the wall was not fully resolved");
	TEST_ASSERT_EQUAL_INT32_MESSAGE(settled.y, o.position.y, "the floor was not fully resolved");
}

static void test_collision_tolerates_null(void)
{
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));
	TEST_ASSERT_FALSE(checkObjectCollisionCell(NULL, &o, &room));
	TEST_ASSERT_FALSE(checkObjectCollisionCell(NULL, NULL, NULL));
	TEST_ASSERT_FALSE(collideRectangle(NULL, &room, vect(0,0,0), vect(0,0,0)));
	TEST_ASSERT_FALSE(collideRectangle(&o, NULL, vect(0,0,0), vect(0,0,0)));
}

static void test_the_search_box_reaches_five_radii_along_gravity(void)
{
	/*
	 * Before the distance test, each surface is culled against a box around
	 * the sphere: one radius to each side, but five along gravity, because
	 * the weighted test reaches sqrt(transY) times further that way and the
	 * box must not cut it short. The portals are the observer here - they
	 * are consulted for every surface that passes the cull - which is what
	 * tells "culled" apart from "considered and missed": at five radii
	 * exactly the floor is still considered, one unit further and it is gone.
	 */
	portal1.used = true;
	portal2.used = true;
	addFloor();

	physicsObject_struct o = playerAt(roomSpace(1536, 5*PLAYERRADIUS, 1536));
	TEST_ASSERT_FALSE_MESSAGE(checkObjectCollision(&o, &room), "five radii up is outside even the weighted reach");
	TEST_ASSERT_EQUAL_INT_MESSAGE(2, levelFixtureCounts.portalCollisions,
		"a floor five radii below should still pass the cull");

	levelFixtureReset();
	portal1.used = true;
	portal2.used = true;
	addFloor();

	physicsObject_struct past = playerAt(roomSpace(1536, 5*PLAYERRADIUS+1, 1536));
	TEST_ASSERT_FALSE(checkObjectCollision(&past, &room));
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, levelFixtureCounts.portalCollisions,
		"past five radii the floor should be culled before the portals are consulted");
}

static void test_the_cull_only_tests_the_axis_the_surface_is_flat_on(void)
{
	/*
	 * The cull compares a single coordinate: the axis the rectangle has no
	 * extent along. A sphere level with the floor but far beyond its edge
	 * passes the cull and is rejected by the distance test instead - the
	 * closest-point clamp covers the other two axes, so culling them here
	 * would be redundant work rather than extra safety. Pinned so a
	 * rewritten cull that quietly becomes a full box test shows up.
	 */
	portal1.used = true;
	portal2.used = true;
	addFloor();

	physicsObject_struct o = playerAt(roomSpace(6000, 100, 1536));
	TEST_ASSERT_FALSE(checkObjectCollision(&o, &room));
	TEST_ASSERT_EQUAL_INT_MESSAGE(2, levelFixtureCounts.portalCollisions,
		"level with the floor, the cull should hand it to the distance test");
}

/* --- portals ------------------------------------------------------------- */

static void test_the_portals_are_not_consulted_when_unplaced(void)
{
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	checkObjectCollision(&o, &room);

	TEST_ASSERT_EQUAL_INT(0, levelFixtureCounts.portalCollisions);
}

static void test_both_portals_are_consulted_for_each_surface(void)
{
	/*
	 * With both portals down, every surface the player is resolved against
	 * has to be checked against both of them - that is what stops a surface
	 * with a portal in it from pushing the player back out of the hole.
	 */
	portal1.used = true;
	portal2.used = true;

	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	checkObjectCollision(&o, &room);

	TEST_ASSERT_EQUAL_INT_MESSAGE(2, levelFixtureCounts.portalCollisions,
		"each surface should be tested against both portals");
}

static void test_one_portal_alone_is_not_enough(void)
{
	/* A single portal is not a hole yet, so the check is skipped. */
	portal1.used = true;

	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	checkObjectCollision(&o, &room);

	TEST_ASSERT_EQUAL_INT(0, levelFixtureCounts.portalCollisions);
}

/* --- platforms ----------------------------------------------------------- */

static void test_a_platform_under_the_player_is_collided_with(void)
{
	platform[0].used = true;
	platform[0].position = roomSpace(1536, 0, 1536);

	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	TEST_ASSERT_TRUE(checkObjectCollision(&o, &room));
	TEST_ASSERT_TRUE_MESSAGE(platform[0].touched, "the platform was not marked as stood on");
	TEST_ASSERT_GREATER_THAN_INT32(100, o.position.y);
}

static void test_an_unused_platform_slot_is_skipped(void)
{
	platform[0].used = false;
	platform[0].position = roomSpace(1536, 0, 1536);

	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	TEST_ASSERT_FALSE(checkObjectCollision(&o, &room));
	TEST_ASSERT_FALSE(platform[0].touched);
}

static void test_a_platform_elsewhere_is_not_stood_on(void)
{
	platform[0].used = true;
	platform[0].position = roomSpace(20000, 0, 20000);

	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));

	TEST_ASSERT_FALSE(checkObjectCollision(&o, &room));
	TEST_ASSERT_FALSE(platform[0].touched);
}

static void test_a_sphere_rests_on_a_platform_at_the_floor_height(void)
{
	/* The platform path goes through collideRectangle rather than through the
	 * grid, but the arithmetic is shared, so the rest height must be too - or
	 * stepping from a floor onto a platform would bump the camera. */
	platform[0].used = true;
	platform[0].position = roomSpace(1536, 0, 1536);

	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));
	for(int i=0;i<8;i++)checkObjectCollision(&o, &room);

	TEST_ASSERT_EQUAL_INT32_MESSAGE(FLOOR_REST_HEIGHT, o.position.y,
		"a platform should carry the player at the same height as a floor");
}

static void test_the_platform_contact_threshold_matches_the_floors(void)
{
	/* The exact boundary of the previous test: at the floor rest height the
	 * surface still reports contact - which is what o->contact and every
	 * standing-on trigger are built from - and one unit higher it is clear.
	 * Pinned on collideRectangle directly so the two resolvers cannot drift
	 * apart. */
	const vect3D p = vect(-inttof32(1), 0, -inttof32(1));
	const vect3D s = vect(inttof32(2), 0, inttof32(2));

	physicsObject_struct touching = playerAt(vect(0, FLOOR_REST_HEIGHT, 0));
	TEST_ASSERT_TRUE_MESSAGE(collideRectangle(&touching, &room, p, s),
		"the rest height should still be in contact");

	physicsObject_struct clear = playerAt(vect(0, FLOOR_REST_HEIGHT+1, 0));
	TEST_ASSERT_FALSE_MESSAGE(collideRectangle(&clear, &room, p, s),
		"one unit above the rest height should be clear");
}

/* --- the elevator cylinder ----------------------------------------------- */

/*
 * The inverse of every other case in the file: the player is kept *inside* a
 * cylinder rather than outside a surface. Two radii are involved - an inner
 * one the player is held within while in the shaft, and an outer one they are
 * pushed clear of from the other side.
 */
#define ELEVATOR_RADIUS_IN  (TILESIZE*2-64)
#define ELEVATOR_RADIUS_OUT (TILESIZE*2+128)
#define ELEVATOR_SHAFT_HEIGHT (TILESIZE*16)

static elevator_struct shaftAt(vect3D position)
{
	elevator_struct ev;
	memset(&ev, 0, sizeof(ev));
	ev.position = position;
	/* Put the floor far below so these tests see only the cylinder. */
	ev.realPosition = vect(position.x, position.y-100000, position.z);
	ev.state = ELEVATOR_CLOSING;
	return ev;
}

static void test_standing_in_the_middle_of_the_shaft_is_untouched(void)
{
	elevator_struct ev = shaftAt(vect(0,0,0));
	physicsObject_struct o = playerAt(vect(100, 0, 0));
	const vect3D before = o.position;

	TEST_ASSERT_EQUAL_UINT8(0, checkObjectElevatorCollision(&o, &room, &ev));
	TEST_ASSERT_EQUAL_INT32(before.x, o.position.x);
}

static void test_the_shaft_wall_holds_the_player_in(void)
{
	elevator_struct ev = shaftAt(vect(0,0,0));
	physicsObject_struct o = playerAt(vect(ELEVATOR_RADIUS_IN-50, 0, 0));

	TEST_ASSERT_EQUAL_UINT8(1, checkObjectElevatorCollision(&o, &room, &ev));
	TEST_ASSERT_LESS_THAN_INT32_MESSAGE(ELEVATOR_RADIUS_IN-50, o.position.x,
		"the player was not pulled back inside the shaft");
	TEST_ASSERT_LESS_THAN_INT32(ELEVATOR_RADIUS_IN, o.position.x);
}

static void test_the_outside_of_the_shaft_pushes_the_player_away(void)
{
	elevator_struct ev = shaftAt(vect(0,0,0));
	physicsObject_struct o = playerAt(vect(ELEVATOR_RADIUS_OUT, 0, 0));

	TEST_ASSERT_EQUAL_UINT8(1, checkObjectElevatorCollision(&o, &room, &ev));
	TEST_ASSERT_GREATER_THAN_INT32_MESSAGE(ELEVATOR_RADIUS_OUT, o.position.x,
		"the player was not pushed clear of the shaft");
}

static void test_standing_well_clear_of_the_shaft_is_untouched(void)
{
	elevator_struct ev = shaftAt(vect(0,0,0));
	physicsObject_struct o = playerAt(vect(ELEVATOR_RADIUS_OUT+PLAYERRADIUS+400, 0, 0));
	const vect3D before = o.position;

	TEST_ASSERT_EQUAL_UINT8(0, checkObjectElevatorCollision(&o, &room, &ev));
	TEST_ASSERT_EQUAL_INT32(before.x, o.position.x);
}

static void test_the_shaft_only_reaches_so_far_up(void)
{
	/* Above the shaft there is no cylinder to be held in or out of. */
	elevator_struct ev = shaftAt(vect(0,0,0));
	physicsObject_struct o = playerAt(vect(ELEVATOR_RADIUS_OUT, ELEVATOR_SHAFT_HEIGHT+500, 0));
	const vect3D before = o.position;

	checkObjectElevatorCollision(&o, &room, &ev);
	TEST_ASSERT_EQUAL_INT32(before.x, o.position.x);
}

static void test_an_open_door_lets_the_player_through_the_wall(void)
{
	/*
	 * While the doors are open there is a cone the player can walk out
	 * through, or they could never leave. Facing 0 opens towards +x: a player
	 * on that side passes, one on the far side is still held in.
	 */
	elevator_struct ev = shaftAt(vect(0,0,0));
	ev.state = ELEVATOR_OPEN;
	ev.direction = 0;

	physicsObject_struct through = playerAt(vect(ELEVATOR_RADIUS_IN-50, 0, 0));
	const vect3D beforeThrough = through.position;
	const u8 throughResult = checkObjectElevatorCollision(&through, &room, &ev);

	physicsObject_struct held = playerAt(vect(-(ELEVATOR_RADIUS_IN-50), 0, 0));
	const u8 heldResult = checkObjectElevatorCollision(&held, &room, &ev);

	TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, throughResult, "the doorway side should let the player pass");
	TEST_ASSERT_EQUAL_INT32_MESSAGE(beforeThrough.x, through.position.x,
		"the doorway side should not move the player");
	TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, heldResult, "the far side should still hold the player in");
}

static void test_a_closed_door_holds_the_player_on_every_side(void)
{
	/* With the doors shut there is no cone, so both sides are walls. */
	elevator_struct ev = shaftAt(vect(0,0,0));
	ev.state = ELEVATOR_CLOSING;
	ev.direction = 0;

	physicsObject_struct plus = playerAt(vect(ELEVATOR_RADIUS_IN-50, 0, 0));
	physicsObject_struct minus = playerAt(vect(-(ELEVATOR_RADIUS_IN-50), 0, 0));

	TEST_ASSERT_EQUAL_UINT8(1, checkObjectElevatorCollision(&plus, &room, &ev));
	TEST_ASSERT_EQUAL_UINT8(1, checkObjectElevatorCollision(&minus, &room, &ev));
}

static void test_elevator_collision_tolerates_null(void)
{
	elevator_struct ev = shaftAt(vect(0,0,0));
	physicsObject_struct o = playerAt(vect(0,0,0));

	TEST_ASSERT_EQUAL_UINT8(0, checkObjectElevatorCollision(NULL, &room, &ev));
	TEST_ASSERT_EQUAL_UINT8(0, checkObjectElevatorCollision(&o, NULL, &ev));
	TEST_ASSERT_EQUAL_UINT8(0, checkObjectElevatorCollision(&o, &room, NULL));
}

static void test_stepping_onto_the_exit_lift_closes_it(void)
{
	/* Landing on the exit elevator's floor is what ends a chamber. */
	exitWallDoor.used = true;
	exitWallDoor.elevator = shaftAt(vect(0,0,0));
	exitWallDoor.elevator.realPosition = roomSpace(1536, 0, 1536);
	exitWallDoor.elevator.position = roomSpace(1536, 0, 1536);

	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));
	checkObjectCollision(&o, &room);

	TEST_ASSERT_EQUAL_INT_MESSAGE(1, levelFixtureCounts.elevatorsClosed,
		"standing on the exit lift did not close it");
}

/* --- gravity and movement ------------------------------------------------ */

static void test_gravity_accumulates_into_the_speed(void)
{
	physicsObject_struct o = playerAt(roomSpace(1536, 40000, 1536));

	collideObjectRoom(&o, &room);
	TEST_ASSERT_EQUAL_INT32(-16, o.speed.y);

	collideObjectRoom(&o, &room);
	TEST_ASSERT_EQUAL_INT32(-32, o.speed.y);
}

static void test_falling_moves_the_object_down(void)
{
	physicsObject_struct o = playerAt(roomSpace(1536, 40000, 1536));
	const int32 before = o.position.y;

	collideObjectRoom(&o, &room);

	TEST_ASSERT_LESS_THAN_INT32(before, o.position.y);
}

static void test_terminal_velocity_is_capped(void)
{
	/* Without this a long fall would move the player further in one frame
	 * than the collision sweep can cover, and they would leave the level. */
	physicsObject_struct o = playerAt(roomSpace(1536, 4000000, 1536));

	for(int i=0;i<200;i++)collideObjectRoom(&o, &room);

	TEST_ASSERT_EQUAL_INT32_MESSAGE(-800, o.speed.y, "terminal velocity is not being clamped");
}

static void test_falling_freely_reports_no_contact(void)
{
	physicsObject_struct o = playerAt(roomSpace(1536, 40000, 1536));

	collideObjectRoom(&o, &room);

	TEST_ASSERT_FALSE_MESSAGE(o.contact, "nothing was touched, so contact must be clear");
}

static void test_landing_on_a_floor_reports_contact(void)
{
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, FLOOR_REST_HEIGHT, 1536));

	collideObjectRoom(&o, &room);

	TEST_ASSERT_TRUE_MESSAGE(o.contact, "standing on a floor must report contact");
}

static void test_a_body_at_rest_on_a_floor_stays_there(void)
{
	/*
	 * The property a player standing still depends on: gravity pulls every
	 * frame, the floor pushes back every frame, and after a hundred of those
	 * the camera has not drifted.
	 */
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, FLOOR_REST_HEIGHT, 1536));

	for(int i=0;i<100;i++)collideObjectRoom(&o, &room);

	TEST_ASSERT_INT32_WITHIN_MESSAGE(8, FLOOR_REST_HEIGHT, o.position.y, "the resting height drifted");
	TEST_ASSERT_TRUE(o.contact);
}

static void test_a_fall_lands_on_the_floor_rather_than_through_it(void)
{
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, 20000, 1536));

	for(int i=0;i<200;i++)collideObjectRoom(&o, &room);

	TEST_ASSERT_TRUE_MESSAGE(o.contact, "the object never landed");
	TEST_ASSERT_INT32_WITHIN_MESSAGE(8, FLOOR_REST_HEIGHT, o.position.y, "the object did not come to rest on the floor");
}

static void test_ground_friction_halves_horizontal_speed(void)
{
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(1536, FLOOR_REST_HEIGHT, 1536));
	o.speed = vect(400, 0, 0);

	collideObjectRoom(&o, &room);

	TEST_ASSERT_TRUE(o.contact);
	TEST_ASSERT_EQUAL_INT32_MESSAGE(200, o.speed.x, "ground friction should take half the speed");
}

static void test_air_friction_is_much_gentler(void)
{
	/* A thirty-second rather than a half, so a jump carries. */
	physicsObject_struct o = playerAt(roomSpace(1536, 40000, 1536));
	o.speed = vect(400, 0, 0);

	collideObjectRoom(&o, &room);

	TEST_ASSERT_FALSE(o.contact);
	TEST_ASSERT_EQUAL_INT32_MESSAGE(388, o.speed.x, "air friction should take a thirty-second of the speed");
}

static void test_speed_reversed_by_a_collision_is_zeroed(void)
{
	/*
	 * After resolving, the speed is recomputed as the distance actually
	 * travelled - and any component that ended up pointing the other way is
	 * dropped rather than kept as a bounce. Walking into a wall stops you; it
	 * does not push you back.
	 */
	addWall();
	/* Already closer to the wall than it will be allowed to rest, and still
	 * creeping into it - so the frame ends with the object behind where it
	 * started, against the direction it was moving. Slowly, so that the step
	 * does not carry the centre across the plane; see the tunnelling test
	 * below for what happens when it does. */
	physicsObject_struct o = playerAt(roomSpace(2*TILESIZE*2 - 100, 100, 1536));
	o.speed = vect(20, 0, 0);

	collideObjectRoom(&o, &room);

	TEST_ASSERT_EQUAL_INT32_MESSAGE(0, o.speed.x, "a component that reversed should be dropped, not kept");
}

static void test_a_crawling_horizontal_speed_snaps_to_zero(void)
{
	/* Otherwise the player creeps forever after letting go of the stick. */
	physicsObject_struct o = playerAt(roomSpace(1536, 40000, 1536));
	o.speed = vect(2, 0, 2);

	collideObjectRoom(&o, &room);

	TEST_ASSERT_EQUAL_INT32(0, o.speed.x);
	TEST_ASSERT_EQUAL_INT32(0, o.speed.z);
}

static void test_a_slow_walk_into_a_wall_is_blocked(void)
{
	/* The case that works: each step is short enough that the sphere's centre
	 * never reaches the wall's plane, so it is pushed back every frame. */
	addWall();
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(2*TILESIZE*2 - 100, FLOOR_REST_HEIGHT, 1536));
	o.speed = vect(50, 0, 0);

	for(int i=0;i<10;i++)collideObjectRoom(&o, &room);

	const int32 wallPlane = 2*TILESIZE*2;
	TEST_ASSERT_LESS_THAN_INT32_MESSAGE(wallPlane, o.position.x + TILESIZE,
		"a slow walk should not get through a wall");
}

static void test_every_step_of_a_sweep_is_checked(void)
{
	/*
	 * Above a speed of 200 the move is broken into 128 unit steps with a
	 * collision check after each. The check used to be folded in with
	 * ret=ret||checkObjectCollision(...), and || short circuits - so once
	 * anything had been touched, which on the first step is the floor
	 * underfoot, every remaining step of that sweep skipped collision
	 * entirely and the object slid the rest of the way through whatever was
	 * in front of it.
	 *
	 * Starting from where the resolver actually leaves a player standing
	 * against a wall, and pushing into it at every speed up to terminal
	 * velocity.
	 */
	const int32 speeds[] = { 250, 400, 550, 700, 800 };
	const int32 wallPlane = 2*TILESIZE*2;

	for(unsigned i=0;i<sizeof(speeds)/sizeof(speeds[0]);i++)
	{
		levelFixtureReset();
		addWall();
		addFloor();

		physicsObject_struct o = playerAt(roomSpace(wallPlane - PLAYERRADIUS, FLOOR_REST_HEIGHT, 1536));
		o.speed = vect(speeds[i], 0, 0);

		for(int frame=0;frame<40;frame++)collideObjectRoom(&o, &room);

		TEST_ASSERT_LESS_THAN_INT32_MESSAGE(wallPlane, o.position.x + TILESIZE,
			"a swept move went through the wall");
	}
}

static void test_a_walk_starting_inside_a_wall_is_pushed_back_out(void)
{
	/*
	 * A player who begins the frame already inside a wall's resting zone -
	 * which a portal exit or a spawn point can leave them in - and keeps
	 * pushing into it. This used to end up on the far side: the step landed
	 * the centre exactly on the plane, the correction divided by zero and did
	 * nothing, and the frame after that the centre was through and the
	 * correction had the wrong sign.
	 */
	addWall();
	addFloor();
	physicsObject_struct o = playerAt(roomSpace(2*TILESIZE*2 - 100, FLOOR_REST_HEIGHT, 1536));
	o.speed = vect(100, 0, 0);

	for(int i=0;i<20;i++)collideObjectRoom(&o, &room);

	const int32 wallPlane = 2*TILESIZE*2;
	TEST_ASSERT_LESS_THAN_INT32_MESSAGE(wallPlane, o.position.x + TILESIZE,
		"the object ended up on the far side of the wall");
}

static void test_movement_tolerates_null(void)
{
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 1536));
	collideObjectRoom(NULL, &room);
	collideObjectRoom(&o, NULL);
}

/* --- gravity direction --------------------------------------------------- */

static void test_changing_gravity_sets_both_vectors(void)
{
	changeGravity(vect(inttof32(1), 0, 0), 32);

	TEST_ASSERT_EQUAL_INT32(inttof32(1), normGravityVector.x);
	TEST_ASSERT_EQUAL_INT32(0, normGravityVector.y);
	TEST_ASSERT_EQUAL_INT32_MESSAGE(32, gravityVector.x, "the acceleration should be the direction scaled by the strength");

	resetGravity();
}

static void test_a_floor_still_works_with_gravity_along_x(void)
{
	/*
	 * "Down" is a variable here, which is why checkObjectCollisionCell
	 * branches on which component of normGravityVector is set. With gravity
	 * along x, a wall in the x plane becomes the floor - and the widened
	 * search box has to widen along x instead of y, or the surface is culled
	 * before it is ever tested.
	 */
	changeGravity(vect(-inttof32(1), 0, 0), 16);
	addWall();

	/*
	 * 500 units off the wall is the distance that makes this a real test.
	 * It is further than the collision radius, so the unwidened search box
	 * would cull the surface outright, but well inside the widened one - and
	 * the weighted distance test does still find an overlap there, because
	 * along the gravity axis the reach is transY times longer. Place the
	 * sphere any closer and the test passes whether the widening follows
	 * gravity or not.
	 */
	physicsObject_struct o = playerAt(roomSpace(2*TILESIZE*2 - 500, 100, 1536));
	const vect3D before = o.position;

	TEST_ASSERT_TRUE_MESSAGE(checkObjectCollision(&o, &room), "the surface was culled once gravity turned");
	TEST_ASSERT_LESS_THAN_INT32_MESSAGE(before.x, o.position.x, "the surface was found but not resolved against");

	resetGravity();
}

static void test_a_floor_still_works_with_gravity_along_z(void)
{
	/* The z twin of the test above, for the third branch of the axis pick -
	 * the one an x- and a y-gravity test both leave unexercised. */
	changeGravity(vect(0, 0, -inttof32(1)), 16);

	/* A wall in the z=2 tile plane, facing +z - the floor, once gravity
	 * points along -z. */
	levelFixtureCellAdd(vect(0,0,2), vect(4,4,0), vect(0,0,inttof32(1)));

	/* 500 off the plane, for the reason the x test explains: outside the
	 * unwidened box, inside the widened one, within the weighted reach. */
	physicsObject_struct o = playerAt(roomSpace(1536, 100, 2*TILESIZE*2 - 500));
	const vect3D before = o.position;

	TEST_ASSERT_TRUE_MESSAGE(checkObjectCollision(&o, &room), "the surface was culled once gravity turned");
	TEST_ASSERT_LESS_THAN_INT32_MESSAGE(before.z, o.position.z, "the surface was found but not resolved against");

	resetGravity();
}

/* --- room helpers -------------------------------------------------------- */

static void test_a_point_inside_the_room_is_recognised(void)
{
	room.width = 4;
	room.height = 4;

	TEST_ASSERT_TRUE(pointInRoom(&room, roomSpace(1536, 0, 1536), NULL));
}

static void test_a_point_outside_the_room_is_rejected(void)
{
	room.width = 4;
	room.height = 4;

	TEST_ASSERT_FALSE(pointInRoom(&room, roomSpace(-100, 0, 1536), NULL));
	TEST_ASSERT_FALSE(pointInRoom(&room, roomSpace(1536, 0, 99999), NULL));
	TEST_ASSERT_FALSE(pointInRoom(NULL, roomSpace(0,0,0), NULL));
}

static void test_the_room_relative_position_is_reported(void)
{
	room.width = 4;
	room.height = 4;

	vect3D relative;
	pointInRoom(&room, roomSpace(1536, 250, 1536), &relative);

	TEST_ASSERT_EQUAL_INT32(1536, relative.x);
	TEST_ASSERT_EQUAL_INT32(1536, relative.z);
}

static void test_world_positions_convert_to_tile_coordinates(void)
{
	/* One tile is TILESIZE*2 across and HEIGHTUNIT tall. */
	vect3D c = convertCoord(&room, roomSpace(0, 0, 0));
	TEST_ASSERT_EQUAL_INT32(0, c.x);
	TEST_ASSERT_EQUAL_INT32(0, c.z);

	c = convertCoord(&room, roomSpace(TILESIZE*2, HEIGHTUNIT, TILESIZE*2));
	TEST_ASSERT_EQUAL_INT32(1, c.x);
	TEST_ASSERT_EQUAL_INT32(1, c.y);
	TEST_ASSERT_EQUAL_INT32(1, c.z);
}

static void test_negative_positions_round_down_not_towards_zero(void)
{
	/*
	 * Truncating division would put everything between -1 and 0 in tile 0,
	 * giving tile 0 twice the width of every other and putting the player in
	 * the wrong grid cell just outside the room origin. The explicit -1 in
	 * convertCoord is what avoids that.
	 */
	vect3D c = convertCoord(&room, roomSpace(-1, 0, -1));
	TEST_ASSERT_EQUAL_INT32_MESSAGE(-1, c.x, "just left of the origin belongs to tile -1");
	TEST_ASSERT_EQUAL_INT32_MESSAGE(-1, c.z, "just behind the origin belongs to tile -1");

	c = convertCoord(&room, roomSpace(-TILESIZE*2, 0, -TILESIZE*2));
	TEST_ASSERT_EQUAL_INT32(-2, c.x);
	TEST_ASSERT_EQUAL_INT32(-2, c.z);

	TEST_ASSERT_EQUAL_INT32(0, convertCoord(NULL, roomSpace(0,0,0)).x);
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_a_sphere_clear_of_the_floor_is_untouched);
	RUN_TEST(test_a_sphere_sunk_into_the_floor_is_pushed_up);
	RUN_TEST(test_the_push_out_does_not_move_it_sideways);
	RUN_TEST(test_resolving_converges_in_one_step);
	RUN_TEST(test_the_rest_height_is_the_same_from_any_depth);
	RUN_TEST(test_a_hair_above_the_plane_settles_slightly_short);
	RUN_TEST(test_the_surface_that_was_hit_is_marked);
	RUN_TEST(test_a_surface_that_was_missed_is_not_marked);
	RUN_TEST(test_a_non_colliding_surface_is_ignored);
	RUN_TEST(test_a_sphere_beside_the_floor_does_not_touch_it);
	RUN_TEST(test_a_wall_pushes_horizontally);
	RUN_TEST(test_a_sphere_rests_a_radius_away_from_a_wall);
	RUN_TEST(test_a_sphere_exactly_in_the_plane_is_pushed_clear);
	RUN_TEST(test_a_sphere_in_the_plane_backs_out_the_way_it_came);
	RUN_TEST(test_a_point_outside_the_grid_collides_with_nothing);
	RUN_TEST(test_a_floor_and_a_wall_are_both_resolved);
	RUN_TEST(test_collision_tolerates_null);
	RUN_TEST(test_the_search_box_reaches_five_radii_along_gravity);
	RUN_TEST(test_the_cull_only_tests_the_axis_the_surface_is_flat_on);

	RUN_TEST(test_the_portals_are_not_consulted_when_unplaced);
	RUN_TEST(test_both_portals_are_consulted_for_each_surface);
	RUN_TEST(test_one_portal_alone_is_not_enough);

	RUN_TEST(test_a_platform_under_the_player_is_collided_with);
	RUN_TEST(test_an_unused_platform_slot_is_skipped);
	RUN_TEST(test_a_platform_elsewhere_is_not_stood_on);
	RUN_TEST(test_a_sphere_rests_on_a_platform_at_the_floor_height);
	RUN_TEST(test_the_platform_contact_threshold_matches_the_floors);

	RUN_TEST(test_standing_in_the_middle_of_the_shaft_is_untouched);
	RUN_TEST(test_the_shaft_wall_holds_the_player_in);
	RUN_TEST(test_the_outside_of_the_shaft_pushes_the_player_away);
	RUN_TEST(test_standing_well_clear_of_the_shaft_is_untouched);
	RUN_TEST(test_the_shaft_only_reaches_so_far_up);
	RUN_TEST(test_an_open_door_lets_the_player_through_the_wall);
	RUN_TEST(test_a_closed_door_holds_the_player_on_every_side);
	RUN_TEST(test_elevator_collision_tolerates_null);
	RUN_TEST(test_stepping_onto_the_exit_lift_closes_it);

	RUN_TEST(test_gravity_accumulates_into_the_speed);
	RUN_TEST(test_falling_moves_the_object_down);
	RUN_TEST(test_terminal_velocity_is_capped);
	RUN_TEST(test_falling_freely_reports_no_contact);
	RUN_TEST(test_landing_on_a_floor_reports_contact);
	RUN_TEST(test_a_body_at_rest_on_a_floor_stays_there);
	RUN_TEST(test_a_fall_lands_on_the_floor_rather_than_through_it);
	RUN_TEST(test_ground_friction_halves_horizontal_speed);
	RUN_TEST(test_air_friction_is_much_gentler);
	RUN_TEST(test_speed_reversed_by_a_collision_is_zeroed);
	RUN_TEST(test_a_crawling_horizontal_speed_snaps_to_zero);
	RUN_TEST(test_a_slow_walk_into_a_wall_is_blocked);
	RUN_TEST(test_every_step_of_a_sweep_is_checked);
	RUN_TEST(test_a_walk_starting_inside_a_wall_is_pushed_back_out);
	RUN_TEST(test_movement_tolerates_null);

	RUN_TEST(test_changing_gravity_sets_both_vectors);
	RUN_TEST(test_a_floor_still_works_with_gravity_along_x);
	RUN_TEST(test_a_floor_still_works_with_gravity_along_z);

	RUN_TEST(test_a_point_inside_the_room_is_recognised);
	RUN_TEST(test_a_point_outside_the_room_is_rejected);
	RUN_TEST(test_the_room_relative_position_is_reported);
	RUN_TEST(test_world_positions_convert_to_tile_coordinates);
	RUN_TEST(test_negative_positions_round_down_not_towards_zero);

	return UNITY_END();
}
