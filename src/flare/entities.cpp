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

/* Map entities, model instances and light-source creation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "code/game/surfaceflags.h"



#include "flare/map.h"

#include "flare/shaders.h"
#include "flare/polygon.h"
#include "flare/options.h"
#include "flare/print.h"
#include "flare/preview.h"


static const float lightPowerScale = 4.0f / 255.0f;

#define MAX_MAP_ENTITIES 8192
#define POLYFLAG_NOSHADOW POLYFLAG_0x01
#define POLYFLAG_ALPHASHADOW POLYFLAG_0x02
#define POLYFLAG_VISUAL_ONLY 0x08
#define POLYFLAG_MODEL_SURFACE 0x20
#define SHADER_TRANSPARENT (SHADERFLAG_ALPHA_TEST | SHADERFLAG_ALPHA_BLEND | SHADERFLAG_0x08)
#define SHADER_NOSHADOW_MASK (SHADERFLAG_0x10 | SHADERFLAG_0x20 | SHADERFLAG_0x40)

MapInfo mapInfo;
extern float MaxFloat(float a, float b);
extern "C" BspEntity entities[];
#include "flare/parse.h"
extern void BuildEntity(MapInfo *map, MapEntity *entity);

void FreeEntity(MapEntity *entity);
bool Q3Map_ReadEntities(MapInfo *map, const char *entityText);
const char *ValueForKey(const MapEntity *entity, const char *key);
Vector3d VectorForKey(const MapEntity *entity, const char *key);
float FloatForKey(const MapEntity *entity, const char *key);
int IntForKey(const MapEntity *entity, const char *key);
MapEntity *FindTargetEntity(MapInfo *map, const char *target);
static void *XModelAlloc(unsigned int size);
bool Q3Map_ParseEntity(MapInfo *map, const char **text, int sourceIndex);
unsigned short SurfaceFlags(MapInfo *map, int shaderIndex, int entityIndex);

/*
=============================================================================

Entity loading

=============================================================================
*/

/*
=============
FreeEntity
=============
*/
void FreeEntity(MapEntity *entity)
{
    delete[] entity->keys[0];
    delete[] entity->keys;
    delete[] entity->values;
}

/*
=============
Q3Map_ReadEntities

Parse the complete entity list before building lights and models, so
spotlight targets can refer to entities that appear later in the lump.
=============
*/
bool Q3Map_ReadEntities(MapInfo *map, const char *entityText)
{
    map->entities = new MapEntity[MAX_MAP_ENTITIES];
    if (!map->entities)
        Q3Map_Error("Q3Map_ReadEntities: couldn't allocate storage for map entities (%i bytes)\n",
                    MAX_MAP_ENTITIES * sizeof(MapEntity));
    const char *text = entityText;
    char token[2048];
    int i;
    for (i = 0; GetToken(&text, token, sizeof(token), true); i++) {
        if (token[0] == '{') {
            if (!Q3Map_ParseEntity(map, &text, i))
                return false;
        } else {
            Q3Map_Warning(1, "Unexpected token '%s' in entity lump, stopped parsing\n", token);
            return false;
        }
    }
    for (i = 0; i < map->numEntities; i++)
        BuildEntity(map, &map->entities[i]);
    return true;
}

extern "C" void AngleVectors(const float *angles, float *forward, float *right, float *up);

#include "flare/light.h"

/*
=============
ColorRGB::operator*=
=============
*/
inline void ColorRGB::operator*=(float scale)
{
    r *= scale;
    g *= scale;
    b *= scale;
}

/*
=============
ColorRGB::Normalize
=============
*/
inline void ColorRGB::Normalize()
{
    float max = MaxFloat(r, MaxFloat(g, b));
    if (max > 0) {
        float scale = 1.0f / max;
        r = scale * r;
        b = scale * b;
        g = scale * g;
    }
}

