/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Light interfaces, source geometry and attenuation models.
===========================================================================
*/
#ifndef FLARE_LIGHT_H
#define FLARE_LIGHT_H

#include "flare/mathlib.h"
#include "flare/geometry.h"
#include "common/bsp_light_disk.h"

/* Base light interface and common color and sampling flags (light.cpp). */
class Light
{
public:
    Light() {}

    /* number of passes (cube faces) the light needs: area 0, sun/spot 1, cell 5, point 6 */
    virtual int NumFaces() = 0;
    /* prepare the selected face (frustum planes, radius, basis); face 0 also derives the light's constants */
    virtual void SelectFace(int face) = 0;
    /* a position is available (point-like lights; the sun derives one from its sky polygon) */
    virtual bool HasOrigin() = 0;
    virtual Vector3d GetOrigin() = 0;
    /* directional light; direction of travel */
    virtual bool IsDirectional() = 0;
    virtual Vector3d GetDirection() = 0;
    /* the light is bound to a source polygon (sky polygon of a sun) */
    virtual bool HasPolygon() = 0;
    virtual Polygon *GetPolygon() = 0;
    /* fill the 72-byte BSP light record (bspLight_t) */
    virtual void Export(bspLight_t *out) = 0;
    /* the light that owns the BSP record (itself by default; a cell light returns its area light) */
    virtual Light *GetSource();
    /* bounds of a BSP node vs the light volume: 0 = fully inside, 1 = outside, 2 = straddles */
    virtual int ClassifyBounds(const Bounds &bounds) = 0;
    /* the point lies outside the light volume (radius or a side plane) */
    virtual bool IsOutside(const Vector3d &point) = 0;
    /* the light lies behind the polygon's plane (the polygon cannot receive it) */
    virtual bool IsBackFacing(const Polygon *poly) = 0;
    /* BSP node split plane vs the light: which child holds the light (nearSide), whether the other
     * child is also reached (reachesFar), per-child visit flags and whether the light lies on the plane */
    virtual void ClassifyPlane(const Plane *plane, int *nearSide, bool *reachesFar, bool visit[2], bool *onPlane) = 0;
    /* clip the polygon to the light volume; returns the clipped polygon, the same one, or NULL */
    virtual Polygon *ClipPolygon(Polygon *poly) = 0;
    /* build the occluder (side planes + 2D extent) cast by the polygon for this light */
    virtual void BuildOccluder(const Polygon *poly, Occluder *occluder) = 0;
    /* project world points along the light onto the polygon's lightmap space (s,t pairs) */
    virtual void ProjectPoints(const Polygon *poly, const Vector3d *points, int numPoints, float (*st)[2]) = 0;
    /* nothing blocks the segment between the point and the light (traces with contents mask 0x4B) */
    virtual bool IsUnoccluded(const Vector3d &point) = 0;
    /* unit direction from the light to the point */
    virtual Vector3d DirectionTo(const Vector3d &point) = 0;
    /* color for a lightmap footprint; the base evaluates the sample
     * position/normal and multiplies color by AngularAttenuation */
    virtual Color3f Illuminate(const Polygon *poly, const float (*points)[2], int numPoints, float weight,
                               const float *st);
    /* surface-facing attenuation, including the distance factor */
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal) = 0;
    /* distance attenuation */
    virtual float DistanceAttenuation(const Vector3d &point) = 0;
    /* estimated maximum attenuation over the polygon, scaled by the light's luminance */
    virtual float MaxAttenuation(const Polygon *poly);
    /* preview drawing (GL); selected is true for the current light */
    virtual void Draw(bool selected) = 0;

    ColorRGB color;
    bool noLeafInsert;      /* skip insertion into BSP leaf light lists */
    bool noHitSamples;      /* skip per-hit lightmap samples */
    bool noOccluders;       /* skip occluder generation */
    bool isGlobal;          /* affects every BSP leaf */
};
FLARE_LAYOUT(Light_size, sizeof(Light) == 0x14);
FLARE_LAYOUT(Light_color, FLARE_OFFSET(Light, color) == 0x04);
FLARE_LAYOUT(Light_flags, FLARE_OFFSET(Light, noLeafInsert) == 0x10 && FLARE_OFFSET(Light, isGlobal) == 0x13);

