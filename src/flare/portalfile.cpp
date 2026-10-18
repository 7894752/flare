/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Portal file input.
===========================================================================
*/
#include <stdio.h>
#include <string.h>
#include "flare/mathlib.h"
#include "flare/geometry.h"
#include "flare/polygon.h"
#include "flare/map.h"
#include "libs/cmdlib/cmdlib.h"

#define PORTAL_POLYGON_FLAG 0x40

void Q3Map_Error(const char *format, ...);

/*
=============
AddPolygonToBounds
=============
*/
inline void AddPolygonToBounds(const Polygon *poly, Bounds *bounds)
{
    for (int i = 0; i < poly->numPoints; i++)
        bounds->AddPoint(poly->points[i]);
}

void Q3Map_Warning(int level, const char *format, ...);

/*
=============
Bounds::Clear
=============
*/
inline void Bounds::Clear()
{
    mins[0] = FLT_MAX;
    mins[1] = FLT_MAX;
    mins[2] = FLT_MAX;
    maxs[0] = -FLT_MAX;
    maxs[1] = -FLT_MAX;
    maxs[2] = -FLT_MAX;
}

/*
=============
Q3Map_ReadPortalFile

Create an oppositely wound polygon for each adjacent cell and extend its bounds.
The header face count and per-portal hint are read as part of the PRT1 format.
=============
*/
void Q3Map_ReadPortalFile(const char *name)
{
    char filename[64];
    char magic[80];
    Vector3d portalPoints[2][MAX_VERTS_PER_POLY];
    FILE *file;
    int numCells, numPortals, numFaces;
    int numPoints, cells[2], hint;
    int i, j;
    Polygon *poly;

    strcpy(filename, name);
    StripExtension(filename);
    strcat(filename, ".prt");
    file = fopen(filename, "r");
    if (!file)
        Q3Map_Error("Q3Map_ReadPortalFile: could not open '%s'\n", filename);
    if (fscanf(file, "%79s\n%i\n%i\n%i\n", magic,
               &numCells, &numPortals, &numFaces) != 4)
    {
        fclose(file);
        Q3Map_Error("Q3Map_ReadPortalFile: failed to read header\n");
    }
    if (strcmp(magic, "PRT1"))
    {
        fclose(file);
        Q3Map_Error("Q3Map_ReadPortalFile: not a valid portal file\n");
    }

    mapInfo.numLightLeaves = numCells;
    mapInfo.leafBounds = new Bounds[mapInfo.numLightLeaves];
    for (i = 0; i < mapInfo.numLightLeaves; i++)
        mapInfo.leafBounds[i].Clear();

    for (i = 0; i < numPortals; i++)
    {
        if (fscanf(file, "%i %i %i %i ", &numPoints, &cells[0], &cells[1], &hint) != 4)
        {
            fclose(file);
            Q3Map_Error("Q3Map_ReadPortalFile: couldn't read line\n");
        }
        if (numPoints < 3 || numPoints > MAX_VERTS_PER_POLY ||
            cells[0] < 0 || cells[0] >= mapInfo.numLightLeaves ||
            cells[1] < 0 || cells[1] >= mapInfo.numLightLeaves)
        {
            fclose(file);
            Q3Map_Error("Q3Map_ReadPortalFile: inconsistent numbers\n");
        }
        for (j = 0; j < numPoints; j++)
        {
            double x, y, z;
            if (fscanf(file, "(%lf %lf %lf ) ", &x, &y, &z) != 3)
            {
                fclose(file);
                Q3Map_Error("Q3Map_ReadPortalFile: couldn't read vertex\n");
            }
            portalPoints[0][j].x = x;
            portalPoints[0][j].y = y;
            portalPoints[0][j].z = z;
            portalPoints[1][numPoints - j - 1].x = x;
            portalPoints[1][numPoints - j - 1].y = y;
            portalPoints[1][numPoints - j - 1].z = z;
        }
        poly = CreatePoly(PORTAL_POLYGON_FLAG, numPoints, portalPoints[0], 0, 0, 0, -1, -1, true);
        if (!poly)
            Q3Map_Warning(1, "Q3Map_ReadPortalFile: couldn't create portal poly\n");
        else
        {
            poly->SetCellIndex(cells[0]);
            AddPolygonToBounds(poly, &mapInfo.leafBounds[cells[0]]);
        }
        poly = CreatePoly(PORTAL_POLYGON_FLAG, numPoints, portalPoints[1], 0, 0, 0, -1, -1, true);
        if (!poly)
            Q3Map_Warning(1, "Q3Map_ReadPortalFile: couldn't create portal poly\n");
        else
        {
            poly->SetCellIndex(cells[1]);
            AddPolygonToBounds(poly, &mapInfo.leafBounds[cells[1]]);
        }
    }
    fclose(file);
}
