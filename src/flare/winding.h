/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Inline Winding member functions.
===========================================================================
*/
#ifndef FLARE_WINDING_H
#define FLARE_WINDING_H

#include "flare/math_types.h"

inline Winding::Winding()
{
    numPoints = 0;
    maxPoints = 0;
    points = 0;
}

inline Winding::~Winding()
{
    delete[] points;
    points = 0;
}

inline unsigned int Winding::NumPoints() const
{
    return numPoints;
}

inline Vector3d &Winding::operator[](int index)
{
    return points[index];
}

inline const Vector3d &Winding::operator[](int index) const
{
    return points[index];
}

inline void Winding::operator=(const Winding &other)
{
    Copy(other);
}

#endif
