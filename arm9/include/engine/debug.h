/**
 * @file debug.h
 * @brief ARM9 profiling counters and no$gba debug output.
 *
 * @ref PROF_START / @ref PROF_END chain timers 2 and 3 into a single 32 bit
 * cycle counter; this is what the frame time and per-subsystem figures shown
 * by the debug overlay are measured with.
 *
 * @ref NOGBA prints to the no$gba emulator's debug window and compiles to a
 * harmless no-op on real hardware, so calls can be left in shipping code. It is
 * the only logging facility available - there is no console on screen once the
 * 3D engine has taken over both displays.
 *
 * @see arm7/include/debug.h for the near-identical ARM7 version, which needs
 *      its own cut-down snprintf.
 */

#ifndef __DEBUG9__
#define __DEBUG9__

#define VERSIONMAGIC 45464873 /**< Sentinel written into save data so a stale format can be detected. */

/**
 * @brief Starts the profiling counter.
 *
 * Timer 3 is cascaded off timer 2, giving one 32 bit counter at the CPU clock.
 */
#define     PROF_START()                \
do {                                \
    TIMER2_DATA = 0; TIMER3_DATA = 0;   \
    TIMER3_CR = TIMER_ENABLE | TIMER_CASCADE | TIMER_IRQ_REQ; \
    TIMER2_CR = TIMER_ENABLE;    \
} while(0)

/** @brief Reads the profiling counter into @p _time without stopping it. */
#define PROF_GET(_time) _time = ( TIMER3_DATA << 16 ) | TIMER2_DATA;

/** @brief Reads the profiling counter into @p _time and stops both timers. */
#define     PROF_END(_time)             \
do {                                \
    _time = ( TIMER3_DATA << 16 ) | TIMER2_DATA;  \
    TIMER2_CR = 0; TIMER3_CR = 0;    \
} while(0)


#define PROF2_START() /**< @brief Disabled second profiling channel. */
#define PROF2_END(_time) _time=92431 /**< @brief Disabled second profiling channel; yields a recognisable dummy value. */

/** @brief printf-style logging to the no$gba debug window. Does nothing on hardware. */
#define NOGBA(_fmt, _args...) do { char nogba_buffer[128]; snprintf(nogba_buffer, sizeof(nogba_buffer), _fmt, ##_args); nocashMessage(nogba_buffer); } while(0)

void DS_Debug(char* string, ...); /**< @brief printf-style message to the debug console. */
void DS_DebugPause(void);         /**< @brief Blocks until a key is pressed; a poor man's breakpoint. */
size_t DS_UsedMem(void);          /**< @brief Bytes currently allocated on the heap. */
size_t DS_FreeMem(void);          /**< @brief Bytes still available on the heap. */

#endif
