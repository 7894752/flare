/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Cursor-based token and line parsing.
===========================================================================
*/
#ifndef FLARE_PARSE_H
#define FLARE_PARSE_H

bool GetToken(const char **text, char *token, int maxChars, bool crossLine);
bool SkipLine(const char **text);
bool GetLine(const char **text, char *line, int maxChars);

#endif
