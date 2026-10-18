/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Command-line options, quality presets and compiler initialization.
===========================================================================
*/
#include <direct.h>
#include "flare/options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "code/qcommon/qcommon.h"

void Q3Map_Error(const char *format, ...);
void Q3Map_Warning(int level, const char *format, ...);
void Q3Map_Printf(const char *format, ...);
void PrintUsage();
bool ExecConfigFile(const char *filename);

class OptionsParserState {
public:
    unsigned int reserved; /* Unused parser-state word. */
    bool initialized;

    OptionsParserState() {}
};

OptionsParserState parserState;
options_t options;

int Option_Help(int argc, char **argv);
int Option_Verbose(int argc, char **argv);
int Option_Quiet(int argc, char **argv);
int Option_Warn(int argc, char **argv);
int ParseIntOption(int argc, char **argv, int *value, int minimum, int maximum);
int Option_Exec(int argc, char **argv);
int Option_BaseDir(int argc, char **argv);
int ParseStringOption(int argc, char **argv, char *value, int size);
int Option_Fast(int argc, char **argv);
void SetFastOptions();
int Option_Extra(int argc, char **argv);
void SetExtraOptions();
int Option_MaxPolys(int argc, char **argv);
int Option_MaxVerts(int argc, char **argv);
int Option_MaxBrushes(int argc, char **argv);
int Option_MaxOccluders(int argc, char **argv);
int Option_Visual(int argc, char **argv);
int Option_NoVisualOnlyPolys(int argc, char **argv);
int Option_Width(int argc, char **argv);
int Option_Height(int argc, char **argv);
int Option_Fullscreen(int argc, char **argv);
int Option_PicMip(int argc, char **argv);
int Option_DebugLmapAreas(int argc, char **argv);
int Option_DebugLmapBleeding(int argc, char **argv);
int Option_CorrectSolid(int argc, char **argv);
int Option_NoCorrectSolid(int argc, char **argv);
int Option_DiffuseSun(int argc, char **argv);
int Option_NoDiffuseSun(int argc, char **argv);
int Option_SampleDiffuseSun(int argc, char **argv);
int Option_FastDiffuseSun(int argc, char **argv);
int Option_ExtraDiffuseSun(int argc, char **argv);
int Option_AreaDiffuseSun(int argc, char **argv);
int Option_BackSplash(int argc, char **argv);
int Option_NoSmoothNormals(int argc, char **argv);
int Option_LeafLights(int argc, char **argv);
int Option_NoLeafLights(int argc, char **argv);
int Option_ModelShadow(int argc, char **argv);
int Option_NoModelShadow(int argc, char **argv);
int Option_WorldModels(int argc, char **argv);
int Option_LocalModels(int argc, char **argv);
int Option_ModelAlphaShadow(int argc, char **argv);
int Option_NoModelAlphaShadow(int argc, char **argv);
int Option_MergeOccluders(int argc, char **argv);
int Option_NoMergeOccluders(int argc, char **argv);
int Option_FastAlphaCheck(int argc, char **argv);
int Option_ExtraAlphaCheck(int argc, char **argv);
int Option_MaxAlphaPixels(int argc, char **argv);
int Option_SplitCost(int argc, char **argv);
int Option_AxialThreshold(int argc, char **argv);
int Option_SunColor(int argc, char **argv);
int ParseVectorOption(int argc, char **argv, double *value);
int Option_SunLight(int argc, char **argv);
int ParseFloatOption(int argc, char **argv, float *value, float minimum, float maximum);
int Option_SunDiffuseColor(int argc, char **argv);
int Option_DiffuseFraction(int argc, char **argv);
int Option_SunDirection(int argc, char **argv);
int Option_MinLight(int argc, char **argv);
int Option_SunDiffuseSamples(int argc, char **argv);
int Option_SunDiffuseStep(int argc, char **argv);
int Option_AreaLightStep(int argc, char **argv);
int Option_DumpOptions(int argc, char **argv);
void PrintBoolOption(const char *label, bool value);
void PrintIntOption(const char *label, int value);
void PrintFloatOption(const char *label, float value);
bool ExecuteOptions(int argc, char **argv);
bool ParseOptions(int argc, char **argv, int remaining);

struct CommandLineOption {
    const char *name;
    const char *description;
    int (*callback)(int argc, char **argv);
};

