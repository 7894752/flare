/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Lightmap sample colors, coverage areas and sample pages.
===========================================================================
*/
#ifndef FLARE_LIGHTMAP_H
#define FLARE_LIGHTMAP_H

#include "flare/mathlib.h"

struct LightmapSample
{
    Color3f color;
    float area;                                    /* accumulated polygon coverage */
    LightmapSample(void);
};
typedef char flare_layout_LightmapSample[(sizeof(LightmapSample) == 0x10) ? 1 : -1];

struct Lightmap
{
    LightmapSample samples[512][512];
};

extern Lightmap *lightmapSamples;

inline ColorRGB::operator float *()
{
    return &r;
}

extern int numLightmaps;
void InitAmbientLightmaps(void);
void BuildLightmapMask(void);

#endif
