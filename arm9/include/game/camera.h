/**
 * @file camera.h
 * @brief Cameras, view frustums and the matrix stack.
 *
 * A @ref camera_struct is a position, an orientation and the three matrices
 * derived from them. There is more than one: @ref playerCamera is the one you
 * look through, but each portal owns a camera too (see @ref portals.h), placed
 * where you would be standing if you were on the far side. Rendering a portal
 * view is exactly "render the room again through that camera".
 *
 * @par Why the frustum is kept explicitly
 * The DS hardware clips for you, but it charges you for every polygon you
 * submit. With a room drawn up to three times per frame - once for the main
 * view and once per open portal - the only way to fit in the budget is to
 * reject geometry on the CPU first. @ref updateFrustum rebuilds the six planes
 * from the current view, and the room's spatial grid is tested against them
 * before anything is sent.
 *
 * @par Matrix conventions
 * 3x3 matrices are row-major in a flat 9 element array; 4x4 matrices follow the
 * DS geometry engine's column-major layout, because they are loaded into it
 * directly. Hence the two families of multiply, @ref multMatrix33 and
 * @ref multMatrix44, which are not interchangeable.
 *
 * @par World space versus view space
 * The renderer works in "view position" - the world scaled down so a room fits
 * comfortably inside the geometry engine's fixed point range.
 * @ref getViewPosition and @ref reverseViewPosition convert between the two.
 * Mixing them up is the classic bug in this file's callers.
 */

#ifndef __CAMERA9__
#define __CAMERA9__

/**
 * @brief A plane in @c Ax+By+Cz+D=0 form.
 *
 * Distinct from the ARM7's plane_struct: this one is only used for frustum
 * culling, never for collision.
 */
typedef struct
{
	int32 A, B, C, D; /**< Plane coefficients; @c (A,B,C) is a unit normal pointing inwards. */
	vect3D point;     /**< A point on the plane, cached for intersection tests. */
}plane_struct;

/**
 * @brief A view frustum: six bounding planes plus the parameters they came from.
 */
typedef struct
{
	plane_struct plane[6]; /**< Near, far, left, right, top and bottom, normals pointing inwards. */
	int32 nLeft, nTop, near; /**< Half width, half height and distance of the near plane. */
	int32 fLeft, fTop, far;  /**< Half width, half height and distance of the far plane. */
	int32 fovy, aspect;      /**< Vertical field of view and aspect ratio the frustum was built from. */
}frustum_struct;

/**
 * @brief A camera: where it is, where it is looking, and the matrices for both.
 */
typedef struct
{
	vect3D position;     /**< Position in world space. */
	vect3D viewPosition; /**< The same position in the renderer's scaled view space. */
	frustum_struct frustum; /**< Culling frustum, rebuilt by @ref updateFrustum. */
	vect3D space[3];     /**< The camera's own axis triple: right, up, forward. */
	int32 transformationMatrix[3*3]; /**< Orientation, row-major 3x3. Columns are ::space. */
	int32 projectionMatrix[4*4];     /**< Projection matrix, in geometry engine layout. */
	int32 viewMatrix[4*4];           /**< View matrix, in geometry engine layout. */
	bool lookAt;         /**< True when the orientation is driven by a look-at target rather than by ::space. */
	physicsObject_struct object; /**< Collision proxy, so the camera can be pushed out of walls. */
}camera_struct;

/**
 * @brief Signed distance from a point to a plane.
 *
 * Positive on the side the normal points to. Since frustum normals point
 * inwards, a point is inside the frustum when this is positive for all six
 * planes.
 */
static inline int32 evaluatePlanePoint(plane_struct* p, vect3D v)
{
	if(!p)return 0;
	return mulf32(p->A,v.x)+mulf32(p->B,v.y)+mulf32(p->C,v.z)+p->D;
}

extern camera_struct playerCamera; /**< The camera the player looks through. */

/**
 * @brief Resets a camera to the origin with an identity orientation.
 * @param c camera to initialise, or NULL for @ref playerCamera.
 *
 * @note Almost every function in this file takes NULL to mean
 *       @ref playerCamera, which is why so many call sites pass it.
 */
void initCamera(camera_struct* c);

/** @brief Loads a camera's projection matrix into the geometry engine. */
void projectCamera(camera_struct* c);

/** @brief Loads a camera's view matrix, so subsequent geometry is drawn from its point of view. */
void transformCamera(camera_struct* c);

/** @brief Undoes @ref transformCamera, returning to world space. */
void untransformCamera(camera_struct* c);

/**
 * @brief Moves a camera by a vector, subject to collision.
 * @param c camera to move, or NULL for the player's.
 * @param v displacement in world space.
 */
void moveCamera(camera_struct* c, vect3D v);

/**
 * @brief Rotates a camera.
 * @param c camera to rotate, or NULL for the player's.
 * @param a per-axis rotation angles.
 */
void rotateCamera(camera_struct* c, vect3D a);

