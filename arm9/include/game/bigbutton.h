/**
 * @file bigbutton.h
 * @brief Floor buttons - the ones you hold down with a cube.
 *
 * A pressure plate. It is active for exactly as long as something is standing
 * on it, which is the whole reason weighted cubes exist: the player cannot be
 * in two places at once, so holding a button open means finding something else
 * to put on it.
 *
 * Each frame @ref updateBigButtons checks whether the player or any rigid body
 * is within range, and calls @ref useActivator or @ref unuseActivator as the
 * answer changes. See @ref activator.h for what happens next.
 *
 * @see timedbutton.h for the other kind - the ones that stay on for a while
 *      after being shot.
 */

#ifndef BIGBUTTON_H
#define BIGBUTTON_H

#define NUMBIGBUTTONS (16) /**< Maximum number of floor buttons in a room. */

/** @brief A floor button. */
typedef struct
{
	room_struct* room;                  /**< Room the button is in. */
	rectangle_struct* surface;          /**< The floor face the button sits on. */
	activator_struct activator;         /**< What this button drives. */
	modelInstance_struct modelInstance; /**< Model and its press animation. */
	vect3D position;                    /**< Position in tile coordinates. */
	bool active;                        /**< True while something is standing on it. */
	bool used;                          /**< False when this slot is free. */
	u8 id;                              /**< Slot index. */
}bigButton_struct;

/** @brief Clears the button pool and loads the button model. */
void initBigButtons(void);

/** @brief Releases the button model. */
void freeBigButtons(void);

/**
 * @brief Places a floor button.
 * @param r        room to place it in.
 * @param position position in tile coordinates.
 * @return the new button, or NULL if the pool is full.
 */
bigButton_struct* createBigButton(room_struct* r, vect3D position);

/** @brief Draws every floor button. */
void drawBigButtons(void);

/** @brief Tests what is standing on each button and fires or releases its targets. */
void updateBigButtons(void);

#endif
