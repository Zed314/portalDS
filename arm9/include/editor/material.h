/**
 * @file material.h
 * @brief Surface materials: what a wall looks like.
 *
 * A @ref material_struct is what gets attached to a @ref rectangle_struct, and
 * it is deliberately made of three parts rather than one texture. A surface
 * gets a different look depending on which way it faces - ::top for upward
 * faces, ::bottom for downward, ::side for the four vertical ones - so a single
 * material describes a complete building block: concrete floor, tiled walls,
 * grimy ceiling.
 *
 * The parts are @ref materialSlice_struct, which pair a texture with how it
 * should be mapped: ::factorX and ::factorY scale the texture coordinates so a
 * texture tiles at a consistent real-world size regardless of the surface's
 * dimensions, and ::align pins the mapping to the surface origin rather than
 * letting it float.
 *
 * Both tables are loaded from ini files at startup ("slices.ini" and
 * "materials.ini"), so the artwork can be changed without rebuilding.
 */

#ifndef __MATERIAL9__
#define __MATERIAL9__

#define NUMMATERIALS 256      /**< Maximum number of materials. */
#define NUMMATERIALSLICES 256 /**< Maximum number of texture slices. */

/** @brief One texture, plus how it should be mapped onto a surface. */
typedef struct
{
	mtlImg_struct* img; /**< The texture. */
	u16 id;             /**< Slice index; written to the level file. */
	s16 factorX;        /**< Horizontal texture coordinate scale, so tiling is consistent across surface sizes. */
	s16 factorY;        /**< Vertical texture coordinate scale. */
	bool align;         /**< True to pin the mapping to the surface origin rather than letting it float. */
	bool used;          /**< False when this slot is free. */
}materialSlice_struct;

/**
 * @brief A material: three slices, chosen by which way a surface faces.
 */
typedef struct
{
	materialSlice_struct* top;    /**< Used on upward-facing surfaces. */
	materialSlice_struct* side;   /**< Used on vertical surfaces. */
	materialSlice_struct* bottom; /**< Used on downward-facing surfaces. */
	u16 id;                       /**< Material index; written to the level file. */
	bool used;                    /**< False when this slot is free. */
}material_struct;


extern material_struct materials[NUMMATERIALS]; /**< The material table. */


/** @brief Returns a material's id, or 0 for NULL - the id written to the level file. */
static inline u16 getMaterialID(material_struct* m){if(m)return m->id;return 0;}

/** @brief Clears both tables. */
void initMaterials(void);

/** @brief Allocates a material slot. @return the new material, or NULL if the table is full. */
material_struct* createMaterial();

/** @brief Allocates a slice slot. @return the new slice, or NULL if the table is full. */
materialSlice_struct* createMaterialSlice();

/** @brief Loads one slice's texture. */
void loadMaterialSlice(materialSlice_struct* ms, char* filename);

/**
 * @brief Loads the slice table from an ini file.
 * @param filename normally "slices.ini".
 */
void loadMaterialSlices(char* filename);

/**
 * @brief Loads the material table from an ini file.
 *
 * Must run after @ref loadMaterialSlices, since materials reference slices by
 * name.
 *
 * @param filename normally "materials.ini".
 */
void loadMaterials(char* filename);

/** @brief Returns the material with a given id, or NULL. */
material_struct* getMaterial(u16 i);

/**
 * @brief Builds the material name list shown in the editor's material picker.
 * @param m  out: number of entries.
 * @param cl out: the corresponding material ids.
 * @return an array of names; release it with @ref freeMaterialList.
 */
char** getMaterialList(int* m, int** cl);

/** @brief Releases a list returned by @ref getMaterialList. */
void freeMaterialList(char** l);

#endif
