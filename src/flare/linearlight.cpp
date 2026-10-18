/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Point lights with linear distance falloff and optional angle scaling.
===========================================================================
*/
#include "flare/light_local.h"

/*
==================
LinearLight::Export

Fills the BSP light record for this light.
==================
*/
void LinearLight::Export(bspLight_t *out)
{
    memset(out, 0, sizeof(*out));
    out->type = BSP_LIGHT_LINEAR;
    out->origin[0] = origin.x;
    out->origin[1] = origin.y;
    out->origin[2] = origin.z;
    out->color[0] = color.r;
    out->color[1] = color.g;
    out->color[2] = color.b;
    out->data.linear.inverseRange = range;
}

/*
==================
LinearLight::SelectFace

Prepares the frame and the clip planes for one pass of the light.
==================
*/
void LinearLight::SelectFace(int face)
{
    PointLight::SelectFace(face);
    if (face == 0)
    {
        radius = 1.0f / range;
        radiusSq = radius * radius;
    }
}

/*
==================
LinearLight::DistanceAttenuation

Falloff by the distance to the light.
==================
*/
float LinearLight::DistanceAttenuation(const Vector3d &point)
{
    Vector3d delta = origin - point;
    double distanceSq = delta * delta;
    if (distanceSq >= radiusSq)
        return 0.0f;
    return 1.0 - sqrt(distanceSq) / radius;
}

/*
==================
LinearLight::AngularAttenuation

Linear falloff, optionally reduced by the surface incidence cosine.
==================
*/
float LinearLight::AngularAttenuation(const Vector3d &point, const Vector3d &normal)
{
    if (angleScaled)
    {
        Vector3d delta = origin - point;
        double distance = sqrt(delta * delta);
        double attenuation = (delta * normal) / distance - distance / radius;
        if (attenuation <= 0.0)
            return 0.0f;
        return attenuation;
    }
    Vector3d delta = origin - point;
    if (delta * delta >= radiusSq)
        return 0.0f;
    return 1.0 - sqrt(delta * delta) / radius;
}

/*
==================
LinearLight::Draw

Draws the light in the preview.
==================
*/
void LinearLight::Draw(bool selected)
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
