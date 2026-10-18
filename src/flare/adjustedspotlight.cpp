/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Spot lights with quadratic and constant distance attenuation.
===========================================================================
*/
#include "flare/light_local.h"

/* Fade to full intensity over 32 world units inside the cone edge. */
static const float adjustedSpotEdgeScale = 1.0f / 32.0f;

/*
==================
AdjustedSpotLight::Export

Fills the BSP light record for this light.
==================
*/
void AdjustedSpotLight::Export(bspLight_t *out)
{
    memset(out, 0, sizeof(*out));
    out->type = BSP_LIGHT_ADJUSTED_SPOT;
    out->origin[0] = origin.x;
    out->origin[1] = origin.y;
    out->origin[2] = origin.z;
    out->direction[0] = direction.x;
    out->direction[1] = direction.y;
    out->direction[2] = direction.z;
    out->color[0] = color.r;
    out->color[1] = color.g;
    out->color[2] = color.b;
    out->data.adjustedSpot.quadratic = quadratic;
    out->data.adjustedSpot.constant = constant;
    out->data.adjustedSpot.coneCosine = coneCosine;
    out->data.adjustedSpot.exponent = exponent;
}

/*
==================
AdjustedSpotLight::DistanceAttenuation

Distance attenuation with the cone edge and axial-cosine falloff.
==================
*/
float AdjustedSpotLight::DistanceAttenuation(const Vector3d &point)
{
    Vector3d delta = origin - point;
    double axial = delta * direction;
    if (axial < 0.0)
        return 0.0f;
    double distanceSq = delta * delta;
    double cosine = axial / sqrt(distanceSq);
    Vector3d projection(axial * direction.x, axial * direction.y, axial * direction.z);
    Vector3d radial = delta - projection;
    double attenuation = (axial * coneTangent - sqrt(radial * radial)) * adjustedSpotEdgeScale;
    if (attenuation <= 0.0)
        return 0.0f;
    if (attenuation > 1.0)
        attenuation = 1.0;
    attenuation /= quadratic * distanceSq + constant;
    for (int i = exponent; i; i--)
        attenuation *= cosine;
    return attenuation;
}

/*
==================
AdjustedSpotLight::AngularAttenuation

Cone and distance attenuation multiplied by the surface incidence cosine.
==================
*/
float AdjustedSpotLight::AngularAttenuation(const Vector3d &point, const Vector3d &normal)
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
    double attenuation = (axial * coneTangent - sqrt(radial * radial)) * adjustedSpotEdgeScale;
    if (attenuation <= 0.0)
        return 0.0f;
    if (attenuation > 1.0)
        attenuation = 1.0;
    attenuation = attenuation / ((quadratic * distanceSq + constant) * distance) * incidence;
    for (int i = exponent; i; i--)
        attenuation *= cosine;
    return attenuation;
}
