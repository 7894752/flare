/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

File path and extension helpers for command-line tools.
===========================================================================
*/

/* File-name extraction shared by the command-line tools (libs/cmdlib/cmdlib.cpp). */
#ifndef FLARE_CMDLIB_PATH_H
#define FLARE_CMDLIB_PATH_H

#ifdef __cplusplus
extern "C"
{
#endif

void ExtractFileName( const char *path, char *dest );

#ifdef __cplusplus
}
#endif

#endif
