/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Preview view setup, scene drawing and texture upload.
===========================================================================
*/
#define _WIN32_WINNT 0x0400
#include "flare/optiondefs.h"
#include "flare/mathlib.h"
#include "flare/geometry.h"
#include "flare/shaders.h"
#include "flare/preview.h"
#include "flare/preview_view.h"
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "flare/preview_gl.h"
#include <math.h>
#include <string.h>
#include "flare/print.h"
#include "flare/map.h"

/* Preview render-state bits cached in previewState; shared with GL_SetState (preview_state.cpp). */
enum {
    GLSTATE_TEXTURE0_2D = 0x1,
    GLSTATE_TEXTURE1_2D = 0x4,
    GLSTATE_TEXCOORD0_ARRAY = 0x100,
    GLSTATE_TEXCOORD1_ARRAY = 0x200,
    GLSTATE_COLOR_ARRAY = 0x1000,
    GLSTATE_NORMAL_ARRAY = 0x2000,
    GLSTATE_VERTEX_ARRAY = 0x4000,
    GLSTATE_DEPTH_WRITE = 0x8000,
    GLSTATE_COLOR_WRITE = 0x10000,
    GLSTATE_ALPHA_WRITE = 0x20000,
    GLSTATE_STENCIL_WRITE = 0x40000,
    GLSTATE_DEPTH_TEST = 0x80000,
    GLSTATE_ALPHA_TEST = 0x100000,
    GLSTATE_STENCIL_TEST = 0x200000
};


struct Polygon;

bool previewBrightGamma = false;

extern void DrawSceneLights();
extern void PickSceneLight( const Vector3d &origin, const Vector3d &direction );


#include "flare/preview_step.h"

extern int numOccluders, numPolyOccluders;
extern Occluder *occluderPool;
extern Occluder *polyOccluders[32768];

#define PREVIEW_PI 3.1415927f

enum {
    PREVIEW_SHADER_TEXTURE_BASE = 8192,          /* texture object of shader N is this + N; lightmap textures are 1-based */
    PREVIEW_MAX_SHADERS = 4096,
    PREVIEW_MAX_POLYGON_POINTS = 1024,
    PREVIEW_LIGHTMAP_SIZE = 512,                    /* samples per lightmap page side */
    PREVIEW_IMAGE_BUFFER_BYTES = 64 * 1024 * 1024   /* scratch buffer for one image and its mip chain */
};
#define PREVIEW_PICK_DISTANCE 8192.0


#include "flare/model.h"

/* Texture filters (GL enums) and the leak-file point the camera last jumped to. */

int previewLightmapFilter = GL_NEAREST;
int previewMinTextureFilter = GL_LINEAR_MIPMAP_NEAREST;
int previewMagTextureFilter = GL_LINEAR;
static int previewCurrentLeakPoint = -1;

/* Shader texture upload and in-place mipmap generation. */


#include "flare/lightmap.h"


/* Polygon accessors, selection and gamma restore used by the preview. */


extern void GL_VertexPointerf( const float *points );

/*
=============
Plane::SetSignBits

Record which normal components are negative for plane-side tests.
=============
*/
inline void Plane::SetSignBits() {
    sign[0] = normal.x < 0 ? true : false;
    sign[1] = normal.y < 0 ? true : false;
    sign[2] = normal.z < 0 ? true : false;
}

