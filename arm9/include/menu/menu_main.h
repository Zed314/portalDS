/**
 * @file menu_main.h
 * @brief Umbrella include for the front end.
 *
 * Every file under arm9/source/menu/ includes this. Like the editor, the menu
 * pulls in the whole game: its background is a real 3D scene drawn with the
 * game's renderer, not a static image.
 *
 * The three parts:
 *  - @ref menuscene.h - the animated background, including the in-fiction
 *    terminal display that the level lists are drawn on;
 *  - @ref menupage.h - the pages of buttons, and where the game and editor
 *    states are actually launched from;
 *  - @ref cameratransition.h - the sweeps between camera positions that play
 *    when you move between pages.
 *
 * @see menu_ex.h for the small surface the rest of the program sees.
 */

#ifndef MENUMAIN_H
#define MENUMAIN_H

#include "common/general.h"
#include "game/game_main.h"        // the menu background uses the game's renderer
#include "menu/cameratransition.h" // camera sweeps between pages
#include "menu/menupage.h"         // the pages of buttons
#include "menu/menuscene.h"        // the animated background and its terminal

#endif