static const CommandLineOption commandLineOptions[] = {
    { "-?", "Displays usage information", Option_Help },
    { "-Help", "Displays usage information", Option_Help },
    { "-Verbose", "Turns on verbose prints", Option_Verbose },
    { "-Quiet", "Turns off progress counters (can be used with -Verbose)", Option_Quiet },
    { "-Warn", "Sets the warning level", Option_Warn },
    { "-Exec", "Executes a config file", Option_Exec },
    { "-BaseDir", "Name of the directory containing the game executable", Option_BaseDir },
    { "-Fast", "Use fast presets for several options", Option_Fast },
    { "-Extra", "Use high-quality presets for several options", Option_Extra },
    { "-MaxPolys", "Maximum number of polys to use during light compile", Option_MaxPolys },
    { "-MaxVerts", "Maximum number of vertices to use during light compile", Option_MaxVerts },
    { "-MaxBrushes", "Maximum number of brushes to use during bsp solid check", Option_MaxBrushes },
    { "-MaxOccluders", "Maximum number of occluders to use during light compile", Option_MaxOccluders },
    { "-Visual", "Activates visual preview / debugging", Option_Visual },
    { "-NoVisualOnlyPolys", "Don't keep polys to be drawn that don't affect lighting", Option_NoVisualOnlyPolys },
    { "-Width", "Sets screen width for visual debugging", Option_Width },
    { "-Height", "Sets screen height for visual debugging", Option_Height },
    { "-Fullscreen", "Uses fullscreen mode for visual debugging", Option_Fullscreen },
    { "-PicMip", "Skips the first N mipmap levels to save texture memory", Option_PicMip },
    { "-DebugLmapAreas", "Shows relative coverage of each lmap pixel as intensity", Option_DebugLmapAreas },
    { "-DebugLmapBleeding", "Tests lightmap bleeding", Option_DebugLmapBleeding },
    { "-CorrectSolid", "Forces correcting for partially covered polygons", Option_CorrectSolid },
    { "-NoCorrectSolid", "Skips correcting for partially covered polygons", Option_NoCorrectSolid },
    { "-DiffuseSun", "Froces diffuse sunlighting", Option_DiffuseSun },
    { "-NoDiffuseSun", "Doesn't do diffuse sunlighting", Option_NoDiffuseSun },
    { "-SampleDiffuseSun", "Diffuse sun uses point sampling instead of area lights", Option_SampleDiffuseSun },
    { "-AreaDiffuseSun", "Diffuse sun uses area lights instead of point sampling", Option_AreaDiffuseSun },
    { "-FastDiffuseSun", "SampleDiffuseSun uses a single vertical sky trace", Option_FastDiffuseSun },
    { "-ExtraDiffuseSun", "SampleDiffuseSun uses multiple sky traces", Option_ExtraDiffuseSun },
    { "-BackSplash", "Adds a backsplash pointlight for area lights", Option_BackSplash },
    { "-NoSmoothNormals", "Uses polygon normals instead of smoothed vertex normals", Option_NoSmoothNormals },
    { "-LeafLights", "Forces finding of entity lighting info (needs .PRT file)", Option_LeafLights },
    { "-NoLeafLights", "Don't find entity lighting info (also ignores .PRT file)", Option_NoLeafLights },
    { "-ModelShadow", "Allows model surfaces to cast shadows", Option_ModelShadow },
    { "-NoModelShadow", "Prevents model surfaces from casting shadows", Option_NoModelShadow },
    { "-WorldModels", "Adds model surfaces to the world BSP", Option_WorldModels },
    { "-LocalModels", "Keeps model surfaces out of the world BSP", Option_LocalModels },
    { "-ModelAlphaShadow", "Allows transparent model surfaces to cast shadows", Option_ModelAlphaShadow },
    { "-NoModelAlphaShadow", "Prevents transparent model surfaces from casting shadows", Option_NoModelAlphaShadow },
    { "-MergeOccluders", "Tries to merge occluders (may speed up model shadows)", Option_MergeOccluders },
    { "-NoMergeOccluders", "Doesn't merge occluders (may speeds up no model shadows)", Option_NoMergeOccluders },
    { "-FastAlphaCheck", "Does alpha checking at a point instead of over an area", Option_FastAlphaCheck },
    { "-ExtraAlphaCheck", "Does alpha checking over an area instead of at a point", Option_ExtraAlphaCheck },
    { "-MaxAlphaPixels", "Max pixels to do area alpha filtering (extra alpha only)", Option_MaxAlphaPixels },
    { "-SplitCost", "Heuristic penalty for splitting a poly (relative to 1)", Option_SplitCost },
    { "-AxialThreshold", "BSP always looks for axial planes for > this many polys", Option_AxialThreshold },
    { "-SunColor", "Direct sun color to use (-suncolor R G B)", Option_SunColor },
    { "-SunLight", "Intensity of direct sunlight", Option_SunLight },
    { "-SunDiffuseColor", "Diffuse sun color to use (-sundiffusecolor R G B)", Option_SunDiffuseColor },
    { "-DiffuseFraction", "Fraction of sun's intensity to be diffuse lighting", Option_DiffuseFraction },
    { "-SunDirection", "Sun direction to use (-sundirection pitch yaw roll)", Option_SunDirection },
    { "-MinLight", "Minimum intensity for a lightmap pixel (-minlight R G B)", Option_MinLight },
    { "-SunDiffuseSamples", "Hemicube edge size for SampleDiffuseSun (3N^2 samples)", Option_SunDiffuseSamples },
    { "-SunDiffuseStep", "Units between diffuse sun form factor light samples", Option_SunDiffuseStep },
    { "-AreaLightStep", "Units between area light samples", Option_AreaLightStep },
    { "-DumpOptions", "Displays current settings of most parameters", Option_DumpOptions },
};

