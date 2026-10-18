/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Preview window, camera, drawing and texture interfaces.
===========================================================================
*/

#ifndef FLARE_PREVIEW_H
#define FLARE_PREVIEW_H

#include "flare/optiondefs.h"

struct Vector3d;
struct BspNode;

extern int numLightmaps;
extern unsigned int movementFlags;

/* Bits of movementFlags set by the +/- movement commands (config.cpp) and consumed by GL_UpdateMovement. */
enum {
    PREVIEW_MOVE_LEFT = 1,
    PREVIEW_MOVE_RIGHT = 2,
    PREVIEW_MOVE_UP = 4,
    PREVIEW_MOVE_DOWN = 8,
    PREVIEW_MOVE_FORWARD = 16,
    PREVIEW_MOVE_BACK = 32,
    PREVIEW_MOVE_HORIZONTAL = PREVIEW_MOVE_LEFT | PREVIEW_MOVE_RIGHT | PREVIEW_MOVE_FORWARD | PREVIEW_MOVE_BACK
};

extern bool previewBrightGamma;
extern int previewLightmapFilter;
extern int previewMinTextureFilter;
extern int previewMagTextureFilter;
extern unsigned int previewDrawFrame;
extern const char *previewWindowTitle;
extern const char *previewWindowClass;

float Preview_CvarFloat( const char *name );
int Preview_CvarInteger( const char *name );
BspNode *PointInLeaf( BspNode *node, const Vector3d &point );
void GL_SetCameraOrigin( const Vector3d *origin );
void GL_SetCameraDirection( const Vector3d *direction );
void GL_UpdateLightmaps();
void GL_DrawFrame();

void GL_SetCameraOrigin(const Vector3d *origin);
void GL_SetCameraDirection(const Vector3d *direction);
bool Key_MouseMove( int dx, int dy );
extern unsigned int previewState;
extern int previewWindowWidth, previewWindowHeight;
void GL_SetState( unsigned int state );
void GL_ProcessMessages();
void GL_UpdateMovement();
void GL_UpdateLeakPoint();
void GL_SetupView();
void GL_DrawWorld();
void GL_DrawDebugScene();
extern int previewActiveTexture, previewClientTexture;
extern struct Polygon *previewPolygons;
extern int previewNumPolygons;
void GL_ResetGamma();
void Key_Shutdown();
void Key_Event( int key, bool down );
void Key_ReleaseMouse();
void Key_MouseEvent( int x, int y );
extern int previewNumTextureUnits;
extern int previewActiveTexture;

#endif
