/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Storage for loaded Call of Duty BSP lumps.
===========================================================================
*/

/* BSP lump counts and storage arrays (bspfile.c). */
#ifndef FLARE_COD_BSP_LOAD_STORAGE_H
#define FLARE_COD_BSP_LOAD_STORAGE_H
#include "cod_bsp_disk_records.h"
#include "bsp_light_disk.h"
#include "cod_bsp_lump_indices.h"
enum {
    COD_BSP_MAX_TRIANGLE_SURFACES = 131072,
    COD_BSP_MAX_LIGHTS = 2048,
    COD_BSP_MAX_LIGHT_INDEXES = 65536
};
extern int numShaders;
extern dshader_t dshaders[MAX_MAP_SHADERS];
extern int numLightBytes;
extern byte lightBytes[];
extern int numplanes;
extern dplane_t dplanes[MAX_MAP_PLANES];
extern int numbrushsides;
extern dbrushside_t dbrushsides[MAX_MAP_BRUSHSIDES];
extern int numbrushes;
extern dbrush_t dbrushes[MAX_MAP_BRUSHES];
extern int numFogs;
extern dfog_t dfogs[MAX_MAP_FOGS];
extern int numTriangleSurfaces;
extern cod_bsp_tri_soup_t triangleSurfaces[];
extern int numDrawVerts;
extern drawVert_t triangleSoupData[];
extern int numDrawIndexes;
extern unsigned short drawIndexes[];
extern int numCullGroups;
extern DiskGfxCullGroup cullGroups[];
extern int numCullGroupIndexes;
extern int cullGroupIndexes[];
extern int numPortalVerts;
extern cod_bsp_vertex_t dportalVerts[];
extern int numOccluders;
extern DiskGfxOccluder doccluders[];
extern int numOccluderPlanes;
extern int occluderPlane[];
extern int numOccluderEdges;
extern cod_bsp_occluder_edge_t bspOccluderEdges[];
extern int numOccluderIndexes;
extern unsigned short occluderIndexes[];
extern int numaabbtreenodes;
extern DiskGfxAabbTree daabbtrees[];
extern int numCells;
extern DiskGfxCell dcells[];
extern int numPortals;
extern DiskGfxPortal dportals[];
extern int numLightIndexes;
extern unsigned short bspLightIndexes[];
extern int numnodes;
extern dnode_t dnodes[MAX_MAP_NODES];
extern int numleafs;
extern dleaf_t dleafs[MAX_MAP_LEAFS];
extern int numleafbrushes;
extern int dleafbrushes[MAX_MAP_LEAFBRUSHES];
extern int numleafsurfaces;
extern int dleafsurfaces[MAX_MAP_LEAFFACES];
extern int numPatchCollides;
extern cod_bsp_collision_surface_t patchCollides[];
extern int numCollisionVerts;
extern cod_bsp_vertex_t collisionVerts[];
extern int numCollisionIndexes;
extern unsigned short collisionIndexes[];
extern int nummodels;
extern dmodel_t dmodels[MAX_MAP_MODELS];
extern int numLights;
extern bspLight_t dlights[];
extern int numVisBytes;
extern byte visBytes[MAX_MAP_VISIBILITY];
extern int entdatasize;
extern char dEntstring[MAX_MAP_ENTSTRING];
void SwapBSPFile( void );
#endif