/*
=============
Option_Help
=============
*/
int Option_Help(int argc, char **argv) {
    PrintUsage();
    return 1;
}

/*
=============
Option_Verbose
=============
*/
int Option_Verbose(int argc, char **argv) {
    options.verbose = true;
    return 1;
}

/*
=============
Option_Quiet
=============
*/
int Option_Quiet(int argc, char **argv) {
    options.quiet = true;
    return 1;
}

/*
=============
Option_Warn
=============
*/
int Option_Warn(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.warningLevel, 0, 4);
}

/*
=============
ParseIntOption

Read one value after the option name and clamp it to the allowed range.
Return the number of arguments consumed, including the option name.
=============
*/
int ParseIntOption(int argc, char **argv, int *value, int minimum, int maximum) {
    if (argc < 2)
        Q3Map_Error("%s: missing argument\n", argv[0]);
    *value = atoi(argv[1]);
    if (*value < minimum) {
        Q3Map_Warning(1, "%s: clamping to %i\n", argv[0], minimum);
        *value = minimum;
    } else if (*value > maximum) {
        Q3Map_Warning(1, "%s: clamping to %i\n", argv[0], maximum);
        *value = maximum;
    }
    return 2;
}

/*
=============
Option_Exec
=============
*/
int Option_Exec(int argc, char **argv) {
    if (argc < 2)
        Q3Map_Error("%s: missing argument\n", argv[0]);
    ExecConfigFile(argv[1]);
    return 2;
}

/*
=============
Option_BaseDir
=============
*/
int Option_BaseDir(int argc, char **argv) {
    return ParseStringOption(argc, argv, options.baseDir, sizeof(options.baseDir));
}

/*
=============
ParseStringOption

Copy one string argument after checking its capacity.  Return the number
of arguments consumed, including the option name.
=============
*/
int ParseStringOption(int argc, char **argv, char *value, int size) {
    if (argc < 2)
        Q3Map_Error("%s: missing argument\n", argv[0]);
    if ((int)strlen(argv[1]) >= size)
        Q3Map_Error("%s: string is longer than %i characters\n", argv[0], size);
    strcpy(value, argv[1]);
    return 2;
}

/*
=============
Option_Fast
=============
*/
int Option_Fast(int argc, char **argv) {
    SetFastOptions();
    return 1;
}

/*
=============
SetFastOptions

Use coarser light sampling and cheaper shadow and alpha checks.
=============
*/
void SetFastOptions() {
    options.noModelShadows = true;
    options.noModelAlphaShadows = true;
    options.mergeOccluders = false;
    options.backSplash = true;
    options.noCorrectSolid = true;
    options.noDiffuseSun = false;
    options.sampleDiffuseSun = true;
    options.fastDiffuseSun = true;
    options.fastAlphaCheck = true;
    options.maxAlphaPixels = 1024;
    options.oneOverMinIntensity = 63.75f * options.overbrightScale;
    options.minIntensity = 1.0f / options.oneOverMinIntensity;
    options.minDiffuseIntensity = 2.0f * options.minIntensity;
    options.areaLightStep = 64;
    options.sunDiffuseStep = 1024;
    options.sunDiffuseSamples = 2;
}

/*
=============
Option_Extra
=============
*/
int Option_Extra(int argc, char **argv) {
    SetExtraOptions();
    return 1;
}

