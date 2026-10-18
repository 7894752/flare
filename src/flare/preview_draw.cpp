/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Preview drawing helpers for lines, windings and light bounds.
===========================================================================
*/
#include "flare/mathlib.h"
#include <windows.h>
#include <GL/gl.h>
#include "flare/preview_gl.h"

/* Build a perpendicular axis for the light's local coordinate system. */

typedef double previewVec3_t[3];
typedef struct {
    unsigned short numpoints;
    unsigned short maxpoints;
    previewVec3_t *points;
} previewWinding_t;


/* Expose the contiguous double components to the OpenGL vertex-array API. */
/*
=============
Vector3d::operator double *

Return the contiguous vector components for OpenGL array calls.
=============
*/
inline Vector3d::operator double *() { return &x; }

extern "C" {

/* ONE/ZERO is opaque drawing; disable blending rather than changing its factors. */
/*
=============
GL_BlendFunc

Apply and cache blend factors, disabling blending for the opaque ONE/ZERO
combination.
=============
*/
inline void GL_BlendFunc( GLenum source, GLenum dest ) {
    if ( source == previewBlendSource && dest == previewBlendDest )
        return;
    if ( source == GL_ONE && dest == GL_ZERO ) {
        glDisable( GL_BLEND );
    } else {
        if ( previewBlendSource == GL_ONE && previewBlendDest == GL_ZERO )
            glEnable( GL_BLEND );
        glBlendFunc( source, dest );
    }
    previewBlendSource = source;
    previewBlendDest = dest;
}

/*
=============
GL_VertexPointer

Use the supplied array of three-component doubles as the OpenGL vertex array.
=============
*/
void GL_VertexPointer( const previewVec3_t *points ) {
    glVertexPointer( 3, GL_DOUBLE, 0, points );
}

/* Scale bright colors together to preserve their RGB ratios. */
#define PREVIEW_MAX(a,b) ((a) > (b) ? (a) : (b))
/*
=============
GL_Color4f

Set the drawing color, scaling overly bright RGB values together to preserve
their ratios. Enable alpha blending only for translucent colors.
=============
*/
void GL_Color4f( float r, float g, float b, float a ) {
    float maxColor = PREVIEW_MAX( r, PREVIEW_MAX( g, b ) );
    if ( maxColor > 1.0f ) {
        maxColor = 1.0f / maxColor;
        glColor4f( r * maxColor, g * maxColor, b * maxColor, a );
    } else {
        glColor4f( r, g, b, a );
    }
    if ( a < 1.0f )
        GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    else
        GL_BlendFunc( GL_ONE, GL_ZERO );
}

/*
=============
GL_Color3fv

Set an opaque RGB drawing color, scaling overly bright values together to
preserve their ratios.
=============
*/
void GL_Color3fv( const float *color ) {
    float rgb[3];
    float maxColor;
    rgb[0] = color[0];
    rgb[1] = color[1];
    rgb[2] = color[2];
    maxColor = PREVIEW_MAX( rgb[0], PREVIEW_MAX( rgb[1], rgb[2] ) );
    if ( maxColor > 1.0f ) {
        maxColor = 1.0f / maxColor;
        rgb[0] *= maxColor;
        rgb[1] *= maxColor;
        rgb[2] *= maxColor;
    }
    glColor3f( rgb[0], rgb[1], rgb[2] );
    GL_BlendFunc( GL_ONE, GL_ZERO );
}

/*
=============
GL_DrawLine

Draw a line segment between the supplied endpoints.
=============
*/
void GL_DrawLine( const previewVec3_t *start, const previewVec3_t *end ) {
    previewVec3_t points[2];
    memcpy( points[0], *start, sizeof( previewVec3_t ) );
    memcpy( points[1], *end, sizeof( previewVec3_t ) );
    glVertexPointer( 3, GL_DOUBLE, 0, points );
    glDrawArrays( GL_LINES, 0, 2 );
}

/*
=============
GL_DrawLineLoop

Draw a closed line loop through the supplied points.
=============
*/
void GL_DrawLineLoop( const previewVec3_t *points, int count ) {
    glVertexPointer( 3, GL_DOUBLE, 0, points );
    glDrawArrays( GL_LINE_LOOP, 0, count );
}

/*
=============
GL_DrawWinding

Draw the winding points as a filled polygon.
=============
*/
void GL_DrawWinding( const previewWinding_t *winding ) {
    glVertexPointer( 3, GL_DOUBLE, 0, winding->points );
    glDrawArrays( GL_POLYGON, 0, winding->numpoints );
}

/*
=============
GL_DrawWindingOutline

Draw a closed outline through the winding points.
=============
*/
void GL_DrawWindingOutline( const previewWinding_t *winding ) {
    glVertexPointer( 3, GL_DOUBLE, 0, winding->points );
    glDrawArrays( GL_LINE_LOOP, 0, winding->numpoints );
}

/*
=============
GL_DrawOctahedron

Draw a filled octahedron centered at the origin with a separate radius along
each axis.
=============
*/
void GL_DrawOctahedron( const previewVec3_t *origin, const previewVec3_t *radius ) {
    static const GLushort triangles[8][3] = {
        { 0, 4, 2 }, { 2, 4, 1 }, { 1, 4, 3 }, { 3, 4, 0 },
        { 0, 2, 5 }, { 2, 1, 5 }, { 1, 3, 5 }, { 3, 0, 5 }
    };
    previewVec3_t points[6];
    int i;
    for ( i = 0; i < 6; i++ )
        memcpy( points[i], *origin, sizeof( previewVec3_t ) );
    points[0][0] += (*radius)[0];
    points[1][0] -= (*radius)[0];
    points[2][1] += (*radius)[1];
    points[3][1] -= (*radius)[1];
    points[4][2] += (*radius)[2];
    points[5][2] -= (*radius)[2];
    glVertexPointer( 3, GL_DOUBLE, 0, points );
    glDrawElements( GL_TRIANGLES, 24, GL_UNSIGNED_SHORT, triangles );
}

/*
=============
GL_DrawOctahedronOutline

Draw the octahedron in line mode and restore filled polygon drawing.
=============
*/
void GL_DrawOctahedronOutline( const previewVec3_t *origin, const previewVec3_t *radius ) {
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    GL_DrawOctahedron( origin, radius );
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

/*
=============
GL_DrawPyramid

Draw a filled pyramid aligned with the supplied direction. The x and y radii
span its base and the z radius locates its apex.
=============
*/
void GL_DrawPyramid( const Vector3d *origin, const Vector3d *direction, const Vector3d *radius ) {
    static const GLushort triangles[6][3] = {
        { 0, 4, 2 }, { 2, 4, 1 }, { 1, 4, 3 }, { 3, 4, 0 },
        { 0, 2, 1 }, { 0, 1, 3 }
    };
    Vector3d right, up;
    Vector3d points[5];
    /* The local x/y radii span the base; the z radius reaches the apex. */
    right = PerpendicularVector3d( *direction );
    up = direction->Cross( right );
    points[0] = *origin + right * radius->x;
    points[1] = *origin - right * radius->x;
    points[2] = *origin + up * radius->y;
    points[3] = *origin - up * radius->y;
    points[4] = *origin + *direction * radius->z;
    glVertexPointer( 3, GL_DOUBLE, 0, points[0] );
    glDrawElements( GL_TRIANGLES, 18, GL_UNSIGNED_SHORT, triangles );
}

/*
=============
GL_DrawPyramidOutline

Draw the directional pyramid in line mode and restore filled polygon drawing.
=============
*/
void GL_DrawPyramidOutline( const Vector3d *origin, const Vector3d *direction, const Vector3d *radius ) {
    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    GL_DrawPyramid( origin, direction, radius );
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
}

}
