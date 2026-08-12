/**
 * @file cubes.h
 * @brief Cube dispensers, and the cubes they produce.
 *
 * Weighted storage cubes are ARM7 rigid bodies (see @ref PI9.h) and have no
 * game-side struct of their own. What lives here is the dispenser: the thing
 * that spawns one, holds onto it, and replaces it when it is destroyed.
 *
 * Each dispenser owns at most one cube at a time
 * (cubeDispenser_struct::currentCube), and every cube it produces carries a
 * back-pointer to it in OBB_struct::spawner. That is what makes the
 * emancipation grid work correctly: when a cube is disintegrated,
 * @ref listenPI9 follows the spawner pointer and calls
 * @ref resetCubeDispenserCube to produce a fresh one, rather than leaving the
 * chamber unsolvable.
 *
 * A dispenser can also produce the companion cube
 * (cubeDispenser_struct::companion), which differs only in its texture.
 */

#ifndef CUBES_H
#define CUBES_H

#define NUMCUBEDISPENSERS (8) /**< Maximum number of dispensers in a room. */

/** @brief A cube dispenser. */
typedef struct
{
	vect3D position;                    /**< Position in tile coordinates. */
	modelInstance_struct modelInstance; /**< The dispenser's own model and its opening animation. */
	bool companion;                     /**< True to dispense the companion cube instead of a storage cube. */
	bool active;                        /**< True while the dispenser is open. */
	bool oldActive;                     /**< ::active last frame; the transition is what actually drops a cube. */
	rectangle_struct* openingRectangle; /**< The hatch face, toggled to open and close the hole. */
	OBB_struct* currentCube;            /**< The cube this dispenser has out, or NULL. */
	bool used;                          /**< False when this slot is free. */
	u8 id;                              /**< Slot index. */
}cubeDispenser_struct;

/** @brief Clears the dispenser pool and loads the cube and dispenser models. */
void initCubes(void);

/** @brief Releases the models and every dispenser's cube. */
void freeCubes(void);

/** @brief Draws every dispenser. The cubes themselves are drawn by @ref drawOBBs. */
void drawCubeDispensers(void);

/** @brief Advances every dispenser: the opening animation, and dropping a cube when triggered. */
void updateCubeDispensers(void);

/**
 * @brief Replaces a dispenser's cube with a fresh one at the dispensing point.
 *
 * Called when the cube is emancipated or falls into sludge.
 */
void resetCubeDispenserCube(cubeDispenser_struct* cd);

/**
 * @brief Places a dispenser.
 * @param r         room to place it in.
 * @param pos       position in tile coordinates.
 * @param companion true for a companion cube dispenser.
 * @return the new dispenser, or NULL if the pool is full.
 */
cubeDispenser_struct* createCubeDispenser(room_struct* r, vect3D pos, bool companion);

#endif