/*
=============
GL_BlendFunc

Apply the requested blend factors and cache them. Disable blending for opaque
drawing.
=============
*/
extern "C" inline void GL_BlendFunc( GLenum source, GLenum dest ) {
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

/* Draw the eight corners of a bounds box. */
/*
=============
GL_DrawBoundsOutline

Draw a line strip through the eight corners of the bounds.
=============
*/
static void GL_DrawBoundsOutline( const Bounds &bounds ) {
    Vector3d corners[8];
    for ( int i = 0; i < 8; i++ ) {
        corners[i].x = bounds.points[i & 1][0];
        corners[i].y = bounds.points[( i >> 1 ) & 1][1];
        corners[i].z = bounds.points[( i >> 2 ) & 1][2];
    }
    glVertexPointer( 3, GL_DOUBLE, 0, corners );
    glDrawArrays( GL_LINE_STRIP, 0, 8 );
}

/* Perspective projection from the field of view and clip distances, then the view matrix of the preview camera. */
/*
=============
GL_SetupView

Load the perspective projection and camera view matrices from the preview
settings.
=============
*/
void GL_SetupView() {
    double scale = 1.0 / tan( previewView.fieldOfView * (PREVIEW_PI / 360.0f) );
    double depthScale = 1.0f / (previewView.nearClip - previewView.farClip);
    float matrix[16];
    memset( matrix, 0, sizeof( matrix ) );
    matrix[0] = scale;
    matrix[5] = scale * ((double)previewWindowWidth / previewWindowHeight);
    matrix[10] = (previewView.farClip + previewView.nearClip) * depthScale;
    matrix[11] = -1;
    matrix[14] = 2.0f * previewView.farClip * previewView.nearClip * depthScale;
    glMatrixMode( GL_PROJECTION );
    glLoadMatrixf( matrix );

    memset( matrix, 0, sizeof( matrix ) );
    matrix[2] = -1;
    matrix[4] = -1;
    matrix[9] = 1;
    matrix[15] = 1;
    glMatrixMode( GL_MODELVIEW );
    glLoadMatrixf( matrix );
    glRotatef( -previewView.pitch, 0, 1, 0 );
    glRotatef( -previewView.yaw, 0, 0, 1 );
    glTranslated( -PreviewVector( previewView.origin ).x, -PreviewVector( previewView.origin ).y,
        -PreviewVector( previewView.origin ).z );
}

/*
=============
Vector3d::operator+=

Add each component of the supplied vector to this vector.
=============
*/
inline void Vector3d::operator+=(const Vector3d &v)
{
    x += v.x;
    y += v.y;
    z += v.z;
}

/*
=============
Vector3d::operator-=

Subtract each component of the supplied vector from this vector.
=============
*/
inline void Vector3d::operator-=(const Vector3d &v)
{
    x -= v.x;
    y -= v.y;
    z -= v.z;
}

/*
=============
GL_UpdateLeakPoint

Move the camera to the selected leak point when the leakpoint setting changes.
=============
*/
void GL_UpdateLeakPoint() {
    if ( previewView.numLeakPoints ) {
        int point = Preview_CvarInteger( "leakpoint" ) % previewView.numLeakPoints;
        if ( point != previewCurrentLeakPoint ) {
            PreviewVector( previewView.origin ) = previewView.leakPoints[point];
            previewCurrentLeakPoint = point;
        }
    }
}

/*
=============
Vector3d::LengthSquared

Return the squared length of this vector without taking a square root.
=============
*/
inline double Vector3d::LengthSquared() const
{
    return x * x + y * y + z * z;
}

#include "flare/polygon.h"

/*
=============
Polygon::GetShaderIndex

Return the shader index stored in the polygon information.
=============
*/
int Polygon::GetShaderIndex() const { return info->shaderIndex; }

/*
=============
Polygon::GetLightmapIndex

Return the lightmap page index stored in the polygon information.
=============
*/
int Polygon::GetLightmapIndex() const { return info->lightmapIndex; }

/*
=============
Polygon::GetModel

Return the model associated with the polygon.
=============
*/
MiscModel *Polygon::GetModel() const { return info->model; }

/*
=============
GL_SetScene

Set the polygon array to display and reset the field of view and clipping
distances.
=============
*/
void GL_SetScene( struct Polygon *polygons, int numPolygons ) {
    previewPolygons = polygons;
    previewNumPolygons = numPolygons;
    previewView.fieldOfView = 90;
    previewView.nearClip = 4;
    previewView.farClip = 4096;
}

#include "flare/traceresult.h"

/*
=============
GL_ClearSelection

Clear the selection flag on every map polygon.
=============
*/
void GL_ClearSelection() {
    for ( int i = 0; i < numPolygons; i++ )
        polygons[i].selected = false;
}

/*
=============
GL_ResetGamma

Restore a linear gamma ramp on the preview display.
=============
*/
void GL_ResetGamma() {
    unsigned short ramp[3][256];
    int i;
    for ( i = 0; i < 256; i++ ) {
        ramp[2][i] = ramp[1][i] = ramp[0][i] = i * 257;
    }
    SetDeviceGammaRamp( previewDC, ramp );
}

/*
=============
GL_MipMap

Reduce an RGBA image by averaging adjacent pixels in place. Handle images with
only one row or column as one-dimensional images.
=============
*/
static void GL_MipMap( unsigned char *in, int width, int height ) {
    int i, j;
    unsigned char *out;
    int row;
    if ( width == 1 && height == 1 )
        return;
    row = width * 4;
    out = in;
    width >>= 1;
    height >>= 1;
    if ( width && height ) {
        for ( i = 0; i < height; i++, in += row ) {
            for ( j = 0; j < width; j++, out += 4, in += 8 ) {
                out[0] = (in[0] + in[4] + in[row] + in[row+4]) >> 2;
                out[1] = (in[1] + in[5] + in[row+1] + in[row+5]) >> 2;
                out[2] = (in[2] + in[6] + in[row+2] + in[row+6]) >> 2;
                out[3] = (in[3] + in[7] + in[row+3] + in[row+7]) >> 2;
            }
        }
    } else {
        width += height;
        for ( i = 0; i < width; i++, out += 4, in += 8 ) {
            out[0] = (in[0] + in[4]) >> 1;
            out[1] = (in[1] + in[5]) >> 1;
            out[2] = (in[2] + in[6]) >> 1;
            out[3] = (in[3] + in[7]) >> 1;
        }
    }
}

/*
=============
GL_SetCameraOrigin

Copy the supplied position into the preview camera origin.
=============
*/
void GL_SetCameraOrigin( const Vector3d *origin ) {
    PreviewVector( previewView.origin ) = *origin;
}

/*
=============
GL_SetCameraDirection

Set the camera yaw and pitch from a nonzero direction vector.
=============
*/
void GL_SetCameraDirection( const Vector3d *direction ) {
    if ( direction->LengthSquared() != 0 ) {
        previewView.yaw = atan2( direction->y, direction->x ) * (180.0 / PREVIEW_PI);
        previewView.pitch = asin( -direction->z ) * (180.0 / PREVIEW_PI);
    }
}

/*
=============
GL_SetLeakPoints

Store the leak point array and its count for drawing and camera navigation.
=============
*/
void GL_SetLeakPoints( Vector3d *points, int count ) {
    previewView.leakPoints = points;
    previewView.numLeakPoints = count;
}

/* The point array is also used directly by OpenGL's vertex-array interface. */
/*
=============
Polygon::GetPoints

Return the polygon vertex array for direct access.
=============
*/
inline const Vector3d *Polygon::GetPoints() const { return points; }

/*
=============
GL_SelectTexture

Select the server texture unit when it differs from the cached unit.
=============
*/
inline void GL_SelectTexture( int unit ) {
    if ( unit != previewActiveTexture ) {
        glActiveTextureARB( GL_TEXTURE0_ARB + unit );
        previewActiveTexture = unit;
    }
}

/*
=============
GL_BindTexture

Bind a texture on the requested unit when its cached binding changes.
=============
*/
inline void GL_BindTexture( int unit, GLenum target, GLuint texture ) {
    if ( texture != previewBoundTextures[unit] ) {
        GL_SelectTexture( unit );
        glBindTexture( target, texture );
        previewBoundTextures[unit] = texture;
    }
}

/*
=============
GL_VertexPointerf

Use the supplied array of three-component floats as the OpenGL vertex array.
=============
*/
inline void GL_VertexPointerf( const float *points ) {
    glVertexPointer( 3, GL_FLOAT, 0, points );
}

/*
=============
GL_TexCoordPointer

Select the client texture unit and set its float texture-coordinate array.
=============
*/
inline void GL_TexCoordPointer( int unit, int size, const float *coords ) {
    if ( unit != previewClientTexture ) {
        glClientActiveTextureARB( GL_TEXTURE0_ARB + unit );
        previewClientTexture = unit;
    }
    glTexCoordPointer( size, GL_FLOAT, 0, coords );
}

/*
=============
GL_ChangeState

Clear and set the requested bits in the current render state.
=============
*/
inline void GL_ChangeState( unsigned int set, unsigned int clear ) {
    GL_SetState( (previewState & ~clear) | set );
}

/*
=============
GL_SetShader

Configure blending, alpha testing, depth writes and polygon offset for the
shader.
=============
*/
static void GL_SetShader( int shaderIndex ) {
    switch ( shaders[shaderIndex].image.alphaType ) {
    case 0:
        GL_ChangeState( GLSTATE_DEPTH_WRITE, GLSTATE_ALPHA_TEST );
        GL_BlendFunc( GL_ONE, GL_ZERO );
        break;
    case 1:
        GL_ChangeState( 0, GLSTATE_DEPTH_WRITE | GLSTATE_ALPHA_TEST );
        GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
        break;
    case 2:
        GL_ChangeState( GLSTATE_DEPTH_WRITE | GLSTATE_ALPHA_TEST, 0 );
        glAlphaFunc( GL_GREATER, 0 );
        GL_BlendFunc( GL_ONE, GL_ZERO );
        break;
    case 3:
        GL_ChangeState( GLSTATE_DEPTH_WRITE | GLSTATE_ALPHA_TEST, 0 );
        glAlphaFunc( GL_GEQUAL, 0.5f );
        GL_BlendFunc( GL_ONE, GL_ZERO );
        break;
    case 4:
        GL_ChangeState( GLSTATE_DEPTH_WRITE | GLSTATE_ALPHA_TEST, 0 );
        glAlphaFunc( GL_LESS, 0.5f );
        GL_BlendFunc( GL_ONE, GL_ZERO );
        break;
    case 5:
        GL_ChangeState( 0, GLSTATE_DEPTH_WRITE | GLSTATE_ALPHA_TEST );
        GL_BlendFunc( GL_ONE, GL_ONE );
        break;
    }
    if ( shaders[shaderIndex].flags & SHADERFLAG_0x20 ) {
        glEnable( GL_POLYGON_OFFSET_FILL );
        glPolygonOffset( -1, -1 );
    } else {
        glDisable( GL_POLYGON_OFFSET_FILL );
    }
}

/*
=============
GL_DrawModel

Draw each model surface with its material and object transform. Skip a model
already drawn during this scene pass.
=============
*/
static void GL_DrawModel( MiscModel *model ) {
    if ( model->frameStamp == previewDrawFrame )
        return;
    model->frameStamp = previewDrawFrame;
    double matrix[16] = {
        model->axis[0].x, model->axis[0].y, model->axis[0].z, 0,
        model->axis[1].x, model->axis[1].y, model->axis[1].z, 0,
        model->axis[2].x, model->axis[2].y, model->axis[2].z, 0,
        model->origin.x, model->origin.y, model->origin.z, 1
    };
    glPushMatrix();
    glMultMatrixd( matrix );
    unsigned int state = previewState;
    GL_ChangeState( 0, GLSTATE_TEXTURE1_2D | GLSTATE_TEXCOORD1_ARRAY );
    for ( int surface = 0; surface < model->model->numSurfaces; surface++ ) {
        GL_SetShader( model->model->surfaces[surface].material );
        GL_BindTexture( 0, GL_TEXTURE_2D, PREVIEW_SHADER_TEXTURE_BASE + model->model->surfaces[surface].material );
        GL_TexCoordPointer( 0, 2, (const float *)model->model->surfaces[surface].texcoords );
        glVertexPointer( 3, GL_FLOAT, 0, model->model->surfaces[surface].vertices );
        glDrawElements( GL_TRIANGLES, model->model->surfaces[surface].numIndices, GL_UNSIGNED_SHORT, model->model->surfaces[surface].indices );
    }
    GL_SetState( state );
    glPopMatrix();
}

/*
=============
GL_DrawPolygon

Draw a polygon with its shader and lightmap coordinates, or draw its
associated model. Optionally display vertex normals.
=============
*/
void GL_DrawPolygon( const struct Polygon *polygon ) {
    int i;
    float vertices[PREVIEW_MAX_POLYGON_POINTS][3];
    float texcoords[PREVIEW_MAX_POLYGON_POINTS][2];
    float lightmapCoords[PREVIEW_MAX_POLYGON_POINTS][2];
    if ( polygon->info->flags & POLYFLAG_MODEL ) {
        GL_DrawModel( polygon->info->model );
        return;
    }
    int shaderIndex = polygon->info->shaderIndex;
    if ( shaderIndex >= 0 ) {
        glColor4f( 1, 1, 1, 1 );
        GL_SetShader( shaderIndex );
    } else if ( polygon->info->flags & POLYFLAG_0x40 ) {
        if ( !Preview_CvarInteger( "r_showbspleaf" ) )
            return;
        GL_ChangeState( 0, GLSTATE_DEPTH_WRITE | GLSTATE_ALPHA_TEST );
        GL_BlendFunc( GL_ONE, GL_ONE );
    }
    for ( i = 0; i < polygon->numPoints; i++ ) {
        vertices[i][0] = polygon->points[i].x;
        vertices[i][1] = polygon->points[i].y;
        vertices[i][2] = polygon->points[i].z;
        polygon->WorldToTexture( polygon->points[i], texcoords[i] );
    }
    GL_ChangeState( 0, GLSTATE_COLOR_ARRAY );
    if ( polygon->info->flags & POLYFLAG_0x40 )
        glColor4f( 0.1f, 0.07f, 0.02f, 1 );
    else
        glColor4f( 1, 1, 1, 1 );
    for ( i = 0; i < polygon->numPoints; i++ ) {
        float *st = lightmapCoords[i];
        const Vector3d &point = polygon->points[i];
        polygon->WorldToLightmap( &point, st );
        lightmapCoords[i][0] *= 1.0f / PREVIEW_LIGHTMAP_SIZE;
        lightmapCoords[i][1] *= 1.0f / PREVIEW_LIGHTMAP_SIZE;
    }
    GL_BindTexture( 0, GL_TEXTURE_2D, polygon->info->lightmapIndex + 1 );
    GL_BindTexture( 1, GL_TEXTURE_2D, shaderIndex + PREVIEW_SHADER_TEXTURE_BASE );
    glVertexPointer( 3, GL_FLOAT, 0, vertices );
    GL_TexCoordPointer( 0, 2, (const float *)lightmapCoords );
    GL_TexCoordPointer( 1, 2, (const float *)texcoords );
    glDrawArrays( GL_POLYGON, 0, polygon->numPoints );
    if ( polygon->normals && Preview_CvarInteger( "r_shownormals" ) ) {
        unsigned int state = previewState;
        GL_SetState( GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE | GLSTATE_ALPHA_WRITE | GLSTATE_DEPTH_TEST );
        glBegin( GL_LINES );
        for ( i = 0; i < polygon->numPoints; i++ ) {
            glColor3f( 0, 0, 0 );
            glVertex3dv( &polygon->points[i].x );
            glColor3f( 1, 1, 1 );
            Vector3d endpoint = polygon->points[i] + polygon->normals[i] * 4.0;
            glVertex3dv( &endpoint.x );
        }
        glEnd();
        GL_SetState( state );
    }
}

/* Per-frame camera basis and free-fly movement; the step is limited to one second of movement. */
/*
=============
GL_UpdateMovement

Update the camera basis from its angles and apply held movement controls.
Limit the elapsed movement interval to one second.
=============
*/
void GL_UpdateMovement() {
    static int oldTime = timeGetTime();
    int time = timeGetTime();
    int elapsed = time - oldTime;
    if ( elapsed >= 1000 )
        elapsed = 1000;
    double distance = Preview_CvarFloat( "movespeed" ) * elapsed * 0.001f;
    float yaw = previewView.yaw * (PREVIEW_PI / 180.0f);
    double sy = sin( yaw ), cy = cos( yaw );
    double cp = cos( previewView.pitch * (PREVIEW_PI / 180.0f) );
    PreviewVector( previewView.forward ).x = cp * cy;
    PreviewVector( previewView.forward ).y = cp * sy;
    PreviewVector( previewView.forward ).z = -sin( previewView.pitch * (PREVIEW_PI / 180.0f) );
    PreviewVector( previewView.right ).x = -sy;
    PreviewVector( previewView.right ).y = cy;
    PreviewVector( previewView.right ).z = 0;
    PreviewVector( previewView.up ) = PreviewVector( previewView.forward ).Cross( PreviewVector( previewView.right ) );
    if ( movementFlags & PREVIEW_MOVE_HORIZONTAL ) {
        Vector3d strafe( -sy * distance, cy * distance, 0 );
        Vector3d advance( cy * distance, sy * distance, 0 );
        if ( movementFlags & PREVIEW_MOVE_LEFT )
            PreviewVector( previewView.origin ) += strafe;
        if ( movementFlags & PREVIEW_MOVE_RIGHT )
            PreviewVector( previewView.origin ) -= strafe;
        if ( movementFlags & PREVIEW_MOVE_FORWARD )
            PreviewVector( previewView.origin ) += advance;
        if ( movementFlags & PREVIEW_MOVE_BACK )
            PreviewVector( previewView.origin ) -= advance;
    }
    if ( movementFlags & PREVIEW_MOVE_UP )
        PreviewVector( previewView.origin ).z += distance;
    if ( movementFlags & PREVIEW_MOVE_DOWN )
        PreviewVector( previewView.origin ).z -= distance;
    oldTime = time;
}

/*
=============
GL_DrawLeak

Draw the leak path in red with blue point markers and a thicker depth-tested
line.
=============
*/
void GL_DrawLeak() {
    if ( previewView.numLeakPoints ) {
        GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE );
        glColor3f( 1, 0, 0 );
        glVertexPointer( 3, GL_DOUBLE, 0, previewView.leakPoints );
        glDrawArrays( GL_LINE_STRIP, 0, previewView.numLeakPoints );
        GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE | GLSTATE_DEPTH_TEST );
        glLineWidth( 3 );
        glDrawArrays( GL_LINE_STRIP, 0, previewView.numLeakPoints );
        GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE );
        glPointSize( 5 );
        glColor3f( 0, 0, 1 );
        glDrawArrays( GL_POINTS, 0, previewView.numLeakPoints );
        glLineWidth( 1 );
        glPointSize( 1 );
    }
}

