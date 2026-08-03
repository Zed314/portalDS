/*
 * Host stand-ins for the game state the level reader builds into, used by
 * test_levelfile and test_room.
 *
 * game/room.c is the reader for the level file format. It is worth testing on
 * the host for one reason above all others: it is the only part of the
 * codebase that parses data the player did not produce. Levels are downloaded
 * from the project's webpage, so every count, offset and length in a .map is
 * hostile until proven otherwise.
 *
 * The reader itself is pure parsing - fread, bounds, a switch - but it ends
 * each case by calling into the entity pool that owns that kind of thing
 * (createTurret, createDoor, ...). Those pools are the stateful, hardware-tied
 * half of the game and cannot come to the host, so this file supplies them.
 *
 * The stubs are not silent: each records what it was handed, in order, so a
 * test can ask what a file actually produced rather than only whether the run
 * survived. That is what makes the entity format testable - see
 * levelFixtureCreated below. They also return non-NULL, which keeps the reader
 * on its more demanding path, the one where it stores the returned activator
 * back into its wiring tables.
 *
 * addRoomRectangle is the exception: it is a real list append rather than a
 * stub, because the room geometry helpers walk the list it builds. It pushes
 * at the front, as the editor's own implementation does - the reader relies on
 * that ordering being reversed when it pairs rectangles with lighting.
 */

#ifndef PORTALDS_TEST_LEVEL_FIXTURE_H
#define PORTALDS_TEST_LEVEL_FIXTURE_H

#include "game/game_main.h"
#include "editor/entity.h"  /* NUMENTITIES */
#include "editor/io.h"      /* mapHeader_struct */

/** Which pool the reader reached into. */
typedef enum
{
	CREATED_ENERGY_DEVICE = 1,
	CREATED_TIMED_BUTTON,
	CREATED_BIG_BUTTON,
	CREATED_CUBE_DISPENSER,
	CREATED_PLATFORM,
	CREATED_DOOR,
	CREATED_TURRET,
	CREATED_LIGHT,
	CREATED_GRID,
	CREATED_WALLDOOR
} createdKind_type;

/**
 * One create* call, as the reader made it.
 *
 * The fields are deliberately generic: every entity has a position, several
 * have one more number, a few have a flag. Naming them after any one entity
 * would make the other cases read strangely, so the per-type meaning is
 * documented here and asserted in the tests.
 */
typedef struct
{
	createdKind_type kind;
	vect3D pos;   /**< Primary position. Platforms: the origin. */
	vect3D pos2;  /**< Platforms only: the destination. */
	int32  param; /**< Turret facing, timed button angle, grid length, door and wall door orientation. */
	bool   flag;  /**< Energy device type (catcher/launcher), grid direction, platform back-and-forth. */
} createdEntity_struct;

#define MAX_CREATED 512

extern createdEntity_struct levelFixtureCreated[MAX_CREATED];
extern int levelFixtureCreatedCount;

/** Coarse counters, for tests that only care how much came out. */
typedef struct
{
	int entities;         /**< create* calls of any kind. */
	int lights;           /**< createLight calls. */
	int turrets;          /**< createTurret calls. */
	int rectangles;       /**< addRoomRectangle plus addSludgeRectangle calls. */
	int sludgeRectangles; /**< addSludgeRectangle calls alone. */
	int activatorTargets; /**< addActivatorTarget calls - the trigger wiring. */
	int portalCollisions; /**< collidePortal calls - physics.c consulting the portals. */
	int timedButtonChecks;/**< checkObjectTimedButtonsCollision calls. */
	int elevatorsClosed;  /**< closeElevator calls. */
} levelFixtureCounts_struct;

extern levelFixtureCounts_struct levelFixtureCounts;

/**
 * Clears the counters, the creation log and every piece of reader-visible
 * global state - the entity wiring tables in room.c included. Every test calls
 * this from setUp(), for the same reason the physics suites call
 * physicsReset().
 */
void levelFixtureReset(void);

/*
 * The grid cell game/physics.c collides against.
 *
 * On a real DS getCurrentCell() picks a cell out of the room's spatial grid,
 * which generateRoomGrid() builds. Here the test builds the cell directly and
 * getCurrentCell() hands back whatever it was given, which is what makes the
 * collision resolver testable one surface at a time: nothing else decides
 * which rectangles it is asked to consider.
 *
 * Rectangles are in tile coordinates, as they are in a level file - physics.c
 * multiplies up by TILESIZE and HEIGHTUNIT itself.
 */
void levelFixtureCellClear(void);

/**
 * Puts a collidable rectangle in the cell and returns it, so a test can read
 * its touched flag back afterwards.
 */
rectangle_struct* levelFixtureCellAdd(vect3D position, vect3D size, vect3D normal);

/** Makes getCurrentCell() return NULL, as it does for a point outside the room. */
void levelFixtureCellDetach(void);

/** Appends a rectangle to a room's list, exactly as addRoomRectangle does. */
rectangle_struct* levelFixturePushRectangle(room_struct* r, vect3D position, vect3D size, vect3D normal);

/** Releases a list built by levelFixturePushRectangle or addRoomRectangle. */
void levelFixtureFreeRectangles(room_struct* r);

/*
 * room.c's entity wiring tables. Not declared in any game header - they are
 * private to room.c in spirit - but they have external linkage and they are
 * exactly what the entity count bounds protect, so the tests read them.
 */
extern s16 entityTargetArray[NUMENTITIES];
extern activator_struct* entityActivatorArray[NUMENTITIES];
extern void* entityEntityArray[NUMENTITIES];
extern activatorTarget_type entityTargetTypeArray[NUMENTITIES];

/* Under test, from arm9/source/game/room.c. */
void readEntities(FILE* f);
void readMapInfo(char* filename);
void readSludgeRectangles(FILE* f);
void readRectangles(room_struct* r, FILE* f);
vect3D orientVector(vect3D v, u8 k);
void invertRectangle(rectangle_struct* rec);
void roomOriginSize(room_struct* r, vect3D* o, vect3D* s);
void roomResetOrigin(room_struct* r);

/* Under test, from arm9/source/game/physics.c. */
bool checkObjectCollision(physicsObject_struct* o, room_struct* r);
bool checkObjectCollisionCell(gridCell_struct* gc, physicsObject_struct* o, room_struct* r);
bool collideRectangle(physicsObject_struct* o, room_struct* r, vect3D p, vect3D s);
u8 checkObjectElevatorCollision(physicsObject_struct* o, room_struct* r, elevator_struct* ev);
void collideObjectRoom(physicsObject_struct* o, room_struct* r);
void changeGravity(vect3D v, int32 l);
bool pointInRoom(room_struct* r, vect3D p, vect3D* v);
vect3D convertCoord(room_struct* r, vect3D p);

#endif
