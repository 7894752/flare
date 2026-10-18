/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Scene lights, diffuse sunlight sampling and polygon shadowing.
===========================================================================
*/
#include "flare/math_types.h"
#include <stdlib.h>
#include "common/q_round.h"

#include "flare/light.h"
#include "flare/lightmap.h"
#include "flare/map.h"
#include "flare/shaders.h"
#include "flare/model.h"
#include "flare/options.h"
#include "flare/print.h"
#include "flare/geometry.h"
#include "flare/occluderpairs.h"
#include "flare/preview_step.h"
#include "flare/image.h"
/*
=============
Vector3d::operator+=

Add the corresponding components of v to this vector.
=============
*/
inline void Vector3d::operator+=(const Vector3d &v)
{
    x += v.x;
    y += v.y;
    z += v.z;
}

#include "flare/traceresult.h"
#include "flare/color.h"
#include "polygon.h"

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2
#define SIDE_CROSS 3

#define MAX_ALPHA_SHADOW_POLYS 1024
#define MAX_OCCLUDERS_PER_POLY 32768
#define MAX_SUBDIVIDE_POINTS 516
#define NUM_SUBDIVIDE_BUFFERS 4
#define LIGHTMAP_SIZE 512
#define AREA_LIGHT_PI 3.14159265358979323846f
#define MIN_DIFFUSE_COSINE 0.01f
#define DIFFUSE_SAMPLE_ESCAPE_DISTANCE 16.0

#define POLYFLAG_NOSHADOW POLYFLAG_0x01
#define POLYFLAG_ALPHASHADOW POLYFLAG_0x02
#define POLYFLAG_VISUAL_ONLY 0x08
#define POLYFLAG_MODEL_SURFACE 0x20
#define POLYFLAG_PORTAL POLYFLAG_0x40

static const float diffusePolygonOffset = 0.5f;
static const float diffuseRayOffset = 0.125f;
static const float diffuseTraceDistance = 262144.0f;
static const float areaLightPlaneOffset = 0.25f;
static const float backSplashFraction = 0.05f;
static const float backSplashDistance = 24.0f;
static const float lightPickCosSquared = 0.99f;

struct SceneLight
{
    Light *light;
    bool selected;
    SceneLight *prev;
    SceneLight *next;
};

/* Shared scratch for polygon subdivision and its area-light callback. */
struct AreaLightCell
{
    ColorRGB color;
    Vector3d axisS;
    Vector3d axisT;
    Plane plane;
    float radius;
    bool backSplash;
    bool noOccluders;
    bool isGlobal;
    Light *source;
};


void CollectOccluders_r(BspNode *node, bool clip);
void FilterDiffuseSunlight_r(BspNode *node, Polygon *poly);


typedef float vec2_t[2];

struct DiffuseSample
{
    Vector3d direction;
    double weight;
};


Polygon *alphaShadowPolys[MAX_ALPHA_SHADOW_POLYS] = { 0 };
int numSceneLights = 0;
SceneLight *sceneLights = 0;
SceneLight *pickedSceneLight = 0;
Light *currentLight = 0;
int occluderPasses = 0;
bool buildingOccluders = false;
Light *firstSunLight = 0;
int numAlphaShadowPolys = 0;
int rejectedOccluders = 0;
int testedOccluders = 0;
Polygon *diffuseSourcePolygon = 0;
int numDiffuseSamples = 0;
DiffuseSample *diffuseSamples = 0;
void (*diffuseSunlightCallback)(Polygon *poly) = 0;
bool cellHasDiffuseSunlight = false;
Vector3d lightOrigin(0, 0, 0);
Vector3d lightDirection(1, 0, 0);

AreaLightCell areaLightState;






void CreateAreaLightCallback(float area, const float *st, const float (*points)[2], int count);
bool IsAreaLightVisible_r(Polygon *poly, int index, int planeIndex);
void PrepareLightOccluders(void);
bool LightPolygonOccluders_r(Polygon *poly, int index, int planeIndex);
void PushAlphaShadowPoly(Polygon *poly);
void PopAlphaShadowPoly(void);
void AccumulateLightmapLighting(float area, const float *st, const vec2_t *points, int count);
bool HasAlphaShadowPolys(void);
inline bool PolygonInFrontOfPlane(const Polygon *polygon, const Plane &plane);
bool CellHasDiffuseSunlight(int cellIndex);

/*
=============
AddLightToScene
=============
*/
void AddLightToScene(Light *light)
{
    SceneLight *node;

    node = new SceneLight;
    if (!node)
        Q3Map_Error("AddLightToScene: couldn't allocate linked list node (%i bytes)\n",
                   sizeof(SceneLight));

    node->light = light;
    node->next = sceneLights;
    node->prev = 0;
    node->selected = false;
    if (sceneLights)
        sceneLights->prev = node;
    sceneLights = node;
    numSceneLights++;
}

/*
=============
GetPickedLightOrigin
=============
*/
const Vector3d &GetPickedLightOrigin(void)
{
    return lightOrigin;
}

/*
=============
GetPickedLightDirection
=============
*/
const Vector3d &GetPickedLightDirection(void)
{
    return lightDirection;
}

/*
=============
FloodBspFromLights
=============
*/
void FloodBspFromLights(BspNode *node)
{
    SceneLight *light;
    for (light = sceneLights; light; light = light->next)
        if (light->light->HasOrigin())
            FloodBspFromPoint(node, light->light->GetOrigin());
}

/*
=============
DrawSceneLights
=============
*/
void DrawSceneLights(void)
{
    SceneLight *light = sceneLights;
    while (light)
    {
        light->light->Draw(light->light == currentLight);
        light = light->next;
    }
}

