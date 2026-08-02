/**
 * @file lights.h
 * @brief Point lights.
 *
 * Lights are placed in the editor and are used at two quite different times:
 *
 *  - **offline**, when the editor bakes them into lightmaps or per-vertex
 *    colours (@ref generateLightmaps, @ref generateVertexLighting). This is
 *    where all the world lighting comes from - nothing about the room is lit
 *    at runtime;
 *  - **at runtime**, only for moving objects. Those cannot use baked lighting,
 *    so @ref setupObjectLighting picks the three nearest lights and feeds them
 *    to the hardware's lighting unit - which happens to accept exactly four,
 *    with the fourth reserved for ambient.
 *
 * The three-light limit is why @ref gridCell_struct caches
 * @ref getClosestLights per cell rather than searching every frame.
 */

#ifndef LIGHTS_H
#define LIGHTS_H

#define NUMLIGHTS 64 /**< Maximum number of lights in a room. */

/** @brief A point light. */
typedef struct
{
	vect3D position; /**< Position in tile coordinates. */
	int32 intensity; /**< Brightness; also determines its effective radius. */
	bool used;       /**< False when this slot is free. */
}light_struct;

extern light_struct lights[NUMLIGHTS]; /**< The light pool. */

/** @brief Marks every light slot as free. */
void initLights(void);

/**
 * @brief Allocates a light.
 * @param pos       position in tile coordinates.
 * @param intensity brightness.
 * @return the new light, or NULL if the pool is full.
 */
light_struct* createLight(vect3D pos, int32 intensity);

/**
 * @brief Finds the three lights nearest a position.
 *
 * Three because that is what the hardware lighting unit has room for once
 * ambient is accounted for.
 *
 * @param tilepos position in tile coordinates.
 * @param ll1     out: nearest light.
 * @param ll2     out: second nearest.
 * @param ll3     out: third nearest.
 * @param dd1     out: distance to the nearest.
 * @param dd2     out: distance to the second.
 * @param dd3     out: distance to the third.
 */
void getClosestLights(vect3D tilepos, light_struct** ll1, light_struct** ll2, light_struct** ll3, int32* dd1, int32* dd2, int32* dd3);

#endif
