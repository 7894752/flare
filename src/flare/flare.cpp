/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Lighting compiler entry point and color normalization.
===========================================================================
*/
#include "flare/options.h"
#include "flare/bspio.h"
#include "flare/print.h"
#include <stdio.h>
#include "flare/scr_stringlist.h"
#include "flare/geometry.h"
#include "flare/light.h"

float MaxFloat(float a, float b);
void DObjInit();
void DObjShutdown();
bool InitOptions(int argc, char **argv);
void FinalizePolygons();
void AllocLightmapSamples();
bool GL_CreateWindow(int width, int height, bool fullscreen);
void GL_InitTextures();
void Light_Compile();
struct BspNode;
BspNode *BuildBsp();
bool BuildPortals();
void FinalizeLightmaps();
void GL_UpdateLightmaps();
void CalculateSceneLighting();
void BuildLightmapMask();
void GL_Step(int level);
void GL_DrawFrame();
void GL_ResetGamma();

/*
=============
ColorRGB::operator*=
=============
*/
inline void ColorRGB::operator*=(float scale)
{
    r *= scale;
    g *= scale;
    b *= scale;
}

/*
============
main
============
*/
int main(int argc, char **argv)
{
    if (!InitOptions(argc, argv))
        return 0;
    SL_Init();
    DObjInit();
    if (!LoadBspForLighting(options.mapName))
    {
        DObjShutdown();
        return 0;
    }
    FinalizePolygons();
    AllocLightmapSamples();
    if (!options.sunDiffuseColorSet)
    {
        options.sunDiffuseColor = options.sunColor;
        options.sunDiffuseColorSet = true;
    }
    options.sunColor.Normalize();
    options.sunDiffuseColor.Normalize();
    printf("sunlight = %g\n", options.sunLight);
    printf("ambient = %g\n", options.ambientIntensity);
    options.sunColor *= (options.sunLight - options.ambientIntensity) *
                        (1.0f - options.sunDiffuseFraction);
    options.sunDiffuseColor *= (options.sunLight - options.ambientIntensity) *
                               options.sunDiffuseFraction;
    options.ambientColor *= options.ambientIntensity;
    if (options.visual && !GL_CreateWindow(options.screenWidth, options.screenHeight, options.fullScreen))
    {
        Q3Map_Warning(1, "Couldn't create window\n");
        options.visual = false;
    }
    GL_InitTextures();
    Light_Compile();
    BuildBsp();
    if (!options.noCorrectSolid || !options.debugLightmap)
    {
        if (!BuildPortals())
            options.noCorrectSolid = true;
    }
    FinalizeLightmaps();
    GL_UpdateLightmaps();
    if (!numSceneLights)
        Q3Map_Error("No lights\n");
    if (!options.noLightmapCalculation)
        CalculateSceneLighting();
    else if (options.debugLmapBleeding)
    {
        BuildLightmapMask();
        GL_UpdateLightmaps();
    }
    Q3Map_Printf("used %i polys and %i verts total\n", peakPolygons, vectorPoolHighWater);
    GL_Step(0);
    if (!options.debugLmapAreas && !options.debugLmapBleeding)
        WriteBspLighting(options.mapName);
    GL_UpdateLightmaps();
    if (options.visual)
    {
        for (;;)
            GL_DrawFrame();
    }
    DObjShutdown();
    GL_ResetGamma();
    return 0;
}

/*
=============
ColorRGB::Normalize
=============
*/
inline void ColorRGB::Normalize()
{
    float maxComponent = MaxFloat(r, MaxFloat(g, b));
    if (maxComponent > 0)
    {
        float scale = 1.0f / maxComponent;
        r = scale * r;
        b = scale * b;
        g = scale * g;
    }
}

/*
=============
MaxFloat
=============
*/
float MaxFloat(float a, float b)
{
    return a < b ? b : a;
}
