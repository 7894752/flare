/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Script memory tree: buddy allocator for small script allocations.
===========================================================================
*/
#ifndef __SCR_MEMORYTREE_H__
#define __SCR_MEMORYTREE_H__

#ifdef __cplusplus
extern "C" {
#endif

extern char *mt_buffer;

void			MT_Init( void );
void			MT_DumpTree( void );
unsigned short	MT_AllocIndex( int numBytes, int type );
void			MT_FreeIndex( unsigned short nodeNum, int numBytes );
void			*MT_Alloc( int numBytes, int type );
void			MT_Free( void *p, int numBytes );

#ifdef __cplusplus
}
#endif

#endif
