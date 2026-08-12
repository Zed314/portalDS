/**
 * @file emancipation.h
 * @brief Emancipation grids, and the disintegration effect they produce.
 *
 * An emancipation grid is the blue field across a doorway that destroys
 * anything carried through it and closes both portals. Mechanically it is what
 * stops a chamber being solved by simply carrying a cube out of it.
 *
 * Two unrelated things live here:
 *
 *  - @ref emancipationGrid_struct - the field itself: a line segment in the
 *    floor plan, extruded up to @ref EMANCIPATIONGRIDHEIGHT. It is queried
 *    rather than simulated: @ref collideBoxEmancipationGrids is called from
 *    @ref listenPI9 as each box's new position arrives, and
 *    @ref collideLineEmancipationGrids is what stops the portal gun firing
 *    through one;
 *  - @ref emancipator_struct - the visual effect. When something is
 *    disintegrated it is replaced by one of these, which keeps drawing the
 *    dead object's model while spinning it, blackening it over
 *    @ref BLACKENINGTIME frames and then fading it out over @ref FADINGTIME.
 *    That is why @ref createEmancipator takes a model instance and an
 *    orientation matrix: it inherits the corpse's exact appearance.
 */

#ifndef EMANCIPATION_H
#define EMANCIPATION_H

#define NUMEMANCIPATIONGRIDS (16) /**< Maximum number of grids in a room. */
#define NUMEMANCIPATORS (16)      /**< Maximum number of simultaneous disintegration effects. */

#define EMANCIPATIONGRIDHEIGHT (HEIGHTUNIT*8) /**< How far a grid extends upwards. */
#define BLACKENINGTIME (16) /**< Frames spent darkening before the fade begins. */
#define FADINGTIME (24)     /**< Frames spent fading out after blackening. */

/**
 * @brief The disintegration effect: a dead object's model, spinning and fading.
 */
typedef struct
{
	vect3D position;  /**< Where the effect is, in world space. */
	vect3D velocity;  /**< Drift applied each frame. */
	vect3D axis;      /**< Axis the debris tumbles about. */
	modelInstance_struct modelInstance; /**< A copy of the destroyed object's model state. */
	int32 transformationMatrix[9];      /**< Its orientation at the moment it died. */
	u16 counter;      /**< Frames elapsed; drives the blacken-then-fade sequence. */
	u16 angle;        /**< Current tumble angle. */
	bool used;        /**< False when this slot is free. */
}emancipator_struct;

/**
 * @brief An emancipation grid: a line in plan, extruded upwards.
 */
typedef struct
{
	vect3D position; /**< One end of the line, in tile coordinates. */
	int32 length;    /**< Length of the line. */
	bool direction;  /**< Which axis it runs along. */ //true=Z, false=X
	bool used;       /**< False when this slot is free. */
}emancipationGrid_struct;

/** @brief Clears both pools. */
void initEmancipation(void);

/** @brief Releases everything both pools hold. */
void freeEmancipation(void);

/**
 * @brief Starts a disintegration effect in place of a destroyed object.
 * @param mi  the destroyed object's model instance, copied so the effect looks like it.
 * @param pos where it died.
 * @param m   its orientation at that moment.
 */
void createEmancipator(modelInstance_struct* mi, vect3D pos, int32* m);

/** @brief Advances every disintegration effect and retires the finished ones. */
void updateEmancipators(void);

/** @brief Draws every disintegration effect. */
void drawEmancipators(void);

/**
 * @brief Places an emancipation grid.
 * @param r   room to place it in.
 * @param pos one end of the line, in tile coordinates.
 * @param l   length.
 * @param dir which axis it runs along.
 */
void createEmancipationGrid(room_struct* r, vect3D pos, int32 l, bool dir);

/** @brief Advances the grids' shimmer animation. */
void updateEmancipationGrids(void);

/** @brief Draws every grid. */
void drawEmancipationGrids(void);

/**
 * @brief Computes a grid's bounding box.
 * @param eg  grid to measure.
 * @param pos out: minimum corner.
 * @param sp  out: extent.
 */
void getEmancipationGridAAR(emancipationGrid_struct* eg, vect3D* pos, vect3D* sp);

/**
 * @brief Tests whether a rigid body has passed through a grid.
 *
 * Called from @ref listenPI9 as each box's new position arrives; a true result
 * destroys the box.
 */
bool collideBoxEmancipationGrids(OBB_struct* o);

/**
 * @brief Tests whether a ray crosses a grid.
 *
 * This is what stops the portal gun placing a portal through one.
 *
 * @param l ray origin.
 * @param v ray direction.
 * @param d maximum distance.
 */
bool collideLineEmancipationGrids(vect3D l, vect3D v, int32 d);

#endif