/*
=============
BuildEntity

Apply worldspawn lighting settings unless overridden by command-line
options.  Other entities contribute lights, models or spotlight targets.
=============
*/
void BuildEntity(MapInfo *map, MapEntity *entity)
{
    float ambient;
    const char *classname = ValueForKey(entity, "classname");
    if (!stricmp(classname, "worldspawn")) {
        if (ValueForKey(entity, "ambient") && ValueForKey(entity, "_color")) {
            Vector3d color = VectorForKey(entity, "_color");
            ambient = FloatForKey(entity, "ambient");
            options.ambientColor.r = (float)color.x;
            options.ambientColor.g = (float)color.y;
            options.ambientColor.b = (float)color.z;
            options.ambientColor.Normalize();
            options.ambientIntensity = ambient;
        }
        if (!options.sunColorSet && ValueForKey(entity, "suncolor")) {
            Vector3d color = VectorForKey(entity, "suncolor");
            options.sunColorSet = true;
            options.sunColor.r = (float)color.x;
            options.sunColor.g = (float)color.y;
            options.sunColor.b = (float)color.z;
        }
        if (!options.sunLightSet && ValueForKey(entity, "sunlight")) {
            options.sunLightSet = true;
            options.sunLight = FloatForKey(entity, "sunlight");
        }
        if (!options.sunDiffuseColorSet && ValueForKey(entity, "sundiffusecolor")) {
            Vector3d color = VectorForKey(entity, "sundiffusecolor");
            options.sunDiffuseColorSet = true;
            options.sunDiffuseColor.r = (float)color.x;
            options.sunDiffuseColor.g = (float)color.y;
            options.sunDiffuseColor.b = (float)color.z;
        }
        if (!options.sunDiffuseFractionSet && ValueForKey(entity, "diffuseFraction")) {
            options.sunDiffuseFractionSet = true;
            options.sunDiffuseFraction = FloatForKey(entity, "diffuseFraction");
            if (options.sunDiffuseFraction < 0) {
                options.sunDiffuseFraction = 0;
                Q3Map_Warning(1, "diffuseFraction < 0; should be in range [0-1]\n");
            } else if (options.sunDiffuseFraction > 1) {
                options.sunDiffuseFraction = 1;
                Q3Map_Warning(1, "diffuseFraction > 1; should be in range [0-1]\n");
            }
        }
        if (!options.minLightSet && ValueForKey(entity, "minlight") && ValueForKey(entity, "minlightcolor")) {
            Vector3d color = VectorForKey(entity, "minlightcolor");
            float intensity = FloatForKey(entity, "minlight");
            options.minLight.r = (float)color.x;
            options.minLight.g = (float)color.y;
            options.minLight.b = (float)color.z;
            options.minLight.Normalize();
            options.minLight.r *= intensity;
            options.minLight.g *= intensity;
            options.minLight.b *= intensity;
            options.minLightSet = true;
        }
        if (!options.sunDirectionSet) {
            float forward[3];
            if (ValueForKey(entity, "sundirection")) {
                Vector3d direction = VectorForKey(entity, "sundirection");
                float angles[3];
                angles[0] = (float)direction.x;
                angles[1] = (float)direction.y;
                angles[2] = (float)direction.z;
                AngleVectors(angles, forward, 0, 0);
                options.sunDirectionSet = true;
                options.sunDirection.x = forward[0];
                options.sunDirection.y = forward[1];
                options.sunDirection.z = forward[2];
            }
        }
        entity->modelIndex = 0;
        entity->origin.Clear();
        entity->axis[0] = Vector3d(1, 0, 0);
        entity->axis[1] = Vector3d(0, 1, 0);
        entity->axis[2] = Vector3d(0, 0, 1);
    } else if (!stricmp(classname, "light")) {
        Q3Map_BuildLight(map, entity);
        entity->modelIndex = -1;
    } else if (!stricmp(classname, "misc_model")) {
        Q3Map_BuildMiscModel(map, entity);
        entity->modelIndex = -1;
    } else {
        const char *model = ValueForKey(entity, "model");
        if (model && model[0] == '*') {
            entity->modelIndex = atoi(model + 1);
            if (entity->modelIndex == 0) {
                Q3Map_Warning(1, "Entity %i is trying to use the world as it's brush model\n", entity->sourceIndex);
                entity->modelIndex = -1;
                return;
            }
            Vector3d angles3d;
            entity->origin = VectorForKey(entity, "origin");
            angles3d = VectorForKey(entity, "angles");
            float angles[3], forward[3], right[3], up[3];
            angles[0] = (float)angles3d.x;
            angles[1] = (float)angles3d.y;
            angles[2] = (float)angles3d.z;
            AngleVectors(angles, forward, right, up);
            entity->axis[0].x = forward[0];
            entity->axis[0].y = forward[1];
            entity->axis[0].z = forward[2];
            entity->axis[1].x = -right[0];
            entity->axis[1].y = -right[1];
            entity->axis[1].z = -right[2];
            entity->axis[2].x = up[0];
            entity->axis[2].y = up[1];
            entity->axis[2].z = up[2];
        } else if (ValueForKey(entity, "targetname")) {
            entity->origin = VectorForKey(entity, "origin");
            entity->modelIndex = -1;
        }
    }
}

