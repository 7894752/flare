/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Inverse-square point lights, cube-face clipping and sampling.
===========================================================================
*/
#include "flare/math_types.h"
#include "flare/light_local.h"
#include "flare/polygon.h"
#include "flare/color.h"
#include "flare/traceresult.h"

#define MIN_POINT_LIGHT_DISTANCE_SQUARED (16.0 * 16.0)
#define LIGHT_TRACE_IGNORE_FLAGS 0x4B

/*
Side-plane normals of the four sides of each of the six 90 degree frusta
(one per cube face) of a point light.  The nonzero components are
+-1/sqrt(2). The three face tables give each view's right, up and forward axes.
*/
extern const double rootHalf = 0.7071067811865475;
Vector3d frustumNormals[6][4] = {
    {
        Vector3d(-rootHalf, rootHalf, 0),
        Vector3d(-rootHalf, -rootHalf, 0),
        Vector3d(-rootHalf, 0, rootHalf),
        Vector3d(-rootHalf, 0, -rootHalf)
    },
    {
        Vector3d(rootHalf, rootHalf, 0),
        Vector3d(rootHalf, -rootHalf, 0),
        Vector3d(rootHalf, 0, rootHalf),
        Vector3d(rootHalf, 0, -rootHalf)
    },
    {
        Vector3d(rootHalf, -rootHalf, 0),
        Vector3d(-rootHalf, -rootHalf, 0),
        Vector3d(0, -rootHalf, rootHalf),
        Vector3d(0, -rootHalf, -rootHalf)
    },
    {
        Vector3d(rootHalf, rootHalf, 0),
        Vector3d(-rootHalf, rootHalf, 0),
        Vector3d(0, rootHalf, rootHalf),
        Vector3d(0, rootHalf, -rootHalf)
    },
    {
        Vector3d(rootHalf, 0, -rootHalf),
        Vector3d(-rootHalf, 0, -rootHalf),
        Vector3d(0, rootHalf, -rootHalf),
        Vector3d(0, -rootHalf, -rootHalf)
    },
    {
        Vector3d(rootHalf, 0, rootHalf),
        Vector3d(-rootHalf, 0, rootHalf),
        Vector3d(0, rootHalf, rootHalf),
        Vector3d(0, -rootHalf, rootHalf)
    }
};

Vector3d faceRightTable[6] = {
    Vector3d(0, 1, 0), Vector3d(0, 0, -1), Vector3d(0, 0, 1), Vector3d(-1, 0, 0), Vector3d(1, 0, 0), Vector3d(0, -1, 0)
};
Vector3d faceUpTable[6] = {
    Vector3d(0, 0, 1), Vector3d(0, -1, 0), Vector3d(1, 0, 0), Vector3d(0, 0, -1), Vector3d(0, 1, 0), Vector3d(-1, 0, 0)
};
Vector3d faceForwardTable[6] = {
    Vector3d(1, 0, 0), Vector3d(-1, 0, 0), Vector3d(0, 1, 0), Vector3d(0, -1, 0), Vector3d(0, 0, 1), Vector3d(0, 0, -1)
};

/*
==================
PointLight::NumFaces

Number of passes (faces) the light is evaluated in.
==================
*/
int PointLight::NumFaces()
{
    return 6;
}

/*
==================
PointLight::HasOrigin

True when the light has a position.
==================
*/
bool PointLight::HasOrigin()
{
    return true;
}

/*
==================
PointLight::IsDirectional

True when the light is a directional (sun) light.
==================
*/
bool PointLight::IsDirectional()
{
    return false;
}

/*
==================
PointLight::HasPolygon

True when the light is bound to a source polygon.
==================
*/
bool PointLight::HasPolygon()
{
    return false;
}

/*
==================
PointLight::GetPolygon

The source polygon of the light.
==================
*/
Polygon *PointLight::GetPolygon()
{
    return 0;
}

/*
==================
PointLight::Export

Fills the BSP light record for this light.
==================
*/
void PointLight::Export(bspLight_t *out)
{
    memset(out, 0, sizeof(*out));
    out->type = BSP_LIGHT_POINT;
    out->origin[0] = origin.x;
    out->origin[1] = origin.y;
    out->origin[2] = origin.z;
    out->color[0] = color.r;
    out->color[1] = color.g;
    out->color[2] = color.b;
}

/*
=============
Vector3d::operator[]

Return the component at index, where 0, 1 and 2 select x, y and z.
=============
*/
inline double Vector3d::operator[](int index) const
{
    return (&x)[index];
}

/*
==================
PointLight::DirectionTo

Unit direction from the light to the point.
==================
*/
Vector3d PointLight::DirectionTo(const Vector3d &point)
{
    return (point - origin).Normalized();
}

