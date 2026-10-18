/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Camera vectors, view angles, clipping distances and leak-file points.
===========================================================================
*/
#ifndef FLARE_PREVIEW_VIEW_H
#define FLARE_PREVIEW_VIEW_H

#include "flare/mathlib.h"

struct StoredVector3d
{
    double x, y, z;
};

struct PreviewView
{
    StoredVector3d origin, forward, right, up;
    float yaw, pitch;
    float fieldOfView, nearClip, farClip;
    Vector3d *leakPoints;
    int numLeakPoints;
};

extern PreviewView previewView;

/* View the camera's stored components through the vector interface. */
#define PreviewVector(vector) (reinterpret_cast<Vector3d &>( vector ))

#endif