/*
=============================================================================

Entity properties

=============================================================================
*/

/*
=============
ValueForKey
=============
*/
const char *ValueForKey(const MapEntity *entity, const char *key)
{
    int i;
    for (i = 0; i < entity->numKeys; i++) {
        if (!stricmp(entity->keys[i], key))
            return entity->values[i];
    }
    return NULL;
}

/*
=============
VectorForKey
=============
*/
Vector3d VectorForKey(const MapEntity *entity, const char *key)
{
    const char *value;
    Vector3d vector;
    value = ValueForKey(entity, key);
    if (value) {
        if (sscanf(value, "%lg %lg %lg", &vector.x, &vector.y, &vector.z) == 3)
            return vector;
        if (sscanf(value, "%lg, %lg, %lg", &vector.x, &vector.y, &vector.z) == 3)
            return vector;
        if (sscanf(value, "(%lg %lg %lg)", &vector.x, &vector.y, &vector.z) == 3)
            return vector;
        if (sscanf(value, "(%lg, %lg, %lg)", &vector.x, &vector.y, &vector.z) == 3)
            return vector;
        Q3Map_Warning(2, "key '%s' has value '%s' which is not a vector\n", key, value);
    }
    return Vector3d(0.0, 0.0, 0.0);
}

/*
=============
FloatForKey
=============
*/
float FloatForKey(const MapEntity *entity, const char *key)
{
    const char *value;
    value = ValueForKey(entity, key);
    if (!value)
        return 0.0f;
    return (float)atof(value);
}

/*
=============================================================================

Light sources

=============================================================================
*/

/*
=============
Vector3d::NormalizeLength
=============
*/
inline double Vector3d::NormalizeLength(void)
{
    double length = sqrt(*this * *this);
    if (length != 0) {
        double inverse = 1.0 / length;
        x *= inverse;
        y *= inverse;
        z *= inverse;
    }
    return length;
}

