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

/* Vector construction and angle conversion. */
#ifndef Q3MAP_Q_SHARED_VECTORS_H
#define Q3MAP_Q_SHARED_VECTORS_H

#include "q_shared.h"

#define VectorSet( v, x, y, z ) ( (v)[0] = (x), (v)[1] = (y), (v)[2] = (z) )

void AngleVectors( const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up );

#endif
