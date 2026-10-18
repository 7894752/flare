/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Command-line and worldspawn lighting options.
===========================================================================
*/
#ifndef FLARE_OPTIONDEFS_H
#define FLARE_OPTIONDEFS_H
#include "flare/math_types.h"
struct options_t
{
    bool worldModels;
    bool noModelShadows;
    bool noModelAlphaShadows;
    bool mergeOccluders;
    bool backSplash;
    bool noSmoothNormals;
    bool visual;
    bool keepVisualOnlyPolys;
    bool debugLmapAreas;
    bool debugLmapBleeding;
    bool noCorrectSolid;
    bool noDiffuseSun;
    bool sampleDiffuseSun;
    bool fastDiffuseSun;
    bool noLightmapCalculation;
    bool debugLightmap;
    bool fastAlphaCheck;
    bool noLeafLights;
    int maxPolys;
    int maxVerts;
    int maxMapBrushes;
    int maxOccluders;
    int maxAlphaPixels;
    int splitCost;
    int axialThreshold;
    float pointLightScale;
    float linearLightScale;
    float overbrightScale;
    float lightGridScale;
    float oneOverMinIntensity;
    float minIntensity;
    float minDiffuseIntensity;
    float minDiffuseSunIntensity;
    float areaLightStep;
    float sunDiffuseStep;
    Vector3d lightGridSize;
    int sunDiffuseSamples;
    bool sunDirectionSet;
    bool sunColorSet;
    bool sunDiffuseColorSet;
    bool sunLightSet;
    bool sunDiffuseFractionSet;
    bool minLightSet;
    Vector3d sunDirection;
    ColorRGB sunColor;
    ColorRGB sunDiffuseColor;
    ColorRGB ambientColor;
    float sunLight;
    float sunDiffuseFraction;
    float ambientIntensity;
    ColorRGB minLight;
    int picMip;
    int screenWidth;
    int screenHeight;
    bool fullScreen;
    bool verbose;
    bool quiet;
    int warningLevel;
    char baseDir[256];
    char mapName[256];
    char gameDir[256];
    char gameName[64];
};
extern options_t options;

#endif
