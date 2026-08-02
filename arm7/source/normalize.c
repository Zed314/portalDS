/**
 * @file normalize.c
 * @brief Fast vector normalisation via a reciprocal square root.
 *
 * @ref normalize_arm7 is one of the hottest routines in the physics engine, so
 * it avoids a divide entirely. Instead of computing the length and dividing by
 * it, it computes 1/sqrt(len^2) directly and multiplies.
 *
 * The pipeline is:
 *  1. an inline @c smull / @c smlal chain squares and sums the three
 *     components at full 64 bit precision - the squared magnitude of a large
 *     vector genuinely does not fit in 32 bits;
 *  2. sqrt64_helper() normalises that into a fixed exponent range using
 *     @c clz, so the core routine always sees an operand of known scale;
 *  3. sqrt_core_asm() runs fourteen Newton-like refinement steps, again fully
 *     branch-free using conditional adds;
 *  4. each component is scaled by the result and the exponent shift is undone.
 *
 * The comments about overflow cancelling underflow in sqrt_core() are load
 * bearing: the intermediate @c y deliberately wraps, and the subtraction that
 * follows wraps back, so the final result is correct in modular arithmetic
 * even though the intermediate is not.
 *
 * @see math.h for the @ref normalize wrapper the rest of the engine calls.
 */

//Copyright (C) 2026 Dominik Kurz

//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 3 of the License, or (at your option) any later version.

//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//Lesser General Public License for more details.

//You should have received a copy of the GNU Lesser General Public License
//along with this program; if not, write to the Free Software Foundation,
//Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


#include <nds.h>
#include <stdint.h>
ARM_CODE static uint32_t sqrt_core(uint32_t x, uint32_t y)
{
    x>>=1;
    uint32_t t=x+(x>>1);
    if (t < (1u<<31)){ //first iteration is special cased
        t+=t>>1;
        if (t < (1u<<31)) {
            x=t;
            y+=y>>1;
        }
    }
    for (uint32_t i =2; i<14; i+=1){
        uint32_t t=x+(x>>i);
        t+=t>>i;
        if (t < (1u<<31)) {
            x=t;
            y+=y>>i;
        }
    }
    uint32_t hi = ((uint64_t)(x)*(y)  )>>32;
    y+=y>>1; //this can overflow
    return ((y)-(hi) ); //but then this underflows so they cancel
}

ARM_CODE static uint32_t sqrt_core_asm(uint32_t x, uint32_t y)
{
    for (uint32_t i =1; i<14; i+=1){
        asm(".syntax unified\n\t"
            "adds r12, %[x], %[x], lsr %[i]\n\t"
            "addscc r12, r12, r12, lsr %[i]\n\t"
            "movcc %[x], r12\n\t"
            "addcc %[y], %[y], %[y], lsr %[i]"
        : [x]"+r"(x), [y]"+r"(y)  //output
        : [i]"Ir"(i)//input
        : "r12","cc"//clobber
        );
    }
    uint32_t hi = ((uint64_t)(x)*(y))>>32;
    y+=y>>1; //this can overflow
    return ((y)-(hi>>1)); //but then this underflows so they cancel
}


uint32_t rsqrt1616(uint32_t m)
{
//input: 16.16 number
    int clz=__builtin_clz(m);
    int ilog2=31-clz;
    ilog2>>=1;
    ilog2<<=1;
    int shift=30-ilog2;
    m<<=shift;
    int shift2=22-(shift>>1);
    uint32_t result=sqrt_core_asm(m, 1<<30);
    return shift2>=0 ? result>>shift2 : result<<-shift2;
//output: 16.16 number
}

ARM_CODE static inline int32_t sqrt64_helper(uint64_t m, int * exp)
{
    int clz=__builtin_clzll(m);
    int ilog2=63-clz;
    ilog2>>=1;
    ilog2<<=1;
    int shift=30-ilog2;
    m= shift>=0 ? m<<shift : m>>-shift;
    int shift2=22-(shift>>1);
    *exp=shift2;
    return sqrt_core_asm(m, 1<<30);
}

ARM_CODE __attribute__((noinline)) void normalize_arm7(int32_t * a)
{
    register uint32_t Lo;
    register uint32_t Hi;
    asm (
        ".syntax unified \n\t"
        "ldm %[a], {r1,r2,r3} \n\t"
        "smull %[Lo],%[Hi], r1, r1 \n\t"
        "smlal %[Lo],%[Hi], r2, r2 \n\t"
        "smlal %[Lo],%[Hi], r3, r3 \n\t" :
        [Lo]"=r"(Lo), [Hi]"=r"(Hi) :
        [a]"r"(a), "m"(*(int32_t (*)[3]) a) :
        "r1", "r2", "r3"
    );
    uint64_t squared_magnitude = Lo + ((uint64_t)Hi << 32);
    if (__builtin_expect(squared_magnitude==0, 0))
        return;
    int exp;
    uint32_t res=sqrt64_helper(squared_magnitude, &exp);
    exp+=12;
    for(int i=0; i<3;i++)
    {
        int32_t t=a[i];
        uint32_t tu=t;
        if (t<0)
            tu=-tu;

        uint64_t prod=(uint64_t)tu*res;
        prod= exp >=0 ? prod>>exp : prod<<-exp;
        int32_t sprod=prod;
        if (t<0)
            sprod=-sprod;

        a[i]=sprod;
    }
    return;
}
