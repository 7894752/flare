/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Shader flags, decoded images and material records.
===========================================================================
*/
#ifndef FLARE_SHADERS_H
#define FLARE_SHADERS_H

#include "flare/mathlib.h"

#ifndef FLARE_LAYOUT
#define FLARE_LAYOUT(name, cond) typedef char flare_layout_##name[(cond) ? 1 : -1]
#define FLARE_OFFSET(type, member) ((int)(&((type *)0)->member))
#endif

/* Material flags set by shader stages and surface parameters. */
enum
{
    SHADERFLAG_0x01 = 0x01,         /* surfaceparm sky */
    SHADERFLAG_ALPHA_TEST = 0x02,   /* alphaFunc GT0; combined with 0x04 for LT128 */
    SHADERFLAG_ALPHA_BLEND = 0x04,  /* alphaFunc GE128 */
    SHADERFLAG_0x08 = 0x08,         /* translucent blending */
    SHADERFLAG_0x10 = 0x10,         /* additive blending */
    SHADERFLAG_0x20 = 0x20,         /* polygonOffset */
    SHADERFLAG_0x40 = 0x40          /* surfaceparm noshadow */
};

/* Decoded image owned by a shader. */
struct ImageRecord
{
    char name[64];                  /* file name without extension */
    int alphaType;                  /* 0 opaque, 1 blend, 2 GT0, 3 GE128, 4 LT128, 5 additive;
                                      image transmission filters use the complementary alpha tests */
    unsigned char channels;         /* image channel selection */
    bool hasAlpha;
    int width;
    int height;
    unsigned char *pixels;          /* RGBA bytes */
};
FLARE_LAYOUT(ImageRecord_size, sizeof(ImageRecord) == 0x54);
FLARE_LAYOUT(ImageRecord_members, FLARE_OFFSET(ImageRecord, alphaType) == 0x40 && FLARE_OFFSET(ImageRecord, hasAlpha) == 0x45
                                      && FLARE_OFFSET(ImageRecord, width) == 0x48 && FLARE_OFFSET(ImageRecord, pixels) == 0x50);

struct ShaderInfo
{
    char name[64];
    unsigned int flags;             /* SHADERFLAG_* */
    float surfaceLight;             /* q3map_surfacelight */
    ColorRGB color;
    ColorRGB averageColor;
    char editorImage[64];
    char lightImage[64];
    char texture[64];
    ImageRecord image;
};
FLARE_LAYOUT(ShaderInfo_size, sizeof(ShaderInfo) == 0x174);
FLARE_LAYOUT(ShaderInfo_members, FLARE_OFFSET(ShaderInfo, flags) == 0x40 && FLARE_OFFSET(ShaderInfo, color) == 0x48
                                     && FLARE_OFFSET(ShaderInfo, averageColor) == 0x54
                                     && FLARE_OFFSET(ShaderInfo, editorImage) == 0x60 && FLARE_OFFSET(ShaderInfo, lightImage) == 0xA0
                                     && FLARE_OFFSET(ShaderInfo, texture) == 0xE0 && FLARE_OFFSET(ShaderInfo, image) == 0x120
                                     && FLARE_OFFSET(ShaderInfo, image.alphaType) == 0x160
                                     && FLARE_OFFSET(ShaderInfo, image.width) == 0x168
                                     && FLARE_OFFSET(ShaderInfo, image.pixels) == 0x170);

#define MAX_SHADERS 4096
/* Shader table. */
extern ShaderInfo shaders[MAX_SHADERS];

/* Image decoding. */
bool LoadImage(const char *filename, ImageRecord *image);

void Q3Map_InitShaders(void);
bool Q3Map_LoadShader(const char *name, int index);
short Q3Map_RequestShader(const char *name);
extern ShaderInfo shaders[4096];

#endif