/*
=============
PickSceneLight

Choose the nearest visible light inside the picking cone and move it to the
current position in the scene list for preview and compilation.
=============
*/
void PickSceneLight(const Vector3d &origin, const Vector3d &direction)
{
    if (buildingOccluders)
    {
        Q3Map_Printf("current light is locked\n");
        Preview_SetCvarInteger("r_picklight", 0);
        return;
    }

    SceneLight *best = 0;
    double bestDistance = FLT_MAX;
    SceneLight *light;
    for (light = sceneLights; light; light = light->next)
    {
        if (light->light->HasOrigin())
        {
            Vector3d lightOrigin = light->light->GetOrigin();
            Vector3d toLight = lightOrigin - origin;
            double distanceSquared = toLight * toLight;
            if (distanceSquared < bestDistance)
            {
                double projection = toLight * direction;
                if (projection > 0 && projection * projection > lightPickCosSquared * distanceSquared)
                {
                    if (bspRoot)
                    {
                        TraceResult trace;
                        TraceSegment(origin, lightOrigin, &trace, 0);
                        if (trace.hit)
                            continue;
                    }
                    best = light;
                    bestDistance = distanceSquared;
                }
            }
        }
    }

    if (best && best->light != currentLight)
    {
        if (!pickedSceneLight)
            pickedSceneLight = sceneLights;
        if (pickedSceneLight != best)
        {
            if (best->next == pickedSceneLight)
                pickedSceneLight = best;
            else
            {
                SceneLight *prev = pickedSceneLight->prev;
                SceneLight *prevNode = best->prev;
                SceneLight *nextNode = best->next;
                if (prev)
                    prev->next = best;
                else
                    sceneLights = best;
                if (prevNode)
                    prevNode->next = nextNode;
                else
                    sceneLights = nextNode;
                if (nextNode)
                    nextNode->prev = prevNode;
                best->prev = prev;
                best->next = pickedSceneLight;
                pickedSceneLight->prev = best;
                pickedSceneLight = best;
            }
        }
        currentLight = best->light;
        if (currentLight->HasOrigin())
            lightOrigin = currentLight->GetOrigin();
        if (currentLight->IsDirectional())
            lightDirection = currentLight->GetDirection();
    }
}

/*
=============
CreateAreaLight

Subdivide an emissive polygon in its plane. Cells provide sampled lighting;
the optional source light supplies the BSP record for the whole surface.
=============
*/
void CreateAreaLight(Polygon *poly, const ColorRGB &color, float backsplash)
{
    areaLightState.plane = poly->GetInfo()->plane;
    areaLightState.plane.dist = areaLightPlaneOffset + areaLightState.plane.dist;
    areaLightState.axisS = PerpendicularVector3d(areaLightState.plane.normal);
    areaLightState.axisT = areaLightState.plane.normal.Cross(areaLightState.axisS);

    vec2_t stPoints[NUM_SUBDIVIDE_BUFFERS][MAX_SUBDIVIDE_POINTS];
    float minS = FLT_MAX;
    float sMaxValue = -FLT_MAX;
    float tMin = FLT_MAX;
    float maxT = -FLT_MAX;
    int v;
    for (v = 0; v < poly->numPoints; v++)
    {
        stPoints[0][v][0] = (float)(areaLightState.axisS * poly->points[v]);
        stPoints[0][v][1] = (float)(areaLightState.axisT * poly->points[v]);
        if (minS > stPoints[0][v][0])
            minS = stPoints[0][v][0];
        if (sMaxValue < stPoints[0][v][0])
            sMaxValue = stPoints[0][v][0];
        if (tMin > stPoints[0][v][1])
            tMin = stPoints[0][v][1];
        if (maxT < stPoints[0][v][1])
            maxT = stPoints[0][v][1];
    }

    float stepSize = poly->info->flags & POLYFLAG_SKY ? options.sunDiffuseStep : options.areaLightStep;
    int numS = CeilToInt((sMaxValue - minS) / stepSize);
    int rows = CeilToInt((maxT - tMin) / stepSize);
    if (numS * rows == 0)
    {
        Q3Map_Warning(2, "surfacelight poly has no area");
        return;
    }
    float deltaS = (sMaxValue - minS) / numS;
    float tStep = (maxT - tMin) / rows;
    areaLightState.color = color;
    areaLightState.backSplash = options.backSplash && !(poly->info->flags & POLYFLAG_SKY);
    vec2_t uvCentroid;
    float area = PolygonCentroid2D(stPoints[0], poly->numPoints, uvCentroid) * 0.5f;
    double r = sqrt(areaLightState.color.Luminance() * area * options.oneOverMinIntensity / AREA_LIGHT_PI);
    areaLightState.radius = (float)r;
    areaLightState.noOccluders = false;
    areaLightState.isGlobal = (poly->info->flags & POLYFLAG_SKY) != 0;
    areaLightState.source = 0;
    if (!options.noLeafLights && !(poly->info->flags & POLYFLAG_SKY))
    {
        AreaLight *light = new AreaLight;
        if (!light)
            Q3Map_Error("CreateAreaLight: couldn't allocate light for BSP (%i bytes)\n", sizeof(AreaLight));
        light->color = areaLightState.color * area;
        light->noLeafInsert = true;
        light->noOccluders = false;
        light->noHitSamples = true;
        light->isGlobal = false;
        light->origin = uvCentroid[0] * areaLightState.axisS + uvCentroid[1] * areaLightState.axisT +
                        areaLightState.plane.normal * areaLightState.plane.dist;
        light->normal = areaLightState.plane.normal;
        light->axisS = areaLightState.axisS;
        light->axisT = areaLightState.axisT;
        light->width = sMaxValue - minS;
        light->height = maxT - tMin;
        areaLightState.source = light;
    }
    SubdividePolygon2D(stPoints, poly->numPoints, numS, rows, minS, tMin, deltaS, tStep,
                       CreateAreaLightCallback);
    if (areaLightState.backSplash)
    {
        PointLight *light = new PointLight;
        if (!light)
            Q3Map_Error("CreateAreaLight: couldn't allocate backsplash light (%i bytes)\n", sizeof(PointLight));
        light->color = areaLightState.color * area * backSplashFraction * backsplash;
        light->noLeafInsert = false;
        light->noOccluders = true;
        light->noHitSamples = false;
        light->isGlobal = false;
        light->origin = uvCentroid[0] * areaLightState.axisS + uvCentroid[1] * areaLightState.axisT +
                        areaLightState.plane.normal * (backSplashDistance + areaLightState.plane.dist);
        AddLightToScene(light);
    }
}

