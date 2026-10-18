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

/* BSP construction, portals, visibility cells and leak detection. */
#include "flare/math_types.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "flare/geometry_types.h"
#include "flare/optiondefs.h"
#include "flare/geometry.h"
/*
=============
Vector3d::operator+=

Add the corresponding components of v to this vector.
=============
*/
inline void Vector3d::operator+=(const Vector3d &v)
{
    x += v.x;
    y += v.y;
    z += v.z;
}

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
#include "flare/vector.h"
#include "flare/winding.h"
#include "flare/plane.h"
#include "flare/traceresult.h"


#define MAX_LEAK_POINTS 2048
#define POLYGON_SIDE_ON 2
#define PORTAL_PLANAR_EPSILON 0.1f
#define LEAK_POINT_OFFSET_STEP 0.001f
#define PORTAL_NORMAL_DOT_MIN 0.996f
#define PORTAL_PLANE_EPSILON 0.25
#define HEADNODE_PORTAL_PADDING 1.0

#define POLYFLAG_NOSHADOW POLYFLAG_0x01
#define POLYFLAG_ALPHASHADOW POLYFLAG_0x02
#define POLYFLAG_VISUAL_ONLY 0x08
#define POLYFLAG_PORTAL POLYFLAG_0x40

void Q3Map_Error(const char *format, ...);

int numProcessedBspPolygons = 0;
int totalBspPolygons = 0;
Portal *leakPortals[MAX_LEAK_POINTS] = { 0 };
float *axialMins = 0;
float *axialMaxs = 0;
float *axialCoplanars = 0;
BspNode *bspRoot = 0;
unsigned int numLeakPoints = 0;
unsigned int numBspNodes = 0;
int numPortals = 0;
BspNode outsideNode;
Vector3d leakPointsBuffer[MAX_LEAK_POINTS];

void GL_StepNode(int level, BspNode *node);
void AddLeakPoint(const Vector3d &point);

BspNode *PointInLeaf(BspNode *node, const Vector3d &point);

int CountBspPolygons(BspNode *node);

void AddPortalToNodes(Portal *portal, BspNode *front, BspNode *back);

void RemovePortalFromNode(Portal *portal, BspNode *node);

void ClearBspFlooded(BspNode *node);

void RecordBspLeak(BspNode *node, Portal *portal);
bool FloodPortals_r(BspNode *node);

void FloodBspFromPoint(BspNode *root, const Vector3d &point);

void ComputeBspBounds(BspNode *node);

void FreePortal(Portal *portal);
void FreeBspLeakPortal(Portal *portal);

bool PointOutsideBsp(const Vector3d &point);

BspNode *AllocBspNode();

Portal *AllocPortal();

static Portal *CopyPortal(Portal *source, const Winding &winding);

int CompareFloat(const void *a, const void *b);

void MakeNodePortal(BspNode *node);
void SplitNodePortals(BspNode *node);
void UpdateProgress(int count);

void MakeTreePortals_r(BspNode *node);

void BeginProgress(const char *label);
void SetProgress(int current, int total);
void EndProgress();
void GL_Step(int level);
void GL_ClearStep();
void GL_SetLeakPoints(Vector3d *points, int count);
void Q3Map_Printf(const char *format, ...);
void Q3Map_Verbose(const char *format, ...);
void Q3Map_Warning(int level, const char *format, ...);
void FilterBrushesIntoTree(BspNode *node);
void FloodBspFromLights(BspNode *node);
void MakeHeadnodePortals(BspNode *node);
void ClipBspPortals_r(BspNode *node);
bool PlugBspLeak();

bool BuildPortals();

void ClipPortalByPolygons(Portal *portal, const Plane *plane, const Bounds &bounds, BspNode *node, Polygon *polygons);

void GL_StepPortal(int level, Portal *portal);

void TraceSegment(const Vector3d &start, const Vector3d &end, TraceResult *result, unsigned int mask);

struct BspSplitCandidate {
    const Plane *plane;
    int score;
    int splits;
};

/*
Polygon tallies for one split candidate.  Slots 0/1 are the two sides
(front/back in PolygonPlaneSide order, back/front during the axial sweep),
2 the polygons on the plane, and 3 the polygons it cuts.
*/
struct SplitCounts {
    int n[4];
    /* Favour planes that balance the two sides and split few faces. */
    int Score() const {
        return n[2] * 2 - n[3] * options.splitCost - abs(n[0] - n[1]);
    }
};

int PolygonPlaneSide(const Polygon *poly, const Plane &plane);
void SelectAxialSplitPlane(BspSplitCandidate *best, Polygon *polys, int count);
void SelectNonAxialSplitPlane(BspSplitCandidate *best, Polygon *polys, int count);

const Plane *SelectSplitPlane(Polygon *polys, int count);

void GL_StepPortalNode(int level, Portal *portal, BspNode *node);

