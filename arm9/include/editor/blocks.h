/**
 * @file blocks.h
 * @brief The block array - what the editor actually edits.
 *
 * The game's world is a soup of rectangles (@ref room.h), which is efficient to
 * render and collide but hopeless to author with a stylus. So the editor works
 * in a different model entirely: a dense 3D array of blocks, each either solid
 * or empty, carved out like Minecraft in reverse. Rectangles are only produced
 * at save time, by @ref generateOptimizedRectangles.
 *
 * @par The block word
 * Each block is a @ref BLOCK_TYPE bitfield:
 * @code
 *  bit 0     : solid
 *  bits 1-6  : portalable, one bit per face direction
 *  bit 7     : no walls (BLOCK_NOWALLS)
 *  bit 8     : sludge (BLOCK_SLUDGE)
 *  bits 9-15 : unused
 * @endcode
 * Direction indices run 0:+X 1:-X 2:+Y 3:-Y 4:+Z 5:-Z throughout the editor,
 * and @ref oppositeDirection flips one.
 *
 * @par Block faces
 * A @ref blockFace_struct is one visible face of one block - the boundary
 * between a solid block and the empty one next to it. The list is what the
 * editor draws and what the stylus picks against, and it is rebuilt by
 * @ref generateBlockFacesRange whenever blocks change. Faces come from a fixed
 * pool of @ref BLOCKFACEPOOLSIZE entries, not the heap.
 *
 * @par Optimisation at save time
 * @ref generateOptimizedRectangles is the important one: it merges adjacent
 * coplanar faces into the largest rectangles it can. A room the player would
 * describe as "a big square wall" becomes one rectangle rather than a hundred,
 * which is the difference between a level that renders and one that does not.
 */

#ifndef BLOCKS_H
#define BLOCKS_H

#define BLOCKFACEPOOLSIZE (2048*4) /**< Size of the block face pool. */

#define ROOMARRAYSIZEX (64) /**< Block array width. */
#define ROOMARRAYSIZEY (64) /**< Block array height. */
#define ROOMARRAYSIZEZ (64) /**< Block array depth. */

#define BLOCKMULT (2) /**< Overall block-to-tile scale. */

#define BLOCKMULTX (BLOCKMULT*1) /**< Tiles per block along X. */
#define BLOCKMULTY (BLOCKMULT*4) /**< Height units per block along Y - blocks are taller than they are wide. */
#define BLOCKMULTZ (BLOCKMULT*1) /**< Tiles per block along Z. */

#define BLOCKSIZEX (TILESIZE*2*BLOCKMULTX) /**< Block width in world units. */
#define BLOCKSIZEY (HEIGHTUNIT*BLOCKMULTY) /**< Block height in world units. */
#define BLOCKSIZEZ (TILESIZE*2*BLOCKMULTZ) /**< Block depth in world units. */

#define BLOCK_NOWALLS (1<<7) /**< Block flag: suppress the surrounding wall faces. */
#define BLOCK_SLUDGE (1<<8)  /**< Block flag: this block's surface is toxic goo. */

#define BLOCK_TYPE u16 /**< Storage type of one block; see the bit layout above. */

//DIR : 0 X
//		1 -X
//		2 Y
//		3 -Y
//		4 Z
//		5 -Z

//BLOCK DATA STRUCTURE
// BIT 0 : WALL
// BITS 1-6 : PORTALABILITY (1 BIT PER DIRECTION)
// BIT 7 : NOWALLS
// BIT 8 : SLUDGE
// BITS 9-15 : unused

/**
 * @brief One visible face of one block.
 *
 * A face exists where a solid block meets an empty one. These are what the
 * editor draws and what the stylus picks against.
 */
typedef struct blockFace_struct
{
	u8 x, y, z;      /**< Coordinates of the block this face belongs to. */
	u8 direction;    /**< Which face: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z. */
	bool draw;       /**< Whether to draw it; cleared for faces hidden by an entity. */
	struct blockFace_struct* next; /**< Next face in the list. */
}blockFace_struct;

/** @brief A room as the editor holds it: the block array plus its face list. */
typedef struct
{
	BLOCK_TYPE* blockArray;        /**< The dense block array, @ref ROOMARRAYSIZEX x Y x Z. */
	blockFace_struct* blockFaceList; /**< Every visible face, regenerated when blocks change. */
}editorRoom_struct;

extern vect3D faceNormals[6];  /**< Outward normal for each of the six face directions. */
extern vect3D faceOrigin[6];   /**< Corner offset of each face within its block. */
extern u32 packedVertex[6][4]; /**< The four corners of each face, pre-packed for the geometry engine. */
extern u8 oppositeDirection[6];/**< Maps a direction index to the opposite one. */

/**
 * @brief Callback that changes one block's attribute bits.
 *
 * Passed to @ref changeAttributeBlockArrayRange so one traversal can serve
 * every attribute; @ref changePortalableBlockDirection and
 * @ref changeSludgeBlock are the implementations.
 */