/*
=============
GL_DrawHighlightedPolygon

Draw the polygon outline and a translucent fill using the current color.
=============
*/
static void GL_DrawHighlightedPolygon( const struct Polygon *polygon ) {
    GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_COLOR_WRITE );
    glVertexPointer( 3, GL_DOUBLE, 0, polygon->GetPoints() );
    glDrawArrays( GL_LINE_LOOP, 0, polygon->numPoints );
    GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    GL_CullFace( 0 );
    glDrawArrays( GL_POLYGON, 0, polygon->numPoints );
    GL_BlendFunc( GL_ONE, GL_ZERO );
}

/*
=============
GL_DrawHighlightedPolygons

Draw an outline and translucent fill for every polygon in the linked list.
=============
*/
static inline void GL_DrawHighlightedPolygons( const struct Polygon *polygon ) {
    for ( ; polygon; polygon = polygon->next )
        GL_DrawHighlightedPolygon( polygon );
}

/*
=============
GL_DrawHighlightedPortal

Draw a thick portal outline and a translucent triangle fan using the current
color.
=============
*/
static void GL_DrawHighlightedPortal( const Portal *portal ) {
    glLineWidth( 2 );
    GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_COLOR_WRITE );
    glVertexPointer( 3, GL_DOUBLE, 0, portal->winding.points );
    glDrawArrays( GL_LINE_LOOP, 0, portal->winding.numPoints );
    GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    GL_CullFace( 0 );
    glDrawArrays( GL_TRIANGLE_FAN, 0, portal->winding.numPoints );
    GL_BlendFunc( GL_ONE, GL_ZERO );
    glLineWidth( 1 );
}

