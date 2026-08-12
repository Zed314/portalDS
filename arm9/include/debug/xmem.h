/**
 * @file xmem.h
 * @brief Heap inspection.
 *
 * From SunDEV's xlibrary. Walks the newlib heap to report how much of it is in
 * use, which matters a great deal here: the ARM9 has 4MB total and a busy room
 * with several loaded models can come close to exhausting it. The figures are
 * what @ref DS_UsedMem and @ref DS_FreeMem report, and what the allocation
 * failure messages in memory.c print.
 *
 * @note @ref getMemUsed walks the free list, so it is not something to call
 *       every frame.
 */

/*
 *	xmem.h
 *	  part of the xlibrary by SunDEV (http://sundev.890m.com)
 *
 *	Changelog :
 *	  21-03-09 : First public release
 *
 */

#ifndef _XMEM_H
#define _XMEM_H

extern size_t latestUsed, latestFree; /**< Results of the most recent @ref getMemUsed / @ref getMemFree call, cached for display. */

u8 *getHeapStart(); /**< @brief Lowest address of the heap. */
u8 *getHeapEnd();   /**< @brief Current top of the heap - how far it has actually grown. */
u8 *getHeapLimit(); /**< @brief Highest address the heap may grow to before it meets the stack. */
size_t getMemUsed(); /**< @brief Bytes currently allocated, found by walking the free list. */
size_t getMemFree(); /**< @brief Bytes still available between the heap top and its limit. */

#endif
