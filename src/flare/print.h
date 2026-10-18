/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Diagnostics, warnings and verbose output.
===========================================================================
*/
#ifndef FLARE_PRINT_H
#define FLARE_PRINT_H

void Q3Map_Error(const char *format, ...);
void Q3Map_Printf(const char *format, ...);
void Q3Map_Warning(int level, const char *format, ...);
void Q3Map_Verbose(const char *format, ...);

void BeginProgress(const char *label);
void SetProgress(int current, int total);
void UpdateProgress(int count);
void EndProgress(void);

#endif
