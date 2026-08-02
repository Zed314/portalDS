/**
 * @file roomeditor.h
 * @brief The editor's frame loop: camera, stylus cursor and editing.
 *
 * The top of the editor proper. @ref updateRoomEditor reads the stylus and the
 * buttons, turns a touch into a world ray, works out which block face or entity
 * it hits, and drives the selection; @ref drawRoomEditor then draws the block
 * faces, the entities, the selection highlight and the interface.
 *
 * @ref switchScreens swaps which physical screen shows the 3D view. The editor
 * needs the view and the toolbar on opposite screens, and which arrangement is
 * comfortable depends on whether you are holding the stylus in your left or
 * right hand - hence the toggle.
 *
 * @ref editorRoom is the single room being edited; there is no notion of
 * multiple open documents.
 */

#ifndef ROOMEDITOR_H
#define ROOMEDITOR_H

extern editorRoom_struct editorRoom; /**< The room currently being edited. */

/** @brief Builds the editor: room, entities, materials, selection and interface. */
void initRoomEdition(void);

/** @brief Draws the room, entities, selection highlight and interface. */
void drawRoomEditor(void);

/** @brief Releases everything @ref initRoomEdition allocated. */
void freeRoomEditor(void);

/** @brief Reads input and applies it: camera movement, selection, and edits. */
void updateRoomEditor(void);

/** @brief Swaps which physical screen shows the 3D view and which shows the toolbar. */
void switchScreens(void);

#endif