/*
==================
PointLight::DistanceAttenuation

Falloff by the distance to the light.
==================
*/
float PointLight::DistanceAttenuation(const Vector3d &point)
{
    Vector3d delta = origin - point;
    return 1.0 / (delta * delta);
}

/*
==================
PointLight::AngularAttenuation

Inverse-square attenuation multiplied by the surface incidence cosine.
==================
*/
float PointLight::AngularAttenuation(const Vector3d &point, const Vector3d &normal)
{
    Vector3d delta = origin - point;
    double incidence = delta * normal;
    if (incidence < 0.0)
        return 0.0f;
    double distanceSq = delta * delta;
    double distance = sqrt(distanceSq);
    /* Limit the inverse-square term near the source. */
    if (distanceSq < MIN_POINT_LIGHT_DISTANCE_SQUARED)
        distanceSq = MIN_POINT_LIGHT_DISTANCE_SQUARED;
    return incidence / (distanceSq * distance);
}

/*
=============
Bounds::DistanceSquared

Return the squared distance from point to the nearest point in the box.
Points inside the box have zero distance.
=============
*/
inline double Bounds::DistanceSquared(const Vector3d &point) const
{
    double distanceSq = 0.0;
    double delta;
    int i;

    for (i = 0; i < 3; i++)
    {
        delta = point[i] - maxs[i];
        if (delta > 0.0 || (delta = mins[i] - point[i]) > 0.0)
            distanceSq += delta * delta;
    }
    return distanceSq;
}

/*
==================
PointLight::SelectFace

Prepares the frame and the clip planes for one pass of the light.
==================
*/
void PointLight::SelectFace(int face)
{
    if (face == 0)
    {
        radiusSq = color.Luminance() * options.oneOverMinIntensity;
        radius = sqrt(radiusSq);
    }
    clip[0].normal = frustumNormals[face][0];
    clip[0].dist = clip[0].normal * origin;
    clip[0].SetSignBits();
    clip[1].normal = frustumNormals[face][1];
    clip[1].dist = clip[1].normal * origin;
    clip[1].SetSignBits();
    clip[2].normal = frustumNormals[face][2];
    clip[2].dist = clip[2].normal * origin;
    clip[2].SetSignBits();
    clip[3].normal = frustumNormals[face][3];
    clip[3].dist = clip[3].normal * origin;
    clip[3].SetSignBits();
    faceRight = faceRightTable[face];
    faceUp = faceUpTable[face];
    faceForward = faceForwardTable[face];
}

/*
==================
PointLight::GetOrigin

Position of the light.
==================
*/
Vector3d PointLight::GetOrigin()
{
    return origin;
}

/*
==================
PointLight::GetDirection

Direction the light shines in.
==================
*/
Vector3d PointLight::GetDirection()
{
    return faceForward;
}

/*
==================
PointLight::IsOutside

True when the point lies outside the light volume.
==================
*/
bool PointLight::IsOutside(const Vector3d &point)
{
    Vector3d delta = origin - point;
    return (delta * delta > radiusSq
        || clip[0].normal * point > clip[0].dist
        || clip[1].normal * point > clip[1].dist
        || point * clip[2].normal > clip[2].dist
        || point * clip[3].normal > clip[3].dist) ? true : false;
}

/*
==================
PointLight::ClassifyBounds

Classifies a node's bounds against the light volume:
0 inside, 1 outside, 2 straddling.
==================
*/
int PointLight::ClassifyBounds(const Bounds &bounds)
{
    if (bounds.DistanceSquared(origin) > radiusSq)
        return 1;
    if (bounds.IsInFrontOfPlane(clip[0]) || bounds.IsInFrontOfPlane(clip[1])
        || bounds.IsInFrontOfPlane(clip[2]) || bounds.IsInFrontOfPlane(clip[3]))
        return 1;
    if (bounds.IsBehindPlane(clip[0]) && bounds.IsBehindPlane(clip[1])
        && bounds.IsBehindPlane(clip[2]) && bounds.IsBehindPlane(clip[3]))
        return 0;
    return 2;
}

/*
==================
PointLight::IsBackFacing

True when the light lies behind the plane of the polygon.
==================
*/
bool PointLight::IsBackFacing(const Polygon *poly)
{
    const Plane &plane = poly->info->plane;
    return origin * plane.normal < plane.dist ? true : false;
}

/*
==================
PointLight::ClassifyPlane

Classifies a split plane against the light: which side it is on,
whether the far side is reached too, and the per-side visit flags.
==================
*/
void PointLight::ClassifyPlane(const Plane *plane, int *nearSide, bool *reachesFar, bool visit[2], bool *onPlane)
{
    double distance = origin * plane->normal - plane->dist;
    double epsilon = fabs(distance) * NORMAL_EPSILON;

    if (EQUAL_EPSILON > epsilon)
        epsilon = EQUAL_EPSILON;
    *onPlane = (distance > -epsilon && distance < epsilon);
    if (distance > 0.0)
    {
        *nearSide = 0;
        *reachesFar = (distance < radius);
        visit[0] = true;
        visit[1] = false;
    }
    else
    {
        *nearSide = 1;
        *reachesFar = (distance > -radius);
        visit[0] = false;
        visit[1] = true;
    }
}

