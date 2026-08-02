/**
 * @file selection.h
 * @brief Dragging out a box of blocks, and acting on it.
 *
 * The editor's central interaction. Touching a block face and dragging picks
 * out a rectangular region of the block array: selection_struct::firstFace is
 * where the drag started, ::currentFace is where the stylus is now, and
 * ::origin and ::size are the resulting box.
 *
 * Once a selection exists, a context menu (@ref contextbuttons.h) offers what
 * can be done with it - fill, empty, make portalable, make sludge - and the
 * choice of menu depends on the selection's shape, which is why there are
 * several button arrays here rather than one.
 *
 * selection_struct::planar distinguishes a selection dragged out across a
 * single surface from one dragged into the volume; ::error marks a selection
 * that cannot be acted on, which is drawn in a different colour rather than
 * silently refused.
 *
 * The same mechanism selects a *target* when wiring a button to a door -
 * selection_struct::selectingTarget puts it in that mode, and ::entity holds
 * the entity being wired.
 */

#ifndef SELECTION_H
#define SELECTION_H

/** @brief The current selection, or the drag in progress. */
typedef struct
{
	blockFace_struct *firstFace;   /**< Face the drag started on. */
	blockFace_struct *secondFace;  /**< Face it ended on, once the drag is finished. */
	blockFace_struct *currentFace; /**< Face under the stylus right now. */
	entity_struct* entity;         /**< Entity being wired, while ::selectingTarget is set. */
	vect3D origin;          /**< Minimum corner of the selected box, in block coordinates. */
	vect3D size;            /**< Extent of the selected box. */
	vect3D currentPosition; /**< Block under the stylus right now. */
	bool active;    /**< True when a selection exists. */
	bool selecting; /**< True while a drag is in progress. */
	bool selectingTarget; /**< True while picking a trigger target rather than a region. */
	bool planar;    /**< True if the selection lies within a single surface. */
	bool error;     /**< True if the current selection is not something that can be acted on. */
}selection_struct;

extern selection_struct editorSelection; /**< The editor's single selection. */

/**
 * @name Context menus, chosen according to the selection's shape.
 * @{
 */
extern contextButton_struct targetSelectionButtonArray[];    /**< Shown while wiring a trigger to a target. */
extern contextButton_struct planarSelectionButtonArray[];    /**< Shown for a selection within one surface. */
extern contextButton_struct groundSelectionButtonArray[];    /**< Shown for a selection on a floor. */
extern contextButton_struct nonplanarSelectionButtonArray[]; /**< Shown for a volume selection. */
/** @} */

/** @brief Clears a selection. */
void initSelection(selection_struct* s);

/** @brief Draws the selection highlight over the affected faces. */
void drawSelection(selection_struct* s);

/** @brief Tracks the stylus, extends the drag and puts up the right context menu. */
void updateSelection(selection_struct* s);

/** @brief Cancels the current selection. */
void undoSelection(selection_struct* s);

/** @brief Tests whether a block face lies within a selection. */
bool isFaceInSelection(blockFace_struct* bf, selection_struct* s);

/**
 * @brief Recomputes a selection's box after the room underneath it has changed.
 *
 * The saved copies of the three faces are passed by value because the originals
 * may have been freed by the edit that prompted the adjustment.
 *
 * @param er room being edited.
 * @param s  selection to fix up.
 * @param of the start face, as it was.
 * @param os the end face, as it was.
 * @param oc the current face, as it was.
 * @param v  offset the edit moved things by.
 */
void adjustSelection(editorRoom_struct* er, selection_struct* s, blockFace_struct of, blockFace_struct os, blockFace_struct oc, vect3D v);

#endif
