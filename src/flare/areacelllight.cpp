/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Emissive surface cells and polygon form-factor lighting.
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
#include "flare/polygon.h"

/*
==================
AreaCellLight::Export

The owning area light supplies the BSP record.
==================
*/
void AreaCellLight::Export(bspLight_t *out)
{
}

/*
==================
AreaCellLight::GetSource

The light that owns the BSP record.
==================
*/
Light *AreaCellLight::GetSource()
{
    return source;
}

/*
==================
AreaCellLight::NumFaces

Number of passes (faces) the light is evaluated in.
==================
*/
int AreaCellLight::NumFaces()
{
    return 5;
}

/*
==================
AreaCellLight::SetPlane

Stores the plane of the cell.
==================
*/
void AreaCellLight::SetPlane(const Plane &cellPlane)
{
    plane = cellPlane;
}

/*
==================
AreaCellLight::SetRadius

Stores the radius of the cell light.
==================
*/
void AreaCellLight::SetRadius(float cellRadius)
{
    radius = cellRadius;
}

/*
==================
Bounds::Zero

Clears the bounds to zero.
==================
*/
void Bounds::Zero()
{
    mins[0] = 0.0f;
    mins[1] = 0.0f;
    mins[2] = 0.0f;
    maxs[0] = 0.0f;
    maxs[1] = 0.0f;
    maxs[2] = 0.0f;
}

/*
==================
AreaCellLight::SelectFace

Prepares the frame and the clip planes for one pass of the light.
==================
*/
void AreaCellLight::SelectFace(int face)
{
    static Vector3d directions[8];
    static const int faces[5][4] = {
        {1, 0, 2, 3}, {0, 1, 5, 4}, {2, 0, 4, 6}, {3, 2, 6, 7}, {1, 3, 7, 5}
    };
    const Vector3d &lightOrigin = origin;
    int i;

    if (face == 0)
    {
        radiusSq = radius * radius;
        tangent = PerpendicularVector3d(plane.normal);
        const Vector3d tangentAxis = tangent;
        bitangent = Vector3d(plane.normal.y * tangentAxis.z - plane.normal.z * tangentAxis.y,
                            plane.normal.z * tangentAxis.x - plane.normal.x * tangentAxis.z,
                            plane.normal.x * tangentAxis.y - plane.normal.y * tangentAxis.x);
        /* Four raised corners and four corners on the emitting plane. */
        for (i = 0; i < 8; i++)
        {
            if (i < 4)
                directions[i] = plane.normal;
            else
            {
                Vector3d &direction = directions[i];
                direction.x = 0.0;
                direction.y = 0.0;
                direction.z = 0.0;
            }
            if (i & 2)
                directions[i] -= tangent;
            else
                directions[i] += tangent;
            if (i & 1)
                directions[i] -= bitangent;
            else
                directions[i] += bitangent;
        }
    }
    for (i = 0; i < 4; i++)
    {
        int first = faces[face][i];
        int second = faces[face][(i + 1) & 3];
        Vector3d normal = directions[second].Cross(directions[first]);
        clip[i].normal = normal.Normalized();
        double distance = lightOrigin * clip[i].normal;
        clip[i].dist = distance;
        clip[i].SetSignBits();
    }
    switch (face)
    {
    case 0:
        faceRight = tangent;
        faceUp = bitangent;
        faceForward = plane.normal;
        break;
    case 1:
        faceRight = bitangent;
        faceUp = plane.normal;
        faceForward = tangent;
        break;
    case 2:
        faceRight = plane.normal;
        faceUp = tangent;
        faceForward = bitangent;
        break;
    case 3:
        faceRight = -plane.normal;
        faceUp = -bitangent;
        faceForward = -tangent;
        break;
    case 4:
        faceRight = -tangent;
        faceUp = -plane.normal;
        faceForward = -bitangent;
        break;
    }
}

/*
==================
AreaCellLight::Illuminate

Color the light delivers to a lightmap footprint.
==================
*/
Color3f AreaCellLight::Illuminate(const Polygon *poly, const float (*points)[2], int numPoints,
                                  float weight, const float *st)
{
    Vector3d point, normal;
    poly->LightmapToWorld(st[0], st[1], &point);
    poly->InterpolateNormal(&point, &normal);
    const Vector3d *vertices = cell.points;
    unsigned short count = cell.numPoints;
    return color * PointToPolygonFormFactor(point, normal, vertices, count);
}

/*
==================
AreaCellLight::DistanceAttenuation

Form factor evaluated with the receiver normal facing the emitting plane.
==================
*/
float AreaCellLight::DistanceAttenuation(const Vector3d &point)
{
    Vector3d normal = -plane.normal;
    const Vector3d *vertices = cell.points;
    unsigned short count = cell.numPoints;
    return PointToPolygonFormFactor(point, normal, vertices, count);
}

/*
==================
AreaCellLight::AngularAttenuation

Form factor for the receiver's position and surface normal.
==================
*/
float AreaCellLight::AngularAttenuation(const Vector3d &point, const Vector3d &normal)
{
    const Vector3d *vertices = cell.points;
    unsigned short count = cell.numPoints;
    return PointToPolygonFormFactor(point, normal, vertices, count);
}

/*
==================
AreaCellLight::Draw

Draws the light in the preview.
==================
*/
void AreaCellLight::Draw(bool selected)
{
    float size = selected ? 8.0f : 4.0f;
    Vector3d drawRadius(size, size, size);

    GL_Color3fv(&color.r);
    GL_DrawPyramid(&origin, &plane.normal, &drawRadius);
    if (selected)
    {
        float white[3] = { 1.0f, 1.0f, 1.0f };

        GL_Color3fv(white);
        GL_DrawPyramidOutline(&origin, &plane.normal, &drawRadius);
    }
}