/*
=============
CreateAreaLightCallback

Convert a subdivision cell back to world space and attach it to its source
area light. Global sky cells can serve as their own source.
=============
*/
void CreateAreaLightCallback(float area, const float *st, const float (*points)[2], int count)
{
    AreaCellLight *areaLight = new AreaCellLight;
    int i;
    if (!areaLight)
        Q3Map_Error("CreateAreaLightCallback: couldn't allocate light (%i bytes)\n", sizeof(AreaCellLight));
    areaLight->origin = (st[0] * areaLightState.axisS + st[1] * areaLightState.axisT) +
                        areaLightState.plane.normal * areaLightState.plane.dist;
    areaLight->color = areaLightState.color;
    areaLight->SetPlane(areaLightState.plane);
    areaLight->SetRadius(areaLightState.radius);
    areaLight->cell.Resize(count);
    for (i = 0; i < count; i++)
    {
        areaLight->cell.AddPoint((points[i][0] * areaLightState.axisS + points[i][1] * areaLightState.axisT) +
                                areaLightState.plane.normal * areaLightState.plane.dist);
    }
    areaLight->noLeafInsert = false;
    areaLight->noOccluders = areaLightState.noOccluders;
    areaLight->noHitSamples = false;
    areaLight->isGlobal = areaLightState.isGlobal;
    if (areaLightState.source)
        areaLight->source = areaLightState.source;
    else if (areaLight->isGlobal)
        areaLight->source = areaLight;
    AddLightToScene(areaLight);
}

/*
=============
IsAreaLightVisible_r

Clip away the volumes behind opaque occluders. Any fragment left after
the last occluder makes the area visible; alpha shadows do not reject it.
=============
*/
bool IsAreaLightVisible_r(Polygon *poly, int index, int planeIndex)
{
    Polygon *front, *back;
    Occluder *occluder;
    int side;
    while (index < numPolyOccluders)
    {
        occluder = polyOccluders[index];
        if (!(occluder->polygon->info->flags & POLYFLAG_ALPHASHADOW))
        {
            while (planeIndex < occluder->polygon->numPoints)
            {
                side = SplitPolygonEpsilon(poly, occluder->sidePlanes[planeIndex], &front, &back);
                if (side == SIDE_CROSS)
                {
                    bool visible = IsAreaLightVisible_r(front, index + 1, 0);
                    front->Free();
                    if (!visible && planeIndex + 1 < occluder->polygon->numPoints)
                        visible = IsAreaLightVisible_r(back, index, planeIndex + 1);
                    back->Free();
                    return visible;
                }
                if (side == SIDE_FRONT)
                    break;
                if ((side == SIDE_BACK && planeIndex + 1 == occluder->polygon->numPoints) || side == SIDE_ON)
                    return false;
                planeIndex++;
            }
        }
        planeIndex = 0;
        index++;
    }
    return true;
}

/*
=============
Light_Compile

Create direct sunlight from sky polygons. Diffuse sunlight is either added to
that color, emitted by area cells, or left for the later sampling pass.
=============
*/
void Light_Compile(void)
{
    Color3f lightColor = options.sunColor;
    int i;
    if (options.noDiffuseSun)
        lightColor = Color3f(lightColor.r + options.sunDiffuseColor.r,
                            lightColor.g + options.sunDiffuseColor.g,
                            lightColor.b + options.sunDiffuseColor.b);
    for (i = 0; i < numPermanentPolygons; i++)
    {
        Polygon *poly = &polygons[i];
        if (!(poly->info->flags & POLYFLAG_SKY))
            continue;
        if (!options.noDiffuseSun && !options.sampleDiffuseSun)
            CreateAreaLight(poly, options.sunDiffuseColor, 0);
        if (options.sunDirection * poly->info->plane.normal < 0)
        {
            DirectionalLight *light = new SunLight;
            if (!light)
                Q3Map_Error("Light_Compile: couldn't allocate sun light (%i bytes)\n", sizeof(SunLight));
            light->color = lightColor;
            light->direction = options.sunDirection;
            light->skyPoly = poly;
            light->noLeafInsert = false;
            light->noOccluders = false;
            light->noHitSamples = false;
            light->isGlobal = false;
            AddLightToScene(light);
            if (!firstSunLight)
                firstSunLight = light;
        }
        if (!options.noLeafLights && options.sampleDiffuseSun && poly->GetInfo()->plane.normal.z < 0)
        {
            DirectionalLight *light = new SunLight;
            if (!light)
                Q3Map_Error("Light_Compile: couldn't allocate sun light (%i bytes)\n", sizeof(SunLight));
            light->color = options.sunDiffuseColor;
            light->direction = Vector3d(0, 0, 1);
            light->skyPoly = poly;
            light->noLeafInsert = false;
            light->noOccluders = false;
            light->noHitSamples = true;
            light->isGlobal = true;
            AddLightToScene(light);
        }
    }
}

/*
=============
TraceDiffuseSample

Trace toward the sky, recursively passing through alpha-shadow surfaces.
Project the receiver footprint onto each such surface to filter its color.
=============
*/
Color3f TraceDiffuseSample(const Vector3d &origin, const Vector3d &direction,
                           int count, const Vector3d *points)
{
    Vector3d end;
    end = origin + diffuseTraceDistance * direction;
    TraceResult trace;
    TraceSegment(origin, end, &trace,
                 POLYFLAG_NOSHADOW | POLYFLAG_VISUAL_ONLY | POLYFLAG_MODEL | POLYFLAG_PORTAL);
    if (!trace.hit)
        return options.sunDiffuseColor;

    int flags = trace.hit->info->flags;
    if (flags & POLYFLAG_SKY)
        return options.sunDiffuseColor;
    if (!(flags & POLYFLAG_ALPHASHADOW))
        return Color3f(0, 0, 0);

    Color3f diffuseColor = TraceDiffuseSample(trace.hitPoint + diffuseRayOffset * direction,
                                             direction, count, points);
    if (diffuseColor.r == 0 && diffuseColor.g == 0 && diffuseColor.b == 0)
        return diffuseColor;

    double d = trace.hit->GetInfo()->plane.normal * firstSunLight->GetDirection();
    if (d < MIN_DIFFUSE_COSINE)
        return diffuseColor;

    ImageRecord *hitImage = &shaders[trace.hit->info->shaderIndex].image;
    vec2_t texPoints[MAX_SUBDIVIDE_POINTS];
    firstSunLight->ProjectPoints(trace.hit, points, count, texPoints);
    return FilterTexture(hitImage, diffuseColor, texPoints, count);
}

