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


#define MIN_BRUSH_FACES 4
#define MAX_TEMP_BRUSH_FACES 128
#define DOUBLE_TO_INT_BIAS 6755399441055744.0
#define DOUBLE_FLOOR_BIAS 0.4999997615814209

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_CROSS 3

#include "flare/geometry_types.h"
#include "flare/optiondefs.h"

void Q3Map_Error(const char *format, ...);
void Q3Map_Warning(int level, const char *format, ...);

extern const float brushClipEpsilon = 0.5f;

int RoundToInt(double value);

inline int FloorToInt(double value);

static int numMapBrushes = 0;
static Brush *brushPool = 0;

void AllocBrushPool(int maxBrushes);

void FreeBrushPool();

inline void FreeBrush(Brush *brush);

Brush *AllocTempBrush(int numFaces);

void FreeTempBrush(Brush *brush);

Brush *CopyTempBrush(const Brush *source);

Brush *AllocBrush(int numFaces);

int PlaneSideForBrush(const Plane &plane, const Brush *brush);

void SplitBrush(const Brush *brush, const Plane &plane, Brush **front, Brush **back);

static inline void FilterBrushIntoTree(BspNode *node, const Brush *brush);

void BeginProgress(const char *label);
void SetProgress(int current, int total);
void UpdateProgress(int count);
void EndProgress();
void GL_Step(int level);
void GL_StepBrush(int level, Brush *brush);

void FilterBrushesIntoTree(BspNode *node);

bool CreateBrush(const Plane *planes, int numPlanes, bool solid);

/*
=============
PlaneSideForBrush

Classify the brush from its face windings.  Faces on the plane do not
contribute to either side.
=============
*/
int PlaneSideForBrush(const Plane &plane, const Brush *brush) {
    bool front = false, back = false;
    int i, side;
    for (i = 0; i < brush->numFaces; i++) {
        side = brush->faces[i].winding.PlaneSide(plane);
        if (side == SIDE_FRONT) {
            if (back)
                return SIDE_CROSS;
            front = true;
        } else if (side == SIDE_BACK) {
            if (front)
                return SIDE_CROSS;
            back = true;
        } else if (side == SIDE_CROSS) {
            return SIDE_CROSS;
        }
    }
    if (!front && !back)
        Q3Map_Error("PlaneSideForBrush: planar brush\n");
    return !front;
}

/*
=============
Vector3d::Vector3d

Construct a vector without initializing its components.
=============
*/
inline Vector3d::Vector3d()
{}

/*
=============
Vector3d::Vector3d

Initialize the vector with the supplied x, y and z components.
=============
*/
inline Vector3d::Vector3d(double x, double y, double z) : x(x), y(y), z(z)
{}

/*
=============
Vector3d::operator-

Return a vector with each component negated.
=============
*/
inline Vector3d Vector3d::operator-() const
{
    return Vector3d(-x, -y, -z);
}

/*
=============
Vector3d::operator*

Return the dot product of this vector and v.
=============
*/
inline double Vector3d::operator*(const Vector3d &v) const
{
    return x * v.x + y * v.y + z * v.z;
}

/*
=============
operator*

Return a copy of v with each component multiplied by scale.
=============
*/
inline Vector3d operator*(double scale, const Vector3d &v)
{
    return Vector3d(scale * v.x, scale * v.y, scale * v.z);
}


/*
=============
AllocBrushPool

Reserve the permanent map brushes.  Face arrays are allocated as brushes
are added to the pool.
=============
*/
void AllocBrushPool(int maxBrushes) {
    options.maxMapBrushes = maxBrushes;
    brushPool = new Brush[options.maxMapBrushes];
    if (!brushPool)
        Q3Map_Error("AllocBrushPool: allocation failed on %i bytes", options.maxMapBrushes * sizeof(Brush));
}

