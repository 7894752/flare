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

/* Common engine diagnostics and fatal error handling. */
#define MAX_PRINT_MESSAGE 65536

#include <stdarg.h>
#include <stdio.h>
#include "flare/mathlib.h"
#include "flare/print.h"
#include "code/qcommon/qcommon.h"

/*
=============
Com_Error

Route engine errors through the compiler's fatal-error handler.
=============
*/
void Com_Error(int code, const char *format, ...)
{
    va_list argptr;
    char text[MAX_PRINT_MESSAGE];

    va_start(argptr, format);
    _vsnprintf(text, sizeof(text), format, argptr);
    va_end(argptr);
    Q3Map_Error("%s", text);
}

/*
=============
Com_Printf

Route engine messages through the compiler's output handler.

Pass unformatted text through "%s" so percent signs remain literal.
=============
*/
void Com_Printf(const char *format, ...)
{
    va_list argptr;
    char text[MAX_PRINT_MESSAGE];

    va_start(argptr, format);
    _vsnprintf(text, sizeof(text), format, argptr);
    va_end(argptr);
    Q3Map_Printf("%s", text);
}

/*
================
Com_DPrintf

Debug output is disabled in the tool.
================
*/
void Com_DPrintf(const char *format, ...)
{
}
