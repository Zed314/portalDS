/**
 * @file portals.h
 * @brief Portals: placement, the view through them, and walking through them.
 *
 * There are exactly two portals, @ref portal1 (blue) and @ref portal2
 * (orange), and each points at the other. Everything else in this file exists
 * to support two illusions.
 *
 * @par Seeing through a portal
 * Each portal owns a @ref camera_struct positioned where the player would be
 * standing if they were on the far side - that is what
 * @ref updatePortalCamera computes, warping both the position and the
 * orientation through @ref warpVector. The room is then rendered again through
 * that camera into portal_struct::viewPoint, and the result is used as a
 * texture on the portal's own quad. So a frame renders the room up to three
 * times: once per open portal, then once for the real view.
 *
 * The portal's visible shape is not a rectangle. portal_struct::polygon and
 * portal_struct::outline hold the ellipse-ish outline, clipped against the
 * frustum on the CPU, so the captured view is masked to the right shape.
 *
 * @par Walking through a portal
 * checkPortalPlayerWarp() watches which side of the portal plane the player is
 * on, using portal_struct::oldZ to remember the previous frame. When the sign
 * flips while the player is inside the outline, warpPlayer() moves the camera
 * and its velocity through to the far portal. Rigid bodies are teleported
 * independently on the ARM7 - see @ref updateOBBPortals.
 *
 * @par Placement rules
 * A portal shot only sticks if it lands somewhere legal, which is what
 * @ref isPortalOnWall and @ref portalToPortalIntersection decide: the portal
 * must fit entirely on one portalable surface, and it must not overlap the
 * other portal.
 *
 * @see common/include/PIC.h for the transport maths, and PI9.h for keeping the
 *      ARM7's copy of the portals in step.
 */

#ifndef __PORTALS9__
#define __PORTALS9__

#define PORTALMARGIN (32) /**< Clearance a portal needs from the edge of a surface, in f32. */

/**
 * @brief One portal.
 *
 * Much larger than the ARM7's portal_struct, because this one carries all the
 * rendering state as well as the geometry.
 */
typedef struct portal_struct
{
	camera_struct camera;    /**< Camera used to render what is seen through this portal. */
	vect3D position;         /**< Centre of the portal in world space. */
	u16 viewPoint[256*192];  /**< Full screen capture of the view through this portal, used as its texture. */
	u16 color;               /**< Portal colour: blue or orange. */
	u16 innerOutlineColor;   /**< Inner ring colour. */
	u16 outlineColor;        /**< Outer ring colour. */
	u16 animCNT;             /**< Counter driving the opening animation; reset whenever the portal moves. */
	vect3D normal;           /**< Outward surface normal. */
	vect3D plane[2];         /**< The two tangents spanning the portal's plane. */
	int32 oldZ;              /**< Player's signed distance to the plane last frame; the sign flip is what triggers a teleport. */
	u32* displayList;        /**< Pre-built display list of the room as seen from this portal. */
	polygon_struct *outline;             /**< Portal rim, clipped to the frustum. */
	polygon_struct *unprojectedOutline;  /**< The rim before projection, kept for clipping. */
	polygon_struct *polygon;             /**< Portal opening, clipped to the frustum. */
	polygon_struct *unprojectedPolygon;  /**< The opening before projection. */
	struct portal_struct* targetPortal;  /**< The other portal; never NULL. */
	bool used;               /**< False until this portal has been shot. */
}portal_struct;

extern portal_struct portal1, portal2; /**< The blue and orange portals. */
extern portal_struct* currentPortal;   /**< The portal the next shot will place. */

/** @brief Creates both portals, links them and allocates the polygon pool. */
void initPortals(void);

/** @brief Releases both portals' display lists. */
void freePortals(void);

/** @brief Closes both portals, here and on the ARM7. */
void resetPortals(void);

/**
 * @brief Advances both portals by one frame.
 *
 * Checks whether the player has crossed either portal, then updates each
 * portal's animation and camera.
 */
void updatePortals(void);

/**
 * @brief Sets up one portal's fixed properties.
 * @param p      portal to initialise.
 * @param pos    initial position.
 * @param normal initial surface normal.
 * @param color  false for blue, true for orange.
 */
void initPortal(portal_struct* p, vect3D pos, vect3D normal, bool color);

/** @brief Draws a portal: its captured view, masked by its outline, plus the coloured rim. */
void drawPortal(portal_struct* p);

/**
 * @brief Places a portal on a surface.
 *
 * With @p actualMove set this is the full move: the ARM7 is told, the cached
 * display list is thrown away and rebuilt for the new viewpoint, and the
 * portal is marked used. With it clear only the geometry is updated, which is
 * what the placement tests use to try out a candidate position.
 *
 * @param p          portal to move.
 * @param pos        new centre.
 * @param normal     new surface normal.
 * @param plane0     new first tangent; the second is derived.
 * @param actualMove false to update geometry only.
 */
void movePortal(portal_struct* p, vect3D pos, vect3D normal, vect3D plane0, bool actualMove);

/**
 * @brief Positions a portal's camera as if it were the viewer on the far side.
 *
 * Warps both the position and each column of the orientation matrix through to
 * the target portal. This is what makes the view through a portal line up with
 * the room behind it.
 *
 * @param p portal to update.
 * @param c the real camera to derive from, or NULL for the player's.
 */
void updatePortalCamera(portal_struct* p, camera_struct* c);

/** @brief Renders the room from a portal's camera into its view texture. */
void drawPortalRoom(portal_struct* p);

/**
 * @brief Tests a ray against a portal, for shooting through an open one.
 * @param r     room being tested.
 * @param rec   rectangle the ray hit.
 * @param p     portal to test.
 * @param point in/out: the hit point, moved through the portal if it passes.
 */
void collidePortal(room_struct* r, rectangle_struct* rec, portal_struct* p, vect3D* point);

/**
 * @brief Returns the tint to apply at a position, for the glow near a portal.
 * @param o position in world space.
 * @return a 15 bit colour.
 */
u16 getCurrentPortalColor(vect3D o);

/**
 * @brief Tests whether a portal fits on a legal surface, optionally nudging it.
 *
 * A portal must lie entirely within one portalable rectangle. With @p fix set,
 * a portal that overhangs is slid back inside rather than rejected - which is
 * what makes shooting near an edge feel forgiving.
 *
 * @param r   room to test against.
 * @param p   portal to place.
 * @param fix true to nudge the portal into a legal position.
 * @return true if the portal ended up somewhere legal.
 */
bool isPortalOnWall(room_struct* r, portal_struct* p, bool fix);

/**
 * Determines if the two portals do not collide.
 *
 * \param[in] p one portal
 *
 * \param[in] p2 the other portal
 *
 * \return true if the two portal do not collides, false otherwise
 *
 * \warning Does not work if two portals are on the ceiling.
*/
bool portalToPortalIntersection(const  portal_struct* p, const portal_struct* p2);

/**
 * @brief Tests whether a point lies within a portal's outline.
 *
 * Also hands back the point expressed in the portal's own frame, which the
 * teleport code needs anyway.
 *
 * @param p portal to test against.
 * @param o point in world space.
 * @param v out: the point relative to the portal centre. May be NULL.
 * @param x out: coordinate along the first tangent.
 * @param y out: coordinate along the second tangent.
 * @param z out: signed distance along the normal.
 * @return true if the point is inside the outline.
 */
bool isPointInPortal(portal_struct* p, vect3D o, vect3D *v, int32* x, int32* y, int32* z);

#endif
