/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Occluder pools, side planes and covered-occluder removal.
===========================================================================
*/

#include "flare/light.h"
#include "flare/polygon.h"
#include "flare/options.h"
#include "flare/print.h"
#include "flare/geometry.h"

#define MAX_OCCLUDERS_PER_POLY 32768
#define MODEL_OCCLUDER_ALLOWANCE 100000

#include "flare/occluderpairs.h"

Occluder *polyOccluders[MAX_OCCLUDERS_PER_POLY] = { 0 };
int numPolyOccluders = 0;
Occluder *occluderPool = 0;
int occluderCapacity = 0;
int numOccluders = 0;
static Plane *sidePlanePool = 0;
static int maxSidePlanes = 0;
static int numSidePlanes = 0;
Occluder *activeOccluders = 0;
int numOccluderPairs = 0;
OccluderPair *occluderPairs = 0;

/*
=============
AllocOccluderSidePlanes

Take a contiguous range from the side-plane pool.
=============
*/
static inline Plane *AllocOccluderSidePlanes(int count)
{
    Plane *planes;
    if (numSidePlanes + count > maxSidePlanes)
        Q3Map_Error("Side planes (%i) exceeded... increase maxoccluders\n", maxSidePlanes);
    planes = &sidePlanePool[numSidePlanes];
    numSidePlanes += count;
    return planes;
}

/*
=============
FreeOccluderSidePlanes
=============
*/
static inline void FreeOccluderSidePlanes(int count)
{
    numSidePlanes -= count;
}

/*
=============
AllocOccluder
=============
*/
inline Occluder *AllocOccluder(void)
{
    if (numOccluders == occluderCapacity)
        Q3Map_Error("Occluders (%i) exceeded... increase maxoccluders\n", occluderCapacity);
    return &occluderPool[numOccluders++];
}

/*
=============
FreeOccluder

Release the most recently allocated occluder and its side planes.
=============
*/
void FreeOccluder(Occluder *occluder)
{
    FreeOccluderSidePlanes(occluder->polygon->numPoints);
    numOccluders--;
}

/*
=============
AddOccluder

Build the polygon's occluder for the current light.
=============
*/
Occluder *AddOccluder(const Polygon *polygon)
{
    Occluder *occluder = AllocOccluder();
    occluder->polygon = polygon;
    occluder->sidePlanes = AllocOccluderSidePlanes(polygon->numPoints);
    currentLight->BuildOccluder(polygon, occluder);
    return occluder;
}

/*
=============
ClearOccluderPools
=============
*/
inline void ClearOccluderPools(void)
{
    numOccluders = 0;
    numSidePlanes = 0;
    activeOccluders = 0;
}

/*
=============
FreeOccluderPools
=============
*/
void FreeOccluderPools(void)
{
    ClearOccluderPools();
    delete[] occluderPool;
    occluderPool = 0;
    occluderCapacity = 0;
    numOccluders = 0;
    numPolyOccluders = 0;
    delete[] sidePlanePool;
    sidePlanePool = 0;
    maxSidePlanes = 0;
}

/*
=============
UnlinkOccluder

Remove an occluder from the active list without releasing its pool storage.
=============
*/
inline void UnlinkOccluder(Occluder *occluder)
{
    Occluder *prev;

    if (occluder == activeOccluders)
    {
        activeOccluders = occluder->next;
        occluder->next = 0;
        return;
    }

    for (prev = activeOccluders; prev->next != occluder; prev = prev->next)
        ;
    prev->next = occluder->next;
    occluder->next = 0;
}

/*
=============
CompareOccluderArea

Sort projected bounding rectangles from largest to smallest.
=============
*/
int CompareOccluderArea(const void *a, const void *b)
{
    const Occluder *first = *(Occluder *const *)a;
    const Occluder *second = *(Occluder *const *)b;
    float firstArea = (first->maxs[1] - first->mins[1]) *
                      (first->maxs[0] - first->mins[0]);
    float secondArea = (second->maxs[1] - second->mins[1]) *
                       (second->maxs[0] - second->mins[0]);
    if (firstArea < secondArea)
        return 1;
    if (firstArea > secondArea)
        return -1;
    return 0;
}

/*
=============
RemoveCoveredOccluders

Without force, keep the first covered occluder until a second is found.
=============
*/
bool RemoveCoveredOccluders(const Polygon *polygon, Occluder *newOccluder, bool force)
{
    bool removed = false;
    Occluder *first = 0;
    int i, j, k;

    for (i = 0; i < numPolyOccluders; i++)
    {
        Occluder *old = polyOccluders[i];

        if (newOccluder->mins[0] > old->mins[0] ||
            newOccluder->maxs[0] < old->maxs[0] ||
            newOccluder->mins[1] > old->mins[1] ||
            newOccluder->maxs[1] < old->maxs[1])
            continue;

        /* Any outside vertex keeps the old occluder; resume the outer list scan. */
        for (j = 0; j < old->polygon->numPoints; j++)
        {
            const Vector3d &point = old->polygon->points[j];
            for (k = 0; k < polygon->numPoints; k++)
            {
                if (newOccluder->sidePlanes[k].normal * point >
                    newOccluder->sidePlanes[k].dist)
                    goto nextOccluder;
            }
        }

        if (!removed && !force && !first)
        {
            first = old;
        }
        else
        {
            if (first)
            {
                UnlinkOccluder(first);
                first = 0;
            }
            UnlinkOccluder(old);
            removed = true;
        }

nextOccluder:
        continue;
    }
    return removed;
}

/*
=============
AllocOccluderPools

Reserve the occluder and side-plane storage for subsequent light passes.
=============
*/
void AllocOccluderPools(void)
{
    int limit = numBspPolygons;
    if (!options.worldModels)
        limit += MODEL_OCCLUDER_ALLOWANCE;

    occluderCapacity = options.maxOccluders;
    if (!occluderCapacity || occluderCapacity > limit)
        occluderCapacity = limit;
    maxSidePlanes = 4 * occluderCapacity;

    occluderPool = new Occluder[occluderCapacity];
    if (!occluderPool)
        Q3Map_Error("AllocOccluderPools: allocation of occluder pool failed on %i bytes, reduce maxoccluders\n",
                   occluderCapacity * sizeof(Occluder));
    Plane *planes = new Plane[maxSidePlanes];
    sidePlanePool = planes;
    if (!sidePlanePool)
        Q3Map_Error("AllocOccluderPools: allocation of side plane pool failed on %i bytes, reduce maxoccluders\n",
                   maxSidePlanes * sizeof(Plane));
    numOccluders = 0;
    numSidePlanes = 0;
    activeOccluders = 0;
}

