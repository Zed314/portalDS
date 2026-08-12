/**
 * @file platform.h
 * @brief Moving platforms, as the game sees them.
 *
 * The mirror of arm7/include/platform.h. The ARM7 owns the motion and the
 * collision; this struct exists so the game can draw the platform and wire it
 * up to whatever triggers it.
 *
 * Each frame @ref listenPI9 reads the authoritative position back off the FIFO
 * into platform_struct::position, and derives ::velocity from the change - the
 * ARM7 does not send velocity, but the game needs it to carry the player along
 * when they are standing on a moving platform.
 *
 * Platforms are targets, not triggers; see @ref activator.h.
 *
 * @warning The struct here and the one on the ARM7 share a name but not a
 *          layout. This one has no collision rectangle and no simulation state.
 */

#ifndef PLATFORM_H
#define PLATFORM_H

/** @brief A moving platform, game side. */
typedef struct
{
	vect3D position;    /**< Current position, read back from the ARM7 each frame. */
	vect3D velocity;    /**< Derived from the change in ::position; used to carry riders along. */
	vect3D origin;      /**< One end of the run. */
	vect3D destination; /**< The other end. */
	bool direction;     /**< Travel direction: true means origin -> destination. */ //true=orig->dest
	bool touched;       /**< True if something is riding the platform this frame. */
	bool oldTouched;    /**< ::touched last frame. */
	bool active;        /**< True while moving. */
	bool oldactive;     /**< ::active last frame; the change is what gets sent to the ARM7. */
	bool backandforth;  /**< True for a platform that shuttles back and forth forever. */
	bool used;          /**< False when this slot is free. */
	u8 id;              /**< Slot index; this is the id used on the FIFO. */
}platform_struct;

extern platform_struct platform[NUMPLATFORMS]; /**< The platform pool, mirroring the ARM7's. */

/** @brief Clears the platform pool and loads the platform model. */
void initPlatforms(void);

/** @brief Releases the platform model. */
void freePlatforms(void);

/** @brief Draws every platform at the position the ARM7 last reported. */
void drawPlatforms(void);

/** @brief Pushes start/stop changes to the ARM7 and updates the derived velocity. */
void updatePlatforms(void);

/**
 * @brief Creates a platform here and on the ARM7.
 * @param r    room to place it in.
 * @param orig one end of the run, in tile coordinates.
 * @param dest the other end.
 * @param BAF  true for a platform that shuttles back and forth forever.
 * @return the new platform, or NULL if the pool is full.
 */
platform_struct* createPlatform(room_struct* r, vect3D orig, vect3D dest, bool BAF);

#endif
