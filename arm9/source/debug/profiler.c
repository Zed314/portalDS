/**
 * @file profiler.c
 * @brief Implements @ref profiler.h. Compiled to nothing without FRAME_PROFILING.
 *
 * The mechanism is a single running timestamp against the timer pair gameFrame
 * restarts at every vblank: each profilerSectionEnd charges "now minus the
 * last mark" to its section, so the sections tile the half-frame exactly and
 * anything not covered by a mark still shows up in busy. Sums are u32: a
 * section can reach at most ~560k ticks per half, times 128 halves is well
 * inside range.
 */
#include "common/general.h"

#ifdef FRAME_PROFILING

/** Halves of each kind per report: 128 of each is ~4.3 seconds of game. */
#define PROFILER_REPORT_HALVES 128

/** Timer ticks in one 59.8261Hz vblank period at 33.513982MHz. */
#define PROFILER_FRAME_TICKS 560190

static u32 sectionSum[2][PROF_NSECTIONS];
static u32 busySum[2], busyMax[2];
static u16 halves[2];
static u32 lastStamp;
static int currentHalf;
static bool started; /**< False until the first epoch: the timer holds garbage before the first cpuStartTiming. */

static u32 ticksToUsec(u32 ticks)
{
	return (u32)(((u64)ticks*1000)/33514);
}

void profilerEpochStart(int half)
{
	currentHalf=half;
	lastStamp=0; //the caller just restarted the timer
	started=true;
}

void profilerSectionEnd(profilerSection_type s)
{
	if(!started)return;
	const u32 now=cpuGetTiming();
	sectionSum[currentHalf][s]+=now-lastStamp;
	lastStamp=now;
}

/**
 * DeSmuME's CLI has no no$gba string channel, but it logs every write to an
 * unimplemented IO address together with the written value - which makes a
 * plain volatile store a machine-readable side channel there. The address is
 * unmapped everywhere, so hardware and other emulators ignore the writes.
 * Layout: tag byte ((half<<4)|code) in the top 8 bits, microseconds below;
 * codes 0-4 are profilerSection_type, 5 is busy, 6 is busy max.
 */
#define PROFILER_MMIO_SINK (*(volatile u32*)0x04FFFC00)

static void emitWord(int half, int code, u32 usec)
{
	PROFILER_MMIO_SINK=((u32)((half<<4)|code)<<24)|(usec&0xFFFFFF);
}

static void report(void)
{
	static const char* label[2]={"A","B"};
	char line[128];
	for(int h=0;h<2;h++)
	{
		const u32 busy=busySum[h]/PROFILER_REPORT_HALVES;
		int n=snprintf(line,sizeof(line),"PROF %s:",label[h]);
		#define APPEND(_name,_sec) \
			if(sectionSum[h][_sec]) \
			{ \
				const u32 us=ticksToUsec(sectionSum[h][_sec]/PROFILER_REPORT_HALVES); \
				n+=snprintf(line+n,sizeof(line)-n," %s %lu",_name,(unsigned long)us); \
				emitWord(h,_sec,us); \
			}
		APPEND("cpy",PROF_COPY);
		APPEND("pp",PROF_POSTPROC);
		APPEND("upd",PROF_UPDATES);
		APPEND("sub",PROF_SUBMIT);
		APPEND("phy",PROF_PHYSICS);
		#undef APPEND
		snprintf(line+n,sizeof(line)-n," | busy %luus max %lu (%lu%%)",
			(unsigned long)ticksToUsec(busy),
			(unsigned long)ticksToUsec(busyMax[h]),
			(unsigned long)(((u64)busy*100)/PROFILER_FRAME_TICKS));
		emitWord(h,5,ticksToUsec(busy));
		emitWord(h,6,ticksToUsec(busyMax[h]));
		nocashMessage(line);
	}
	memset(sectionSum,0,sizeof(sectionSum));
	memset(busySum,0,sizeof(busySum));
	memset(busyMax,0,sizeof(busyMax));
	memset(halves,0,sizeof(halves));
}

void profilerHalfEnd(void)
{
	if(!started)return;
	const u32 busy=cpuGetTiming();
	busySum[currentHalf]+=busy;
	if(busy>busyMax[currentHalf])busyMax[currentHalf]=busy;
	halves[currentHalf]++;

	if(halves[0]>=PROFILER_REPORT_HALVES && halves[1]>=PROFILER_REPORT_HALVES)report();
}

#endif