/* Emissive surface split into area-light cells (arealight.cpp). */
class AreaLight : public Light
{
public:
    AreaLight() {}

    virtual int NumFaces();
    virtual void SelectFace(int face);
    virtual bool HasOrigin();
    virtual Vector3d GetOrigin();
    virtual bool IsDirectional();
    virtual Vector3d GetDirection();
    virtual bool HasPolygon();
    virtual Polygon *GetPolygon();
    virtual void Export(bspLight_t *out);
    virtual int ClassifyBounds(const Bounds &bounds);
    virtual bool IsOutside(const Vector3d &point);
    virtual bool IsBackFacing(const Polygon *poly);
    virtual void ClassifyPlane(const Plane *plane, int *nearSide, bool *reachesFar, bool visit[2], bool *onPlane);
    virtual Polygon *ClipPolygon(Polygon *poly);
    virtual void BuildOccluder(const Polygon *poly, Occluder *occluder);
    virtual void ProjectPoints(const Polygon *poly, const Vector3d *points, int numPoints, float (*st)[2]);
    virtual bool IsUnoccluded(const Vector3d &point);
    virtual Vector3d DirectionTo(const Vector3d &point);
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal);
    virtual float DistanceAttenuation(const Vector3d &point);
    virtual void Draw(bool selected);

    Vector3d origin;        /* center of the surface */
    Vector3d normal;        /* exported as bspLight_t::direction */
    Vector3d axisS;         /* first surface axis */
    Vector3d axisT;         /* second surface axis */
    float width;
    float height;
};
FLARE_LAYOUT(AreaLight_size, sizeof(AreaLight) == 0x80);
FLARE_LAYOUT(AreaLight_axis, FLARE_OFFSET(AreaLight, origin) == 0x18 && FLARE_OFFSET(AreaLight, axisS) == 0x48 && FLARE_OFFSET(AreaLight, axisT) == 0x60
                                 && FLARE_OFFSET(AreaLight, width) == 0x78);

/* Parallel rays through a sky polygon (directionallight.cpp). */
class DirectionalLight : public Light
{
public:
    DirectionalLight() {}

    virtual int NumFaces();
    virtual void SelectFace(int face);
    virtual bool HasOrigin();
    virtual Vector3d GetOrigin();
    virtual bool IsDirectional();
    virtual Vector3d GetDirection();
    virtual bool HasPolygon();
    virtual Polygon *GetPolygon();
    virtual void Export(bspLight_t *out);
    virtual int ClassifyBounds(const Bounds &bounds);
    virtual bool IsOutside(const Vector3d &point);
    virtual bool IsBackFacing(const Polygon *poly);
    virtual void ClassifyPlane(const Plane *plane, int *nearSide, bool *reachesFar, bool visit[2], bool *onPlane);
    virtual Polygon *ClipPolygon(Polygon *poly);
    virtual void BuildOccluder(const Polygon *poly, Occluder *occluder);
    virtual void ProjectPoints(const Polygon *poly, const Vector3d *points, int numPoints, float (*st)[2]);
    virtual bool IsUnoccluded(const Vector3d &point);
    virtual Vector3d DirectionTo(const Vector3d &point);
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal);
    virtual float DistanceAttenuation(const Vector3d &point);
    virtual void Draw(bool selected);

    void BuildSidePlanes(const Polygon *poly, Plane *planes);   /* one plane per polygon edge */

    Vector3d direction;     /* unit vector toward the sun */
    Vector3d axisU;         /* basis perpendicular to direction (SelectFace) */
    Vector3d axisV;
    Polygon *skyPoly;       /* sky polygon the sun shines through (both suns of a sky polygon point at it) */
};
FLARE_LAYOUT(DirectionalLight_size, sizeof(DirectionalLight) == 0x68);
FLARE_LAYOUT(DirectionalLight_sky, FLARE_OFFSET(DirectionalLight, axisU) == 0x30
                                       && FLARE_OFFSET(DirectionalLight, skyPoly) == 0x60);