/*
=============
SetExtraOptions

Use finer light sampling with model shadows and full alpha checks.
=============
*/
void SetExtraOptions() {
    options.noModelShadows = false;
    options.noModelAlphaShadows = false;
    options.mergeOccluders = false;
    options.backSplash = true;
    options.noCorrectSolid = false;
    options.noDiffuseSun = false;
    options.sampleDiffuseSun = true;
    options.fastDiffuseSun = false;
    options.fastAlphaCheck = false;
    options.maxAlphaPixels = 1024;
    options.oneOverMinIntensity = 127.5f * options.overbrightScale;
    options.minIntensity = 1.0f / options.oneOverMinIntensity;
    options.minDiffuseIntensity = 4.0f * options.minIntensity;
    options.minDiffuseSunIntensity = 4.0f * options.minIntensity;
    options.areaLightStep = 16;
    options.sunDiffuseStep = 128;
    options.sunDiffuseSamples = 8;
}

/*
=============
Option_MaxPolys
=============
*/
int Option_MaxPolys(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.maxPolys, 256, 8388608);
}

/*
=============
Option_MaxVerts
=============
*/
int Option_MaxVerts(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.maxVerts, 1024, 33554432);
}

/*
=============
Option_MaxBrushes
=============
*/
int Option_MaxBrushes(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.maxMapBrushes, 64, 2097152);
}

/*
=============
Option_MaxOccluders
=============
*/
int Option_MaxOccluders(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.maxOccluders, 256, 8388608);
}

/*
=============
Option_Visual
=============
*/
int Option_Visual(int argc, char **argv) {
    options.visual = true;
    options.keepVisualOnlyPolys = true;
    return 1;
}

/*
=============
Option_NoVisualOnlyPolys
=============
*/
int Option_NoVisualOnlyPolys(int argc, char **argv) {
    options.keepVisualOnlyPolys = false;
    return 1;
}

/*
=============
Option_Width
=============
*/
int Option_Width(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.screenWidth, 320, 2048);
}

/*
=============
Option_Height
=============
*/
int Option_Height(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.screenHeight, 240, 1536);
}

/*
=============
Option_Fullscreen
=============
*/
int Option_Fullscreen(int argc, char **argv) {
    options.fullScreen = true;
    return 1;
}

/*
=============
Option_PicMip
=============
*/
int Option_PicMip(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.picMip, 0, 16);
}

/*
=============
Option_DebugLmapAreas
=============
*/
int Option_DebugLmapAreas(int argc, char **argv) {
    options.noCorrectSolid = false;
    options.debugLmapAreas = true;
    options.debugLmapBleeding = false;
    options.noLightmapCalculation = true;
    options.debugLightmap = true;
    return 1;
}

/*
=============
Option_DebugLmapBleeding
=============
*/
int Option_DebugLmapBleeding(int argc, char **argv) {
    options.noCorrectSolid = false;
    options.debugLmapAreas = false;
    options.debugLmapBleeding = true;
    options.noLightmapCalculation = true;
    options.debugLightmap = true;
    return 1;
}

/*
=============
Option_CorrectSolid
=============
*/
int Option_CorrectSolid(int argc, char **argv) {
    options.noCorrectSolid = false;
    return 1;
}

/*
=============
Option_NoCorrectSolid
=============
*/
int Option_NoCorrectSolid(int argc, char **argv) {
    options.noCorrectSolid = true;
    return 1;
}

/*
=============
Option_DiffuseSun
=============
*/
int Option_DiffuseSun(int argc, char **argv) {
    options.noDiffuseSun = false;
    return 1;
}

/*
=============
Option_NoDiffuseSun
=============
*/
int Option_NoDiffuseSun(int argc, char **argv) {
    options.noDiffuseSun = true;
    return 1;
}

/*
=============
Option_SampleDiffuseSun
=============
*/
int Option_SampleDiffuseSun(int argc, char **argv) {
    options.noDiffuseSun = false;
    options.sampleDiffuseSun = true;
    return 1;
}

/*
=============
Option_FastDiffuseSun
=============
*/
int Option_FastDiffuseSun(int argc, char **argv) {
    options.noDiffuseSun = false;
    options.fastDiffuseSun = true;
    return 1;
}

/*
=============
Option_ExtraDiffuseSun
=============
*/
int Option_ExtraDiffuseSun(int argc, char **argv) {
    options.fastDiffuseSun = false;
    return 1;
}

/*
=============
Option_AreaDiffuseSun
=============
*/
int Option_AreaDiffuseSun(int argc, char **argv) {
    options.sampleDiffuseSun = false;
    return 1;
}

