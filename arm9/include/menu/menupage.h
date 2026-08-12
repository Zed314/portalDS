/**
 * @file menupage.h
 * @brief The menu's pages of buttons.
 *
 * The front end is a small stack of pages - home, chapter select, custom
 * levels, editor - each of which is a set of @ref simplegui.h buttons plus a
 * camera viewpoint (@ref cameratransition.h) to sweep to.
 *
 * This is also where the program actually branches: the button callbacks in
 * menupage.c are what call @ref setMapFilePath and @c changeState(&gameState),
 * or @ref setEditorMapFilePath and @c changeState(&editorState). Everything
 * else in the menu is presentation.
 *
 * Only two functions are exported, because a page is only ever entered from
 * another page's button - the rest of the page setup functions are static to
 * menupage.c.
 */

#ifndef MENUPAGE_H
#define MENUPAGE_H



/**
 * Inits menu buttons. Must be called once.
 */
void initMenuButtons(void);

/**
 * Displays first page of menu, that is loaded first
 * when game is booted.
 **/
void setupHomeMenuPage(void);

/**
 * @brief Draws the credits, if that page is up.
 *
 * Called from menuFrame() for the screen that is not showing the buttons, in
 * place of the logo. Does nothing on any other page.
 */
void drawMenuCredits(void);

/**
 * @brief Draws the options and their values, if that page is up.
 *
 * Called from menuFrame() for the screen that is not showing the buttons, in
 * place of the logo, exactly as @ref drawMenuCredits is. Does nothing on any
 * other page.
 */
void drawMenuOptions(void);

#endif
