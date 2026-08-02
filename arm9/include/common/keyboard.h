/**
 * @file keyboard.h
 * @brief On-screen keyboard for entering level names.
 *
 * Builds a grid of @ref simplegui.h buttons - four rows of letters and digits
 * plus a backspace - wired to append to a caller-supplied character buffer.
 * The only user is the editor, when naming a level to save.
 *
 * The keyboard has no lifetime of its own: it creates buttons in the current
 * GUI page and stops existing when that page is torn down.
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

/**
 * @brief Creates the keyboard's buttons and points them at a text buffer.
 *
 * Typing appends to @p buffer and keeps it NUL-terminated; backspace removes
 * the last character. Input stops when @p size characters have been entered.
 *
 * @param buffer destination for the typed text; must hold @p size +1 bytes.
 * @param size   maximum number of characters to accept.
 * @param x      left edge of the keyboard, in screen pixels.
 * @param y      top edge of the keyboard, in screen pixels.
 */
void setupKeyboard(char* buffer, u8 size, s16 x, s16 y);

#endif
