/**
 * @file editor_ex.h
 * @brief The editor state's external interface.
 *
 * All the menu can see of the editor: the four state callbacks (see
 * @ref state.h) and the file path to open.
 *
 * As with the game, the level to load is handed over before the state starts
 * rather than passed as an argument - @ref setEditorMapFilePath, then
 * @c changeState(&editorState).
 *
 * @see editor.c
 */

#ifndef __EDITOREX9__
#define __EDITOREX9__

/** @brief Sets up the editor's video modes and builds an empty or loaded room. */
void initEditor(void);

/** @brief Runs one editor frame: input, then drawing. */
void editorFrame(void);

/** @brief Tears the editor down and releases everything it allocated. */
void killEditor(void);

/** @brief Vblank handler for the editor state. Currently does nothing. */
void editorVBL(void);

/** @brief Sets the level file the editor opens when it next starts. */
void setEditorMapFilePath(char* str);

/** @brief Returns the level file path currently being edited. */
char* getEditorMapFilePath(void);

#endif