/* Sky-polygon visibility and global sunlight (sunlight.cpp). */
class SunLight : public DirectionalLight
{
public:
    SunLight() {}

    virtual void SelectFace(int face);
    virtual bool HasOrigin();
    virtual Vector3d GetOrigin();
    virtual bool HasPolygon();
    virtual Polygon *GetPolygon();
    virtual void Export(bspLight_t *out);
    virtual Light *GetSource();
    virtual int ClassifyBounds(const Bounds &bounds);
    virtual bool IsOutside(const Vector3d &point);
    virtual Polygon *ClipPolygon(Polygon *poly);
    virtual bool IsUnoccluded(const Vector3d &point);
    virtual void Draw(bool selected);
};
FLARE_LAYOUT(SunLight_size, sizeof(SunLight) == 0x68);

/* Point light processed as six cube faces (pointlight.cpp). */
class PointLight : public Light
{
public:
    PointLight() {}

    virtual int NumFaces();
    virtual void SelectFace(int face);
    virtual bool HasOrigin();
    virtual Vector3d GetOrigin();
    virtual bool IsDirectional();
    virtual Vector3d GetDirection();
    virtual bool HasPolygon();
    virtual Polygon *GetPolygon();
    virtual void Export(bspLight_t *out);
    virtual int ClassifyBounds(const Bounds &bounds);
    virtual bool IsOutside(const Vector3d &point);
    virtual bool IsBackFacing(const Polygon *poly);
    virtual void ClassifyPlane(const Plane *plane, int *nearSide, bool *reachesFar, bool visit[2], bool *onPlane);
    virtual Polygon *ClipPolygon(Polygon *poly);
    virtual void BuildOccluder(const Polygon *poly, Occluder *occluder);
    virtual void ProjectPoints(const Polygon *poly, const Vector3d *points, int numPoints, float (*st)[2]);
    virtual bool IsUnoccluded(const Vector3d &point);
    virtual Vector3d DirectionTo(const Vector3d &point);
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal);
    virtual float DistanceAttenuation(const Vector3d &point);
    virtual void Draw(bool selected);

    void BuildSidePlanes(const Polygon *poly, Plane *planes);

    Vector3d origin;
    Plane clip[4];          /* side planes of the selected face */
    float radius;
    float radiusSq;
    Vector3d faceRight;     /* right vector of the selected cube face */
    Vector3d faceUp;        /* up vector of the selected cube face */
    Vector3d faceForward;   /* forward vector of the selected cube face */
};
FLARE_LAYOUT(PointLight_size, sizeof(PointLight) == 0x120);
FLARE_LAYOUT(PointLight_members, FLARE_OFFSET(PointLight, origin) == 0x18 && FLARE_OFFSET(PointLight, clip) == 0x30
                                     && FLARE_OFFSET(PointLight, radius) == 0xD0
                                     && FLARE_OFFSET(PointLight, faceForward) == 0x108);

/* Linear distance falloff (linearlight.cpp). */
class LinearLight : public PointLight
{
public:
    LinearLight() {}

    virtual void SelectFace(int face);
    virtual void Export(bspLight_t *out);
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal);
    virtual float DistanceAttenuation(const Vector3d &point);
    virtual void Draw(bool selected);

    bool angleScaled;       /* apply angular attenuation (spawnflag 2) */
    float range;            /* inverse range exported as bspLinearLight_t::inverseRange */
};
FLARE_LAYOUT(LinearLight_size, sizeof(LinearLight) == 0x128);

/* Quadratic and constant distance falloff (adjustedpointlight.cpp). */
class AdjustedPointLight : public PointLight
{
public:
    AdjustedPointLight() {}

    virtual void Export(bspLight_t *out);
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal);
    virtual float DistanceAttenuation(const Vector3d &point);

    float quadratic;
    float constant;
};
FLARE_LAYOUT(AdjustedPointLight_size, sizeof(AdjustedPointLight) == 0x128);

