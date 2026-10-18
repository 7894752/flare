/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Polygon storage and surface mappings.
===========================================================================
*/
#include "flare/math_types.h"
#include "common/q_round.h"
#include <string.h>
#include <stdlib.h>
#include "flare/mathlib.h"
#include "geometry.h"
#include "options.h"
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
Vector3d::operator/=

Divide each component of this vector by scale. The divisor must be nonzero.
=============
*/
inline void Vector3d::operator/=(double scale)
{
    *this *= 1.0 / scale;
}
#include "polygon.h"

#define LIGHTMAP_SIZE 512
#define MAPPING_DETERMINANT_EPSILON 1e-15f
#define TEXTURE_COORD_EPSILON 0.1f
#define MIN_POLYGON_EDGE_LENGTH_SQUARED 0.1f
static const double portalEdgeExpansion = 0.375;
#define POLYFLAG_PORTAL POLYFLAG_0x40

#define POLYFLAG_NOSHADOW POLYFLAG_0x01
#define POLYFLAG_VISUAL_ONLY 0x08

double MaxDouble(double a, double b);
bool NearlyEqual(double a, double b);
inline bool MappingVectorsEqual(const Vector3d &a, const Vector3d &b);

void Q3Map_Error(const char *format, ...);
int numPolygons = 0;
int peakPolygons = 0;
int numPermanentPolygons = 0;
int numBspPolygons = 0;
bool polygonPoolPermanent = false;
Polygon *freePolygons = 0;
Polygon *polygons = 0;
int maxPolygons = 0;
int savedNumPolygons = 0;
int temporaryNumPolygons = 0;
int numPermanentVectors = 0;
int savedNumVectors = 0;
int temporaryNumVectors = 0;
/* Opaque caller value saved with the temporary polygon pool. */
int temporaryPolygonFlags = 0;

void MakeVectorPoolPermanent();
int GetVectorPoolUsed();
void SetVectorPoolUsed(int count);

Vector3d *AllocVectors(int count);
int ComparePolygons(const void *a, const void *b);
void GL_SetScene(Polygon *polys, int count);

typedef float vec2_t[2];
float PolygonCentroid2D(const vec2_t *points, int count, vec2_t centroid);

/*
=============
Bounds::Clear
=============
*/
inline void Bounds::Clear()
{
    mins[0] = FLT_MAX;
    mins[1] = FLT_MAX;
    mins[2] = FLT_MAX;
    maxs[0] = -FLT_MAX;
    maxs[1] = -FLT_MAX;
    maxs[2] = -FLT_MAX;
}

/*
=============
InitPolygonPool
=============
*/
void InitPolygonPool(int maxCount)
{
    polygons = new Polygon[maxCount];
    if (!polygons)
        Q3Map_Error("Allocation failed on poly pool (%i bytes) - maxpolys too big?\n", maxCount * sizeof(Polygon));
    maxPolygons = maxCount;
    numPolygons = 0;
}

/*
=============
AllocPolygon
=============
*/
Polygon *AllocPolygon()
{
    Polygon *poly;
    if (freePolygons)
    {
        poly = freePolygons;
        freePolygons = 0;
    }
    else
    {
        if (numPolygons == maxPolygons)
            Q3Map_Error("maxpolys(%i) exceeded\n", maxPolygons);
        poly = &polygons[numPolygons];
        numPolygons++;
        if (peakPolygons < numPolygons)
            peakPolygons = numPolygons;
    }
    poly->selected = false;
    return poly;
}

void Q3Map_Warning(int level, const char *format, ...);
bool PolygonVerticesEqual(const Vector3d &a, const Vector3d &b);
bool PointsAreCollinear(const Vector3d &a, const Vector3d &b, const Vector3d &c);
bool ComputePrincipalAxes(const Vector3d *points, int count, const Vector3d &origin,
                          Vector3d axes[3], double eigenvalues[3]);
Vector3d PerpendicularVector3d(const Vector3d &normal);
PolyInfo *AllocPolyInfo();
bool SolveTextureMapping(const Vector3d *points, const float (*st)[2], int i0, int i1, int i2,
                         const Vector3d &normal, PolyMappingForward *mapping);
bool SolveLightmapMapping(const Vector3d *points, const float (*st)[2], int i0, int i1, int i2,
                          const Vector3d &normal, PolyMappingInverse *mapping);
bool TextureMappingMatches(int count, const Vector3d *points, const float (*st)[2], const UVMapping &mapping);
bool TextureMappingMatchesAbsolute(int count, const Vector3d *points, const float (*st)[2], const UVMapping &mapping);
bool LightmapMappingMatches(int count, const Vector3d *points, const float (*st)[2], const PolyMappingInverse &mapping);

