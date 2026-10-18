/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Double-precision winding construction, clipping, splitting and tests.
===========================================================================
*/

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2
#define SIDE_CROSS 3
#define MIN_WINDING_EDGE_LENGTH 0.4f
/* The cross-product sum is twice the polygon area. */
#define MIN_WINDING_CROSS_SQUARED 4.0

static const float maxWorldSize = 268435456.0f;

#include "flare/math_types.h"
#include "flare/print.h"
#include "flare/geometry.h"
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
Vector3d::LengthSquared

Return the sum of the squared components without taking a square root.
=============
*/
inline double Vector3d::LengthSquared() const
{
    return x * x + y * y + z * z;
}

#include "flare/mathlib.h"


/*
=============
Vector3d::operator[]

Return the component at index, where 0, 1 and 2 select x, y and z.
=============
*/
inline double Vector3d::operator[](int index) const
{
    return (&x)[index];
}

/*
=============
Vector3d::operator[]

Return a reference to the component at index so the caller can change it.
Indices 0, 1 and 2 select x, y and z.
=============
*/
inline double &Vector3d::operator[](int index)
{
    return (&x)[index];
}

/*
=============
Vector3d::operator*=

Multiply each component of this vector by scale.
=============
*/
inline void Vector3d::operator*=(double scale)
{
    x *= scale;
    y *= scale;
    z *= scale;
}


/*
=============
Winding::Resize

Grow vertex capacity without changing the winding's point count.
=============
*/
void Winding::Resize( int count )
{
    int i;
    if ( count <= maxPoints )
        return;
    if ( count > MAX_VERTS_PER_POLY )
        Q3Map_Error( "Winding tried to exceed MAX_VERTS_PER_POLY (%i)\n", MAX_VERTS_PER_POLY );
    if ( numPoints )
    {
        Vector3d saved[MAX_VERTS_PER_POLY];
        for ( i = 0; i < numPoints; i++ )
            saved[i] = points[i];
        delete[] points;
        points = new Vector3d[count];
        if ( !points )
            Q3Map_Error( "Couldn't allocate winding vertices (%i vertices, %i bytes)\n", count, count * sizeof( Vector3d ) );
        for ( i = 0; i < numPoints; i++ )
            points[i] = saved[i];
    }
    else
    {
        delete[] points;
        points = new Vector3d[count];
        if ( !points )
            Q3Map_Error( "Couldn't allocate winding vertices (%i vertices, %i bytes)\n", count, count * sizeof( Vector3d ) );
    }
    maxPoints = count;
}

/*
=============
Winding::AddPoint
=============
*/
void Winding::AddPoint( const Vector3d &point )
{
    if ( numPoints == maxPoints )
        Resize( maxPoints + 2 );
    points[numPoints] = point;
    numPoints++;
}

/*
=============
Winding::Copy
=============
*/
void Winding::Copy( const Winding &other )
{
    numPoints = 0;
    Resize( other.numPoints );
    numPoints = other.NumPoints();
    for ( int i = 0; i < numPoints; i++ )
        points[i] = other.points[i];
}

/*
=============
Winding::ReverseCopy
=============
*/
void Winding::ReverseCopy( const Winding &other )
{
    numPoints = 0;
    Resize( other.numPoints );
    numPoints = other.NumPoints();
    for ( int i = 0; i < numPoints; i++ )
        points[numPoints - 1 - i] = other.points[i];
}

/*
=============
Winding::BaseForPlane
=============
*/
void Winding::BaseForPlane( const Plane &plane )
{
    Vector3d right, org, up;
    BuildOrthonormalBasis( plane.normal, right, up );
    org = plane.normal * plane.dist;
    right *= 2.0 * maxWorldSize;
    up *= 2.0 * maxWorldSize;
    numPoints = 0;
    Resize( 4 );
    numPoints = 4;
    points[0] = org - right - up;
    points[1] = org - right + up;
    points[2] = org + right + up;
    points[3] = org + right - up;
}