/*
==================
PointLight::ClipPolygon

Clips the polygon to the light volume.
==================
*/
Polygon *PointLight::ClipPolygon(Polygon *poly)
{
    int i;

    if (poly->bounds.DistanceSquared(origin) > radiusSq)
        return 0;
    SaveTemporaryPolygons((int)poly);
    i = 4;
    do
    {
        poly = ClipPolygonBack(poly, clip[--i]);
    } while (i && poly);
    return RestoreTemporaryPolygons(poly);
}

/*
==================
PointLight::BuildSidePlanes

Builds one side plane per edge of the polygon.
==================
*/
void PointLight::BuildSidePlanes(const Polygon *poly, Plane *planes)
{
    int count = poly->numPoints;
    Vector3d directions[2];
    int current;
    int i;

    directions[0] = poly->points[count - 1] - origin;
    current = 0;
    if (poly->info->plane.normal * origin < poly->info->plane.dist)
    {
        for (i = 0; i < poly->numPoints; i++)
        {
            int previous = current;
            current ^= 1;
            directions[current] = poly->points[i] - origin;
            Plane &plane = planes[i];
            plane.normal = directions[previous].Cross(directions[current]).Normalized();
            plane.dist = plane.normal * origin;
            plane.SetSignBits();
        }
    }
    else
    {
        for (i = 0; i < poly->numPoints; i++)
        {
            int previous = current;
            current ^= 1;
            directions[current] = poly->points[i] - origin;
            planes[i].normal = directions[current].Cross(directions[previous]).Normalized();
            planes[i].SetSignBits();
            planes[i].dist = origin * planes[i].normal;
        }
    }
}

/*
==================
PointLight::BuildOccluder

Builds the occluder (side planes and face-space extent) cast by a polygon.
==================
*/
void PointLight::BuildOccluder(const Polygon *poly, Occluder *occluder)
{
    int i;

    BuildSidePlanes(poly, occluder->sidePlanes);
    occluder->mins[0] = FLT_MAX;
    occluder->mins[1] = FLT_MAX;
    occluder->maxs[0] = -FLT_MAX;
    occluder->maxs[1] = -FLT_MAX;
    for (i = 0; i < poly->numPoints; i++)
    {
        Vector3d direction = poly->points[i] - origin;
        double inverseDepth = 1.0 / (direction * faceForward);
        float s = (direction * faceRight) * inverseDepth;
        float t;

        if (occluder->mins[0] > s)
            occluder->mins[0] = s;
        if (occluder->maxs[0] < s)
            occluder->maxs[0] = s;
        t = (direction * faceUp) * inverseDepth;
        if (occluder->mins[1] > t)
            occluder->mins[1] = t;
        if (occluder->maxs[1] < t)
            occluder->maxs[1] = t;
    }
}

/*
==================
PointLight::ProjectPoints

Projects world points along the light onto the face of the polygon.
==================
*/
void PointLight::ProjectPoints(const Polygon *poly, const Vector3d *points, int numPoints, float (*st)[2])
{
    const Plane &plane = poly->info->plane;
    double lightDistance = origin * plane.normal - plane.dist;
    int i;

    for (i = 0; i < numPoints; i++)
    {
        double distance = points[i] * plane.normal - plane.dist;
        double fraction = distance / (distance - lightDistance);
        Vector3d projected = origin * fraction + points[i] * (1.0 - fraction);

        poly->WorldToTexture(projected, st[i]);
    }
}

/*
==================
PointLight::IsUnoccluded

True when nothing blocks the way between the point and the light.
==================
*/
bool PointLight::IsUnoccluded(const Vector3d &point)
{
    TraceResult trace;

    TraceSegment(point, origin, &trace, LIGHT_TRACE_IGNORE_FLAGS);
    return trace.hit == 0 ? true : false;
}

/*
==================
PointLight::Draw

Draws the light in the preview.
==================
*/
void PointLight::Draw(bool selected)
{
    float size = selected ? 8.0f : 4.0f;
    Vector3d drawRadius(size, size, size);

    GL_Color3fv(&color.r);
    GL_DrawOctahedron(&origin, &drawRadius);
    if (selected)
    {
        float white[3] = { 1.0f, 1.0f, 1.0f };

        GL_Color3fv(white);
        GL_DrawOctahedronOutline(&origin, &drawRadius);
    }
}
