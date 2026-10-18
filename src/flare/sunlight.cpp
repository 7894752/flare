/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Directional sunlight volumes bounded by sky polygons.
===========================================================================
*/
#include "flare/math_types.h"
#include "flare/light_local.h"
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

#include "polygon.h"
#include "flare/traceresult.h"

#define SUN_ORIGIN_OFFSET 0.125
#define LIGHT_TRACE_IGNORE_FLAGS 0x4B

int numSunPlanes = 0;
Light *sunSource = 0;
Plane sunPlanes[MAX_SKY_POLY_SIDES];

/*
==================
SunLight::Export

Fills the BSP light record for this light.
==================
*/
void SunLight::Export(bspLight_t *out)
{
    memset(out, 0, sizeof(*out));
    out->type = BSP_LIGHT_SUN;
    out->direction[0] = direction.x;
    out->direction[1] = direction.y;
    out->direction[2] = direction.z;
    out->color[0] = options.sunColor.r + options.sunDiffuseColor.r;
    out->color[1] = options.sunColor.g + options.sunDiffuseColor.g;
    out->color[2] = options.sunColor.b + options.sunDiffuseColor.b;
}

/*
==================
SunLight::GetSource

The light that owns the BSP record.
==================
*/
Light *SunLight::GetSource()
{
    if (isGlobal)
        return this;
    if (!sunSource)
        sunSource = this;
    return sunSource;
}

/*
==================
SunLight::HasOrigin

True when the light has a position.
==================
*/
bool SunLight::HasOrigin()
{
    return (bool)skyPoly;
}

/*
==================
SunLight::HasPolygon

True when the light is bound to a source polygon.
==================
*/
bool SunLight::HasPolygon()
{
    return (bool)skyPoly;
}

/*
==================
SunLight::GetPolygon

The source polygon of the light.
==================
*/
Polygon *SunLight::GetPolygon()
{
    return skyPoly;
}

/*
==================
SunLight::ClipPolygon

Clips the polygon to the light volume.
==================
*/
Polygon *SunLight::ClipPolygon(Polygon *poly)
{
    int i;

    SaveTemporaryPolygons((int)poly);
    for (i = 0; i < numSunPlanes && poly; i++)
        poly = ClipPolygonBack(poly, sunPlanes[i]);
    return RestoreTemporaryPolygons(poly);
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
Vector3d::operator/=

Divide each component of this vector by scale. The divisor must be nonzero.
=============
*/
inline void Vector3d::operator/=(double scale)
{
    *this *= 1.0 / scale;
}

/*
=============
Polygon::GetBackPlane

Return the reversed plane stored in the polygon's shared surface information.
=============
*/
inline Plane *Polygon::GetBackPlane() const
{
    return &info->backPlane;
}

/*
==================
SunLight::SelectFace

Prepares the frame and the clip planes for one pass of the light.
==================
*/
void SunLight::SelectFace(int face)
{
    if (skyPoly->numPoints > MAX_SKY_POLY_SIDES)
        Q3Map_Error("MAX_SKY_POLY_SIDES (%i) exceeded\n", skyPoly->numPoints);
    if (skyPoly->numPoints < 3)
        return;
    sunPlanes[0] = *skyPoly->GetBackPlane();
    BuildSidePlanes(skyPoly, &sunPlanes[1]);
    numSunPlanes = skyPoly->numPoints + 1;
    axisU = PerpendicularVector3d(direction);
    axisV = axisU.Cross(direction);
}

/*
==================
SunLight::GetOrigin

Position of the light.
==================
*/
Vector3d SunLight::GetOrigin()
{
    if (!skyPoly)
        return Vector3d(0.0, 0.0, 0.0);
    Vector3d center(0.0, 0.0, 0.0);
    for (int i = 0; i < skyPoly->numPoints; i++)
        center += skyPoly->points[i];
    center /= skyPoly->numPoints;
    return center - direction * SUN_ORIGIN_OFFSET;
}

/*
==================
SunLight::IsOutside

True when the point lies outside the light volume.
==================
*/
bool SunLight::IsOutside(const Vector3d &point)
{
    for (int i = 0; i < numSunPlanes; i++)
        if (sunPlanes[i].normal * point > sunPlanes[i].dist)
            return true;
    return false;
}

/*
==================
SunLight::ClassifyBounds

Classifies a node's bounds against the light volume:
0 inside, 1 outside, 2 straddling.
==================
*/
int SunLight::ClassifyBounds(const Bounds &bounds)
{
    bool straddles = false;
    for (int i = 0; i < numSunPlanes; i++)
    {
        const Plane &plane = sunPlanes[i];
        if (bounds.points[plane.sign[0]][0] * plane.normal.x +
            bounds.points[plane.sign[1]][1] * plane.normal.y +
            bounds.points[plane.sign[2]][2] * plane.normal.z > plane.dist)
            return 1;
        if (bounds.points[1 - plane.sign[0]][0] * plane.normal.x +
            bounds.points[1 - plane.sign[1]][1] * plane.normal.y +
            bounds.points[1 - plane.sign[2]][2] * plane.normal.z < plane.dist)
            continue;
        straddles = true;
    }
    if (straddles)
        return 2;
    return 0;
}

/*
==================
SunLight::Draw

Draws the light in the preview.
==================
*/
void SunLight::Draw(bool selected)
{
    if (selected)
    {
        GL_Color4f(color.r, color.g, color.b, 0.25f);
        GL_DrawWinding(skyPoly);
        GL_Color3fv(&color.r);
        GL_DrawWindingOutline(skyPoly);
    }
    Vector3d start;
    start = GetOrigin();
    Vector3d end = start - direction * 8.0;
    GL_DrawLine(&start, &end);
}

/*
==================
SunLight::IsUnoccluded

True when nothing blocks the way between the point and the light.
==================
*/
bool SunLight::IsUnoccluded(const Vector3d &point)
{
    const Plane &plane = skyPoly->GetInfo()->plane;
    TraceResult trace;
    double scale = 1.0 / (direction * plane.normal);
    double distance = (point * plane.normal - plane.dist) * scale;

    TraceSegment(point, point - direction * distance, &trace, LIGHT_TRACE_IGNORE_FLAGS);
    /* The source sky polygon is an allowed endpoint of the trace. */
    return trace.hit == 0 || trace.hit == skyPoly ? true : false;
}
