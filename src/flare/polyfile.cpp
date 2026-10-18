/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Polygon file input.
===========================================================================
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "flare/mathlib.h"
#include "flare/geometry.h"
#include "libs/cmdlib/cmdlib.h"


struct MapInfo;

short Q3Map_RequestShader(const char *name);
unsigned short SurfaceFlags(MapInfo *map, int shaderIndex, int entityIndex);
void Q3Map_Error(const char *format, ...);

/*
=============
Q3Map_ReadPolyFile

Each record contains a shader name, a vertex count, then position and s/t pairs.
Load optional .poly geometry using the map's shader surface flags.
=============
*/
void Q3Map_ReadPolyFile(MapInfo *map, const char *name)
{
    char filename[64];
    char shader[1024];
    char line[1024];
    float st[MAX_VERTS_PER_POLY][2];
    Vector3d points[MAX_VERTS_PER_POLY];
    FILE *file;
    int shaderIndex, flags, numPoints;
    int i;

    strcpy(filename, name);
    StripExtension(filename);
    strcat(filename, ".poly");
    file = fopen(filename, "r");
    if (!file)
        return;

    while (fgets(shader, sizeof(shader), file))
    {
        i = strlen(shader);
        while (shader[i - 1] == '\r' || shader[i - 1] == '\n')
            i--;
        shader[i] = 0;

        shaderIndex = Q3Map_RequestShader(shader);
        if (shaderIndex < 0)
            Q3Map_Error("missing shader '%s' in '%s'\n", shader, filename);
        flags = SurfaceFlags(map, shaderIndex, 0);

        if (!fgets(line, sizeof(line), file))
            Q3Map_Error("Unexpected end of file in %s\n", filename);
        numPoints = atoi(line);
        for (i = 0; i < numPoints; i++)
        {
            if (!fgets(line, sizeof(line), file))
                Q3Map_Error("Unexpected end of file in %s\n", filename);
            if (sscanf(line, "( %lg %lg %lg %g %g )\n",
                       &points[i].x, &points[i].y, &points[i].z,
                       &st[i][0], &st[i][1]) != 5)
                Q3Map_Error("File %s has been corrupted\n", filename);
        }
        CreatePoly(flags, numPoints, points, 0, 0, st, shaderIndex, -1, true);
    }
}
