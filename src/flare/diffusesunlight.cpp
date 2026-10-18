/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Diffuse sunlight visibility queries for BSP cells.
===========================================================================
*/
#include "flare/mathlib.h"
#include "flare/geometry.h"
#include "flare/polygon.h"
#include "flare/map.h"
#include "flare/light.h"

#define POLYFLAG_PORTAL POLYFLAG_0x40

void FilterDiffuseSunlight_r(BspNode *node, Polygon *poly);

/*
=============
CellHasDiffuseSunlight
=============
*/
bool CellHasDiffuseSunlight(int cellIndex)
{
    Polygon *polygon;
    int polyIndex;
    for (polyIndex = 0; polyIndex < numPermanentPolygons; polyIndex++)
    {
        polygon = &polygons[polyIndex];
        if (!(polygon->GetFlags() & POLYFLAG_PORTAL))
            continue;
        if (polygon->GetCellIndex() != cellIndex)
            continue;
        cellHasDiffuseSunlight = false;
        diffuseSourcePolygon = polygon;
        FilterDiffuseSunlight_r(bspRoot, polygon);
        if (cellHasDiffuseSunlight)
            return true;
    }
    return false;
}