/*
=============
FreeBrushPool

Release the permanent brush pool and reset its limits and count.
=============
*/
void FreeBrushPool() {
    delete[] brushPool;
    brushPool = 0;
    options.maxMapBrushes = 0;
    numMapBrushes = 0;
}

/*
=============
Plane::SetSignBits
=============
*/
inline void Plane::SetSignBits() {
    sign[0] = normal.x < 0 ? true : false;
    sign[1] = normal.y < 0 ? true : false;
    sign[2] = normal.z < 0 ? true : false;
}

/*
=============
RoundToInt

IEEE double bias conversion, for coordinates within the signed-int range.
=============
*/
int RoundToInt(double value) {
    value += DOUBLE_TO_INT_BIAS;
    return *(int *)&value;
}

/*
=============
FloorToInt
=============
*/
inline int FloorToInt(double value) {
    value -= DOUBLE_FLOOR_BIAS;
    value += DOUBLE_TO_INT_BIAS;
    return *(int *)&value;
}

/*
=============
Winding::Winding

Initialize an empty winding with no allocated point storage.
=============
*/
inline Winding::Winding()
{
    numPoints = 0;
    maxPoints = 0;
    points = 0;
}

/*
=============
Winding::~Winding

Release the winding's point array and clear its pointer.
=============
*/
inline Winding::~Winding()
{
    delete[] points;
    points = 0;
}

/*
=============
Winding::NumPoints

Return the number of points currently stored in the winding.
=============
*/
inline unsigned int Winding::NumPoints() const
{
    return numPoints;
}

/*
=============
Winding::operator[]

Return a reference to the point at index so the caller can change it.
=============
*/
inline Vector3d &Winding::operator[](int index)
{
    return points[index];
}

/*
=============
Winding::operator[]

Return a read-only reference to the point at index.
=============
*/
inline const Vector3d &Winding::operator[](int index) const
{
    return points[index];
}

/*
=============
Winding::operator=

Copy the other winding into this winding, resizing its point storage as needed.
=============
*/
inline void Winding::operator=(const Winding &other)
{
    Copy(other);
}


/*
=============
BrushFace::~BrushFace
=============
*/
BrushFace::~BrushFace() {}

/*
=============
Plane::Plane

Construct a plane without initializing its normal, distance or sign bits.
=============
*/
inline Plane::Plane() {}


/*
=============
CopyTempBrush

Copy the faces into a separately allocated temporary brush.
=============
*/
Brush *CopyTempBrush(const Brush *source) {
    Brush *brush;
    int i;
    brush = AllocTempBrush(source->numFaces);
    for (i = 0; i < brush->numFaces; i++)
        brush->faces[i] = source->faces[i];
    return brush;
}

/*
=============
Vector3d::Snap

Snap coordinates close to integer grid points.
=============
*/
void Vector3d::Snap() {
    float rounded;
    rounded = FloorToInt(x + 0.5);
    if (fabs(x - rounded) <= EQUAL_EPSILON)
        x = rounded;
    rounded = FloorToInt(y + 0.5);
    if (fabs(y - rounded) <= EQUAL_EPSILON)
        y = rounded;
    rounded = FloorToInt(z + 0.5);
    if (fabs(z - rounded) <= EQUAL_EPSILON)
        z = rounded;
}

