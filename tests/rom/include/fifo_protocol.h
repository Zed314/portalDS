/*
 * The ARM9 -> ARM7 command protocol, as the test ROM understands it.
 *
 * These constants are deliberately a *copy* of the ones in
 * common/include/PIC.h rather than an include of it. Two reasons:
 *
 *  - PIC.h cannot be included on its own. It needs vect3D, portal_struct and
 *    AAR_struct, so reaching it means pulling in one CPU's whole world, and
 *    the test driver is neither CPU.
 *  - more importantly, this is a test of a contract between two separately
 *    built binaries. A sender that shares its definitions with the receiver
 *    cannot catch the two drifting apart, because both move together. Writing
 *    the encoding out by hand from the documented layout is the point: if
 *    PI7.c stops matching what PIC.h documents, these tests notice.
 *
 * If PIC.h changes on purpose, this file has to be updated to match, and that
 * edit is the moment to ask whether the change was meant to be a protocol
 * break.
 *
 * Command layout, from the block comment at the top of PIC.h:
 *
 *   31                    6 5        0
 *  +-----------------------+----------+
 *  |        object id      |  opcode  |
 *  +-----------------------+----------+
 *
 * sent on FIFO_USER_08, followed by a fixed number of argument words.
 * Replies come back on FIFO_USER_01 .. FIFO_USER_07.
 */

#ifndef PORTALDS_TEST_FIFO_PROTOCOL_H
#define PORTALDS_TEST_FIFO_PROTOCOL_H

#include <nds.h>

#define PI_SIGNAL_BITS (6)

/* Opcodes; must match message_type in common/include/PIC.h. */
#define CMD_START           (1)
#define CMD_PAUSE           (2)
#define CMD_STOP            (3)
#define CMD_RESET           (4)
#define CMD_ADDBOX          (5)
#define CMD_APPLYFORCE      (6)
#define CMD_ADDAAR          (7)
#define CMD_MAKEGRID        (8)
#define CMD_SETVELOCITY     (9)
#define CMD_UPDATEPLAYER    (10)
#define CMD_UPDATEPORTAL    (11)
#define CMD_ADDPLATFORM     (12)
#define CMD_UPDATEPLATFORM  (13)
#define CMD_TOGGLEPLATFORM  (14)
#define CMD_KILLBOX         (15)
#define CMD_RESETPORTALS    (16)
#define CMD_TOGGLEAAR       (17)
#define CMD_RESETALL        (18)

/* Rigid body pool size on the ARM7; ids at or above this are platforms. */
#define NUM_BODIES (8)

/* AAR normal, encoded as a bit per direction in the PI_ADDAAR normal word. */
#define AAR_NORMAL_NEG_X (1)
#define AAR_NORMAL_POS_X (2)
#define AAR_NORMAL_NEG_Y (4)
#define AAR_NORMAL_POS_Y (8)
#define AAR_NORMAL_NEG_Z (16)
#define AAR_NORMAL_POS_Z (32)

/* 20.12 fixed point, same as both CPUs. */
#define F32_ONE (1 << 12)
#define INT_TO_F32(n) ((n) << 12)

#endif
