/**
 * @file energyball.h
 * @brief High energy pellets, and the launchers and catchers they travel between.
 *
 * Two entity types that only make sense together:
 *
 *  - an @ref energyDevice_struct is a wall fixture, either a **launcher** or a
 *    **catcher** (energyDevice_struct::type). A launcher periodically emits a
 *    pellet along its facing direction; a catcher fires its
 *    @ref activator_struct when one arrives;
 *  - an @ref energyBall_struct is a pellet in flight.
 *
 * The puzzle is getting a pellet from a launcher into a catcher that it cannot
 * reach in a straight line, using portals. Pellets are not rigid bodies - they
 * travel in a straight line at constant speed and bounce off walls, and their
 * portal transport is handled directly in energyball.c.
 *
 * A pellet has a limited life (energyBall_struct::life) so a badly aimed shot
 * eventually expires and the launcher can produce another, rather than the
 * chamber filling up with strays.
 */

#ifndef ENERGYBALL_H
#define ENERGYBALL_H



/**
 * @brief Which axis a wall fixture faces along.
 *
 * The values are ordered so that flipping the low bit gives the opposite
 * direction, which the reflection code relies on.
 */
typedef enum
{
	pX=0, /**< Facing +X. */
	mX=1, /**< Facing -X. */
	pY=2, /**< Facing +Y. */
	mY=3, /**< Facing -Y. */
	pZ=4, /**< Facing +Z. */
	mZ=5  /**< Facing -Z. */
}deviceOrientation_type;

/** @brief A pellet launcher or catcher. */
typedef struct
{
	modelInstance_struct modelInstance;  /**< Model and its idle/active animation. */
	deviceOrientation_type orientation;  /**< Which way the device faces. */
	rectangle_struct* surface;           /**< The wall face it is mounted on. */
	vect3D position;                     /**< Position in tile coordinates. */
	activator_struct activator;          /**< What a catcher drives when a pellet arrives. Unused on a launcher. */
	bool type;                           /**< True for a launcher, false for a catcher. */ //true=launcher
	bool active;                         /**< For a catcher: whether it currently holds a pellet. */
	bool used;                           /**< False when this slot is free. */
	u8 id;                               /**< Slot index. */
}energyDevice_struct;

/** @brief A pellet in flight. */
typedef struct
{
	modelInstance_struct modelInstance; /**< The pellet's model. */
	energyDevice_struct* launcher;      /**< Where it came from, so that launcher can be told when it dies. */
	vect3D position;                    /**< Current position in world space. */
	vect3D direction;                   /**< Direction of travel; normalised. */
	int32 speed;                        /**< Travel speed in world units per frame. */
	u16 maxLife;                        /**< Lifetime it started with, used to fade it out near the end. */
	u16 life;                           /**< Frames remaining before it expires. */
	bool used;                          /**< False when this slot is free. */
	u8 id;                              /**< Slot index. */
}energyBall_struct;

/** @brief Clears both pools and loads the pellet and device models. */
void initEnergyBalls(void);

/** @brief Releases the models. */
void freeEnergyBalls(void);

/**
 * @brief Places a launcher or catcher.
 * @param r    room to place it in.
 * @param pos  position in tile coordinates.
 * @param or   which way it faces.
 * @param type true for a launcher, false for a catcher.
 * @return the new device, or NULL if the pool is full.
 */
energyDevice_struct* createEnergyDevice(room_struct* r, vect3D pos, deviceOrientation_type or, bool type);

/** @brief Draws every launcher and catcher. */
void drawEnergyDevices(void);

/** @brief Advances every device: launcher cadence, and catcher activation. */
void updateEnergyDevices(void);

/**
 * @brief Emits a pellet.
 * @param launcher device it came from; may be NULL.
 * @param pos      starting position.
 * @param dir      direction of travel.
 * @param life     lifetime in frames.
 * @return the new pellet, or NULL if the pool is full.
 */
energyBall_struct* createEnergyBall(energyDevice_struct* launcher, vect3D pos, vect3D dir, u16 life);

/** @brief Draws every pellet in flight. */
void drawEnergyBalls(void);

/** @brief Advances every pellet: movement, wall bounces, portal transport, and expiry. */
void updateEnergyBalls(void);

#endif