/*
=============
CreatePoly
=============
*/
Polygon *CreatePoly(unsigned short flags, int numPoints, const Vector3d *points,
                    const Vector3d *normals, const float (*lightmapST)[2], const float (*st)[2],
                    short shaderIndex, short lightmapIndex, bool check)
{
    int iVertMap[MAX_VERTS_PER_POLY];
    int count, i;
    int i0, i1, i2;
    Polygon *pPoly;
    Plane plane;

    /* Remove consecutive duplicates, then collinear corners. */
    iVertMap[0] = 0;
    count = 1;
    for (i = 1; i < numPoints; i++)
    {
        if (!PolygonVerticesEqual(points[i], points[iVertMap[count-1]]))
            iVertMap[count++] = i;
    }
    if (PolygonVerticesEqual(points[iVertMap[count-1]], points[iVertMap[0]]))
        count--;
    if (count < 3)
    {
        if (check)
            Q3Map_Warning(2, "skipping degenerate poly; fewer than 3 vertices after duplicates removed.\n");
        return 0;
    }
    i = 0;
    while (i < count && count > 3)
    {
        int prev = (i > 0 ? i : count) - 1;
        int next = i + 1 < count ? i + 1 : 0;
        if (PointsAreCollinear(points[iVertMap[prev]], points[iVertMap[i]], points[iVertMap[next]]))
        {
            memmove(&iVertMap[i], &iVertMap[i+1], (count-i-1)*sizeof(iVertMap[0]));
            count--;
        }
        else
            i++;
    }
    if (count < 3)
    {
        if (check)
            Q3Map_Warning(2, "skipping degenerate poly; fewer than 3 vertices after colinear points removed.\n");
        return 0;
    }
    i0 = iVertMap[0];
    i1 = iVertMap[1];
    i2 = iVertMap[2];
    pPoly = AllocPolygon();
    pPoly->numPoints = count;
    if (pPoly->numPoints != count)
        Q3Map_Error("ERROR: poly with more than 65,535 vertices\n");
    if (normals && !options.noSmoothNormals)
    {
        pPoly->points = AllocVectors(count * 2);
        pPoly->normals = pPoly->points + count;
    }
    else
    {
        pPoly->points = AllocVectors(count);
        pPoly->normals = 0;
    }
    Vector3d polyCentroid(0.0, 0.0, 0.0);
    for (i = 0; i < count; i++)
    {
        pPoly->points[i] = points[iVertMap[i]];
        polyCentroid += points[iVertMap[i]];
        if (pPoly->normals)
            pPoly->normals[i] = normals[iVertMap[i]];
    }
    polyCentroid /= count;
    Vector3d eigenvectors[3];
    double eigenvalues[3];
    /* Fit the plane to all input points and orient it to the winding. */
    ComputePrincipalAxes(points, numPoints, polyCentroid, eigenvectors, eigenvalues);
    plane.normal = eigenvectors[0];
    Vector3d crossProduct = (points[i2]-points[i1]).Cross(points[i1]-points[i0]);
    if (plane.normal * crossProduct < 0.0)
        plane.normal = -plane.normal;
    plane.dist = plane.normal * polyCentroid;
    plane.SetSignBits();
    pPoly->info = AllocPolyInfo();
    pPoly->info->plane = plane;
    pPoly->info->backPlane.normal = -plane.normal;
    pPoly->info->backPlane.dist = -plane.dist;
    pPoly->info->backPlane.SetSignBits();
    if (shaderIndex >= 0)
    {
        for (i = 0; i < count; i++)
        {
            i0 = iVertMap[i];
            i1 = iVertMap[(i+1)%count];
            i2 = iVertMap[(i+2)%count];
            if (!SolveTextureMapping(points, st, i0, i1, i2, pPoly->GetInfo()->plane.normal,
                                      pPoly->info->TextureForward()))
                continue;
            if (!TextureMappingMatchesAbsolute(numPoints, points, st, pPoly->info->worldToTexture.uv))
                continue;
            break;
        }
        if (i == count && check)
            Q3Map_Warning(2, "Singular texture mapping or non-linear texture coordinates for poly at (%lg, %lg, %lg)\n",
                          polyCentroid.x, polyCentroid.y, polyCentroid.z);
    }
    if (lightmapIndex < 0)
    {
        PolyInfo *info = pPoly->info;
        pPoly->info->worldToLightmap.uv.s = PerpendicularVector3d(info->plane.normal);
        pPoly->info->worldToLightmap.uv.t = pPoly->info->worldToLightmap.uv.s.Cross(info->plane.normal);
        pPoly->info->worldToLightmap.uv.sOffset = 0.0;
        pPoly->info->worldToLightmap.uv.tOffset = 0.0;
        pPoly->info->worldToLightmap.orientation = 1.0;
        pPoly->info->lightmapToWorld.sAxis = pPoly->info->worldToLightmap.uv.s;
        pPoly->info->lightmapToWorld.tAxis = pPoly->info->worldToLightmap.uv.t;
        pPoly->info->lightmapToWorld.origin = info->plane.dist * info->plane.normal;
        pPoly->info->lightmapToWorld.orientation = 1.0;
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            i0 = iVertMap[i];
            i1 = iVertMap[(i+1)%count];
            i2 = iVertMap[(i+2)%count];
            if (!SolveTextureMapping(points, lightmapST, i0, i1, i2, pPoly->GetInfo()->plane.normal,
                                      pPoly->info->LightmapForward()))
                continue;
            if (!TextureMappingMatches(numPoints, points, lightmapST, pPoly->info->worldToLightmap.uv))
                continue;
            if (!SolveLightmapMapping(points, lightmapST, i0, i1, i2, pPoly->GetInfo()->plane.normal,
                                       pPoly->info->LightmapInverse()))
                continue;
            if (!LightmapMappingMatches(numPoints, points, lightmapST, pPoly->info->lightmapToWorld))
                continue;
            break;
        }
        if (i == count)
        {
            if (check)
                Q3Map_Warning(1, "Couldn't derive lightmap <-> vertex equations for poly at (%lg, %lg, %lg)\n",
                              polyCentroid.x, polyCentroid.y, polyCentroid.z);
            pPoly->Free();
            return 0;
        }
    }
    pPoly->next = 0;
    pPoly->info->shaderIndex = shaderIndex;
    pPoly->info->lightmapIndex = lightmapIndex;
    if (lightmapIndex >= 0)
    {
        pPoly->info->worldToLightmap.uv.sOffset *= LIGHTMAP_SIZE;
        pPoly->info->worldToLightmap.uv.tOffset *= LIGHTMAP_SIZE;
        pPoly->info->worldToLightmap.uv.s *= LIGHTMAP_SIZE;
        pPoly->info->worldToLightmap.uv.t *= LIGHTMAP_SIZE;
        pPoly->info->lightmapToWorld.sAxis *= 1.0/LIGHTMAP_SIZE;
        pPoly->info->lightmapToWorld.tAxis *= 1.0/LIGHTMAP_SIZE;
        pPoly->info->lightmapToWorld.orientation *= 1.0/(LIGHTMAP_SIZE*LIGHTMAP_SIZE);
    }
    pPoly->info->flags = flags;
    if (pPoly->info->lightmapIndex < 0 && (pPoly->info->flags & POLYFLAG_NOSHADOW))
        pPoly->info->flags |= POLYFLAG_VISUAL_ONLY;
    pPoly->UpdateLightmapBounds();
    return pPoly;
}