void GL_StepPolygons(int level, Polygon *polys);
void GL_StepPolygon(int level, Polygon *poly);
int SplitPolygonEpsilon(Polygon *poly, const Plane &plane, Polygon **front, Polygon **back);

BspNode *BuildBsp_r(Polygon *polys, int count);

void MakePolygonPoolPermanent();

BspNode *BuildBsp();

/*
=============
Bounds::Clear
=============
*/
inline void Bounds::Clear() {
    mins[0] = FLT_MAX;
    mins[1] = FLT_MAX;
    mins[2] = FLT_MAX;
    maxs[0] = -FLT_MAX;
    maxs[1] = -FLT_MAX;
    maxs[2] = -FLT_MAX;
}

/*
=============
PointInLeaf

Follow the split planes to the leaf containing point.
=============
*/
BspNode *PointInLeaf(BspNode *node, const Vector3d &point) {
    while (!node->isLeaf) {
        const Plane *p = &node->plane;
        double distance = point.z * p->normal.z + (point.y * p->normal.y + point.x * p->normal.x) - p->dist;
        if (distance < 0)
            node = node->children[1];
        else
            node = node->children[0];
    }
    return node;
}

/*
=============================================================================

Leaf flooding and leak paths

=============================================================================
*/

/*
=============
FloodBspFromPoint

Flood open leaves from point.  Save the starting point if the flood
reaches the outside node.
=============
*/
void FloodBspFromPoint(BspNode *root, const Vector3d &point) {
    BspNode *leaf;
    if (outsideNode.flooded)
        return;
    leaf = PointInLeaf(root, point);
    if (!FloodPortals_r(leaf))
        AddLeakPoint(point);
}

/*
=============
AddLeakPoint

Append the flood origin, which has no portal of its own.
=============
*/
void AddLeakPoint(const Vector3d &point) {
    if (numLeakPoints < MAX_LEAK_POINTS) {
        leakPointsBuffer[numLeakPoints] = point;
        leakPortals[numLeakPoints] = 0;
        numLeakPoints++;
    }
}

/*
=============
FloodPortals_r

Visit connected non-solid leaves.  On reaching the outside, record the
portal path while unwinding and return false.
=============
*/
bool FloodPortals_r(BspNode *node) {
    Portal *portal;
    int side;
    if (node->flooded || node->solid)
        return true;
    GL_StepNode(2, node);
    node->flooded = true;
    GL_StepNode(1, node);
    if (node == &outsideNode)
        return false;
    portal = node->portals;
    while (portal) {
        side = portal->nodes[1] == node;
        if (!FloodPortals_r(portal->nodes[!side])) {
            RecordBspLeak(node, portal);
            return false;
        }
        portal = portal->next[side];
    }
    return true;
}

/*
=============
RecordBspLeak

Save a portal center on the leak path, nudged into the given leaf to
avoid tracing exactly along its boundary.
=============
*/
void RecordBspLeak(BspNode *node, Portal *portal) {
    if (numLeakPoints < MAX_LEAK_POINTS) {
        leakPointsBuffer[numLeakPoints] = portal->winding.Center();
        leakPortals[numLeakPoints] = portal;
        float distance;
        for (distance = LEAK_POINT_OFFSET_STEP; distance < 1.0f; distance += LEAK_POINT_OFFSET_STEP) {
            Vector3d offset = portal->plane->normal * distance;
            if (PointInLeaf(bspRoot, leakPointsBuffer[numLeakPoints] + offset) == node) {
                leakPointsBuffer[numLeakPoints] += offset;
                break;
            }
            if (PointInLeaf(bspRoot, leakPointsBuffer[numLeakPoints] - offset) == node) {
                leakPointsBuffer[numLeakPoints] -= offset;
                break;
            }
        }
        if (distance >= 1.0f)
            Q3Map_Warning(3, "paper-thin leaf?\n");
        numLeakPoints++;
    }
}

/*
=============================================================================

BSP construction

=============================================================================
*/

