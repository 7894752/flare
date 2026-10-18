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

/* Image decoding, resampling and alpha filtering. */
#define MAX_JPEG_SCANLINE 4096
#define TGA_HEADER_SIZE 18
#define TGA_TRUECOLOR 2
#define DXT_BLOCK_SIZE 4
#define DDS_MAGIC 0x20534444
#define DDS_HEADER_SIZE 124
#define DDS_DATA_OFFSET 128
#define DDS_FOURCC_DXT1 0x31545844
#define DDS_FOURCC_DXT3 0x33545844
#define DDS_FOURCC_DXT5 0x35545844
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define TEXTURE_CLIP_BUFFER_SIZE 516

/* These names describe light transmission through rejected shader pixels.
 * ImageRecord::alphaType modes 2/3/4 select opaque GT0/GE128/LT128 tests;
 * these callbacks collect alpha == 0, < 128 and >= 128, respectively. */
#define IMAGE_ALPHA_BLEND 1
#define IMAGE_ALPHA_ZERO 2
#define IMAGE_ALPHA_LESS_128 3
#define IMAGE_ALPHA_AT_LEAST_128 4

#include "flare/optiondefs.h"
#include "flare/color.h"
#include <stdio.h>
#include <string.h>
#include "flare/image.h"
#include "flare/print.h"
#include "code/qcommon/qcommon.h"
#include "flare/geometry.h"

extern "C" {
#include "libs/jpeg6b/jpeglib.h"
}

/*
=============
ExpandBgr24

TGA rows are stored bottom first, with blue preceding red.
=============
*/
static void ExpandBgr24(const unsigned char *source, unsigned char *pixels, int width, int height) {
    unsigned char *dest = pixels + (height - 1) * width * 4;
    while (dest >= pixels) {
        int count = width;
        do {
            dest[0] = source[2];
            dest[1] = source[1];
            dest[2] = source[0];
            dest[3] = 255;
            source += 3;
            dest += 4;
        } while (--count);
        dest -= width * 8;
    }
}

/*
=============
ExpandBgra32
=============
*/
static bool ExpandBgra32(const unsigned char *source, int width, unsigned char *pixels, int height) {
    bool hasAlpha = false;
    unsigned char *dest = pixels + (height - 1) * width * 4;
    while (dest >= pixels) {
        int count = width;
        do {
            dest[0] = source[2];
            dest[1] = source[1];
            dest[2] = source[0];
            dest[3] = source[3];
            hasAlpha |= source[3] != 255;
            source += 4;
            dest += 4;
        } while (--count);
        dest -= width * 8;
    }
    return hasAlpha;
}

