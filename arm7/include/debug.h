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

/**
 * @name Debug logging
 *
 * @ref NOGBA writes to no$gba's debug console, and is compiled out unless
 * @c NOGBA_LOGGING is defined.
 *
 * It is off by default for two reasons. The write lands on 0x04FFFA14, which
 * real hardware ignores but which emulators report: DeSmuME prints
 * "write32 to undefined register 04FFFA14h" for every single call, and a
 * message on a per-frame path buries its own log in the noise. And each call
 * formats into a 128 byte stack buffer before writing, which is not free on a
 * 33MHz ARM7 when it happens every frame.
 *
 * Switch it back on for a debugging session with
 *
 *     ./docker-build.sh clean
 *     ./docker-build.sh 'DEFINES=-DPICOLIBC_LONG_LONG_PRINTF_SCANF -DNOGBA_LOGGING'
 *
 * (DEFINES is the Makefile variable the flag has to join; overriding it drops
 * the picolibc one, so pass both.)
 *
 * or by defining it above this header. The disabled form still compiles the
 * format string and its arguments - inside @c sizeof, which is not evaluated -
 * so they stay checked against each other and still count as used, and no code
 * is generated for them.
 * @{
 */
#ifdef NOGBA_LOGGING
	#define NOGBA(_fmt, _args...) do { char nogba_buffer[128]; snprintf_arm7(nogba_buffer, sizeof(nogba_buffer), _fmt, ##_args); nocashMessage(nogba_buffer); } while(0)
#else
	#define NOGBA(_fmt, _args...) do { char nogba_buffer[1]; (void)sizeof(snprintf_arm7(nogba_buffer, 0, _fmt, ##_args)); } while(0)
#endif
/** @} */

void DS_Debug(char* string, ...); /**< @brief Legacy debug print. Not implemented on the ARM7. */
void DS_DebugPause(void);         /**< @brief Legacy debug breakpoint. Not implemented on the ARM7. */
size_t DS_UsedMem(void);          /**< @brief Bytes currently allocated on the heap. */
size_t DS_FreeMem(void);          /**< @brief Bytes still available on the heap. */

#endif