/*
=============
BuildBsp

Build a tree from the permanent, non-visual-only polygons, then collect
its bounds and polygon counts.
=============
*/
BspNode *BuildBsp() {
    int numPolys = 0;
    Polygon *polys = 0;
    Polygon **last = &polys;
    int i;
    int oldNumVectors;
    for (i = 0; i < numPermanentPolygons; i++) {
        Polygon *p = &polygons[i];
        p->next = 0;
        if (p->info->flags & POLYFLAG_VISUAL_ONLY)
            continue;
        *last = p;
        last = &p->next;
        numPolys++;
    }
    oldNumVectors = vectorPoolUsed;
    axialMins = new float[numPolys];
    if (!axialMins)
        Q3Map_Error("BuildBsp: couldn't allocate temp storage for axial mins (%i bytes)\n", numPolys * sizeof(float));
    axialMaxs = new float[numPolys];
    if (!axialMaxs)
        Q3Map_Error("BuildBsp: couldn't allocate temp storage for axial maxs (%i bytes)\n", numPolys * sizeof(float));
    axialCoplanars = new float[numPolys];
    if (!axialCoplanars)
        Q3Map_Error("BuildBsp: couldn't allocate temp storage for axial coplanars (%i bytes)\n", numPolys * sizeof(float));
    BeginProgress("Building flare's bsp from all polygons...");
    numProcessedBspPolygons = 0;
    totalBspPolygons = numPolys;
    GL_Step(0);
    bspRoot = BuildBsp_r(polys, numPolys);
    EndProgress();
    GL_ClearStep();
    MakePolygonPoolPermanent();
    delete[] axialMins;
    delete[] axialMaxs;
    delete[] axialCoplanars;
    axialMins = 0;
    axialMaxs = 0;
    axialCoplanars = 0;
    ComputeBspBounds(bspRoot);
    numBspPolygons = CountBspPolygons(bspRoot);
    Q3Map_Verbose("%i polys before bsp, %i polys after, %i total, %i max\n", numPolys, numBspPolygons, numPolygons, peakPolygons);
    Q3Map_Verbose("%i verts before bsp, %i total after, %i max\n", oldNumVectors, vectorPoolUsed, vectorPoolHighWater);
    return bspRoot;
}

/*
=============
BuildBsp_r

Choose a plane and divide the polygons between its two children.
Keep coplanar polygons at this node; an empty list becomes a leaf.
=============
*/
BspNode *BuildBsp_r(Polygon *polys, int count) {
    Polygon *p;
    Polygon *pNextPoly;
    Polygon *frontList;
    Polygon *backPolys;
    int numFrontPolygons;
    Polygon *coplanarPolys;
    Polygon *front;
    BspNode *bspNode;
    int numBackPolygons;
    Polygon *backFragment;
    const Plane *splitPlane;
    int numCoplanar;
    if (!count) {
        BspNode *leaf = AllocBspNode();
        leaf->isLeaf = true;
        return leaf;
    }
    frontList = 0;
    backPolys = 0;
    coplanarPolys = 0;
    numFrontPolygons = 0;
    numBackPolygons = 0;
    numCoplanar = 0;
    front = 0;
    backFragment = 0;
    GL_StepPolygons(1, polys);
    splitPlane = SelectSplitPlane(polys, count);
    GL_Step(2);
    for (p = polys; p; p = pNextPoly) {
        GL_StepPolygon(3, p);
        pNextPoly = p->next;
        p->next = 0;
        if (SplitPolygonEpsilon(p, *splitPlane, &front, &backFragment) == POLYGON_SIDE_ON) {
            p->next = coplanarPolys;
            coplanarPolys = p;
            numCoplanar++;
            continue;
        }
        if (front && backFragment && p - polygons >= numPermanentPolygons)
            p->Free();
        if (front) {
            front->next = frontList;
            frontList = front;
            numFrontPolygons++;
        }
        if (backFragment) {
            backFragment->next = backPolys;
            backPolys = backFragment;
            numBackPolygons++;
        }
    }
    numProcessedBspPolygons += numCoplanar;
    totalBspPolygons += numFrontPolygons + numBackPolygons + numCoplanar - count;
    if (numFrontPolygons == count || numBackPolygons == count)
        Q3Map_Error("BuildBsp_r: chosen plane doesn't split\n");
    SetProgress(numProcessedBspPolygons, totalBspPolygons);
    bspNode = AllocBspNode();
    bspNode->plane = *splitPlane;
    bspNode->polygons = coplanarPolys;
    bspNode->children[0] = BuildBsp_r(frontList, numFrontPolygons);
    bspNode->children[1] = BuildBsp_r(backPolys, numBackPolygons);
    bspNode->children[0]->parent = bspNode;
    bspNode->children[1]->parent = bspNode;
    return bspNode;
}

/*
=============
AllocBspNode

Allocate a cleared node and count it for progress reporting.
=============
*/
BspNode *AllocBspNode() {
    BspNode *node;
    numBspNodes++;
    node = new BspNode;
    if (!node)
        Q3Map_Error("AllocBspNode: allocation failed on %i bytes\n", sizeof(BspNode));
    memset(node, 0, sizeof(BspNode));
    return node;
}

/*
=============
SelectSplitPlane

Prefer an axial split.  Also consider polygon planes when the axial
search fails or the polygon count is below the configured threshold.
=============
*/
const Plane *SelectSplitPlane(Polygon *polys, int count) {
    BspSplitCandidate best = { 0, INT_MIN, INT_MAX };
    SelectAxialSplitPlane(&best, polys, count);
    if (!best.plane || !options.axialThreshold || count < options.axialThreshold)
        SelectNonAxialSplitPlane(&best, polys, count);
    return best.plane;
}

