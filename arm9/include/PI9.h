/**
 * @file PI9.h
 * @brief Portal Interaction, ARM9 side - the game's view of the physics engine.
 *
 * The ARM7 owns the simulation (see @ref PI7.h); this header is the ARM9's
 * mirror of it. Each function here either sends a @ref message_type command
 * over the FIFO or reads back what the ARM7 has computed.
 *
 * @par The mirror is deliberately thin
 * The @ref OBB_struct declared here is *not* the one in arm7/include/OBB.h.
 * It carries only what the game needs to draw and reason about a box - a
 * position, an orientation and the model to render - and none of the dynamics
 * state. Trying to keep the two in sync would defeat the point of moving the
 * simulation off this CPU.
 *
 * @par Reading results back
 * @ref listenPI9 is called once per frame and drains the reply channels. Two
 * details are worth knowing:
 *  - the orientation arrives as six 16 bit values (two matrix columns);
 *    the third column is reconstructed here as their cross product, which
 *    saves a whole FIFO word per box per frame;
 *  - the same channel carries platform positions, distinguished by an index at
 *    or above @ref NUMOBJECTS.
 *
 * It is also where a box that has been dropped into an emancipation grid or
 * into sludge gets destroyed, since that is the moment its new position is
 * known.
 *
 * @see common/include/PIC.h for the wire protocol.
 */

#ifndef PI9_H
#define PI9_H

#include "math.h"
#include "../../common/include/PIC.h"

#define NUMOBJECTS (8) /**< Hard cap on simultaneous rigid bodies. Must match the ARM7's value. */

/**
 * @brief The game's view of a rigid body.
 *
 * Position and orientation are written by @ref listenPI9 from the ARM7's
 * results; everything else is owned by the ARM9.
 */
typedef struct
{
	int32 transformationMatrix[9]; /**< Orientation, row-major 3x3. The third column is reconstructed locally. */
	modelInstance_struct modelInstance; /**< Model and animation state used to draw the box. */
	vect3D position; /**< Centre in world space, as reported by the ARM7. */
	vect3D size;     /**< Half extents; sent to the ARM7 at creation and kept for picking. */
	void* spawner;   /**< Owning cubeDispenser_struct, if this box came out of a dispenser. */
	s16 groundID;    /**< Rectangle the box is resting on, or -1. Lets the game tell what it landed on. */
	int32 mass;      /**< Mass in f32; only used when re-creating the box on the ARM7. */
	s32 startAngle;  /**< Yaw the box was spawned at, so a reset can restore it. */
	bool inPortal;   /**< True while the box straddles a portal, which suppresses some interactions. */
	bool used;       /**< False when this slot is free. */
	u8 id;           /**< Slot index; this is the id used on the FIFO. */
}OBB_struct;

/**
 * @brief The game's view of a static collision rectangle.
 *
 * The ARM9 keeps these so the editor and the level loader can address them by
 * id; the actual collision geometry lives on the ARM7.
 */
typedef struct
{
	vect3D position; /**< Minimum corner. */
	vect3D size;     /**< Extent from ::position; flat along one axis. */
	bool touched;    /**< Set when something has collided with this rectangle; used by trigger logic. */
	bool used;       /**< False when this slot is free. */
	u16 id;          /**< Slot index; this is the id used on the FIFO. */
}AAR_struct;

extern AAR_struct aaRectangles[NUMAARS]; /**< Mirror of the ARM7's static rectangle pool. */

/** @brief Clears both mirrors and assigns every slot its id. Does not touch the ARM7. */
void initPI9(void);

/** @brief Sends @ref PI_START, resuming the simulation. */
void startPI(void);

/** @brief Sends @ref PI_PAUSE, freezing the simulation with its state intact. */
void pausePI(void);

/** @brief Sends @ref PI_RESETALL and clears the local mirrors. Used when loading a new room. */
void resetAllPI(void);

/**
 * @brief Reads back everything the ARM7 has produced since the last call.
 *
 * Updates box positions and orientations and platform positions, and destroys
 * any box that has just been emancipated or has fallen into sludge. Call once
 * per frame.
 */
void listenPI9(void);

/** @brief Advances the visual state of every box: model animation and portal straddling. */
void updateOBBs(void);

/**
 * @brief Sends @ref PI_APPLYFORCE - an impulse applied at a point on a box.
 * @param id  box slot index.
 * @param pos application point, relative to the box centre.
 * @param v   impulse vector.
 */
void applyForce(u8 id, vect3D pos, vect3D v);

/**
 * @brief Allocates a static collision rectangle and sends it to the ARM7.
 *
 * Call @ref makeGrid once after adding a batch of these.
 *
 * @param size   extent; must be flat along the normal's axis.
 * @param pos    minimum corner.
 * @param normal outward normal.
 * @return the new rectangle's id, or -1 if the pool is full.
 */
