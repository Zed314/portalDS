/**
 * @file state.h
 * @brief The top-level state machine: menu, game and editor.
 *
 * The program is always in exactly one of three states, each described by a
 * @ref state_struct holding four callbacks. main() runs them in a fixed cycle:
 *
 * @code
 * while(1)
 * {
 *     currentState->init();               // build the world
 *     while(currentState->used)
 *         currentState->frame();          // one frame, until someone calls changeState
 *     currentState->kill();               // tear it down
 *     applyState();                       // switch to the pending state
 * }
 * @endcode
 *
 * A state never switches immediately. @ref changeState only records the state
 * to move to and clears state_struct::used, which drops out of the frame loop;
 * the actual switch happens in @ref applyState once @c kill has run. That is
 * what makes it safe to call @ref changeState from deep inside game logic.
 *
 * @par Why states own their allocations
 * @ref applyState calls initMalloc(), and state_struct::mc_id counts the
 * allocations made since. Everything a state allocates through @ref alloc is
 * freed wholesale by @ref freeState when it ends, so a mode can leak freely
 * during its lifetime without fragmenting the heap for the next one. See
 * @ref memory.h.
 */

#ifndef __STATE9__
#define __STATE9__


typedef void(*function)(); /**< @brief Signature of a state callback: no arguments, no result. */

/**
 * @brief One top-level mode of the program.
 */
typedef struct{
	function init;  /**< Called once when the state is entered; builds the world. */
	function frame; /**< Called every frame while the state is current. */
	function kill;  /**< Called once when the state ends; must release anything @c init took. */
	function vbl;   /**< Installed as the vblank interrupt handler for the duration of the state. */
	u16 mc_id;      /**< Number of allocations made through @ref alloc since the state started. */
	u8 id;          /**< Stable identifier: 0 game, 1 menu, 2 editor. */
	bool used;      /**< While true the frame loop keeps running; cleared by @ref changeState. */
}state_struct;

extern state_struct* currentState; /**< The state whose callbacks are running right now. */

/**
 * @brief Commits the pending state switch.
 *
 * Makes the state recorded by @ref changeState current, resets the allocation
 * tracker and points the vblank interrupt at the new state's handler. Called
 * from main() after the outgoing state's @c kill has returned.
 */
void applyState(void);

/** @brief Legacy hardware setup hook. No longer implemented. */
void initHardware(void);

/** @brief Returns the currently running state. */
state_struct* getCurrentState(void);

/**
 * @brief Makes a state current immediately, without running any callbacks.
 *
 * Bypasses the normal kill/init cycle, so it is only safe during startup.
 * Prefer @ref changeState.
 *
 * @param s state to install; ignored if NULL.
 */
void setState(state_struct* s);

/**
 * @brief Requests a switch to another state at the end of the current frame.
 *
 * Clears the current state's state_struct::used flag, which ends the frame
 * loop, and records @p s as the state to enter next. The switch itself happens
 * in @ref applyState.
 *
 * @param s state to switch to; ignored if NULL.
 */
void changeState(state_struct* s);

/**
 * @brief Fills in a state's callbacks and assigns it an id.
 *
 * @note Currently compiled out - the three states are initialised statically
 *       in main.c instead.
 */
void createState(state_struct* s, function i, function f, function k, function vbl);

#endif
