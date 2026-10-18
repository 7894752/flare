/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Cached OpenGL state for the preview renderer.
===========================================================================
*/
#include <windows.h>
#include <GL/gl.h>
#include "flare/mathlib.h"
#include "flare/preview_gl.h"
#include "flare/preview_view.h"

#define GL_TEXTURE_CUBE_MAP_ARB 0x8513
#define GL_TEXTURE_3D 0x806F

/* Four two-bit texture targets, followed by the array and render-state bits. */
enum {
    MAX_PREVIEW_TEXTURE_UNITS = 4,
    GLSTATE_TEXTURE_TARGET_BITS = 2,
    GLSTATE_TEXTURE_UNIT_MASK = (1 << GLSTATE_TEXTURE_TARGET_BITS) - 1,
    GLSTATE_DEPTH_WRITE_SHIFT = 15,
    GLSTATE_COLOR_WRITE_SHIFT = 16,
    GLSTATE_ALPHA_WRITE_SHIFT = 17,
    PREVIEW_STENCIL_MASK = 0xFF,
    GLSTATE_TEXTURE_TARGET_MASK = 0xFF,
    GLSTATE_TEXCOORD0_ARRAY = 0x100,
    GLSTATE_TEXCOORD_ARRAY_MASK = 0xF00,
    GLSTATE_COLOR_ARRAY = 0x1000,
    GLSTATE_NORMAL_ARRAY = 0x2000,
    GLSTATE_VERTEX_ARRAY = 0x4000,
    GLSTATE_DEPTH_WRITE = 0x8000,
    GLSTATE_COLOR_WRITE = 0x10000,
    GLSTATE_ALPHA_WRITE = 0x20000,
    GLSTATE_STENCIL_WRITE = 0x40000,
    GLSTATE_DEPTH_TEST = 0x80000,
    GLSTATE_ALPHA_TEST = 0x100000,
    GLSTATE_STENCIL_TEST = 0x200000,
    GLSTATE_COLOR_WRITE_MASK = GLSTATE_COLOR_WRITE | GLSTATE_ALPHA_WRITE,
    GLSTATE_WRITE_MASK = GLSTATE_DEPTH_WRITE | GLSTATE_COLOR_WRITE_MASK | GLSTATE_STENCIL_WRITE,
    GLSTATE_TEST_MASK = GLSTATE_DEPTH_TEST | GLSTATE_ALPHA_TEST | GLSTATE_STENCIL_TEST
};

/* Preview state counter; never read in this program. */
int previewStateSerial;
int previewNumTextureUnits = 0;
GLuint previewBoundTextures[MAX_PREVIEW_TEXTURE_UNITS] = { 0 };
int previewActiveTexture = 0;
int previewClientTexture = 0;
GLenum previewCullFace = 0;
GLenum previewBlendSource = 0;
GLenum previewBlendDest = 0;
unsigned int previewState = 0;
unsigned int previewDrawFrame = 0;
struct Polygon *previewPolygons = NULL;
int previewNumPolygons = 0;
/* Camera vectors, angles, clipping distances and leak points share one record.
 * Its vector storage is plain data so zero initialization needs no constructor. */
PreviewView previewView = { 0 };

/*
=============
GL_CullFace

Update the cached face-culling mode, enabling or disabling culling as needed.
=============
*/
inline void GL_CullFace( GLenum face ) {
    if ( face != previewCullFace ) {
        if ( !face ) {
            glDisable( GL_CULL_FACE );
        } else {
            glCullFace( face );
            if ( !previewCullFace )
                glEnable( GL_CULL_FACE );
        }
        previewCullFace = face;
    }
}

