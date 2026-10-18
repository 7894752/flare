/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

/* Shader parsing, material flags and image loading. */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>


#include "flare/parse.h"
#include "code/qcommon/qcommon.h"

#include "flare/mathlib.h"
#include "flare/print.h"

static const float sunLightScale = 4.0f / 255.0f;

char *shaderText = 0;
static const char **shaderDefinitions = 0;
static int numShaderDefinitions = 0;

#include "flare/shaders.h"
#include "flare/options.h"

#define SHADER_SKY SHADERFLAG_0x01
#define SHADER_ALPHA_GT0 SHADERFLAG_ALPHA_TEST
#define SHADER_ALPHA_GE128 SHADERFLAG_ALPHA_BLEND
#define SHADER_ALPHA_LT128 (SHADER_ALPHA_GT0 | SHADER_ALPHA_GE128)
#define SHADER_ALPHA_TEST_MASK (SHADER_ALPHA_GT0 | SHADER_ALPHA_GE128)
#define SHADER_BLEND SHADERFLAG_0x08
#define SHADER_ADDITIVE SHADERFLAG_0x10
#define SHADER_POLYGON_OFFSET SHADERFLAG_0x20
#define SHADER_NOSHADOW SHADERFLAG_0x40
#define SHADER_TRANSPARENT (SHADER_ALPHA_TEST_MASK | SHADER_BLEND)
#define MAX_REQUESTABLE_SHADERS 1024
#define FIRST_REQUESTABLE_SHADER (MAX_SHADERS - MAX_REQUESTABLE_SHADERS)

#define IMAGE_ALPHA_NONE 0
#define IMAGE_ALPHA_BLEND 1
#define IMAGE_ALPHA_GT0 2
#define IMAGE_ALPHA_GE128 3
#define IMAGE_ALPHA_LT128 4
#define IMAGE_ALPHA_ADDITIVE 5

ShaderInfo shaders[MAX_SHADERS] = {0};
static short numRequestableShaders = FIRST_REQUESTABLE_SHADER;

/*
=============
ScanShaderDefinitions

Count complete shader definitions, recording their starts when storage is available.
=============
*/
static void ScanShaderDefinitions(void)
{
    const char *text;
    char token[512];
    int depth;

    numShaderDefinitions = 0;
    text = shaderText;
    while (GetToken(&text, token, sizeof(token), true))
    {
        if (shaderDefinitions)
            shaderDefinitions[numShaderDefinitions] = text - strlen(token);
        SkipLine(&text);
        if (GetToken(&text, token, sizeof(token), true) && token[0] == '{')
        {
            depth = 1;
            SkipLine(&text);
            while (GetToken(&text, token, sizeof(token), true))
            {
                if (token[0] == '{')
                    depth++;
                else if (token[0] == '}')
                {
                    depth--;
                    if (!depth)
                    {
                        numShaderDefinitions++;
                        break;
                    }
                }
                SkipLine(&text);
            }
        }
    }
}

/*
=============
ParseStage

Read image and transparency settings from a shader stage.
=============
*/
static const char *ParseStage(const char *text, int shaderIndex,
                              const char *textureName, bool firstStage, bool useTextureName)
{
    char token[512];

    SkipLine(&text);
    while (GetToken(&text, token, sizeof(token), true))
    {
        if (token[0] == '}')
        {
            SkipLine(&text);
            break;
        }
        if (!stricmp(token, "map"))
        {
            while (GetToken(&text, token, 64, false))
            {
                if (token[0] != '$')
                    strcpy(shaders[shaderIndex].texture, token);
                else if (!stricmp(token, "$texturename"))
                    strcpy(shaders[shaderIndex].texture, textureName);
            }
        }
        else if (!stricmp(token, "blendfunc"))
        {
            if (firstStage && GetToken(&text, token, sizeof(token), false))
            {
                if (!stricmp(token, "blend"))
                    shaders[shaderIndex].flags |= SHADER_BLEND;
                else if (!stricmp(token, "GL_SRC_ALPHA"))
                {
                    if (GetToken(&text, token, sizeof(token), false) &&
                        !stricmp(token, "GL_ONE_MINUS_SRC_ALPHA"))
                        shaders[shaderIndex].flags |= SHADER_BLEND;
                }
                else if (!stricmp(token, "add"))
                    shaders[shaderIndex].flags |= SHADER_ADDITIVE;
                else if (!stricmp(token, "GL_ONE"))
                {
                    if (GetToken(&text, token, sizeof(token), false) &&
                        !stricmp(token, "GL_ONE"))
                        shaders[shaderIndex].flags |= SHADER_ADDITIVE;
                }
            }
        }
        else if (!stricmp(token, "alphaFunc"))
        {
            if (firstStage && GetToken(&text, token, sizeof(token), false))
            {
                if (!stricmp(token, "gt0"))
                    shaders[shaderIndex].flags |= SHADER_ALPHA_GT0;
                else if (!stricmp(token, "ge128"))
                    shaders[shaderIndex].flags |= SHADER_ALPHA_GE128;
                else if (!stricmp(token, "lt128"))
                    shaders[shaderIndex].flags |= SHADER_ALPHA_LT128;
            }
        }
        SkipLine(&text);
    }
    return text;
}