/*
=============
DiffuseSampleCallback

Rotate the hemicube samples into the receiver normal's basis and average
the visible sky color. The fast path uses a vertical ray and a normal-based
weight. Scale the result by this fragment's share of the texel area.
=============
*/
void DiffuseSampleCallback(float area, const float *st, const vec2_t *points, int count)
{
    Vector3d worldPoints[MAX_SUBDIVIDE_POINTS];
    Lightmap *lightmap = &lightmapSamples[diffuseSourcePolygon->info->lightmapIndex];
    int sPosition = (int)st[0];
    int t = (int)st[1];
    Vector3d sampleOrigin;
    Vector3d n;
    diffuseSourcePolygon->LightmapToWorld(st[0], st[1], &sampleOrigin);
    diffuseSourcePolygon->InterpolateNormal(&sampleOrigin, &n);
    sampleOrigin += n * diffuseRayOffset;
    int k;
    for (k = 0; k < count; k++)
        diffuseSourcePolygon->LightmapToWorld(points[k][0], points[k][1], &worldPoints[k]);

    float coverageFraction = area / lightmap->samples[t][sPosition].area;
    Color3f accumColor;
    if (numDiffuseSamples)
    {
        Vector3d right = PerpendicularVector3d(n);
        Vector3d up = n.Cross(right);
        accumColor = Color3f(0, 0, 0);
        for (k = 0; k < numDiffuseSamples; k++)
        {
            Vector3d direction;
            direction = diffuseSamples[k].direction.x * right;
            direction += diffuseSamples[k].direction.y * up;
            direction += diffuseSamples[k].direction.z * n;
            if (direction.z >= 0)
                accumColor += TraceDiffuseSample(sampleOrigin, direction, count, worldPoints) *
                              (float)diffuseSamples[k].weight;
        }
        lightmap->samples[t][sPosition].color += accumColor * coverageFraction;
    }
    else
    {
        accumColor = TraceDiffuseSample(sampleOrigin, Vector3d(0, 0, 1), count, worldPoints);
        if (accumColor.Luminance() == 0)
        {
            Vector3d end = sampleOrigin + n * DIFFUSE_SAMPLE_ESCAPE_DISTANCE;
            TraceResult trace;
            TraceSegment(sampleOrigin, end, &trace,
                         POLYFLAG_NOSHADOW | POLYFLAG_VISUAL_ONLY | POLYFLAG_MODEL | POLYFLAG_PORTAL);
            if (!trace.hit)
                accumColor = TraceDiffuseSample(end, Vector3d(0, 0, 1), count, worldPoints);
        }
        lightmap->samples[t][sPosition].color += accumColor * (float)(coverageFraction * (1.0 + n.z) * 0.5);
    }
}

/*
=============
CellDiffuseSampleCallback

Test diffuse sunlight visibility without accumulating a lightmap color.
Hemicube sampling stops at the first nonzero contribution. The fast vertical
path sets the flag only after a successful escape retry.
=============
*/
void CellDiffuseSampleCallback(float area, const float *st, const vec2_t *points, int count)
{
    Vector3d worldPoints[MAX_SUBDIVIDE_POINTS];
    if (cellHasDiffuseSunlight)
        return;
    Vector3d sampleOrigin;
    Vector3d n;
    diffuseSourcePolygon->LightmapToWorld(st[0], st[1], &sampleOrigin);
    diffuseSourcePolygon->InterpolateNormal(&sampleOrigin, &n);
    sampleOrigin += n * diffuseRayOffset;
    int k;
    for (k = 0; k < count; k++)
        diffuseSourcePolygon->LightmapToWorld(points[k][0], points[k][1], &worldPoints[k]);

    Color3f sunColor;
    if (numDiffuseSamples)
    {
        Vector3d right = PerpendicularVector3d(n);
        Vector3d up = n.Cross(right);
        sunColor = Color3f(0, 0, 0);
        for (k = 0; k < numDiffuseSamples; k++)
        {
            Vector3d direction;
            direction = diffuseSamples[k].direction.x * right;
            direction += diffuseSamples[k].direction.y * up;
            direction += diffuseSamples[k].direction.z * n;
            if (direction.z >= 0)
            {
                sunColor += TraceDiffuseSample(sampleOrigin, direction, count, worldPoints) *
                            (float)diffuseSamples[k].weight;
                if (sunColor.Luminance() != 0)
                {
                    cellHasDiffuseSunlight = true;
                    return;
                }
            }
        }
    }
    else
    {
        sunColor = TraceDiffuseSample(sampleOrigin, Vector3d(0, 0, 1), count, worldPoints);
        if (sunColor.Luminance() == 0)
        {
            Vector3d end = sampleOrigin + n * DIFFUSE_SAMPLE_ESCAPE_DISTANCE;
            TraceResult trace;
            TraceSegment(sampleOrigin, end, &trace,
                         POLYFLAG_NOSHADOW | POLYFLAG_VISUAL_ONLY | POLYFLAG_MODEL | POLYFLAG_PORTAL);
            if (!trace.hit)
            {
                sunColor = TraceDiffuseSample(end, Vector3d(0, 0, 1), count, worldPoints);
                if (sunColor.Luminance() != 0)
                    cellHasDiffuseSunlight = true;
            }
        }
    }
}