/*
=============
AllocPolyInfo
=============
*/
PolyInfo *AllocPolyInfo()
{
    PolyInfo *info = new PolyInfo;
    if (!info)
        Q3Map_Error("Allocation failed on poly info (%i bytes)\n", sizeof(PolyInfo));
    memset(info, 0, sizeof(PolyInfo));
    info->refCount = 1;
    return info;
}

/*
=============
SolveTextureMapping

Project onto the dominant plane and solve the two affine texture equations.
=============
*/
bool SolveTextureMapping(const Vector3d *points, const float (*st)[2], int i0, int i1, int i2,
                         const Vector3d &normal, PolyMappingForward *mapping)
{
    int major = 0;
    if (fabs(normal.x) < fabs(normal.y))
        major = 1;
    if (fabs(normal[major]) < fabs(normal.z))
        major = 2;
    /* Drop the dominant axis to avoid a nearly edge-on projection. */
    int i = ~major & 1;
    int j = ~major & 2;
    double coord[3][2];
    coord[0][0] = points[i0][i];
    coord[0][1] = points[i0][j];
    coord[1][0] = points[i1][i];
    coord[1][1] = points[i1][j];
    coord[2][0] = points[i2][i];
    coord[2][1] = points[i2][j];
    double determinant = (coord[1][1]-coord[2][1])*coord[0][0]
                       + (coord[2][0]-coord[1][0])*coord[0][1]
                       + coord[1][0]*coord[2][1] - coord[2][0]*coord[1][1];
    if (fabs(determinant) <= MAPPING_DETERMINANT_EPSILON)
        return false;
    double inverse = 1.0 / determinant;
    mapping->uv.s[i] = (st[i0][0]*(coord[1][1]-coord[2][1])
                          + st[i1][0]*(coord[2][1]-coord[0][1])
                          + st[i2][0]*(coord[0][1]-coord[1][1])) * inverse;
    mapping->uv.t[i] = (st[i0][1]*(coord[1][1]-coord[2][1])
                          + st[i1][1]*(coord[2][1]-coord[0][1])
                          + st[i2][1]*(coord[0][1]-coord[1][1])) * inverse;
    mapping->uv.s[j] = (st[i0][0]*(coord[2][0]-coord[1][0])
                          + st[i1][0]*(coord[0][0]-coord[2][0])
                          + st[i2][0]*(coord[1][0]-coord[0][0])) * inverse;
    mapping->uv.t[j] = (st[i0][1]*(coord[2][0]-coord[1][0])
                          + st[i1][1]*(coord[0][0]-coord[2][0])
                          + st[i2][1]*(coord[1][0]-coord[0][0])) * inverse;
    mapping->uv.s[major] = 0.0;
    mapping->uv.t[major] = 0.0;
    mapping->uv.sOffset = (st[i0][0]*(coord[1][0]*coord[2][1]-coord[2][0]*coord[1][1])
                          + st[i1][0]*(coord[2][0]*coord[0][1]-coord[0][0]*coord[2][1])
                          + st[i2][0]*(coord[0][0]*coord[1][1]-coord[1][0]*coord[0][1])) * inverse;
    mapping->uv.tOffset = (st[i0][1]*(coord[1][0]*coord[2][1]-coord[2][0]*coord[1][1])
                          + st[i1][1]*(coord[2][0]*coord[0][1]-coord[0][0]*coord[2][1])
                          + st[i2][1]*(coord[0][0]*coord[1][1]-coord[1][0]*coord[0][1])) * inverse;
    mapping->orientation = mapping->uv.s.Cross(mapping->uv.t) * normal;
    return true;
}

PolyInfo *RetainPolyInfo(PolyInfo *info);

/*
=============
TextureMappingMatches
=============
*/
bool TextureMappingMatches(int numPoints, const Vector3d *points,
                           const float (*st)[2], const UVMapping &mapping)
{
    for (int i = 0; i < numPoints; i++)
    {
        if (!NearlyEqual(mapping.s * points[i] + mapping.sOffset, st[i][0]))
            return false;
        if (!NearlyEqual(mapping.t * points[i] + mapping.tOffset, st[i][1]))
            return false;
    }
    return true;
}

/*
=============
AbsoluteEqual
=============
*/
inline bool AbsoluteEqual(double a, double b)
{
    return fabs(a-b) <= TEXTURE_COORD_EPSILON;
}