/*
=============
SelectNonAxialSplitPlane

Score polygon planes by coplanar coverage, splits and side balance.
Break equal scores in favor of fewer splits.
=============
*/
void SelectNonAxialSplitPlane(BspSplitCandidate *best, Polygon *polys, int count) {
    Polygon *p;
    for (p = polys; p; p = p->next) {
        SplitCounts counts;
        int planeScore;
        memset(&counts, 0, sizeof(counts));
        for (Polygon *p2 = polys; p2; p2 = p2->next) {
            if (p == p2)
                continue;
            int side;
            side = PolygonPlaneSide(p2, p->GetInfo()->plane);
            counts.n[side]++;
        }
        planeScore = counts.Score();
        if (planeScore > best->score || (planeScore == best->score && counts.n[3] < best->splits)) {
            best->plane = &p->GetInfo()->plane;
            best->score = planeScore;
            best->splits = counts.n[3];
        }
    }
}

/*
=============
SelectAxialSplitPlane

Sweep the sorted polygon bounds along each axis, maintaining the side,
coplanar and split counts.  Only consider cuts with populated, balanced
sides, and break equal scores in favor of fewer splits.
=============
*/
void SelectAxialSplitPlane(BspSplitCandidate *best, Polygon *polys, int count) {
    static Plane plane;
    float bestDist;
    float nextPosition;
    int splitAxis;
    int iMax;
    float distance;
    int iMin;
    int numOnPlane;
    int numCoplanars;
    SplitCounts counts;
    int iCoplanar;
    int minHere;
    int numBounds;
    int axis;
    Polygon *p;
    splitAxis = -1;
    bestDist = 0;
    for (axis = 0; axis < 3; axis++) {
        numCoplanars = 0;
        numBounds = 0;
        for (p = polys; p; p = p->next) {
            if (p->bounds.mins[axis] == p->bounds.maxs[axis]) {
                axialCoplanars[numCoplanars] = p->bounds.mins[axis];
                numCoplanars++;
            } else {
                axialMins[numBounds] = p->bounds.mins[axis];
                axialMaxs[numBounds] = p->bounds.maxs[axis];
                numBounds++;
            }
        }
        qsort(axialMins, numBounds, sizeof(float), CompareFloat);
        qsort(axialMaxs, numBounds, sizeof(float), CompareFloat);
        qsort(axialCoplanars, numCoplanars, sizeof(float), CompareFloat);
        counts.n[0] = 0;
        counts.n[1] = count;
        counts.n[3] = 0;
        counts.n[2] = 0;
        iMin = 0;
        iMax = 0;
        iCoplanar = 0;
        minHere = 0;
        numOnPlane = 0;
        nextPosition = axialMins[0] < axialCoplanars[0] ? axialMins[0] : axialCoplanars[0];
        while (nextPosition < FLT_MAX) {
            distance = nextPosition;
            nextPosition = FLT_MAX;
            counts.n[3] += minHere;
            counts.n[1] -= minHere;
            minHere = 0;
            while (iMin < numBounds && axialMins[iMin] == distance) {
                minHere++;
                iMin++;
            }
            if (iMin < numBounds && axialMins[iMin] < nextPosition)
                nextPosition = axialMins[iMin];
            while (iMax < numBounds && axialMaxs[iMax] == distance) {
                counts.n[0]++;
                counts.n[3]--;
                iMax++;
            }
            if (iMax < numBounds && axialMaxs[iMax] < nextPosition)
                nextPosition = axialMaxs[iMax];
            counts.n[0] += numOnPlane;
            counts.n[2] -= numOnPlane;
            numOnPlane = 0;
            while (iCoplanar < numCoplanars && axialCoplanars[iCoplanar] == distance) {
                numOnPlane++;
                iCoplanar++;
            }
            counts.n[2] += numOnPlane;
            counts.n[1] -= numOnPlane;
            if (iCoplanar < numCoplanars && axialCoplanars[iCoplanar] < nextPosition)
                nextPosition = axialCoplanars[iCoplanar];
            if (counts.n[0] > 0 && counts.n[1] > 0 && abs(counts.n[0] - counts.n[1]) < count / 2) {
                int score = counts.Score();
                if (score > best->score || (score == best->score && counts.n[3] < best->splits)) {
                    best->score = score;
                    best->splits = counts.n[3];
                    splitAxis = axis;
                    bestDist = distance;
                }
            }
        }
    }
    if (splitAxis < 0)
        return;
    plane.normal.x = 0;
    plane.normal.y = 0;
    plane.normal.z = 0;
    plane.normal[splitAxis] = 1;
    plane.dist = bestDist;
    plane.sign[0] = 0;
    plane.sign[1] = 0;
    plane.sign[2] = 0;
    best->plane = &plane;
}

/*
=============
CompareFloat
=============
*/
int CompareFloat(const void *a, const void *b) {
    float delta;
    delta = *(const float *)a - *(const float *)b;
    if (delta < 0)
        return -1;
    return delta > 0 ? 1 : 0;
}