/*
=============
Q3Map_BuildLight

Choose a point, linear or targeted spot light from the entity properties.
Normalize its color and apply the configured intensity and falloff.
=============
*/
void Q3Map_BuildLight(MapInfo *map, MapEntity *entity)
{
    PointLight *pLight;
    LinearLight *pLinearLight;
    SpotLight *pSpotLight;
    AdjustedPointLight *pAdjustedPointLight;
    AdjustedSpotLight *adjustedSpot;
    const char *targetname;
    bool linear;
    int lightFlags;
    float intensity;
    /* Start with no overbright adjustment. */
    float overbright = 0;
    float lightPower;
    float brightness;
    Vector3d vColor;

    lightFlags = IntForKey(entity, "spawnflags");
    linear = (lightFlags & 1) ? true : false;
    overbright = FloatForKey(entity, "overbrightShift");
    if (overbright < 0 || overbright > 1) {
        Q3Map_Warning(1, "overbrightShift on entity %i is not in range [0-1], so it is ignored\n",
                      entity - mapInfo.entities);
        overbright = 0;
    }

    targetname = ValueForKey(entity, "target");
    if (targetname) {
        pLinearLight = 0;
        pAdjustedPointLight = 0;
        if (overbright != 0) {
            adjustedSpot = new AdjustedSpotLight;
            pSpotLight = adjustedSpot;
        } else {
            adjustedSpot = 0;
            pSpotLight = new SpotLight;
        }
        if (!pSpotLight)
            Q3Map_Error("Q3Map_BuildLight: couldn't allocate spotlight (%i bytes)\n", sizeof(SpotLight));
        pLight = pSpotLight;
    } else {
        pSpotLight = 0;
        adjustedSpot = 0;
        if (linear) {
            pAdjustedPointLight = 0;
            pLinearLight = new LinearLight;
            if (!pLinearLight)
                Q3Map_Error("Q3Map_BuildLight: couldn't allocate linear light (%i bytes)\n", sizeof(LinearLight));
            pLight = pLinearLight;
        } else {
            pLinearLight = 0;
            if (overbright != 0) {
                pAdjustedPointLight = new AdjustedPointLight;
                if (!pAdjustedPointLight)
                    Q3Map_Error("Q3Map_BuildLight: couldn't allocate light (%i bytes)\n", sizeof(AdjustedPointLight));
                pLight = pAdjustedPointLight;
            } else {
                pAdjustedPointLight = 0;
                pLight = new PointLight;
                if (!pLight)
                    Q3Map_Error("Q3Map_BuildLight: couldn't allocate light (%i bytes)\n", sizeof(PointLight));
            }
        }
    }

    pLight->origin = VectorForKey(entity, "origin");
    intensity = FloatForKey(entity, "light");
    if (intensity == 0) {
        intensity = FloatForKey(entity, "_light");
        if (intensity == 0)
            intensity = 300;
    }
    lightPower = intensity * options.pointLightScale;
    brightness = lightPower * lightPowerScale;
    vColor = VectorForKey(entity, "_color");
    if (vColor * vColor == 0)
        vColor = Vector3d(1, 1, 1);
    pLight->color.r = (float)vColor.x;
    pLight->color.g = (float)vColor.y;
    pLight->color.b = (float)vColor.z;
    pLight->color.Normalize();
    pLight->color *= brightness;
    if (!stricmp(ValueForKey(entity, "classname"), "light")) {
        pLight->noLeafInsert = false;
        pLight->noOccluders = false;
        pLight->noHitSamples = false;
        pLight->isGlobal = false;
    }
    if (targetname) {
        double dist;
        MapEntity *pTargetEntity;
        double radius;
        pTargetEntity = FindTargetEntity(map, targetname);
        if (!pTargetEntity) {
            Q3Map_Warning(1, "spotlight at (%g, %g, %g) cannot find targetname '%s', skipping lightsource\n",
                          pLight->origin.x, pLight->origin.y, pLight->origin.z, targetname);
            return;
        }
        pSpotLight->direction = pSpotLight->origin - VectorForKey(pTargetEntity, "origin");
        dist = pSpotLight->direction.NormalizeLength();
        if (dist == 0) {
            Q3Map_Warning(1, "spotlight at (%g, %g, %g) is at same location as its target, skipping lightsource\n",
                          pLight->origin.x, pLight->origin.y, pLight->origin.z);
            return;
        }
        radius = FloatForKey(entity, "radius");
        if (radius == 0)
            radius = 64;
        pSpotLight->coneCosine = dist / sqrt(dist * dist + radius * radius);
        pSpotLight->exponent = IntForKey(entity, "exponent");
    } else if (pLinearLight) {
        pLinearLight->range = FloatForKey(entity, "fade");
        if (pLinearLight->range <= 0)
            pLinearLight->range = 1;
        pLinearLight->angleScaled = (lightFlags & 2) != 0;
        pLinearLight->range /= lightPower * options.linearLightScale;
        pLinearLight->color *= options.linearLightScale;
    }
    if (pAdjustedPointLight || adjustedSpot) {
        float constant;
        float scale;
        float maxRadius;
        float luminance;
        float nearRadius;
        float quadratic;
        luminance = pLight->color.Luminance();
        nearRadius = sqrt(luminance) * (1 - overbright);
        maxRadius = sqrt(luminance / 0.1f);
        scale = 9.0f;
        quadratic = (1.0f / (maxRadius * maxRadius - nearRadius * nearRadius)) * scale * luminance;
        constant = luminance - nearRadius * nearRadius * quadratic;
        if (pAdjustedPointLight) {
            pAdjustedPointLight->quadratic = quadratic;
            pAdjustedPointLight->constant = constant;
        } else {
            adjustedSpot->quadratic = quadratic;
            adjustedSpot->constant = constant;
        }
    }
    AddLightToScene(pLight);
}

