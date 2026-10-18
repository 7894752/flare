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

/* Call of Duty BSP limits and disk records. */

#ifndef __QFILES_H__
#define __QFILES_H__

// the maximum size of game relative pathnames
#define	MAX_QPATH		64

// there shouldn't be any problem with increasing these values at the
// expense of more memory allocation in the utilities
#define	MAX_MAP_MODELS		0x800
#define	MAX_MAP_BRUSHES		0x8000
#define	MAX_MAP_ENTITIES	0x2000
#define	MAX_MAP_ENTSTRING	0x100000
#define	MAX_MAP_SHADERS		0x400

#define	MAX_MAP_FOGS		0x100
#define	MAX_MAP_PLANES		0x40000
#define	MAX_MAP_NODES		0x20000
#define	MAX_MAP_BRUSHSIDES	0x28000
#define	MAX_MAP_LEAFS		0x20000
#define	MAX_MAP_LEAFFACES	0x20000
#define	MAX_MAP_LEAFBRUSHES 0x40000
#define	MAX_MAP_LIGHTING	0x1000000
#define	MAX_MAP_VISIBILITY	0x200000

#define	MAX_MAP_DRAW_VERTS	0x80000
#define	MAX_MAP_DRAW_INDEXES	0x80000

// CoD BSP storage limits.
#define MAX_MAP_TRISOUPS             0x20000
#define MAX_MAP_CELLS                0x1000
#define MAX_MAP_CULLGROUPS           0x800
#define MAX_MAP_CULLGROUP_INDEXES    0x1000
#define MAX_MAP_BSP_PORTALS          0x2000
#define MAX_MAP_PORTAL_VERTS         0x10000
#define MAX_MAP_LIGHTS               0x800
#define MAX_MAP_LIGHT_INDEXES        0x10000
#define MAX_MAP_COLLISION_INDEXES    0x40000
#define MAX_MAP_OCCLUDERS            0x1000
#define MAX_MAP_OCCLUDER_EDGES        0x10000
#define MAX_MAP_OCCLUDER_PLANES       0x8000
#define MAX_MAP_AABBTREES            0x100000

// key / value pair sizes in the entities lump
#define	MAX_KEY				32
#define	MAX_VALUE			1024

//=============================================================================


// CoD lump directory entries store length before offset. The CoD lump numbers
// are COD_LUMP_* in common/cod_bsp_lump_indices.h.
typedef struct {
	int		filelen, fileofs;
} lump_t;

#define	HEADER_LUMPS		33

typedef struct {
	int			ident;
	int			version;

	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct {
	float		mins[3], maxs[3];
	int			firstSurface, numSurfaces;
	int			firstLeafSurface, numLeafSurfaces;
	int			firstBrush, numBrushes;
} dmodel_t;

typedef struct {
	char		shader[MAX_QPATH];
	int			surfaceFlags;
	int			contentFlags;
} dshader_t;

// Plane x^1 is always the opposite of plane x.

typedef struct {
	float		normal[3];
	float		dist;
} dplane_t;

typedef struct {
	int			planeNum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	int			mins[3];		// for frustom culling
	int			maxs[3];
} dnode_t;

typedef struct {
	int			cluster;
	int			area;
	int			firstLeafSurface;
	int			numLeafSurfaces;
	int			firstLeafBrush;
	int			numLeafBrushes;
	int			cellIndex;
	int			firstLightIndex;
	int			numLightIndexes;
} dleaf_t;

typedef union {
	int			planeNum;
	float		planeDist;
} dbrushsidePlane_t;

typedef struct {
	dbrushsidePlane_t plane;	// axial sides store distance; others store plane index
	int			shaderNum;
} dbrushside_t;

typedef struct {
	short		numSides;
	short		shaderNum;		// the shader that determines the contents flags
} dbrush_t;

typedef struct {
	char		shader[MAX_QPATH];
	int			brushNum;
	int			visibleSide;	// the brush side that ray tests need to clip against (-1 == none)
} dfog_t;

typedef struct {
	vec3_t		xyz;
	float		st[2];
	float		lightmap[2];
	vec3_t		normal;
	byte		color[4];
} drawVert_t;

#endif