/*
=============
ComputeBspBounds

Combine this node's polygon bounds with those of its non-leaf children.
=============
*/
void ComputeBspBounds(BspNode *node) {
    Polygon *p;
    int i;
    node->bounds.Clear();
    if (!node->children[0]->isLeaf) {
        ComputeBspBounds(node->children[0]);
        node->bounds.AddBounds(node->children[0]->bounds);
    }
    if (!node->children[1]->isLeaf) {
        ComputeBspBounds(node->children[1]);
        node->bounds.AddBounds(node->children[1]->bounds);
    }
    for (p = node->polygons; p; p = p->next) {
        for (i = 0; i < p->numPoints; i++)
            node->bounds.AddPoint(p->points[i]);
    }
}

/*
=============
CountBspPolygons

Count polygons stored at this node and throughout its children.
=============
*/
int CountBspPolygons(BspNode *node) {
    Polygon *p;
    int count;
    if (node->isLeaf)
        return 0;
    count = CountBspPolygons(node->children[0]);
    count += CountBspPolygons(node->children[1]);
    for (p = node->polygons; p; p = p->next)
        count++;
    return count;
}

/*
=============
Bounds::AddPoint

Expand the minimum and maximum coordinates to include point.
=============
*/
inline void Bounds::AddPoint(const Vector3d &point)
{
    if (mins[0] > point.x) mins[0] = (float)point.x;
    if (maxs[0] < point.x) maxs[0] = (float)point.x;
    if (mins[1] > point.y) mins[1] = (float)point.y;
    if (maxs[1] < point.y) maxs[1] = (float)point.y;
    if (mins[2] > point.z) mins[2] = (float)point.z;
    if (maxs[2] < point.z) maxs[2] = (float)point.z;
}

/*
=============
Bounds::AddBounds
=============
*/
void Bounds::AddBounds(const Bounds &other) {
    if (mins[0] > other.mins[0])
        mins[0] = other.mins[0];
    if (maxs[0] < other.maxs[0])
        maxs[0] = other.maxs[0];
    if (mins[1] > other.mins[1])
        mins[1] = other.mins[1];
    if (maxs[1] < other.maxs[1])
        maxs[1] = other.maxs[1];
    if (mins[2] > other.mins[2])
        mins[2] = other.mins[2];
    if (maxs[2] < other.maxs[2])
        maxs[2] = other.maxs[2];
}

/*
=============================================================================

Portal construction and clipping

=============================================================================
*/

/*
=============
BuildPortals

Build and clip the tree portals, then mark solid leaves from map brushes.
Flood from the lights, removing blocking portals along leak paths until
the flood is enclosed or no further portal can be removed.
=============
*/
bool BuildPortals() {
    int plugged = 0;
    BeginProgress("Finding portals in flare's bsp...");
    SetProgress(0, numBspNodes);
    GL_Step(0);
    MakeHeadnodePortals(bspRoot);
    MakeTreePortals_r(bspRoot);
    ClipBspPortals_r(bspRoot);
    EndProgress();
    GL_ClearStep();
    FilterBrushesIntoTree(bspRoot);
    Q3Map_Printf("Finding non-solid part of flare's bsp...\n");
    for (;;) {
        FloodBspFromLights(bspRoot);
        if (!outsideNode.flooded) {
            Q3Map_Verbose("plugged %i leaks\n", plugged);
            return true;
        }
        if (!PlugBspLeak()) {
            Q3Map_Verbose("plugged %i leaks\n", plugged);
            Q3Map_Warning(1, "LEAKED!\n");
            GL_SetLeakPoints(leakPointsBuffer, numLeakPoints);
            return false;
        }
        plugged++;
    }
}

/*
==================
MakeTreePortals_r

Create portals down the tree and flag leaves with a coplanar boundary.
==================
*/
void MakeTreePortals_r(BspNode *node) {
    Portal *p1, *p2;
    int i;
    double d;
    if (node->isLeaf) {
        for (p1 = node->portals; p1; p1 = p1->next[p2->nodes[1] == node]) {
            for (p2 = node->portals; p2; p2 = p2->next[p2->nodes[1] == node]) {
                if (p1 == p2)
                    continue;
                for (i = 0; i < (int)p2->winding.NumPoints(); i++) {
                    d = p2->winding[i] * p1->plane->normal - p1->plane->dist;
                    /* Reject this candidate on the first point off its plane. */
                    if (d * d >= PORTAL_PLANAR_EPSILON * PORTAL_PLANAR_EPSILON)
                        goto nextPortal;
                }
            }
            if (!p2) {
                node->flag53 = true;
                break;
            }
nextPortal:
            continue;
        }
    } else {
        GL_StepNode(1, node);
        MakeNodePortal(node);
        SplitNodePortals(node);
        UpdateProgress(1);
        MakeTreePortals_r(node->children[0]);
        MakeTreePortals_r(node->children[1]);
    }
}

