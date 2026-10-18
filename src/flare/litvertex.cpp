/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Storage for lit model vertices.
===========================================================================
*/

#include "flare/vertexgroup.h"
#include "flare/print.h"

int maxLitVertices = 0;
int numLitVertices = 0;
LitVertex *litVertexPool = 0;

/*
=============
CountLitVertices
=============
*/
void CountLitVertices(int count)
{
    maxLitVertices += count;
}

/*
=============
LitVertex::LitVertex
=============
*/
LitVertex::LitVertex(void)
{
}

/*
=============
AllocLitVertices
=============
*/
LitVertex *AllocLitVertices(int count)
{
    LitVertex *verts;

    if (!litVertexPool)
    {
        litVertexPool = new LitVertex[maxLitVertices];
        if (!litVertexPool)
            Q3Map_Error("Allocation failed for lit vertex pool (%i bytes)\n",
                       maxLitVertices * sizeof(LitVertex));
    }
    verts = &litVertexPool[numLitVertices];
    numLitVertices += count;
    return verts;
}