/*
=============
Option_BackSplash
=============
*/
int Option_BackSplash(int argc, char **argv) {
    options.backSplash = true;
    return 1;
}

/*
=============
Option_NoSmoothNormals
=============
*/
int Option_NoSmoothNormals(int argc, char **argv) {
    options.noSmoothNormals = true;
    return 1;
}

/*
=============
Option_LeafLights
=============
*/
int Option_LeafLights(int argc, char **argv) {
    options.noLeafLights = false;
    return 1;
}

/*
=============
Option_NoLeafLights
=============
*/
int Option_NoLeafLights(int argc, char **argv) {
    options.noLeafLights = true;
    return 1;
}

/*
=============
Option_ModelShadow
=============
*/
int Option_ModelShadow(int argc, char **argv) {
    options.noModelShadows = false;
    return 1;
}

/*
=============
Option_NoModelShadow
=============
*/
int Option_NoModelShadow(int argc, char **argv) {
    options.noModelShadows = true;
    return 1;
}

/*
=============
Option_WorldModels
=============
*/
int Option_WorldModels(int argc, char **argv) {
    options.worldModels = true;
    return 1;
}

/*
=============
Option_LocalModels
=============
*/
int Option_LocalModels(int argc, char **argv) {
    options.worldModels = false;
    return 1;
}

/*
=============
Option_ModelAlphaShadow
=============
*/
int Option_ModelAlphaShadow(int argc, char **argv) {
    options.noModelAlphaShadows = false;
    return 1;
}

/*
=============
Option_NoModelAlphaShadow
=============
*/
int Option_NoModelAlphaShadow(int argc, char **argv) {
    options.noModelAlphaShadows = true;
    return 1;
}

/*
=============
Option_MergeOccluders
=============
*/
int Option_MergeOccluders(int argc, char **argv) {
    options.mergeOccluders = true;
    return 1;
}

/*
=============
Option_NoMergeOccluders
=============
*/
int Option_NoMergeOccluders(int argc, char **argv) {
    options.mergeOccluders = false;
    return 1;
}

/*
=============
Option_FastAlphaCheck
=============
*/
int Option_FastAlphaCheck(int argc, char **argv) {
    options.fastAlphaCheck = true;
    return 1;
}

/*
=============
Option_ExtraAlphaCheck
=============
*/
int Option_ExtraAlphaCheck(int argc, char **argv) {
    options.fastAlphaCheck = false;
    return 1;
}

/*
=============
Option_MaxAlphaPixels
=============
*/
int Option_MaxAlphaPixels(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.maxAlphaPixels, 0, 1048576);
}

/*
=============
Option_SplitCost
=============
*/
int Option_SplitCost(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.splitCost, INT_MIN, INT_MAX);
}

/*
=============
Option_AxialThreshold
=============
*/
int Option_AxialThreshold(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.axialThreshold, 0, INT_MAX);
}

/*
=============
Option_SunColor
=============
*/
int Option_SunColor(int argc, char **argv) {
    double color[3];
    int numArgs;
    numArgs = ParseVectorOption(argc, argv, color);
    options.sunColorSet = true;
    options.sunColor.r = (float)color[0];
    options.sunColor.g = (float)color[1];
    options.sunColor.b = (float)color[2];
    return numArgs;
}

/*
=============
ParseVectorOption

Read three components and return the argument count including the option.
=============
*/
int ParseVectorOption(int argc, char **argv, double *value) {
    if (argc < 4)
        Q3Map_Error("%s: missing argument\n", argv[0]);
    value[0] = atof(argv[1]);
    value[1] = atof(argv[2]);
    value[2] = atof(argv[3]);
    return 4;
}

/*
=============
Option_SunLight
=============
*/
int Option_SunLight(int argc, char **argv) {
    options.sunLightSet = true;
    return ParseFloatOption(argc, argv, &options.sunLight, 0, FLT_MAX);
}

/*
=============
ParseFloatOption

Read one value after the option name and clamp it to the allowed range.
Return the number of arguments consumed, including the option name.
=============
*/
int ParseFloatOption(int argc, char **argv, float *value, float minimum, float maximum) {
    if (argc < 2)
        Q3Map_Error("%s: missing argument\n", argv[0]);
    *value = (float)atof(argv[1]);
    if (*value < minimum) {
        Q3Map_Warning(1, "%s: clamping to %g\n", argv[0], minimum);
        *value = minimum;
    } else if (*value > maximum) {
        Q3Map_Warning(1, "%s: clamping to %g\n", argv[0], maximum);
        *value = maximum;
    }
    return 2;
}

