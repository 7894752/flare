/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

TraceResult: segment trace result.
===========================================================================
*/

#ifndef FLARE_TRACERESULT_H
#define FLARE_TRACERESULT_H

#include "flare/geometry_types.h"

/* Segment endpoints, first polygon hit and crossed leaf contents. */
struct TraceResult
{
    TraceResult();
    Vector3d start;
    Vector3d end;
    Vector3d hitPoint;              /* first hit, or end when unobstructed */
    bool hitSolid;                  /* crossed a solid leaf */
    bool hitFlooded;                /* crossed an unflooded leaf */
    struct Polygon *hit;            /* polygon hit, NULL if none */
    unsigned int mask;              /* polygon flags to test (0x4B for light visibility, 0x78 for picking) */
};
FLARE_LAYOUT(TraceResult_size, sizeof(TraceResult) == 0x58);
FLARE_LAYOUT(TraceResult_members, FLARE_OFFSET(TraceResult, end) == 0x18 && FLARE_OFFSET(TraceResult, hit) == 0x4C
                                      && FLARE_OFFSET(TraceResult, mask) == 0x50);

inline TraceResult::TraceResult()
{}

#endif
