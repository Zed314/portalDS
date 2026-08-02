/**
 * @file timedbutton.h
 * @brief Wall buttons that stay pressed for a while.
 *
 * The other half of the button vocabulary. Unlike a floor button
 * (@ref bigbutton.h), a timed button is not held down by weight: pressing it
 * starts a countdown, and its targets stay active until the countdown expires.
 * timedButton_struct::active *is* that countdown - it is a frame count, not a
 * boolean.
 *
 * That is what turns a chamber into a timing puzzle: press the button, then get
 * through the door it opened before it closes again.
 *
 * A timed button can be pressed either by touching it
 * (@ref checkObjectTimedButtonsCollision) or by shooting it with the gun
 * (@ref collideRayTimedButtons).
 */

#ifndef TIMEDBUTTON_H
#define TIMEDBUTTON_H

#define NUMTIMEDBUTTONS (16) /**< Maximum number of timed buttons in a room. */

/** @brief A wall-mounted timed button. */
typedef struct
{
	room_struct* room;                  /**< Room the button is in. */
	activator_struct activator;         /**< What this button drives. */
	modelInstance_struct modelInstance; /**< Model and its press animation. */
	vect3D position;                    /**< Position in tile coordinates. */
	u16 angle;                          /**< Facing angle - which wall it is mounted on. */
	u16 active;                         /**< Frames remaining before the button releases; zero means idle. */
	bool used;                          /**< False when this slot is free. */
	u8 id;                              /**< Slot index. */
}timedButton_struct;

/** @brief Clears the button pool and loads the button model. */
void initTimedButtons(void);

/** @brief Releases the button model. */
void freeTimedButtons(void);

/** @brief Presses a button: starts its countdown and fires its targets. */
void activateTimedButton(timedButton_struct* tb);

/**
 * @brief Finds the timed button a ray hits - how the gun presses one from a distance.
 * @param o ray origin.
 * @param v ray direction; must be normalised.
 * @param l maximum range.
 * @return the button hit, or NULL.
 */
timedButton_struct* collideRayTimedButtons(vect3D o, vect3D v, int32 l);

/**
 * @brief Places a timed button.
 * @param r        room to place it in.
 * @param position position in tile coordinates.
 * @param angle    which way it faces.
 * @return the new button, or NULL if the pool is full.
 */
timedButton_struct* createTimedButton(room_struct* r, vect3D position, u16 angle);

/**
 * @brief Presses any button the given object is touching.
 * @return true if a button was pressed.
 */
bool checkObjectTimedButtonsCollision(physicsObject_struct* o, room_struct* r);

/** @brief Draws every timed button. */
void drawTimedButtons(void);

/** @brief Counts each pressed button down and releases its targets when it expires. */
void updateTimedButtons(void);

#endif
