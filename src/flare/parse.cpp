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

/* Token and line parsing for compiler text input. */
#include <ctype.h>
#include "flare/mathlib.h"

void Q3Map_Error(const char *format, ...);

/*
==============
GetToken

Read a word, quoted string or semicolon, skipping // comments when crossing lines.
==============
*/
bool GetToken(const char **text, char *token, int maxChars, bool crossLine)
{
    int length;
    bool success;
    const char *data;
    token[0] = 0;
    if (!text)
        return false;
    data = *text;
    if (!data || !*data)
        return false;

    for (;;)
    {
        while (isspace(*data))
        {
            if (!crossLine && *data == '\n')
                return false;
            data++;
        }

        length = 0;
        success = true;
        if (*data == '"')
        {
            data++;
            while (*data && *data != '"')
            {
                token[length] = *data;
                length++;
                data++;
                if (length == maxChars)
                    Q3Map_Error("max token chars (%i) exceeded\n", maxChars);
            }
            token[length] = 0;
            if (*data != '"')
                return false;
            data++;
        }
        else if (*data == ';')
        {
            token[0] = ';';
            token[1] = 0;
            data++;
        }
        else if (*data == '/' && data[1] == '/')
        {
            if (!crossLine)
                return false;
            data += 2;
            while (*data != '\n')
            {
                if (!*data)
                    break;
                data++;
            }
            continue;
        }
        else if (*data)
        {
            do
            {
                token[length] = *data;
                length++;
                data++;
                if (length == maxChars)
                    Q3Map_Error("max token chars (%i) exceeded\n", maxChars);
            } while (*data && !isspace(*data) && *data != ';');
            token[length] = 0;
        }
        else
            success = false;
        break;
    }

    *text = data;
    return success;
}

/*
=============
SkipLine

Advance past the next newline; leave the cursor unchanged if none is found.
=============
*/
bool SkipLine(const char **text)
{
    if (!text)
        return false;

    const char *data = *text;
    if (!data || !*data)
        return false;

    while (*data && *data != '\n')
        data++;

    if (*data == '\n')
    {
        data++;
        *text = data;
        return true;
    }
    return false;
}

/*
=============
GetLine

Copy a line after skipping leading whitespace.
=============
*/
bool GetLine(const char **text, char *line, int maxChars)
{
    int length;
    bool success;
    const char *data;
    line[0] = 0;
    if (!text)
        return false;
    data = *text;
    if (!data || !*data)
        return false;

    while (isspace(*data))
        data++;

    length = 0;
    success = true;
    if (*data)
    {
        do
        {
            line[length] = *data;
            length++;
            data++;
            if (length == maxChars)
                Q3Map_Error("max token chars (%i) exceeded\n", maxChars);
        } while (*data && *data != '\n');
        line[length] = 0;
        if (*data == '\n')
            data++;
    }
    else
        success = false;

    *text = data;
    return success;
}
