/**
 * @file player.h
 * @brief The player: movement, the portal gun, and taking damage.
 *
 * There is exactly one player, reached through @ref getPlayer. It is a sphere
 * (see @ref physics.h) carried around by @ref playerCamera, not an ARM7 rigid
 * body - see the note in physics.h for why.
 *
 * @par The gun does three jobs
 * @ref shootPlayerGun takes a @p mode bit mask, and the same trigger does
 * whichever of these applies to whatever it hit:
 *  - bit 0: activate a switch or button;
 *  - bit 1: grab a cube. The held cube's id goes in @ref gravityGunTarget, and
 *    while the trigger stays down its velocity is driven towards a point in
 *    front of the camera each frame - that is the whole gravity gun;
 *  - bit 2: place a portal, if the surface is portalable and not covered by an
 *    emancipation grid.
 *
 * @par Portals and the player
 * player_struct::inPortal and ::oldInPortal track whether the player is
 * straddling a portal, which suppresses collision against the wall the portal
 * is cut into. The crossing itself is detected in portals.c.
 */

#ifndef __PLAYER9__
#define __PLAYER9__

#include "game/sfx.h"

#define PLAYERRADIUS (256) /**< Collision sphere radius, in f32. */
#define ERRORMARGIN (2)    /**< Slack allowed in collision tests, to absorb fixed point error. */
#define SQPLAYERRADIUS ((PLAYERRADIUS*PLAYERRADIUS)>>12) /**< Radius squared, in f32. */

#define PLAYERGROUNDSPEED (inttof32(3)>>8) /**< Acceleration per frame while on the ground. */

/**
 * @name The refused shot wobble
 *
 * Firing plays the same animation whether or not a portal appears, so a shot
 * that goes nowhere - a wall that takes no portals, a spot the portal will not
 * fit, or one already covered by the other portal - used to look exactly like
 * one that worked. These shake the gun briefly instead. See @ref renderGun.
 * @{
 */
#define GUNREFUSEDFRAMES (18)  /**< How long the shake lasts, in frames. */
#define GUNREFUSEDANGLE (700)  /**< Widest swing, as a binary angle: roughly eight degrees. */
#define GUNREFUSEDSWINGS (2)   /**< Complete left-right swings over those frames. */
/** @} */
#define PLAYERAIRSPEED (inttof32(1)>>9)    /**< Acceleration per frame while airborne - deliberately much lower. */

/**
 * @brief The player.
 */
typedef struct
{
	vect3D relativePosition;     /**< Position within the current room, in tile coordinates. */
	vect3D relativePositionReal; /**< The same position unrounded, in world units. */
	vect3D tempAngle;            /**< Look angles accumulated from input this frame, before being applied. */
	s32 walkCnt;                 /**< Phase counter driving the head bob and footstep sounds. */
	room_struct* currentRoom;    /**< Room the player is in. */
	physicsObject_struct* object;/**< Collision sphere; shared with the camera. */
	modelInstance_struct modelInstance;      /**< The portal gun model, drawn in first person. */
	modelInstance_struct playerModelInstance;/**< The player's own body, visible through portals. */
	bool currentPortal;          /**< Which portal the next shot places: false blue, true orange. */
	bool inPortal;               /**< True while the player straddles a portal. */
	bool oldInPortal;            /**< ::inPortal last frame; a change is what triggers the teleport. */
	s16 life;                    /**< Health; reaching zero restarts the chamber. */
	s16 refusedCNT;              /**< Counts the refused-shot shake down to zero; see @ref GUNREFUSEDFRAMES. */
}player_struct;

extern s16 gravityGunTarget;   /**< Id of the box currently held by the gravity gun, or -1. */
extern bool idle;              /**< True when the player has not moved recently; drives the idle animation. */
extern SFX_struct *gunSFX1, *gunSFX2; /**< Firing sounds for the two portal colours. */
extern SFX_struct *gunRefusedSFX;     /**< Played instead of nothing when a shot places no portal. */
extern bool currentPortalColor; /**< Colour of the portal the next shot places. */ //true=orange

/** @brief Returns the room the player is currently in. */
room_struct* getCurrentRoom(void);

/**
 * @brief Creates the player, loads the gun and body models and places them.
 * @param p player to initialise, or NULL for the global one.
 */
void initPlayer(player_struct* p);

/** @brief Reads input and turns it into movement and look angles. */
void playerControls(player_struct* p);

/** @brief Advances the player: movement, collision, animation, gravity gun and portal crossing. */
void updatePlayer(player_struct* p);

/** @brief Draws the portal gun in first person, in front of everything else. */
void renderGun(player_struct*);

/** @brief Returns the single player instance. */
player_struct* getPlayer(void);

/**
 * @brief Fires the gun.
 *
 * Casts a ray from the camera and acts on whatever it hits, according to
 * @p mode.
 *
 * @param p    player firing, or NULL for the global one.
 * @param R    true for the right trigger, false for the left. Selects the portal colour.
 * @param mode bit mask: 1 activate, 2 grab, 4 place a portal.
 */
/**
 * @brief Fires the gun: casts a ray from the camera and acts on what it hits.
 *
 * @param p    player firing; NULL for the local one.
 * @param R    which portal colour this shot places.
 * @param mode bitmask of what the shot may act on: 1 timed buttons, 2 the
 *             gravity gun, 4 portal placement.
 * @return true if the caller should play the firing sound. False means the
 *         shot was refused and the error sound has already been played in its
 *         place - see refuseShot() - so playing the firing sound as well would
 *         drown it out.
 */
bool shootPlayerGun(player_struct* p, bool R, u8 mode);

/** @brief Releases the player's models. */
void freePlayer(void);

/** @brief Draws the crosshair. */
void drawCrosshair(void);

/** @brief Applies one unit of damage and plays the hurt response. */
void damagePlayer(player_struct* p);

/**
 * @brief Applies damage from a direction, with knockback. Used by turret fire.
 * @param p      player being shot.
 * @param v      direction the shot came from.
 * @param damage amount of health to remove.
 */
void shootPlayer(player_struct* p, vect3D v, u8 damage);

/**
 * @brief Draws the player's body.
 *
 * Only visible through a portal - you never see it from your own camera.
 */
void drawPlayer(player_struct* p);

#endif
