/**
 * @file debug.c
 * @brief Debug printing and heap reporting.
 *
 * Implements the functions declared in @ref engine/debug.h. @ref DS_UsedMem
 * and @ref DS_FreeMem wrap the heap walk in debug/xmem.c; @ref DS_Debug and
 * @ref DS_DebugPause are the older console-based helpers, which only work
 * before the 3D engine has claimed both screens.
 *
 * For anything that needs to work during gameplay, use the @ref NOGBA macro
 * instead - it goes to the emulator's debug window and needs no screen.
 */

#include "common/general.h"

void DS_Debug(char* string, ...)
{
	//va_list varg;
	//NOGBA(string);
	// iprintf(string);
}

void DS_DebugPause(void)
{
	DS_Debug("\n..Touch the screen to continue..\n");
	scanKeys();
	while(!(keysDown() & KEY_TOUCH))scanKeys();
}

size_t DS_UsedMem(void)
{
	return getMemUsed();
}

size_t DS_FreeMem(void)
{
	return getMemFree();
}