/*
================
SplitBrush

Generate temporary brushes on either side, leaving the original unchanged.
Close a real cut with oppositely facing copies of the plane winding.
================
*/
void SplitBrush(const Brush *brush, const Plane &plane, Brush **front, Brush **back) {
    double d, dFront = 0, dBack = 0;
    int i, j;

    for (i = 0; i < brush->numFaces; i++) {
        const BrushFace &face = brush->faces[i];
        for (j = 0; j < (int)face.winding.NumPoints(); j++) {
            d = face.winding[j] * plane.normal - plane.dist;
            if (d > dFront)
                dFront = d;
            else if (d < dBack)
                dBack = d;
        }
    }
    if (-dBack >= dFront) {
        if (dFront < brushClipEpsilon) {
            *front = 0;
            *back = CopyTempBrush(brush);
            return;
        }
    } else if (dBack > -brushClipEpsilon) {
        *front = CopyTempBrush(brush);
        *back = 0;
        return;
    }

    Winding windings[2][MAX_TEMP_BRUSH_FACES];
    int faceIndices[2][MAX_TEMP_BRUSH_FACES];
    int numFront = 0, numBack = 0;
    for (i = 0; i < brush->numFaces; i++) {
        Winding *frontWinding, *backWinding;
        brush->faces[i].winding.Split(plane, &frontWinding, &backWinding);
        if (frontWinding) {
            faceIndices[0][numFront] = i;
            windings[0][numFront].Copy(*frontWinding);
            numFront++;
        }
        if (backWinding) {
            faceIndices[1][numBack] = i;
            windings[1][numBack].Copy(*backWinding);
            numBack++;
        }
        /* A one-sided split borrows the face winding; a cut allocates both halves. */
        if (frontWinding && backWinding) {
            delete frontWinding;
            delete backWinding;
        }
    }

    *front = 0;
    *back = 0;
    if (numFront < 3) {
        *back = CopyTempBrush(brush);
        return;
    }
    if (numBack < 3) {
        *front = CopyTempBrush(brush);
        return;
    }

    Winding midWinding;
    midWinding.BaseForPlane(plane);
    for (i = 0; i < brush->numFaces; i++)
        midWinding.ClipBack(brush->faces[i].plane);
    if (midWinding.NumPoints() >= 3) {
        *front = AllocTempBrush(numFront + 1);
        *back = AllocTempBrush(numBack + 1);
        (*front)->faces[numFront].winding.ReverseCopy(midWinding);
        (*front)->faces[numFront].plane.normal = -plane.normal;
        (*front)->faces[numFront].plane.dist = -plane.dist;
        (*front)->faces[numFront].plane.SetSignBits();
        (*back)->faces[numBack].winding.Copy(midWinding);
        (*back)->faces[numBack].plane.normal = plane.normal;
        (*back)->faces[numBack].plane.dist = plane.dist;
        (*back)->faces[numBack].plane.SetSignBits();
    } else {
        if (numFront >= MIN_BRUSH_FACES)
            *front = AllocTempBrush(numFront);
        if (numBack >= MIN_BRUSH_FACES)
            *back = AllocTempBrush(numBack);
    }
    if (*front) {
        for (i = 0; i < numFront; i++) {
            (*front)->faces[i].winding.Copy(windings[0][i]);
            (*front)->faces[i].plane = brush->faces[faceIndices[0][i]].plane;
        }
    }
    if (*back) {
        for (i = 0; i < numBack; i++) {
            (*back)->faces[i].winding.Copy(windings[1][i]);
            (*back)->faces[i].plane = brush->faces[faceIndices[1][i]].plane;
        }
    }
}


/*
=============
FilterBrushIntoTree

Split a solid brush down the tree and mark the leaves it reaches as solid.
Free each temporary fragment after visiting its child.
=============
*/
/* Let the recursive filter expand one level into itself. */
#pragma inline_depth(2)
#pragma inline_recursion(on)
static inline void FilterBrushIntoTree(BspNode *node, const Brush *brush) {
    Brush *front, *back;
    if (node->isLeaf) {
        node->solid = true;
        return;
    }
    SplitBrush(brush, node->plane, &front, &back);
    if (front) {
        FilterBrushIntoTree(node->children[0], front);
        FreeTempBrush(front);
    }
    if (back) {
        FilterBrushIntoTree(node->children[1], back);
        FreeTempBrush(back);
    }
}

