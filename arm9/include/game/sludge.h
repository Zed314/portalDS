/**
 * @file sludge.h
 * @brief Toxic goo.
 *
 * The bottomless-pit hazard: anything that falls into it is destroyed, and the
 * player dies. It has no entity of its own - a sludge surface is just a
 * @ref rectangle_struct that has been registered with @ref addSludgeRectangle
 * by the level loader, and this file keeps a list of them to test against.
 *
 * The queries are the point of the file: @ref collideBoxSludge is called from
 * @ref listenPI9 as each rigid body's new position arrives, alongside the
 * emancipation grid test, so a cube dropped in the goo is destroyed and
 * re-dispensed rather than sinking out of sight.
 */

#ifndef SLUDGE_H
#define SLUDGE_H

#define SLUDGEMARGIN (256) /**< How far above the surface counts as being in the sludge, in f32. */

/** @brief Clears the sludge surface list and loads the goo texture. */
void initSludge(void);

/** @brief Releases the goo texture. */
void freeSludge(void);

/** @brief Registers a rectangle as a sludge surface. Called by the level loader. */
void addSludgeRectangle(rectangle_struct* rec);

/** @brief Draws every sludge surface with its animated texture. */
void drawSludge(room_struct* r);

/**
 * @brief Tests whether a rigid body has fallen into sludge.
 *
 * Called from @ref listenPI9; a true result destroys the box.
 */
bool collideBoxSludge(OBB_struct* o);

/**
 * @brief Tests whether an axis aligned box overlaps any sludge surface.
 * @param p minimum corner.
 * @param s extent.
 */
bool collideAABBSludge(vect3D p, vect3D s);

#endif
