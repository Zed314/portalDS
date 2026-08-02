/**
 * @file cameratransition.h
 * @brief Smooth camera sweeps between fixed viewpoints in the menu.
 *
 * The menu background is a real 3D scene, and each page is looked at from its
 * own vantage point. Rather than cutting between them, the camera glides: a
 * @ref cameraTransition_struct interpolates position and angle from one
 * @ref cameraState_struct to another over a fixed number of frames.
 *
 * The viewpoints themselves are a static table, @ref cameraStates, indexed by
 * page - so adding a menu page means adding an entry there and starting a
 * transition to it.
 */

#ifndef CAMERATRANSITION_H
#define CAMERATRANSITION_H

/** @brief A fixed camera viewpoint. */
typedef struct
{
	vect3D position; /**< Where the camera sits. */
	vect3D angle;    /**< Which way it looks. */
}cameraState_struct;

/** @brief A sweep in progress between two viewpoints. */
typedef struct
{
	cameraState_struct *start;  /**< Viewpoint being left. */
	cameraState_struct *finish; /**< Viewpoint being approached. */
	int progress;               /**< Frames elapsed. */
	int length;                 /**< Total frames the sweep takes. */
}cameraTransition_struct;

extern cameraState_struct cameraStates[];      /**< The table of menu viewpoints, indexed by page. */
extern cameraTransition_struct testTransition; /**< The sweep currently playing. */

/** @brief Snaps a camera to a viewpoint with no interpolation. */
void applyCameraState(camera_struct* c, cameraState_struct* cs);

/**
 * @brief Begins a sweep between two viewpoints.
 * @param s      viewpoint to start from.
 * @param f      viewpoint to end at.
 * @param length duration in frames.
 * @return the transition, to be passed to @ref updateCameraTransition each frame.
 */
cameraTransition_struct startCameraTransition(cameraState_struct* s, cameraState_struct* f, int length);

/**
 * @brief Advances a sweep by one frame and moves the camera accordingly.
 *
 * Does nothing once the transition has finished, so it is safe to keep calling.
 */
void updateCameraTransition(camera_struct* c, cameraTransition_struct* ct);

#endif
