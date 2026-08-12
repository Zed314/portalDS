/**
 * @file contextbuttons.c
 * @brief The pop-up menus that follow the selection.
 *
 * Implements @ref contextbuttons.h. @ref setupContextButtons tears down
 * whatever menu was showing and builds @ref simplegui.h buttons for the new
 * one, laid out next to the current selection.
 *
 * @ref updateContextButtons reports whether the touch hit a menu entry, so the
 * caller knows not to also treat it as an edit - without that, tapping a menu
 * would simultaneously move the selection underneath it.
 */

#include "editor/editor_main.h"

#define CONTEXTMARGINX (4)
#define CONTEXTMARGINY (4)

#define CONTEXTSTEPY (SIMPLEBUTTONSIZEY+2)

void initContextButtons(void)
{
	initSimpleGui();
}

bool updateContextButtons(touchPosition* tp)
{
	if(!(keysHeld() & KEY_TOUCH)) return updateSimpleGui(-1, -1);
	else return updateSimpleGui(tp->px,tp->py);
}

void setupContextButtons(contextButton_struct* cb, u8 n)
{
	if(!cb || !n)return;

	cleanUpContextButtons();
	int i;
	for(i=0;i<n;i++)
	{
		createSimpleButton(vect(CONTEXTMARGINX,CONTEXTMARGINY+CONTEXTSTEPY*i,0), cb[i].string, cb[i].targetFunction);
	}
}

void drawContextButtons(void)
{
	drawSimpleGui();
}

void cleanUpContextButtons(void)
{
	cleanUpSimpleButtons();
}
