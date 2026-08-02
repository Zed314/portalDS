/**
 * @file font.h
 * @brief Bitmap text rendering through the 3D engine.
 *
 * Text is drawn as textured quads, not with the 2D text engine - both
 * backgrounds are already spoken for by the dual-screen 3D setup (see
 * @ref dual3D.h), so the only way to get characters on screen is to draw them
 * as geometry.
 *
 * A font is one texture holding a 16x16 grid of glyphs indexed by ASCII code.
 * font_struct::charsize is the size of a cell in the texture, while
 * font_struct::rendersize is how large it is drawn - keeping them separate is
 * what lets the same font be used for both the menu headings and the small
 * labels in the editor.
 */

#ifndef __FONT9__
#define __FONT9__

#define CHARSIZE 16 /**< Glyphs per row and per column in the font texture. */

/**
 * @brief A loaded font.
 */
typedef struct
{
	mtlImg_struct tex;  /**< Texture holding the 16x16 glyph grid. */
	int32 charsizef32;  /**< Cell size in f32, precomputed for texture coordinate maths. */
	u8 charsize;        /**< Cell size in the texture, in pixels. */
	u8 rendersize;      /**< Size glyphs are drawn at, in pixels. */
}font_struct;

/** @brief Sets up the 3D state text rendering expects. Call before any drawing. */
void initText(void);

/** @brief Selects the font subsequent @ref drawChar and @ref drawString calls use. */
void setFont(font_struct* f);

/**
 * @brief Loads a font texture and computes its derived sizes.
 * @param f          font to fill in.
 * @param charsize   cell size in the texture, in pixels.
 * @param rendersize size to draw glyphs at, in pixels.
 */
void loadFont(font_struct* f, u8 charsize, u8 rendersize);

/**
 * @brief Draws a single character.
 * @param c     character to draw; its ASCII code indexes the glyph grid.
 * @param color 15 bit colour to tint with.
 * @param x     left edge, in f32 screen coordinates.
 * @param y     top edge, in f32 screen coordinates.
 */
void drawChar(char c, u16 color, int32 x, int32 y);

/**
 * @brief Draws a NUL-terminated string.
 * @param s     text to draw.
 * @param color 15 bit colour to tint with.
 * @param size  character size in f32; overrides the font's render size.
 * @param x     left edge, in f32 screen coordinates.
 * @param y     top edge, in f32 screen coordinates.
 */
void drawString(char* s, u16 color, int32 size, int32 x, int32 y);

#endif
