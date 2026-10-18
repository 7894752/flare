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

/* BSP surface conversion and lighting input and output. */
#include <string.h>
#include "code/game/surfaceflags.h"
#include "common/cod_bsp_lump_indices.h"
#include "flare/print.h"
#include "flare/bspio.h"

#define BSP_LUMP_COUNT 33
#define MAX_BRUSH_SIDES 128
#define LIGHTMAP_SIZE 512
#define SHADER_TRANSPARENT (SHADERFLAG_ALPHA_TEST | SHADERFLAG_ALPHA_BLEND | SHADERFLAG_0x08)

#include "flare/mathlib.h"
#include "flare/geometry.h"
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

static const float sunLightScale = 4.0f / 255.0f;
static const float colorComponents = 3.0f;
static const float diffuseLightScale = 0.25f;


#include "flare/map.h"

#include "flare/shaders.h"
#include "flare/options.h"

#include "flare/lightmap.h"

struct BspLump {
    unsigned int length, offset;
};

struct BspHeader {
    int ident, version;
    BspLump lumps[BSP_LUMP_COUNT];
};

unsigned char *bspFile = 0;
int bspFileSize = 0;


bool Q3Map_ReadEntities(MapInfo *map, const char *entityText);
unsigned short SurfaceFlags(MapInfo *map, int shaderIndex, int entityIndex);
bool Q3Map_Read(const void *data, int size);
void Q3Map_Free(MapInfo *map);
bool LoadMapShaders(void);
void LoadModelSurfaces(MapInfo *map, int modelIndex, int entityIndex);
static void LoadSurface(MapInfo *map, int surfaceIndex, int entityIndex);
void ConvertBrushes(MapInfo *map);
void RebuildBsp(BspHeader *header);
void ConvertLightmaps(void);
int Q3Map_Write(void **out);
int MaxInt(int a, int b);
int MinInt(int a, int b);

/*
=============================================================================

BSP input

=============================================================================
*/

/*
=============
Q3Map_Read

Keep a writable copy of the BSP and bind its lump arrays to the map.
Read entities before loading their shaders, model surfaces and brushes.
=============
*/
bool Q3Map_Read(const void *data, int size)
{
    bspFile = new unsigned char[size];
    if (!bspFile)
        Q3Map_Error("Q3Map_Read: couldn't allocate storage for copy of bsp file (%i bytes)\n", size);
    memcpy(bspFile, data, size);
    bspFileSize = size;
    BspHeader *header = (BspHeader *)bspFile;
    if (header->ident != COD_BSP_IDENT || header->version != COD_BSP_VERSION)
        return false;
    memset(&mapInfo, 0, sizeof(mapInfo));
    if (!Q3Map_ReadEntities(&mapInfo, (char *)bspFile + header->lumps[COD_LUMP_ENTITIES].offset)) {
        Q3Map_Free(&mapInfo);
        return false;
    }

#define READ_BSP_LUMP(member, count, type, index, error) \
    mapInfo.member = (type *)(bspFile + header->lumps[index].offset); \
    mapInfo.count = header->lumps[index].length / sizeof(type); \
    if (header->lumps[index].length % sizeof(type)) \
        Q3Map_Error(error)

    READ_BSP_LUMP(shaders, numShaders, BspShader, COD_LUMP_SHADERS, "Funny size for LUMP_SHADERS\n");
    READ_BSP_LUMP(planes, numPlanes, BspPlane, COD_LUMP_PLANES, "Funny size for LUMP_PLANES\n");
    READ_BSP_LUMP(models, numModels, BspModel, COD_LUMP_MODELS, "Funny size for LUMP_MODELS\n");
    READ_BSP_LUMP(brushes, numBrushes, BspBrush, COD_LUMP_BRUSHES, "Funny size for LUMP_BRUSHES\n");
    READ_BSP_LUMP(brushSides, numBrushSides, BspBrushSide, COD_LUMP_BRUSHSIDES, "Funny size for LUMP_BRUSHSIDES\n");
    READ_BSP_LUMP(vertices, numVertices, BspVertex, COD_LUMP_DRAWVERTS, "Funny size for LUMP_DRAWVERTS\n");
    READ_BSP_LUMP(indices, numIndices, short, COD_LUMP_DRAWINDEXES, "Funny size for LUMP_DRAWINDEXES\n");
    READ_BSP_LUMP(surfaces, numSurfaces, BspSurface, COD_LUMP_TRIANGLE_SOUPS, "Funny size for LUMP_TRIANGLES\n");
#undef READ_BSP_LUMP

    mapInfo.lightmaps = (unsigned char (*)[LIGHTMAP_SIZE][LIGHTMAP_SIZE][3])(bspFile + header->lumps[COD_LUMP_LIGHTMAPS].offset);
    mapInfo.numLightmaps = header->lumps[COD_LUMP_LIGHTMAPS].length / (LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);
    if (header->lumps[COD_LUMP_LIGHTMAPS].length % (LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3))
        Q3Map_Error("Funny size for LUMP_LIGHTMAPS\n");
    return LoadMapShaders();
}

