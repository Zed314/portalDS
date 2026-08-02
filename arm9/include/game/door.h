/**
 * @file door.h
 * @brief Sliding doors.
 *
 * The standard triggered door: a model that slides open, plus two collision
 * rectangles (one per leaf) that are toggled off while it is open. That
 * toggling goes all the way through to the ARM7 via @ref toggleAAR - otherwise
 * a cube would still bounce off an open doorway.
 *
 * Doors are targets, not triggers: something else - a button, usually - drives
 * door_struct::active through an @ref activator_struct. See @ref activator.h.
 *
 * @see walldoor.h for the larger doors that lead in and out of a chamber.
 */

#ifndef DOOR_H
#define DOOR_H

#define NUMDOORS (16) /**< Maximum number of doors in a room. */

/** @brief A sliding door. */
typedef struct
{
	vect3D position;                    /**< Position in tile coordinates. */
	modelInstance_struct modelInstance; /**< Model and its open/close animation. */
	rectangle_struct* rectangle[2];     /**< The two leaves' collision faces, disabled while open. */
	bool orientation;                   /**< Which axis the door lies along. */
	bool active;                        /**< True while open; driven by whatever triggers this door. */
	bool used;                          /**< False when this slot is free. */
	u8 id;                              /**< Slot index. */
}door_struct;


/** @brief Clears the door pool and loads the door model. */
void initDoors(void);

/** @brief Releases the door model. */
void freeDoors(void);

/**
 * @brief Places a door.
 * @param r           room to place it in.
 * @param position    position in tile coordinates.
 * @param orientation which axis the door lies along.
 * @return the new door, or NULL if the pool is full.
 */
door_struct* createDoor(room_struct* r, vect3D position, bool orientation);

/** @brief Advances every door's animation and enables or disables its collision. */
void updateDoors(void);

/** @brief Draws every door. */
void drawDoors(void);

#endif
