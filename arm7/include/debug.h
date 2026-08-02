/**
 * @file debug.h
 * @brief ARM7 profiling and no$gba debug output.
 *
 * Two unrelated debugging aids that happen to share a header:
 *
 *  - a cycle counter built from the DS's timer 2 and 3 chained together, which
 *    is how the @c coll / @c integ / @c impul figures in OBB.c were measured;
 *  - @ref NOGBA, which prints to the no$gba emulator's debug window. It is a
 *    no-op on real hardware, so it can be left in place.
 *
 * @note This header is not part of the normal build - stdafx.h includes it only
 *       when you uncomment the line at the bottom of that file.
 * @note The include guard says @c __DEBUG9__ because this file started life as
 *       a copy of the ARM9 one.
 */

#ifndef __DEBUG9__
#define __DEBUG9__

#define VERSIONMAGIC 45464873 /**< Sentinel written into save data to detect a stale format. */

/**
 * @brief Starts the profiling counter.
 *
 * Timer 3 is cascaded off timer 2, giving a single 32 bit counter ticking at
 * the CPU clock.
 */
#define     PROF_START()                \
do {                                \
    TIMER2_DATA = 0; TIMER3_DATA = 0;   \
    TIMER3_CR = TIMER_ENABLE | TIMER_CASCADE | TIMER_IRQ_REQ; \
    TIMER2_CR = TIMER_ENABLE;    \
} while(0)

/** @brief Reads the profiling counter without stopping it. */
#define PROF_GET(_time) _time = ( TIMER3_DATA << 16 ) | TIMER2_DATA;

/** @brief Reads the profiling counter into @p _time and stops both timers. */
#define     PROF_END(_time)             \
do {                                \
    _time = ( TIMER3_DATA << 16 ) | TIMER2_DATA;  \
    TIMER2_CR = 0; TIMER3_CR = 0;    \
} while(0)


#define PROF2_START() /**< @brief Disabled second profiling channel. */
#define PROF2_END(_time) _time=92431 /**< @brief Disabled second profiling channel; yields a recognisable dummy value. */
#include <stddef.h>

/**
 * @brief Minimal snprintf for the ARM7.
 *
 * The ARM7 links against a cut-down libc without a full printf, so
 * snprintf7_arm7.c provides just enough to format debug messages.
 *
 * @param s   destination buffer.
 * @param n   size of @p s including the terminator.
 * @param fmt printf-style format string.
 * @return the number of characters that would have been written.
 */
int snprintf_arm7(char * s, size_t n , const char * fmt, ...);

/** @brief printf-style logging to the no$gba debug window. Does nothing on hardware. */
#define NOGBA(_fmt, _args...) do { char nogba_buffer[128]; snprintf_arm7(nogba_buffer, sizeof(nogba_buffer), _fmt, ##_args); nocashMessage(nogba_buffer); } while(0)

void DS_Debug(char* string, ...); /**< @brief Legacy debug print. Not implemented on the ARM7. */
void DS_DebugPause(void);         /**< @brief Legacy debug breakpoint. Not implemented on the ARM7. */
size_t DS_UsedMem(void);          /**< @brief Bytes currently allocated on the heap. */
size_t DS_FreeMem(void);          /**< @brief Bytes still available on the heap. */

#endif