/*
=============
IntForKey
=============
*/
int IntForKey(const MapEntity *entity, const char *key)
{
    const char *value;
    value = ValueForKey(entity, key);
    if (!value)
        return 0;
    return atoi(value);
}

/*
=============
FindTargetEntity
=============
*/
MapEntity *FindTargetEntity(MapInfo *map, const char *target)
{
    int i;
    const char *name;
    for (i = 0; i < map->numEntities; i++) {
        name = ValueForKey(&map->entities[i], "targetname");
        if (name && !strcmp(target, name))
            return &map->entities[i];
    }
    return NULL;
}

/*
=============================================================================

Model instances

=============================================================================
*/

#include "flare/model.h"

/*
=============
Q3Map_BuildMiscModel

Load transformed model surfaces into the world, or keep the model separate
and insert its bounding faces for shadow collection.
=============
*/
void Q3Map_BuildMiscModel(MapInfo *map, MapEntity *entity)
{
    Vector3d p[4];
    Bounds modelBox;
    MiscModel *pModel;
    const char *modelname;
    unsigned short sFlags;
    float right[3], forwardVector[3], angle[3], upVector[3];

    modelname = ValueForKey(entity, "model");
    if (!modelname) {
        Q3Map_Warning(1, "misc_model without a model\n");
        return;
    }
    if (strnicmp(modelname, "xmodel", 6) || (modelname[6] != '/' && modelname[6] != '\\')) {
        Q3Map_Warning(2, "flare only supports misc_model surfaces that are xmodels\n");
        return;
    }
    sFlags = POLYFLAG_MODEL_SURFACE;
    if (options.noModelShadows || (IntForKey(entity, "spawnflags") & 2))
        sFlags |= POLYFLAG_NOSHADOW;
    Vector3d scale(1, 1, 1);
    int i;
    if (ValueForKey(entity, "modelscale_vec"))
        scale = VectorForKey(entity, "modelscale_vec");
    else if (ValueForKey(entity, "modelscale"))
        scale *= FloatForKey(entity, "modelscale");
    entity->origin = VectorForKey(entity, "origin");
    Vector3d angles = VectorForKey(entity, "angles");
    angle[0] = (float)angles.x;
    angle[1] = (float)angles.y;
    angle[2] = (float)angles.z;
    AngleVectors(angle, forwardVector, right, upVector);
    entity->axis[0].x = forwardVector[0];
    entity->axis[0].y = forwardVector[1];
    entity->axis[0].z = forwardVector[2];
    entity->axis[0] *= scale.x;
    entity->axis[1].x = -right[0];
    entity->axis[1].y = -right[1];
    entity->axis[1].z = -right[2];
    entity->axis[1] *= scale.y;
    entity->axis[2].x = upVector[0];
    entity->axis[2].y = upVector[1];
    entity->axis[2].z = upVector[2];
    entity->axis[2] *= scale.z;
    if (options.worldModels)
        LoadXModelSurfaces(map, entity, modelname, sFlags, entity->origin, entity->axis);
    else {
        pModel = new MiscModel(map, modelname, sFlags, entity->origin, entity->axis);
        modelBox = pModel->GetBounds();
        for (i = 0; i < 6; i++) {
            int axis1;
            int otherSide;
            int axis2;
            Polygon *polygon;
            int s;
            int axis;
            s = i & 1;
            otherSide = s ^ 1;
            axis = i >> 1;
            axis1 = (axis + 1) % 3;
            axis2 = 3 - (axis + axis1);
            p[0][axis] = modelBox.points[s][axis];
            p[1][axis] = modelBox.points[s][axis];
            p[2][axis] = modelBox.points[s][axis];
            p[3][axis] = modelBox.points[s][axis];
            p[0][axis1] = modelBox.points[s][axis1];
            p[1][axis1] = modelBox.points[s][axis1];
            p[2][axis1] = modelBox.points[otherSide][axis1];
            p[3][axis1] = modelBox.points[otherSide][axis1];
            p[0][axis2] = modelBox.maxs[axis2];
            p[1][axis2] = modelBox.mins[axis2];
            p[2][axis2] = modelBox.mins[axis2];
            p[3][axis2] = modelBox.maxs[axis2];
            polygon = CreatePoly(sFlags | POLYFLAG_MODEL, 4, p, 0, 0, 0, -1, -1, false);
            if (polygon)
                polygon->info->model = pModel;
        }
        entity->origin.x = (modelBox.mins[0] + modelBox.maxs[0]) * 0.5f;
        entity->origin.y = (modelBox.mins[1] + modelBox.maxs[1]) * 0.5f;
        entity->origin.z = (modelBox.mins[2] + modelBox.maxs[2]) * 0.5f;
        entity->modelLoaded = true;
    }
}

