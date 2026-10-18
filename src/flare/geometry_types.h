/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Polygon mappings, geometry pools, BSP nodes, portals and traces.
===========================================================================
*/
#ifndef FLARE_GEOMETRY_TYPES_H
#define FLARE_GEOMETRY_TYPES_H

#include "flare/math_types.h"

#ifndef FLARE_LAYOUT
#define FLARE_LAYOUT(name, cond) typedef char flare_layout_##name[(cond) ? 1 : -1]
#define FLARE_OFFSET(type, member) ((int)(&((type *)0)->member))
#endif

struct MiscModel;       /* model instance */
struct Polygon;
struct BspNode;
struct TraceResult;

/* World-point projection onto texture or lightmap coordinates. */
struct UVMapping
{
    Vector3d s;
    Vector3d t;
    double sOffset;
    double tOffset;
};

/* Polygon surface flags (PolyInfo::flags). */
enum
{
    POLYFLAG_0x01 = 0x01,       /* simple occluder when combined with 0x40 */
    POLYFLAG_0x02 = 0x02,       /* excluded from lighting passes */
    POLYFLAG_SKY = 0x04,        /* sky surface */
    POLYFLAG_MODEL = 0x10,      /* PolyInfo::model is valid */
    POLYFLAG_0x40 = 0x40        /* participates in occlusion and diffuse sunlight */
};

/* Forward projection and inverse world-space basis for a polygon mapping. */
struct PolyMappingForward
{
    UVMapping uv;
    double orientation;             /* signed projected area scale */
};
FLARE_LAYOUT(PolyMappingForward_size, sizeof(PolyMappingForward) == 0x48);

struct PolyMappingInverse
{
    Vector3d sAxis;
    Vector3d tAxis;
    Vector3d origin;
    double orientation;
};
FLARE_LAYOUT(PolyMappingInverse_size, sizeof(PolyMappingInverse) == 0x50);

/* Surface information shared by its polygons (polygon.cpp). */
struct PolyInfo
{
    Plane plane;                    /* front plane */
    Plane backPlane;                /* the same plane flipped */
    PolyMappingInverse lightmapToWorld; /* lightmap (s,t) to world space */
    PolyMappingForward worldToLightmap; /* world space to lightmap (s,t) */
    PolyMappingForward worldToTexture;  /* world space to texture (s,t) */
    unsigned short refCount;        /* initially one reference */
    unsigned short flags;           /* POLYFLAG_* */
    short shaderIndex;              /* index into the shader table */
    short lightmapIndex;            /* -1 for a polygon without a lightmap */
    Polygon *owner;                 /* first polygon of the surface */
    union                           /* selected by the surface flags */
    {
        MiscModel *model;           /* POLYFLAG_MODEL */
        int cellIndex;              /* portal cell index */
    };

    /* Mapping subobjects used by the affine solvers. */
    PolyMappingForward *LightmapForward()
    {
        return &worldToLightmap;
    }
    PolyMappingForward *TextureForward()
    {
        return &worldToTexture;
    }
    PolyMappingInverse *LightmapInverse()
    {
        return &lightmapToWorld;
    }
};
FLARE_LAYOUT(PolyInfo_size, sizeof(PolyInfo) == 0x140);
FLARE_LAYOUT(PolyInfo_members, FLARE_OFFSET(PolyInfo, backPlane) == 0x28 && FLARE_OFFSET(PolyInfo, lightmapToWorld) == 0x50
                                   && FLARE_OFFSET(PolyInfo, worldToLightmap) == 0xA0
                                   && FLARE_OFFSET(PolyInfo, worldToTexture) == 0xE8
                                   && FLARE_OFFSET(PolyInfo, refCount) == 0x130 && FLARE_OFFSET(PolyInfo, cellIndex) == 0x13C);

/* Creates a polygon from points, optional normals, lightmap and texture coordinates (polygon.cpp). */
Polygon *CreatePoly(unsigned short flags, int numPoints, const Vector3d *points, const Vector3d *normals,
                    const float (*lightmapST)[2], const float (*st)[2], short shaderIndex, short lightmapIndex, bool check);

/* Polygon edge planes and its projected bounds. */
struct Occluder
{
    const Polygon *polygon;
    Plane *sidePlanes;              /* one plane per polygon edge */
    float mins[2];                  /* projected face-space bounds */
    float maxs[2];
    Occluder *next;
};
FLARE_LAYOUT(Occluder_size, sizeof(Occluder) == 0x1C);

/* Winding and per-node links of a BSP portal. */
struct Portal
{
    const Plane *plane;             /* split plane that created the portal */
    Winding winding;                /* portal outline */
    BspNode *nodes[2];
    Portal *next[2];                /* per-node link, indexed by node == nodes[1] */

    Portal() {}
    ~Portal();                                      /* out of line */
    Portal &operator=(const Portal &other);
};
FLARE_LAYOUT(Portal_size, sizeof(Portal) == 0x1C);

/* BSP split node or polygon leaf. */
struct BspNode
{
    BspNode *parent;                /* parent node */
    BspNode *children[2];           /* valid when !isLeaf */
    Bounds bounds;                  /* bounds of child nodes or leaf polygons */
    Plane plane;                    /* split plane */
    bool isLeaf;
    bool flooded;                  /* reached by FloodPortals_r */
    bool solid;                     /* solid leaf contents */
    bool flag53;                    /* leaf with a coplanar portal boundary */
    Polygon *polygons;              /* leaf polygon list, linked by Polygon::next */
    Portal *portals;                /* first portal, linked by Portal::next[side] */
    unsigned int reserved5C;        /* reserved, cleared on allocation */
};
FLARE_LAYOUT(BspNode_size, sizeof(BspNode) == 0x60);
FLARE_LAYOUT(BspNode_members, FLARE_OFFSET(BspNode, bounds) == 0x0C && FLARE_OFFSET(BspNode, plane) == 0x28
                                  && FLARE_OFFSET(BspNode, isLeaf) == 0x50 && FLARE_OFFSET(BspNode, polygons) == 0x54);

/* Plane windings of a solid map brush. */
struct BrushFace
{
    Plane plane;
    Winding winding;

    BrushFace();                                    /* empty, out of line */
    ~BrushFace();
};
FLARE_LAYOUT(BrushFace_size, sizeof(BrushFace) == 0x30);

struct Brush
{
    bool solid;
    int numFaces;
    BrushFace *faces;
};
FLARE_LAYOUT(Brush_size, sizeof(Brush) == 0x0C);

#endif
