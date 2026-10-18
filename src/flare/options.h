/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Lighting option values and shared option declarations.
===========================================================================
*/
#ifndef FLARE_OPTIONS_H
#define FLARE_OPTIONS_H

#include "flare/mathlib.h"
#include "optiondefs.h"

void Preview_SetCvarInteger(const char *name, int value);
void ExecuteConfigLine( const char *text );
float Preview_CvarFloat( const char *name );

#endif