#include <malloc.h>
#include "flare/xmodel.h"

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

/*
=============
Vector3d::operator*=

Multiply each component of this vector by scale.
=============
*/
inline void Vector3d::operator*=(double scale)
{
    x *= scale;
    y *= scale;
    z *= scale;
}

/*
=============
ColorRGB::Luminance

Return the weighted luminance of the red, green and blue channels.
=============
*/
inline float ColorRGB::Luminance() const
{
    return r * 0.299f + g * 0.587f + b * 0.114f;
}
XModel_s *XModelPrecache(const char *name, int loadSurfaces,
                         void *(*alloc)(unsigned int), void *(*allocTemp)(unsigned int));

/*
=============
LoadXModelSurfaces

Prefer the shadow model when available.  Evaluate its base pose and turn
each surface triangle into a transformed polygon with shader properties.
=============
*/
void LoadXModelSurfaces(MapInfo *map, MapEntity *entity, const char *name,
                       unsigned short flags, const Vector3d &origin, const Vector3d *axis)
{
    char shadowName[128];
    strcpy(shadowName, "shadow_");
    strcat(shadowName, name + 7);
    bool shadowModel = true;
    XModel_s *model = XModelPrecache(shadowName, 1, XModelAlloc, XModelAlloc);
    if (!model) {
        shadowModel = !strnicmp(name + 7, "shadow_", 7);
        model = XModelPrecache(name + 7, 1, XModelAlloc, XModelAlloc);
    }
    if (!model) {
        Q3Map_Warning(1, "failed to load misc_model '%s'\n", name);
        return;
    }

    Vector3d points[3];
    DObjModel_s modelList;
    modelList.model = model;
    modelList.boneName = 0;
    modelList.boneNames = 0;
    /* Skeletal transforms use 16-byte-aligned object storage. */
    __declspec(align(16)) DObj_s object;
    DObjCreate(&modelList, 1, 0, &object, 0);
    DObj_s *obj = &object;
    int skelSize = DObjGetAllocSkelSize(obj);
    DObjCreateSkel(obj, _alloca(skelSize));
    int partBits[4];
    memset(partBits, 255, sizeof(partBits));
    DObjCalcAnim(obj, partBits);
    DObjCalcSkel(obj, partBits);
    DObjSkelMat *matrices = DObjGetMatrixArray(obj, 0);
    int lod = 0;
    int numSurfaces = DObjGetNumSurfaces(obj, &lod);
    DObjSurface_s *surfaces = (DObjSurface_s *)_alloca(numSurfaces * sizeof(DObjSurface_s));
    DObjGetSurfaces(obj, surfaces, partBits, &lod);
    Vector3d mins(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3d maxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = 0; i < numSurfaces; i++) {
        XSurface_s *surface = DObjGetSurface(obj, (short)surfaces[i].modelIndex, (short)surfaces[i].surfaceIndex, &lod);
        char shaderName[64];
        sprintf(shaderName, "skins/%s", DObjGetSurfaceName(obj, (short)surfaces[i].modelIndex, (short)surfaces[i].surfaceIndex, &lod));
        short shaderIndex = Q3Map_RequestShader(shaderName);
        unsigned short surfaceFlags = SurfaceFlags(map, shaderIndex, 0);
        surfaceFlags |= POLYFLAG_MODEL_SURFACE;
        surfaceFlags |= flags;
        if ((surfaceFlags & POLYFLAG_ALPHASHADOW) && (!shadowModel || options.noModelAlphaShadows)) {
            surfaceFlags |= POLYFLAG_NOSHADOW;
            surfaceFlags &= ~POLYFLAG_ALPHASHADOW;
        }
        int numTriangles = XSurfaceGetNumTris(surface);
        if (numTriangles > 0) {
            unsigned short triangles[16384][3];
            float texCoords[8192][2];
            float vertices[8192][3];
            XSurfaceGetTris(surface, triangles[0], 0);
            XSurfaceGetVerts(surface, matrices, vertices[0], texCoords[0], 0);
            unsigned short *index = triangles[0];
            for (int j = 0; j < numTriangles; j++) {
                float texture[3][2];
                for (int k = 0; k < 3; k++, index++) {
                    points[k] = entity->origin + vertices[*index][0] * axis[0]
                                              + vertices[*index][1] * axis[1]
                                              + vertices[*index][2] * axis[2];
                    texture[k][0] = texCoords[*index][0];
                    texture[k][1] = texCoords[*index][1];
                    if (maxs.x < points[k].x)
                        maxs.x = points[k].x;
                    if (mins.x > points[k].x)
                        mins.x = points[k].x;
                    if (maxs.y < points[k].y)
                        maxs.y = points[k].y;
                    if (mins.y > points[k].y)
                        mins.y = points[k].y;
                    if (maxs.z < points[k].z)
                        maxs.z = points[k].z;
                    if (mins.z > points[k].z)
                        mins.z = points[k].z;
                }
                Polygon *poly = CreatePoly(surfaceFlags, 3, points, 0, 0, texture, shaderIndex, -1, false);
                if (poly && (poly->info->flags & POLYFLAG_VISUAL_ONLY) && !options.keepVisualOnlyPolys)
                    poly->Free();
            }
        }
    }
    entity->origin = (mins + maxs) * 0.5;
    entity->modelLoaded = true;
}

