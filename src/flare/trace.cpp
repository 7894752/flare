/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Segment traces through the polygon BSP.
===========================================================================
*/
/* Segment traces through flare's polygon BSP. */
#include "flare/mathlib.h"
#include "flare/geometry.h"
#include "flare/polygon.h"
#include "flare/traceresult.h"
#include "flare/map.h"


/*
=============
TraceSegment_r

Walk the near half first, stopping at the first polygon hit.
=============
*/
/* Let the recursive trace expand into itself. */
#pragma inline_recursion(on)
inline void TraceSegment_r( BspNode *node, Vector3d start, Vector3d end, TraceResult *trace )
{
    while ( !node->isLeaf )
    {
        Vector3d mid;
        const Plane &plane = node->plane;
        double d1 = start * plane.normal - plane.dist;
        double d2 = end * plane.normal - plane.dist;
        if ( d1 >= 0 && d2 >= 0 )
        {
            node = node->children[0];
            continue;
        }
        if ( d1 <= 0 && d2 <= 0 )
        {
            node = node->children[1];
            continue;
        }
        if ( d1 == 0 )
            mid = start;
        else if ( d2 == 0 )
            mid = end;
        else
            mid = (end * d1 - start * d2) / (d1 - d2);
        int side = d1 < 0;
        if ( d1 != 0 )
        {
            TraceSegment_r( node->children[side], start, mid, trace );
            if ( trace->hit )
                return;
        }
        for ( Polygon *polygon = node->polygons; polygon; polygon = polygon->next )
        {
            if ( !(polygon->GetFlags() & trace->mask) && polygon->ContainsPoint( mid ) )
            {
                trace->hit = polygon;
                trace->hitPoint = mid;
                return;
            }
        }
        if ( d2 == 0 )
            return;
        start = mid;
        node = node->children[!side];
    }
    trace->hitSolid |= node->solid;
    /* hitFlooded records entry into an unflooded leaf. */
    trace->hitFlooded |= !node->flooded;
}

/*
=============
TraceSegment

Initialize the result and trace the segment through the polygon BSP.
=============
*/
void TraceSegment( const Vector3d &start, const Vector3d &end, TraceResult *trace, unsigned int mask )
{
    trace->mask = mask;
    trace->start = start;
    trace->end = end;
    trace->hit = 0;
    trace->hitPoint = end;
    trace->hitSolid = false;
    trace->hitFlooded = false;
    TraceSegment_r( bspRoot, start, end, trace );
}
