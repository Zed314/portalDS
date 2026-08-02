/**
 * @file lighting.h
 * @brief Baked lighting data, in either of two forms.
 *
 * All world lighting is precomputed. A room's lighting is held in a
 * @ref lightingData_struct, which is a tagged union of the two schemes:
 *
 *  - **lightmaps** (@ref LIGHTMAP_DATA) - a texture atlas of light values, one
 *    patch per rectangle. Detailed, and gives soft shadows across a large
 *    surface, but costs VRAM and needs the atlas packed (see
 *    @ref rectangle.h);
 *  - **vertex lighting** (@ref VERTEXLIGHT_DATA) - a small grid of light values
 *    per rectangle, interpolated across it. Nearly free, but only as detailed
 *    as the geometry.
 *
 * The choice is per room. A level with many small faces gets little from
 * lightmaps and would pay heavily in VRAM; a level with a few large walls gets
 * a great deal.
 *
 * Both are produced at save time by @ref generateLightmaps and
 * @ref generateVertexLighting, from the @ref light_struct list. Nothing is lit
 * at runtime except moving objects, which use @ref setupObjectLighting instead.
 */

#ifndef LIGHTING_H
#define LIGHTING_H

/** @brief Which lighting scheme a room uses. */
typedef enum
{
	LIGHTMAP_DATA,   /**< Lightmap atlas; lightMapData_struct is live. */
	VERTEXLIGHT_DATA /**< Per-vertex values; vertexLightingData_struct is live. */
}lightingData_type;

/** @brief Where one rectangle's patch sits within the lightmap atlas. */
typedef struct
{
	vect3D lmPos;  /**< Position of the patch in the atlas. */
	vect3D lmSize; /**< Size of the patch. */
	bool rot;      /**< True if the patch was rotated 90 degrees to pack better. */
}lightMapCoordinates_struct;

/** @brief A room's lightmap atlas. */
typedef struct
{
	vect3D lmSize; /**< Atlas dimensions. */
	u8* buffer;    /**< The light values, before upload. */
	lightMapCoordinates_struct* coords; /**< One entry per rectangle, saying where its patch is. */
	mtlImg_struct* texture; /**< The uploaded atlas. */
}lightMapData_struct;

/** @brief One rectangle's per-vertex light values. */
typedef struct
{
	u8 width, height; /**< Dimensions of the value grid. */
	u8* values;       /**< The light values, row-major. */
}vertexLightingData_struct;

/** @brief A room's lighting, in whichever form it uses. */
typedef struct
{
	lightingData_type type; /**< Which member of ::data is live. */
	union{
		lightMapData_struct lightMap;            /**< Live when ::type is @ref LIGHTMAP_DATA. */
		vertexLightingData_struct* vertexLighting;/**< Live when ::type is @ref VERTEXLIGHT_DATA. */
	}data;
	u16 size; /**< Number of rectangles the data covers. */
}lightingData_struct;

/** @brief Clears a lighting record without allocating anything. */
void initLightData(lightingData_struct* ld);

/** @brief Releases whichever form of lighting data a record holds. */
void freeLightData(lightingData_struct* ld);

/**
 * @brief Sets a record up for lightmaps.
 * @param ld record to initialise.
 * @param n  number of rectangles to cover.
 */
void initLightDataLM(lightingData_struct* ld, u16 n);

/**
 * @brief Sets a record up for vertex lighting.
 * @param ld record to initialise.
 * @param n  number of rectangles to cover.
 */
void initLightDataVL(lightingData_struct* ld, u16 n);

#endif
