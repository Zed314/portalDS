/**
 * @file memory.h
 * @brief State-scoped heap allocation.
 *
 * A thin bookkeeping layer over @c malloc. Every allocation made through
 * @ref alloc is recorded in a fixed table, and @ref freeState releases the
 * whole table at once.
 *
 * The point is lifetime, not speed: the game, menu and editor each build a lot
 * of structures on entry and want all of them gone on exit. Tying allocations
 * to the owning @ref state_struct means a mode can allocate freely without
 * anyone having to track down every last pointer in its @c kill callback, and
 * the heap comes back completely unfragmented for the next mode.
 *
 * @warning This is not a general purpose allocator: there is no individual
 *          free, the table is a fixed @ref MAX_MALLOC entries, and blocks are
 *          released in reverse order of allocation.
 */

#ifndef __MEMORY9__
#define __MEMORY9__

#define MAX_MALLOC 512 /**< Maximum number of live tracked allocations. */

/** @brief Returns the current stack pointer. Used when diagnosing stack/heap collisions. */
void *GetStackPointer();

/**
 * @brief Allocates memory owned by a state.
 *
 * Logs the pointer in the tracking table and bumps state_struct::mc_id.
 * Failures are reported to the no$gba debug window rather than being fatal, so
 * callers still need to check the result.
 *
 * @param size  bytes to allocate.
 * @param state owning state, or NULL for the current one.
 * @return the block, or NULL if the heap or the tracking table is exhausted.
 */
void* alloc(size_t size, state_struct* state);

/**
 * @brief Resizes a tracked allocation.
 *
 * @warning Not a real realloc: the old block is freed and a new one allocated
 *          without copying, so the previous contents are lost. Callers rely on
 *          this only for buffers they are about to overwrite anyway.
 *
 * @param p    block previously returned by @ref alloc.
 * @param size new size in bytes.
 * @param s    owning state, or NULL for the current one.
 * @return the new block, or NULL if @p p was not tracked or the heap is full.
 */
void* reAlloc(void* p, size_t size, state_struct* s);

/**
 * @brief Frees every allocation made by a state.
 *
 * Releases in reverse order of allocation, which keeps the heap contiguous.
 *
 * @param state state to clean up, or NULL for the current one.
 */
void freeState(state_struct* state);

/**
 * @brief Empties the tracking table without freeing anything.
 *
 * Called by @ref applyState on every state switch, on the assumption that the
 * outgoing state has already run @ref freeState.
 */
void initMalloc();

#endif
