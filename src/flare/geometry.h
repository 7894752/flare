/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Shared geometry types and math operations.
===========================================================================
*/
#ifndef FLARE_GEOMETRY_H
#define FLARE_GEOMETRY_H

#include "geometry_types.h"

extern Polygon *polygons;
extern int numBspPolygons;
extern int numPermanentPolygons;
extern int numPolygons, peakPolygons;
extern int vectorPoolUsed, vectorPoolHighWater;
void SavePolygonPool(void);
void RestorePolygonPool(void);
Polygon *OffsetPolygon(const Polygon *poly, double distance);
int SplitPolygonEpsilon(Polygon *poly, const Plane &plane, Polygon **front, Polygon **back);
void TraceSegment(const Vector3d &start, const Vector3d &end, TraceResult *trace, unsigned int mask);
void BuildOrthonormalBasis(const Vector3d &normal, Vector3d &right, Vector3d &up);
typedef float vec2_t[2];
float PolygonCentroid2D(const vec2_t *points, int count, vec2_t centroid);
#define MAX_VERTS_PER_POLY 512

typedef void (*PolygonCellCallback)(float area, const float *centroid, const vec2_t *points, int count);
void SubdividePolygon2D(vec2_t buffers[4][MAX_VERTS_PER_POLY + 4], int count, int columns, int rows,
                        float startX, float startY, float stepX, float stepY,
                        PolygonCellCallback callback);
double PointToPolygonFormFactor(const Vector3d &point, const Vector3d &normal,
                                const Vector3d *points, int count);

bool PlaneFromPoints(const Vector3d &a, const Vector3d &b, const Vector3d &c, Vector3d &normal, double &distance);

#endif
