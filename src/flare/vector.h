/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Inline Vector3d construction and arithmetic.
===========================================================================
*/
#ifndef FLARE_VECTOR_H
#define FLARE_VECTOR_H

#include "flare/math_types.h"

inline Vector3d::Vector3d()
{}

inline Vector3d::Vector3d(double x, double y, double z) : x(x), y(y), z(z)
{}

inline Vector3d Vector3d::operator-() const
{
    return Vector3d(-x, -y, -z);
}

inline Vector3d Vector3d::operator-(const Vector3d &v) const
{
    return Vector3d(x - v.x, y - v.y, z - v.z);
}

inline double Vector3d::operator*(const Vector3d &v) const
{
    return x * v.x + y * v.y + z * v.z;
}

inline Vector3d Vector3d::operator+(const Vector3d &v) const
{
    return Vector3d(x + v.x, y + v.y, z + v.z);
}

inline Vector3d operator*(double scale, const Vector3d &v)
{
    return Vector3d(scale * v.x, scale * v.y, scale * v.z);
}

inline Vector3d Vector3d::operator*(double scale) const
{
    return Vector3d(x * scale, y * scale, z * scale);
}

inline Vector3d Vector3d::operator/(double scale) const
{
    return *this * (1.0 / scale);
}

/* Cross product with the right operand passed by value. */
inline Vector3d Vector3d::Cross(Vector3d v) const
{
    return Vector3d(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
}

#endif
