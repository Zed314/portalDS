/**
 * @file io.h
 * @brief The level file format.
 *
 * A level file is a fixed-size @ref mapHeader_struct followed by five
 * independently located sections. The header stores a byte offset for each, so
 * a reader can seek straight to the part it wants - which is what the @c flags
 * argument to @ref newReadMap uses.
 *
 * @par The sections
 *  - **data** - the editor's block array, RLE compressed (see
 *    @ref compress.h). Only the editor reads this; the game ignores it.
 *  - **rectangles** - the optimised rectangle soup produced by
 *    @ref generateOptimizedRectangles. This is what the game actually plays.
 *  - **entities** - placed objects, with trigger targets stored as indices.
 *  - **lights** - lightmaps or baked vertex colours.
 *  - **sludge** - the rectangles flagged as toxic goo.
 *
 * So a saved level carries both representations: the editable block array *and*
 * the derived rectangles. That is why saving takes a moment - the conversion,
 * the lightmap packing and the lighting bake all happen at save time so that
 * loading is fast.
 *
 * @par Symmetry
 * writeEntity() in io.c and readEntity() in game/room.c must agree exactly. The
 * format is positional with no self-description, so adding a field to one
 * without the other silently corrupts every entity after it.
 *
 * @ref mapHeader_struct::blank is reserved space for exactly that kind of
 * extension.
 */

#ifndef EDITORIO_H
#define EDITORIO_H


#define MAPHEADER_SIZE (256) /**< Fixed size of the level file header, in bytes. */

/**
 * @brief The level file header: a byte offset per section.
 */
typedef struct
{
	u32 dataSize;           /**< Size of the compressed block array. */
	u32 dataPosition;       /**< Offset of the compressed block array. */
	u32 rectanglesPosition; /**< Offset of the rectangle list the game plays. */
	u32 entityPosition;     /**< Offset of the entity list. */
	u32 lightPosition;      /**< Offset of the lighting data. */
	u32 sludgePosition;     /**< Offset of the sludge rectangle list. */

	u8 blank[MAPHEADER_SIZE-6*4]; /**< Reserved, so new sections can be added without breaking old files. */ //for future use
}mapHeader_struct;

/** @brief Reads the header from an open level file. */
void readHeader(mapHeader_struct* h, FILE* f);

// The editor-only entry points, hidden from translation units that have not
// included blocks.h and therefore have no editorRoom_struct.
#ifdef BLOCKS_H
	/**
	 * @brief Saves a level.
	 *
	 * Writes the block array, then derives and writes everything the game
	 * needs: optimised rectangles, packed lightmaps and baked lighting. This
	 * is where the expensive part of editing happens.
	 *
	 * @param er room to save.
	 * @param str file name to write.
	 */
	void writeMapEditor(editorRoom_struct* er, const char* str);

	/**
	 * @brief Loads a level for editing.
	 *
	 * Reads the block array section and rebuilds the face list and entities
	 * from it; the derived rectangle and lighting sections are ignored, since
	 * they will be regenerated on the next save.
	 *
	 * @param er room to load into.
	 * @param str file name to read.
	 * @return true on success.
	 */
	bool loadMapEditor(editorRoom_struct* er, const char* str);
#endif

#endif
