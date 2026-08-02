/**
 * @file state.c
 * @brief The top-level state machine.
 *
 * Implements @ref state.h. There is very little to it: @ref currentState is
 * what main() is running, @c nextState is what it will run after the current
 * one tears itself down, and @ref applyState moves one to the other.
 *
 * The deferred switch is the whole point. Game logic calls @ref changeState
 * from wherever it happens to notice the level is over, and nothing changes
 * until control has unwound back to main() - so no state is ever destroyed
 * while its own code is still on the stack.
 *
 * @ref applyState also re-points the vblank interrupt at the incoming state's
 * handler and resets the allocation tracker, which is what gives each state a
 * clean heap to work with.
 */

#include "common/general.h"

//static u8 state_id;

extern state_struct menuState;

state_struct *currentState= &menuState;
static state_struct *nextState= &menuState;

state_struct* getCurrentState(void)
{
	return currentState;
}

void changeState(state_struct* s)
{

    if (s==NULL)
        return;
    if (currentState==NULL)
        return;
    currentState->used=0;
    nextState=s;
    return;
}

#if 0
void createState(state_struct* s, function i, function f, function k, function vbl)
{
	s->init=(function)i;
	s->frame=(function)f;
	s->kill=(function)k;
	s->vbl=(function)vbl;

	s->id=state_id;
	s->mc_id=0;
	state_id++;
}
#endif

void setState(state_struct* s)
{
    if (!s)
        return;
    currentState=s;
    currentState->used=1;
}

void applyState()
{
	currentState=nextState;
	currentState->used=1;
	currentState->mc_id=0;
	initMalloc();
	irqSet(IRQ_VBLANK, currentState->vbl);
}
