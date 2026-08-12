/**
 * @file map.h
 * @brief World scale, tile/world conversion, and every query over a room.
 *
 * Where @ref room.h defines what a room *is*, this file defines the coordinate
 * system it lives in and provides the operations on it: building rooms,
 * ray casting through them, culling them, and lighting objects inside them.
 *
 * @par Three coordinate systems
 * Keeping these straight is most of the battle when reading the game code:
 *
 *  1. **tile coordinates** - the integer grid the editor and the level format
 *     work in. One tile is @ref TILESIZE across and @ref HEIGHTUNIT tall.
 *  2. **world coordinates** - f32, what physics and gameplay use.
 *     @ref convertVect and @ref reverseConvertVect move between 1 and 2, and
 *     note that @ref convertVect places a tile's *centre*, offsetting by half a
 *     tile.
 *  3. **view coordinates** - world scaled by @ref SCALEFACT so a room fits
 *     inside the geometry engine's fixed point range. See
 *     @ref getViewPosition.
 *
 * @par The room grid
 * @ref generateRoomGrid divides the room into @ref CELLSIZE cubes and records,
 * per cell, which rectangles overlap it and which three lights are nearest.
 * Everything performance-sensitive goes through it: @ref collideGridCell for
 * ray casts, @ref setupObjectLighting for dynamic lighting, and the culling in
 * @ref generateRoomDisplayList.
 *
 * @par Display list generation
 * @ref generateRoomDisplayList bakes a room into a hardware command buffer.
 * Passing a position and normal culls to what is visible from there, which is
 * how each portal gets its own pre-culled copy of the room - the alternative,
 * re-submitting the whole room three times a frame, does not fit in the
 * budget.
 */

#ifndef __MAP9__
#define __MAP9__

#define TILESIZE (384)          /**< Half the width of one tile, in f32 world units. */
#define HEIGHTUNIT (192)        /**< Height of one vertical unit, in f32 world units. */
#define SCALEFACT (inttof32(150)) /**< World-to-view scale factor applied before rendering. */

#define CELLSIZE (4) /**< Edge of one spatial grid cell, in tiles. */

// #define MAXHEIGHT 31
#define MAXHEIGHT 47   /**< Number of vertical units a room may span. */
#define STARTHEIGHT 16 /**< Height new rooms start at in the editor. */ //TEMP

#define DEFAULTFLOOR 8    /**< Floor height of a freshly created room. */
#define DEFAULTCEILING 24 /**< Ceiling height of a freshly created room. */

#define LIGHTMAPRESOLUTION 5 /**< Lightmap texels per tile. */

#define LIGHTCONST (0)    /**< Constant term added to every computed light value. */
#define AMBIENTLIGHT (8)  /**< Ambient floor, so nothing is ever fully black. */ //portal is pretty bright, right ?

/** @brief Converts a world-space position to tile coordinates. */
static inline vect3D reverseConvertVect(vect3D v)
{
	return vect((v.x+TILESIZE)/(TILESIZE*2),v.y/HEIGHTUNIT,(v.z+TILESIZE)/(TILESIZE*2));
}

/**
 * @brief Converts tile coordinates to a world-space position.
 * @note Returns the tile's centre, not its corner - hence the half-tile offset.
 */
static inline vect3D convertVect(vect3D v)
{
	return vect(v.x*TILESIZE*2-TILESIZE,v.y*HEIGHTUNIT,v.z*TILESIZE*2-TILESIZE);
}

/** @brief Converts a size in tiles to a size in world units. No centring offset. */
static inline vect3D convertSize(vect3D v)
{
	return vect(v.x*TILESIZE*2,v.y*HEIGHTUNIT,v.z*TILESIZE*2);
}

extern room_struct gameRoom; /**< The room currently being played. */

/** @brief Empties a rectangle list. */
void initRectangleList(rectangleList_struct* p);

/** @brief Pushes a rectangle onto the front of a list. @return the stored copy. */
rectangle_struct* addRectangle(rectangle_struct r, rectangleList_struct* p);

/**
 * @brief Creates an empty room.
 * @param r room to initialise.
 * @param w width in tiles.
 * @param h depth in tiles.
 * @param p world-space origin.
 */
void initRoom(room_struct* r, u16 w, u16 h, vect3D p);

/** @brief Changes a room's footprint, preserving the rectangles that still fit. */
void resizeRoom(room_struct* r, u16 l, u16 w, vect3D p);

/**
 * @brief Adds a rectangle to a room, assigning it a material.
 * @param r          room to add to.
 * @param rec        rectangle to add.
 * @param mat        material to apply.
 * @param portalable whether portals may be placed on it.
 * @return the stored rectangle.
 */
rectangle_struct* addRoomRectangle(room_struct* r, rectangle_struct rec, material_struct* mat, bool portalable);

/** @brief Fills in a rectangle's position and size, deriving its normal from the flat axis. */
void initRectangle(rectangle_struct* rec, vect3D pos, vect3D size);

