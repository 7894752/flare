/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Inline Bounds member functions.
===========================================================================
*/
#ifndef FLARE_BOUNDS_H
#define FLARE_BOUNDS_H

#include "flare/math_types.h"

inline bool Bounds::IsInFrontOfPlane(const Plane &plane) const
{
    double distance = points[plane.sign[0]][0] * plane.normal.x
                    + points[plane.sign[1]][1] * plane.normal.y
                    + points[plane.sign[2]][2] * plane.normal.z;
    return distance > plane.dist ? true : false;
}

/* Bounds support point behind a plane. */

inline bool Bounds::IsBehindPlane(const Plane &plane) const
{
    double distance = points[1 - plane.sign[0]][0] * plane.normal.x
                    + points[1 - plane.sign[1]][1] * plane.normal.y
                    + points[1 - plane.sign[2]][2] * plane.normal.z;
    return distance < plane.dist ? true : false;
}

inline void Bounds::AddPoint(const Vector3d &point)
{
    if (mins[0] > point.x) mins[0] = (float)point.x;
    if (maxs[0] < point.x) maxs[0] = (float)point.x;
    if (mins[1] > point.y) mins[1] = (float)point.y;
    if (maxs[1] < point.y) maxs[1] = (float)point.y;
    if (mins[2] > point.z) mins[2] = (float)point.z;
    if (maxs[2] < point.z) maxs[2] = (float)point.z;
}

#endif
