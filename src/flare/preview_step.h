/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Diagnostic selection shared by stepping and preview drawing.
===========================================================================
*/
#ifndef FLARE_PREVIEW_STEP_H
#define FLARE_PREVIEW_STEP_H

struct Polygon;
struct Portal;
struct BspNode;
struct Brush;

struct PreviewStep {
    struct Polygon *polygon;
    struct Polygon *polygons;
    Portal *portal;
    BspNode *node;
    Brush *brush;
};
extern PreviewStep previewStep;

void GL_Step(int level);
void GL_StepPolygon(int level, struct Polygon *poly);
void GL_ClearStep(void);

#endif
