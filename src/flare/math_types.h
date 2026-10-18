/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Double-precision vectors, planes, colors, windings and float bounds.
===========================================================================
*/
#ifndef FLARE_MATH_TYPES_H
#define FLARE_MATH_TYPES_H

#include <float.h>
#include <math.h>
#include "epsilon.h"

struct Vector3d
{
    double x, y, z;

    Vector3d();
    void Clear()
    {
        x = 0;
        y = 0;
        z = 0;
    }
    Vector3d(double x, double y, double z);
    Vector3d(const Vector3d &v)
    {
        x = v.x;
        y = v.y;
        z = v.z;
    }

    operator double *();                            /* component array */
    double operator[](int index) const;             /* component value */
    Vector3d operator-() const;   /* unary minus */
    Vector3d operator-(const Vector3d &v) const;
    double operator*(const Vector3d &v) const;
    double &operator[](int index);                  /* mutable component access */
    Vector3d operator+(const Vector3d &v) const;
    Vector3d operator*(double scale) const;
    Vector3d operator/(double scale) const;
    void operator+=(const Vector3d &v);
    void operator-=(const Vector3d &v);
    void operator*=(double scale);
    void operator/=(double scale);
    bool operator==(const Vector3d &v) const
    {
        return (bool)(x == v.x && y == v.y && z == v.z);
    }
    bool operator!=(const Vector3d &v) const
    {
        return (bool)(x != v.x || y != v.y || z != v.z);
    }
    double LengthSquared() const;
    double Length() const;
    Vector3d Cross(Vector3d v) const;               /* by-value argument */
    Vector3d Normalized() const;
    void Normalize();                               /* requires nonzero length */
    double NormalizeLength();                       /* normalize and return the original length */
    void Snap();
    void Free(int count);                           /* returns count vectors to the vector pool */
};

/* Componentwise comparisons using the position and normal tolerances. */
inline bool VectorCompare(const Vector3d &a, const Vector3d &b)
{
    return fabs(a.x - b.x) <= EQUAL_EPSILON && fabs(a.y - b.y) <= EQUAL_EPSILON && fabs(a.z - b.z) <= EQUAL_EPSILON;
}

inline bool NormalCompare(const Vector3d &a, const Vector3d &b)
{
    return fabs(a.x - b.x) <= NORMAL_EPSILON && fabs(a.y - b.y) <= NORMAL_EPSILON && fabs(a.z - b.z) <= NORMAL_EPSILON;
}

/* Plane with per-axis sign flags used by the bounds support-point tests. */
struct Plane
{
    Vector3d normal;
    double dist;
    unsigned char sign[3];                          /* set when the normal component is negative */

    Plane();
    void SetSignBits();
};

/* ColorRGB supplies RGB storage and operations without construction.
 * Color3f adds value construction. */
struct Color3f;

struct ColorRGB
{
    float r, g, b;

    void operator+=(const ColorRGB &color)
    {
        r += color.r;
        g += color.g;
        b += color.b;
    }
    void operator*=(float scale);
    void operator/=(float scale);
    operator float *();
    void Clear();
    void Normalize();                               /* scales so the largest component is 1 */
    void FromBytes(const unsigned char *rgb);
    void ToBytes(unsigned char *rgb) const;
    float Luminance() const;
    inline Color3f operator*(const ColorRGB &color) const;
    inline Color3f operator*(float scale) const;
    inline Color3f operator/(float scale) const;
};

struct Color3f : ColorRGB
{
    Color3f() {}
    Color3f(const Color3f &color)
    {
        r = color.r;
        g = color.g;
        b = color.b;
    }
    Color3f(float red, float green, float blue)
    {
        r = red;
        g = green;
        b = blue;
    }
    Color3f(const ColorRGB &color)
    {
        r = color.r;
        g = color.g;
        b = color.b;
    }
    void operator=(const ColorRGB &color)
    {
        r = color.r;
        g = color.g;
        b = color.b;
    }
};

inline Color3f ColorRGB::operator*(const ColorRGB &color) const
{
    return Color3f(r * color.r, g * color.g, b * color.b);
}
inline Color3f ColorRGB::operator*(float scale) const
{
    return Color3f(r * scale, g * scale, b * scale);
}
inline Color3f ColorRGB::operator/(float scale) const
{
    scale = 1.0f / scale;
    return *this * scale;
}

/* Polygon outline owning a heap array of points. */
struct Winding
{
    unsigned short numPoints, maxPoints;
    Vector3d *points;

    Winding();
    ~Winding();
    Winding(const Winding &other)
    {
        numPoints = 0;
        maxPoints = 0;
        points = 0;
        Copy(other);
    }
    void operator=(const Winding &other);
    unsigned int NumPoints() const;
    Vector3d &operator[](int index);
    const Vector3d &operator[](int index) const;
    void Resize(int count);
    void AddPoint(const Vector3d &point);
    void Copy(const Winding &other);
    void ReverseCopy(const Winding &other);
    void BaseForPlane(const Plane &plane);
    int PlaneSide(const Plane &plane) const;
    Vector3d Center() const;
    void ClipFront(const Plane &plane);
    void ClipBack(const Plane &plane);
    int Split(const Plane &plane, Winding **front, Winding **back);    /* returns the classification (3 = crossing) */
    bool IsDegenerate() const;
    bool operator==(const Winding &other) const;
};

/* Single-precision bounds, indexed by side and axis or as mins/maxs. */
struct Bounds
{
    union
    {
        float points[2][3];
        struct
        {
            float mins[3];
            float maxs[3];
        };
    };

    /* Reset to empty extents (FLT_MAX / -FLT_MAX). */
    void Clear();
    void Zero();                                    /* all six floats 0 */
    void AddPoint(const Vector3d &point);
    void AddBounds(const Bounds &other);
    bool Contains(const Vector3d &point) const;
    double DistanceSquared(const Vector3d &point) const;
    bool IsInFrontOfPlane(const Plane &plane) const;
    bool IsBehindPlane(const Plane &plane) const;
};

/* Return a unit vector perpendicular to v. */
Vector3d PerpendicularVector3d(const Vector3d &v);

#endif
