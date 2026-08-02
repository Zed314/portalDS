/**
 * @file turrets.h
 * @brief Sentry turrets.
 *
 * The only hostile entity. A turret is unusual in that it is *both* a game
 * entity and an ARM7 rigid body: turret_struct::OBB is a real simulated box, so
 * a turret can be knocked over by a thrown cube, shoved with the gravity gun,
 * or dropped through a portal - and once it is on its side it stops shooting
 * and marks itself turret_struct::dead.
 *
 * @par The laser sight
 * The visible red beam is a ray cast forward each frame. What makes it
 * interesting is turret_struct::laserThroughPortal: if the beam hits an open
 * portal, it is continued out of the far one, which is why the second pair of
 * @c laserOrigin2 / @c laserDestination2 endpoints exists. The same is true of
 * the turret's fire, so a turret really can shoot you through a portal.
 */

#ifndef TURRETS_H
#define TURRETS_H

#define NUMTURRETS (8)              /**< Maximum number of turrets in a room. */
#define TURRETMASS (inttof32(1))    /**< Mass of a turret's rigid body. */

/** @brief Where a turret is in its open/close animation. */
typedef enum
{
	TURRET_CLOSED,  /**< Idle, wings folded, not shooting. */
	TURRET_OPENING, /**< Has spotted the player and is deploying. */
	TURRET_OPEN,    /**< Deployed and firing. */
	TURRET_CLOSING  /**< Has lost the player and is folding away. */
}turretState_type;

/** @brief A sentry turret. */
typedef struct
{
	OBB_struct* OBB;            /**< The turret's rigid body; it really can be knocked over. */
	vect3D laserOrigin;         /**< Start of the laser sight. */
	vect3D laserDestination;    /**< Where the laser sight stops. */
	vect3D laserOrigin2;        /**< Start of the beam's continuation past a portal. */
	vect3D laserDestination2;   /**< End of that continuation. */
	turretState_type state;     /**< Current animation state. */
	bool laserThroughPortal;    /**< True when the laser passes through a portal, making the second segment live. */
	u16 shotAngle[2];           /**< Muzzle flash angles for the two barrels. */
	u8 drawShot[2];             /**< Frames left to draw each muzzle flash. */
	u8 counter;                 /**< General timer: firing cadence and state transitions. */
	bool dead;                  /**< Set when the turret has been knocked off its feet. */
	bool used;                  /**< False when this slot is free. */
}turret_struct;

/** @brief Clears the turret pool and loads the turret model. */
void initTurrets(void);

/** @brief Releases the turret model and every live turret's rigid body. */
void freeTurrets(void);

/**
 * @brief Places a turret.
 * @param r        room to place it in.
 * @param position position in tile coordinates.
 * @param d        facing direction.
 * @return the new turret, or NULL if the pool is full.
 */
turret_struct* createTurret(room_struct* r, vect3D position, u8 d);

/** @brief Advances every turret: line of sight, laser, firing and the tipped-over check. */
void updateTurrets(void);

/** @brief Draws the turrets' lasers and muzzle flashes. The bodies are drawn as rigid bodies. */
void drawTurretsStuff(void);

#endif
