/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Emissive surface records shared by area-light sampling cells.
===========================================================================
*/
#include "flare/light_local.h"

/*
==================
AreaLight::NumFaces

Only the sampling cells contribute lighting passes.
==================
*/
int AreaLight::NumFaces()
{
    return 0;
}

/*
==================
AreaLight::SelectFace

No face setup is needed; the sampling cells supply the lighting passes.
==================
*/
void AreaLight::SelectFace(int face)
{
}

/*
==================
AreaLight::HasOrigin

The parent record is not inserted as a positional light.
==================
*/
bool AreaLight::HasOrigin()
{
    return false;
}

/*
==================
AreaLight::IsDirectional

Area lights are not directional lights.
==================
*/
bool AreaLight::IsDirectional()
{
    return false;
}

/*
==================
AreaLight::HasPolygon

The parent record has no source polygon for visibility tests.
==================
*/
bool AreaLight::HasPolygon()
{
    return false;
}

/*
==================
AreaLight::GetPolygon

The sampling cells hold the emitting geometry.
==================
*/
Polygon *AreaLight::GetPolygon()
{
    return 0;
}

/*
==================
AreaLight::Export

Fills the BSP light record for this light.
==================
*/
void AreaLight::Export(bspLight_t *out)
{
    memset(out, 0, sizeof(*out));
    out->type = BSP_LIGHT_AREA;
    out->color[0] = color.r;
    out->color[1] = color.g;
    out->color[2] = color.b;
    out->origin[0] = origin.x;
    out->origin[1] = origin.y;
    out->origin[2] = origin.z;
    out->direction[0] = normal.x;
    out->direction[1] = normal.y;
    out->direction[2] = normal.z;
    out->data.area.axis[1][0] = axisT.x;
    out->data.area.axis[1][1] = axisT.y;
    out->data.area.axis[1][2] = axisT.z;
    out->data.area.axis[0][0] = axisS.x;
    out->data.area.axis[0][1] = axisS.y;
    out->data.area.axis[0][2] = axisS.z;
    out->data.area.height = height;
    out->data.area.width = width;
}

/*
==================
AreaLight::IsOutside

Volume tests are performed on the sampling cells.
==================
*/
bool AreaLight::IsOutside(const Vector3d &point)
{
    return false;
}

/*
==================
AreaLight::ClassifyBounds

The parent record is excluded from BSP light traversal.
==================
*/
int AreaLight::ClassifyBounds(const Bounds &bounds)
{
    return 1;
}

/*
==================
AreaLight::IsBackFacing

Facing tests are performed on the sampling cells.
==================
*/
bool AreaLight::IsBackFacing(const Polygon *poly)
{
    return false;
}

/*
==================
AreaLight::ClassifyPlane

The parent does not traverse BSP splits; the sampling cells do.
==================
*/
void AreaLight::ClassifyPlane(const Plane *plane, int *nearSide, bool *reachesFar, bool visit[2], bool *onPlane)
{
}

/*
==================
AreaLight::ClipPolygon

The parent supplies no clipped polygon.
==================
*/
Polygon *AreaLight::ClipPolygon(Polygon *poly)
{
    return 0;
}

/*
==================
AreaLight::BuildOccluder

The sampling cells build their own occluders.
==================
*/
void AreaLight::BuildOccluder(const Polygon *poly, Occluder *occluder)
{
}

/*
==================
AreaLight::ProjectPoints

The parent does not project samples.
==================
*/
void AreaLight::ProjectPoints(const Polygon *poly, const Vector3d *points, int numPoints, float (*st)[2])
{
}

/*
==================
AreaLight::IsUnoccluded

The parent contributes no visibility samples.
==================
*/
bool AreaLight::IsUnoccluded(const Vector3d &point)
{
    return false;
}

/*
==================
AreaLight::DistanceAttenuation

The parent contributes no direct illumination.
==================
*/
float AreaLight::DistanceAttenuation(const Vector3d &point)
{
    return 0.0f;
}

/*
==================
AreaLight::AngularAttenuation

The parent contributes no surface illumination.
==================
*/
float AreaLight::AngularAttenuation(const Vector3d &point, const Vector3d &normal)
{
    return 0.0f;
}

/*
==================
AreaLight::Draw

The sampling cells supply the preview markers.
==================
*/
void AreaLight::Draw(bool selected)
{
}

/*
==================
AreaLight::GetOrigin

Position of the light.
==================
*/
Vector3d AreaLight::GetOrigin()
{
    return origin;
}

/*
==================
AreaLight::GetDirection

The parent supplies no ray direction.
==================
*/
Vector3d AreaLight::GetDirection()
{
    return Vector3d(0.0, 0.0, 0.0);
}

/*
==================
AreaLight::DirectionTo

The parent supplies no sample direction.
==================
*/
Vector3d AreaLight::DirectionTo(const Vector3d &point)
{
    return Vector3d(0.0, 0.0, 0.0);
}
