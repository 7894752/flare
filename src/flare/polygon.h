/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Polygon: convex polygon stored in the polygon pool.
===========================================================================
*/
#ifndef FLARE_POLYGON_H
#define FLARE_POLYGON_H

#include "flare/geometry_types.h"

/* Convex polygon stored in the polygon pool (polygon.cpp). */
struct Polygon
{
    unsigned short numPoints;
    bool selected;                  /* selected in the preview */
    Vector3d *points;               /* numPoints vectors from the vector pool */
    Vector3d *normals;              /* optional vertex normals, following the point array */
    unsigned int reserved0C;        /* reserved */
    Vector3d center;                /* lightmap-area centroid in world space */
    PolyInfo *info;
    Bounds bounds;                  /* bounds of the polygon points */
    short lightmapX;                /* lightmap rectangle, clamped to 512 */
    short lightmapY;
    short lightmapWidth;
    short lightmapHeight;
    struct Polygon *next;                  /* BSP polygon list or free-list link */

    const Vector3d *GetPoints() const;              /* preview point array */

    /* Convert lightmap coordinates to a world point. */
    void LightmapToWorld(double s, double t, Vector3d *out) const;
    /* Project a world point into a pair of lightmap coordinates. */
    void WorldToLightmap(const Vector3d *point, float *st) const;
    bool PointOnPolygon(const Vector3d &point) const;                      /* on the plane (epsilon) and inside the outline */
    bool ContainsPoint(const Vector3d &point) const;                       /* inside the outline, 2D test */
    void WorldToTexture(const Vector3d &point, float *st) const;           /* texture coordinates of a world point */
    int TrimPortalWinding(Winding *winding, const Bounds &bounds, Winding **fragments) const;   /* portal winding outside the polygon */
    void InterpolateNormal(const Vector3d *point, Vector3d *normal) const;   /* interpolated vertex normal at a point */
    bool IntersectsWinding(const Winding &winding, const Bounds &windingBounds) const;
    bool ContainsWinding(const Winding &winding, const Bounds &windingBounds) const;
    bool IntersectSegment(const Vector3d &start, const Vector3d &end) const;
    void Free();                                                           /* release to the polygon pool */
    void UpdateLightmapBounds();                                           /* lightmap rectangle enclosing the points */
    void SetCellIndex(int index)
    {
        info->cellIndex = index;
    }
    int GetCellIndex() const
    {
        return info->cellIndex;
    }
    bool IsLightmapFlipped() const
    {
        return info->lightmapToWorld.orientation < 0 ? true : false;
    }
    PolyInfo *GetInfo() const;
    Plane *GetBackPlane() const;                                     /* defined inline in sunlight.cpp */
    int GetFlags() const
    {
        return info->flags;
    }
    int GetShaderIndex() const;
    int GetLightmapIndex() const;
    MiscModel *GetModel() const;
};
FLARE_LAYOUT(Polygon_size, sizeof(struct Polygon) == 0x50);

/* Convert lightmap coordinates to a world point. */
inline void Polygon::LightmapToWorld(double s, double t, Vector3d *out) const
{
    out->x = s * info->lightmapToWorld.sAxis.x + t * info->lightmapToWorld.tAxis.x + info->lightmapToWorld.origin.x;
    out->y = s * info->lightmapToWorld.sAxis.y + t * info->lightmapToWorld.tAxis.y + info->lightmapToWorld.origin.y;
    out->z = s * info->lightmapToWorld.sAxis.z + t * info->lightmapToWorld.tAxis.z + info->lightmapToWorld.origin.z;
}
/* Project a world point into lightmap coordinates. */
inline void Polygon::WorldToLightmap(const Vector3d *point, float *st) const
{
    const Vector3d &s = info->worldToLightmap.uv.s;
    st[0] = *point * s + info->worldToLightmap.uv.sOffset;
    const Vector3d &t = info->worldToLightmap.uv.t;
    st[1] = *point * t + info->worldToLightmap.uv.tOffset;
}

FLARE_LAYOUT(Polygon_members, FLARE_OFFSET(struct Polygon, points) == 0x04 && FLARE_OFFSET(struct Polygon, normals) == 0x08 && FLARE_OFFSET(struct Polygon, center) == 0x10
                                  && FLARE_OFFSET(struct Polygon, info) == 0x28 && FLARE_OFFSET(struct Polygon, bounds) == 0x2C
                                  && FLARE_OFFSET(struct Polygon, lightmapX) == 0x44 && FLARE_OFFSET(struct Polygon, next) == 0x4C);

inline PolyInfo *Polygon::GetInfo() const
{
    return info;
}

inline void Polygon::WorldToTexture(const Vector3d &point, float *st) const
{
    const Vector3d &sAxis = info->worldToTexture.uv.s;
    st[0] = point * sAxis + info->worldToTexture.uv.sOffset;
    const Vector3d &tAxis = info->worldToTexture.uv.t;
    st[1] = point * tAxis + info->worldToTexture.uv.tOffset;
}

#endif
