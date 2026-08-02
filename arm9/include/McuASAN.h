/**
 * @file McuASAN.h
 * @brief Address sanitizer for bare-metal builds, by Erich Styger.
 *
 * A development aid, disabled by default (see @ref McuASANconfig.h). When
 * enabled it redirects @c malloc, @c free, @c memcpy and @c memset to checked
 * versions that surround every allocation with a poisoned red zone and hold
 * freed blocks in quarantine, so overruns and use-after-free are caught at the
 * moment they happen rather than as mysterious corruption several frames later.
 *
 * That makes it invaluable on this codebase, which does a lot of manual buffer
 * arithmetic in the level loader and the display list builder - but it costs
 * both memory and speed, so it is off in normal builds.
 *
 * Turn it on by defining @c McuASAN_CONFIG_IS_ENABLED to 1; main() then calls
 * @ref McuASAN_Init at startup.
 *
 * @warning The macro redirection below means any file that includes this
 *          header gets the checked allocator, whether it wants it or not.
 */

/*
 * Copyright (c) 2021, Erich Styger
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef MCUASAN_H_
#define MCUASAN_H_

#include <stddef.h>
#include "McuASANconfig.h"

#if McuASAN_CONFIG_IS_ENABLED && McuASAN_CONFIG_CHECK_MALLOC_FREE
  /* replace malloc and free calls */

  #undef memcpy
  #undef memset
  #define malloc __asan_malloc
  #define free   __asan_free
  #define memcpy __asan_memcpy
  #define memset __asan_memset
#endif

void * __asan_memcpy(void * restrict dest, const void * restrict src, size_t bytes); /**< @brief memcpy that checks both ranges are valid. */
void * __asan_memset(void * dest, int val, size_t bytes); /**< @brief memset that checks the range is valid. */
void *__asan_malloc(size_t size); /**< @brief malloc that adds a poisoned red zone either side of the block. */
void __asan_free(void *p);        /**< @brief free that poisons the block and holds it in quarantine. */
void McuASAN_Init(void);          /**< @brief Sets up the shadow map. Call once before any tracked allocation. */

#endif /* MCUASAN_H_ */