/*
=============
TextureMappingMatchesAbsolute
=============
*/
bool TextureMappingMatchesAbsolute(int numPoints, const Vector3d *points,
                                   const float (*st)[2], const UVMapping &mapping)
{
    for (int i = 0; i < numPoints; i++)
    {
        if (!AbsoluteEqual(mapping.s * points[i] + mapping.sOffset, st[i][0]))
            return false;
        if (!AbsoluteEqual(mapping.t * points[i] + mapping.tOffset, st[i][1]))
            return false;
    }
    return true;
}

/*
=============
SolveLightmapMapping

Solve the inverse affine map from texture coordinates to the polygon plane.
=============
*/
bool SolveLightmapMapping(const Vector3d *points, const float (*st)[2], int i0, int i1, int i2,
                          const Vector3d &normal, PolyMappingInverse *mapping)
{
    double determinant = (st[i1][1]-st[i2][1])*st[i0][0] + (st[i2][0]-st[i1][0])*st[i0][1]
                         + st[i1][0]*st[i2][1] - st[i2][0]*st[i1][1];
    if (fabs(determinant) <= MAPPING_DETERMINANT_EPSILON)
        return false;
    double inverse = 1.0 / determinant;
    mapping->sAxis.x = (points[i0].x*(st[i1][1]-st[i2][1])
                      + points[i1].x*(st[i2][1]-st[i0][1])
                      + points[i2].x*(st[i0][1]-st[i1][1])) * inverse;
    mapping->sAxis.y = (points[i0].y*(st[i1][1]-st[i2][1])
                      + points[i1].y*(st[i2][1]-st[i0][1])
                      + points[i2].y*(st[i0][1]-st[i1][1])) * inverse;
    mapping->sAxis.z = (points[i0].z*(st[i1][1]-st[i2][1])
                      + points[i1].z*(st[i2][1]-st[i0][1])
                      + points[i2].z*(st[i0][1]-st[i1][1])) * inverse;
    mapping->tAxis.x = (points[i0].x*(st[i2][0]-st[i1][0])
                      + points[i1].x*(st[i0][0]-st[i2][0])
                      + points[i2].x*(st[i1][0]-st[i0][0])) * inverse;
    mapping->tAxis.y = (points[i0].y*(st[i2][0]-st[i1][0])
                      + points[i1].y*(st[i0][0]-st[i2][0])
                      + points[i2].y*(st[i1][0]-st[i0][0])) * inverse;
    mapping->tAxis.z = (points[i0].z*(st[i2][0]-st[i1][0])
                      + points[i1].z*(st[i0][0]-st[i2][0])
                      + points[i2].z*(st[i1][0]-st[i0][0])) * inverse;
    mapping->origin.x = (points[i0].x*(st[i1][0]*st[i2][1]-st[i2][0]*st[i1][1])
                          + points[i1].x*(st[i2][0]*st[i0][1]-st[i0][0]*st[i2][1])
                          + points[i2].x*(st[i0][0]*st[i1][1]-st[i1][0]*st[i0][1])) * inverse;
    mapping->origin.y = (points[i0].y*(st[i1][0]*st[i2][1]-st[i2][0]*st[i1][1])
                          + points[i1].y*(st[i2][0]*st[i0][1]-st[i0][0]*st[i2][1])
                          + points[i2].y*(st[i0][0]*st[i1][1]-st[i1][0]*st[i0][1])) * inverse;
    mapping->origin.z = (points[i0].z*(st[i1][0]*st[i2][1]-st[i2][0]*st[i1][1])
                          + points[i1].z*(st[i2][0]*st[i0][1]-st[i0][0]*st[i2][1])
                          + points[i2].z*(st[i0][0]*st[i1][1]-st[i1][0]*st[i0][1])) * inverse;
    mapping->orientation = mapping->sAxis.Cross(mapping->tAxis) * normal;
    return true;
}

/*
=============
LightmapMappingMatches
=============
*/
bool LightmapMappingMatches(int numPoints, const Vector3d *points,
                            const float (*st)[2], const PolyMappingInverse &mapping)
{
    for (int i = 0; i < numPoints; i++)
    {
        if (!MappingVectorsEqual(st[i][0] * mapping.sAxis + st[i][1] * mapping.tAxis + mapping.origin, points[i]))
            return false;
    }
    return true;
}

/*
=============
MappingVectorsEqual
=============
*/
inline bool MappingVectorsEqual(const Vector3d &a, const Vector3d &b)
{
    Vector3d delta = a-b;
    return delta*delta <= MaxDouble(EQUAL_EPSILON,
                                   (a*a + b*b) * (1.5 * EQUAL_EPSILON * EQUAL_EPSILON));
}

/*
=============
PolygonVerticesEqual
=============
*/
inline bool PolygonVerticesEqual(const Vector3d &a, const Vector3d &b)
{
    Vector3d delta = a-b;
    return delta*delta < 3.0 * EQUAL_EPSILON * EQUAL_EPSILON;
}

/*
=============
AllocSplitPolygon
=============
*/
Polygon *AllocSplitPolygon(const Polygon *source, int numPoints)
{
    Polygon *poly = AllocPolygon();
    poly->numPoints = numPoints;
    if (poly->numPoints != numPoints)
        Q3Map_Error("ERROR: poly with more than 65,535 vertices\n");
    if (source->normals)
    {
        poly->points = AllocVectors(numPoints * 2);
        poly->normals = poly->points + numPoints;
    }
    else
    {
        poly->points = AllocVectors(numPoints);
        poly->normals = 0;
    }
    poly->info = RetainPolyInfo(source->info);
    poly->next = 0;
    return poly;
}

/*
=============
RetainPolyInfo
=============
*/
PolyInfo *RetainPolyInfo(PolyInfo *info)
{
    info->refCount++;
    return info;
}