/*
=============
GL_SetState

Apply changed texture targets, client arrays, write masks and tests from the
render-state bits. Cache the new state and texture-unit selections.
=============
*/
void GL_SetState( unsigned int state ) {
    static const GLenum textureTargets[] = {
        0, GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_3D
    };
    int changed = previewState ^ state;
    int unit, texCoordBit, targetShift;
    GLenum target;
    if ( !changed )
        return;

    /* Handle the selected client unit first to avoid a needless selection call. */
    if ( changed & GLSTATE_TEXCOORD_ARRAY_MASK ) {
        texCoordBit = GLSTATE_TEXCOORD0_ARRAY << previewClientTexture;
        if ( changed & texCoordBit ) {
            changed &= ~texCoordBit;
            if ( state & texCoordBit ) {
                glEnableClientState( GL_TEXTURE_COORD_ARRAY );
            } else {
                glDisableClientState( GL_TEXTURE_COORD_ARRAY );
            }
        }
        for ( unit = 0, texCoordBit = GLSTATE_TEXCOORD0_ARRAY;
              unit < previewNumTextureUnits; unit++, texCoordBit <<= 1 ) {
            if ( changed & texCoordBit ) {
                glClientActiveTextureARB( GL_TEXTURE0_ARB + unit );
                previewClientTexture = unit;
                if ( state & texCoordBit ) {
                    glEnableClientState( GL_TEXTURE_COORD_ARRAY );
                } else {
                    glDisableClientState( GL_TEXTURE_COORD_ARRAY );
                }
            }
        }
    }
    /* The target fields encode disabled, 2D, cube map or 3D. Update the active
     * server unit before selecting the other units that changed. */
    if ( changed & GLSTATE_TEXTURE_TARGET_MASK ) {
        targetShift = GLSTATE_TEXTURE_TARGET_BITS * previewActiveTexture;
        if ( (changed >> targetShift) & GLSTATE_TEXTURE_UNIT_MASK ) {
            target = textureTargets[(previewState >> targetShift) & GLSTATE_TEXTURE_UNIT_MASK];
            if ( target )
                glDisable( target );
            target = textureTargets[(state >> targetShift) & GLSTATE_TEXTURE_UNIT_MASK];
            if ( target )
                glEnable( target );
        }
        for ( unit = 0, targetShift = 0; unit < previewNumTextureUnits;
              unit++, targetShift += GLSTATE_TEXTURE_TARGET_BITS ) {
            if ( (changed >> targetShift) & GLSTATE_TEXTURE_UNIT_MASK ) {
                glActiveTextureARB( GL_TEXTURE0_ARB + unit );
                previewActiveTexture = unit;
                target = textureTargets[(previewState >> targetShift) & GLSTATE_TEXTURE_UNIT_MASK];
                if ( target )
                    glDisable( target );
                target = textureTargets[(state >> targetShift) & GLSTATE_TEXTURE_UNIT_MASK];
                if ( target )
                    glEnable( target );
            }
        }
    }
    if ( changed & GLSTATE_COLOR_ARRAY ) {
        if ( state & GLSTATE_COLOR_ARRAY ) {
            glEnableClientState( GL_COLOR_ARRAY );
        } else {
            glDisableClientState( GL_COLOR_ARRAY );
        }
    }
    if ( changed & GLSTATE_NORMAL_ARRAY ) {
        if ( state & GLSTATE_NORMAL_ARRAY ) {
            glEnableClientState( GL_NORMAL_ARRAY );
        } else {
            glDisableClientState( GL_NORMAL_ARRAY );
        }
    }
    if ( changed & GLSTATE_VERTEX_ARRAY ) {
        if ( state & GLSTATE_VERTEX_ARRAY ) {
            glEnableClientState( GL_VERTEX_ARRAY );
        } else {
            glDisableClientState( GL_VERTEX_ARRAY );
        }
    }
    if ( changed & GLSTATE_WRITE_MASK ) {
        if ( changed & GLSTATE_COLOR_WRITE_MASK ) {
            GLboolean rgb = (state >> GLSTATE_COLOR_WRITE_SHIFT) & 1;
            glColorMask( rgb, rgb, rgb, (state >> GLSTATE_ALPHA_WRITE_SHIFT) & 1 );
        }
        if ( changed & GLSTATE_DEPTH_WRITE )
            glDepthMask( (state >> GLSTATE_DEPTH_WRITE_SHIFT) & 1 );
        if ( changed & GLSTATE_STENCIL_WRITE )
            glStencilMask( (state & GLSTATE_STENCIL_WRITE) ? PREVIEW_STENCIL_MASK : 0 );
    }
    if ( changed & GLSTATE_TEST_MASK ) {
        if ( changed & GLSTATE_DEPTH_TEST ) {
            if ( state & GLSTATE_DEPTH_TEST ) {
                glEnable( GL_DEPTH_TEST );
            } else {
                glDisable( GL_DEPTH_TEST );
            }
        }
        if ( changed & GLSTATE_STENCIL_TEST ) {
            if ( state & GLSTATE_STENCIL_TEST ) {
                glEnable( GL_STENCIL_TEST );
            } else {
                glDisable( GL_STENCIL_TEST );
            }
        }
        if ( changed & GLSTATE_ALPHA_TEST ) {
            if ( state & GLSTATE_ALPHA_TEST ) {
                glEnable( GL_ALPHA_TEST );
            } else {
                glDisable( GL_ALPHA_TEST );
            }
        }
    }
    previewState = state;
}

/*
=============
GL_StencilIncrement

Configure a stencil increment pass with depth testing and face culling chosen
by the front flag.
=============
*/
void GL_StencilIncrement( bool front ) {
    GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_STENCIL_WRITE | GLSTATE_DEPTH_TEST | GLSTATE_STENCIL_TEST );
    glDepthFunc( GL_LESS );
    glStencilFunc( GL_ALWAYS, 0, ~0u );
    if ( front ) {
        GL_CullFace( GL_BACK );
        glStencilOp( GL_KEEP, GL_INCR, GL_KEEP );
    } else {
        GL_CullFace( GL_FRONT );
        glStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
    }
}

/*
=============
GL_StencilDecrement

Configure a stencil decrement pass with depth testing and face culling chosen
by the front flag.
=============
*/
void GL_StencilDecrement( bool front ) {
    GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_STENCIL_WRITE | GLSTATE_DEPTH_TEST | GLSTATE_STENCIL_TEST );
    glDepthFunc( GL_LESS );
    glStencilFunc( GL_ALWAYS, 0, ~0u );
    if ( front ) {
        GL_CullFace( GL_FRONT );
        glStencilOp( GL_KEEP, GL_DECR, GL_KEEP );
    } else {
        GL_CullFace( GL_BACK );
        glStencilOp( GL_KEEP, GL_KEEP, GL_DECR );
    }
}

/* Draw visible lit surfaces where the shadow-volume count is zero. */
/*
=============
GL_StencilLitPass

Configure lit-surface drawing at equal depth where the shadow-volume stencil
count is zero.
=============
*/
void GL_StencilLitPass() {
    GL_SetState( GLSTATE_VERTEX_ARRAY | GLSTATE_COLOR_WRITE_MASK | GLSTATE_DEPTH_TEST | GLSTATE_STENCIL_TEST );
    glStencilFunc( GL_EQUAL, 0, ~0u );
    glDepthFunc( GL_EQUAL );
    GL_CullFace( GL_FRONT );
}
