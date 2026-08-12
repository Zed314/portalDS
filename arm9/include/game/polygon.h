/**
 * @file polygon.h
 * @brief CPU-side polygon clipping, used to build portal shapes.
 *
 * The DS clips polygons in hardware, but only against the view frustum, and it
 * gives you nothing back. Portals need more than that: the outline has to be
 * clipped, projected and then walked so the captured view can be masked to the
 * right shape, and the shape is an ellipse rather than a quad.
 *
 * So polygons here are singly linked vertex rings, built by
 * @ref createEllipse / @ref createQuad and clipped by
 * @ref clipPolygonFrustum. Each @ref polygon_struct is one vertex; the
 * polygon is the ring you get by following ::next.
 *
 * @par The pool
 * Clipping allocates and frees vertices constantly, several times per frame,
 * so they come from a fixed pool of @ref POLYPOOLSIZE entries rather than the
 * heap. Call @ref initPolygonPool once at startup and @ref freePolygon on
 * every polygon you finish with - a leak here shows up as portals silently
 * failing to draw once the pool runs dry.
 *
 * @see portals.h, which is the only real consumer.
 */

#ifndef __POLYGON9__
#define __POLYGON9__

#define POLYPOOLSIZE 512 /**< Number of vertices in the shared pool. */

/**
 * @brief One vertex of a polygon; the polygon is the ring formed by ::next.
 */
typedef struct polygon_struct
{
	vect3D v;    /**< Position. */
	vect3D t;    /**< Texture coordinate. */
	vect3D dir;  /**< Direction to the next vertex, cached during clipping. */
	int32 val;   /**< Signed distance to the current clip plane. */
	int32 dist;  /**< Distance along the edge to the clip intersection. */
	struct polygon_struct* next; /**< Next vertex; the last one points back at the first. */
}polygon_struct;

/** @brief Empties the vertex pool. Call once at startup. */
void initPolygonPool(void);

/** @brief Draws a polygon as a triangle fan. */
void drawPolygon(polygon_struct* p);

/** @brief Returns a whole polygon's vertices to the pool and NULLs the caller's pointer. */
void freePolygon(polygon_struct** p);

/** @brief Allocates a single vertex from the pool. */
polygon_struct* createPolygon(vect3D v);

/** @brief Projects a world-space point to screen coordinates through a camera. */
vect3D projectPoint(camera_struct* c, vect3D p);

/**
 * @brief Builds an ellipse - the portal opening.
 * @param po centre.
 * @param v1 first semi-axis.
 * @param v2 second semi-axis.
 * @param n  number of segments to approximate it with.
 */
polygon_struct* createEllipse(vect3D po, vect3D v1, vect3D v2, int n);

/** @brief Clips a polygon against one plane, returning the part in front of it. */
polygon_struct* clipPolygonPlane(plane_struct* pl, polygon_struct* p);

/** @brief Builds a quad from four corners. */
polygon_struct* createQuad(vect3D v1, vect3D v2, vect3D v3, vect3D v4);

/** @brief Clips a polygon against all six frustum planes in turn. */
polygon_struct* clipPolygonFrustum(frustum_struct* f, polygon_struct* p);

/** @brief Intersects a ray with a plane. */
vect3D intersectSegmentPlane(plane_struct* pl, vect3D o, vect3D v);

/**
 * @brief Clips one edge against a plane, appending the result to a polygon.
 * @param pl  clip plane.
 * @param o   in/out: polygon being built.
 * @param pp1 first endpoint.
 * @param pp2 second endpoint.
 */
void clipSegmentPlane(plane_struct* pl, polygon_struct** o, polygon_struct* pp1, polygon_struct* pp2);

/**
 * @brief Clips a polygon against the camera frustum and projects it to screen.
 *
 * It used to also generate texture coordinates mapping the portal's captured
 * screen image onto its outline, fed by the portal's own axes; that block is
 * commented out in the implementation and its parameters are gone with it.
 *
 * @param c  camera to project through.
 * @param p  in/out: polygon to project.
 */
void projectPolygon(camera_struct* c, polygon_struct** p);

/**
 * @brief Builds a ring between two concentric ellipses - the portal's coloured rim.
 * @param po   centre.
 * @param v1_1 first semi-axis of the inner ellipse.
 * @param v2_1 second semi-axis of the inner ellipse.
 * @param v1_2 first semi-axis of the outer ellipse.
 * @param v2_2 second semi-axis of the outer ellipse.
 * @param norm plane normal.
 * @param n    number of segments.
 */
polygon_struct* createEllipseOutline(vect3D po, vect3D v1_1, vect3D v2_1, vect3D v1_2, vect3D v2_2, vect3D norm, int n);

/**
 * @brief Draws a ring built by @ref createEllipseOutline as a triangle strip.
 * @param p    ring to draw.
 * @param col1 inner edge colour.
 * @param col2 outer edge colour.
 */
void drawPolygonStrip(polygon_struct* p, u16 col1, u16 col2);

#endif
