/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

XModel cache and transformed misc_model occluders.
===========================================================================
*/
#include <malloc.h>
#include <stdio.h>
#include "flare/xmodel.h"
#include "flare/options.h"
#include <string.h>

#include "flare/model.h"
#include "flare/polygon.h"
#include "flare/light.h"
#include "flare/occluderpairs.h"

#define MAX_POLY_OCCLUDERS 32768

#define POLYFLAG_NOSHADOW 0x01
#define POLYFLAG_ALPHASHADOW 0x02
#define POLYFLAG_MODEL_SURFACE 0x20

/*
=============
XModelAlloc
=============
*/
void *XModelAlloc(size_t size)
{
    return ::operator new(size);
}

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

static ModelAsset *modelList = 0;

void Q3Map_Warning(int level, const char *format, ...);
short Q3Map_RequestShader(const char *name);
unsigned short SurfaceFlags(MapInfo *map, int shaderIndex, int entityIndex);

/*
=============
LoadModel

Prefer shadow geometry and store its posed positions, UVs and triangle indices.
=============
*/
ModelAsset *LoadModel(MapInfo *map, const char *name)
{
    if (strnicmp(name, "xmodel", 6) || (name[6] != '/' && name[6] != '\\'))
    {
        Q3Map_Warning(2, "misc_model '%s' is invalid; flare only supports misc_model surfaces that are xmodels\n", name);
        return 0;
    }

    char shadowName[128];
    strcpy(shadowName, "shadow_");
    strcat(shadowName, name + 7);
    bool shadow = true;
    XModel_s *xmodel = XModelPrecache(shadowName, 1, XModelAlloc, XModelAlloc);
    if (!xmodel)
    {
        shadow = !strnicmp(name + 7, "shadow_", 7);
        xmodel = XModelPrecache(name + 7, 1, XModelAlloc, XModelAlloc);
        if (!xmodel)
        {
            Q3Map_Warning(1, "failed to load misc_model '%s'\n", name);
            return 0;
        }
    }

    DObjModel_s objModel;
    objModel.model = xmodel;
    objModel.boneName = 0;
    objModel.boneNames = 0;
    DObj_s obj;
    DObjCreate(&objModel, 1, 0, &obj, 0);
    DObjCreateSkel(&obj, _alloca(DObjGetAllocSkelSize(&obj)));
    int partBits[4];
    memset(partBits, -1, sizeof(partBits));
    DObjCalcAnim(&obj, partBits);
    DObjCalcSkel(&obj, partBits);
    DObjSkelMat *mats = DObjGetMatrixArray(&obj, 0);
    int lod = 0;
    int numSurfaces = DObjGetNumSurfaces(&obj, &lod);
    DObjSurface_s *surfaceInfo = (DObjSurface_s *)_alloca(numSurfaces * sizeof(DObjSurface_s));
    DObjGetSurfaces(&obj, surfaceInfo, partBits, &lod);

    ModelAsset *model = new ModelAsset;
    strcpy(model->name, name);
    model->numSurfaces = (unsigned short)numSurfaces;
    model->surfaces = new ModelSurface[numSurfaces];
    memset(model->surfaces, 0, numSurfaces * sizeof(ModelSurface));
    model->next = modelList;
    modelList = model;

    for (int i = 0; i < numSurfaces; i++)
    {
        XSurface_s *surface = DObjGetSurface(&obj, (short)surfaceInfo[i].modelIndex,
                                           (short)surfaceInfo[i].surfaceIndex, &lod);
        char skin[64];
        sprintf(skin, "skins/%s", DObjGetSurfaceName(&obj, (short)surfaceInfo[i].modelIndex,
                                                   (short)surfaceInfo[i].surfaceIndex, &lod));
        model->surfaces[i].material = Q3Map_RequestShader(skin);
        model->surfaces[i].flags = SurfaceFlags(map, model->surfaces[i].material, 0);
        model->surfaces[i].flags |= POLYFLAG_MODEL_SURFACE;
        if ((model->surfaces[i].flags & POLYFLAG_ALPHASHADOW) && (!shadow || options.noModelAlphaShadows))
        {
            model->surfaces[i].flags |= POLYFLAG_NOSHADOW;
            model->surfaces[i].flags &= ~POLYFLAG_ALPHASHADOW;
        }
        model->surfaces[i].numVertices = XSurfaceGetNumVerts(surface);
        model->surfaces[i].numIndices = 3 * XSurfaceGetNumTris(surface);
        model->surfaces[i].vertices = new float[model->surfaces[i].numVertices][3];
        model->surfaces[i].texcoords = new float[model->surfaces[i].numVertices][2];
        model->surfaces[i].indices = new unsigned short[model->surfaces[i].numIndices];
        XSurfaceGetTris(surface, model->surfaces[i].indices, 0);
        XSurfaceGetVerts(surface, mats, &model->surfaces[i].vertices[0][0],
                         &model->surfaces[i].texcoords[0][0], 0);
    }
    return model;
}

