/**
 * @file profiler.h
 * @brief Compile-time-optional frame profiler for the game loop.
 *
 * Measures where each half-frame of gameFrame() goes, using the timer pair
 * that gameFrame already restarts at every vblank (cpuStartTiming/
 * cpuGetTiming), and reports averages every couple of seconds over the
 * no$gba debug channel - the same sink NOGBA uses, shown by no$gba's TTY
 * window, melonDS's console output and DeSmuME's console.
 *
 * A game frame is two vblanks (each screen updates at 30Hz - see dual3D.c),
 * so the report has two lines, one per half:
 *
 *     PROF A: cpy  812 pp 1520 upd 2200 sub 3100 | busy  7632us max  9100 (45%)
 *     PROF B: cpy  812 pp  900 sub 2500 phy 1800 | busy  6012us max  8000 (36%)
 *
 * Section key - all values are averages in microseconds against the 16715us
 * vblank period, busy is their sum plus anything between the marks, and the
 * remainder of the period is idle headroom:
 *  - cpy: the 96KB capture-buffer dmaCopy at the tail of the previous half;
 *  - pp:  postProcess1/postProcess2 (portal screen compositing);
 *  - upd: input, player and entity updates (half A only);
 *  - sub: 3D scene submission up to and including glFlush;
 *  - phy: listenPI9 + updateOBBs, the ARM9 side of physics (half B only).
 *    The simulation itself runs on the ARM7 and is not visible here.
 *
 * Enable with
 *
 *     ./docker-build.sh clean
 *     ./docker-build.sh 'DEFINES=-DPICOLIBC_LONG_LONG_PRINTF_SCANF -DFRAME_PROFILING'
 *
 * (DEFINES is the Makefile variable the flag has to join; overriding it drops
 * the picolibc one, so pass both.) Disabled, every call in this header is an
 * empty inline and no code is generated.
 */
#ifndef PROFILER_H
#define PROFILER_H

/** @brief The measured slices of a half-frame. Not every half uses every one. */
typedef enum
{
	PROF_COPY,     /**< Capture-buffer dmaCopy at the tail of the previous half. */
	PROF_POSTPROC, /**< postProcess1 or postProcess2. */
	PROF_UPDATES,  /**< Input, player and entity updates (half A). */
	PROF_SUBMIT,   /**< Scene submission through glFlush (render1/render2). */
	PROF_PHYSICS,  /**< listenPI9 + updateOBBs (half B). */
	PROF_NSECTIONS
}profilerSection_type;

#ifdef FRAME_PROFILING

/** @brief Declares which half the epoch that just began belongs to (0=A, 1=B).
 *  Call right after the cpuStartTiming(0) that opens it. */
void profilerEpochStart(int half);

/** @brief Ends a section: everything since the previous mark is charged to @p s. */
void profilerSectionEnd(profilerSection_type s);

/** @brief Ends the busy part of the current half. Call just before
 *  swiWaitForVBlank; emits the report once enough halves have accumulated. */
void profilerHalfEnd(void);

/** @brief Emits the low 24 bits of @p value on the raw-register side channel
 *  under tag 0xFF - for run harness telemetry (which auto-aim landed, etc). */
void profilerEmitDebug(u32 value);

#else

static inline void profilerEpochStart(int half){(void)half;}
static inline void profilerSectionEnd(profilerSection_type s){(void)s;}
static inline void profilerHalfEnd(void){}
static inline void profilerEmitDebug(u32 value){(void)value;}

#endif

#endif