/*
=============
GenerateDiffuseSamples

Build unit directions and form-factor weights for a hemicube above a
horizontal receiver. Opposite side faces share the same weights.
=============
*/
void GenerateDiffuseSamples(void)
{
    int size = options.sunDiffuseSamples;
    int numSideSamples = size;
    int sideHeight = numSideSamples / 2 < 1 ? 1 : numSideSamples / 2;
    double topStep = 2.0f / size;
    double sScale = 2.0f / numSideSamples;
    double heightIncrement = 1.0f / sideHeight;
    Vector3d corners[4];
    Vector3d sampleOrigin(0, 0, 0);
    Vector3d n(0, 0, 1);

    /* A hemicube contributes its top face and the upper half of four sides. */
    numDiffuseSamples = size * size + 4 * (numSideSamples * sideHeight);
    diffuseSamples = new DiffuseSample[numDiffuseSamples];
    int numDiffuse = 0;
    int s, t;
    for (s = 0; s < size; s++)
        for (t = 0; t < size; t++)
        {
            double x = s * topStep - 1.0;
            double y = t * topStep - 1.0;
            corners[0] = Vector3d(x, y, 1);
            corners[1] = Vector3d(x + topStep, y, 1);
            corners[2] = Vector3d(x + topStep, y + topStep, 1);
            corners[3] = Vector3d(x, y + topStep, 1);
            diffuseSamples[numDiffuse].weight = PointToPolygonFormFactor(sampleOrigin, n, corners, 4);
            diffuseSamples[numDiffuse].direction = Vector3d(x + topStep * 0.5, y + topStep * 0.5, 1);
            diffuseSamples[numDiffuse].direction.NormalizeLength();
            numDiffuse++;
        }
    for (s = 0; s < numSideSamples; s++)
        for (t = 0; t < sideHeight; t++)
        {
            double x = s * sScale - 1.0;
            double z = t * heightIncrement;
            corners[0] = Vector3d(x, 1, z);
            corners[1] = Vector3d(x, 1, z + heightIncrement);
            corners[2] = Vector3d(x + sScale, 1, z + heightIncrement);
            corners[3] = Vector3d(x + sScale, 1, z);
            diffuseSamples[numDiffuse].weight = PointToPolygonFormFactor(sampleOrigin, n, corners, 4);
            diffuseSamples[numDiffuse].direction = Vector3d(x + sScale * 0.5, 1, z + heightIncrement * 0.5);
            diffuseSamples[numDiffuse].direction.NormalizeLength();
            numDiffuse++;
            diffuseSamples[numDiffuse].weight = diffuseSamples[numDiffuse - 1].weight;
            diffuseSamples[numDiffuse].direction = Vector3d(x + sScale * 0.5, -1, z + heightIncrement * 0.5);
            diffuseSamples[numDiffuse].direction.NormalizeLength();
            numDiffuse++;
            diffuseSamples[numDiffuse].weight = diffuseSamples[numDiffuse - 1].weight;
            diffuseSamples[numDiffuse].direction = Vector3d(1, x + sScale * 0.5, z + heightIncrement * 0.5);
            diffuseSamples[numDiffuse].direction.NormalizeLength();
            numDiffuse++;
            diffuseSamples[numDiffuse].weight = diffuseSamples[numDiffuse - 1].weight;
            diffuseSamples[numDiffuse].direction = Vector3d(-1, x + sScale * 0.5, z + heightIncrement * 0.5);
            diffuseSamples[numDiffuse].direction.NormalizeLength();
            numDiffuse++;
        }
}

/*
=============
Vector3d::NormalizeLength
=============
*/
inline double Vector3d::NormalizeLength(void)
{
    double length = sqrt(*this * *this);
    if (length != 0)
    {
        double inverse = 1.0 / length;
        x *= inverse;
        y *= inverse;
        z *= inverse;
    }
    return length;
}

/*
=============
SampleDiffuseLighting

Offset receiver polygons away from their surfaces, split them through the
BSP, and sample the fragments in flooded space.
=============
*/
void SampleDiffuseLighting(void)
{
    if (!firstSunLight)
        return;
    bool oldCentroidTextureFilter = options.fastAlphaCheck;
    if (!options.fastDiffuseSun)
        options.fastAlphaCheck = true;
    if (!options.fastDiffuseSun)
        GenerateDiffuseSamples();
    int numPolys = 0;
    int i;
    for (i = 0; i < numPermanentPolygons; i++)
        if (polygons[i].info->lightmapIndex >= 0)
            numPolys++;
    BeginProgress("Performing sample-based diffuse lighting...");
    if (numDiffuseSamples)
        Q3Map_Printf("Using %i samples with hemicube form factor approximation\n", numDiffuseSamples);
    else
        Q3Map_Printf("Using single vertical sample and surface normal form factor approximation\n");
    SetProgress(0, numPolys);
    diffuseSunlightCallback = DiffuseLightCallback;
    for (i = 0; i < numPermanentPolygons; i++)
    {
        diffuseSourcePolygon = &polygons[i];
        if (diffuseSourcePolygon->info->lightmapIndex < 0)
            continue;
        Polygon *poly = OffsetPolygon(diffuseSourcePolygon, diffusePolygonOffset);
        FilterDiffuseSunlight_r(bspRoot, poly);
        poly->Free();
        UpdateProgress(1);
    }
    diffuseSunlightCallback = 0;
    EndProgress();
    options.fastAlphaCheck = oldCentroidTextureFilter;
}

/*
=============
DiffuseLightCallback
=============
*/
void DiffuseLightCallback(Polygon *poly)
{
    vec2_t mins, maxima;
    vec2_t buffers[NUM_SUBDIVIDE_BUFFERS][MAX_SUBDIVIDE_POINTS];
    int i;
    mins[0] = FLT_MAX;
    mins[1] = FLT_MAX;
    maxima[0] = -FLT_MAX;
    maxima[1] = -FLT_MAX;
    for (i = 0; i < poly->numPoints; i++)
    {
        poly->WorldToLightmap(&poly->points[i], buffers[0][i]);
        if (mins[0] > buffers[0][i][0])
            mins[0] = buffers[0][i][0];
        if (maxima[0] < buffers[0][i][0])
            maxima[0] = buffers[0][i][0];
        if (mins[1] > buffers[0][i][1])
            mins[1] = buffers[0][i][1];
        if (maxima[1] < buffers[0][i][1])
            maxima[1] = buffers[0][i][1];
    }
    int firstS = FloorToInt(mins[0]);
    int minT = FloorToInt(mins[1]);
    int width = FloorToInt(maxima[0]) - firstS + 1;
    int rows = FloorToInt(maxima[1]) - minT + 1;
    SubdividePolygon2D(buffers, poly->numPoints, width, rows,
                       (float)firstS, (float)minT, 1.0f, 1.0f, DiffuseSampleCallback);
}