/*
=============
Q3Map_Free
=============
*/
void Q3Map_Free(MapInfo *map)
{
}

/*
=============
LoadMapShaders
=============
*/
bool LoadMapShaders(void)
{
    int i;
    Q3Map_InitShaders();
    for (i = 0; i < mapInfo.numShaders; i++) {
        if (strcmp(mapInfo.shaders[i].name, "noshader") &&
            !Q3Map_LoadShader(mapInfo.shaders[i].name, i)) {
            Q3Map_Warning(1, "couldn't load shader '%s'\n", mapInfo.shaders[i].name);
            shaders[i].flags &= ~SHADER_TRANSPARENT;
            mapInfo.shaders[i].surfaceFlags &= ~SURF_ALPHASHADOW;
        }
    }
    for (i = 0; i < mapInfo.numEntities; i++)
        LoadModelSurfaces(&mapInfo, mapInfo.entities[i].modelIndex, i);
    ConvertBrushes(&mapInfo);
    Q3Map_Free(&mapInfo);
    return true;
}

/*
=============================================================================

Surface and brush conversion

=============================================================================
*/

/*
=============
LoadModelSurfaces
=============
*/
void LoadModelSurfaces(MapInfo *map, int modelIndex, int entityIndex)
{
    int i;
    BspModel *model;
    if (modelIndex < 0)
        return;
    if (modelIndex >= map->numModels) {
        Q3Map_Warning(1, "ignoring bad model index %i\n", modelIndex);
        return;
    }
    model = &map->models[modelIndex];
    for (i = 0; i < model->numSurfaces; i++)
        LoadSurface(map, model->firstSurface + i, entityIndex);
}

/*
=============
LoadSurface
=============
*/
static void LoadSurface(MapInfo *map, int surfaceIndex, int entityIndex)
{
    BspSurface *surface;
    if (surfaceIndex < 0 || surfaceIndex >= map->numSurfaces) {
        Q3Map_Warning(1, "ignoring bad surface index %i\n", surfaceIndex);
        return;
    }
    surface = &map->surfaces[surfaceIndex];
    if (surface->shaderIndex < 0 ||
        !strcmp(map->shaders[surface->shaderIndex].name, "noshader"))
        return;
    ConvertSurface(map, surface, entityIndex);
}

/*
=============
ConvertSurface

Transform the indexed triangles into world-space polygons, preserving
texture coordinates, lightmap coordinates and any varying vertex normals.
=============
*/
void ConvertSurface(MapInfo *map, const BspSurface *surface, int entityIndex)
{
    int indexNum;
    /* Last polygon created for this surface. */
    Polygon *newPoly;
    float texCoords[3][2];
    const Vector3d *basisVectors;
    Vector3d positions[3];
    Vector3d vertexNormals[3];
    Vector3d offset = map->entities[entityIndex].origin;
    basisVectors = map->entities[entityIndex].axis;
    BspVertex *verts = &map->vertices[surface->firstVertex];
    unsigned short flags = SurfaceFlags(map, surface->shaderIndex, entityIndex);
    short *drawIndices = &map->indices[surface->firstIndex];
    for (indexNum = 0; indexNum < surface->numIndices; indexNum += 3) {
        float lightmap[3][2];
        for (int j = 0; j < 3; j++) {
            positions[j] = offset + verts[drawIndices[indexNum + j]].xyz[0] * basisVectors[0]
                               + verts[drawIndices[indexNum + j]].xyz[1] * basisVectors[1]
                               + verts[drawIndices[indexNum + j]].xyz[2] * basisVectors[2];
            vertexNormals[j].x = verts[drawIndices[indexNum + j]].normal[0];
            vertexNormals[j].y = verts[drawIndices[indexNum + j]].normal[1];
            vertexNormals[j].z = verts[drawIndices[indexNum + j]].normal[2];
            texCoords[j][0] = verts[drawIndices[indexNum + j]].texCoord[0];
            texCoords[j][1] = verts[drawIndices[indexNum + j]].texCoord[1];
            lightmap[j][0] = verts[drawIndices[indexNum + j]].lightmap[0];
            lightmap[j][1] = verts[drawIndices[indexNum + j]].lightmap[1];
        }
        newPoly = CreatePoly(flags, 3, positions,
                             vertexNormals[1] == vertexNormals[0] && vertexNormals[2] == vertexNormals[0] ? 0 : vertexNormals,
                             lightmap, texCoords, surface->shaderIndex, surface->lightmapIndex, true);
    }
}

