/**
 * @file pause.h
 * @brief The pause menu.
 *
 * A small overlay offering resume, restart and quit-to-menu. It runs inside the
 * game state rather than being a state of its own, so the level stays loaded
 * and resuming costs nothing.
 *
 * @ref doPause takes the frame buffer it should draw over: the paused frame is
 * kept on screen and dimmed underneath the menu, which is why the buffer has to
 * be passed in rather than simply drawn to.
 */

#ifndef PAUSE_H
#define PAUSE_H

/** @brief Loads the pause menu's textures and builds its buttons. */
void initPause(void);

/**
 * @brief Runs the pause menu until the player resumes or leaves.
 *
 * Blocks the game's frame loop for as long as it is open.
 *
 * @param buffer the frozen game frame to dim and draw the menu over.
 */
void doPause(u16* buffer);

/** @brief Releases the pause menu's resources. */
void freePause(void);

#endif