/*
=============
XModelAlloc
=============
*/
static void *XModelAlloc(unsigned int size)
{
    return new unsigned char[size];
}

/*
=============================================================================

Entity parsing and filtering

=============================================================================
*/

/*
=============
Q3Map_ParseEntity

Read one entity, pack its key/value strings into shared storage and keep
it only when it affects lighting or supplies a spotlight target.
=============
*/
bool Q3Map_ParseEntity(MapInfo *map, const char **text, int sourceIndex)
{
    struct KeyPair {
        char key[64];
        char value[512];
    } pairs[64];
    int numKeypairs = 0;
    bool hasClassname = false;
    while (GetToken(text, pairs[numKeypairs].key, sizeof(pairs[numKeypairs].key), true)) {
        if (pairs[numKeypairs].key[0] == '}') {
            if (numKeypairs < 1) {
                Q3Map_Warning(1, "skipping entity with no keypairs\n");
                return true;
            }
            if (!hasClassname) {
                Q3Map_Warning(1, "skipping entity with no classname\n");
                return true;
            }
            MapEntity *entity = &map->entities[map->numEntities];
            entity->sourceIndex = sourceIndex;
            entity->numKeys = numKeypairs;
            entity->keys = new char *[numKeypairs];
            if (!entity->keys)
                Q3Map_Error("Q3Map_ParseEntity: couldn't allocate storage for keys (%i bytes)\n",
                            numKeypairs * sizeof(char *));
            entity->values = new char *[numKeypairs];
            if (!entity->values)
                Q3Map_Error("Q3Map_ParseEntity: couldn't allocate storage for values (%i bytes)\n",
                            numKeypairs * sizeof(char *));
            int numChars = 0;
            int i;
            for (i = 0; i < numKeypairs; i++) {
                numChars += strlen(pairs[i].key) + 1;
                numChars += strlen(pairs[i].value) + 1;
            }
            /* Keys and values share one allocation, owned by keys[0]. */
            char *strings = new char[numChars];
            if (!strings)
                Q3Map_Error("Q3Map_ParseEntity: couldn't allocate storage for strings (%i bytes)\n", numChars);
            for (i = 0; i < numKeypairs; i++) {
                int lenKey = strlen(pairs[i].key) + 1;
                int lenValue = strlen(pairs[i].value) + 1;
                entity->keys[i] = strings;
                memcpy(entity->keys[i], pairs[i].key, lenKey);
                strings += lenKey;
                entity->values[i] = strings;
                memcpy(entity->values[i], pairs[i].value, lenValue);
                strings += lenValue;
            }
            if (!EntityAffectsLighting(entity))
                FreeEntity(entity);
            else {
                entity->modelLoaded = false;
                entity->bspEntity = &entities[sourceIndex];
                map->numEntities++;
            }
            return true;
        }
        if (!GetToken(text, pairs[numKeypairs].value, sizeof(pairs[numKeypairs].value), true)) {
            Q3Map_Warning(1, "unexpected end-of-lump while parsing key-pair in entity lump");
            return false;
        }
        if (!stricmp(pairs[numKeypairs].key, "classname"))
            hasClassname = true;
        numKeypairs++;
    }
    Q3Map_Warning(1, "unexpected end-of-lump while parsing entities\n");
    return false;
}