s16 createAAR(vect3D size, vect3D pos, vect3D normal);

/**
 * @brief Sends @ref PI_ADDPLATFORM, creating a moving platform.
 * @param id   platform index.
 * @param orig one end of the run.
 * @param dest the other end.
 * @param BAF  true for a platform that shuttles back and forth forever.
 */
void addPlatform(u8 id, vect3D orig, vect3D dest, bool BAF);

/** @brief Sends @ref PI_UPDATEPLATFORM, teleporting a platform to @p pos. */
void changePlatform(u8 id, vect3D pos);

/** @brief Sends @ref PI_RESETPORTALS, marking both portals unused on the ARM7. */
void resetPortalsPI(void);

/**
 * @brief Sends @ref PI_TOGGLEAAR, enabling or disabling a static rectangle.
 *
 * This is how doors and moving walls stop blocking things when they open.
 *
 * @param id rectangle id; ignored if negative.
 */
void toggleAAR(s16 id);

/** @brief Destroys a box: frees the local slot and sends @ref PI_KILLBOX. */
void killBox(OBB_struct* o);

/**
 * @brief Returns a box to a position with its original orientation and no velocity.
 *
 * Used by cube dispensers and when a chamber is restarted.
 *
 * @param o   box to reset.
 * @param pos position to place it at.
 */
void resetBox(OBB_struct* o, vect3D pos);

/**
 * @brief Allocates a box and sends @ref PI_ADDBOX to spawn it on the ARM7.
 *
 * @param pos   initial centre.
 * @param mass  mass in f32.
 * @param model model to draw it with.
 * @param angle initial yaw; the sine and cosine are computed here and sent, so
 *              the ARM7 never has to do trigonometry.
 * @return the new box, or NULL if the pool is full.
 */
OBB_struct* createBox(vect3D pos, int32 mass, md2Model_struct* model, s32 angle);

/**
 * @brief Computes a box's world-space axis aligned bounding box.
 * @param o box to bound.
 * @param s output: two vectors, the minimum corner and the extent.
 */
void getBoxAABB(OBB_struct* o, vect3D* s);

/**
 * @brief Tests whether an axis aligned box overlaps a static rectangle.
 * @param o1 box minimum corner.
 * @param s1 box extent.
 * @param o2 rectangle minimum corner.
 * @param sp rectangle extent.
 */
bool intersectAABBAAR(vect3D o1, vect3D s1, vect3D o2, vect3D sp);

/** @brief Tests whether a box currently straddles a portal's outline. */
bool intersectOBBPortal(portal_struct* p, OBB_struct* o);

/**
 * @brief Pushes any box straddling a portal clear of it.
 *
 * Called when a portal closes or moves, so a box is never left half way
 * through a portal that no longer exists.
 */
void ejectPortalOBBs(portal_struct* p);

/**
 * @brief Perpendicular distance from a point to an infinite line.
 * @param o point on the line.
 * @param u line direction; must be normalised.
 * @param p point to measure.
 */
int32 distanceLinePoint(vect3D o, vect3D u, vect3D p);

/**
 * @brief Finds the box a ray hits first - the portal gun's and gravity gun's picking test.
 * @param o ray origin.
 * @param u ray direction; must be normalised.
 * @param l maximum range.
 * @return the nearest box hit, or NULL.
 */
OBB_struct* collideRayBoxes(vect3D o, vect3D u, int32 l);

/** @brief Draws every live box. */
void drawOBBs(void);

/** @brief Debug draw of the static collision rectangles. */
void drawAARs(void);

/**
 * @brief Sends @ref PI_MAKEGRID, telling the ARM7 to rebuild its broadphase.
 *
 * Must be called after a batch of @ref createAAR calls, or nothing will
 * collide with the new geometry.
 */
void makeGrid(void);

/** @brief Sends @ref PI_SETVELOCITY, overwriting a box's linear velocity. */
void setVelocity(u8 id, vect3D v);

/** @brief Sends @ref PI_TOGGLEPLATFORM, starting or stopping a platform. */
void togglePlatform(u8 id, bool active);

/**
 * @brief Sends @ref PI_UPDATEPORTAL, moving a portal on the ARM7.
 *
 * Only the normal and the first tangent are sent; the ARM7 derives the second
 * with @ref computePortalPlane.
 *
 * @param id     portal index, 0 or 1.
 * @param pos    centre of the portal.
 * @param normal outward surface normal.
 * @param plane0 first tangent spanning the portal plane.
 */
void updatePortalPI(u8 id, vect3D pos, vect3D normal, vect3D plane0);

/**
 * @brief Multiplies a 3x3 rotation onto the hardware's current matrix.
 *
 * Expands the 3x3 into the 4x4 the geometry engine expects.
 *
 * @param m row-major 3x3 rotation.
 */
void multTMatrix(int32* m);

/** @brief Draws one box. */
void drawOBB(OBB_struct* o);
#endif
