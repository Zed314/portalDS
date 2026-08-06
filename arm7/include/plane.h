/**
 * @file plane.h
 * @brief Infinite planes and box-versus-plane contacts.
 *
 * A leftover from the early days of the physics engine, when the world was
 * bounded by a handful of hand-placed planes rather than the rectangle soup in
 * @ref AAR.h. Nothing in the shipped game creates planes any more, but
 * @ref PLANECOLLISION contacts still flow through the impulse solver and the
 * code is a useful, much simpler reference for how contact generation works.
 */

#ifndef PLANE_H
#define PLANE_H

/**
 * @brief A plane in @c Ax+By+Cz+D=0 form, kept normalised.
 *
 * Because the normal @c (A,B,C) is unit length, @ref evaluatePlanePoint
 * returns a true signed distance rather than a scaled one, which is what lets
 * the contact code use it directly as a penetration depth.
 */
typedef struct
{
	int32 A, B, C, D; /**< Plane coefficients; @c (A,B,C) is a unit normal. */
	vect3D point;     /**< The point on the plane closest to the origin; cached for segment intersection. */
}plane_struct;

/**
 * @brief Signed distance from a point to a plane.
 *
 * Positive in front of the plane (the side the normal points to), negative
 * behind it.
 *
 * @param p plane to evaluate against; returns 0 if NULL.
 * @param v point in world space.
 * @return the signed distance, in f32.
 */
static inline int32 evaluatePlanePoint(plane_struct* p, vect3D v)
{
	if(!p)return 0;
	//return mulf32(p->A,v.x)+mulf32(p->B,v.y)+mulf32(p->C,v.z)+p->D;
	int32_t t= ((int64_t)p->A*v.x+(int64_t)p->B*v.y+(int64_t)p->C*v.z)>>12;
    return t+p->D;
}

//extern plane_struct testPlane;

/**
 * @brief Initialises a plane from raw coefficients.
 *
 * The coefficients are divided through by the length of @c (A,B,C), so callers
 * can pass any scaling they like.
 *
 * @param pl plane to fill; ignored if NULL.
 * @param A  x coefficient.
 * @param B  y coefficient.
 * @param C  z coefficient.
 * @param D  constant term.
 */
void initPlane(plane_struct* pl, int32 A, int32 B, int32 C, int32 D);

/**
 * @brief Intersects a ray with a plane.
 *
 * @param pl plane to hit; returns @p o unchanged if NULL.
 * @param o  ray origin.
 * @param v  ray direction.
 * @return the intersection point. Undefined if the ray is parallel to the plane.
 */
vect3D intersectSegmentPlane(plane_struct* pl, vect3D o, vect3D v);

/**
 * @brief Generates contacts between a plane and a rigid body.
 *
 * Tests all eight corners of the box and emits a @ref PLANECOLLISION contact
 * for each that has sunk to or below the plane.
 *
 * @warning Unlike @ref AARsOBBContacts, this *resets* the body's contact list
 *          rather than appending to it, so it must run before any other
 *          contact generation for that body.
 *
 * @param p plane to test against.
 * @param o body to test.
 */
void planeOBBContacts(plane_struct* p, OBB_struct* o);


#endif
