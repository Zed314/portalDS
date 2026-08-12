/**
 * @file platform.h
 * @brief Moving platforms, as simulated on the ARM7.
 *
 * A platform slides back and forth between two fixed points at a constant
 * speed. It is not a rigid body: nothing can push it, it just carries whatever
 * is standing on it. Collision is handled by a single upward-facing rectangle
 * (platform_struct::AAR) that is dragged along with the platform, which is why
 * a body only ever lands on top of one and never bumps into its side.
 *
 * The ARM9 owns the *visual* platform and the buttons that trigger it; it
 * sends @ref PI_ADDPLATFORM / @ref PI_TOGGLEPLATFORM and then reads the
 * position back out of the FIFO each frame.
 */

#ifndef PLATFORM7_H
#define PLATFORM7_H

/**
 * @brief A moving platform.
 */
typedef struct
{
	vect3D position; /**< Current centre in world space. */
	vect3D velocity; /**< Constant travel velocity; zeroed when the platform stops. */
	vect3D origin;      /**< One end of the run. */
	vect3D destination; /**< The other end of the run. */
	bool direction;  /**< Travel direction: true means origin -> destination. */ //true=orig->dest
	bool active;     /**< Whether the platform is currently moving. */
	bool backandforth; /**< If true the platform reverses at each end; if false it stops on arrival. */
	// u16 aarID;
	AAR_struct AAR;  /**< The upward-facing collision surface, moved along with the platform. */
	bool used;       /**< False when this slot is free. */
}platform_struct;

extern platform_struct platform[NUMPLATFORMS]; /**< Platform pool; index is the id used on the FIFO. */

/** @brief Marks every platform slot as free. */
void initPlatforms(void);

/**
 * @brief Teleports a platform to a position, dragging its collision rectangle along.
 * @param id  platform index; ignored if out of range.
 * @param pos new centre.
 */
void movePlatform(u8 id, vect3D pos);

/**
 * @brief Advances every live platform by one frame.
 *
 * A platform that has passed its target either reverses (if
 * platform_struct::backandforth) or stops and deactivates.
 */
void updatePlatforms(void);

/**
 * @brief Fills a platform slot and builds its collision rectangle.
 *
 * The platform starts parked at @p orig and inactive.
 *
 * @param id   slot index.
 * @param orig one end of the run.
 * @param dest the other end.
 * @param BAF  true for a platform that shuttles back and forth forever.
 */
void createPlatform(u16 id, vect3D orig, vect3D dest, bool BAF);

/**
 * @brief Collides a rigid body against every live platform.
 *
 * Called from @ref AARsOBBContacts, including for sleeping bodies - a sleeping
 * cube still has to be carried by a platform that starts moving under it.
 *
 * @param o body to test.
 * @param v the body's eight world-space corners, precomputed by the caller.
 */
void collideOBBPlatforms(OBB_struct* o, vect3D* v);

/**
 * @brief Starts or stops a platform.
 * @param id     platform index; ignored if out of range.
 * @param active true to start moving, false to freeze in place.
 */
void togglePlatform(u8 id, bool active);

#endif