/** @brief Moves a camera by a vector, ignoring collision. Used when following a teleport. */
void moveCameraImmediate(camera_struct* c, vect3D v);

/** @brief Returns a camera's world-space position. */
vect3D getCameraPosition(camera_struct* c);

/** @brief Teleports a camera to a position. */
void setCamera(camera_struct* c, vect3D v);

/** @brief Recomputes a camera's derived state: view position, matrices and frustum. */
void updateCamera(camera_struct* c);

// void updateCameraPreview(room_struct* r, camera_struct* c);

/** @brief Returns @ref playerCamera. */
camera_struct* getPlayerCamera(void);

/** @brief Returns the direction a camera is facing, normalised. */
vect3D getUnitVector(camera_struct* c);

/** @brief Rebuilds a camera's view matrix from its position and orientation. */
void updateViewMatrix(camera_struct* c);

/**
 * @brief Re-orthonormalises a 3x3 rotation matrix in place.
 *
 * Needed because repeated incremental rotation makes the matrix drift away
 * from being a pure rotation.
 */
void fixMatrix(int32* m);

/** @brief Rebuilds a camera's six frustum planes from its current view. */
void updateFrustum(camera_struct* c);

/** @brief Converts a world-space position into the renderer's scaled view space. */
vect3D getViewPosition(vect3D p);

/** @brief Converts a view-space position back into world space. */
vect3D reverseViewPosition(vect3D p);

/**
 * @brief Builds a perspective projection matrix.
 * @param c      camera to configure.
 * @param fovy   vertical field of view.
 * @param aspect aspect ratio, in f32.
 * @param near   near plane distance.
 * @param far    far plane distance.
 */
void initProjectionMatrix(camera_struct* c, int fovy, int32 aspect, int32 near, int32 far);

/**
 * @brief Perspective projection for the bottom screen.
 *
 * Same as @ref initProjectionMatrix but offset vertically, so that the two
 * screens read as one tall viewport with the hinge gap in between.
 */
void initProjectionMatrixBottom(camera_struct* c, int fovy, int32 aspect, int32 near, int32 far);

/** @brief Builds an orthographic projection matrix. Used by the editor's plan view. */
void initProjectionMatrixOrtho(camera_struct* c, int left, int right, int bottom, int top, int zNear, int zFar);

/**
 * @brief Turns a screen pixel into a world-space ray.
 *
 * The inverse of projection - this is how a touch on the screen becomes a
 * selection in the editor.
 *
 * @param c camera to unproject through.
 * @param x screen x in pixels.
 * @param y screen y in pixels.
 * @param o out: ray origin.
 * @param v out: ray direction.
 */
void getUnprojectedZLine(camera_struct* c, s16 x, s16 y, vect3D* o, vect3D* v);

/**
 * @brief Builds a change-of-basis matrix from three axes.
 * @param tm destination 3x3 matrix.
 * @param x  first basis vector.
 * @param y  second basis vector.
 * @param z  third basis vector.
 * @param r  true to write the transpose (world to basis rather than basis to world).
 */
void changeBase(int32* tm, vect3D x, vect3D y, vect3D z, bool r);

/** @brief Multiplies two row-major 3x3 matrices: @p m = @p m1 * @p m2. */
void multMatrix33(int32* m1, int32* m2, int32* m);

/** @brief Multiplies two 4x4 matrices in geometry engine layout. */
void multMatrix44(int32* m1, int32* m2, int32* m);

/** @brief Applies a translation to a 4x4 matrix in place. */
void translateMatrix(int32* tm, int32 x, int32 y, int32 z);

/**
 * @brief Clips a polygon vertex against the frustum.
 *
 * Used when building portal outlines, which must be clipped on the CPU because
 * they are used as a stencil rather than simply drawn.
 *
 * @param f  frustum to clip against.
 * @param v  vertex array.
 * @param vid index of the vertex being clipped.
 * @param vn  total number of vertices.
 * @return the clipped position.
 */
vect3D clipPointFrustum(frustum_struct* f, vect3D* v, u8 vid, const u8 vn);

/** @brief Resets a camera's orientation matrix to the identity. */
void initTransformationMatrix(camera_struct* c);

/**
 * @brief Rotates a 3x3 matrix about the X axis, in place.
 * @param tm matrix to rotate.
 * @param x  angle.
 * @param r  true to pre-multiply (world space), false to post-multiply (local space).
 */
void rotateMatrixX(int32* tm, int32 x, bool r);

/** @brief Rotates a 3x3 matrix about the Y axis, in place. @see rotateMatrixX */
void rotateMatrixY(int32* tm, int32 x, bool r);

/** @brief Rotates a 3x3 matrix about the Z axis, in place. @see rotateMatrixX */
void rotateMatrixZ(int32* tm, int32 x, bool r);

/** @brief Multiplies a 3x3 rotation onto the geometry engine's current matrix. */
void multMatrixGfx33(int32* n);

#endif