/*
=============
ShaderFileNameCompare

Compare the shader names at the starts of indexed definitions.
=============
*/
int ShaderFileNameCompare(const void *a, const void *b)
{
    const char *name1 = *(const char * const *)a;
    const char *name2 = *(const char * const *)b;
    return stricmp(name1, name2);
}

/*
=============
Q3Map_InitShaders

Collect script text and build a sorted index of shader definitions.
=============
*/
void Q3Map_InitShaders(void)
{
    char filename[1024];
    char **files;
    void *buffer;
    int numFiles;
    int i, j, fileSize, length, size, used;

    files = FS_ListFiles("scripts", ".shader", &numFiles);
    size = 1;
    for (i = 0; i < numFiles; i++)
    {
        sprintf(filename, "scripts/%s", files[i]);
        fileSize = FS_ReadFile(filename, NULL);
        if (fileSize < 0)
            Q3Map_Error("couldn't get the file size of '%s'\n", files[i]);
        size += fileSize + 1;
    }
    if (size == 1)
        return;
    shaderText = new char[size];
    if (!shaderText)
        Q3Map_Error("Q3Map_InitShaders: couldn't allocate shader storage (%i bytes)\n", size);
    used = 0;
    for (j = 0; j < numFiles; j++)
    {
        sprintf(filename, "scripts/%s", files[j]);
        length = FS_ReadFile(filename, &buffer);
        if (used + length > size)
            Q3Map_Error("predicted shader size exceeded by actual size\n");
        memcpy(shaderText + used, buffer, length);
        shaderText[used + length] = '\n';
        used += length + 1;
        FS_FreeFile(buffer);
    }
    if (used + 1 > size)
        Q3Map_Error("predicted shader size exceeded by actual size\n");
    shaderText[used] = 0;
    ScanShaderDefinitions();
    if (!numShaderDefinitions)
    {
        delete [] shaderText;
        shaderText = NULL;
        return;
    }
    shaderDefinitions = new const char *[numShaderDefinitions];
    if (!shaderText)
        Q3Map_Error("Q3Map_InitShaders: couldn't allocate shader definition storage (%i bytes)\n",
                    numShaderDefinitions * sizeof(*shaderDefinitions));
    ScanShaderDefinitions();
    qsort(shaderDefinitions, numShaderDefinitions, sizeof(*shaderDefinitions), ShaderFileNameCompare);
}

/*
=============
ColorRGB::operator*=
=============
*/
void ColorRGB::operator *= (float scale)
{
    r *= scale;
    g *= scale;
    b *= scale;
}

/*
=============
ColorRGB::operator/=
=============
*/
void ColorRGB::operator /= (float scale)
{
    float inverse = 1.0f / scale;
    r *= inverse;
    g *= inverse;
    b *= inverse;
}

/*
=============
ColorRGB::operator float *
=============
*/
ColorRGB::operator float * (void)
{
    return &r;
}