PolyInfo *CopyPolyInfo(const PolyInfo *info);

void ReleasePolyInfo(PolyInfo *info);

/*
=============
OffsetPolygon
=============
*/
Polygon *OffsetPolygon(const Polygon *source, double distance)
{
    Polygon *poly = AllocPolygon();
    poly->numPoints = source->numPoints;
    if (source->normals)
    {
        poly->points = AllocVectors(source->numPoints * 2);
        poly->normals = poly->points + source->numPoints;
    }
    else
    {
        poly->points = AllocVectors(source->numPoints);
        poly->normals = 0;
    }
    Vector3d delta = distance * source->info->plane.normal;
    poly->info = CopyPolyInfo(source->info);
    poly->info->worldToLightmap.uv.sOffset -= poly->info->worldToLightmap.uv.s * delta;
    poly->info->worldToLightmap.uv.tOffset -= poly->info->worldToLightmap.uv.t * delta;
    poly->info->worldToTexture.uv.sOffset -= poly->info->worldToTexture.uv.s * delta;
    poly->info->worldToTexture.uv.tOffset -= poly->info->worldToTexture.uv.t * delta;
    poly->info->lightmapToWorld.origin += delta;
    poly->info->plane.dist += distance;
    for (int i = 0; i < source->numPoints; i++)
    {
        poly->points[i] = source->points[i] + delta;
        if (source->normals)
            poly->normals[i] = source->normals[i];
    }
    poly->next = 0;
    poly->UpdateLightmapBounds();
    return poly;
}

/*
=============
CopyPolyInfo
=============
*/
PolyInfo *CopyPolyInfo(const PolyInfo *info)
{
    PolyInfo *copy = AllocPolyInfo();
    if (!copy)
        Q3Map_Error("Allocation failed on poly info (%i bytes)\n", sizeof(PolyInfo));
    memcpy(copy, info, sizeof(PolyInfo));
    copy->refCount = 1;
    return copy;
}

/*
=============
CopyPolygon
=============
*/
Polygon *CopyPolygon(const Polygon *source)
{
    Polygon *poly = AllocPolygon();
    poly->numPoints = source->numPoints;
    if (source->normals)
    {
        poly->points = AllocVectors(source->numPoints * 2);
        poly->normals = poly->points + source->numPoints;
        memcpy(poly->points, source->points, source->numPoints * sizeof(Vector3d));
        memcpy(poly->normals, source->normals, source->numPoints * sizeof(Vector3d));
    }
    else
    {
        poly->points = AllocVectors(source->numPoints);
        poly->normals = 0;
        memcpy(poly->points, source->points, source->numPoints * sizeof(Vector3d));
    }
    poly->info = RetainPolyInfo(source->info);
    poly->next = 0;
    poly->UpdateLightmapBounds();
    return poly;
}

/*
=============
Polygon::UpdateLightmapBounds
=============
*/
void Polygon::UpdateLightmapBounds()
{
    int i;
    vec2_t st[MAX_VERTS_PER_POLY], mins, max, centroid;
    WorldToLightmap(points, st[0]);
    mins[0] = st[0][0];
    mins[1] = st[0][1];
    max[0] = st[0][0];
    max[1] = st[0][1];
    for (i = 1; i < numPoints; i++)
    {
        WorldToLightmap(&points[i], st[i]);
        if (mins[0] > st[i][0])
            mins[0] = st[i][0];
        else if (max[0] < st[i][0])
            max[0] = st[i][0];
        if (mins[1] > st[i][1])
            mins[1] = st[i][1];
        else if (max[1] < st[i][1])
            max[1] = st[i][1];
    }
    /* Enclose the polygon in texel centers, clipped to the lightmap. */
    lightmapX = FloorToInt(mins[0] - 0.5f);
    lightmapY = FloorToInt(mins[1] - 0.5f);
    lightmapWidth = CeilToInt(max[0] - 0.5f) - lightmapX + 1;
    lightmapHeight = CeilToInt(max[1] - 0.5f) - lightmapY + 1;
    if (lightmapX + lightmapWidth > LIGHTMAP_SIZE)
        lightmapWidth = LIGHTMAP_SIZE - lightmapX;
    if (lightmapY + lightmapHeight > LIGHTMAP_SIZE)
        lightmapHeight = LIGHTMAP_SIZE - lightmapY;
    PolygonCentroid2D(st, numPoints, centroid);
    LightmapToWorld(centroid[0], centroid[1], &center);
    bounds.Clear();
    for (i = 0; i < numPoints; i++)
        bounds.AddPoint(points[i]);
    /* Round the tolerance to the precision of the bounds before expansion. */
    bounds.mins[0] -= (float)EQUAL_EPSILON;
    bounds.mins[1] -= (float)EQUAL_EPSILON;
    bounds.mins[2] -= (float)EQUAL_EPSILON;
    bounds.maxs[0] += (float)EQUAL_EPSILON;
    bounds.maxs[1] += (float)EQUAL_EPSILON;
    bounds.maxs[2] += (float)EQUAL_EPSILON;
}

/*
=============
Polygon::Free
=============
*/
void Polygon::Free()
{
    ReleasePolyInfo(info);
    if (this == &polygons[numPolygons] - 1)
    {
        numPolygons--;
        if (normals)
            normals->Free(numPoints);
        points->Free(numPoints);
    }
    else
    {
        freePolygons = this;
        if (normals)
            points->Free(numPoints * 2);
        else
            points->Free(numPoints);
    }
}

/*
=============
ReleasePolyInfo
=============
*/
void ReleasePolyInfo(PolyInfo *info)
{
    info->refCount--;
    if (!info->refCount)
        delete info;
}

