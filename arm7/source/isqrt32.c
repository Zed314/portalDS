/**
 * @file isqrt32.c
 * @brief Branch-free 32 bit integer square root in ARM assembly.
 *
 * Computes a root by the classic restoring shift-and-subtract method: sixteen
 * fixed iterations, each testing one bit pair of the result. The whole thing is
 * written with conditional ARM instructions (@c subcs, @c adc) so there is not
 * a single branch in the loop body, which matters a great deal on the ARM7's
 * shallow pipeline and complete absence of a divide instruction.
 *
 * The rotate-based addressing (@c ror @c %[i]) is what lets one instruction
 * sequence handle every bit position without a shift table.
 *
 * Called via @ref sqrtv in math.c, which adds a refinement step for large
 * inputs.
 */

// SPDX-License-Identifier: Zlib
//
// Copyright (C) 2025 Dominik Kurz
#include <nds.h>
#include <stdint.h>

ARM_CODE uint32_t isqrt_asm(uint32_t r0)
{
    uint32_t r2=(3u<<30);
    uint32_t r1=(1u<<30);
    for (int i =0; i<(16*2); i+=2)
    {
        asm(".syntax unified\n\t"
            "cmp %[r0], %[r1], ror %[i]\n\t"
            "subcs %[r0], %[r0], %[r1], ror %[i]\n\t"
            "adc %[r1], %[r2], %[r1], lsl #1"
        : [r0]"+r"(r0), [r1]"+r"(r1)  //output
        : [r2]"r"(r2),[i]"Ir"(i)//input
        :"cc"//clobber
        );
    }
    return (r1<<2)>>2;
}
