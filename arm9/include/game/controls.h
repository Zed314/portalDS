/**
 * @file controls.h
 * @brief Input mapping, and the touch screen camera.
 *
 * Two jobs. The first is remapping: @ref loadControlConfiguration reads an ini
 * file naming which physical button does what, so the layout is not hard coded.
 *
 * The second is the camera. The DS has one stick's worth of input and needs
 * two - movement and looking - so looking is done by dragging on the touch
 * screen. @ref updateControls turns the drag delta into a rotation and feeds it
 * to the player camera. @ref touchCnt counts how long the screen has been held,
 * which is what separates a tap (switch portal colour) from a drag (look
 * around).
 *
 * @see game.c, which calls @ref updateControls once per frame, and player.h for
 *      what the mapped buttons end up doing.
 */

#ifndef CONTROLS_H
#define CONTROLS_H

extern u8 touchCnt; /**< Frames the touch screen has been held; distinguishes a tap from a drag. */

/**
 * @brief Reads the button mapping from an ini file.
 * @param filename configuration file, normally "config.ini".
 */
void loadControlConfiguration(char* filename);

/**
 * @brief Samples input and applies it: look rotation, and the mapped actions.
 *
 * Call once per frame, after @c scanKeys.
 */
void updateControls(void);

#endif
