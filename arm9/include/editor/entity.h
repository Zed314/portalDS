/**
 * @file entity.h
 * @brief Placeable objects in the editor, described by a table.
 *
 * Everything you can place in a level - turrets, cubes, buttons, doors, lights,
 * energy devices - is one @ref entity_struct pointing at a shared
 * @ref entityType_struct. The type carries the model, which faces the object
 * may be mounted on, which context menu it gets, and up to four optional
 * behaviour hooks.
 *
 * That table is the point. Adding a new entity to the editor means adding a
 * table entry and, if it needs unusual behaviour, writing a hook - not
 * threading a new case through the placement, drawing and movement code. The
 * hooks are:
 *  - entityType_struct::specialInit - extra setup on creation;
 *  - entityType_struct::specialDraw - draw something other than the model,
 *    which is how the emancipation grid draws its beam;
 *  - entityType_struct::specialMove - custom response to being dragged;
 *  - entityType_struct::specialMoveCheck - veto a placement that would be
 *    illegal for this type.
 *
 * @par Entities are mounted on faces
 * An entity does not float in space: it is attached to a
 * @ref blockFace_struct, and entityType_struct::possibleDirections restricts
 * which orientations are legal (a floor button cannot go on a ceiling). Carving
 * away the block underneath takes the entity with it - see
 * @ref getEntityBlockFacesRange.
 *
 * @par Targets
 * entity_struct::target is the trigger wiring - which door this button opens.
 * It is a pointer here, and is converted to and from an index
 * (entity_struct::writeID) when the level is saved and loaded.
 */

#ifndef ENTITY_H
#define ENTITY_H

#include "editor/blocks.h"
#include "editor/contextbuttons.h"
#define NUMENTITYTYPES (15) /**< Number of entity types in the table. */
#define NUMENTITIES (64)    /**< Maximum number of entities in a level. */

/** @brief Bit mask of face directions an entity type may be mounted on. */
enum directionMask_type
{
	pX_mask = 1,    /**< May be mounted facing +X. */
	mX_mask = 1<<1, /**< May be mounted facing -X. */
	pY_mask = 1<<2, /**< May be mounted facing +Y - that is, on a floor. */
	mY_mask = 1<<3, /**< May be mounted facing -Y - that is, on a ceiling. */
	pZ_mask = 1<<4, /**< May be mounted facing +Z. */
	mZ_mask = 1<<5  /**< May be mounted facing -Z. */
};

struct entity_struct;

typedef void(*entityFunction)(struct entity_struct*);     /**< @brief Hook run when an entity is created. */
typedef void(*entityDrawFunction)(struct entity_struct*); /**< @brief Hook replacing the default model draw. */
typedef void(*entityMoveFunction)(struct entity_struct*, vect3D, u8, bool); /**< @brief Hook run when an entity is dragged. */
typedef bool(*entityMoveCheckFunction)(struct entity_struct*, vect3D, u8 dir); /**< @brief Hook vetoing an illegal placement. */

/**
 * @brief The shared description of one kind of entity.
 *
 * One of these per entity type, held in a table in entity.c.
 */
typedef struct
{
	const char* modelName;   /**< Model file to load. */
	const char* textureName; /**< Texture file to load. */
	u8 possibleDirections;   /**< Which faces this type may be mounted on; see @ref directionMask_type. */
	contextButton_struct* contextButtonsArray; /**< Context menu shown when one is selected. */
	u8 numButtons;           /**< Number of entries in ::contextButtonsArray. */
	entityFunction specialInit;           /**< Optional extra setup on creation. */
	entityDrawFunction specialDraw;       /**< Optional replacement for the default draw. */
	entityMoveFunction specialMove;       /**< Optional custom drag behaviour. */
	entityMoveCheckFunction specialMoveCheck; /**< Optional placement veto. */
	bool removeTarget;       /**< Whether removing this entity should clear other entities' targets pointing at it. */
	bool rotate;             /**< Whether this type can be rotated in place. */
	md2Model_struct model;   /**< The loaded model, shared by every instance. */
	u8 id;                   /**< Type index; this is what gets written to the level file. */
}entityType_struct;

