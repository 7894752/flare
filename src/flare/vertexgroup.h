/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Lit vertices, vertex groups and shared lighting accumulators.
===========================================================================
*/
#ifndef FLARE_VERTEXGROUP_H
#define FLARE_VERTEXGROUP_H

#include "flare/mathlib.h"
#include "flare/geometry.h"

struct LitVertex
{
    Vector3d position;
    Vector3d normal;
    Color3f color;
    float weight;
    LitVertex(void);
};

struct VertexGroup : Bounds
{
    int numVerts;
    LitVertex *verts;
    int numPolys;
    Polygon *polys[6];
    void ClearLighting(void);
};

/* litvertex.cpp */
void CountLitVertices(int count);
LitVertex *AllocLitVertices(int count);

/* vertexgroup.cpp */
void CountVertexGroup(int count);
void ForEachVertexGroup(void (*callback)(VertexGroup *));
VertexGroup *CreateVertexGroup(const Vector3d *positions, const Vector3d *normals,
                               const Color3f *colors, int count);
void ClearVertexGroupLighting(void);

/* litvertex.cpp */
extern int maxLitVertices;
extern int numLitVertices;
extern LitVertex *litVertexPool;

/* vertexgroup.cpp */
extern int maxVertexGroups;
extern int numVertexGroups;
extern VertexGroup *vertexGroupPool;

#endif