/*
=============
FilterDiffuseSunlight_r

Split a receiver at BSP planes and pass only flooded-leaf fragments to the
active diffuse-light callback.
=============
*/
void FilterDiffuseSunlight_r(BspNode *node, Polygon *poly)
{
    Polygon *front, *back;
    int result;
    if (node->isLeaf)
    {
        if (node->flooded)
            diffuseSunlightCallback(poly);
        return;
    }
    result = SplitPolygonEpsilon(poly, node->plane, &front, &back);
    switch (result)
    {
    case SIDE_FRONT:
        FilterDiffuseSunlight_r(node->children[0], poly);
        break;
    case SIDE_BACK:
        FilterDiffuseSunlight_r(node->children[1], poly);
        break;
    case SIDE_CROSS:
        FilterDiffuseSunlight_r(node->children[0], front);
        front->Free();
        FilterDiffuseSunlight_r(node->children[1], back);
        back->Free();
        break;
    case SIDE_ON:
        if (node->plane.normal * poly->info->plane.normal > 0)
            FilterDiffuseSunlight_r(node->children[0], poly);
        else
            FilterDiffuseSunlight_r(node->children[1], poly);
        break;
    }
}

/*
=============
CalculateSceneLighting

Start with ambient and diffuse sky lighting, then visit each scene light.
Finish leaf sunlight visibility and extend colors into uncovered texels.
=============
*/
void CalculateSceneLighting(void)
{
    InitAmbientLightmaps();
    if (!options.noDiffuseSun && options.sampleDiffuseSun)
        SampleDiffuseLighting();
    AllocOccluderPools();
    BeginProgress("Performing world and model lighting...");
    SetProgress(0, numSceneLights);
    GL_Step(0);
    for (pickedSceneLight = sceneLights; pickedSceneLight; pickedSceneLight = pickedSceneLight->next)
    {
        if (pickedSceneLight->selected)
            continue;
        currentLight = pickedSceneLight->light;
        if (currentLight->HasOrigin())
            lightOrigin = currentLight->GetOrigin();
        if (currentLight->IsDirectional())
            lightDirection = currentLight->GetDirection();
        PrepareLightOccluders();
        pickedSceneLight->selected = true;
        UpdateProgress(1);
    }
    currentLight = 0;
    FreeOccluderPools();
    EndProgress();
    Q3Map_Verbose("%i of %i tested polys were occluded (%.1f%%)\n",
                  rejectedOccluders, testedOccluders,
                  rejectedOccluders * 100.0f / testedOccluders);
    if (!options.noLeafLights)
    {
        BeginProgress("More thorough check for model diffuse sunlighting areas...");
        SetProgress(0, mapInfo.numLightLeaves);
        diffuseSunlightCallback = CellDiffuseLightCallback;
        for (int i = 0; i < mapInfo.numLightLeaves; i++)
        {
            if (!mapInfo.leafGlobalLights[i])
                mapInfo.leafGlobalLights[i] = CellHasDiffuseSunlight(i);
            UpdateProgress(1);
        }
        diffuseSunlightCallback = 0;
    }
    GL_Step(0);
    BuildLightmapMask();
    GL_ClearStep();
}

/*
=============
PrepareLightOccluders

Register the source in the BSP leaves, then light each face of its volume.
Temporary clipped polygons are reclaimed after each face.
=============
*/
void PrepareLightOccluders(void)
{
    int i, numFaces;
    Light *source;
    GL_Step(1);
    buildingOccluders = true;
    numFaces = currentLight->NumFaces();
    source = currentLight->GetSource();
    if (source && !source->noLeafInsert)
    {
        if (source->HasPolygon())
            FilterLightPolygon(source, source->GetPolygon());
        else if (source->HasOrigin())
            AddLightAtPoint(source, source->GetOrigin());
    }
    for (i = 0; i < numFaces; i++)
    {
        occluderPasses++;
        GL_Step(2);
        SavePolygonPool();
        currentLight->SelectFace(i);
        ClearOccluderPools();
        CollectOccluders_r(bspRoot, true);
        RestorePolygonPool();
    }
    buildingOccluders = false;
}

bool ProcessPolygonOcclusion(Polygon *poly, Occluder *occluder, bool visible);

/*
=============
CollectOccluders_r

Walk from the light-facing child to the far child. Light the node's
polygons before adding their shadows, so they do not occlude each other.
Model surfaces contribute their occluders as a group.
=============
*/
void CollectOccluders_r(BspNode *node, bool clip)
{
    Polygon *p, *clipped;
    Occluder *firstNode, *lastOccluder, *occlusion;
    const Plane *splitPlane;
    int frontSide, i;
    MiscModel *polyModel;
    bool crosses, lightSide[2], skipNodePolygons;
    if (node->isLeaf)
        return;
    if (clip)
    {
        int boundsSide = currentLight->ClassifyBounds(node->bounds);
        if (boundsSide == 1)
            return;
        if (boundsSide == 0)
            clip = false;
    }
    firstNode = 0;
    lastOccluder = 0;
    splitPlane = &node->plane;
    currentLight->ClassifyPlane(splitPlane, &frontSide, &crosses, lightSide, &skipNodePolygons);
    if (node->children[frontSide])
        CollectOccluders_r(node->children[frontSide], clip);
    if (!skipNodePolygons)
    {
        /* i counts the node's polygons; nothing reads it any more. */
        for (i = 0, p = node->polygons; p; i++, p = p->next)
        {
            if (p->info->flags & POLYFLAG_MODEL)
            {
                polyModel = p->info->model;
                if (polyModel->ShouldIgnore(occluderPasses))
                    continue;
            }
            if (clip)
            {
                clipped = currentLight->ClipPolygon(p);
                if (!clipped)
                    continue;
            }
            else
                clipped = p;
            occlusion = AddOccluder(clipped);
            int back = splitPlane->normal * clipped->info->plane.normal < 0 ? 1 : 0;
            bool visible = ProcessPolygonOcclusion(clipped, occlusion, lightSide[back]);
            testedOccluders++;
            if (!visible)
                rejectedOccluders++;
            if (!visible || (clipped->info->flags & (POLYFLAG_NOSHADOW | POLYFLAG_PORTAL)))
            {
                FreeOccluder(occlusion);
                if (clipped != p)
                {
                    clipped->Free();
                    GL_ClearStep();
                }
            }
            else
            {
                if (clipped->info->flags & POLYFLAG_MODEL)
                {
                    FreeOccluder(occlusion);
                    if (clipped != p)
                    {
                        clipped->Free();
                        GL_ClearStep();
                    }
                    polyModel = p->info->model;
                    polyModel->CollectOccluders(occluderPasses, &firstNode, &lastOccluder);
                }
                else
                {
                    if (!lastOccluder)
                    {
                        firstNode = occlusion;
                        lastOccluder = occlusion;
                    }
                    else
                    {
                        occlusion->next = firstNode;
                        firstNode = occlusion;
                    }
                }
            }
        }
    }
    if (firstNode)
    {
        lastOccluder->next = activeOccluders;
        activeOccluders = firstNode;
    }
    if (crosses)
        CollectOccluders_r(node->children[1 - frontSide], clip);
}