/*
=============
Option_SunDiffuseColor
=============
*/
int Option_SunDiffuseColor(int argc, char **argv) {
    double color[3];
    int numArgs;
    numArgs = ParseVectorOption(argc, argv, color);
    options.sunDiffuseColorSet = true;
    options.sunDiffuseColor.r = (float)color[0];
    options.sunDiffuseColor.g = (float)color[1];
    options.sunDiffuseColor.b = (float)color[2];
    return numArgs;
}

/*
=============
Option_DiffuseFraction
=============
*/
int Option_DiffuseFraction(int argc, char **argv) {
    options.sunDiffuseFractionSet = true;
    return ParseFloatOption(argc, argv, &options.sunDiffuseFraction, 0, 1);
}

/*
=============
Option_SunDirection

Convert pitch, yaw and roll to the forward vector used for sunlight.
=============
*/
int Option_SunDirection(int argc, char **argv) {
    double angles[3];
    float direction[3], inputAngles[3];
    int numArgs;
    numArgs = ParseVectorOption(argc, argv, angles);
    inputAngles[0] = (float)angles[0];
    inputAngles[1] = (float)angles[1];
    inputAngles[2] = (float)angles[2];
    AngleVectors(inputAngles, direction, 0, 0);
    options.sunDirectionSet = true;
    options.sunDirection.x = direction[0];
    options.sunDirection.y = direction[1];
    options.sunDirection.z = direction[2];
    return numArgs;
}

/*
=============
Option_MinLight
=============
*/
int Option_MinLight(int argc, char **argv) {
    double color[3];
    int numArgs;
    numArgs = ParseVectorOption(argc, argv, color);
    options.minLightSet = true;
    options.minLight.r = (float)color[0];
    options.minLight.g = (float)color[1];
    options.minLight.b = (float)color[2];
    return numArgs;
}

/*
=============
Option_SunDiffuseSamples
=============
*/
int Option_SunDiffuseSamples(int argc, char **argv) {
    return ParseIntOption(argc, argv, &options.sunDiffuseSamples, 2, 64);
}

/*
=============
Option_SunDiffuseStep
=============
*/
int Option_SunDiffuseStep(int argc, char **argv) {
    return ParseFloatOption(argc, argv, &options.sunDiffuseStep, 32, 16384);
}

/*
=============
Option_AreaLightStep
=============
*/
int Option_AreaLightStep(int argc, char **argv) {
    return ParseFloatOption(argc, argv, &options.areaLightStep, 4, 16384);
}

/*
=============
Option_DumpOptions

Print the current lighting, resource and preview settings.
=============
*/
int Option_DumpOptions(int argc, char **argv) {
    PrintBoolOption("models in world bsp:", options.worldModels);
    PrintBoolOption("model shadows:", !options.noModelShadows);
    PrintBoolOption("model alpha shadows:", !options.noModelAlphaShadows);
    PrintBoolOption("occluder merging:", options.mergeOccluders);
    PrintBoolOption("area light backsplash:", options.backSplash);
    PrintBoolOption("smooth normals:", !options.noSmoothNormals);
    PrintBoolOption("visual debugging:", options.visual);
    PrintBoolOption("keep visual only polys:", options.keepVisualOnlyPolys);
    PrintBoolOption("verbose messages:", options.verbose);
    PrintBoolOption("debug lightmap areas:", options.debugLmapAreas);
    PrintBoolOption("debug lightmap bleeding:", options.debugLmapBleeding);
    PrintBoolOption("poly in solid correction:", !options.noCorrectSolid);
    PrintBoolOption("diffuse sunlighting:", !options.noDiffuseSun);
    PrintBoolOption("point-sampled diffuse sun:", options.sampleDiffuseSun);
    PrintBoolOption("single-sample diffuse sun:", options.fastDiffuseSun);
    PrintBoolOption("lightmap calculation:", !options.noLightmapCalculation);
    PrintBoolOption("extra alpha check:", !options.fastAlphaCheck);
    PrintIntOption("max alpha pixels:", options.maxAlphaPixels);
    PrintBoolOption("find leafs for lights:", !options.noLeafLights);
    PrintIntOption("max polys:", options.maxPolys);
    PrintIntOption("max vertices:", options.maxVerts);
    PrintIntOption("max brushes:", options.maxMapBrushes);
    PrintIntOption("max occluders:", options.maxOccluders);
    PrintIntOption("bsp poly split cost:", options.splitCost);
    PrintIntOption("axis-aligned bsp threshold:", options.axialThreshold);
    PrintFloatOption("point light scale:", options.pointLightScale);
    PrintFloatOption("linear light scale:", options.linearLightScale);
    PrintFloatOption("light grid scale:", options.lightGridScale);
    PrintFloatOption("overbright scale:", options.overbrightScale);
    PrintFloatOption("one over min intensity:", options.oneOverMinIntensity);
    PrintIntOption("hemicube edge size:", options.sunDiffuseSamples);
    PrintFloatOption("surface light step world:", options.areaLightStep);
    PrintFloatOption("surface light step sky:", options.sunDiffuseStep);
    return 1;
}