/*
=============
GL_DrawNodePortals

Draw all portals attached to the node, following the link for its side of each
portal.
=============
*/
static inline void GL_DrawNodePortals( const BspNode *node ) {
    if ( node ) {
        for ( const Portal *portal = node->portals; portal; portal = portal->next[node == portal->nodes[1]] )
            GL_DrawHighlightedPortal( portal );
    }
}

/*
=============
GL_DrawNodePolygons

Highlight every polygon in the node polygon list.
=============
*/
static inline void GL_DrawNodePolygons( const BspNode *node ) {
    if ( node ) {
        for ( const struct Polygon *polygon = node->polygons; polygon; polygon = polygon->next )
            GL_DrawHighlightedPolygon( polygon );
    }
}

/*
=============
GL_DrawHighlightedWinding

Draw the winding outline and a translucent fill using the current color.
=============
*/
static void GL_DrawHighlightedWinding( const Winding *winding ) {
    GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_COLOR_WRITE );
    glVertexPointer( 3, GL_DOUBLE, 0, winding->points );
    glDrawArrays( GL_LINE_LOOP, 0, winding->numPoints );
    GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    GL_CullFace( 0 );
    glDrawArrays( GL_POLYGON, 0, winding->numPoints );
    GL_BlendFunc( GL_ONE, GL_ZERO );
}

