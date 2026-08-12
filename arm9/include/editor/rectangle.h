/**
 * @file rectangle.h
 * @brief 2D rectangle packing for lightmap atlases, plus rectangle geometry tests.
 *
 * Two unrelated things share this file.
 *
 * @par Packing
 * Every rectangle in a room needs a patch in the lightmap atlas, and they have
 * to be fitted into one texture without overlapping. @ref packRectangles solves
 * that with the classic binary-tree bin packing algorithm: the atlas starts as
 * one empty node, and each rectangle inserted splits the node it lands in into
 * the used part and the two leftovers - hence @ref treeNode_type's
 * @ref HORIZONTAL and @ref VERTICAL splits. Patches may be rotated 90 degrees
 * to fit, which is what rectangle2D_struct::rot records.
 *
 * @ref packRectanglesSize goes further and searches for an atlas size that
 * works, since the smallest texture that fits is not known in advance.
 *
 * @par Geometry
 * The rest is rectangle maths used throughout the game and editor:
 * ray-rectangle intersection (@ref collideLineRectangle), closest-point queries
 * used by the collision code, and @ref bindMaterial, which picks the right
 * material slice for a face and computes its texture coordinates.
 */

#ifndef __RECTANGLE9__
#define __RECTANGLE9__

/** @brief What a node of the packing tree represents. */
typedef enum
{
	EMPTY,      /**< Free space that nothing has been placed in yet. */
	RECTANGLE,  /**< Occupied by a placed rectangle. */
	HORIZONTAL, /**< Split horizontally into two children. */
	VERTICAL    /**< Split vertically into two children. */
}treeNode_type;

/** @brief One node of the binary packing tree. */
typedef struct treeNode_struct
{
	struct treeNode_struct* son[2]; /**< The two children, for a split node. */
	treeNode_type type;             /**< What this node is. */
	short data;                     /**< Split position, or the index of the rectangle placed here. */
}treeNode_struct;

/** @brief A packing tree over one atlas. */
typedef struct
{
	treeNode_struct* root;  /**< Root node, covering the whole atlas. */
	short width, height;    /**< Atlas dimensions. */
}tree_struct;

/** @brief A rectangle being packed into an atlas. */
typedef struct
{
	vect2D position; /**< Position within the atlas, filled in by the packer. */
	vect2D size;     /**< Size of the patch. */
	lightMapCoordinates_struct* real; /**< Where to write the result back to. */
	bool rot;        /**< True if the packer rotated this patch to make it fit. */
}rectangle2D_struct;

/** @brief One cell of a 2D rectangle list. */
typedef struct listCell2D_struct
{
	rectangle2D_struct data;        /**< The rectangle, stored by value. */
	struct listCell2D_struct* next; /**< Next cell, or NULL. */
}listCell2D_struct;

/**
 * @brief A list of rectangles to pack.
 *
 * Kept sorted by size, largest first - bin packing gives much better results
 * that way, which is why ::surface is tracked as they are inserted.
 */
typedef struct
{
	listCell2D_struct* first; /**< Head of the list. */
	int surface;              /**< Total area of every rectangle in the list. */
	int num;                  /**< Number of rectangles. */
}rectangle2DList_struct;

/** @brief Empties a 2D rectangle list. */
void initRectangle2DList(rectangle2DList_struct* l);

/** @brief Inserts a rectangle, keeping the list ordered largest first. */
void insertRectangle2DList(rectangle2DList_struct* l, rectangle2D_struct rec);

/**
 * @brief Packs every rectangle in a list into an atlas of the given size.
 * @param l list to pack; each entry's position is filled in.
 * @param w atlas width.
 * @param h atlas height.
 */
void packRectangles(rectangle2DList_struct* l, short w, short h);

/**
 * @brief Finds an atlas size that everything fits into, and packs it.
 * @param l list to pack.
 * @param w in/out: starting width, and the width that worked.
 * @param h in/out: starting height, and the height that worked.
 * @return true if a size was found.
 */
bool packRectanglesSize(rectangle2DList_struct* l, short* w, short* h);

/** @brief Releases a 2D rectangle list. */
void freeRectangle2DList(rectangle2DList_struct* l);

/** @brief Fills a rectangular region of a byte buffer with a mask value. */
void fillRectangle(u8* data, int w, int h, vect2D* pos, vect2D* size, u8 mask);

/**
 * @brief Finds the largest rectangle of a given value in a byte buffer.
 *
 * This is the merging step behind @ref generateOptimizedRectangles - run
 * repeatedly over a face's occupancy map, it turns a grid of block faces into
 * as few large rectangles as possible.
 *
 * @param data buffer to search.
 * @param val  value to look for.
 * @param w    buffer width.
 * @param h    buffer height.
 * @param pos  out: minimum corner of the rectangle found.
 * @param size out: its size.
 */
void getMaxRectangle(u8* data, u8 val, int w, int h, vect2D* pos, vect2D* size);

/**
 * @brief Intersects a ray with a rectangle.
 * @param rec rectangle to test.
 * @param o   ray origin.
 * @param v   ray direction.
 * @param d   maximum distance.
 * @param kk  out: distance along the ray to the hit.
 * @param ip  out: the hit point.
 */
bool collideLineRectangle(rectangle_struct* rec, vect3D o, vect3D v, int32 d, int32* kk, vect3D* ip);

/** @brief Ray-rectangle intersection against an explicit normal, position and size. */
bool collideLineConvertedRectangle(vect3D n, vect3D p, vect3D s, vect3D o, vect3D v, int32 d, int32* kk, vect3D* ip);

/** @brief Returns the point on a rectangle closest to @p o. Used by the sphere collision. */
vect3D getClosestPointRectangleStruct(rectangle_struct* rec, vect3D o);

/** @brief Closest-point query against an explicit position and size. */
vect3D getClosestPointRectangle(vect3D p, vect3D s, vect3D o);

/**
 * @brief Selects and binds the right material slice for a face, and builds its texture coordinates.
 *
 * Which slice is used depends on the rectangle's normal - top, side or bottom.
 *
 * @param m   material to bind.
 * @param rec rectangle being drawn.
 * @param t   out: texture coordinates.
 * @param v   out: the rectangle's corners.
 * @param DL  true to record into a display list rather than submit directly.
 * @return the slice that was bound.
 */
materialSlice_struct* bindMaterial(material_struct* m, rectangle_struct* rec, int32* t, vect3D* v,bool DL);

#endif