/*
=============
ConvertBrushes
=============
*/
void ConvertBrushes(MapInfo *map)
{
    int modelNum = -1;
    /* Brush total for the converted models. */
    int firstBrush = 0;
    BspBrushSide *brushSide = map->brushSides;
    for (int i = 0; i < map->numEntities; i++) {
        if (map->entities[i].modelIndex < 0)
            continue;
        modelNum++;
        brushSide = ConvertEntityBrushes(map, &map->entities[i], brushSide);
        firstBrush += map->models[modelNum].numBrushes;
    }
}


/*
=============
ConvertEntityBrushes

The first six sides store axial distances; later sides reference BSP
planes.  Transform each plane and mark the brush solid only if every
side has solid, shadow-casting contents.
=============
*/
BspBrushSide *ConvertEntityBrushes(MapInfo *map, MapEntity *entity, BspBrushSide *side)
{
    Vector3d n;
    int s;
    Plane brushPlanes[MAX_BRUSH_SIDES];
    BspModel *pModel = &map->models[entity->modelIndex];
    BspBrush *b;
    bool solid;
    int brushIndex;
    for (brushIndex = 0; brushIndex < pModel->numBrushes; brushIndex++) {
        b = &map->brushes[pModel->firstBrush + brushIndex];
        if (b->numSides > MAX_BRUSH_SIDES)
            Q3Map_Error("brush has side count %i > MAX_BRUSH_SIDES (%i)\n", b->numSides, MAX_BRUSH_SIDES);
        solid = true;
        for (s = 0; s < b->numSides; s++, side++) {
            if (s < 6) {
                float sign = (s & 1) ? 1.0f : -1.0f;
                n.x = 0;
                n.y = 0;
                n.z = 0;
                n[s >> 1] = sign;
                brushPlanes[s].dist = sign * side->plane.distance;
            } else {
                BspPlane *plane = &map->planes[side->plane.planeIndex];
                n.x = plane->normal[0];
                n.y = plane->normal[1];
                n.z = plane->normal[2];
                brushPlanes[s].dist = plane->distance;
            }
            brushPlanes[s].normal.x = n * entity->axis[0];
            brushPlanes[s].normal.y = n * entity->axis[1];
            brushPlanes[s].normal.z = n * entity->axis[2];
            brushPlanes[s].dist += brushPlanes[s].normal * entity->origin;
            brushPlanes[s].SetSignBits();
            BspShader *shader = &map->shaders[side->shaderIndex];
            if ((shader->contentFlags & (CONTENTS_TRANSLUCENT | CONTENTS_WATER)) ||
                !(shader->contentFlags & CONTENTS_SOLID) ||
                ((shader->surfaceFlags & SURF_NODRAW) && !(shader->surfaceFlags & SURF_CASTSHADOW)))
                solid = false;
        }
        CreateBrush(brushPlanes, b->numSides, solid);
    }
    return side;
}

/*
=============================================================================

BSP output

=============================================================================
*/