/*
=============
FilterBrushesIntoTree

Mark the leaves occupied by solid map brushes.
=============
*/
void FilterBrushesIntoTree(BspNode *node) {
    int i;
    BeginProgress("Filtering solid brushes into tree...");
    SetProgress(0, numMapBrushes);
    GL_Step(0);
    for (i = 0; i < numMapBrushes; i++) {
        Brush *brush = &brushPool[i];
        if (brush->solid) {
            GL_StepBrush(1, brush);
            FilterBrushIntoTree(node, brush);
        }
        UpdateProgress(1);
    }
    EndProgress();
}

/*
================
AllocBrush

Take a permanent map brush from the pool and allocate its face array.
================
*/
Brush *AllocBrush(int numFaces) {
    Brush *brush;
    if (numFaces < MIN_BRUSH_FACES)
        Q3Map_Error("AllocBrush: iFaceCount < 4\n");
    if (numMapBrushes == options.maxMapBrushes)
        Q3Map_Error("AllocBrush: MAX_MAP_BRUSHES (%i) exceeded\n", options.maxMapBrushes);
    brush = &brushPool[numMapBrushes];
    brush->numFaces = numFaces;
    brush->faces = new BrushFace[numFaces];
    if (!brush->faces)
        Q3Map_Error("AllocBrush: face allocation failed on %i bytes\n", numFaces * sizeof(BrushFace));
    numMapBrushes++;
    return brush;
}

/*
=============
BrushFace::BrushFace
=============
*/
BrushFace::BrushFace() {}


/*
================
FreeBrush

Release a discarded map brush's faces and return its pool slot.
================
*/
inline void FreeBrush(Brush *brush) {
    delete[] brush->faces;
    brush->faces = 0;
    brush->numFaces = 0;
    numMapBrushes--;
}

/*
=============
AllocTempBrush

Allocate a brush and face array independently of the map brush pool.
=============
*/
Brush *AllocTempBrush(int numFaces) {
    Brush *brush;
    if (numFaces < MIN_BRUSH_FACES)
        Q3Map_Error("AllocTempBrush: iFaceCount < 4\n");
    brush = new Brush;
    if (!brush)
        Q3Map_Error("AllocTempBrush: brush allocation failed on %i bytes\n", sizeof(Brush));
    brush->numFaces = numFaces;
    brush->faces = new BrushFace[numFaces];
    if (!brush->faces)
        Q3Map_Error("AllocTempBrush: face allocation failed on %i bytes\n", numFaces * sizeof(BrushFace));
    return brush;
}

/*
=============
FreeTempBrush

Release a temporary brush and its faces.
=============
*/
void FreeTempBrush(Brush *brush) {
    delete[] brush->faces;
    brush->faces = 0;
    brush->numFaces = 0;
    delete brush;
}


/*
=============
CreateBrush

Build each face by clipping its plane winding against the other planes.
Discard empty faces and reject brushes that no longer enclose a volume.
=============
*/
bool CreateBrush(const Plane *planes, int numPlanes, bool solid) {
    Brush *brush;
    int i, j, numFaces;

    brush = AllocBrush(numPlanes);
    if (!brush)
        return false;
    numFaces = 0;
    for (i = 0; i < numPlanes; i++) {
        brush->faces[numFaces].plane = planes[i];
        brush->faces[numFaces].winding.BaseForPlane(planes[i]);
        for (j = 0; j < numPlanes; j++) {
            if (i != j)
                brush->faces[numFaces].winding.ClipBack(planes[j]);
        }
        for (j = 0; j < (int)brush->faces[numFaces].winding.NumPoints(); j++)
            brush->faces[numFaces].winding[j].Snap();
        if (brush->faces[numFaces].winding.NumPoints() >= 3)
            numFaces++;
    }
    brush->numFaces = numFaces;
    if (numFaces < MIN_BRUSH_FACES) {
        FreeBrush(brush);
        Q3Map_Warning(1, "skipping empty brush\n");
        return false;
    }
    brush->solid = solid;
    return true;
}
