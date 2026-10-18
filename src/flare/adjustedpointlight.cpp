/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Point lights with quadratic and constant distance attenuation.
===========================================================================
*/
#include "flare/light_local.h"

/*
==================
AdjustedPointLight::Export

Fills the BSP light record for this light.
==================
*/
void AdjustedPointLight::Export(bspLight_t *out)
{
    memset(out, 0, sizeof(*out));
    out->type = BSP_LIGHT_ADJUSTED_POINT;
    out->origin[0] = origin.x;
    out->origin[1] = origin.y;
    out->origin[2] = origin.z;
    out->color[0] = color.r;
    out->color[1] = color.g;
    out->color[2] = color.b;
    out->data.adjustedPoint.quadratic = quadratic;
    out->data.adjustedPoint.constant = constant;
}

/*
==================
AdjustedPointLight::DistanceAttenuation

Falloff by the distance to the light.
==================
*/
float AdjustedPointLight::DistanceAttenuation(const Vector3d &point)
{
    Vector3d delta = origin - point;
    return 1.0 / (quadratic * (delta * delta) + constant);
}

/*
==================
AdjustedPointLight::AngularAttenuation

Distance attenuation multiplied by the surface incidence cosine.
==================
*/
float AdjustedPointLight::AngularAttenuation(const Vector3d &point, const Vector3d &normal)
{
    Vector3d delta = origin - point;
    double cosine = delta * normal;
    if (cosine < 0.0)
        return 0.0f;
    double distanceSq = delta * delta;
    return cosine / (sqrt(distanceSq) * (quadratic * distanceSq + constant));
}
