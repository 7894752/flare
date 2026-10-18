/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

BSP loading, surface conversion and lighting output.
===========================================================================
*/
#ifndef FLARE_BSPIO_H
#define FLARE_BSPIO_H

bool LoadBspForLighting(const char *name);
void WriteBspLighting(const char *name);

struct MapInfo;
struct MapEntity;
struct BspSurface;
struct BspBrushSide;
void ConvertSurface(MapInfo *map, const BspSurface *surface, int entityIndex);
BspBrushSide *ConvertEntityBrushes(MapInfo *map, MapEntity *entity, BspBrushSide *side);

#endif
