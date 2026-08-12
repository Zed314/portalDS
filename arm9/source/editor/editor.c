/**
 * @file editor.c
 * @brief The editor state: setup, frame loop and teardown.
 *
 * Implements @ref editor_ex.h. Deliberately thin - it sets up video modes,
 * VRAM banks and the 3D engine, then hands everything to
 * @ref initRoomEdition and @ref updateRoomEditor.
 *
 * Note @c lcdMainOnBottom() at the top of @ref initEditor - the editor puts the
 * 3D view on the bottom screen so it can be drawn on directly with the stylus,
 * which is the opposite of the game. @ref switchScreens flips this at runtime.
 */

#include "editor/editor_main.h"

void editorVBL(void)
{

}

void initEditor(void)
{
	lcdMainOnBottom();
	videoSetMode(MODE_5_3D);
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	
	vramSetPrimaryBanks(VRAM_A_TEXTURE,VRAM_B_TEXTURE,VRAM_C_SUB_BG,VRAM_D_TEXTURE);	
	
	glInit();
	
	glEnable(GL_TEXTURE_2D);
	// glEnable(GL_ANTIALIAS);
	glEnable(GL_BLEND);
	glEnable(GL_OUTLINE);
	
	glClearPolyID(63);
	glClearDepth(0x7FFF);
	glViewport(0,0,255,191);
	
	initVramBanks(1);
	initTextures();
	
	initRoomEdition();
	NOGBA("START mem free : %dko (%do)",getMemFree()/1024,getMemFree());

	fadeIn();
}

int cnd=0;

void editorFrame(void)
{
	scanKeys();
	GFX_CLEAR_COLOR=RGB15(27,27,27)|(31<<16);
	
	updateRoomEditor();
	drawRoomEditor();
	
	swiWaitForVBlank();
}

void killEditor(void)
{
	fadeOut();
	freeRoomEditor();
	freeState(NULL);
}


