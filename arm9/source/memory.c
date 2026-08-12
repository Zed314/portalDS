/**
 * @file memory.c
 * @brief State-scoped allocation tracking.
 *
 * Implements @ref memory.h. A flat table of at most @ref MAX_MALLOC pointers,
 * so that @ref freeState can release everything a state took in one call.
 *
 * Two things to be aware of when reading this:
 *  - @ref reAlloc does not preserve contents. It frees and re-allocates, so
 *    callers must treat the returned block as uninitialised;
 *  - @ref freeState walks the table backwards, releasing the most recent
 *    allocation first. With newlib's allocator that lets the heap top come
 *    straight back down instead of leaving a fragmented gap.
 *
 * Allocation failures are logged through @ref NOGBA rather than being fatal,
 * which is why the callers still have to check for NULL.
 */

#include "common/general.h"
#include <errno.h>

static void *mallocList[MAX_MALLOC];

void initMalloc()
{
	int i;

	for(i=0;i<MAX_MALLOC;i++)
	{
		mallocList[i]=NULL;
	}
}

void* alloc(size_t size, state_struct* s)
{
	int i;
	if(!s)s=getCurrentState();
	for(i=0;i<MAX_MALLOC;i++)
	{
		if(mallocList[i]==NULL)
		{
			mallocList[i]=malloc(size);
			if(mallocList[i]==NULL)NOGBA("MALLOC ERROR ! DUCK FOR COVER : %d, %s (%d,%d)",errno,strerror(errno),DS_UsedMem()/1024,DS_FreeMem()/1024);
			s->mc_id++;
			return mallocList[i];
		}
	}
	NOGBA("malloc error !");
	return NULL;
}

void* reAlloc(void* p, size_t size, state_struct* s)
{
	int i;
	if(!s)s=getCurrentState();
	for(i=0;i<MAX_MALLOC;i++)
	{
		if(mallocList[i]==p)
		{
			free(p);
			mallocList[i]=malloc(size);
			if(mallocList[i]==NULL)NOGBA("MALLOC ERROR ! DUCK FOR COVER : %d, %s (%d,%d)",errno,strerror(errno),DS_UsedMem()/1024,DS_FreeMem()/1024);
			return mallocList[i];
		}
	}
	NOGBA("reAlloc error !");
	return NULL;
}

void freeState(state_struct* s)
{
	if(!s)s=getCurrentState();
	if(s->mc_id>0)
	{
		int i;
		for(i=MAX_MALLOC-1;i>=0;i--)
		{
			if(mallocList[i]!=NULL)
			{
				free(mallocList[i]);
				mallocList[i]=NULL;
				//NOGBA("%d, %p\n",i,GetStackPointer());
			}
		}
		s->mc_id=0;
	}
}
