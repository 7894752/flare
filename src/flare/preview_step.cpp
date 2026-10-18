/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Interactive single-step visualization of BSP operations.
===========================================================================
*/
#include "flare/optiondefs.h"
#include "flare/preview.h"
#include <string.h>
#include "flare/mathlib.h"
#include "flare/geometry.h"
#include "flare/print.h"


enum {
    PREVIEW_STEP_MIN = -1,
    PREVIEW_STEP_MAX = 4
};

/* A step selects one diagnostic object, or a portal together with its node. */
#include "flare/preview_step.h"

PreviewStep previewStep = { 0 };
int previewStepLevel = 0;
bool previewContinue = false;

/*
=============
GL_WaitForStep

Refresh available lightmaps and draw frames until the continue command arrives
for an enabled step level. Consume the command before returning.
=============
*/
void GL_WaitForStep( int level ) {
    if ( options.visual && level <= previewStepLevel ) {
        if ( numLightmaps )
            GL_UpdateLightmaps();
        /* Drawing also pumps window messages, including the continue command. */
        while ( !previewContinue )
            GL_DrawFrame();
        /* Consume the command so the following step waits again. */
        previewContinue = false;
    }
}

/*
=============
GL_ClearStep

Clear all diagnostic object pointers for the current step.
=============
*/
inline void GL_ClearStep() {
    memset( &previewStep, 0, sizeof( previewStep ) );
}

/* The inclusive range also allows -1 to skip all nonnegative step levels. */
/*
=============
GL_SetStepLevel

Clamp and report the step level. A level of -1 skips all nonnegative step
levels.
=============
*/
inline void GL_SetStepLevel( int level ) {
    previewStepLevel = level;
    if ( previewStepLevel < PREVIEW_STEP_MIN )
        previewStepLevel = PREVIEW_STEP_MIN;
    if ( previewStepLevel > PREVIEW_STEP_MAX )
        previewStepLevel = PREVIEW_STEP_MAX;
    Q3Map_Verbose( "step level = %i\n", previewStepLevel );
}

/*
=============
GL_IncreaseStepLevel

Raise the step level within its allowed range.
=============
*/
void GL_IncreaseStepLevel() {
    GL_SetStepLevel( previewStepLevel + 1 );
}

/*
=============
GL_DecreaseStepLevel

Lower the step level within its allowed range.
=============
*/
void GL_DecreaseStepLevel() {
    GL_SetStepLevel( previewStepLevel - 1 );
}

/*
=============
GL_Continue

Allow the current or next step wait to continue.
=============
*/
void GL_Continue() {
    previewContinue = true;
}

/*
=============
GL_Step

Clear the highlighted objects and wait at the requested level when
visualization is enabled.
=============
*/
void GL_Step( int level ) {
    if ( options.visual ) {
        GL_ClearStep();
        GL_WaitForStep( level );
    }
}

/*
=============
GL_StepPolygon

Highlight one polygon and wait at the requested level when visualization is
enabled.
=============
*/
void GL_StepPolygon( int level, Polygon *polygon ) {
    if ( options.visual ) {
        GL_ClearStep();
        previewStep.polygon = polygon;
        GL_WaitForStep( level );
    }
}

/*
=============
GL_StepPolygons

Highlight a polygon list and wait at the requested level when visualization is
enabled.
=============
*/
void GL_StepPolygons( int level, Polygon *polygons ) {
    if ( options.visual ) {
        GL_ClearStep();
        previewStep.polygons = polygons;
        GL_WaitForStep( level );
    }
}

/*
=============
GL_StepPortal

Highlight one portal and wait at the requested level when visualization is
enabled.
=============
*/
void GL_StepPortal( int level, Portal *portal ) {
    if ( options.visual ) {
        GL_ClearStep();
        previewStep.portal = portal;
        GL_WaitForStep( level );
    }
}

/*
=============
GL_StepNode

Highlight one BSP node and wait at the requested level when visualization is
enabled.
=============
*/
void GL_StepNode( int level, BspNode *node ) {
    if ( options.visual ) {
        GL_ClearStep();
        previewStep.node = node;
        GL_WaitForStep( level );
    }
}

/*
=============
GL_StepPortalNode

Highlight a portal together with its BSP node and wait at the requested level
when visualization is enabled.
=============
*/
void GL_StepPortalNode( int level, Portal *portal, BspNode *node ) {
    if ( options.visual ) {
        GL_ClearStep();
        previewStep.portal = portal;
        previewStep.node = node;
        GL_WaitForStep( level );
    }
}

/*
=============
GL_StepBrush

Highlight one brush and wait at the requested level when visualization is
enabled.
=============
*/
void GL_StepBrush( int level, Brush *brush ) {
    if ( options.visual ) {
        GL_ClearStep();
        previewStep.brush = brush;
        GL_WaitForStep( level );
    }
}
