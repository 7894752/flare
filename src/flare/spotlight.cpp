/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Inverse-square spot lights with cone-edge fading and axial falloff.
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

/*
=============
Vector3d::operator-=

Subtract the corresponding components of v from this vector.
=============
*/
inline void Vector3d::operator-=(const Vector3d &v)
{
    x -= v.x;
    y -= v.y;
    z -= v.z;
}
#include "flare/color.h"

/* Fade to full intensity over 32 world units inside the cone edge. */
static const float spotEdgeScale = 1.0f / 32.0f;

/*
==================
SpotLight::Export

Fills the BSP light record for this light.
==================
*/
void SpotLight::Export(bspLight_t *out)
{
    memset(out, 0, sizeof(*out));
    out->type = BSP_LIGHT_SPOT;
    out->origin[0] = origin.x;
    out->origin[1] = origin.y;
    out->origin[2] = origin.z;
    out->direction[0] = direction.x;
    out->direction[1] = direction.y;
    out->direction[2] = direction.z;
    out->color[0] = color.r;
    out->color[1] = color.g;
    out->color[2] = color.b;
    out->data.spot.coneCosine = coneCosine;
    out->data.spot.exponent = exponent;
}

/*
==================
SpotLight::NumFaces

Number of passes (faces) the light is evaluated in.
==================
*/
int SpotLight::NumFaces()
{
    return 1;
}

/*
==================
SpotLight::SelectFace

Prepares the frame and the clip planes for one pass of the light.
==================
*/
void SpotLight::SelectFace(int face)
{
    radius = (float)sqrt(color.Luminance() * options.oneOverMinIntensity);
    radiusSq = radius * radius;
    if (coneCosine <= EQUAL_EPSILON)
        coneCosine = EQUAL_EPSILON;
    coneTangent = sqrt(1.0 / (coneCosine * coneCosine) - 1.0);

    Vector3d right = coneTangent * PerpendicularVector3d(direction);
    Vector3d up = right.Cross(direction);
    Vector3d corners[4];
    for (int i = 0; i < 4; i++)
    {
        corners[i] = -direction;
        if (i & 1)
            corners[i] -= right;
        else
            corners[i] += right;
        if (i & 2)
            corners[i] -= up;
        else
            corners[i] += up;
    }

    clip[0].normal = corners[1].Cross(corners[0]).Normalized();
    clip[0].dist = clip[0].normal * origin;
    clip[0].SetSignBits();
    clip[1].normal = corners[0].Cross(corners[2]).Normalized();
    clip[1].dist = clip[1].normal * origin;
    clip[1].SetSignBits();
    clip[2].normal = corners[2].Cross(corners[3]).Normalized();
    clip[2].dist = clip[2].normal * origin;
    clip[2].SetSignBits();
    clip[3].normal = corners[3].Cross(corners[1]).Normalized();
    clip[3].dist = clip[3].normal * origin;
    clip[3].SetSignBits();
    faceForward = -direction;
    faceRight = PerpendicularVector3d(faceForward);
    faceUp = faceForward.Cross(faceRight);
}

/*
==================
SpotLight::DistanceAttenuation

Inverse-square attenuation with cone-edge and axial-cosine falloff.
==================
*/
float SpotLight::DistanceAttenuation(const Vector3d &point)
{
    Vector3d delta = origin - point;
    double axial = delta * direction;
    if (axial < 0.0)
        return 0.0f;
    double distanceSq = delta * delta;
    double cosine = axial / sqrt(distanceSq);
    Vector3d projection(axial * direction.x, axial * direction.y, axial * direction.z);
    Vector3d radial = delta - projection;
    double attenuation = (axial * coneTangent - sqrt(radial * radial)) * spotEdgeScale;
    if (attenuation <= 0.0)
        return 0.0f;
    if (attenuation > 1.0)
        attenuation = 1.0;
    attenuation /= distanceSq;
    for (int i = exponent; i; i--)
        attenuation *= cosine;
    return attenuation;
}

/*
==================
SpotLight::AngularAttenuation

Cone and distance attenuation multiplied by the surface incidence cosine.
==================
*/
float SpotLight::AngularAttenuation(const Vector3d &point, const Vector3d &normal)
{
    Vector3d delta = origin - point;
    double axial = delta * direction;
    if (axial < 0.0)
        return 0.0f;
    double incidence = delta * normal;
    if (incidence < 0.0)
        return 0.0f;
    double distanceSq = delta * delta;
    double distance = sqrt(distanceSq);
    double cosine = axial / distance;
    Vector3d projection(axial * direction.x, axial * direction.y, axial * direction.z);
    Vector3d radial = delta - projection;
    double attenuation = (axial * coneTangent - sqrt(radial * radial)) * spotEdgeScale;
    if (attenuation <= 0.0)
        return 0.0f;
    if (attenuation > 1.0)
        attenuation = 1.0;
    attenuation = attenuation / (distanceSq * distance) * incidence;
    for (int i = exponent; i; i--)
        attenuation *= cosine;
    return attenuation;
}

/*
==================
SpotLight::Draw

Draws the light in the preview.
==================
*/
void SpotLight::Draw(bool selected)
{
    GL_Color3fv(&color.r);
    {
        Vector3d end = origin - direction * 8.0;
        GL_DrawLine(&origin, &end);
    }
    if (selected)
    {
        Vector3d reverse(-direction.x, -direction.y, -direction.z);
        Vector3d size(2.0, 2.0, 4.0);
        GL_DrawPyramid(&origin, &reverse, &size);
    }
}
