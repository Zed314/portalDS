/**
 * @file interface.h
 * @brief The editor's toolbar.
 *
 * The fixed row of tool icons, as opposed to the context menus in
 * @ref contextbuttons.h which come and go with the selection. Each button is an
 * image loaded from a PCX file with a position and an
 * interfaceButton_struct::argument identifying which tool or mode it selects.
 *
 * Unlike @ref simplegui.h widgets these keep their own pressed state
 * (interfaceButton_struct::down), because a tool button stays visibly held down
 * for as long as its mode is active.
 */

#ifndef INTERFACE_H
#define INTERFACE_H

/** @brief One toolbar button. */
typedef struct
{
	u8 x, y;                        /**< Position on screen, in pixels. */
	const char* imageName;          /**< Icon file to load. */
	struct gl_texture_t* imageData; /**< The loaded icon. */
	u16 argument;                   /**< Identifies which tool or mode this button selects. */
	bool down;                      /**< True while this button's mode is active. */
}interfaceButton_struct;

/** @brief Loads the toolbar icons and lays the buttons out. */
void initInterface(void);

/**
 * @brief Hit-tests a touch against the toolbar and activates the tool it hits.
 * @param x touch x in pixels.
 * @param y touch y in pixels.
 */
void updateInterfaceButtons(u8 x, u8 y);

/** @brief Opens the editor's pause menu - save, load, test and quit. */
void pauseEditorInterface(void);

/** @brief Releases the toolbar icons. */
void freeInterface(void);

#endif
