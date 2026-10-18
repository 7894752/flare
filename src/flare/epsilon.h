/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Shared distance and relative-normal tolerances for geometry operations.
===========================================================================
*/
#ifndef FLARE_EPSILON_H
#define FLARE_EPSILON_H

static const struct
{
    double equal, normal;
} epsilonPair = { 0.001, 0.00001 };
#define EQUAL_EPSILON  (epsilonPair.equal)
#define NORMAL_EPSILON (epsilonPair.normal)

#endif
