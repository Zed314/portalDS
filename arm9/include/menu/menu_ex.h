/**
 * @file menu_ex.h
 * @brief The menu state's external interface.
 *
 * Just the four state callbacks (see @ref state.h). The menu is where the
 * program starts and where the game and the editor are launched from, so
 * nothing needs to hand it any parameters - it is the other two that need
 * setting up before they start.
 *
 * @see menu.c
 */

#ifndef MENUEX_H
#define MENUEX_H

extern u8 logoAlpha; /**< Fade level of the logo overlay, animated on entry. */

/** @brief Sets up the menu's video modes, background scene and first page. */
void initMenu(void);

/** @brief Runs one menu frame: input, scene animation, then drawing. */
void menuFrame(void);

/** @brief Tears the menu down and releases its resources. */
void killMenu(void);

/** @brief Vblank handler for the menu state. */
void menuVBL(void);

#endif
