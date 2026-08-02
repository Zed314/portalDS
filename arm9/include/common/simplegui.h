/**
 * @file simplegui.h
 * @brief A minimal touch screen button system.
 *
 * Everything the menus and the editor put on the bottom screen is one of these
 * buttons. There is no widget hierarchy, no layout and no event queue: a flat
 * pool of at most @ref NUMSIMPLEBUTTONS buttons, each with a position, a label
 * and a callback.
 *
 * The lifecycle is deliberately blunt. A screen builds its buttons with
 * @ref createSimpleButton, calls @ref updateSimpleGui and @ref drawSimpleGui
 * every frame, and calls @ref cleanUpSimpleButtons to throw the whole set away
 * when moving on. Nothing is retained between screens, so nothing has to be
 * individually destroyed.
 *
 * Buttons are drawn with the 3D engine like everything else, which is why
 * positions and sizes are @ref vect3D in f32 rather than plain pixel integers.
 *
 * @see menu/menupage.h, which builds whole menu pages out of these, and
 *      keyboard.h, which builds an on-screen keyboard out of them.
 */

#ifndef SIMPLEGUI_H
#define SIMPLEGUI_H

#define NUMSIMPLEBUTTONS (64)   /**< Size of the button pool. */
#define SIMPLEBUTTONMARGINX (6) /**< Horizontal padding between a label and the button edge, in pixels. */
#define SIMPLEBUTTONMARGINY (3) /**< Vertical padding between a label and the button edge, in pixels. */

#define SIMPLEBUTTONSIZEY (SIMPLEBUTTONMARGINY*2+8) /**< Height of a button: one 8 pixel text row plus margins. */

struct sguiButton_struct;

/** @brief Callback invoked when a button is tapped; receives the button itself. */
typedef void(*buttonTargetFunction)(struct sguiButton_struct*);

/**
 * @brief One touchable button.
 *
 * A button shows either a text label (::string) or an image (::mtl), or both.
 */
struct sguiButton_struct
{
	vect3D position; /**< Top left corner, in f32 screen coordinates. */
	vect3D size;     /**< Width and height, in f32. Derived from the label unless an image is set. */
	char* string;    /**< Label text. Not owned - the caller must keep it alive. */
	mtlImg_struct* mtl; /**< Optional image to draw instead of a plain background. */
	vect3D mtlOffset;   /**< Top left of the region of ::mtl to draw. */
	vect3D mtlSize;     /**< Size of the region of ::mtl to draw. */
	buttonTargetFunction targetFunction; /**< Called when the button is tapped. */
	bool used;   /**< False when this pool slot is free. */
	bool active; /**< False for a greyed out button that ignores taps. */
};

typedef struct sguiButton_struct sguiButton_struct;


/** @brief Empties the button pool. Call before building a new screen. */
void initSimpleGui(void);

/**
 * @brief Allocates a button.
 *
 * The size is computed from the label; call @ref simpleButtonSetImage
 * afterwards to give it an image instead.
 *
 * @param p   top left corner, in f32 screen coordinates.
 * @param str label text; must outlive the button.
 * @param f   callback to run when tapped, or NULL.
 * @return the new button, or NULL if the pool is full.
 */
sguiButton_struct* createSimpleButton(vect3D p, const char* str, buttonTargetFunction f);

/**
 * @brief Hit-tests a touch position and fires the matching callback.
 *
 * @param x touch x in screen pixels.
 * @param y touch y in screen pixels.
 * @return true if a button was hit.
 */
bool updateSimpleGui(s16 x, s16 y);

/** @brief Draws every live button. */
void drawSimpleGui(void);

/** @brief Frees every button in the pool. */
void cleanUpSimpleButtons(void);

/**
 * @brief Gives a button an image, and sizes it to match.
 * @param b   button to change.
 * @param mtl texture to draw from.
 * @param o   top left of the region of @p mtl to use.
 * @param s   size of that region.
 */
void simpleButtonSetImage(sguiButton_struct* b, mtlImg_struct* mtl, vect3D o, vect3D s);

#endif
