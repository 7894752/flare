/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Ordered pairs of occluders shared by the scene registry.
===========================================================================
*/
#ifndef FLARE_OCCLUDERPAIRS_H
#define FLARE_OCCLUDERPAIRS_H

#include "flare/mathlib.h"
#include "flare/geometry.h"

struct OccluderPair
{
    Occluder *second;
    Occluder *first;
    OccluderPair *next;
};

void ClearOccluderPools(void);
extern int numPolyOccluders;
extern Occluder *activeOccluders;
int CompareOccluderArea(const void *a, const void *b);
bool RemoveCoveredOccluders(const Polygon *polygon, Occluder *newOccluder, bool force);
extern Occluder *polyOccluders[];
void AllocOccluderPools(void);
void FreeOccluderPools(void);
Occluder *AddOccluder(const Polygon *poly);
void FreeOccluder(Occluder *occluder);
extern int numOccluderPairs;
extern OccluderPair *occluderPairs;
extern Occluder *occluderPool;


#endif