typedef void(*blockAttributeFunction)(BLOCK_TYPE* ba, u8 x, u8 y, u8 z, u16 mask, bool unset);

/** @brief Allocates a room's block array and clears it to empty. */
void initEditorRoom(editorRoom_struct* er);

/** @brief Releases a room's block array and face list. */
void freeEditorRoom(editorRoom_struct* er);

/** @brief Draws every visible block face. */
void drawEditorRoom(editorRoom_struct* er);

/** @brief Applies the editor camera's transform to the geometry engine. */
void editorRoomTransform(void);

/** @brief Initialises the block face pool. */
void initBlocks(void);

/** @brief Returns every face to the pool. */
void freeBlockFacePool(void);

/** @brief Removes and returns the first face of a list. */
blockFace_struct* popBlockFace(blockFace_struct** l);

/** @brief Converts block coordinates to a world-space position. */
vect3D getBlockPosition(u8 x, u8 y, u8 z);

/** @brief Returns a single face to the pool. */
void freeBlockFace(blockFace_struct* bf);

/** @brief Pushes a face onto the front of a list. */
void addBlockFace(blockFace_struct** l, blockFace_struct* bf);

/** @brief Searches a list for the face of a given block in a given direction. */
blockFace_struct* findBlockFace(blockFace_struct* l, u8 x, u8 y, u8 z, u8 direction);

/**
 * @brief Finds the nearest face a ray hits - the editor's stylus picking test.
 * @param l face list to search.
 * @param o ray origin.
 * @param v ray direction.
 * @param d in/out: maximum distance on entry, distance to the hit on exit.
 * @return the face hit, or NULL.
 */
blockFace_struct* collideLineBlockFaceListClosest(blockFace_struct* l, vect3D o, vect3D v, int32* d);

/** @brief Permutes a vector's components into the frame of a face direction. */
vect3D adjustVectForNormal(u8 dir, vect3D v);

/** @brief Returns the minimum corner of the occupied part of a block array. */
vect3D getMinBlockArray(BLOCK_TYPE* ba);

/** @brief Normalises an origin and size so the size is non-negative. */
void fixOriginSize(vect3D* o, vect3D* s);

/** @brief Returns a whole face list to the pool and NULLs the caller's pointer. */
void freeBlockFaceList(blockFace_struct** l);

/**
 * @brief Regenerates the visible faces over a range of blocks.
 * @param ba        block array.
 * @param l         face list to update.
 * @param o         minimum corner of the range.
 * @param s         extent of the range.
 * @param outskirts true to also refresh the faces just outside the range,
 *                  which is needed because carving a block exposes its
 *                  neighbours' faces.
 */
void generateBlockFacesRange(BLOCK_TYPE* ba, blockFace_struct** l, vect3D o, vect3D s, bool outskirts);

/** @brief Clears a box of blocks to empty and refreshes the affected faces. */
void emptyBlockArrayRange(BLOCK_TYPE* ba, blockFace_struct** l, vect3D o, vect3D s);

/** @brief Sets a box of blocks solid and refreshes the affected faces. */
void fillBlockArrayRange(BLOCK_TYPE* ba, blockFace_struct** l, vect3D o, vect3D s);

/** @brief Sets or clears one block's portalable bit for one direction. A @ref blockAttributeFunction. */
void changePortalableBlockDirection(BLOCK_TYPE* ba, u8 x, u8 y, u8 z, u16 u, bool portalable);

/** @brief Sets or clears one block's sludge flag. A @ref blockAttributeFunction. */
void changeSludgeBlock(BLOCK_TYPE* ba, u8 x, u8 y, u8 z, u16 u, bool nosludge);

/** @brief Applies an attribute change over a box of blocks. */
void changeAttributeBlockArrayRange(BLOCK_TYPE* ba, blockAttributeFunction f, blockFace_struct* l, vect3D o, vect3D s, bool unset);

/** @brief Applies an attribute change over a box of blocks, for one face direction only. */
void changeAttributeBlockArrayRangeDirection(BLOCK_TYPE* ba, blockAttributeFunction f, blockFace_struct* l, vect3D o, vect3D s, u8 dir, bool portalable);

/**
 * @brief Converts a block array into the game's rectangle representation.
 *
 * The bridge between the two models, run when a level is saved. Adjacent
 * coplanar faces are merged into the largest rectangles possible - without
 * this a wall would become one rectangle per block face and no level would fit
 * in the polygon budget.
 *
 * @param ba         block array to convert.
 * @param sludgeList out: the rectangles flagged as toxic goo, kept separate.
 * @return the room's rectangle list.
 */
rectangleList_struct generateOptimizedRectangles(BLOCK_TYPE* ba, rectangleList_struct* sludgeList);

/**
 * @brief Reads one block, treating out-of-range coordinates as empty.
 *
 * The bounds check is what lets the face generator look at a block's
 * neighbours without special-casing the array edges.
 */
BLOCK_TYPE getBlock(BLOCK_TYPE* ba, s8 x, s8 y, s8 z);
#endif