/** @brief Builds a rectangle by value. */
rectangle_struct createRectangle(vect3D pos, vect3D size, bool portalable);

/** @brief Frees every rectangle in a room. */
void removeRectangles(room_struct* r);

/** @brief Draws every rectangle in a list, without culling. */
void drawRectangleList(rectangleList_struct* rl);

/**
 * @brief Draws a room.
 * @param r     room to draw.
 * @param mode  drawing mode; selects lighting and polygon attributes.
 * @param color tint to apply.
 */
void drawRoom(room_struct* r, u8 mode, u16 color);

/** @brief Releases everything a room owns: rectangles, grid, lighting and display list. */
void freeRoom(room_struct* r);

/** @brief Draws a single rectangle at an explicit position and size. */
void drawRect(rectangle_struct rec, vect3D pos, vect3D size, bool c);

/**
 * @brief Casts a ray through a room and reports the first hit.
 * @param r   room to trace through.
 * @param rec rectangle to ignore, or NULL. Used to avoid re-hitting the surface a ray started on.
 * @param l   ray origin.
 * @param u   ray direction; must be normalised.
 * @param d   maximum distance.
 * @param i   out: hit point.
 * @param n   out: surface normal at the hit.
 * @return true if anything was hit.
 */
bool collideLineMap(room_struct* r, rectangle_struct* rec, vect3D l, vect3D u, int32 d, vect3D* i, vect3D* n);

/** @brief Casts a ray against the rectangles in a single grid cell. @return the rectangle hit, or NULL. */
rectangle_struct* collideGridCell(gridCell_struct* gc, rectangle_struct* rec, vect3D l, vect3D u, int32 d, vect3D* i, vect3D* n);

/**
 * @brief Casts a ray and returns the *nearest* hit rather than the first found.
 *
 * This is the one the portal gun uses - it has to hit the closest surface, not
 * merely a surface.
 *
 * @param r   room to trace through.
 * @param rec rectangle to ignore, or NULL.
 * @param l   ray origin.
 * @param u   ray direction; must be normalised.
 * @param d   maximum distance.
 * @param i   out: hit point.
 * @param lk  out: distance along the ray to the hit.
 * @return the rectangle hit, or NULL.
 */
rectangle_struct* collideLineMapClosest(room_struct* r, rectangle_struct* rec, vect3D l, vect3D u, int32 d, vect3D* i, int32* lk);

/** @brief Shifts every rectangle in a room by an offset, in tile coordinates. */
void translateRectangles(room_struct* r, vect3D v);

/**
 * @brief Finds the floor or ceiling height above or below a position.
 * @param r     room to look in.
 * @param pos   position in tile coordinates.
 * @param floor true for the floor, false for the ceiling.
 */
u8 getHeightValue(room_struct* r, vect3D pos, bool floor);

/**
 * @brief Bakes a room into a hardware display list, optionally culled to a viewpoint.
 *
 * @param r      room to bake, or NULL for @ref gameRoom.
 * @param pos    viewpoint to cull against.
 * @param normal viewing direction to cull against.
 * @param cull   true to cull; false to include every rectangle.
 * @return a newly allocated display list, owned by the caller.
 */
u32* generateRoomDisplayList(room_struct* r, vect3D pos, vect3D normal, bool cull);

/**
 * @brief Computes lighting parameters for a dynamic object at a position.
 *
 * Uses the three nearest lights cached in the object's grid cell.
 *
 * @param r      room the object is in.
 * @param pos    object position.
 * @param params out: polygon attribute words to submit before drawing it.
 */
void setupObjectLighting(room_struct* r, vect3D pos, u32* params);

/** @brief Builds a room's spatial grid. Must be run after the rectangles are final. */
void generateRoomGrid(room_struct* r);

/** @brief Returns the grid cell containing a world-space position, or NULL. */
gridCell_struct* getCurrentCell(room_struct* r, vect3D o);

/** @brief Returns a rectangle's outward normal as a unit vector. */
vect3D getUnitVect(rectangle_struct* rec);

//lightmaps.h

/** @brief Uploads a room's lightmap atlas into VRAM. */
void loadLightMap(room_struct* r);

/** @brief Releases a room's lightmap from VRAM. */
void unloadLightMap(room_struct* r);

/** @brief Releases every lightmap a room holds. */
void unloadAllLightMaps(room_struct* r);

/** @brief Releases the lightmaps @p r holds that @p r2 does not also need. */
void unloadLightMaps(room_struct* r, room_struct* r2);

/**
 * @brief Registers a room's collision rectangles with the ARM7.
 *
 * Sends a @ref createAAR for every colliding face and records the returned ids
 * in rectangle_struct::AARid. Follow with @ref makeGrid, or none of it will
 * collide.
 */
void transferRectangles(room_struct* r);

/** @brief Removes the front rectangle from a list. */
void popRectangle(rectangleList_struct* p);
#endif
