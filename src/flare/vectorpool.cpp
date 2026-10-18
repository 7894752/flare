/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Reusable vector storage for polygon points and normals.
===========================================================================
*/
#include "flare/mathlib.h"
#include "flare/print.h"

/* Freed vector storage holds the free-list links. */
struct FreeVectorBlock
{
    int count;
    FreeVectorBlock *next;
};


Vector3d *vectorPool = 0;
int vectorPoolCapacity = 0;
int vectorPoolHighWater = 0;
int vectorPoolUsed = 0;
FreeVectorBlock *freeVectorBlocks = 0;
int numFreeVectorBlocks = 0;
int maxFreeVectorBlocks = 0;
bool vectorPoolPermanent = false;

/*
=============
AllocVectors

Reuse an exact-size free block before extending the occupied pool.
=============
*/
Vector3d *AllocVectors( int count )
{
    Vector3d *v;
    FreeVectorBlock *free;
    FreeVectorBlock **prev;
    for ( prev = &freeVectorBlocks, free = *prev; free; prev = &free->next, free = *prev )
    {
        if ( free->count == count )
        {
            numFreeVectorBlocks--;
            *prev = free->next;
            return (Vector3d *)free;
        }
    }
    if ( vectorPoolUsed + count > vectorPoolCapacity )
        Q3Map_Error( "maxverts(%i) exceeded", vectorPoolCapacity );
    v = vectorPool + vectorPoolUsed;
    vectorPoolUsed += count;
    if ( vectorPoolHighWater < vectorPoolUsed )
        vectorPoolHighWater = vectorPoolUsed;
    return v;
}

/*
=============
Vector3d::Free
=============
*/
void Vector3d::Free( int count )
{
    FreeVectorBlock *block;
    /* Tail allocations can be reclaimed without a free-list entry. */
    if ( this + count == vectorPool + vectorPoolUsed )
    {
        vectorPoolUsed -= count;
    }
    else
    {
        block = (FreeVectorBlock *)this;
        block->count = count;
        block->next = freeVectorBlocks;
        freeVectorBlocks = block;
        numFreeVectorBlocks++;
        if ( maxFreeVectorBlocks < numFreeVectorBlocks )
            maxFreeVectorBlocks = numFreeVectorBlocks;
    }
}

/*
=============
MakeVectorPoolPermanent
=============
*/
void MakeVectorPoolPermanent()
{
    Q3Map_Verbose( "ending free vector chain length was %i\n", numFreeVectorBlocks );
    Q3Map_Verbose( "max free vector chain length was %i\n", maxFreeVectorBlocks );
    vectorPoolPermanent = true;
    freeVectorBlocks = 0;
    numFreeVectorBlocks = 0;
}

/*
=============
InitVectorPool
=============
*/
void InitVectorPool( int count )
{
    vectorPool = new Vector3d[count];
    if ( !vectorPool )
        Q3Map_Error( "Couldn't allocate vector pool (%i bytes) - maxverts too big?\n", count * sizeof( Vector3d ) );
    vectorPoolCapacity = count;
    vectorPoolUsed = 0;
}

/*
=============
GetVectorPoolUsed
=============
*/
int GetVectorPoolUsed()
{
    return vectorPoolUsed;
}

/*
=============
SetVectorPoolUsed
=============
*/
void SetVectorPoolUsed( int count )
{
    vectorPoolUsed = count;
}

/*
=============
RemovePoolVectors
=============
*/
void RemovePoolVectors( int first, int count )
{
    int i;
    vectorPoolUsed -= count;
    for ( i = first; i < vectorPoolUsed; i++ )
        vectorPool[i] = vectorPool[i + count];
}