/*
=============
Winding::ClipFront
=============
*/
void Winding::ClipFront( const Plane &plane )
{
    int i;
    Vector3d verts[MAX_VERTS_PER_POLY];
    int pointSide[MAX_VERTS_PER_POLY];
    double epsilon = fabs( NORMAL_EPSILON * plane.dist ) < EQUAL_EPSILON ? EQUAL_EPSILON : fabs( NORMAL_EPSILON * plane.dist );
    /* Preserve the input while rebuilding the clipped winding. */
    double dists[MAX_VERTS_PER_POLY];
    int sideCount[2] = { 0, 0 };
    int lastVertex;
    for ( i = 0; i < numPoints; i++ )
    {
        verts[i] = points[i];
        dists[i] = plane.normal * points[i] - plane.dist;
        if ( dists[i] > epsilon )
        {
            pointSide[i] = SIDE_FRONT;
            sideCount[SIDE_FRONT]++;
        }
        else if ( dists[i] < -epsilon )
        {
            pointSide[i] = SIDE_BACK;
            sideCount[SIDE_BACK]++;
        }
        else
        {
            pointSide[i] = SIDE_ON;
        }
    }
    if ( !sideCount[SIDE_BACK] )
        return;
    if ( sideCount[SIDE_BACK] == numPoints )
    {
        numPoints = 0;
        return;
    }
    int inputCount = numPoints;
    numPoints = 0;
    lastVertex = inputCount - 1;
    for ( i = 0; i < inputCount; lastVertex = i, i++ )
    {
        if ( pointSide[lastVertex] == SIDE_ON )
        {
            AddPoint( verts[lastVertex] );
        }
        else
        {
            if ( pointSide[lastVertex] == SIDE_FRONT )
                AddPoint( verts[lastVertex] );
            if ( pointSide[i] != SIDE_ON && pointSide[i] != pointSide[lastVertex] )
            {
                Vector3d mid;
                for ( int coord = 0; coord < 3; coord++ )
                {
                    if ( plane.normal[coord] == 1 )
                        mid[coord] = plane.dist;
                    else if ( plane.normal[coord] == -1 )
                        mid[coord] = -plane.dist;
                    else
                        mid[coord] = (verts[i][coord] * dists[lastVertex] - verts[lastVertex][coord] * dists[i]) / (dists[lastVertex] - dists[i]);
                }
                AddPoint( mid );
            }
        }
    }
}

/*
=============
Winding::ClipBack
=============
*/
void Winding::ClipBack( const Plane &plane )
{
    int i;
    Vector3d verts[MAX_VERTS_PER_POLY];
    int pointSide[MAX_VERTS_PER_POLY];
    double epsilon = fabs( NORMAL_EPSILON * plane.dist ) < EQUAL_EPSILON ? EQUAL_EPSILON : fabs( NORMAL_EPSILON * plane.dist );
    /* Preserve the input while rebuilding the clipped winding. */
    double dists[MAX_VERTS_PER_POLY];
    int sideCount[2] = { 0, 0 };
    int lastVertex;
    for ( i = 0; i < numPoints; i++ )
    {
        verts[i] = points[i];
        dists[i] = plane.normal * points[i] - plane.dist;
        if ( dists[i] > epsilon )
        {
            pointSide[i] = SIDE_FRONT;
            sideCount[SIDE_FRONT]++;
        }
        else if ( dists[i] < -epsilon )
        {
            pointSide[i] = SIDE_BACK;
            sideCount[SIDE_BACK]++;
        }
        else
        {
            pointSide[i] = SIDE_ON;
        }
    }
    if ( !sideCount[SIDE_FRONT] )
        return;
    if ( sideCount[SIDE_FRONT] == numPoints )
    {
        numPoints = 0;
        return;
    }
    int inputCount = numPoints;
    numPoints = 0;
    lastVertex = inputCount - 1;
    for ( i = 0; i < inputCount; lastVertex = i, i++ )
    {
        if ( pointSide[lastVertex] == SIDE_ON )
        {
            AddPoint( verts[lastVertex] );
        }
        else
        {
            if ( pointSide[lastVertex] == SIDE_BACK )
                AddPoint( verts[lastVertex] );
            if ( pointSide[i] != SIDE_ON && pointSide[i] != pointSide[lastVertex] )
            {
                Vector3d mid;
                for ( int coord = 0; coord < 3; coord++ )
                {
                    if ( plane.normal[coord] == 1 )
                        mid[coord] = plane.dist;
                    else if ( plane.normal[coord] == -1 )
                        mid[coord] = -plane.dist;
                    else
                        mid[coord] = (verts[i][coord] * dists[lastVertex] - verts[lastVertex][coord] * dists[i]) / (dists[lastVertex] - dists[i]);
                }
                AddPoint( mid );
            }
        }
    }
}