/*
==================
MakeNodePortal

Create a winding for the cutting plane and clip it by the node's ancestor
planes and boundary portals.
==================
*/
void MakeNodePortal(BspNode *node) {
    Portal *p, *new_portal, *pNextPortal;
    const Plane *pl;
    Plane clipPlane;

    new_portal = AllocPortal();
    new_portal->plane = &node->plane;
    new_portal->winding.BaseForPlane(*new_portal->plane);
    {
        BspNode *parent = node;
        while (parent->parent) {
            int side = parent == parent->parent->children[1];
            pl = &parent->parent->plane;
            if (side) {
                clipPlane.normal = -pl->normal;
                clipPlane.dist = -pl->dist;
                clipPlane.SetSignBits();
                pl = &clipPlane;
            }
            new_portal->winding.ClipFront(*pl);
            parent = parent->parent;
        }
    }
    for (p = node->portals; p; p = pNextPortal) {
        int side = p->nodes[1] == node;
        pNextPortal = p->next[side];
        pl = p->plane;
        if (side) {
            clipPlane.normal = -pl->normal;
            clipPlane.dist = -pl->dist;
            clipPlane.SetSignBits();
            pl = &clipPlane;
        }
        new_portal->winding.ClipFront(*pl);
    }
    if (new_portal->winding.IsDegenerate()) {
        FreePortal(new_portal);
        return;
    }
    AddPortalToNodes(new_portal, node->children[0], node->children[1]);
    GL_StepPortal(2, new_portal);
}

/*
=============
AddPortalToNodes

Link a portal into both node lists.  Side 0 is front and side 1 is back.
=============
*/
void AddPortalToNodes(Portal *portal, BspNode *front, BspNode *back) {
    portal->nodes[0] = front;
    portal->next[0] = front->portals;
    front->portals = portal;
    portal->nodes[1] = back;
    portal->next[1] = back->portals;
    back->portals = portal;
}

/*
===========
AllocPortal
===========
*/
Portal *AllocPortal() {
    Portal *portal;
    numPortals++;
    portal = new Portal;
    if (!portal)
        Q3Map_Error("AllocPortal: allocation failed on %i bytes\n", sizeof(Portal));
    return portal;
}

/*
=============
FreePortal
=============
*/
void FreePortal(Portal *portal) {
    numPortals--;
    delete portal;
}

/*
=============
Portal::~Portal
=============
*/
Portal::~Portal() {}

/*
==============
SplitNodePortals

Move or split the portals that bound node so that the node's
children have portals instead of node.
==============
*/
void SplitNodePortals(BspNode *node) {
    Portal *next;
    BspNode *frontChild;
    Winding *frontwinding;
    const Plane *pl;
    Portal *pCurrentPortal;
    BspNode *backChild;
    Winding *backwinding;
    GL_StepNode(2, node);
    pl = &node->plane;
    frontChild = node->children[0];
    backChild = node->children[1];
    for (pCurrentPortal = node->portals; pCurrentPortal; pCurrentPortal = next) {
        GL_StepPortalNode(3, pCurrentPortal, node);
        int iSide = node == pCurrentPortal->nodes[1];
        BspNode *pOtherNode = pCurrentPortal->nodes[!iSide];
        next = pCurrentPortal->next[iSide];
        RemovePortalFromNode(pCurrentPortal, pCurrentPortal->nodes[0]);
        RemovePortalFromNode(pCurrentPortal, pCurrentPortal->nodes[1]);
        pCurrentPortal->winding.Split(*pl, &frontwinding, &backwinding);
        if (frontwinding && frontwinding->IsDegenerate()) {
            if (frontwinding != &pCurrentPortal->winding)
                delete frontwinding;
            frontwinding = 0;
        }
        if (backwinding && backwinding->IsDegenerate()) {
            if (backwinding != &pCurrentPortal->winding)
                delete backwinding;
            backwinding = 0;
        }
        if (!frontwinding && !backwinding) {
            FreePortal(pCurrentPortal);
            continue;
        }
        if (!backwinding) {
            if (!iSide)
                AddPortalToNodes(pCurrentPortal, frontChild, pOtherNode);
            else
                AddPortalToNodes(pCurrentPortal, pOtherNode, frontChild);
            if (frontwinding != &pCurrentPortal->winding) {
                pCurrentPortal->winding.Copy(*frontwinding);
                delete frontwinding;
            }
            continue;
        }
        if (!frontwinding) {
            if (!iSide)
                AddPortalToNodes(pCurrentPortal, backChild, pOtherNode);
            else
                AddPortalToNodes(pCurrentPortal, pOtherNode, backChild);
            if (backwinding != &pCurrentPortal->winding) {
                pCurrentPortal->winding.Copy(*backwinding);
                delete backwinding;
            }
            continue;
        }
        Portal *new_portal = AllocPortal();
        *new_portal = *pCurrentPortal;
        new_portal->winding.Copy(*backwinding);
        pCurrentPortal->winding.Copy(*frontwinding);
        delete backwinding;
        delete frontwinding;
        if (!iSide) {
            AddPortalToNodes(pCurrentPortal, frontChild, pOtherNode);
            AddPortalToNodes(new_portal, backChild, pOtherNode);
        } else {
            AddPortalToNodes(pCurrentPortal, pOtherNode, frontChild);
            AddPortalToNodes(new_portal, pOtherNode, backChild);
        }
    }
}

