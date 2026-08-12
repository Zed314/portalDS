/**
 * @file grid.h
 * @brief A 2D reference grid for the editor. Not currently built.
 *
 * From an earlier revision of the editor, when levels were laid out in a
 * top-down plan view with an orthographic camera and this grid underneath it.
 * The current editor works directly on block faces in 3D (see
 * @ref roomeditor.h), so nothing calls any of this and grid.c is not in the
 * build.
 *
 * Kept because the plan-view approach is still the easier way to lay out a
 * room's footprint, and the code to draw and pick against a grid is the awkward
 * part of bringing it back.
 */

#ifndef __GRID9__
#define __GRID9__

/** @brief Sets up the grid's geometry. */
void initGrid(void);

/** @brief Draws the grid lines. */
void drawGrid(void);

/** @brief Loads the grid's orthographic projection matrix. */
void projectGrid(void);

/** @brief Applies the grid's view transform. */
void transformGrid(void);

/** @brief Sets the zoom level. */
void setGridScale(int32 s);

/** @brief Sets the pan offset. */
void setGridTranslation(vect3D v);

/** @brief Pans by a relative offset. */
void translateGrid(vect3D v);

/** @brief Zooms by a relative factor. */
void scaleGrid(int32 s);

/**
 * @brief Converts a screen pixel to the grid cell under it.
 * @param x  out: cell x.
 * @param y  out: cell y.
 * @param px screen x in pixels.
 * @param py screen y in pixels.
 */
void getGridCell(int* x, int* y, int px, int py);

#endif