/*
=============
RebuildBsp

Replace the lightmap lump and copy the remaining lumps without changing
their contents.  Publish the new buffer after all input data is copied.
=============
*/
void RebuildBsp(BspHeader *header)
{
    unsigned char *source = (unsigned char *)header;
    int lightSize = numLightmaps * (LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3);
    /* Geometry contribution is currently empty. */
    int geometryBytes = 0;
    int newFileSize;
    int i;
    int size = sizeof(BspHeader);
    for (i = 0; i < BSP_LUMP_COUNT; i++) {
        if (i == COD_LUMP_LIGHTMAPS)
            continue;
        size += header->lumps[i].length;
    }
    newFileSize = size + lightSize;
    unsigned char *buffer = new unsigned char[newFileSize];
    BspHeader *newHeader = (BspHeader *)buffer;
    newHeader->ident = COD_BSP_IDENT;
    newHeader->version = COD_BSP_VERSION;
    int pos = sizeof(BspHeader);
    for (i = 0; i < BSP_LUMP_COUNT; i++) {
        if (i == COD_LUMP_LIGHTMAPS) {
            if (mapInfo.numLightmaps && mapInfo.lightmaps) {
                newHeader->lumps[i].offset = pos;
                newHeader->lumps[i].length = lightSize;
                memcpy(buffer + pos, mapInfo.lightmaps, lightSize);
                pos += lightSize;
            } else {
                newHeader->lumps[i].offset = 0;
                newHeader->lumps[i].length = 0;
            }
        } else {
            newHeader->lumps[i].offset = 0;
            newHeader->lumps[i].length = header->lumps[i].length;
            if (newHeader->lumps[i].length) {
                memcpy(buffer + pos, source + header->lumps[i].offset,
                       newHeader->lumps[i].length);
                newHeader->lumps[i].offset = pos;
                pos += newHeader->lumps[i].length;
            }
        }
    }
    delete[] bspFile;
    bspFile = buffer;
    bspFileSize = newFileSize;
}

/*
=============
ConvertLightmaps

Convert floating-point samples to the BSP lightmap pages, applying the
minimum light and output overbright scale in ColorRGB::ToBytes.
=============
*/
void ConvertLightmaps(void)
{
    Lightmap *lightmaps = lightmapSamples;
    unsigned char (*bytes)[LIGHTMAP_SIZE][LIGHTMAP_SIZE][3] = mapInfo.lightmaps;
    int iLightmap, s, t;
    for (iLightmap = 0; iLightmap < mapInfo.numLightmaps; iLightmap++)
        for (t = 0; t < LIGHTMAP_SIZE; t++)
            for (s = 0; s < LIGHTMAP_SIZE; s++)
                lightmaps[iLightmap].samples[t][s].color.ToBytes(bytes[iLightmap][t][s]);
}

/*
=============
Q3Map_Write
=============
*/
int Q3Map_Write(void **out)
{
    if (mapInfo.errors)
        Q3Map_Error("skipping map writing due to previous errors\n");
    bool newLightmaps = false;
    if (numLightmaps != mapInfo.numLightmaps) {
        if (mapInfo.numLightmaps)
            Q3Map_Error("Lightmap count mismatch\n");
        if (!mapInfo.lightmaps)
            mapInfo.lightmaps = new unsigned char[numLightmaps][LIGHTMAP_SIZE][LIGHTMAP_SIZE][3];
        mapInfo.numLightmaps = numLightmaps;
        newLightmaps = true;
    }
    /* Geometry remains from the input BSP. */
    bool newGeometry = false;
    ConvertLightmaps();
    if (newLightmaps || newGeometry)
        RebuildBsp((BspHeader *)bspFile);
    *out = bspFile;
    return bspFileSize;
}

/*
=============
ColorRGB::ToBytes
=============
*/
inline void ColorRGB::ToBytes(unsigned char *rgb) const
{
    int R, G, B;
    R = (int)((255.0f / options.overbrightScale) * MaxFloat(options.minLight.r, r));
    G = (int)((255.0f / options.overbrightScale) * MaxFloat(options.minLight.g, g));
    B = (int)((255.0f / options.overbrightScale) * MaxFloat(options.minLight.b, b));
    rgb[0] = MinInt(255, MaxInt(0, R));
    rgb[1] = MinInt(255, MaxInt(0, G));
    rgb[2] = MinInt(255, MaxInt(0, B));
}

/*
=============
MaxInt
=============
*/
int MaxInt(int a, int b)
{
    return a < b ? b : a;
}

/*
=============
MinInt
=============
*/
int MinInt(int a, int b)
{
    return a < b ? a : b;
}

/* Combined surface-light scale. */
static const float surfaceLightScale = colorComponents * diffuseLightScale * sunLightScale;
