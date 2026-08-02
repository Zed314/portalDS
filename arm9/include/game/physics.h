/**
 * @file physics.h
 * @brief The player's own collision - a sphere pushed out of the world.
 *
 * Nothing here goes near the ARM7. Rigid bodies (cubes) are simulated over
 * there; the player, the camera and a few simple entities use this instead: a
 * sphere that is moved, then pushed back out of anything it ended up inside.
 *
 * The reason for the split is feel. A player modelled as a rigid body tumbles,
 * bounces and gets wedged; a swept sphere with explicit push-out gives the
 * crisp, predictable movement a first-person game needs, and costs a fraction
 * as much. The trade-off is that it resolves penetration after the fact rather
 * than at the time of impact, which is why @ref MAXSTEP limits how far an
 * object may move per iteration.
 *
 * @par Gravity is a variable
 * @ref gravityVector is not a constant. @ref changeGravity re-points it, which
 * is what lets the elevator sections and the gravity-flipping rooms work -
 * "down" is whatever this vector says, and the collision code branches on
 * which component of @ref normGravityVector is non-zero.
 */

#ifndef __PHYSICS9__
#define __PHYSICS9__

#define MAXSTEP 2               /**< Maximum distance an object may move per collision iteration, before it is subdivided. */
#define GRAVITY (inttof32(1)>>7)/**< Gravitational acceleration per frame, in f32. */
#define MARGIN (1)              /**< Slack left between an object and a surface after push-out, to avoid re-colliding every frame. */

/**
 * @brief A sphere that collides with the world.
 *
 * Used for the player, the camera and a handful of simple entities.
 */
typedef struct
{
	vect3D position; /**< Centre in world space. */
	vect3D speed;    /**< Velocity, applied and then corrected each update. */
	int32 radius;    /**< Sphere radius. */
	int32 sqRadius;  /**< Radius squared, cached to keep the inner loop free of multiplies. */
	bool contact;    /**< True if the object touched anything last update - this is the "is on the ground" test. */
}physicsObject_struct;

extern vect3D gravityVector;     /**< Current gravity, as an acceleration per frame. */
extern vect3D normGravityVector; /**< The same direction, normalised. Collision code branches on which component is non-zero. */

/** @brief Returns the room containing a world-space point, or NULL. */
room_struct* getRoomPoint(vect3D p);

/** @brief Applies gravity and velocity to an object, then resolves collision against its room. */
void updatePhysicsObject(physicsObject_struct* o);

/**
 * @brief Pushes an object out of anything it is intersecting.
 *
 * Walks the grid cells the object overlaps and corrects its position against
 * each colliding rectangle, plus any elevators.
 *
 * @param o object to correct.
 * @param r room to collide against.
 */
void collideObjectRoom(physicsObject_struct* o, room_struct* r);

/**
 * @brief Tests whether an object's centre lies within a room's footprint.
 * @param r room to test.
 * @param o object to test.
 * @param v out: the object's position relative to the room origin. May be NULL.
 */
bool objectInRoom(room_struct* r, physicsObject_struct* o, vect3D* v);

/**
 * @brief Cheap movement update with no sub-stepping.
 * @return the id of the rectangle the object landed on, or -1.
 */
s16 updateSimplePhysicsObjectRoom(room_struct* r, physicsObject_struct* o);

/**
 * @brief Full movement update against a room.
 * @param r    room to move within.
 * @param o    object to move.
 * @param both true to resolve collision both before and after integrating,
 *             which stops fast objects tunnelling through thin geometry.
 */
void updatePhysicsObjectRoom(room_struct* r, physicsObject_struct* o, bool both);

/** @brief Converts a world-space position into that room's tile coordinates. */
vect3D convertCoord(room_struct* r, vect3D p);

/**
 * @brief Re-points gravity.
 * @param v new direction; need not be normalised.
 * @param l strength.
 */
void changeGravity(vect3D v, int32 l);

#endif
