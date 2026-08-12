/**
 * @file PI7.h
 * @brief Portal Interaction, ARM7 side - the FIFO front end of the physics engine.
 *
 * The ARM7 owns the authoritative physics state. It spends its life in
 * @ref listenPI7, draining commands the ARM9 has queued, stepping the
 * simulation, and shipping the results back with @ref sendDataPI7.
 *
 * The wire protocol itself lives in @ref PIC.h, which this header pulls in
 * *after* declaring @ref portal_struct - PIC.h's inline helpers need the type
 * to already exist, and the ARM9 declares its own, larger version of the same
 * struct in game/portals.h.
 *
 * @see OBB.h for the rigid bodies, AAR.h for the static world, PI9.h for the
 *      ARM9 counterpart.
 */

#ifndef PI7_H
#define PI7_H

/**
 * @brief What the ARM7 knows about the player.
 *
 * Only the position is mirrored: the player is not simulated as a rigid body,
 * but bodies need to know where they are so they can be woken up (and so the
 * broadphase can prioritise what is near the camera).
 */
typedef struct
{
	vect3D position; /**< Player position in world space, updated by @ref PI_UPDATEPLAYER. */
}player_struct;

/**
 * @brief A portal, as the physics engine sees it.
 *
 * Much smaller than the ARM9's portal_struct: no rendering state, just the
 * plane and the collision geometry needed to let bodies pass through.
 *
 * @note @c plane[0] arrives over the FIFO; @c plane[1] is derived locally by
 *       @ref computePortalPlane.
 */
typedef struct portal_struct
{
	vect3D position; /**< Centre of the portal in world space. */
	vect3D normal;   /**< Outward facing surface normal. */
	vect3D plane[2]; /**< The two tangents spanning the portal's plane. */
	AAR_struct guideAAR[4]; /**< Funnel of invisible rectangles keeping bodies off the portal rim. */
	struct portal_struct* targetPortal; /**< The portal bodies come out of; never NULL in practice. */
	bool used;       /**< False until the portal has been shot at least once. */
}portal_struct;

#include "../../common/include/PIC.h"

extern player_struct player;    /**< The single player instance. */
extern portal_struct portal[2]; /**< Blue portal is index 0, orange is index 1. */

/**
 * @brief Brings the physics engine up from cold.
 *
 * Clears the rigid bodies, static rectangles and platforms, parks the player at
 * the origin, links the two portals to each other and leaves the simulation
 * paused until a @ref PI_START arrives.
 */
void initPI7(void);

/**
 * @brief Reports whether the simulation is currently running.
 * @return true between @ref PI_START and the next @ref PI_PAUSE / @ref PI_STOP.
 */
bool getPI7Status(void);

/**
 * @brief Drains and executes every command waiting in the FIFO.
 *
 * Decodes the header word, then blocks on the argument words - the ARM9 always
 * writes a complete command, so the busy waits inside are bounded. Unknown
 * opcodes cause the whole queue to be flushed, on the assumption that the
 * stream has desynchronised and nothing after it can be trusted.
 */
void listenPI7(void);

/**
 * @brief Ships the simulation results back to the ARM9.
 *
 * Sends position and a compressed orientation for every awake body, then the
 * position of every live platform. Sleeping bodies are skipped entirely, which
 * is the main reason the FIFO does not saturate in a busy room.
 */
void sendDataPI7(void);

#endif
