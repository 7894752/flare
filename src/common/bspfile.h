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

/* BSP input, output, lump storage and entity key/value access (bspfile.c). */
#ifndef BSPFILE_H
#define BSPFILE_H

#include "../code/qcommon/qfiles.h"

#include "cod_bsp_load_storage.h"

void	LoadBSPFile( const char *filename );
void	WriteBSPFile( const char *filename );

/* Entity key/value access (bspfile.c). */

typedef struct epair_s {
	struct epair_s	*next;
	char	*key;
	char	*value;
} epair_t;

typedef struct {
	vec3_t		origin;
	struct bspbrush_s	*brushes;
	struct parseMesh_s	*patches;
	struct terrainSurface_s *terrainSurfaces;	// intermediate terrain meshes, emitted by SubdivideTerrain
	int			firstDrawSurf;
	epair_t		*epairs;
} entity_t;

extern	int			num_entities;
extern	entity_t	entities[MAX_MAP_ENTITIES];

void	ParseEntities( void );
void	UnparseEntities( void );

void 	SetKeyValue( entity_t *ent, const char *key, const char *value );
/* Returns an empty string when the key is absent. */
const char 	*ValueForKey( const entity_t *ent, const char *key );

// Cursor-based CoD parser, distinct from the legacy global-token parser.
epair_t *ParseEpair( char *key, char **data_p );

#endif
