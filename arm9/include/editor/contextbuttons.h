/**
 * @file contextbuttons.h
 * @brief Pop-up menus for whatever is currently selected.
 *
 * When the editor has a selection - a region of blocks, or an entity - a small
 * menu appears offering what can be done with it. Which menu appears is chosen
 * by the caller: @ref selection.h and @ref entity.h each declare several
 * @ref contextButton_struct arrays, and @ref setupContextButtons installs the
 * right one.
 *
 * A context button array is static data - a label and a callback per entry - so
 * giving a new entity type its own menu is a matter of writing the array, not
 * touching this file.
 *
 * The buttons themselves are built from @ref simplegui.h widgets, and are torn
 * down and rebuilt whenever the selection changes.
 */

#ifndef CONTEXTBUTONS_H
#define CONTEXTBUTONS_H

/** @brief One entry of a context menu: a label and what it does. */
typedef struct
{
	const char* string;                  /**< Label text. */
	buttonTargetFunction targetFunction; /**< Called when it is tapped. */
}contextButton_struct;

/** @brief Clears any context menu that is showing. */
void initContextButtons(void);

/**
 * @brief Shows a context menu.
 * @param cb array of entries; must outlive the menu.
 * @param n  number of entries.
 */
void setupContextButtons(contextButton_struct* cb, u8 n);

/**
 * @brief Hit-tests a touch against the current menu.
 * @param tp touch position.
 * @return true if a menu entry was hit, so the caller should not also treat
 *         the touch as an edit.
 */
bool updateContextButtons(touchPosition* tp);

/** @brief Draws the current context menu. */
void drawContextButtons(void);

/** @brief Tears down the current context menu. */
void cleanUpContextButtons(void);

#endif
