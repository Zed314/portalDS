/**
 * @file walldoor.h
 * @brief The large doors at each end of a test chamber.
 *
 * Distinct from the ordinary sliding doors in @ref door.h - there are exactly
 * two of these, @ref entryWallDoor and @ref exitWallDoor, and each one has an
 * @ref elevator_struct embedded in it. Together they are how a chamber begins
 * and ends - you arrive in the entry door's lift, and the level is over when
 * you step into the exit door's.
 *
 * Because they are structural rather than pooled, they are set up directly by
 * the level loader with @ref setupWallDoor rather than created from an entity
 * list.
 *
 * @ref drawWallDoors takes a portal, since the doors have to be drawn
 * differently when seen through one - the lift interior beyond an open door is
 * not part of the room geometry, so it needs the viewing portal to decide what
 * is visible.
 */

#ifndef WALLDOOR_H
#define WALLDOOR_H

/** @brief One of the two chamber doors, with its lift. */
typedef struct
{
	vect3D position;     /**< Position in world space. */
	vect3D gridPosition; /**< The same position in tile coordinates. */
	u8 orientation;      /**< Which wall the door is set into. */
	// rectangle_struct* walls;
	modelInstance_struct modelInstance; /**< Model and its opening animation. */
	material_struct* frameMaterial;     /**< Material used for the surrounding frame. */
	rectangle_struct* rectangle;        /**< The door's collision face, disabled while it is open. */
	elevator_struct elevator;           /**< The lift behind this door, by value. */
	bool override;                      /**< Forces the door open regardless of the lift's state; used when loading mid-sequence. */
	bool used;                          /**< False if this chamber has no door at this end. */
}wallDoor_struct;

extern wallDoor_struct entryWallDoor; /**< The door the player arrives through. */
extern wallDoor_struct exitWallDoor;  /**< The door the player leaves through; entering its lift ends the level. */

/** @brief Loads the wall door model and clears both doors. */
void initWallDoors(void);

/** @brief Releases the wall door model. */
void freeWallDoors(void);

/** @brief Advances both doors and their lifts, and ends the level when the exit lift departs. */
void updateWallDoors(void);

/**
 * @brief Places a wall door and its lift.
 * @param r           room to place it in.
 * @param wd          door to set up: @ref entryWallDoor or @ref exitWallDoor.
 * @param position    position in tile coordinates.
 * @param orientation which wall it is set into.
 */
void setupWallDoor(room_struct* r, wallDoor_struct* wd, vect3D position, u8 orientation);

/**
 * @brief Draws both wall doors and their lift interiors.
 * @param p the portal being looked through, or NULL for the direct view.
 *          Needed because the lift interior is not part of the room geometry
 *          and has to be culled against the right viewpoint.
 */
void drawWallDoors(portal_struct* p);

#endif
