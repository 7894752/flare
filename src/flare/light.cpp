/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Shared light sampling and attenuation bounds.
===========================================================================
*/
#include "flare/light_local.h"

/*
==================
Light::GetSource

The light that owns the BSP record.
==================
*/
Light *Light::GetSource()
{
    return this;
}

#include "polygon.h"
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

/*
==================
Light::Illuminate

Evaluate the light at the footprint's centroid using the interpolated surface
normal. Derived lights can integrate the footprint instead.
==================
*/
Color3f Light::Illuminate(const Polygon *poly, const float (*points)[2], int numPoints, float weight, const float *st)
{
    Vector3d point, normal;

    poly->LightmapToWorld(st[0], st[1], &point);
    poly->InterpolateNormal(&point, &normal);
    return color * AngularAttenuation(point, normal);
}

/*
==================
Light::MaxAttenuation

Estimate the greatest attenuation over a polygon, scaled by the light's
luminance. Test its vertices and, for lights with an origin, the perpendicular
projection onto its plane when that point lies inside the polygon.
==================
*/
float Light::MaxAttenuation(const Polygon *poly)
{
    float maxAttenuation = 0.0f;
    int i;

    if (HasOrigin())
    {
        Vector3d origin = GetOrigin();
        const Plane &plane = poly->GetInfo()->plane;
        double distance = origin * plane.normal - plane.dist;
        Vector3d projected = origin - plane.normal * distance;

        if (poly->PointOnPolygon(projected))
            maxAttenuation = DistanceAttenuation(projected);
    }
    for (i = 0; i < poly->numPoints; i++)
    {
        float attenuation = DistanceAttenuation(poly->points[i]);
        if (maxAttenuation < attenuation)
            maxAttenuation = attenuation;
    }
    return color.Luminance() * maxAttenuation;
}