/*
=============
MakePolygonPoolPermanent
=============
*/
void MakePolygonPoolPermanent()
{
    freePolygons = 0;
    polygonPoolPermanent = true;
    MakeVectorPoolPermanent();
}

/*
=============
SavePolygonPool
=============
*/
void SavePolygonPool()
{
    savedNumPolygons = numPolygons;
    savedNumVectors = GetVectorPoolUsed();
}

/*
=============
RestorePolygonPool
=============
*/
void RestorePolygonPool()
{
    numPolygons = savedNumPolygons;
    SetVectorPoolUsed(savedNumVectors);
}

/*
=============
SaveTemporaryPolygons
=============
*/
void SaveTemporaryPolygons(int savedValue)
{
    temporaryNumPolygons = numPolygons;
    temporaryPolygonFlags = savedValue;
    temporaryNumVectors = GetVectorPoolUsed();
}

/*
=============
RestoreTemporaryPolygons

Keep the last clipping result in the first temporary slot and reclaim the rest.
=============
*/
Polygon *RestoreTemporaryPolygons(Polygon *poly)
{
    int i;
    if (!poly)
    {
        numPolygons = temporaryNumPolygons;
        SetVectorPoolUsed(temporaryNumVectors);
        return 0;
    }
    if (numPolygons <= temporaryNumPolygons + 1)
        return poly;

    Polygon *last = &polygons[numPolygons - 1];
    Polygon *firstPoly = &polygons[temporaryNumPolygons];
    firstPoly->numPoints = last->numPoints;
    int numVectors = firstPoly->numPoints;
    if (firstPoly->normals)
    {
        firstPoly->normals = firstPoly->points + firstPoly->numPoints;
        numVectors += firstPoly->numPoints;
    }
    for (i = 0; i < last->numPoints; i++)
    {
        firstPoly->points[i] = last->points[i];
        if (firstPoly->normals)
            firstPoly->normals[i] = last->normals[i];
    }
    firstPoly->lightmapX = last->lightmapX;
    firstPoly->lightmapY = last->lightmapY;
    firstPoly->lightmapWidth = last->lightmapWidth;
    firstPoly->lightmapHeight = last->lightmapHeight;
    numPolygons = temporaryNumPolygons + 1;
    SetVectorPoolUsed(temporaryNumVectors + numVectors);
    return &polygons[temporaryNumPolygons];
}

/*
=============
CompactLastPolygon

Replace the preceding polygon with the last result and release its vector tail.
=============
*/
void CompactLastPolygon()
{
    int iVert;
    Polygon *last = &polygons[numPolygons - 1];
    Polygon *prevPoly = &polygons[numPolygons - 2];
    int numPoints = last->numPoints;
    int oldNumVerts = prevPoly->numPoints;
    prevPoly->numPoints = last->numPoints;
    if (prevPoly->normals)
    {
        numPoints += numPoints;
        oldNumVerts += oldNumVerts;
        prevPoly->normals = prevPoly->points + prevPoly->numPoints;
    }
    for (iVert = 0; iVert < last->numPoints; iVert++)
    {
        prevPoly->points[iVert] = last->points[iVert];
        if (prevPoly->normals)
            prevPoly->normals[iVert] = last->normals[iVert];
    }
    prevPoly->lightmapX = last->lightmapX;
    prevPoly->lightmapY = last->lightmapY;
    prevPoly->lightmapWidth = last->lightmapWidth;
    prevPoly->lightmapHeight = last->lightmapHeight;
    numPolygons--;
    (prevPoly->points + numPoints)->Free(oldNumVerts);
}

/*
=============
FinalizePolygons
=============
*/
void FinalizePolygons()
{
    qsort(polygons, numPolygons, sizeof(Polygon), ComparePolygons);
    numPermanentPolygons = numPolygons;
    numPermanentVectors = GetVectorPoolUsed();
    for (int i = 0; i < numPermanentPolygons; i++)
        polygons[i].info->owner = &polygons[i];
    GL_SetScene(polygons, numPolygons);
}

/* Polygon ordering uses the decoded shader image's alpha mode. */
#include "flare/shaders.h"


/*
=============
Vector3d::Normalized
=============
*/
inline Vector3d Vector3d::Normalized() const
{
    double length = sqrt(*this * *this);
    if (length != 0.0)
        return (1.0 / length) * *this;
    return Vector3d(0.0, 0.0, 0.0);
}

/*
=============
Vector3d::NormalizeLength
=============
*/
inline double Vector3d::NormalizeLength()
{
    double length = sqrt(*this * *this);
    if (length != 0.0)
    {
        double inverse = 1.0 / length;
        x *= inverse;
        y *= inverse;
        z *= inverse;
    }
    return length;
}

/*
=============
ComparePolygons
=============
*/
int ComparePolygons(const void *a, const void *b)
{
    const Polygon *polygon1 = (const Polygon *)a;
    const Polygon *polygon2 = (const Polygon *)b;
    int index1 = polygon1->info->shaderIndex;
    int index2 = polygon2->info->shaderIndex;
    if ((polygon1->info->flags ^ polygon2->info->flags) & POLYFLAG_PORTAL)
        return (polygon1->info->flags & POLYFLAG_PORTAL) ? 1 : -1;
    if (index1 >= 0 && index2 >= 0)
    {
        int difference = shaders[index1].image.alphaType - shaders[index2].image.alphaType;
        if (difference)
            return difference;
    }
    else
    {
        if (index1 >= 0)
            return 1;
        if (index2 >= 0)
            return -1;
    }
    int lightmapDifference = polygon1->info->lightmapIndex - polygon2->info->lightmapIndex;
    if (lightmapDifference)
        return lightmapDifference;
    return index1 - index2;
}