/*
=============
FindModel

Reuse a cached model or load it on first use.
=============
*/
static ModelAsset *FindModel(MapInfo *map, const char *name)
{
    ModelAsset *model;
    for (model = modelList; model; model = model->next)
    {
        if (!stricmp(name, model->name))
            return model;
    }
    return LoadModel(map, name);
}

/*
=============
MiscModel::ShouldIgnore

Skip missing models, disabled shadows and the receiving entity's own model.
=============
*/
bool MiscModel::ShouldIgnore(int entityNum) const
{
    if (!model || (flags & POLYFLAG_NOSHADOW))
        return true;
    if (this->entityNum == entityNum)
        return true;
    return false;
}

/*
=============
MiscModel::MiscModel

Share the cached surfaces and calculate the instance's world-space bounds.
=============
*/
MiscModel::MiscModel(MapInfo *map, const char *name, unsigned short flags,
                     const Vector3d &origin, const Vector3d axis[3])
{
    this->origin = origin;
    this->axis[0] = axis[0];
    this->axis[1] = axis[1];
    this->axis[2] = axis[2];
    this->flags = flags;
    frameStamp = 0;
    entityNum = 0;
    model = FindModel(map, name);
    if (!model)
    {
        memset(&bounds, 0, sizeof(bounds));
        return;
    }

    bounds.Clear();
    for (int i = 0; i < model->numSurfaces; i++)
    {
        for (int j = 0; j < model->surfaces[i].numVertices; j++)
        {
            const float *v = model->surfaces[i].vertices[j];
            bounds.AddPoint(this->origin + v[0] * this->axis[0]
                            + v[1] * this->axis[1] + v[2] * this->axis[2]);
        }
    }
}

Occluder *AddOccluder(const Polygon *polygon);
void CompactLastPolygon();

/*
=============
MiscModel::CollectOccluders

Transform the model triangles into world space and clip them to the light.
=============
*/
void MiscModel::CollectOccluders(int entityNum, Occluder **head, Occluder **tail)
{
    this->entityNum = entityNum;
    Polygon *firstPoly = 0;
    int numPolys = 0;
    /* Follow the model member through polygon allocation and clipping. */
    ModelAsset *const &asset = model;
    for (int i = 0; i < asset->numSurfaces; i++)
    {
        unsigned short surfaceFlags = asset->surfaces[i].flags | flags;
        if (surfaceFlags & POLYFLAG_NOSHADOW)
            continue;
        for (int j = 0; j < asset->surfaces[i].numIndices; j += 3)
        {
            int i0 = asset->surfaces[i].indices[j];
            int i1 = asset->surfaces[i].indices[j + 1];
            int i2 = asset->surfaces[i].indices[j + 2];
            Vector3d points[3];
            float st[3][2];
            points[0] = origin + asset->surfaces[i].vertices[i0][0] * axis[0]
                               + asset->surfaces[i].vertices[i0][1] * axis[1]
                               + asset->surfaces[i].vertices[i0][2] * axis[2];
            points[1] = origin + asset->surfaces[i].vertices[i1][0] * axis[0]
                               + asset->surfaces[i].vertices[i1][1] * axis[1]
                               + asset->surfaces[i].vertices[i1][2] * axis[2];
            points[2] = origin + asset->surfaces[i].vertices[i2][0] * axis[0]
                               + asset->surfaces[i].vertices[i2][1] * axis[1]
                               + asset->surfaces[i].vertices[i2][2] * axis[2];
            st[0][0] = asset->surfaces[i].texcoords[i0][0];
            st[0][1] = asset->surfaces[i].texcoords[i0][1];
            st[1][0] = asset->surfaces[i].texcoords[i1][0];
            st[1][1] = asset->surfaces[i].texcoords[i1][1];
            st[2][0] = asset->surfaces[i].texcoords[i2][0];
            st[2][1] = asset->surfaces[i].texcoords[i2][1];
            Polygon *poly = CreatePoly(surfaceFlags, 3, points, 0, 0, st,
                                       (unsigned short)asset->surfaces[i].material, -1, false);
            if (!poly)
                continue;
            if (!(surfaceFlags & POLYFLAG_ALPHASHADOW) && currentLight->IsBackFacing(poly))
            {
                poly->Free();
                continue;
            }
            Polygon *clipped = currentLight->ClipPolygon(poly);
            if (!clipped)
            {
                poly->Free();
                continue;
            }
            if (clipped != poly)
                CompactLastPolygon();
            if (!firstPoly)
                firstPoly = poly;
            numPolys++;
        }
    }
    if (numPolys > 0)
    {
        for (int n = 0; n < numPolys; n++)
        {
            Occluder *occluder = AddOccluder(firstPoly + n);
            if (!*tail)
                *tail = *head = occluder;
            else
            {
                occluder->next = *head;
                *head = occluder;
            }
            polyOccluders[numPolyOccluders++] = occluder;
        }
    }
}