/*
=============
ProcessPolygonOcclusion

Select overlapping occluders for this polygon. Lightmap receivers are
clipped and sampled; portal polygons instead record lights for a BSP leaf.
The return value tells the caller whether to keep the polygon's shadow.
=============
*/
bool ProcessPolygonOcclusion(Polygon *poly, Occluder *occluder, bool lightmapVisible)
{
    int i;
    const bool lightmap = lightmapVisible && poly->info->lightmapIndex >= 0 && !currentLight->noHitSamples;
    Plane *clipPlanes = occluder->sidePlanes;
    if (poly->selected || poly->info->owner->selected)
        GL_Step(-1);
    float intensity = 0;
    if (!lightmap && (poly->info->flags & POLYFLAG_PORTAL))
    {
        if (!currentLight->GetSource())
            return false;
        intensity = currentLight->MaxAttenuation(poly);
        if (intensity < options.minDiffuseSunIntensity)
            return false;
        if (FindLeafLight(currentLight->GetSource(), poly->GetCellIndex(), intensity))
            return false;
    }

    numPolyOccluders = 0;
    Occluder *o;
    for (o = activeOccluders; o; o = o->next)
    {
        if (occluder->mins[0] >= o->maxs[0] ||
            occluder->maxs[0] <= o->mins[0] ||
            occluder->mins[1] >= o->maxs[1] ||
            occluder->maxs[1] <= o->mins[1])
            continue;
        if (!lightmap && (poly->info->flags & POLYFLAG_ALPHASHADOW))
            continue;
        /* A separating plane skips this occluder; resume the outer list scan. */
        for (i = 0; i < poly->numPoints; i++)
            if (PolygonInFrontOfPlane(o->polygon, clipPlanes[i]))
                goto nextOccluder;
        if (numPolyOccluders == MAX_OCCLUDERS_PER_POLY)
            Q3Map_Error("MAX_OCCLUDERS_PER_POLY (%i) exceeded\n", MAX_OCCLUDERS_PER_POLY);
        polyOccluders[numPolyOccluders] = o;
        numPolyOccluders++;
nextOccluder:
        continue;
    }
    GL_StepPolygon(3, poly);
    bool visible;
    if (lightmap)
    {
        qsort(polyOccluders, numPolyOccluders, sizeof(polyOccluders[0]), CompareOccluderArea);
        visible = LightPolygonOccluders_r(poly, 0, 0);
        if (options.mergeOccluders)
            visible |= RemoveCoveredOccluders(poly, occluder, visible);
    }
    else if (poly->info->flags & POLYFLAG_NOSHADOW)
        visible = false;
    else if ((poly->info->flags & (POLYFLAG_ALPHASHADOW | POLYFLAG_MODEL_SURFACE)) == POLYFLAG_MODEL_SURFACE &&
             currentLight->IsBackFacing(poly))
        visible = false;
    else if (poly->info->flags & POLYFLAG_PORTAL)
    {
        visible = false;
        if (!currentLight->noOccluders)
        {
            qsort(polyOccluders, numPolyOccluders, sizeof(polyOccluders[0]), CompareOccluderArea);
            if (IsAreaLightVisible_r(poly, 0, 0))
            {
                AddLeafLight(currentLight->GetSource(), poly->GetCellIndex(), intensity);
            }
        }
    }
    else
    {
        qsort(polyOccluders, numPolyOccluders, sizeof(polyOccluders[0]), CompareOccluderArea);
        visible = IsAreaLightVisible_r(poly, 0, 0);
        if (options.mergeOccluders)
            visible |= RemoveCoveredOccluders(poly, occluder, visible);
    }
    return visible;
}

/*
=============
LightPolygonOccluders_r

Split the receiver against each occluder volume. Fragments outside a
volume continue to later occluders; fragments inside an alpha shadow
carry that polygon on the filter stack. Sample the surviving fragments.
=============
*/
bool LightPolygonOccluders_r(Polygon *poly, int index, int planeIndex)
{
    Polygon *front, *back;
    Occluder *occluder;
    int side;
    while (index < numPolyOccluders)
    {
        occluder = polyOccluders[index];
        while (planeIndex < occluder->polygon->numPoints)
        {
            side = SplitPolygonEpsilon(poly, occluder->sidePlanes[planeIndex], &front, &back);
            if (side == SIDE_CROSS)
            {
                bool visible = LightPolygonOccluders_r(front, index + 1, 0);
                front->Free();
                if (planeIndex + 1 < occluder->polygon->numPoints)
                    visible |= LightPolygonOccluders_r(back, index, planeIndex + 1);
                else if (occluder->polygon->info->flags & POLYFLAG_ALPHASHADOW)
                {
                    PushAlphaShadowPoly(const_cast<Polygon *>(occluder->polygon));
                    visible |= LightPolygonOccluders_r(back, index + 1, 0);
                    PopAlphaShadowPoly();
                }
                back->Free();
                return visible;
            }
            if (side == SIDE_FRONT)
                break;
            if (side == SIDE_ON)
                return false;
            if (side == SIDE_BACK && planeIndex + 1 == occluder->polygon->numPoints)
            {
                if (occluder->polygon->info->flags & POLYFLAG_ALPHASHADOW)
                {
                    PushAlphaShadowPoly(const_cast<Polygon *>(occluder->polygon));
                    bool result = LightPolygonOccluders_r(poly, index + 1, 0);
                    PopAlphaShadowPoly();
                    return result;
                }
                return false;
            }
            planeIndex++;
        }
        planeIndex = 0;
        index++;
    }
    LightPolygon(poly);
    return true;
}