/*
=============
Polygon::InterpolateNormal

Cache corner data for repeated samples on the same polygon.
=============
*/
void Polygon::InterpolateNormal(const Vector3d *point, Vector3d *normal) const
{
    /* Split fragments share the original polygon's corner data. */
    if (this != info->owner)
    {
        info->owner->InterpolateNormal(point, normal);
        return;
    }
    if (!normals)
    {
        *normal = info->plane.normal;
        return;
    }

    static double cornerAreas[MAX_VERTS_PER_POLY];
    static const Polygon *lastPolygon;
    static Vector3d edgeNormals[MAX_VERTS_PER_POLY];
    double triangleAreas[MAX_VERTS_PER_POLY];
    int i, prev, prev2;
    double blendWeights[MAX_VERTS_PER_POLY];
    Vector3d edge;

    if (this != lastPolygon)
    {
        lastPolygon = this;
        for (prev2 = numPoints - 2, prev = numPoints - 1, i = 0;
             i < numPoints; prev2 = prev, prev = i, i++)
        {
            cornerAreas[prev] = (points[prev] - points[i]).Cross(points[prev2] - points[i]) * info->plane.normal;
            edge = points[i] - points[prev];
            /* Smooth across the edge without adding a component along it. */
            edgeNormals[prev] = normals[prev] + normals[i];
            edgeNormals[prev] -= ((edge * edgeNormals[prev]) / (edge * edge)) * edge;
            edgeNormals[prev].NormalizeLength();
            triangleAreas[prev] = (points[prev] - *point).Cross(points[prev2] - *point) * info->plane.normal;
        }
    }
    else
    {
        for (prev2 = numPoints - 2, prev = numPoints - 1, i = 0;
             i < numPoints; prev2 = prev, prev = i, i++)
        {
            Vector3d toPrevious = points[prev] - *point;
            Vector3d toBeforePrevious = points[prev2] - *point;
            triangleAreas[prev] = toPrevious.Cross(toBeforePrevious) * info->plane.normal;
            /* On an edge, interpolate its endpoint normals directly. */
            if (triangleAreas[prev] * triangleAreas[prev] <= 0.001f * 0.001f)
            {
                double lengthA = toPrevious.Length();
                double lengthB = toBeforePrevious.Length();
                *normal = (lengthB * normals[prev] + lengthA * normals[prev2]) / (lengthA + lengthB);
                normal->NormalizeLength();
                return;
            }
        }
    }
    for (prev = numPoints - 1, i = 0; i < numPoints; prev = i, i++)
        blendWeights[prev] = cornerAreas[prev] / (triangleAreas[prev] * triangleAreas[i]);
    normal->Clear();
    for (prev = numPoints - 1, i = 0; i < numPoints; prev = i, i++)
    {
        *normal += normals[prev] * (blendWeights[prev] * blendWeights[prev]);
        *normal += edgeNormals[prev] * (blendWeights[prev] * blendWeights[i]);
    }
    normal->NormalizeLength();
}

/*
=============
Vector3d::Length
=============
*/
double Vector3d::Length() const
{
    return sqrt(z * z + (y * y + x * x));
}

/*
=============
Polygon::IntersectsWinding
=============
*/
bool Polygon::IntersectsWinding(const Winding &winding, const Bounds &windingBounds) const
{
    Plane plane;
    if (bounds.mins[0] > windingBounds.maxs[0] || bounds.maxs[0] < windingBounds.mins[0] ||
        bounds.mins[1] > windingBounds.maxs[1] || bounds.maxs[1] < windingBounds.mins[1] ||
        bounds.mins[2] > windingBounds.maxs[2] || bounds.maxs[2] < windingBounds.mins[2])
        return false;

    Winding clipped;
    clipped.Copy(winding);
    const Plane *basePlane = &info->plane;
    int previous = numPoints - 1;
    for (int i = 0; i < numPoints; previous = i, i++)
    {
        Vector3d edge = points[i] - points[previous];
        if (edge * edge < MIN_POLYGON_EDGE_LENGTH_SQUARED)
            continue;
        plane.normal = edge.Cross(basePlane->normal).Normalized();
        plane.dist = plane.normal * points[i];
        plane.dist += EQUAL_EPSILON > fabs(plane.dist) * NORMAL_EPSILON ? EQUAL_EPSILON : fabs(plane.dist) * NORMAL_EPSILON;
        plane.dist += portalEdgeExpansion;
        clipped.ClipFront(plane);
        if (clipped.numPoints < 3)
            return false;
    }
    if (clipped.IsDegenerate())
        return false;
    return true;
}

/*
=============
Polygon::ContainsWinding
=============
*/
bool Polygon::ContainsWinding(const Winding &winding, const Bounds &windingBounds) const
{
    Plane plane;
    if (bounds.mins[0] > windingBounds.maxs[0] || bounds.maxs[0] < windingBounds.mins[0] ||
        bounds.mins[1] > windingBounds.maxs[1] || bounds.maxs[1] < windingBounds.mins[1] ||
        bounds.mins[2] > windingBounds.maxs[2] || bounds.maxs[2] < windingBounds.mins[2])
        return false;

    const Plane *basePlane = &info->plane;
    int previous = numPoints - 1;
    for (int i = 0; i < numPoints; previous = i, i++)
    {
        Vector3d edge = points[i] - points[previous];
        if (edge * edge < MIN_POLYGON_EDGE_LENGTH_SQUARED)
            continue;
        plane.normal = basePlane->normal.Cross(edge).Normalized();
        plane.dist = plane.normal * points[i];
        plane.dist += EQUAL_EPSILON > fabs(plane.dist) * NORMAL_EPSILON ? EQUAL_EPSILON : fabs(plane.dist) * NORMAL_EPSILON;
        plane.dist += portalEdgeExpansion;
        int planeSide = winding.PlaneSide(plane);
        if (planeSide == 0 || planeSide == 3)
            return false;
    }
    return true;
}

