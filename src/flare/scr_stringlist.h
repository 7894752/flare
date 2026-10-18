/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Script string list: interned, reference-counted strings.
===========================================================================
*/
#ifndef __SCR_STRINGLIST_H__
#define __SCR_STRINGLIST_H__

#ifdef __cplusplus
extern "C" {
#endif

void			SL_Init( void );
const char		*SL_ConvertToString( unsigned short stringValue );
unsigned short	SL_FindString( const char *str );
unsigned short	SL_FindLowercaseString( const char *str );
unsigned short	SL_GetStringOfLen( const char *str, unsigned int user, unsigned int len, int type );
unsigned short	SL_GetString_( const char *str, unsigned int user, int type );
unsigned short	SL_GetString( const char *str, unsigned int user );
void			SL_RemoveRefToString( unsigned short stringValue );
void			SL_RemoveRefToStringOfLen( unsigned short stringValue, unsigned int len );

#ifdef __cplusplus
}
#endif

#endif