/*
=============
GL_DrawHighlightedBrush

Highlight the winding of every brush face.
=============
*/
static void GL_DrawHighlightedBrush( const Brush *brush ) {
    for ( int i = 0; i < brush->numFaces; i++ )
        GL_DrawHighlightedWinding( &brush->faces[i].winding );
}

/*
=============
GL_DrawCurrentLeaf

Display the portals of the camera leaf when enabled, colored by its solid and
flood status. Keep the previous leaf when leaf locking is enabled.
=============
*/
void GL_DrawCurrentLeaf() {
    static BspNode *previewCurrentLeaf = NULL;
    if ( Preview_CvarInteger( "r_showleaf" ) ) {
        if ( !Preview_CvarInteger( "r_lockleaf" ) && bspRoot )
            previewCurrentLeaf = PointInLeaf( bspRoot, PreviewVector( previewView.origin ) );
        if ( previewCurrentLeaf ) {
            if ( previewCurrentLeaf->solid )
                glColor4f( 1, 0, 0, 0.25f );
            else if ( previewCurrentLeaf->flooded )
                glColor4f( 0, 1, 0, 0.25f );
            else
                glColor4f( 1, 1, 0, 0.25f );
            GL_DrawNodePortals( previewCurrentLeaf );
#ifdef _DEBUG
            GL_DrawBoundsOutline( previewCurrentLeaf->bounds );
#endif
        }
    }
}

