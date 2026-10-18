/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Vertex groups, bounding polygons and per-vertex lighting.
===========================================================================
*/

#include "flare/vertexgroup.h"
#include "flare/print.h"
#include "flare/geometry.h"

#define NUM_BOUNDS_AXES 3
#define NUM_BOUNDS_FACES 6
#define NUM_FACE_CORNERS 4

int maxVertexGroups = 0;
int numVertexGroups = 0;
VertexGroup *vertexGroupPool = 0;

/*
=============
CountVertexGroup
=============
*/
void CountVertexGroup(int count)
{
    maxVertexGroups++;
    CountLitVertices(count);
}

/*
=============
ForEachVertexGroup
=============
*/
void ForEachVertexGroup(void (*callback)(VertexGroup *))
{
    int i;
    for (i = 0; i < numVertexGroups; i++)
        callback(&vertexGroupPool[i]);
}

/*
=============
Vector3d::operator[]

Return a reference to the component at index so the caller can change it.
Indices 0, 1 and 2 select x, y and z.
=============
*/
inline double &Vector3d::operator[](int index)
{
    return (&x)[index];
}

/*
=============
Bounds::Clear
=============
*/
inline void Bounds::Clear(void)
{
    points[0][0] = FLT_MAX;
    points[0][1] = FLT_MAX;
    points[0][2] = FLT_MAX;
    points[1][0] = -FLT_MAX;
    points[1][1] = -FLT_MAX;
    points[1][2] = -FLT_MAX;
}

/*
=============
CreateVertexGroup
=============
*/
VertexGroup *CreateVertexGroup(const Vector3d *positions, const Vector3d *normals,
                               const Color3f *colors, int count)
{
    VertexGroup *group;
    Vector3d points[NUM_FACE_CORNERS];
    Polygon *poly;
    int i, face, axis, side, nextAxis, lastAxis;

    if (!vertexGroupPool)
    {
        vertexGroupPool = new VertexGroup[maxVertexGroups];
        if (!vertexGroupPool)
            Q3Map_Error("Allocation failed on vertex group pool (%i bytes) - too many models?\n",
                       maxVertexGroups * sizeof(VertexGroup));
    }
    group = &vertexGroupPool[numVertexGroups++];
    group->numVerts = count;
    group->verts = AllocLitVertices(count);
    group->Clear();

    for (i = 0; i < count; i++)
    {
        group->AddPoint(positions[i]);
        group->verts[i].position = positions[i];
        group->verts[i].normal = normals[i];
        group->verts[i].color = colors[i];
    }

    /* Build a rectangular polygon on each of the six bounds faces. */
    group->numPolys = 0;
    for (face = 0; face < NUM_BOUNDS_FACES; face++)
    {
        /* Consecutive faces use the minimum and maximum of one axis. */
        side = face & 1;
        const int oppositeSide = side ^ 1;
        axis = face >> 1;
        nextAxis = (axis + 1) % NUM_BOUNDS_AXES;
        lastAxis = NUM_BOUNDS_AXES - nextAxis - axis;

        points[0][axis] = group->points[side][axis];
        points[1][axis] = group->points[side][axis];
        points[2][axis] = group->points[side][axis];
        points[3][axis] = group->points[side][axis];
        points[0][nextAxis] = group->points[side][nextAxis];
        points[1][nextAxis] = group->points[side][nextAxis];
        points[2][nextAxis] = group->points[oppositeSide][nextAxis];
        points[3][nextAxis] = group->points[oppositeSide][nextAxis];
        points[0][lastAxis] = group->points[1][lastAxis];
        points[1][lastAxis] = group->points[0][lastAxis];
        points[2][lastAxis] = group->points[0][lastAxis];
        points[3][lastAxis] = group->points[1][lastAxis];

        poly = CreatePoly(POLYFLAG_MODEL, NUM_FACE_CORNERS, points, 0, 0, 0, -1, -1, false);
        if (poly)
            group->polys[group->numPolys++] = poly;
    }
    return group;
}

/*
=============
VertexGroup::ClearLighting
=============
*/
inline void VertexGroup::ClearLighting(void)
{
    int i;
    for (i = 0; i < numVerts; i++)
    {
        verts[i].color.Clear();
    }
}

/*
=============
ColorRGB::Clear

Set the red, green and blue channels to zero.
=============
*/
inline void ColorRGB::Clear()
{
    r = 0;
    g = 0;
    b = 0;
}

/*
=============
ClearVertexGroupLighting
=============
*/
void ClearVertexGroupLighting(void)
{
    int i;
    for (i = 0; i < numVertexGroups; i++)
        vertexGroupPool[i].ClearLighting();
}
