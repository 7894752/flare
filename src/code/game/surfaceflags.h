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

/* Surface and contents flags stored in BSP records. */

// A brush can contribute multiple contents bits.

#define	CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	CONTENTS_WATER			32


// Call of Duty contents; some bits have different meanings in Quake III.
#define CONTENTS_NONCOLLIDING	0x0004

#define	CONTENTS_AREAPORTAL		0x8000

#define	CONTENTS_TRANSLUCENT	0x20000000	// don't consume surface fragments inside

#define	SURF_SKY				0x4		// lighting from environment map
#define	SURF_NODRAW				0x80	// don't generate a drawsurface at all
#define	SURF_ALPHASHADOW		0x10000	// do per-pixel light shadow casting in q3map
#define SURF_CASTSHADOW			0x40000	// "castshadow" in the surfaceparm table
