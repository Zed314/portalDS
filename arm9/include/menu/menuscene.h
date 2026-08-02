/**
 * @file menuscene.h
 * @brief The menu's animated 3D background, and the terminal in it.
 *
 * The front end is set inside an Aperture Science room rather than over a flat
 * image. Two things live here:
 *
 *  - **the scene** - the room and the drifting cubes
 *    (@ref menuBox_struct), which spawn periodically, tumble and fade;
 *  - **the terminal** - a screen on the wall that displays text. It is not a
 *    decoration: the level lists are drawn on it, so choosing a chapter means
 *    reading a diegetic monitor rather than a menu widget. Text goes in
 *    @ref menuScreenText, a fixed @ref MENUSCREENLINES x @ref MENUSCREENCHARS
 *    character grid.
 *
 * A @ref screenList_struct is a scrollable list rendered onto that terminal -
 * a title, the entries, and a cursor. The list of strings is *not* copied, so
 * whatever supplied it must keep it alive.
 */

#ifndef MENUSCENE_H
#define MENUSCENE_H

#define NUMMENUBOXES (8)        /**< Maximum number of drifting cubes at once. */
#define MENUBOXSPEED (32)       /**< How fast they drift. */
#define MENUBOXFREQUENCY (20)   /**< Frames between spawns. */
#define MENUBOXANGLESPEED (1024)/**< Maximum tumble rate. */

/* Number of lines displayed in the screen terminal */
#define MENUSCREENLINES 6
/* Number of columns displayed in the screen terminal */
#define MENUSCREENCHARS 14

/** @brief One drifting cube in the background. */
typedef struct
{
	vect3D angle;      /**< Current orientation. */
	vect3D anglespeed; /**< Tumble rate about each axis. */
	u16 progress;      /**< How far through its drift it is; also drives the fade. */
	bool used;         /**< False when this slot is free. */
}menuBox_struct;

extern camera_struct menuCamera; /**< Camera the background scene is viewed through. */

extern char menuScreenText[MENUSCREENLINES][MENUSCREENCHARS]; /**< Character grid drawn on the in-scene terminal. */

/**
 * Inits menu scene, so the animation in the background of the menu.
 * Must only be called once before freeing ressources using #freeMenuScene.
 */
void initMenuScene(void);

/**
 * Free menu scene graphics elements.
 * Must only be called once before allocating again ressources using #initMenuScene.
 */
void freeMenuScene(void);

/**
 * Updates background animation of the menu.
 * Must be called every frame.
 */
void updateMenuScene(void);

/**
 * Draws background animation of the menu.
 * Must be called every frame.
 */
void drawMenuScene(void);

/**
 * Reset background screen (terminal that displays level lists)
 */
void resetSceneScreen(void);

/** @brief A scrollable list rendered onto the in-scene terminal. */
typedef struct
{
	char title[MENUSCREENCHARS]; /**< List heading; copied. */
	char** list;   /**< The entries. Not owned - the caller must keep them alive. */
	int length;    /**< Number of entries. */
	int offset;    /**< Index of the first visible entry; scrolling changes this. */
	int cursor;    /**< Index of the highlighted entry. */
}screenList_struct;

/**
 * Initialises background terminal display list
 *
 * \param[out] sl 		screen list
 * \param[in]  title	title of the list (copied)
 * \param[in]  list  	list of string    (not deep copied)
 * \param[in]  l        length of list
 *
 */
void initScreenList(screenList_struct* sl, char* title, char** list, int l);

/**
 * @brief Moves the cursor, scrolling the visible window if it runs off the end.
 * @param sl   list to move within.
 * @param move signed number of entries to move by.
 */
void screenListMove(screenList_struct* sl, s8 move);

/** @brief Renders a list into @ref menuScreenText so the terminal shows it. */
void updateScreenList(screenList_struct* sl);



#endif