/*
=============
RemovePortalFromNode

Unlink a portal from one node's list, following the links for that node.
=============
*/
void RemovePortalFromNode(Portal *portal, BspNode *node) {
    Portal **prev = &node->portals;
    Portal *next = node->portals;
    while (next) {
        int side = next->nodes[1] == node;
        if (next == portal) {
            *prev = next->next[side];
            break;
        }
        prev = &next->next[side];
        next = *prev;
    }
}

/*
=============
Portal::operator=
=============
*/
inline Portal &Portal::operator=(const Portal &other) {
    unsigned int i, j;
    plane = other.plane;
    winding.Copy(other.winding);
    for (i = 0; i < 2; i++)
        nodes[i] = other.nodes[i];
    for (j = 0; j < 2; j++)
        next[j] = other.next[j];
    return *this;
}

/*
================
MakeHeadnodePortals

Enclose the root bounds with six portals facing the global outside node.
================
*/
void MakeHeadnodePortals(BspNode *node) {
    static Plane planes[6];
    Portal *portals[6];
    int i, j;
    int index;
    outsideNode.isLeaf = true;
    outsideNode.children[0] = 0;
    outsideNode.children[1] = 0;
    outsideNode.parent = 0;
    outsideNode.flooded = false;
    outsideNode.portals = 0;
    index = 0;
    for (i = 0; i < 3; i++) {
        for (j = 0; j <= 1; j++) {
            double sign = j ? 1.0f : -1.0f;
            Portal *p = AllocPortal();
            portals[i * 2 + j] = p;
            if (i == 0) {
                planes[index].normal = Vector3d(sign, 0, 0);
                planes[index].dist = node->bounds.points[j][0] * planes[index].normal.x + HEADNODE_PORTAL_PADDING;
                planes[index].sign[0] = 1 - j;
                planes[index].sign[1] = 0;
                planes[index].sign[2] = 0;
            } else if (i == 1) {
                planes[index].normal = Vector3d(0, sign, 0);
                planes[index].dist = node->bounds.points[j][1] * planes[index].normal.y + HEADNODE_PORTAL_PADDING;
                planes[index].sign[0] = 0;
                planes[index].sign[1] = 1 - j;
                planes[index].sign[2] = 0;
            } else {
                planes[index].normal = Vector3d(0, 0, sign);
                planes[index].dist = node->bounds.points[j][2] * planes[index].normal.z + HEADNODE_PORTAL_PADDING;
                planes[index].sign[0] = 0;
                planes[index].sign[1] = 0;
                planes[index].sign[2] = 1 - j;
            }
            p->plane = &planes[index];
            p->winding.BaseForPlane(planes[index]);
            AddPortalToNodes(p, &outsideNode, node);
            index++;
        }
    }
    for (i = 0; i < 6; i++) {
        for (j = 0; j < 6; j++) {
            if (i == j)
                continue;
            portals[i]->winding.ClipFront(*portals[j]->plane);
        }
    }
}

/*
=============
ClipBspPortals_r

Trim each leaf portal against polygons stored in its ancestor nodes.
=============
*/
void ClipBspPortals_r(BspNode *node) {
    if (!node->isLeaf) {
        ClipBspPortals_r(node->children[0]);
        ClipBspPortals_r(node->children[1]);
    } else {
        Portal *p = node->portals;
        int i;
        while (p) {
            Portal *next = p->next[node == p->nodes[1]];
            Bounds bounds = { FLT_MAX, FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX };
            for (i = 0; i < (int)p->winding.NumPoints(); i++)
                bounds.AddPoint(p->winding[i]);
            if (BspNode *parent = node->parent)
                ClipPortalByPolygons(p, p->plane, bounds, parent, parent->polygons);
            p = next;
        }
        UpdateProgress(1);
    }
}

/*
=============
CopyPortal

Copy a winding fragment and insert it after source in both node lists.
=============
*/
static Portal *CopyPortal(Portal *source, const Winding &winding) {
    Portal *portal;
    portal = AllocPortal();
    portal->plane = source->plane;
    portal->winding.Copy(winding);
    portal->nodes[0] = source->nodes[0];
    portal->nodes[1] = source->nodes[1];
    portal->next[0] = source->next[0];
    portal->next[1] = source->next[1];
    source->next[0] = portal;
    source->next[1] = portal;
    return portal;
}