/*
===============
LoadShaderImage

Load the material image and derive its color and transparency settings.
===============
*/
static bool LoadShaderImage(int shaderIndex)
{
    ShaderInfo *si = &shaders[shaderIndex];
    int i, count;

    si->image.width = 0;
    si->image.height = 0;
    si->image.pixels = NULL;
    if (si->lightImage[0])
        LoadImage(si->lightImage, &si->image);
    if (si->editorImage[0] && !si->image.pixels)
        LoadImage(si->editorImage, &si->image);
    if (si->texture[0] && !si->image.pixels)
        LoadImage(si->texture, &si->image);
    if (!si->image.pixels)
    {
        if (si->editorImage[0] || si->lightImage[0] || si->texture[0])
            return false;
        LoadImage(si->name, &si->image);
    }
    if (si->image.pixels)
    {
        count = si->image.width * si->image.height;
        Color3f total(0.0f, 0.0f, 0.0f);
        for (i = 0; i < count; i++)
        {
            total.r += si->image.pixels[4 * i];
            total.g += si->image.pixels[4 * i + 1];
            total.b += si->image.pixels[4 * i + 2];
        }
        si->color = total;
        si->color.Normalize();
        float scale = 1.0f / (count * 255.0f);
        si->averageColor = Color3f(total.r * scale, total.g * scale, total.b * scale);
        if (si->image.hasAlpha)
        {
            if (si->flags & SHADER_ALPHA_TEST_MASK)
            {
                if ((si->flags & SHADER_ALPHA_TEST_MASK) == SHADER_ALPHA_GT0)
                    si->image.alphaType = IMAGE_ALPHA_GT0;
                else if ((si->flags & SHADER_ALPHA_TEST_MASK) == SHADER_ALPHA_GE128)
                    si->image.alphaType = IMAGE_ALPHA_GE128;
                else if ((si->flags & SHADER_ALPHA_TEST_MASK) == SHADER_ALPHA_LT128)
                    si->image.alphaType = IMAGE_ALPHA_LT128;
            }
            else
            {
                if (si->flags & SHADER_BLEND)
                    si->image.alphaType = IMAGE_ALPHA_BLEND;
                else
                    si->image.alphaType = (si->flags & SHADER_ADDITIVE) ? IMAGE_ALPHA_ADDITIVE : IMAGE_ALPHA_NONE;
            }
            if (si->image.alphaType != IMAGE_ALPHA_NONE && si->image.alphaType != IMAGE_ALPHA_ADDITIVE &&
                ((si->image.width & (si->image.width - 1)) || (si->image.height & (si->image.height - 1))))
                Q3Map_Warning(1, "alpha image '%s' has dimensions %i x %i (not powers of 2), transparency will be incorrect\n",
                              si->image.name, si->image.width, si->image.height);
        }
        else
        {
            if (si->flags & SHADER_ADDITIVE)
                si->image.alphaType = IMAGE_ALPHA_ADDITIVE;
            else
                si->image.alphaType = IMAGE_ALPHA_NONE;
            si->flags &= ~SHADER_TRANSPARENT;
        }
    }
    return true;
}

/*
=============
ParseShader

Read surface lighting, sun settings and stages from one shader definition.
=============
*/
static bool ParseShader(const char *text, int shaderIndex, const char *textureName)
{
    char token[512];
    bool useTextureName = textureName != NULL;
    bool firstStage;
    ShaderInfo *si;
    unsigned int i;
    float values[6];              /* sun RGB, intensity, azimuth and elevation */
    float scale, maxColor;
    Vector3d direction;

    if (!useTextureName)
        SkipLine(&text);
    if (!GetToken(&text, token, sizeof(token), true) || token[0] != '{')
        return false;
    si = &shaders[shaderIndex];
    si->color = Color3f(1.0f, 1.0f, 1.0f);
    si->surfaceLight = 0;
    si->editorImage[0] = 0;
    si->lightImage[0] = 0;
    si->texture[0] = 0;
    si->image.name[0] = 0;
    firstStage = true;
    while (GetToken(&text, token, sizeof(token), true))
    {
        if (token[0] == '}')
            return LoadShaderImage(shaderIndex);
        if (token[0] == '{')
        {
            text = ParseStage(text, shaderIndex, textureName, firstStage, useTextureName);
            firstStage = false;
            continue;
        }
        if (!stricmp(token, "qer_editorimage"))
        {
            if (!GetToken(&text, si->editorImage, sizeof(si->editorImage), false))
                return false;
            SkipLine(&text);
            continue;
        }
        else if (!stricmp(token, "q3map_lightimage"))
        {
            if (!GetToken(&text, si->lightImage, sizeof(si->lightImage), false))
                return false;
            SkipLine(&text);
            continue;
        }
        else if (!stricmp(token, "surfaceparm"))
        {
            if (GetToken(&text, token, sizeof(token), false))
            {
                if (!stricmp(token, "sky"))
                    si->flags |= SHADER_SKY;
                else if (!stricmp(token, "noshadow"))
                    si->flags |= SHADER_NOSHADOW;
            }
            continue;
        }
        else if (!stricmp(token, "q3map_surfacelight"))
        {
            if (!GetToken(&text, token, sizeof(token), false))
                continue;
            si->surfaceLight = (float)atof(token);
            continue;
        }
        else if (!stricmp(token, "q3map_surfacecolor"))
        {
            if (!GetToken(&text, token, sizeof(token), false))
                continue;
            si->surfaceLight = (float)atof(token);
            continue;
        }
        else if (!stricmp(token, "q3map_sun"))
        {
            for (i = 0; i < 6; i++)
            {
                if (!GetToken(&text, token, sizeof(token), false))
                    break;
                values[i] = (float)atof(token);
            }
            if (i != 6)
                continue;
            if (!options.sunColorSet)
            {
                scale = (float)sqrt(values[0] * values[0] + values[1] * values[1] +
                                    values[2] * values[2]);
                if (scale != 0)
                {
                    float intensity = sunLightScale * values[3];
                    scale = intensity / scale;
                }
                Color3f color(values[0] * scale, values[1] * scale, values[2] * scale);
                maxColor = color.r > (color.g > color.b ? color.g : color.b) ?
                    color.r : (color.g > color.b ? color.g : color.b);
                if (maxColor != 0)
                    color /= maxColor;
                options.sunColorSet = true;
                options.sunColor = color;
                if (!options.sunLightSet)
                    options.sunLight = maxColor;
            }
            if (!options.sunDirectionSet)
            {
                options.sunDirectionSet = true;
                direction.x = cos(values[4] * (float)(3.14159265358979323846 / 180.0)) *
                              cos(values[5] * (float)(3.14159265358979323846 / 180.0));
                direction.y = sin(values[4] * (float)(3.14159265358979323846 / 180.0)) *
                              cos(values[5] * (float)(3.14159265358979323846 / 180.0));
                direction.z = sin(values[5] * (float)(3.14159265358979323846 / 180.0));
                options.sunDirection = direction;
            }
            continue;
        }
        else if (!stricmp(token, "polygonOffset"))
        {
            si->flags |= SHADER_POLYGON_OFFSET;
            continue;
        }
        SkipLine(&text);
    }
    return false;
}