/*
=============
PushAlphaShadowPoly
=============
*/
void PushAlphaShadowPoly(Polygon *poly)
{
    if (numAlphaShadowPolys >= MAX_ALPHA_SHADOW_POLYS)
        Q3Map_Error("MAX_ALPHA_SHADOW_POLYS (%i) exceeded\n", MAX_ALPHA_SHADOW_POLYS);
    alphaShadowPolys[numAlphaShadowPolys] = poly;
    numAlphaShadowPolys++;
}

/*
=============
PopAlphaShadowPoly
=============
*/
void PopAlphaShadowPoly(void)
{
    numAlphaShadowPolys--;
}

/*
=============
LightPolygon

Subdivide the visible receiver in lightmap space. Alpha-shadow fragments
use texture filtering; other fragments accumulate the light directly.
=============
*/
void LightPolygon(Polygon *poly)
{
    int i;
    vec2_t buffers[NUM_SUBDIVIDE_BUFFERS][MAX_SUBDIVIDE_POINTS];
    GL_StepPolygon(4, poly);
    if (poly->IsLightmapFlipped())
    {
        for (i = 0; i < poly->numPoints; i++)
            poly->WorldToLightmap(&poly->points[poly->numPoints - 1 - i], buffers[0][i]);
    }
    else
    {
        for (i = 0; i < poly->numPoints; i++)
            poly->WorldToLightmap(&poly->points[i], buffers[0][i]);
    }
    diffuseSourcePolygon = poly;
    PolygonCellCallback filter;
    if (HasAlphaShadowPolys())
        filter = FilteredLightmapCallback;
    else
        filter = AccumulateLightmapLighting;
    SubdividePolygon2D(buffers, poly->numPoints, poly->lightmapWidth, poly->lightmapHeight,
                       (float)poly->lightmapX, (float)poly->lightmapY, 1.0f, 1.0f, filter);
    diffuseSourcePolygon = 0;
}

/*
=============
AccumulateLightmapLighting

Weight the fragment's light by its fraction of the texel's total coverage.
Clamp that fraction to one when clipping tolerances enlarge the fragment.
=============
*/
void AccumulateLightmapLighting(float area, const float *st, const vec2_t *points, int count)
{
    LightmapSample (*samples)[LIGHTMAP_SIZE] = lightmapSamples[diffuseSourcePolygon->info->lightmapIndex].samples;
    int s = (int)st[0];
    int t = (int)st[1];
    float fraction = area / samples[t][s].area > 1.0f ? 1.0f : area / samples[t][s].area;
    Color3f color = currentLight->Illuminate(diffuseSourcePolygon, points, count, area, st) * fraction;
    samples[t][s].color += color;
}

/*
=============
FilteredLightmapCallback

Apply the alpha-shadow stack in reverse order. Project the receiver
footprint onto each shadow polygon before filtering the incoming color.
=============
*/
void FilteredLightmapCallback(float area, const float *st, const vec2_t *points, int count)
{
    LightmapSample (*samples)[LIGHTMAP_SIZE] = lightmapSamples[diffuseSourcePolygon->info->lightmapIndex].samples;
    int s = (int)st[0];
    int t = (int)st[1];
    float fraction = area / samples[t][s].area > 1.0f ? 1.0f : area / samples[t][s].area;
    Color3f color = currentLight->Illuminate(diffuseSourcePolygon, points, count, area, st) * fraction;
    {
        /* World-space points are needed only while filtering the shadow stack. */
        Vector3d worldPoints[MAX_SUBDIVIDE_POINTS];
        for (int i = 0; i < count; i++)
            diffuseSourcePolygon->LightmapToWorld(points[i][0], points[i][1], &worldPoints[i]);
        i = numAlphaShadowPolys;
        do
        {
            i--;
            vec2_t texturePoints[MAX_SUBDIVIDE_POINTS];
            ImageRecord *image = &shaders[alphaShadowPolys[i]->info->shaderIndex].image;
            currentLight->ProjectPoints(alphaShadowPolys[i], worldPoints, count, texturePoints);
            color = FilterTexture(image, color, texturePoints, count);
        } while (i != 0);
    }
    samples[t][s].color += color;
}

/*
=============
HasAlphaShadowPolys
=============
*/
bool HasAlphaShadowPolys(void)
{
    return (bool)numAlphaShadowPolys;
}

/*
=============
PolygonInFrontOfPlane
=============
*/
inline bool PolygonInFrontOfPlane(const Polygon *polygon, const Plane &plane)
{
    int i;
    for (i = 0; i < polygon->numPoints; i++)
    {
        double distance = polygon->points[i] * plane.normal - plane.dist;
        if (distance < -EQUAL_EPSILON)
            return false;
    }
    return true;
}

/*
=============
CellDiffuseLightCallback

Subdivide a cell receiver in coarse lightmap-space steps. The callback
only needs to establish whether any part of the cell receives sunlight.
=============
*/
void CellDiffuseLightCallback(Polygon *poly)
{
    float sampleSize = 64.0f;
    if (cellHasDiffuseSunlight)
        return;
    vec2_t mins, maxima;
    vec2_t buffers[NUM_SUBDIVIDE_BUFFERS][MAX_SUBDIVIDE_POINTS];
    int i;
    mins[0] = FLT_MAX;
    mins[1] = FLT_MAX;
    maxima[0] = -FLT_MAX;
    maxima[1] = -FLT_MAX;
    for (i = 0; i < poly->numPoints; i++)
    {
        poly->WorldToLightmap(&poly->points[i], buffers[0][i]);
        if (mins[0] > buffers[0][i][0])
            mins[0] = buffers[0][i][0];
        if (maxima[0] < buffers[0][i][0])
            maxima[0] = buffers[0][i][0];
        if (mins[1] > buffers[0][i][1])
            mins[1] = buffers[0][i][1];
        if (maxima[1] < buffers[0][i][1])
            maxima[1] = buffers[0][i][1];
    }
    int firstS = FloorToInt(mins[0] / sampleSize);
    int minT = FloorToInt(mins[1] / sampleSize);
    int width = FloorToInt(maxima[0] / sampleSize) - firstS + 1;
    int rows = FloorToInt(maxima[1] / sampleSize) - minT + 1;
    SubdividePolygon2D(buffers, poly->numPoints, width, rows,
                       (float)firstS * sampleSize, (float)minT * sampleSize,
                       sampleSize, sampleSize, CellDiffuseSampleCallback);
}
