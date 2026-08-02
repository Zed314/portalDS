/**
 * @file room.h
 * @brief The room: a soup of textured rectangles, plus a grid over them.
 *
 * A test chamber is not a mesh. It is a flat list of axis aligned rectangles,
 * each with a material, a normal and lighting data - the same representation
 * the editor works in, and the same one the ARM7 collides against (see
 * @ref AAR.h). Everything about the level format follows from that choice: it
 * is trivial to edit on a touch screen, cheap to collide, and it maps directly
 * onto quads for the geometry engine.
 *
 * @par Two structures over the same rectangles
 * The rectangles themselves live in a linked @ref rectangleList_struct, which
 * is what the editor mutates. For anything that has to be fast,
 * @ref generateRoomGrid builds room_struct::rectangleGrid: a 3D array of
 * @ref gridCell_struct, each holding pointers to the rectangles overlapping it
 * plus the three nearest lights. Ray casts, culling and object lighting all go
 * through the grid; only editing goes through the list.
 *
 * @par Lighting
 * Precomputed, either per-vertex or as lightmaps depending on the material -
 * hence the union in @ref rectangle_struct. @ref generateLightmaps and
 * @ref generateVertexLighting bake it, which is why saving a level in the
 * editor takes a moment.
 *
 * @see map.h for the queries over a room, and editor/io.c for the file format.
 */

#ifndef ROOM_H
#define ROOM_H

/**
 * @brief One axis aligned, textured rectangle - the atom the world is built from.
 *
 * ::position is the minimum corner and ::size the extent from there, flat along
 * whichever axis ::normal points down.
 */
typedef struct
{
	vect3D position; /**< Minimum corner. */
	vect3D size;     /**< Extent from ::position; zero along the normal's axis. */
	vect3D normal;   /**< Outward facing normal. */
	material_struct* material; /**< Surface appearance: texture, and whether it takes portals. */
	s16 AARid;       /**< Id of the matching ARM7 collision rectangle, or -1 if this face does not collide. */
	/** @brief Precomputed lighting; which member is live depends on the material. */
	union{
		vertexLightingData_struct* vertex;   /**< Per-corner colours. */
		lightMapCoordinates_struct* lightMap;/**< Coordinates into the room's lightmap atlas. */
	}lightData;
	bool portalable; /**< Whether a portal can be placed on this face. */
	bool hide;       /**< Suppresses drawing; used for faces that only exist to collide. */
	bool touched;    /**< Scratch flag used by the editor's selection and by trigger logic. */
	bool collides;   /**< Whether this face has a collision rectangle at all. */
}rectangle_struct;

/** @brief One cell of the rectangle linked list. */
typedef struct listCell_struct
{
	rectangle_struct data;        /**< The rectangle itself, stored by value. */
	struct listCell_struct* next; /**< Next cell, or NULL. */
}listCell_struct;

/**
 * @brief A linked list of rectangles.
 *
 * The editable representation. New rectangles are pushed at the front, so ids
 * are not stable across edits.
 */
typedef struct
{
	listCell_struct* first; /**< Head of the list. */
	int num;                /**< Number of rectangles. */
}rectangleList_struct;

/**
 * @brief One cell of the spatial grid over a room.
 *
 * Caching the three nearest lights per cell is what makes dynamic object
 * lighting affordable: an object looks up its cell and uses those, rather than
 * considering every light in the room.
 */
typedef struct
{
	rectangle_struct** rectangles; /**< Rectangles overlapping this cell. */
	light_struct* lights[3];       /**< The three nearest lights. */
	int32 lightDistances[3];       /**< Distance to each of them. */
	u8 numRectangles;              /**< Number of entries in ::rectangles. */
}gridCell_struct;

/**
 * @brief A complete room.
 */
typedef struct
{
	material_struct** materials;  /**< Materials used by this room, loaded on demand. */
	vect3D position;              /**< World-space origin of the room. */
	vect3D lmSize;                /**< Dimensions of the lightmap atlas. */
	u16 width, height;            /**< Room extents in tiles. */
	gridCell_struct* rectangleGrid; /**< Spatial grid; see @ref generateRoomGrid. */
	vect3D rectangleGridSize;     /**< Grid dimensions in cells. */
	rectangleList_struct rectangles; /**< The editable rectangle list. */
	lightingData_struct lightingData; /**< Lights and baked lighting for this room. */
	u32* displayList;             /**< Pre-built display list of the whole room. */
}room_struct;

/** @brief Draws the editor's pending edits on top of the room. */
void drawRoomEdits(void);

/** @brief Discards the editor's pending edits. */
void wipeMapEdit(void);

/** @brief Writes the current room to a level file. */
void writeMap(char* filename);

/** @brief Reads a level file in the original format. Superseded by @ref newReadMap. */
void readMap(char* filename, room_struct* r);

/**
 * @brief Reads a level file in the current format.
 * @param filename level file to read.
 * @param r        room to fill, or NULL for @ref gameRoom.
 * @param flags    bit mask selecting which sections to load; 255 loads everything.
 */
void newReadMap(char* filename, room_struct* r, u8 flags);

/** @brief Shifts a room so its minimum corner sits at the origin. */
void roomResetOrigin(room_struct* r);

/**
 * @brief Computes a room's bounding box.
 * @param r room to measure.
 * @param o out: minimum corner.
 * @param s out: extent.
 */
void roomOriginSize(room_struct* r, vect3D* o, vect3D* s);

/**
 * @brief Merges one room into another at an offset and orientation.
 *
 * How multi-room levels are assembled from separately edited pieces.
 *
 * @param r1          destination room.
 * @param r2          room to insert.
 * @param v           offset to place it at.
 * @param orientation quarter turns to rotate it by.
 */
void insertRoom(room_struct* r1, room_struct* r2, vect3D v, u8 orientation);

/** @brief Bakes lightmaps for a room. Slow; run when saving, not when loading. */
void generateLightmaps(room_struct* r, lightingData_struct* ld);

/** @brief Bakes per-vertex lighting for a room. */
void generateVertexLighting(room_struct* r, lightingData_struct* ld);

/**
 * @brief Draws the room as seen in game, including through any open portals.
 * @param mode  drawing mode; selects the lighting and polygon attributes.
 * @param color tint to apply, from @ref getCurrentPortalColor.
 */
void drawRoomsGame(u8 mode, u16 color);
#endif
