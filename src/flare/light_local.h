/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Light-class clipping, tracing and preview declarations.
===========================================================================
*/
#ifndef FLARE_LIGHT_LOCAL_H
#define FLARE_LIGHT_LOCAL_H

#include "flare/optiondefs.h"

#include <string.h>
#include "flare/light.h"

#define MAX_SKY_POLY_SIDES	17

extern Light *sunSource;
extern int numSunPlanes;
/* Slot 0 is the sky plane, leaving room for at most 16 side planes.
 * SelectFace also permits 17-point polygons, which exceed this storage. */
extern Plane sunPlanes[MAX_SKY_POLY_SIDES];

extern const double directionalPlaneEpsilon;

double PointToPolygonFormFactor(const Vector3d &point, const Vector3d &normal,
                                const Vector3d *points, int count);
extern void Q3Map_Error(const char *format, ...);
/* Saves an opaque caller value; current callers pass the source polygon address. */
extern void SaveTemporaryPolygons(int savedValue);
extern Polygon *RestoreTemporaryPolygons(Polygon *poly);
extern Polygon *ClipPolygonBack(const Polygon *poly, const Plane &plane);
extern void TraceSegment(const Vector3d &start, const Vector3d &end,
                         TraceResult *trace, unsigned int mask);

extern "C" {
void GL_Color3fv(const float *color);
void GL_Color4f(float r, float g, float b, float alpha);
void GL_DrawOctahedron(const Vector3d *origin, const Vector3d *radius);
void GL_DrawOctahedronOutline(const Vector3d *origin, const Vector3d *radius);
void GL_DrawLine(const Vector3d *start, const Vector3d *end);
void GL_DrawWinding(const Polygon *poly);
void GL_DrawWindingOutline(const Polygon *poly);
void GL_DrawPyramidOutline(const Vector3d *origin, const Vector3d *direction, const Vector3d *radius);
void GL_DrawPyramid(const Vector3d *origin, const Vector3d *direction, const Vector3d *radius);
}

inline void Plane::SetSignBits()
{
    sign[0] = normal.x < 0.0 ? true : false;
    sign[1] = normal.y < 0.0 ? true : false;
    sign[2] = normal.z < 0.0 ? true : false;
}

inline Vector3d Vector3d::Normalized() const
{
    double length = sqrt(*this * *this);
    if (length != 0.0)
        return *this * (1.0 / length);
    return Vector3d(0.0, 0.0, 0.0);
}

#endif