/*
=============
Q3Map_LoadShader

Load a named shader or shader type, falling back to the material image.
=============
*/
bool Q3Map_LoadShader(const char *name, int shaderIndex)
{
    unsigned int length;
    int low;
    int high;
    int middle, comparison;
    char typeName[64];
    const char *textureName;
    const char *type;

    length = strlen(name);
    high = numShaderDefinitions - 1;
    low = 0;
    if ((unsigned int)shaderIndex >= MAX_SHADERS)
        Q3Map_Error("shadernum %i > MAX_SHADERS (%i)\n", shaderIndex, MAX_SHADERS);
    strcpy(shaders[shaderIndex].name, name);
    type = strrchr(name, '@');
    if (type)
    {
        void *buffer;
        bool result;
        textureName = name;
        if (*name)
        {
            const char *p = name;
            do
            {
                if (*p == '\\' || *p == '/')
                    textureName = p;
                p++;
            } while (*p);
        }
        if (textureName > type)
            Q3Map_Error("shader '%s' has a '/' after an '@'\n", name);
        else
            textureName++;
        if (shaderIndex >= FIRST_REQUESTABLE_SHADER)
            strcpy(typeName, "shadertypes/model/");
        else
            strcpy(typeName, "shadertypes/world/");
        int prefixLength = strlen(typeName);
        strncpy(typeName + prefixLength, textureName, type - textureName);
        strcpy(typeName + prefixLength + (type - textureName), ".stype");
        if (FS_ReadFile(typeName, &buffer) < 0)
            return false;
        result = ParseShader((const char *)buffer, shaderIndex, name);
        FS_FreeFile(buffer);
        return result;
    }
    while (low <= high)
    {
        middle = (low + high) >> 1;
        comparison = strnicmp(shaderDefinitions[middle], name, length);
        if (comparison < 0)
            low = middle + 1;
        else if (!comparison && isspace(shaderDefinitions[middle][length]))
            return ParseShader(shaderDefinitions[middle], shaderIndex, NULL);
        else
            high = middle - 1;
    }
    strcpy(shaders[shaderIndex].editorImage, name);
    if (LoadShaderImage(shaderIndex))
        return true;
    return false;
}

/*
=============
Q3Map_RequestShader

Reuse a requested shader or load it into the next free material slot.
=============
*/
short Q3Map_RequestShader(const char *name)
{
    short i;

    for (i = FIRST_REQUESTABLE_SHADER; i < MAX_SHADERS; i++)
    {
        if (!stricmp(shaders[i].name, name))
            return i;
    }
    if (numRequestableShaders == MAX_SHADERS)
        Q3Map_Error("insufficient requestable shaders (%i exceeded)\n", MAX_REQUESTABLE_SHADERS);
    i = numRequestableShaders;
    if (!Q3Map_LoadShader(name, i))
        return -1;
    numRequestableShaders++;
    return i;
}
