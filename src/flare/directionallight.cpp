/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Directional lights, clipping bases and cosine-weighted sampling.
===========================================================================
*/
#include "flare/light_local.h"
#include "flare/polygon.h"

/* Tolerance for a ray direction parallel to a BSP split plane. */
const double directionalPlaneEpsilon = 0.001745328366;

/*
==================
DirectionalLight::Export

Fills the BSP light record for this light.
==================
*/
void DirectionalLight::Export(bspLight_t *out)
{
}

/*
==================
DirectionalLight::NumFaces

Number of passes (faces) the light is evaluated in.
==================
*/
int DirectionalLight::NumFaces()
{
    return 1;
}

/*
==================
DirectionalLight::HasOrigin

True when the light has a position.
==================
*/
bool DirectionalLight::HasOrigin()
{
    return false;
}

/*
==================
DirectionalLight::IsDirectional

True when the light is a directional (sun) light.
==================
*/
bool DirectionalLight::IsDirectional()
{
    return true;
}

/*
==================
DirectionalLight::HasPolygon

True when the light is bound to a source polygon.
==================
*/
bool DirectionalLight::HasPolygon()
{
    return false;
}

/*
==================
DirectionalLight::GetPolygon

The source polygon of the light.
==================
*/
Polygon *DirectionalLight::GetPolygon()
{
    return 0;
}

/*
==================
DirectionalLight::IsOutside

True when the point lies outside the light volume.
==================
*/
bool DirectionalLight::IsOutside(const Vector3d &point)
{
    return false;
}

/*
==================
DirectionalLight::ClassifyBounds

Classifies a node's bounds against the light volume:
0 inside, 1 outside, 2 straddling.
==================
*/
int DirectionalLight::ClassifyBounds(const Bounds &bounds)
{
    return 0;
}

/*
==================
DirectionalLight::ClipPolygon

Clips the polygon to the light volume.
==================
*/
Polygon *DirectionalLight::ClipPolygon(Polygon *poly)
{
    return poly;
}

/*
==================
DirectionalLight::IsUnoccluded

True when nothing blocks the way between the point and the light.
==================
*/
bool DirectionalLight::IsUnoccluded(const Vector3d &point)
{
    return true;
}

/*
==================
DirectionalLight::DistanceAttenuation

Parallel rays have no distance falloff.
==================
*/
float DirectionalLight::DistanceAttenuation(const Vector3d &point)
{
    return 1.0f;
}

/*
==================
DirectionalLight::Draw

Draws the light in the preview.
==================
*/
void DirectionalLight::Draw(bool selected)
{
}

/*
==================
DirectionalLight::SelectFace

Prepares the frame and the clip planes for one pass of the light.
==================
*/
void DirectionalLight::SelectFace(int face)
{
    axisU = PerpendicularVector3d(direction);
    axisV = axisU.Cross(direction);
}

/*
==================
DirectionalLight::GetOrigin

Position of the light.
==================
*/
Vector3d DirectionalLight::GetOrigin()
{
    return Vector3d(0.0, 0.0, 0.0);
}

/*
==================
DirectionalLight::GetDirection

Direction the light shines in.
==================
*/
Vector3d DirectionalLight::GetDirection()
{
    return -direction;
}

/*
==================
DirectionalLight::IsBackFacing

True when the light lies behind the plane of the polygon.
==================
*/
bool DirectionalLight::IsBackFacing(const Polygon *poly)
{
    const Plane &plane = poly->info->plane;
    return direction * plane.normal < 0.0 ? true : false;
}

/*
==================
DirectionalLight::ClassifyPlane

Classifies a split plane against the light: which side it is on,
whether the far side is reached too, and the per-side visit flags.
==================
*/
void DirectionalLight::ClassifyPlane(const Plane *plane, int *nearSide, bool *reachesFar, bool visit[2], bool *onPlane)
{
    *reachesFar = true;
    double cosine = direction * plane->normal;
    *onPlane = cosine > -directionalPlaneEpsilon && cosine < directionalPlaneEpsilon;
    if (cosine > 0.0)
    {
        *nearSide = 0;
        visit[0] = true;
        visit[1] = false;
    }
    else
    {
        *nearSide = 1;
        visit[0] = false;
        visit[1] = true;
    }
}

/*
==================
DirectionalLight::BuildSidePlanes

Builds one side plane per edge of the polygon.
==================
*/
void DirectionalLight::BuildSidePlanes(const Polygon *poly, Plane *planes)
{
    int i, previous;
    if (direction * poly->GetInfo()->plane.normal < 0.0)
    {
        for (i = 0, previous = poly->numPoints - 1; i < poly->numPoints; i++)
        {
            Vector3d edge = poly->points[i] - poly->points[previous];
            planes[i].normal = edge.Cross(direction).Normalized();
            planes[i].dist = planes[i].normal * poly->points[i];
            planes[i].SetSignBits();
            previous = i;
        }
    }
    else
    {
        for (i = 0, previous = poly->numPoints - 1; i < poly->numPoints; i++)
        {
            Vector3d edge = poly->points[i] - poly->points[previous];
            planes[i].normal = direction.Cross(edge).Normalized();
            planes[i].dist = planes[i].normal * poly->points[i];
            planes[i].SetSignBits();
            previous = i;
        }
    }
}

/*
==================
DirectionalLight::BuildOccluder

Builds the occluder (side planes and face-space extent) cast by a polygon.
==================
*/
void DirectionalLight::BuildOccluder(const Polygon *poly, Occluder *occluder)
{
    BuildSidePlanes(poly, occluder->sidePlanes);
    occluder->mins[0] = FLT_MAX;
    occluder->mins[1] = FLT_MAX;
    occluder->maxs[0] = -FLT_MAX;
    occluder->maxs[1] = -FLT_MAX;
    for (int i = 0; i < poly->numPoints; i++)
    {
        float s = poly->points[i] * axisU;
        if (s < occluder->mins[0])
            occluder->mins[0] = s;
        if (s > occluder->maxs[0])
            occluder->maxs[0] = s;
        float t = poly->points[i] * axisV;
        if (t < occluder->mins[1])
            occluder->mins[1] = t;
        if (t > occluder->maxs[1])
            occluder->maxs[1] = t;
    }
}

/*
==================
DirectionalLight::ProjectPoints

Projects world points along the light onto the face of the polygon.
==================
*/
void DirectionalLight::ProjectPoints(const Polygon *poly, const Vector3d *points, int numPoints, float (*st)[2])
{
    const Plane &plane = poly->GetInfo()->plane;
    double inverseCosine = 1.0 / (direction * plane.normal);
    for (int i = 0; i < numPoints; i++)
    {
        Vector3d projected = points[i] - direction *
            ((points[i] * plane.normal - plane.dist) * inverseCosine);
        poly->WorldToTexture(projected, st[i]);
    }
}

/*
==================
DirectionalLight::DirectionTo

Unit direction from the light to the point.
==================
*/
Vector3d DirectionalLight::DirectionTo(const Vector3d &point)
{
    return -direction;
}

/*
==================
DirectionalLight::AngularAttenuation

Positive surface incidence cosine; back-facing surfaces receive no light.
==================
*/
float DirectionalLight::AngularAttenuation(const Vector3d &point, const Vector3d &normal)
{
    double cosine = direction * normal;
    if (cosine <= 0.0)
        return 0.0f;
    return cosine;
}