/*
=============
EntityAffectsLighting

Keep lighting and model entities.  Player starts set the preview camera
and are then discarded; target entities supply spotlight directions.
=============
*/
bool EntityAffectsLighting(const MapEntity *entity)
{
    const char *classname = ValueForKey(entity, "classname");
    if (!stricmp(classname, "worldspawn"))
        return true;
    if (!stricmp(classname, "light"))
        return true;
    if (!stricmp(classname, "misc_model"))
        return true;
    if (!stricmp(classname, "info_player_start") || !stricmp(classname, "info_player_deathmatch")) {
        GL_SetCameraOrigin(&(VectorForKey(entity, "origin") + Vector3d(0, 0, 56)));
        Vector3d angles = VectorForKey(entity, "angles");
        Vector3d direction;
        direction.x = cos(angles.y * (3.1415927f / 180.0));
        direction.y = sin(angles.y * (3.1415927f / 180.0));
        direction.z = 0;
        GL_SetCameraDirection(&direction);
        return false;
    }
    if (ValueForKey(entity, "targetname"))
        return true;
    else {
        const char *model = ValueForKey(entity, "model");
        if (model && model[0] == '*')
            return true;
    }
    return false;
}

/*
=============
Q3Map_ReadEntityText
=============
*/
bool Q3Map_ReadEntityText(const char *text)
{
    memset(&mapInfo, 0, sizeof(mapInfo));
    if (!Q3Map_ReadEntities(&mapInfo, text)) {
        for (int i = 0; i < mapInfo.numEntities; i++)
            FreeEntity(&mapInfo.entities[i]);
        delete[] mapInfo.entities;
        memset(&mapInfo, 0, sizeof(mapInfo));
        return false;
    }
    return true;
}

/*
=============
SurfaceFlags

Translate BSP and shader properties into polygon flags.
=============
*/
unsigned short SurfaceFlags(MapInfo *map, int shaderIndex, int entityIndex)
{
    unsigned short flags = 0;
    if (entityIndex > 0)
        flags |= POLYFLAG_NOSHADOW;
    if (shaderIndex >= 0) {
        if (shaderIndex < map->numShaders) {
            if (map->shaders[shaderIndex].surfaceFlags & SURF_SKY)
                flags |= POLYFLAG_SKY;
            if (map->shaders[shaderIndex].surfaceFlags & SURF_ALPHASHADOW)
                flags |= POLYFLAG_ALPHASHADOW;
        } else {
            if (shaders[shaderIndex].flags & SHADERFLAG_0x01)
                flags |= POLYFLAG_SKY;
            if (shaders[shaderIndex].flags & SHADER_TRANSPARENT)
                flags |= POLYFLAG_ALPHASHADOW;
        }
        if (shaders[shaderIndex].flags & SHADER_TRANSPARENT)
            flags |= POLYFLAG_ALPHASHADOW;
        if (shaders[shaderIndex].flags & SHADER_NOSHADOW_MASK)
            flags |= POLYFLAG_NOSHADOW;
    }
    return flags;
}