/** @brief One placed object. */
typedef struct entity_struct
{
	u8 direction;   /**< Which way the mounting face points. */
	u8 orientation; /**< Rotation about that face's normal. */
	vect3D position;/**< Position in block coordinates. */
	entityType_struct* type; /**< What kind of entity this is. */
	blockFace_struct* blockFace; /**< The face it is mounted on. */
	struct entity_struct* target;/**< What this entity triggers, or NULL. */
	u8 writeID;     /**< Index used to serialise ::target; only valid while saving or loading. */
	bool used;      /**< False when this slot is free. */
	bool placed;    /**< False while the entity is still being dragged into position. */
}entity_struct;

extern entity_struct entity[NUMENTITIES]; /**< The entity pool. */

/**
 * @name Context menus, one per entity type that has options.
 * @{
 */
extern contextButton_struct ballLauncherButtonArray[];
extern contextButton_struct ballCatcherButtonArray[];
extern contextButton_struct button1ButtonArray[];
extern contextButton_struct button2ButtonArray[];
extern contextButton_struct turretButtonArray[];
extern contextButton_struct cubeButtonArray[];
extern contextButton_struct gridButtonArray[];
extern contextButton_struct platformButtonArray[];
extern contextButton_struct doorButtonArray[];
extern contextButton_struct lightButtonArray[];
/** @} */

/** @brief Clears the entity pool and loads every type's model. */
void initEntities(void);

/** @brief Releases every type's model. */
void freeEntities(void);

/** @brief Removes every placed entity, leaving the models loaded. */
void removeEntities(void);

/**
 * @brief Places an entity.
 * @param pos    position in block coordinates.
 * @param type   entity type index.
 * @param placed false to create it in the "being dragged" state.
 * @return the new entity, or NULL if the pool is full.
 */
entity_struct* createEntity(vect3D pos, u8 type, bool placed);

/**
 * @brief Finds the entity a ray hits - the stylus picking test.
 * @param o  ray origin.
 * @param v  ray direction.
 * @param p1 out: hit point.
 * @param p2 out: surface normal at the hit.
 * @param d  in/out: maximum distance on entry, distance to the hit on exit.
 * @return the entity hit, or NULL.
 */
entity_struct* collideLineEntities(vect3D o, vect3D v, vect3D p1, vect3D p2, int32* d);

/** @brief Finds the block face an entity is mounted on within a list. */
blockFace_struct* getEntityBlockFace(entity_struct* e, blockFace_struct* l);

/** @brief Re-resolves every entity's mounting face against a face list. */
void getEntityBlockFaces(blockFace_struct* l);

/** @brief Changes an entity's type in place, keeping its position and target. */
void changeEntityType(entity_struct* e, u8 type);

/**
 * @brief Moves an entity onto a block face, if that face is legal for its type.
 * @return true if the move was allowed.
 */
bool moveEntityToBlockFace(entity_struct* e, blockFace_struct* bf);

/** @brief Shifts every entity within a box by an offset. Used when dragging a region. */
void moveEntitiesRange(vect3D o, vect3D s, vect3D u);

/**
 * @brief Re-resolves the mounting faces of entities within a box.
 * @param l      current block face list.
 * @param o      minimum corner of the box, in block coordinates.
 * @param s      extent of the box.
 * @param delete true to remove entities whose face no longer exists - which is
 *               what happens when you carve away the block one was standing on.
 */
void getEntityBlockFacesRange(blockFace_struct* l, vect3D o, vect3D s, bool delete);

/** @brief Creates a @ref light_struct for every light entity, ready for the lighting bake. */
void generateLightsFromEntities(void);

/** @brief Draws every entity, honouring each type's optional draw hook. */
void drawEntities(void);

/** @brief Removes one entity and clears any targets pointing at it. */
void removeEntity(entity_struct* e);

/** @brief Returns the length of an emancipation grid entity, measured to the wall it meets. */
int32 getGridLength(entity_struct* e);

/** @brief Writes one block, ignoring out-of-range coordinates. */
void setBlock(BLOCK_TYPE* ba, u8 x, u8 y, u8 z, BLOCK_TYPE v);
#endif
