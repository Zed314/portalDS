/**
 * @file game_ex.h
 * @brief The game state's external interface.
 *
 * Deliberately small: this is all the menu and the editor can see of the game.
 * The four callbacks at the top are what @ref gameState points at (see
 * @ref state.h); everything else is the handful of parameters that have to be
 * handed over before the state starts.
 *
 * @par Starting a level
 * The caller sets the map path with @ref setMapFilePath, optionally sets the
 * follow-on level with @ref setNextMapFilePath, then calls
 * @c changeState(&gameState). @ref initGame reads the path back out and loads
 * from it, which is how the state machine avoids needing to pass arguments.
 *
 * @see game.c
 */

#ifndef __GAMEEX9__
#define __GAMEEX9__

/**
 * @brief Builds a level: video modes, textures, entity pools, the map, physics.
 *
 * Called once by main() when the game state is entered.
 */
void initGame(void);

/**
 * @brief Runs one frame: input, entity updates, then rendering.
 *
 * Rendering is the interesting half - the portal views have to be rendered
 * before the main view, so a frame is several passes rather than one.
 */
void gameFrame(void);

/** @brief Tears the level down and releases everything @ref initGame took. */
void killGame(void);

/** @brief Vblank handler for the game state; drives the frame counter and screen flips. */
void gameVBL(void);

/** @brief Sets the level file to load when the game state next starts. */
void setMapFilePath(char* path);

/** @brief Sets the level to advance to when this one is completed. */
void setNextMapFilePath(char* path);

#define LEVELINFOCHARS 32 /**< Size of the @ref levelTitle and @ref levelAuthor buffers, including the terminator. */

/**
 * @brief Sets the title and author shown briefly when a level starts.
 *
 * Both arguments come straight out of the level's @c .ini and so may be
 * arbitrarily long; both are truncated to fit @ref LEVELINFOCHARS. Either may
 * be NULL, which leaves that line empty.
 *
 * @param title  level title.
 * @param author author name; displayed prefixed with "by".
 * @see levelinfo.c
 */
void setLevelInfo(char* title, char* author);

extern char levelTitle[LEVELINFOCHARS];  /**< Current level's title, always NUL terminated. */
extern char levelAuthor[LEVELINFOCHARS]; /**< Current level's author, prefixed with "by", always NUL terminated. */

/**
 * @brief Ends the current level.
 *
 * Advances to the next level if @ref isNextRoom is set, otherwise returns to
 * the menu.
 */
void endGame(void);

extern bool isNextRoom; /**< True when a follow-on level has been set, so @ref endGame chains rather than exiting. */
extern int mainBG;      /**< Background id the composited scene is displayed on. */
#endif
