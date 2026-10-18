/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Inline ColorRGB member functions.
===========================================================================
*/
#ifndef FLARE_COLOR_H
#define FLARE_COLOR_H

#include "flare/math_types.h"

/* Clear the three lighting color channels. */

inline void ColorRGB::Clear()
{
    r = 0;
    g = 0;
    b = 0;
}

/* Weighted RGB luminance. */

inline float ColorRGB::Luminance() const
{
    return r * 0.299f + g * 0.587f + b * 0.114f;
}

#endif
