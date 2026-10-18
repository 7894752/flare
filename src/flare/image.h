/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

RGBA pixel records used by the image decoders.
===========================================================================
*/
#ifndef FLARE_IMAGE_H
#define FLARE_IMAGE_H

#include "flare/shaders.h"
#include "common/q_round.h"
#include "flare/geometry.h"

/* One decoded pixel, in red, green, blue, alpha order. */
struct Rgba8
{
    unsigned char r, g, b, a;

    Rgba8() {}
};

Color3f FilterTexture(ImageRecord *image, const Color3f &color, const vec2_t *points, int count);

#endif