/* Six frustum planes: near, far and the four side planes through the view origin. */
/*
=============
GL_BuildViewFrustum

Build the near, far and four side planes from the camera and viewport
settings. Set the component sign flags on each plane.
=============
*/
static void GL_BuildViewFrustum( Plane planes[6] ) {
    planes[0].normal = PreviewVector( previewView.forward );
    planes[0].dist = PreviewVector( previewView.origin ) * planes[0].normal + previewView.nearClip;
    planes[0].SetSignBits();
    planes[1].normal = -PreviewVector( previewView.forward );
    planes[1].dist = PreviewVector( previewView.origin ) * planes[1].normal - previewView.farClip;
    planes[1].SetSignBits();
    Vector3d corners[4];
    Vector3d center = PreviewVector( previewView.forward ) * previewView.nearClip;
    double scale = tan( previewView.fieldOfView * (PREVIEW_PI / 360.0f) ) * previewView.nearClip;
    Vector3d right = PreviewVector( previewView.right ) * scale;
    Vector3d up = PreviewVector( previewView.up ) * (previewWindowWidth * scale / previewWindowHeight);
    corners[0] = center - right - up;
    corners[1] = center - right + up;
    corners[2] = center + right + up;
    corners[3] = center + right - up;
    for ( int i = 0; i < 4; i++ ) {
        planes[i+2].normal = corners[i].Cross( corners[(i+3)%4] );
        planes[i+2].dist = PreviewVector( previewView.origin ) * planes[i+2].normal;
        planes[i+2].SetSignBits();
    }
}

/*
=============
GL_DrawDebugScene

Draw the current step objects and enabled light, occluder, selection and leak
overlays.
=============
*/
void GL_DrawDebugScene() {
    if ( previewStep.node ) {
        glColor4f( 0, 1, 1, 0.25f );
        GL_DrawNodePolygons( previewStep.node );
        glColor4f( 0, 1, 0, 0.25f );
        GL_DrawNodePortals( previewStep.node );
    }
    if ( previewStep.brush ) {
        glColor4f( 1, 0.5f, 0, 0.25f );
        GL_DrawHighlightedBrush( previewStep.brush );
    }
    if ( previewStep.portal ) {
        glColor4f( 1, 1, 0, 0.25f );
        GL_DrawHighlightedPortal( previewStep.portal );
    }
    if ( previewStep.polygons ) {
        glColor4f( 0, 0, 1, 0.25f );
        GL_DrawHighlightedPolygons( previewStep.polygons );
    }
    if ( previewStep.polygon ) {
        glColor4f( 0.5f, 1, 0.5f, 0.25f );
        GL_DrawHighlightedPolygon( previewStep.polygon );
    }
    if ( Preview_CvarInteger( "r_showlights" ) ) {
        if ( Preview_CvarInteger( "r_picklight" ) )
            PickSceneLight( PreviewVector( previewView.origin ), PreviewVector( previewView.forward ) );
        GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE | GLSTATE_DEPTH_TEST );
        GL_CullFace( GL_FRONT );
        DrawSceneLights();
    }
    if ( Preview_CvarInteger( "r_showoccluders" ) ) {
        if ( numOccluders ) {
            glColor4f( 1, 0, 1, 0.15f );
            for ( int i = 0; i < numOccluders; i++ )
                GL_DrawHighlightedPolygon( occluderPool[i].polygon );
        }
        if ( numPolyOccluders ) {
            glColor4f( 0, 1, 1, 0.25f );
            for ( int i = 0; i < numPolyOccluders; i++ )
                GL_DrawHighlightedPolygon( polyOccluders[i]->polygon );
        }
    }
    if ( Preview_CvarInteger( "r_showbreakpoints" ) ) {
        glColor4f( 0.75f, 0, 0, 0.25f );
        for ( int i = 0; i < numPolygons; i++ )
            if ( polygons[i].selected )
                GL_DrawHighlightedPolygon( &polygons[i] );
    }
    GL_DrawLeak();
    GL_DrawCurrentLeaf();
}