/* Cone-limited point light (spotlight.cpp). */
class SpotLight : public PointLight
{
public:
    SpotLight() {}

    virtual int NumFaces();
    virtual void SelectFace(int face);
    virtual void Export(bspLight_t *out);
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal);
    virtual float DistanceAttenuation(const Vector3d &point);
    virtual void Draw(bool selected);

    Vector3d direction;     /* axis of the cone (target - origin) */
    double coneCosine;
    double coneTangent;     /* filled by SelectFace(0) */
    int exponent;
};
FLARE_LAYOUT(SpotLight_size, sizeof(SpotLight) == 0x150);
FLARE_LAYOUT(SpotLight_members, FLARE_OFFSET(SpotLight, direction) == 0x120 && FLARE_OFFSET(SpotLight, coneCosine) == 0x138
                                    && FLARE_OFFSET(SpotLight, exponent) == 0x148);

/* Cone light with adjusted falloff (adjustedspotlight.cpp). */
class AdjustedSpotLight : public SpotLight
{
public:
    AdjustedSpotLight() {}

    virtual void Export(bspLight_t *out);
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal);
    virtual float DistanceAttenuation(const Vector3d &point);

    float quadratic;
    float constant;
};
FLARE_LAYOUT(AdjustedSpotLight_size, sizeof(AdjustedSpotLight) == 0x158);

/* One sampling cell of an emissive surface (areacelllight.cpp). */
class AreaCellLight : public PointLight
{
public:
    AreaCellLight() {}

    virtual int NumFaces();
    virtual void SelectFace(int face);
    virtual void Export(bspLight_t *out);
    virtual Light *GetSource();
    virtual Color3f Illuminate(const Polygon *poly, const float (*points)[2], int numPoints, float weight,
                               const float *st);
    virtual float AngularAttenuation(const Vector3d &point, const Vector3d &normal);
    virtual float DistanceAttenuation(const Vector3d &point);
    virtual void Draw(bool selected);

    void SetPlane(const Plane &cellPlane);      /* sampling-cell plane */
    void SetRadius(float cellRadius);           /* radius only; SelectFace(0) derives radiusSq */

    Vector3d tangent;
    Vector3d bitangent;
    Plane plane;
    Winding cell;           /* sampling-cell outline */
    Light *source;          /* the AreaLight (or this light when isGlobal) */
};
FLARE_LAYOUT(AreaCellLight_size, sizeof(AreaCellLight) == 0x188);
FLARE_LAYOUT(AreaCellLight_members, FLARE_OFFSET(AreaCellLight, tangent) == 0x120 && FLARE_OFFSET(AreaCellLight, plane) == 0x150
                                        && FLARE_OFFSET(AreaCellLight, cell) == 0x178
                                        && FLARE_OFFSET(AreaCellLight, source) == 0x180);

struct MapInfo;
struct MapEntity;
extern bool cellHasDiffuseSunlight;
extern Polygon *diffuseSourcePolygon;
bool EntityAffectsLighting(const MapEntity *entity);
void BuildEntity(MapInfo *map, MapEntity *entity);
void Q3Map_BuildLight(MapInfo *map, MapEntity *entity);
void Q3Map_BuildMiscModel(MapInfo *map, MapEntity *entity);
void AddLightToScene(Light *light);
void LoadXModelSurfaces(MapInfo *, MapEntity *, const char *, unsigned short, const Vector3d &, const Vector3d *);
extern int numSceneLights;
void AddLightAtPoint(Light *light, const Vector3d &point);
void FilterLightPolygon(Light *light, Polygon *poly);
void DiffuseLightCallback(Polygon *poly);
bool FindLeafLight(Light *light, int leaf, float intensity);
void AddLeafLight(Light *light, int leaf, float intensity);
void LightPolygon(Polygon *poly);
void FilteredLightmapCallback(float area, const float *st, const vec2_t *points, int count);
void CellDiffuseLightCallback(Polygon *poly);
extern Light *currentLight;
void DrawSceneLights();
void PickSceneLight( const Vector3d &origin, const Vector3d &direction );

#endif