/*
=============
PrintBoolOption
=============
*/
void PrintBoolOption(const char *label, bool value) {
    Q3Map_Printf("%-30s %s\n", label, value ? "enabled" : "disabled");
}

/*
=============
PrintIntOption
=============
*/
void PrintIntOption(const char *label, int value) {
    Q3Map_Printf("%-30s %i\n", label, value);
}

/*
=============
PrintFloatOption
=============
*/
void PrintFloatOption(const char *label, float value) {
    Q3Map_Printf("%-30s %g\n", label, value);
}

/*
=============
PrintUsage

List the command-line options in their table order.
=============
*/
void PrintUsage() {
    unsigned int i;
    Q3Map_Printf("USAGE: FLARE [args] mapname, where args is 0 or more of the following.\n");
    Q3Map_Printf("Options ignore capitalization; it is only present in the list for clarity.\n");
    for (i = 0; i < sizeof(commandLineOptions) / sizeof(commandLineOptions[0]); i++)
        Q3Map_Printf("%-20s %s\n", commandLineOptions[i].name, commandLineOptions[i].description);
}

/*
=============
ExecuteOptions

Allow config-file options only until initialization is complete.
=============
*/
bool ExecuteOptions(int argc, char **argv) {
    if (parserState.initialized) {
        Q3Map_Warning(2, "cannot execute command line options after initialization stage\n");
        return false;
    }
    return ParseOptions(argc, argv, 0);
}

/*
=============
ParseOptions

Dispatch options and advance past their consumed arguments.  Leave
remaining arguments untouched for the caller, or fail on an invalid count.
=============
*/
bool ParseOptions(int argc, char **argv, int remaining) {
    unsigned int i;
    int consumed;
    while (argc > remaining) {
        for (i = 0; i < sizeof(commandLineOptions) / sizeof(commandLineOptions[0]); i++) {
            if (!stricmp(argv[0], commandLineOptions[i].name)) {
                consumed = commandLineOptions[i].callback(argc, argv);
                if (consumed <= 0) {
                    PrintUsage();
                    return false;
                }
                argc -= consumed;
                argv += consumed;
                break;
            }
        }
        if (i == sizeof(commandLineOptions) / sizeof(commandLineOptions[0])) {
            PrintUsage();
            Q3Map_Printf("\n");
            Q3Map_Error("Unknown argument '%s'\n", argv[0]);
        }
    }
    if (argc != remaining) {
        PrintUsage();
        return false;
    }
    return true;
}

extern "C" {
    void SetQdirFromPath(const char *path, const char *baseDir);
    void FS_InitFilesystemFromArgs(const char *basepath, const char *game, const char *basegame);
    void SetErrorHandler(void (*handler)(const char *, va_list));
    extern char qdir[1024];
}
void Q3Map_VError(const char *format, va_list args);
void InitPolygonPool(int maxCount);
void InitVectorPool(int maxCount);
void AllocBrushPool(int maxCount);
void SetDefaultOptions();