/*
=============
Img_LoadJpg
=============
*/
bool Img_LoadJpg(ImageRecord *image, unsigned char *data, int length) {
    jpeg_decompress_struct cinfo;
    jpeg_error_mgr jerr;
    unsigned char scanline[MAX_JPEG_SCANLINE];
    JSAMPROW row;
    unsigned char *pixels, *dest;
    int width, height, i;

    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, data, length);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    if (cinfo.output_components != 3 || cinfo.output_height <= 0 || cinfo.output_width <= 0)
        return false;
    if (cinfo.output_width > MAX_JPEG_SCANLINE) {
        Q3Map_Warning(1, "Img_LoadJpg: image width %i > %i\n", cinfo.output_width, MAX_JPEG_SCANLINE);
        return false;
    }
    width = cinfo.output_width;
    height = cinfo.output_height;
    pixels = new unsigned char[width * height * 4];
    if (!pixels)
        Q3Map_Error("Img_LoadJpg: couldn't allocate pixel buffer (%i bytes)\n", width * height * 4);
    dest = pixels;
    while (cinfo.output_scanline < cinfo.output_height) {
        row = scanline;
        jpeg_read_scanlines(&cinfo, &row, 1);
        for (i = 0; i < width; i++) {
            dest[0] = row[i * 3];
            dest[1] = row[i * 3 + 1];
            dest[2] = row[i * 3 + 2];
            dest[3] = 255;
            dest += 4;
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    image->pixels = pixels;
    image->width = width;
    image->height = height;
    image->hasAlpha = false;
    return true;
}

/* TGA's little-endian dimensions are stored as byte pairs. */
struct TgaHeader {
    unsigned char idLength, colorMapType, imageType;
    unsigned char colorMapFirstIndex[2], colorMapLength[2], colorMapEntrySize;
    unsigned char xOrigin[2], yOrigin[2], width[2], height[2];
    unsigned char bitsPerPixel, imageDescriptor;
};

/*
=============
Img_LoadTga
=============
*/
static bool Img_LoadTga(ImageRecord *image, const unsigned char *data, int length) {
    const TgaHeader *header = (const TgaHeader *)data;
    int idLength = header->idLength;
    int width = *(const unsigned short *)header->width;
    int height = *(const unsigned short *)header->height;
    int imageType = header->imageType;
    int bitsPerPixel = header->bitsPerPixel;
    int bottomUp = ~(header->imageDescriptor >> 5) & 1;
    unsigned char *pixels;
    int size;
    if (!bottomUp) {
        Q3Map_Warning(1, "Img_LoadTga: cannot read top-down TGAs\n");
        return false;
    }
    if (imageType == TGA_TRUECOLOR) {
        if (bitsPerPixel == 24) {
            size = width * height * 4;
            pixels = new unsigned char[size];
            if (!pixels)
                Q3Map_Error("Img_LoadTga: couldn't allocate pixel buffer (%i bytes)\n", size);
            ExpandBgr24(data + TGA_HEADER_SIZE + idLength, pixels, width, height);
            image->hasAlpha = false;
        } else if (bitsPerPixel == 32) {
            size = width * height * 4;
            pixels = new unsigned char[size];
            if (!pixels)
                Q3Map_Error("Img_LoadTga: couldn't allocate pixel buffer (%i bytes)\n", size);
            image->hasAlpha = ExpandBgra32(data + TGA_HEADER_SIZE + idLength, width, pixels, height);
        } else {
            Q3Map_Warning(1, "Img_LoadTga: unsupported bits-per-pixel %i\n", bitsPerPixel);
            return false;
        }
        image->pixels = pixels;
        image->height = height;
        image->width = width;
        return true;
    }
    Q3Map_Warning(1, "Img_LoadTga: unsupported subtype %i\n", imageType);
    return false;
}

/*
=============
DecodeDxtColor
=============
*/
static void DecodeDxtColor(ImageRecord *image, const unsigned char *data, int x, int y, bool fourColors) {
    unsigned short c0 = *(const unsigned short *)data;
    float red[2], green[2], blue[2];
    red[0] = (c0 >> 11) * (1.0f / 32.0f);
    green[0] = ((c0 >> 5) & 63) * (1.0f / 64.0f);
    blue[0] = (c0 & 31) * (1.0f / 32.0f);
    unsigned short c1 = *(const unsigned short *)(data + 2);
    red[1] = (c1 >> 11) * (1.0f / 32.0f);
    green[1] = ((c1 >> 5) & 63) * (1.0f / 64.0f);
    blue[1] = (c1 & 31) * (1.0f / 32.0f);
    Rgba8 colors[4];
    if (!fourColors && c0 <= c1) {
        colors[0].r = RoundToInt(red[0] * 255.0f);
        colors[0].g = RoundToInt(green[0] * 255.0f);
        colors[0].b = RoundToInt(blue[0] * 255.0f);
        colors[0].a = 255;
        colors[1].r = RoundToInt(red[1] * 255.0f);
        colors[1].g = RoundToInt(green[1] * 255.0f);
        colors[1].b = RoundToInt(blue[1] * 255.0f);
        colors[1].a = 255;
        colors[2].r = RoundToInt((red[0] + red[1]) * 0.5f * 255.0f);
        colors[2].g = RoundToInt((green[0] + green[1]) * 0.5f * 255.0f);
        colors[2].b = RoundToInt((blue[0] + blue[1]) * 0.5f * 255.0f);
        colors[2].a = 255;
        colors[3].r = 0;
        colors[3].g = 0;
        colors[3].b = 0;
        colors[3].a = 0;
    } else {
        colors[0].r = RoundToInt(red[0] * 255.0f);
        colors[0].g = RoundToInt(green[0] * 255.0f);
        colors[0].b = RoundToInt(blue[0] * 255.0f);
        colors[0].a = 255;
        colors[1].r = RoundToInt(red[1] * 255.0f);
        colors[1].g = RoundToInt(green[1] * 255.0f);
        colors[1].b = RoundToInt(blue[1] * 255.0f);
        colors[1].a = 255;
        colors[2].r = RoundToInt(red[0] * 170.0f + red[1] * 85.0f);
        colors[2].g = RoundToInt(green[0] * 170.0f + green[1] * 85.0f);
        colors[2].b = RoundToInt(blue[0] * 170.0f + blue[1] * 85.0f);
        colors[2].a = 255;
        colors[3].r = RoundToInt(red[0] * 85.0f + red[1] * 170.0f);
        colors[3].g = RoundToInt(green[0] * 85.0f + green[1] * 170.0f);
        colors[3].b = RoundToInt(blue[0] * 85.0f + blue[1] * 170.0f);
        colors[3].a = 255;
    }
    for (int row = 0; row < DXT_BLOCK_SIZE; row++) {
        ((Rgba8 *)image->pixels)[(y + row) * image->width + x] = colors[data[4 + row] & 3];
        ((Rgba8 *)image->pixels)[(y + row) * image->width + x + 1] = colors[(data[4 + row] >> 2) & 3];
        ((Rgba8 *)image->pixels)[(y + row) * image->width + x + 2] = colors[(data[4 + row] >> 4) & 3];
        ((Rgba8 *)image->pixels)[(y + row) * image->width + x + 3] = colors[data[4 + row] >> 6];
    }
}

/*
=============
DecodeDxt3
=============
*/
static void DecodeDxt3(ImageRecord *image, const unsigned char *data, int x, int y) {
    DecodeDxtColor(image, data + 8, x, y, true);
    for (int row = 0; row < DXT_BLOCK_SIZE; row++) {
        image->pixels[4 * ((y + row) * image->width + x) + 3] = (data[row * 2] & 15) * 17;
        image->pixels[4 * ((y + row) * image->width + x + 1) + 3] = (data[row * 2] >> 4) * 17;
        image->pixels[4 * ((y + row) * image->width + x + 2) + 3] = (data[row * 2 + 1] & 15) * 17;
        image->pixels[4 * ((y + row) * image->width + x + 3) + 3] = (data[row * 2 + 1] >> 4) * 17;
    }
}

/*
=============
DecodeDxt5
=============
*/
static void DecodeDxt5(ImageRecord *image, const unsigned char *data, int x, int y) {
    DecodeDxtColor(image, data + 8, x, y, true);
    unsigned char alpha[8];
    alpha[0] = data[0];
    alpha[1] = data[1];
    if (data[0] > data[1]) {
        alpha[2] = RoundToInt((6 * data[0] + data[1]) * (1.0f / 7.0f));
        alpha[3] = RoundToInt((5 * data[0] + 2 * data[1]) * (1.0f / 7.0f));
        alpha[4] = RoundToInt((4 * data[0] + 3 * data[1]) * (1.0f / 7.0f));
        alpha[5] = RoundToInt((3 * data[0] + 4 * data[1]) * (1.0f / 7.0f));
        alpha[6] = RoundToInt((2 * data[0] + 5 * data[1]) * (1.0f / 7.0f));
        alpha[7] = RoundToInt((data[0] + 6 * data[1]) * (1.0f / 7.0f));
    } else {
        alpha[2] = RoundToInt((4 * data[0] + data[1]) * (1.0f / 5.0f));
        alpha[3] = RoundToInt((3 * data[0] + 2 * data[1]) * (1.0f / 5.0f));
        alpha[4] = RoundToInt((2 * data[0] + 3 * data[1]) * (1.0f / 5.0f));
        alpha[5] = RoundToInt((data[0] + 4 * data[1]) * (1.0f / 5.0f));
        alpha[6] = 0;
        alpha[7] = 255;
    }
    int bits = data[2] | (data[3] << 8);
    int shift = 0;
    unsigned int nextByte = 2;
    for (int row = 0; row < DXT_BLOCK_SIZE; row++) {
        for (int column = 0; column < DXT_BLOCK_SIZE; column++) {
            image->pixels[4 * ((y + row) * image->width + x + column) + 3] = alpha[(bits >> shift) & 7];
            shift += 3;
            if (shift >= 8) {
                shift -= 8;
                bits >>= 8;
                if (nextByte < 6)
                    bits |= data[2 + nextByte++] << 8;
            }
        }
    }
}

/* DDS_HEADER and DDS_PIXELFORMAT as stored in the file. */
struct DdsPixelFormat {
    unsigned int size, flags, fourCC, rgbBits, rMask, gMask, bMask, aMask;
};
struct DdsHeader {
    unsigned int size, flags, height, width, pitch, depth, mipCount;
    unsigned int reserved[11];
    DdsPixelFormat pixelFormat;
    unsigned int caps, caps2, caps3, caps4, reserved2;
};

/*
=============
Img_LoadDds
=============
*/
static bool Img_LoadDds(ImageRecord *image, const unsigned char *data, unsigned int length) {
    if (length <= 4)
        return false;
    if (*(const unsigned int *)data != DDS_MAGIC)
        return false;
    const DdsHeader *header = (const DdsHeader *)(data + 4);
    int format, blockSize;
    switch (header->pixelFormat.fourCC) {
    case DDS_FOURCC_DXT1:
        format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        blockSize = 8;
        break;
    case DDS_FOURCC_DXT3:
        format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        blockSize = 16;
        break;
    case DDS_FOURCC_DXT5:
        format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        blockSize = 16;
        break;
    default:
        return false;
    }
    if (header->size != DDS_HEADER_SIZE)
        return false;
    if (length < DDS_DATA_OFFSET + ((header->width + 3) / 4) * ((header->height + 3) / 4) * blockSize)
        return false;
    image->width = header->width;
    image->height = header->height;
    image->hasAlpha = true;
    image->pixels = new unsigned char[image->width * image->height * 4];
    switch (format) {
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: {
        const unsigned char *block = data + DDS_DATA_OFFSET;
        int x, y;
        for (y = 0; y < image->height; y += DXT_BLOCK_SIZE)
            for (x = 0; x < image->width; x += DXT_BLOCK_SIZE, block += 8)
                DecodeDxtColor(image, block, x, y, false);
        break;
    }
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: {
        const unsigned char *block = data + DDS_DATA_OFFSET;
        int x, y;
        for (y = 0; y < image->height; y += DXT_BLOCK_SIZE)
            for (x = 0; x < image->width; x += DXT_BLOCK_SIZE, block += 16)
                DecodeDxt3(image, block, x, y);
        break;
    }
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: {
        const unsigned char *block = data + DDS_DATA_OFFSET;
        int x, y;
        for (y = 0; y < image->height; y += DXT_BLOCK_SIZE)
            for (x = 0; x < image->width; x += DXT_BLOCK_SIZE, block += 16)
                DecodeDxt5(image, block, x, y);
        break;
    }
    }
    return true;
}


/*
=============
LoadImage
=============
*/
bool LoadImage(const char *filename, ImageRecord *image) {
    char name[512];
    void *data;
    int length;
    bool success;
    Com_StripExtension(filename, name);
    char *extension = name + strlen(name);
    strcpy(extension, ".dds");
    length = FS_ReadFile(name, &data);
    if (length >= 0) {
        success = Img_LoadDds(image, (const unsigned char *)data, length);
        FS_FreeFile(data);
        if (success) {
            strcpy(image->name, name);
            return true;
        }
    }
    strcpy(extension, ".tga");
    length = FS_ReadFile(name, &data);
    if (length >= 0) {
        success = Img_LoadTga(image, (const unsigned char *)data, length);
        FS_FreeFile(data);
        if (success) {
            strcpy(image->name, name);
            return true;
        }
    }
    strcpy(extension, ".jpg");
    length = FS_ReadFile(name, &data);
    if (length >= 0) {
        success = Img_LoadJpg(image, (unsigned char *)data, length);
        FS_FreeFile(data);
        if (success) {
            strcpy(image->name, name);
            return true;
        }
    }
    return false;
}

typedef float vec2_t[2];
ColorRGB textureFilterColor = {0, 0, 0};
float textureFilterArea = 0.0f;
ImageRecord *textureFilterImage = 0;



/*
=============
FilterTextureAlphaBlend
=============
*/
void FilterTextureAlphaBlend(float area, const float *st, const vec2_t *points, int count)
{
    int x = FloorToInt(st[0]) & (textureFilterImage->width - 1);
    int y = FloorToInt(st[1]) & (textureFilterImage->height - 1);
    const unsigned char *pixel = &textureFilterImage->pixels[4 * (y * textureFilterImage->width + x)];
    float alpha = pixel[3] * (1.0f / 255.0f);
    float weight = area * (1.0f - alpha);
    float clear = (area * (1.0f - alpha)) * (1.0f - alpha);
    float scale = weight * alpha * (1.0f / 255.0f);
    Color3f color;
    color.r = pixel[0];
    textureFilterColor.r = textureFilterColor.r + color.r * scale + clear;
    color.g = pixel[1];
    textureFilterColor.g = textureFilterColor.g + color.g * scale + clear;
    color.b = pixel[2];
    textureFilterColor.b = textureFilterColor.b + color.b * scale + clear;
    textureFilterArea += area;
}

/*
=============
FilterTextureAlphaZero
=============
*/
void FilterTextureAlphaZero(float area, const float *st, const vec2_t *points, int count)
{
    int x = FloorToInt(st[0]) & (textureFilterImage->width - 1);
    int y = FloorToInt(st[1]) & (textureFilterImage->height - 1);
    if (textureFilterImage->pixels[4 * (y * textureFilterImage->width + x) + 3] == 0) {
        textureFilterColor.r += area;
        textureFilterColor.g += area;
        textureFilterColor.b += area;
    }
    textureFilterArea += area;
}

/*
=============
FilterTextureAlphaLess128
=============
*/
void FilterTextureAlphaLess128(float area, const float *st, const vec2_t *points, int count)
{
    int x = FloorToInt(st[0]) & (textureFilterImage->width - 1);
    int y = FloorToInt(st[1]) & (textureFilterImage->height - 1);
    if (textureFilterImage->pixels[4 * (y * textureFilterImage->width + x) + 3] < 128) {
        textureFilterColor.r += area;
        textureFilterColor.g += area;
        textureFilterColor.b += area;
    }
    textureFilterArea += area;
}

/*
=============
FilterTextureAlphaAtLeast128
=============
*/
void FilterTextureAlphaAtLeast128(float area, const float *st, const vec2_t *points, int count)
{
    int x = FloorToInt(st[0]) & (textureFilterImage->width - 1);
    int y = FloorToInt(st[1]) & (textureFilterImage->height - 1);
    if (textureFilterImage->pixels[4 * (y * textureFilterImage->width + x) + 3] >= 128) {
        textureFilterColor.r += area;
        textureFilterColor.g += area;
        textureFilterColor.b += area;
    }
    textureFilterArea += area;
}

/*
=============
FilterTextureCentroid
=============
*/
static Color3f FilterTextureCentroid(ImageRecord *image, const Color3f &color,
                                    const vec2_t *points, int count, PolygonCellCallback callback)
{
    vec2_t st = {0, 0};
    int i;
    for (i = 0; i < count; i++) {
        st[0] += points[i][0];
        st[1] += points[i][1];
    }
    st[0] = (float)FloorToInt(image->width * st[0] / (float)count);
    st[1] = (float)FloorToInt(image->height * st[1] / (float)count);
    callback(1.0f, st, 0, 0);
    return textureFilterColor * color;
}

/*
=============
FilterTexturePolygon
=============
*/
static Color3f FilterTexturePolygon(ImageRecord *image, const Color3f &color,
                                   const vec2_t *points, int count, PolygonCellCallback callback)
{
    vec2_t buffers[4][TEXTURE_CLIP_BUFFER_SIZE];
    float width = (float)image->width;
    vec2_t mins, maxs;
    int i;
    buffers[0][0][0] = width * points[0][0];
    float height = (float)image->height;
    buffers[0][0][1] = height * points[0][1];
    mins[0] = buffers[0][0][0];
    mins[1] = buffers[0][0][1];
    maxs[0] = buffers[0][0][0];
    maxs[1] = buffers[0][0][1];
    for (i = 1; i < count; i++) {
        buffers[0][i][0] = width * points[i][0];
        if (mins[0] > buffers[0][i][0])
            mins[0] = buffers[0][i][0];
        else if (maxs[0] < buffers[0][i][0])
            maxs[0] = buffers[0][i][0];
        buffers[0][i][1] = height * points[i][1];
        if (mins[1] > buffers[0][i][1])
            mins[1] = buffers[0][i][1];
        else if (maxs[1] < buffers[0][i][1])
            maxs[1] = buffers[0][i][1];
    }
    int x = FloorToInt(mins[0]);
    int columns = FloorToInt(maxs[0]) - x + 1;
    int y = FloorToInt(mins[1]);
    int rows = FloorToInt(maxs[1]) - y + 1;
    if (columns > options.maxAlphaPixels || rows > options.maxAlphaPixels ||
        columns * rows > options.maxAlphaPixels)
        return FilterTextureCentroid(image, color, points, count, callback);
    SubdividePolygon2D(buffers, count, columns, rows, (float)x, (float)y, 1.0f, 1.0f, callback);
    if (textureFilterArea > 0)
        return (textureFilterColor / textureFilterArea) * color;
    return color;
}

/*
=============
FilterTexture
=============
*/
Color3f FilterTexture(ImageRecord *image, const Color3f &color, const vec2_t *points, int count)
{
    textureFilterColor.Clear();
    textureFilterArea = 0;
    textureFilterImage = image;
    PolygonCellCallback callback = 0;
    switch (image->alphaType) {
    case IMAGE_ALPHA_BLEND:
        callback = FilterTextureAlphaBlend;
        break;
    case IMAGE_ALPHA_ZERO:
        callback = FilterTextureAlphaZero;
        break;
    case IMAGE_ALPHA_LESS_128:
        callback = FilterTextureAlphaLess128;
        break;
    case IMAGE_ALPHA_AT_LEAST_128:
        callback = FilterTextureAlphaAtLeast128;
        break;
    }
    if (options.fastAlphaCheck)
        return FilterTextureCentroid(image, color, points, count, callback);
    return FilterTexturePolygon(image, color, points, count, callback);
}
