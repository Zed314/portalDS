/*
 * A one way character channel from inside the emulator to the harness' stdout.
 *
 * The test ROM has to report results to whatever is running it, and on DeSmuME
 * there is no ordinary way out: nocashMessage() is not forwarded to stdout, and
 * writes to an emulated filesystem are never flushed back to the host (both
 * verified - the ROM side of the FAT write succeeds and the file simply never
 * appears).
 *
 * What DeSmuME does print, for every access, is a warning about writes to
 * undefined I/O registers:
 *
 *   MMU9 write16 to undefined register 04000058h = 00000041h (PC:...)
 *
 * So one 16 bit write to an unused register is one byte of output, in order and
 * immediately. That is enough to build a character device on, and
 * tests/rom/run.sh reassembles the bytes into ordinary text.
 *
 * This is deliberately the only emulator-specific thing in the ROM. If a better
 * channel turns up - a newer DeSmuME, melonDS, a GDB script - only this header
 * changes; nothing in main.c knows how output leaves the machine.
 */

#ifndef PORTALDS_DSPROBE_H
#define PORTALDS_DSPROBE_H

#include <nds.h>
#include <stdarg.h>
#include <stdio.h>

/*
 * An unused register in the 2D engine A block. Reads and writes here do nothing
 * on real hardware, which is what makes it safe to abuse as a port.
 */
#define DSPROBE_PORT (*(vu16 *)0x04000058)

/** Emits one byte. */
static inline void probeChar(char c)
{
    DSPROBE_PORT = (u16)(unsigned char)c;
}

/** Emits a string. */
static inline void probeStr(const char *s)
{
    while (*s)
        probeChar(*s++);
}

/**
 * Emits a formatted line. Kept small deliberately - this runs on a DS with a
 * 128 byte buffer and no reason for anything longer.
 */
__attribute__((format(printf, 1, 2)))
static inline void probePrintf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    probeStr(buf);
}

#endif