/*
=============
Polygon::TrimPortalWinding

Subtract the polygon's interior and return the surviving winding fragments.
=============
*/
int Polygon::TrimPortalWinding(Winding *winding, const Bounds &windingBounds, Winding **fragments) const
{
    *fragments = 0;
    if (bounds.mins[0] > windingBounds.maxs[0] || bounds.maxs[0] < windingBounds.mins[0] ||
        bounds.mins[1] > windingBounds.maxs[1] || bounds.maxs[1] < windingBounds.mins[1] ||
        bounds.mins[2] > windingBounds.maxs[2] || bounds.maxs[2] < windingBounds.mins[2])
        return 0;

    Plane planes[MAX_VERTS_PER_POLY];
    const Plane *basePlane = &info->plane;
    int classifications[MAX_VERTS_PER_POLY];
    int numCrossings = 0;
    int j, prev;
    for (prev = numPoints - 1, j = 0; j < numPoints; prev = j, j++)
    {
        Vector3d edge = points[j] - points[prev];
        if (edge * edge < MIN_POLYGON_EDGE_LENGTH_SQUARED)
        {
            classifications[j] = -1;
            continue;
        }
        planes[j].normal = basePlane->normal.Cross(edge).Normalized();
        planes[j].dist = planes[j].normal * points[j];
        planes[j].dist = portalEdgeExpansion + planes[j].dist;
        classifications[j] = winding->PlaneSide(planes[j]);
        if (classifications[j] == 0)
            return 0;
        if (classifications[j] == 3)
            numCrossings++;
    }
    if (!numCrossings)
    {
        *fragments = winding;
        return 1;
    }

    Winding *originalWinding = winding;
    Winding *frontWinding, *back;
    *fragments = new Winding[numCrossings];
    if (!fragments)
        Q3Map_Error("TrimPortalWinding: couldn't allocate winding list (%i bytes)\n", numCrossings * sizeof(Winding));
    prev = 0;
    for (j = 0; j < numPoints; j++)
    {
        if (classifications[j] == 3)
        {
            winding->Split(planes[j], &frontWinding, &back);
            if (frontWinding)
            {
                if (!frontWinding->IsDegenerate())
                    (*fragments)[prev++].Copy(*frontWinding);
                if (frontWinding != winding)
                    delete frontWinding;
            }
            if (back != winding)
            {
                if (!back)
                    break;
                if (winding != originalWinding)
                    delete winding;
                winding = back;
                if (winding->IsDegenerate())
                    break;
            }
        }
    }
    if (prev == 1 && winding == originalWinding)
    {
        delete[] *fragments;
        *fragments = winding;
    }
    else
    {
        delete winding;
    }
    return prev;
}

/*
=============
Polygon::PointOnPolygon
=============
*/
bool Polygon::PointOnPolygon(const Vector3d &point) const
{
    const Plane *basePlane = &info->plane;
    double distance = basePlane->normal * point;
    if (!NearlyEqual(distance, basePlane->dist))
        return false;
    return ContainsPoint(point);
}

/*
=============
Polygon::ContainsPoint
=============
*/
bool Polygon::ContainsPoint(const Vector3d &point) const
{
    const Plane *basePlane = &info->plane;
    double epsilon = EQUAL_EPSILON > fabs(basePlane->dist) * NORMAL_EPSILON ?
                     EQUAL_EPSILON : fabs(basePlane->dist) * NORMAL_EPSILON;
    if (!bounds.Contains(point))
        return false;
    int previous = numPoints - 1;
    for (int i = 0; i < numPoints; previous = i, i++)
    {
        Vector3d edge = points[i] - points[previous];
        if (edge * edge < MIN_POLYGON_EDGE_LENGTH_SQUARED)
            continue;
        Vector3d edgeNormal = basePlane->normal.Cross(edge).Normalized();
        double distance = edgeNormal * points[i] + epsilon;
        if (edgeNormal * point > distance)
            return false;
    }
    return true;
}

/*
=============
Bounds::Contains
=============
*/
bool Bounds::Contains(const Vector3d &point) const
{
    if (mins[0] > point.x || maxs[0] < point.x)
        return false;
    if (mins[1] > point.y || maxs[1] < point.y)
        return false;
    if (mins[2] > point.z || maxs[2] < point.z)
        return false;
    return true;
}

/*
=============
Polygon::IntersectSegment
=============
*/
bool Polygon::IntersectSegment(const Vector3d &start, const Vector3d &end) const
{
    const Plane *polyPlane = &info->plane;
    double d1 = polyPlane->normal * start - polyPlane->dist;
    double d2 = polyPlane->normal * end - polyPlane->dist;
    if (d1 != 0 && d2 != 0 && (d1 < 0 ? d2 < 0 : d2 > 0))
        return false;
    Vector3d midpoint;
    if (d1 == 0)
        midpoint = start;
    else if (d2 == 0)
        midpoint = end;
    else
        midpoint = (start * d2 - end * d1) / (d2 - d1);
    return ContainsPoint(midpoint);
}
