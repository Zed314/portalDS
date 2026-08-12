/**
 * @file xmem.c
 * @brief Heap inspection, by walking newlib's internal free list.
 *
 * Implements @ref xmem.h. There is no supported way to ask newlib how much of
 * the heap is in use, so this reaches into @c mallinfo and the linker-provided
 * @c __end__ / @c __eheap_end symbols to work it out.
 *
 * Worth having on a machine with 4MB and no virtual memory: the allocation
 * failure messages in memory.c print these figures, which is usually enough to
 * tell a genuine exhaustion from a runaway allocation.
 */

/*
 *	xmem.c
 *	  part of the xlibrary by SunDEV (http://sundev.890m.com)
 *
 *	Changelog :
 *	  21-03-09 : First public release
 *
 */

#include "common/general.h"

extern u8 __end__[];        // end of static code and data
extern u8 __eheap_end[];    // farthest point to which the heap will grow

size_t latestUsed, latestFree;

/*

u8 *getHeapStart() {
	return __end__;
}

u8 *getHeapEnd() {
	return (u8 *)sbrk(0);
}

u8 *getHeapLimit() {
	return __eheap_end;
}

*/

size_t getMemUsed() {
	struct mallinfo mi = mallinfo();
	latestUsed=mi.uordblks;
	return latestUsed;
}

size_t getMemFree() {
	struct mallinfo mi = mallinfo();
	latestFree=mi.fordblks + (getHeapLimit() - getHeapEnd());
	return latestFree;
}