/*
=============
ClipPortalByPolygons

Subtract opaque, coplanar polygons from the portal winding while walking
up the tree.  Remove a covered portal and continue clipping any fragments.
=============
*/
void ClipPortalByPolygons(Portal *portal, const Plane *plane, const Bounds &bounds, BspNode *node, Polygon *polygons) {
    Winding *winding = &portal->winding;
    int i, numWindings;
    Winding *clippedWindings;
    while (node) {
        for (; polygons; polygons = polygons->next) {
            if (polygons->info->flags & (POLYFLAG_NOSHADOW | POLYFLAG_ALPHASHADOW | POLYFLAG_MODEL | POLYFLAG_PORTAL))
                continue;
            if (polygons->GetInfo()->plane.normal * plane->normal < PORTAL_NORMAL_DOT_MIN)
                continue;
            for (i = 0; i < polygons->numPoints; i++) {
                if (fabs(polygons->points[i] * plane->normal - plane->dist) > PORTAL_PLANE_EPSILON)
                    break;
            }
            if (i < polygons->numPoints)
                continue;
            numWindings = polygons->TrimPortalWinding(winding, bounds, &clippedWindings);
            if (numWindings == 1 && clippedWindings == winding) {
                RemovePortalFromNode(portal, portal->nodes[0]);
                RemovePortalFromNode(portal, portal->nodes[1]);
                FreePortal(portal);
                return;
            }
            if (numWindings > 0) {
                winding->Copy(clippedWindings[0]);
                for (i = 1; i < numWindings; i++) {
                    Portal *newPortal = CopyPortal(portal, clippedWindings[i]);
                    ClipPortalByPolygons(newPortal, plane, bounds, node, polygons->next);
                }
                delete[] clippedWindings;
            }
        }
        node = node->parent;
        if (node)
            polygons = node->polygons;
    }
}

/*
=============================================================================

Leak repair and flood queries

=============================================================================
*/

/*
=============
PlugBspLeak

Try to close the recorded leak at a portal covered by a polygon, then by
tracing between leak points.  Keep a no-shadow portal as a fallback.
Return true when a portal is removed so the flood can be retried.
=============
*/
bool PlugBspLeak() {
    Polygon *p;
    int i;
    Portal *lastPortal;
    lastPortal = 0;
    {
        TraceResult trace;
        BspNode *parent;
        int side;
        i = numLeakPoints;
        while (i) {
            /* No-shadow portals restart before the decrement, bypassing the loop guard. */
nextPoint:
            i--;
            Portal *portal = leakPortals[i];
            if (!portal)
                continue;
            Vector3d center = portal->winding.Center();
            for (side = 0; side <= 1; side++) {
                for (parent = portal->nodes[side]->parent; parent; parent = parent->parent) {
                    for (p = parent->polygons; p; p = p->next) {
                        if (p->PointOnPolygon(center)) {
                            if (p->info->flags & POLYFLAG_PORTAL)
                                continue;
                            if (p->info->flags & POLYFLAG_NOSHADOW) {
                                lastPortal = portal;
                                goto nextPoint;
                            }
                            FreeBspLeakPortal(portal);
                            return true;
                        }
                    }
                }
            }
        }
        i = numLeakPoints - 1;
        while (i > 0) {
            i--;
            if (!leakPortals[i + 1])
                continue;
            TraceSegment(leakPointsBuffer[i + 1], leakPointsBuffer[i], &trace, POLYFLAG_NOSHADOW | POLYFLAG_ALPHASHADOW | POLYFLAG_VISUAL_ONLY | POLYFLAG_PORTAL);
            if (trace.hit) {
                FreeBspLeakPortal(leakPortals[i + 1]);
                return true;
            }
        }
    }
    if (lastPortal) {
        FreeBspLeakPortal(lastPortal);
        return true;
    }
    return false;
}

/*
=============
FreeBspLeakPortal

Remove a portal on the leak path and clear the flood state for a retry.
=============
*/
void FreeBspLeakPortal(Portal *portal) {
    RemovePortalFromNode(portal, portal->nodes[0]);
    RemovePortalFromNode(portal, portal->nodes[1]);
    FreePortal(portal);
    ClearBspFlooded(bspRoot);
    outsideNode.flooded = false;
    numLeakPoints = 0;
}

/*
=============
ClearBspFlooded

Clear the flood mark on every node and leaf.
=============
*/
void ClearBspFlooded(BspNode *node) {
    node->flooded = false;
    if (!node->isLeaf) {
        ClearBspFlooded(node->children[0]);
        ClearBspFlooded(node->children[1]);
    }
}

/*
=============
PointOutsideBsp

After an enclosed flood, solid and unreached leaves are outside.
A leaked flood cannot establish an inside region.
=============
*/
bool PointOutsideBsp(const Vector3d &point) {
    BspNode *node;
    if (outsideNode.flooded)
        return false;
    node = PointInLeaf(bspRoot, point);
    if (!node)
        return true;
    if (node->solid || !node->flooded)
        return true;
    return false;
}