/*
=============
GL_DrawScene

Cull scene polygons against the view frustum and draw those that remain.
Advance the draw stamp used to avoid duplicate model draws.
=============
*/
void GL_DrawScene() {
    Plane planes[6];
    GL_BuildViewFrustum( planes );
    previewDrawFrame++;
    struct Polygon *polygon = previewPolygons;
    for ( int i = 0; i < previewNumPolygons; polygon++, i++ ) {
        unsigned int j;
        for ( j = 0; j < 6; j++ )
            if ( polygon->bounds.IsBehindPlane( planes[j] ) )
                break;
        if ( j == 6 )
            GL_DrawPolygon( polygon );
    }
    GL_ChangeState( 0, GLSTATE_COLOR_ARRAY | GLSTATE_ALPHA_TEST );
    GL_BlendFunc( GL_ONE, GL_ZERO );
    glColor4f( 1, 1, 1, 1 );
}

/* Draw the BSP polygons in the mode chosen by the r_wireframe, r_textured and r_fullbright cvars. */
/*
=============
GL_DrawWorld

Draw the scene using the selected wireframe, textured and fullbright modes.
=============
*/
void GL_DrawWorld() {
    int wireframe = Preview_CvarInteger( "r_wireframe" );
    int textured = Preview_CvarInteger( "r_textured" );
    int fullbright = Preview_CvarInteger( "r_fullbright" );
    if ( wireframe ) {
        GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_DEPTH_WRITE | GLSTATE_DEPTH_TEST );
        GL_CullFace( GL_FRONT );
        GL_BlendFunc( GL_ONE, GL_ZERO );
        glDepthFunc( GL_LESS );
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
        GL_DrawScene();
        glDepthRange( 0, 0.9999f );
        GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_COLOR_WRITE | GLSTATE_DEPTH_TEST );
        GL_CullFace( 0 );
        GL_BlendFunc( GL_ONE, GL_ZERO );
        glDepthFunc( GL_LESS );
        glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
        GL_DrawScene();
        glDepthRange( 0, 1 );
        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    } else {
        if ( textured ) {
            if ( fullbright )
                GL_SetState( GLSTATE_TEXTURE1_2D | GLSTATE_TEXCOORD1_ARRAY | GLSTATE_VERTEX_ARRAY | GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE | GLSTATE_DEPTH_TEST );
            else
                GL_SetState( GLSTATE_TEXTURE0_2D | GLSTATE_TEXTURE1_2D | GLSTATE_TEXCOORD0_ARRAY | GLSTATE_TEXCOORD1_ARRAY | GLSTATE_VERTEX_ARRAY | GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE | GLSTATE_DEPTH_TEST );
            GL_CullFace( GL_FRONT );
            GL_BlendFunc( GL_ONE, GL_ZERO );
            glDepthFunc( GL_LESS );
            glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
            GL_DrawScene();
            glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
        } else {
            GL_SetState( GLSTATE_TEXTURE0_2D | GLSTATE_TEXCOORD0_ARRAY | GLSTATE_VERTEX_ARRAY | GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE | GLSTATE_DEPTH_TEST );
            GL_CullFace( GL_FRONT );
            GL_BlendFunc( GL_ONE, GL_ZERO );
            glDepthFunc( GL_LESS );
            glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
            GL_DrawScene();
            glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
        }
    }
}

/* One preview frame: input, movement, clear, world, debug overlays, swap. */
/*
=============
GL_DrawFrame

Process input, update the camera and render one preview frame when
visualization is enabled. Draw the world and diagnostic overlays before
swapping buffers.
=============
*/
void GL_DrawFrame() {
    if ( options.visual ) {
        GL_ProcessMessages();
        GL_UpdateMovement();
        GL_UpdateLeakPoint();
        previewView.fieldOfView = Preview_CvarFloat( "r_fov" );
        wglMakeCurrent( previewDC, previewGLContext );
        GL_SetState( previewState | GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE );
        glColor3f( 1, 1, 1 );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        GL_SetupView();
        GL_DrawWorld();
        GL_DrawDebugScene();
        SwapBuffers( previewDC );
    }
}

