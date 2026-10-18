/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/* Polygon and bounds clipping, intersection and plane tests. */
bool NearlyEqual(double a, double b);
#include "flare/math_types.h"
#include "flare/geometry.h"
/*
=============
Vector3d::operator-=

Subtract the corresponding components of v from this vector.
=============
*/
inline void Vector3d::operator-=(const Vector3d &v)
{
    x -= v.x;
    y -= v.y;
    z -= v.z;
}
#include <string.h>
typedef float vec2_t[2];
#include "geometry_types.h"


/*
=============
Vector3d::operator[]

Return the component at index, where 0, 1 and 2 select x, y and z.
=============
*/
inline double Vector3d::operator[](int index) const
{
    return (&x)[index];
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

#include "polygon.h"
Polygon *AllocSplitPolygon(const Polygon *source, int numPoints);

#include "flare/vector.h"

#define Q_PI 3.14159265358979323846f

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2
#define SIDE_CROSS 3

/*
=============
SplitPolygon

A one-sided result aliases the input polygon; a crossing allocates both halves.
Coplanar input returns SIDE_ON with both outputs empty.
=============
*/
int SplitPolygon(Polygon *poly, const Plane &plane, Polygon **front, Polygon **back)
{
    int prev, j;
    int sides[MAX_VERTS_PER_POLY];
    double dist[MAX_VERTS_PER_POLY];
    double planeEpsilon = fabs(plane.dist) * NORMAL_EPSILON < EQUAL_EPSILON
                    ? EQUAL_EPSILON : fabs(plane.dist) * NORMAL_EPSILON;
    int iPointCounts[2];
    int extraPoints;
    iPointCounts[SIDE_FRONT] = 0;
    iPointCounts[SIDE_BACK] = 0;
    for (prev = 0; prev < poly->numPoints; prev++)
    {
        dist[prev] = poly->points[prev] * plane.normal - plane.dist;
        sides[prev] = SIDE_ON;
        if (-planeEpsilon > dist[prev])
        {
            sides[prev] = SIDE_BACK;
            iPointCounts[SIDE_BACK]++;
        }
        else if (dist[prev] > planeEpsilon)
        {
            sides[prev] = SIDE_FRONT;
            iPointCounts[SIDE_FRONT]++;
        }
    }
    if (!iPointCounts[SIDE_BACK])
    {
        *back = 0;
        if (!iPointCounts[SIDE_FRONT])
        {
            *front = 0;
            return SIDE_ON;
        }
        *front = poly;
        return SIDE_FRONT;
    }
    if (!iPointCounts[SIDE_FRONT])
    {
        *front = 0;
        *back = poly;
        return SIDE_BACK;
    }
    extraPoints = 0;
    if (iPointCounts[SIDE_FRONT] + iPointCounts[SIDE_BACK] == poly->numPoints)
        extraPoints = 2;
    else
    {
        prev = poly->numPoints - 1;
        for (j = 0; j < poly->numPoints; prev = j, j++)
        {
            if (sides[prev] == SIDE_ON || (sides[j] != SIDE_ON && sides[j] != sides[prev]))
                extraPoints++;
        }
    }
    *back = AllocSplitPolygon(poly, iPointCounts[SIDE_BACK] + extraPoints);
    *front = AllocSplitPolygon(poly, iPointCounts[SIDE_FRONT] + extraPoints);
    iPointCounts[SIDE_FRONT] = 0;
    iPointCounts[SIDE_BACK] = 0;
    prev = poly->numPoints - 1;
    for (j = 0; j < poly->numPoints; prev = j, j++)
    {
        if (sides[prev] == SIDE_ON)
        {
            (*front)->points[iPointCounts[SIDE_FRONT]++] = poly->points[prev];
            (*back)->points[iPointCounts[SIDE_BACK]++] = poly->points[prev];
            continue;
        }
        if (sides[prev] == SIDE_FRONT)
            (*front)->points[iPointCounts[SIDE_FRONT]++] = poly->points[prev];
        else if (sides[prev] == SIDE_BACK)
            (*back)->points[iPointCounts[SIDE_BACK]++] = poly->points[prev];
        if (sides[j] == SIDE_ON || sides[j] == sides[prev])
            continue;
        double fraction = dist[prev] / (dist[prev] - dist[j]);
        Vector3d midpoint = fraction * (poly->points[j] - poly->points[prev]) + poly->points[prev];
        (*front)->points[iPointCounts[SIDE_FRONT]] = midpoint;
        (*back)->points[iPointCounts[SIDE_BACK]] = midpoint;
        if (poly->normals)
        {
            Vector3d normal = fraction * (poly->normals[j] - poly->normals[prev]) + poly->normals[prev];
            normal.NormalizeLength();
            (*front)->normals[iPointCounts[SIDE_FRONT]] = normal;
            (*back)->normals[iPointCounts[SIDE_BACK]] = normal;
        }
        iPointCounts[SIDE_FRONT]++;
        iPointCounts[SIDE_BACK]++;
    }
    (*front)->UpdateLightmapBounds();
    (*back)->UpdateLightmapBounds();
    return SIDE_CROSS;
}

/*
=============
SplitPolygonEpsilon

Treat nearly coplanar faces as on-plane before splitting the edges.
=============
*/
int SplitPolygonEpsilon(Polygon *poly, const Plane &plane, Polygon **front, Polygon **back)
{
    int prev, iPoint;
    int sides[MAX_VERTS_PER_POLY];
    double dist[MAX_VERTS_PER_POLY];
    double dot = poly->info->plane.normal * plane.normal;
    if (1.0 - EQUAL_EPSILON < dot)
    {
        if (NearlyEqual(poly->GetInfo()->plane.dist, plane.dist))
        {
            *front = 0;
            *back = 0;
            return SIDE_ON;
        }
    }
    else if (-(1.0 - EQUAL_EPSILON) > dot)
    {
        if (NearlyEqual(poly->GetInfo()->plane.dist, -plane.dist))
        {
            *front = 0;
            *back = 0;
            return SIDE_ON;
        }
    }
    double planeEpsilon = fabs(plane.dist) * NORMAL_EPSILON < EQUAL_EPSILON
                    ? EQUAL_EPSILON : fabs(plane.dist) * NORMAL_EPSILON;
    int numFrontVerts, backVerts;
    int prevNonzeroSide = SIDE_CROSS;
    int iNumSideChanges = 0;
    int extra;
    numFrontVerts = 0;
    backVerts = 0;
    for (prev = 0; prev < poly->numPoints; prev++)
    {
        dist[prev] = poly->points[prev] * plane.normal - plane.dist;
        sides[prev] = SIDE_ON;
        if (-planeEpsilon > dist[prev])
        {
            sides[prev] = SIDE_BACK;
            backVerts++;
            if (prevNonzeroSide != SIDE_BACK)
            {
                iNumSideChanges++;
                prevNonzeroSide = SIDE_BACK;
            }
        }
        else if (dist[prev] > planeEpsilon)
        {
            sides[prev] = SIDE_FRONT;
            numFrontVerts++;
            if (prevNonzeroSide != SIDE_FRONT)
            {
                iNumSideChanges++;
                prevNonzeroSide = SIDE_FRONT;
            }
        }
    }
    /* Count the first noncoplanar run and each subsequent side change.
       More than three runs cannot form one convex polygon per side. */
    if (iNumSideChanges > 3)
    {
        *front = 0;
        *back = 0;
        return SIDE_ON;
    }
    if (!backVerts)
    {
        *back = 0;
        if (!numFrontVerts)
        {
            *front = 0;
            return SIDE_ON;
        }
        *front = poly;
        return SIDE_FRONT;
    }
    if (!numFrontVerts)
    {
        *front = 0;
        *back = poly;
        return SIDE_BACK;
    }
    extra = 0;
    if (numFrontVerts + backVerts == poly->numPoints)
        extra = 2;
    else
    {
        prev = poly->numPoints - 1;
        for (iPoint = 0; iPoint < poly->numPoints; prev = iPoint, iPoint++)
        {
            if (sides[prev] == SIDE_ON || (sides[iPoint] != SIDE_ON && sides[iPoint] != sides[prev]))
                extra++;
        }
    }
    *back = AllocSplitPolygon(poly, backVerts + extra);
    *front = AllocSplitPolygon(poly, numFrontVerts + extra);
    numFrontVerts = 0;
    backVerts = 0;
    prev = poly->numPoints - 1;
    for (iPoint = 0; iPoint < poly->numPoints; prev = iPoint, iPoint++)
    {
        if (sides[prev] == SIDE_ON)
        {
            (*front)->points[numFrontVerts++] = poly->points[prev];
            (*back)->points[backVerts++] = poly->points[prev];
            continue;
        }
        if (sides[prev] == SIDE_FRONT)
            (*front)->points[numFrontVerts++] = poly->points[prev];
        else if (sides[prev] == SIDE_BACK)
            (*back)->points[backVerts++] = poly->points[prev];
        if (sides[iPoint] == SIDE_ON || sides[iPoint] == sides[prev])
            continue;
        double fraction = dist[prev] / (dist[prev] - dist[iPoint]);
        Vector3d mid;
        /* Keep axial intersections exactly on the plane. */
        int k = 2;
        do
        {
            if (plane.normal[k] == 1.0)
                mid[k] = plane.dist;
            else if (plane.normal[k] == -1.0)
                mid[k] = -plane.dist;
            else
                mid[k] = poly->points[prev][k]
                    + fraction * (poly->points[iPoint][k] - poly->points[prev][k]);
            k--;
        } while (k >= 0);
        (*front)->points[numFrontVerts] = mid;
        (*back)->points[backVerts] = mid;
        if (poly->normals)
        {
            Vector3d normal = fraction * (poly->normals[iPoint] - poly->normals[prev]) + poly->normals[prev];
            normal.NormalizeLength();
            (*front)->normals[numFrontVerts] = normal;
            (*back)->normals[backVerts] = normal;
        }
        numFrontVerts++;
        backVerts++;
    }
    (*front)->UpdateLightmapBounds();
    (*back)->UpdateLightmapBounds();
    return SIDE_CROSS;
}

/*
=============
ClipPolygonFront
=============
*/
Polygon *ClipPolygonFront(const Polygon *poly, const Plane &plane)
{
    int prev, j;
    int sides[MAX_VERTS_PER_POLY];
    double dist[MAX_VERTS_PER_POLY];
    double planeEpsilon;
    int iPointCounts[2];
    int extraPoints;
    Polygon *resultPoly;

    if (poly->bounds.IsInFrontOfPlane(plane))
        return (Polygon *)poly;
    if (poly->bounds.IsBehindPlane(plane))
        return 0;
    planeEpsilon = fabs(plane.dist) * NORMAL_EPSILON < EQUAL_EPSILON
            ? EQUAL_EPSILON : fabs(plane.dist) * NORMAL_EPSILON;
    iPointCounts[SIDE_FRONT] = 0;
    iPointCounts[SIDE_BACK] = 0;
    for (prev = 0; prev < poly->numPoints; prev++)
    {
        dist[prev] = poly->points[prev] * plane.normal - plane.dist;
        sides[prev] = SIDE_ON;
        if (-planeEpsilon > dist[prev])
        {
            sides[prev] = SIDE_BACK;
            iPointCounts[SIDE_BACK]++;
        }
        else if (dist[prev] > planeEpsilon)
        {
            sides[prev] = SIDE_FRONT;
            iPointCounts[SIDE_FRONT]++;
        }
    }
    if (!iPointCounts[SIDE_FRONT])
        return 0;
    if (!iPointCounts[SIDE_BACK])
        return (Polygon *)poly;
    extraPoints = 0;
    prev = poly->numPoints - 1;
    for (j = 0; j < poly->numPoints; prev = j, j++)
    {
        if (sides[prev] == SIDE_ON || (sides[j] != SIDE_ON && sides[j] != sides[prev]))
            extraPoints++;
    }
    resultPoly = AllocSplitPolygon(poly, iPointCounts[SIDE_FRONT] + extraPoints);
    iPointCounts[SIDE_FRONT] = 0;
    prev = poly->numPoints - 1;
    for (j = 0; j < poly->numPoints; prev = j, j++)
    {
        if (sides[prev] == SIDE_ON)
        {
            resultPoly->points[iPointCounts[SIDE_FRONT]++] = poly->points[prev];
            continue;
        }
        if (sides[prev] == SIDE_FRONT)
            resultPoly->points[iPointCounts[SIDE_FRONT]++] = poly->points[prev];
        if (sides[j] == SIDE_ON || sides[j] == sides[prev])
            continue;
        double fraction = dist[prev] / (dist[prev] - dist[j]);
        /* Keep axial intersections exactly on the plane. */
        int k = 2;
        do
        {
            if (plane.normal[k] == 1.0)
                resultPoly->points[iPointCounts[SIDE_FRONT]][k] = plane.dist;
            else if (plane.normal[k] == -1.0)
                resultPoly->points[iPointCounts[SIDE_FRONT]][k] = -plane.dist;
            else
                resultPoly->points[iPointCounts[SIDE_FRONT]][k] = poly->points[prev][k]
                    + fraction * (poly->points[j][k] - poly->points[prev][k]);
            k--;
        } while (k >= 0);
        if (poly->normals)
        {
            resultPoly->normals[iPointCounts[SIDE_FRONT]] = fraction * (poly->normals[j] - poly->normals[prev]) + poly->normals[prev];
            resultPoly->normals[iPointCounts[SIDE_FRONT]].NormalizeLength();
        }
        iPointCounts[SIDE_FRONT]++;
    }
    resultPoly->UpdateLightmapBounds();
    return resultPoly;
}

/*
=============
ClipPolygonBack
=============
*/
Polygon *ClipPolygonBack(const Polygon *poly, const Plane &plane)
{
    int prev, j;
    int sides[MAX_VERTS_PER_POLY];
    double dist[MAX_VERTS_PER_POLY];
    double planeEpsilon;
    int iPointCounts[2];
    int extraPoints;
    Polygon *resultPoly;

    if (poly->bounds.IsInFrontOfPlane(plane))
        return 0;
    if (poly->bounds.IsBehindPlane(plane))
        return (Polygon *)poly;
    planeEpsilon = fabs(plane.dist) * NORMAL_EPSILON < EQUAL_EPSILON
            ? EQUAL_EPSILON : fabs(plane.dist) * NORMAL_EPSILON;
    iPointCounts[SIDE_FRONT] = 0;
    iPointCounts[SIDE_BACK] = 0;
    for (prev = 0; prev < poly->numPoints; prev++)
    {
        dist[prev] = poly->points[prev] * plane.normal - plane.dist;
        sides[prev] = SIDE_ON;
        if (-planeEpsilon > dist[prev])
        {
            sides[prev] = SIDE_BACK;
            iPointCounts[SIDE_BACK]++;
        }
        else if (dist[prev] > planeEpsilon)
        {
            sides[prev] = SIDE_FRONT;
            iPointCounts[SIDE_FRONT]++;
        }
    }
    if (!iPointCounts[SIDE_BACK])
        return 0;
    if (!iPointCounts[SIDE_FRONT])
        return (Polygon *)poly;
    extraPoints = 0;
    prev = poly->numPoints - 1;
    for (j = 0; j < poly->numPoints; prev = j, j++)
    {
        if (sides[prev] == SIDE_ON || (sides[j] != SIDE_ON && sides[j] != sides[prev]))
            extraPoints++;
    }
    resultPoly = AllocSplitPolygon(poly, iPointCounts[SIDE_BACK] + extraPoints);
    iPointCounts[SIDE_BACK] = 0;
    prev = poly->numPoints - 1;
    for (j = 0; j < poly->numPoints; prev = j, j++)
    {
        if (sides[prev] == SIDE_ON)
        {
            resultPoly->points[iPointCounts[SIDE_BACK]++] = poly->points[prev];
            continue;
        }
        if (sides[prev] == SIDE_BACK)
            resultPoly->points[iPointCounts[SIDE_BACK]++] = poly->points[prev];
        if (sides[j] == SIDE_ON || sides[j] == sides[prev])
            continue;
        double fraction = dist[prev] / (dist[prev] - dist[j]);
        /* Keep axial intersections exactly on the plane. */
        int k = 2;
        do
        {
            if (plane.normal[k] == 1.0)
                resultPoly->points[iPointCounts[SIDE_BACK]][k] = plane.dist;
            else if (plane.normal[k] == -1.0)
                resultPoly->points[iPointCounts[SIDE_BACK]][k] = -plane.dist;
            else
                resultPoly->points[iPointCounts[SIDE_BACK]][k] = poly->points[prev][k]
                    + fraction * (poly->points[j][k] - poly->points[prev][k]);
            k--;
        } while (k >= 0);
        if (poly->normals)
        {
            resultPoly->normals[iPointCounts[SIDE_BACK]] = fraction * (poly->normals[j] - poly->normals[prev]) + poly->normals[prev];
            resultPoly->normals[iPointCounts[SIDE_BACK]].NormalizeLength();
        }
        iPointCounts[SIDE_BACK]++;
    }
    resultPoly->UpdateLightmapBounds();
    return resultPoly;
}

/*
=============
PolygonPlaneSide
=============
*/
int PolygonPlaneSide(const Polygon *poly, const Plane &plane)
{
    int i;
    if (poly->bounds.IsInFrontOfPlane(plane))
        return SIDE_FRONT;
    if (poly->bounds.IsBehindPlane(plane))
        return SIDE_BACK;
    bool front = false;
    bool back = false;
    for (i = 0; i < poly->numPoints; i++)
    {
        double distance = poly->points[i] * plane.normal - plane.dist;
        if (-EQUAL_EPSILON > distance)
        {
            if (front)
                return SIDE_CROSS;
            back = true;
        }
        else if (distance > EQUAL_EPSILON)
        {
            if (back)
                return SIDE_CROSS;
            front = true;
        }
    }
    if (front)
        return SIDE_FRONT;
    if (back)
        return SIDE_BACK;
    return SIDE_ON;
}

/*
=============
Bounds::IsInFrontOfPlane

Test whether the entire box lies strictly in front of the plane, using the
corner with the smallest projection onto the normal.
=============
*/
inline bool Bounds::IsInFrontOfPlane(const Plane &plane) const
{
    double distance = points[plane.sign[0]][0] * plane.normal.x
                    + points[plane.sign[1]][1] * plane.normal.y
                    + points[plane.sign[2]][2] * plane.normal.z;
    return distance > plane.dist ? true : false;
}

void Q3Map_Error(const char *format, ...);
void SwapInt(int *a, int *b);

#define Vector2Copy(a,b) ((b)[0] = (a)[0], (b)[1] = (a)[1])

/*
=============
PolygonArea2D

Return twice the signed area; clockwise polygons have positive area.
=============
*/
float PolygonArea2D(const vec2_t *points, int count)
{
    int i = count - 1;
    float area = points[0][0] * (points[i][1] - points[1][1])
               + points[i][0] * (points[i-1][1] - points[0][1]);
    for (i = 1; i < count - 1; i++)
        area += points[i][0] * (points[i-1][1] - points[i+1][1]);
    return area;
}

/*
=============
Bounds::IsBehindPlane

Test whether the entire box lies strictly behind the plane, using the
corner with the largest projection onto the normal.
=============
*/
inline bool Bounds::IsBehindPlane(const Plane &plane) const
{
    double distance = points[1 - plane.sign[0]][0] * plane.normal.x
                    + points[1 - plane.sign[1]][1] * plane.normal.y
                    + points[1 - plane.sign[2]][2] * plane.normal.z;
    return distance < plane.dist ? true : false;
}

/*
=============
PolygonCentroid2D

Compute the area-weighted centroid and return twice the signed area.
=============
*/
float PolygonCentroid2D(const vec2_t *points, int count, vec2_t centroid)
{
    int i = count - 1;
    double area = points[0][0] * (points[i][1] - points[1][1])
                + points[i][0] * (points[i-1][1] - points[0][1]);
    double x = points[0][1] * (points[1][0] - points[i][0])
             * (points[1][0] + points[0][0] + points[i][0])
             + points[i][1] * (points[0][0] - points[i-1][0])
             * (points[0][0] + points[i][0] + points[i-1][0]);
    double y = points[0][0] * (points[i][1] - points[1][1])
             * (points[1][1] + points[0][1] + points[i][1])
             + points[i][0] * (points[i-1][1] - points[0][1])
             * (points[0][1] + points[i][1] + points[i-1][1]);
    for (i = 1; i < count - 1; i++)
    {
        area += points[i][0] * (points[i-1][1] - points[i+1][1]);
        x += points[i][1] * (points[i+1][0] - points[i-1][0])
           * (points[i+1][0] + points[i][0] + points[i-1][0]);
        y += points[i][0] * (points[i-1][1] - points[i+1][1])
           * (points[i+1][1] + points[i][1] + points[i-1][1]);
    }
    centroid[0] = x / (area * 3.0);
    centroid[1] = y / (area * 3.0);
    return (float)area;
}

/*
=============
SplitPolygon2D
=============
*/
void SplitPolygon2D(const vec2_t *points, int count, int axis, float split,
                    vec2_t *front, int *outFrontCount, vec2_t *back, int *outBackCount)
{
    int i;
    int sides[MAX_VERTS_PER_POLY];
    int numFrontVertices, numBackVertices;
    float distFront, backdist;

    *outFrontCount = 0;
    *outBackCount = 0;
    numFrontVertices = 0;
    numBackVertices = 0;
    distFront = split + EQUAL_EPSILON;
    backdist = split - EQUAL_EPSILON;
    for (i = 0; i < count; i++)
    {
        sides[i] = SIDE_ON;
        if (points[i][axis] > distFront)
        {
            sides[i] = SIDE_FRONT;
            numFrontVertices++;
        }
        else if (points[i][axis] < backdist)
        {
            sides[i] = SIDE_BACK;
            numBackVertices++;
        }
    }
    if (!numBackVertices)
    {
        memcpy(front, points, count * sizeof(vec2_t));
        *outFrontCount = count;
        return;
    }
    if (!numFrontVertices)
    {
        memcpy(back, points, count * sizeof(vec2_t));
        *outBackCount = count;
        return;
    }
    i = count-1;
    for (int j = 0; j < count; i = j, j++)
    {
        if (sides[i] == SIDE_ON)
        {
            Vector2Copy(points[i], front[*outFrontCount]);
            (*outFrontCount)++;
            Vector2Copy(points[i], back[*outBackCount]);
            (*outBackCount)++;
            continue;
        }
        if (sides[i] == SIDE_FRONT)
        {
            Vector2Copy(points[i], front[*outFrontCount]);
            (*outFrontCount)++;
        }
        else if (sides[i] == SIDE_BACK)
        {
            Vector2Copy(points[i], back[*outBackCount]);
            (*outBackCount)++;
        }
        if (sides[j] == SIDE_ON || sides[j] == sides[i])
            continue;
        vec2_t mid;
        float fraction = (points[i][axis] - split) / (points[i][axis] - points[j][axis]);
        mid[1-axis] = points[i][1-axis]
                    + fraction * (points[j][1-axis] - points[i][1-axis]);
        mid[axis] = split;
        Vector2Copy(mid, front[*outFrontCount]);
        (*outFrontCount)++;
        Vector2Copy(mid, back[*outBackCount]);
        (*outBackCount)++;
    }
    if (*outFrontCount > MAX_VERTS_PER_POLY || *outBackCount > MAX_VERTS_PER_POLY)
        Q3Map_Error("MAX_VERTS_PER_POLY exceeded on 2d poly\n");
}


/*
=============
SubdividePolygon2D

Split into rows, then columns, and visit each nonempty cell.
The four buffers hold the remaining polygon, current row, cell and scratch.
Swap their indices after each cut to avoid copying the remaining vertices.
=============
*/
void SubdividePolygon2D(vec2_t buffers[4][MAX_VERTS_PER_POLY + 4], int count, int columns, int rows,
                        float startX, float startY, float stepX, float stepY,
                        PolygonCellCallback callback)
{
    int src = 0;
    int remainingVertices = count;
    int rowPolygon = 1;
    int cellIndex = 2;
    int swapIndex = 3;
    float yCoord = startY;
    int yCellIndex, column;
    int numRowVerts, cellCount, numRemainderVerts;
    float columnPos;
    vec2_t vCenter;

    for (yCellIndex = 0; yCellIndex < rows && remainingVertices; yCellIndex++)
    {
        if (yCellIndex == rows-1)
        {
            rowPolygon = src;
            numRowVerts = remainingVertices;
        }
        else
        {
            yCoord += stepY;
            SplitPolygon2D(buffers[src], remainingVertices, 1, yCoord,
                           buffers[swapIndex], &numRemainderVerts,
                           buffers[rowPolygon], &numRowVerts);
            SwapInt(&src, &swapIndex);
            remainingVertices = numRemainderVerts;
        }
        columnPos = startX;
        for (column = 0; column < columns && numRowVerts; column++)
        {
            if (column == columns-1)
            {
                SwapInt(&rowPolygon, &cellIndex);
                cellCount = numRowVerts;
            }
            else
            {
                columnPos += stepX;
                SplitPolygon2D(buffers[rowPolygon], numRowVerts, 0, columnPos,
                               buffers[swapIndex], &numRemainderVerts,
                               buffers[cellIndex], &cellCount);
                SwapInt(&rowPolygon, &swapIndex);
                numRowVerts = numRemainderVerts;
            }
            if (cellCount >= 3)
            {
                float area = (float)fabs(PolygonCentroid2D(buffers[cellIndex], cellCount, vCenter));
                if (area > 0.0f)
                    callback(area, vCenter, buffers[cellIndex], cellCount);
            }
        }
    }
}

double MaxDouble(double a, double b);

/*
=============
NearlyEqual
=============
*/
bool NearlyEqual(double a, double b)
{
    return fabs(a-b) <= MaxDouble(EQUAL_EPSILON,
                                 fabs(a+b) * (0.5 * NORMAL_EPSILON));
}

static const double collinearityEpsilon = 3.04617106e-6f;   /* square of sin(0.1 degree), held in single precision */

/*
=====================
PlaneFromPoints

Returns false if the triangle is degenerate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
bool PlaneFromPoints(const Vector3d &a, const Vector3d &b, const Vector3d &c, Vector3d &normal, double &distance)
{
    normal = (a-b).Cross(c-b);
    if (normal.NormalizeLength() == 0.0)
        return false;
    distance = normal * b;
    return true;
}

/*
=============
PerpendicularVector3d
=============
*/
Vector3d PerpendicularVector3d(const Vector3d &normal)
{
    Vector3d point;
    if (fabs(normal.y) > fabs(normal.x) || fabs(normal.z) > fabs(normal.x))
        point = Vector3d(1.0, 0.0, 0.0);
    else
        point = Vector3d(0.0, 1.0, 0.0);
    point -= (point * normal) * normal;
    point.Normalize();
    return point;
}

/*
=============
Vector3d::Normalize
=============
*/
inline void Vector3d::Normalize()
{
    double length = sqrt(*this * *this);
    double inverseLength = 1.0 / length;
    x *= inverseLength;
    y *= inverseLength;
    z *= inverseLength;
}

/*
=============
BuildOrthonormalBasis
=============
*/
void BuildOrthonormalBasis(const Vector3d &normal, Vector3d &right, Vector3d &up)
{
    right = PerpendicularVector3d(normal);
    up = normal.Cross(right);
}

/*
=============
PointsAreCollinear
=============
*/
bool PointsAreCollinear(const Vector3d &p1, const Vector3d &p2, const Vector3d &p3)
{
    Vector3d firstEdge = p1-p2;
    double aLengthSquared = firstEdge*firstEdge;
    if (EQUAL_EPSILON * EQUAL_EPSILON > aLengthSquared)
        return false;
    Vector3d edge2 = p3-p2;
    double length2 = edge2*edge2;
    if (EQUAL_EPSILON * EQUAL_EPSILON > length2)
        return false;
    Vector3d cross = firstEdge.Cross(edge2);
    if (cross*cross <= aLengthSquared * length2 * collinearityEpsilon)
        return true;
    return false;
}

/*
================
PointToPolygonFormFactor

Integrate the polygon's projected solid angle over the incoming hemisphere.
Keep only the two unit directions needed by the current edge.
================
*/
double PointToPolygonFormFactor(const Vector3d &point, const Vector3d &normal,
                                const Vector3d *points, int count)
{
    Vector3d edgeNormal;
    Vector3d rayDirections[2];
    double solidAngle;
    double dotProduct, sinAngleSquared, multiplier;
    int pointIndex, k, currentDir;

    solidAngle = 0.0;
    rayDirections[0] = (points[count-1] - point).Normalized();
    k = 0;
    currentDir = 1;
    for (pointIndex = 0; pointIndex < count; pointIndex++)
    {
        rayDirections[currentDir] = (points[pointIndex] - point).Normalized();
        edgeNormal = rayDirections[k].Cross(rayDirections[currentDir]);
        dotProduct = rayDirections[k] * rayDirections[currentDir];
        sinAngleSquared = edgeNormal * edgeNormal;
        /* Approximate angle / sin(angle) for small angles. */
        if (dotProduct >= 0.0 && sinAngleSquared <= 0.576)
            multiplier = (sinAngleSquared * 0.13935299173356 + 0.15279833715756) * sinAngleSquared + 1.0;
        else
            multiplier = acos(dotProduct) / sqrt(sinAngleSquared);
        solidAngle += (edgeNormal * normal) * multiplier;
        k = currentDir;
        currentDir = !currentDir;
    }
    if (solidAngle <= 0.0)
        return 0.0;
    return solidAngle * (1.0 / (2.0 * Q_PI));
}

/*
=============
Vector3d::Normalized
=============
*/
inline Vector3d Vector3d::Normalized() const
{
    double length = sqrt(*this * *this);
    if (length != 0.0)
        return (1.0 / length) * *this;
    return Vector3d(0.0, 0.0, 0.0);
}

/*
=============
MaxDouble
=============
*/
double MaxDouble(double a, double b)
{
    return a < b ? b : a;
}

/*
=============
Vector3d::NormalizeLength
=============
*/
inline double Vector3d::NormalizeLength()
{
    double length = sqrt(*this * *this);
    if (length != 0.0)
    {
        double inverse = 1.0 / length;
        x *= inverse;
        y *= inverse;
        z *= inverse;
    }
    return length;
}
