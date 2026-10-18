/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Common engine services: printing, errors, memory and file access.
===========================================================================
*/
#ifndef __QCOMMON_H__
#define __QCOMMON_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fileData_s {
	char *name;
	void *data;
	void ( *clearCallback )( struct fileData_s * );
} fileData_t;

void		Com_Printf( const char *format, ... );
void		Com_DPrintf( const char *format, ... );
void		Com_Error( int code, const char *format, ... );
void		Com_Memset( void *dest, int val, size_t count );
void		Com_Memcpy( void *dest, const void *src, size_t count );
void		Com_StripExtension( const char *in, char *out );

int			FS_ReadFile( const char *qpath, void **buffer );
void		FS_FreeFile( void *buffer );
char		**FS_ListFiles( const char *path, const char *extension, int *numfiles );
fileData_t	*FS_GetDataForFile( const char *dir, const char *name, const char *ext );

void		AngleVectors( const float *angles, float *forward, float *right, float *up );
void		QuatMultiply( const float *a, const float *b, float *out );
short		LittleShort( short l );
int			LittleLong( int l );
float		LittleFloat( float l );

#ifdef __cplusplus
}
#endif

#endif