/* Toggle the polygon hit along the view direction; mask 0x78 excludes polygon flags from picking. */
/*
=============
GL_PickPolygon

Trace along the camera direction and toggle selection on the polygon hit by
the trace.
=============
*/
void GL_PickPolygon() {
    if ( bspRoot ) {
        Vector3d start = PreviewVector( previewView.origin );
        Vector3d end = start + PreviewVector( previewView.forward ) * PREVIEW_PICK_DISTANCE;
        TraceResult trace;
        TraceSegment( start, end, &trace, 0x78 );
        if ( trace.hit )
            trace.hit->selected = !trace.hit->selected;
    }
}

/* Upload all loaded shader images as mipmapped textures (picMip drops the top levels). */
/*
=============
GL_InitTextures

Upload loaded shader images and their mip levels as preview textures. Apply
the configured texture filters and discard initial mip levels as requested.
=============
*/
void GL_InitTextures() {
    if ( !options.visual )
        return;
    wglMakeCurrent( previewDC, previewGLContext );
    unsigned char *buffer = new unsigned char[PREVIEW_IMAGE_BUFFER_BYTES];
    if ( !buffer )
        Q3Map_Error( "out of memory: couldn't allocate %i bytes for image loading\n", PREVIEW_IMAGE_BUFFER_BYTES );
    ShaderInfo *shader;
    for ( int i = 0; i < PREVIEW_MAX_SHADERS; i++ ) {
        shader = &shaders[i];
        if ( !shader->image.pixels )
            continue;
        GL_BindTexture( 0, GL_TEXTURE_2D, PREVIEW_SHADER_TEXTURE_BASE + i );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, previewMinTextureFilter );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, previewMagTextureFilter );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
        if ( shader->image.width * shader->image.height * 4 > PREVIEW_IMAGE_BUFFER_BYTES ) {
            gluBuild2DMipmaps( GL_TEXTURE_2D, 4, shader->image.width, shader->image.height, GL_RGBA, GL_UNSIGNED_BYTE, buffer );
        } else {
            memcpy( buffer, shader->image.pixels, shader->image.width * shader->image.height * 4 );
            int width = shader->image.width;
            int height = shader->image.height;
            int level;
            for ( level = 0; level < options.picMip && (width > 1 || height > 1); level++ ) {
                GL_MipMap( buffer, width, height );
                width = (width + 1) >> 1;
                height = (height + 1) >> 1;
            }
            for ( level = 0; ; level++ ) {
                glTexImage2D( GL_TEXTURE_2D, level, 4, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer );
                if ( width == 1 && height == 1 )
                    break;
                GL_MipMap( buffer, width, height );
                width = (width + 1) >> 1;
                height = (height + 1) >> 1;
            }
            glGetError();
        }
    }
    delete[] buffer;
}

/*
=============
GL_LightmapColorToBytes

Convert a lightmap color to RGB bytes and apply the overbright scale. Halve
that scale while the bright display gamma is active.
=============
*/
static void GL_LightmapColorToBytes( const Color3f &color, unsigned char *rgb ) {
    color.ToBytes( rgb );
    float scale = options.overbrightScale;
    if ( previewBrightGamma )
        scale *= 0.5f;
    if ( scale ) {
        for ( int i = 0; i < 3; i++ )
            rgb[i] = min( 255.0f, rgb[i] * scale );
    }
}

/* Upload every lightmap page: samples to 8-bit RGB (overbright scale, halved with the gamma toggle), one texture per page. */
/*
=============
GL_UpdateLightmaps

Convert all lightmap pages to RGB bytes and upload them with the current
lightmap filter.
=============
*/
void GL_UpdateLightmaps() {
    unsigned char image[PREVIEW_LIGHTMAP_SIZE][PREVIEW_LIGHTMAP_SIZE][3];
    if ( !options.visual || numLightmaps <= 0 )
        return;
    for ( int i = 0; i < numLightmaps; i++ ) {
        LightmapSample (*samples)[PREVIEW_LIGHTMAP_SIZE] = lightmapSamples[i].samples;
        for ( int y = 0; y < PREVIEW_LIGHTMAP_SIZE; y++ ) {
            for ( int x = 0; x < PREVIEW_LIGHTMAP_SIZE; x++ ) {
                Color3f color = samples[y][x].color;
                GL_LightmapColorToBytes( color, image[y][x] );
            }
        }
        GL_SelectTexture( 0 );
        GL_BindTexture( 0, GL_TEXTURE_2D, i + 1 );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, previewLightmapFilter );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, previewLightmapFilter );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
        glTexImage2D( GL_TEXTURE_2D, 0, 3, PREVIEW_LIGHTMAP_SIZE, PREVIEW_LIGHTMAP_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, image );
    }
}

