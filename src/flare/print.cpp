/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Compiler diagnostics and progress reporting.
===========================================================================
*/
#include "flare/optiondefs.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include "flare/preview.h"


static int progressStart;
static int progressCurrent;
static int progressTotal;

/*
=============
Q3Map_Verbose

Print only when verbose output is enabled.
=============
*/
void Q3Map_Verbose(const char *format, ...)
{
    va_list args;
    if (!options.verbose)
        return;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/*
=============
Q3Map_Printf
=============
*/
void Q3Map_Printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/*
=============
Q3Map_Warning

Print warnings up to the selected warning level.
=============
*/
void Q3Map_Warning(int level, const char *format, ...)
{
    va_list args;
    if (level > options.warningLevel)
        return;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/*
=============
Q3Map_VError

Print the fatal error, restore display gamma and terminate.
=============
*/
void Q3Map_VError(const char *format, va_list args)
{
    vprintf(format, args);
    GL_ResetGamma();
    exit(-1);
}

/*
=============
Q3Map_Error
=============
*/
void Q3Map_Error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    Q3Map_VError(format, args);
}

/*
=============
PrintMsecDuration

Round to seconds and print hours or minutes only when needed.
=============
*/
void PrintMsecDuration(int milliseconds)
{
    int hour, seconds, minute;
    seconds = (milliseconds + 500) / 1000;
    hour = seconds / 3600;
    seconds -= hour * 3600;
    minute = seconds / 60;
    seconds -= minute * 60;
    if (hour)
        Q3Map_Printf("%i:%02i:%02i", hour, minute, seconds);
    else if (minute)
        Q3Map_Printf("%i:%02i", minute, seconds);
    else
        Q3Map_Printf("%i seconds", seconds);
}

/*
=============
UpdateProgressPrint

Show elapsed and estimated remaining time after the first two seconds.
=============
*/
void UpdateProgressPrint(void)
{
    float percent;
    int elapsed;
    if (options.quiet)
        return;
    percent = progressCurrent * 100.0f / progressTotal;
    Q3Map_Printf("%5.2f%% complete", percent);
    elapsed = timeGetTime() - progressStart;
    if (elapsed >= 2000 && percent > 0.0f)
    {
        Q3Map_Printf(", ");
        PrintMsecDuration(elapsed);
        Q3Map_Printf(" done, ");
        PrintMsecDuration((int)(elapsed / percent * (100.0f - percent)));
        Q3Map_Printf(" remaining               ");
    }
    Q3Map_Printf("\r");
}

/*
=============
BeginProgress

Start a new progress display and reset its timer.
=============
*/
void BeginProgress(const char *label)
{
    progressCurrent = 0;
    progressTotal = 1;
    progressStart = timeGetTime();
    Q3Map_Printf("----------------------------------------\n%s\n", label);
    UpdateProgressPrint();
}

/*
=============
SetProgress
=============
*/
void SetProgress(int current, int total)
{
    progressCurrent = current;
    progressTotal = total;
    UpdateProgressPrint();
}

/*
=============
UpdateProgress
=============
*/
void UpdateProgress(int count)
{
    progressCurrent += count;
    UpdateProgressPrint();
}

/*
=============
EndProgress

Print the final duration and clear the progress line.
=============
*/
void EndProgress(void)
{
    int elapsed;
    elapsed = timeGetTime() - progressStart;
    Q3Map_Printf("Finished in ");
    PrintMsecDuration(elapsed);
    Q3Map_Printf(".                                                 \n");
}