/*
=============
Winding::Split
=============
*/
int Winding::Split( const Plane &plane, Winding **front, Winding **back )
{
    int i, j;
    int sides[MAX_VERTS_PER_POLY];
    double epsilon = fabs( NORMAL_EPSILON * plane.dist ) < EQUAL_EPSILON ? EQUAL_EPSILON : fabs( NORMAL_EPSILON * plane.dist );
    double dists[MAX_VERTS_PER_POLY];
    int sideCount[2] = { 0, 0 };
    int extraPoints;

    for ( i = 0; i < numPoints; i++ )
    {
        dists[i] = plane.normal * points[i] - plane.dist;
        if ( dists[i] > epsilon )
        {
            sides[i] = SIDE_FRONT;
            sideCount[SIDE_FRONT]++;
        }
        else if ( dists[i] < -epsilon )
        {
            sides[i] = SIDE_BACK;
            sideCount[SIDE_BACK]++;
        }
        else
        {
            sides[i] = SIDE_ON;
        }
    }
    /* A coplanar winding belongs to the side its normal faces. */
    if ( !sideCount[SIDE_FRONT] && !sideCount[SIDE_BACK] )
    {
        Vector3d normal;
        double d;
        PlaneFromPoints( points[0], points[1], points[2], normal, d );
        if ( normal * plane.normal < 0 )
        {
            *front = 0;
            *back = this;
        }
        else
        {
            *front = this;
            *back = 0;
        }
        return SIDE_ON;
    }
    if ( !sideCount[SIDE_BACK] )
    {
        *front = this;
        *back = 0;
        return SIDE_FRONT;
    }
    if ( !sideCount[SIDE_FRONT] )
    {
        *front = 0;
        *back = this;
        return SIDE_BACK;
    }
    extraPoints = 0;
    if ( sideCount[SIDE_FRONT] + sideCount[SIDE_BACK] == numPoints )
    {
        extraPoints = 2;
    }
    else
    {
        i = numPoints - 1;
        for ( j = 0; j < numPoints; i = j, j++ )
        {
            if ( sides[i] == SIDE_ON || (sides[j] != SIDE_ON && sides[j] != sides[i]) )
                extraPoints++;
        }
    }
    *front = new Winding;
    if ( !*front )
        Q3Map_Error( "Couldn't allocate front winding for split winding (%i bytes)\n", sizeof( Winding ) );
    *back = new Winding;
    if ( !*back )
        Q3Map_Error( "Couldn't allocate front winding for split winding (%i bytes)\n", sizeof( Winding ) );
    (*front)->Resize( sideCount[SIDE_FRONT] + extraPoints );
    (*back)->Resize( sideCount[SIDE_BACK] + extraPoints );
    j = numPoints - 1;
    for ( i = 0; i < numPoints; j = i, i++ )
    {
        if ( sides[j] == SIDE_ON )
        {
            (*front)->AddPoint( points[j] );
            (*back)->AddPoint( points[j] );
        }
        else
        {
            if ( sides[j] == SIDE_FRONT )
                (*front)->AddPoint( points[j] );
            else if ( sides[j] == SIDE_BACK )
                (*back)->AddPoint( points[j] );
            if ( sides[i] != SIDE_ON && sides[i] != sides[j] )
            {
                double fraction = dists[j] / (dists[j] - dists[i]);
                Vector3d mid = points[i] * fraction + points[j] * (1.0 - fraction);
                (*front)->AddPoint( mid );
                (*back)->AddPoint( mid );
            }
        }
    }
    return SIDE_CROSS;
}

/*
=============
Winding::PlaneSide
=============
*/
int Winding::PlaneSide( const Plane &plane ) const
{
    int i;
    double eps = fabs( NORMAL_EPSILON * plane.dist ) < EQUAL_EPSILON ? EQUAL_EPSILON : fabs( NORMAL_EPSILON * plane.dist );
    int counts[2] = { 0, 0 };
    for ( i = 0; i < numPoints; i++ )
    {
        double distance = plane.normal * points[i] - plane.dist;
        counts[SIDE_FRONT] += distance > eps;
        counts[SIDE_BACK] += distance < -eps;
    }
    if ( !counts[SIDE_FRONT] && !counts[SIDE_BACK] )
        return SIDE_ON;
    if ( !counts[SIDE_FRONT] )
        return SIDE_BACK;
    if ( !counts[SIDE_BACK] )
        return SIDE_FRONT;
    return SIDE_CROSS;
}

/*
=============
Winding::IsDegenerate
=============
*/
bool Winding::IsDegenerate() const
{
    int i;
    if ( numPoints < 3 )
        return true;
    i = 0;
    int j = numPoints - 1;
    int numEdges = 3;
    while ( i < numPoints && numEdges )
    {
        Vector3d edge = points[i] - points[j];
        if ( edge.LengthSquared() > MIN_WINDING_EDGE_LENGTH * MIN_WINDING_EDGE_LENGTH )
            numEdges--;
        j = i;
        i++;
    }
    if ( numEdges )
        return true;
    Vector3d area( 0, 0, 0 );
    for ( i = 2; i < numPoints; i++ )
        area += (points[i] - points[0]).Cross( points[i-1] - points[0] );
    if ( area.LengthSquared() < MIN_WINDING_CROSS_SQUARED )
        return true;
    return false;
}

/*
=============
Winding::Center
=============
*/
Vector3d Winding::Center() const
{
    Vector3d center( 0, 0, 0 );
    if ( numPoints )
    {
        for ( int i = 0; i < numPoints; i++ )
            center += points[i];
        center *= 1.0f / numPoints;
    }
    return center;
}

/*
=============
Winding::operator==

Compare cyclic point order, allowing a different starting corner.
Reversed windings remain distinct.
=============
*/
bool Winding::operator==( const Winding &other ) const
{
    int i, j;
    if ( other.numPoints != numPoints )
        return false;
    for ( j = 0; j < numPoints; j++ )
    {
        if ( points[0] == other.points[j] )
            break;
    }
    if ( j == numPoints )
        return false;
    for ( i = 1; i < numPoints; i++ )
    {
        if ( points[i] != other.points[(i + j) % numPoints] )
            return false;
    }
    return true;
}