/*
=============
InitOptions

Set defaults and paths, load flare.cfg from the current directory or
beside the executable, then apply command-line overrides.  Reserve the
final argument for the map name and allocate the map pools on success.
=============
*/
bool InitOptions(int argc, char **argv) {
    char executableDir[1024];
    char *slash;
    SetDefaultOptions();
    strcpy(executableDir, argv[0]);
    slash = strrchr(executableDir, '/') > strrchr(executableDir, '\\')
          ? strrchr(executableDir, '/') : strrchr(executableDir, '\\');
    if (slash)
        *slash = 0;
    argc--;
    argv++;
    if (!argc) {
        PrintUsage();
        return false;
    }
    {
        char *extension;
        if (!stricmp(argv[0], "-BaseDir")) {
            int consumed = Option_BaseDir(argc, argv);
            if (consumed <= 0) {
                PrintUsage();
                return false;
            }
            argc -= consumed;
            argv += consumed;
        }
        strcpy(options.mapName, argv[argc - 1]);
        slash = strrchr(options.mapName, '/') > strrchr(options.mapName, '\\')
              ? strrchr(options.mapName, '/') : strrchr(options.mapName, '\\');
        extension = strrchr(options.mapName, '.');
        if (extension && (!slash || extension > slash))
            *extension = 0;
        strcat(options.mapName, ".bsp");
        if (options.mapName[0] != '-' && options.mapName[0] != '?') {
            SetQdirFromPath(options.mapName, options.baseDir);
            FS_InitFilesystemFromArgs(qdir, "", "");
            SetErrorHandler(Q3Map_VError);
        }
        if (!ExecConfigFile("flare.cfg")) {
            char configFile[1024];
            sprintf(configFile, "%s/flare.cfg", executableDir);
            if (!ExecConfigFile(configFile))
                Q3Map_Printf("couldn't exec 'flare.cfg' or '%s'\n", configFile);
        }
        if (!ParseOptions(argc, argv, 1))
            return false;
        if (options.mapName[0] == '-' || options.mapName[0] == '?')
            return false;
        parserState.initialized = true;
        InitPolygonPool(options.maxPolys);
        InitVectorPool(options.maxVerts);
        AllocBrushPool(options.maxMapBrushes);
        return true;
    }
}

/*
=============
SetDefaultOptions

Initialize lighting, resource and preview defaults.  Derive the default
game directory from the current directory and the base-directory name.
=============
*/
void SetDefaultOptions() {
    options.worldModels = true;
    options.noModelShadows = true;
    options.noModelAlphaShadows = true;
    options.mergeOccluders = false;
    options.backSplash = true;
    options.noSmoothNormals = false;
    options.visual = false;
    options.keepVisualOnlyPolys = false;
    options.debugLmapAreas = false;
    options.debugLmapBleeding = false;
    options.noCorrectSolid = false;
    options.noDiffuseSun = false;
    options.sampleDiffuseSun = true;
    options.fastDiffuseSun = false;
    options.noLightmapCalculation = false;
    options.debugLightmap = true;
    options.fastAlphaCheck = false;
    options.noLeafLights = false;
    options.maxPolys = 1048576;
    options.maxVerts = 4194304;
    options.maxMapBrushes = 262144;
    options.maxOccluders = 0;
    options.maxAlphaPixels = 16384;
    options.splitCost = 8;
    options.axialThreshold = 500;
    options.pointLightScale = 7500;
    options.linearLightScale = 0.000125f;
    options.overbrightScale = 2;
    options.lightGridScale = 1;
    options.oneOverMinIntensity = 127.5f * options.overbrightScale;
    options.minIntensity = 1.0f / options.oneOverMinIntensity;
    options.minDiffuseIntensity = 2.0f * options.minIntensity;
    options.minDiffuseSunIntensity = 4.0f * options.minIntensity;
    options.areaLightStep = 32;
    options.sunDiffuseStep = 512;
    options.lightGridSize = Vector3d(64, 64, 128);
    options.sunDiffuseSamples = 3;
    options.sunDirectionSet = false;
    options.sunColorSet = false;
    options.sunDiffuseColorSet = false;
    options.sunLightSet = false;
    options.sunDiffuseFractionSet = false;
    options.minLightSet = false;
    options.sunDirection = Vector3d(0.441835f, 0.56807f, 0.694313f);
    options.sunColor = Color3f(1, 1, 0.9f);
    options.sunDiffuseColor = Color3f(0, 0, 0);
    options.ambientColor = Color3f(0, 0, 0);
    options.ambientIntensity = 0;
    options.sunLight = 1;
    options.sunDiffuseFraction = 0.5f;
    options.minLight = Color3f(0, 0, 0);
    options.picMip = 2;
    options.screenWidth = 800;
    options.screenHeight = 600;
    options.fullScreen = false;
    options.verbose = false;
    options.quiet = false;
    options.warningLevel = 4;
    strcpy(options.baseDir, "Call of Duty");
    strcpy(options.gameName, "main");
    _getcwd(options.gameDir, sizeof(options.gameDir));
    char *slash = strrchr(options.gameDir, '/') > strrchr(options.gameDir, '\\')
          ? strrchr(options.gameDir, '/') : strrchr(options.gameDir, '\\');
    if (slash) {
        slash++;
        strcpy(slash, options.baseDir);
    }
}
