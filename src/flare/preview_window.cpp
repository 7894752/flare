/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

Win32 window and OpenGL context for the preview.
===========================================================================
*/
#define _WIN32_WINNT 0x0400
#include "flare/optiondefs.h"
#include "flare/preview.h"
#include "flare/mathlib.h"
#include <windows.h>
#include <GL/gl.h>
#include "flare/preview_gl.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "flare/print.h"


const char *previewWindowTitle = "FLARE Preview Window";
const char *previewWindowClass = "FLARE Preview Window";

void GL_InitExtensions();


/* OpenGL extension entry points. */
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT = NULL;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT = NULL;
void (APIENTRY *glActiveTextureARB)(GLenum texture) = NULL;
void (APIENTRY *glClientActiveTextureARB)(GLenum texture) = NULL;
PFNGLMULTITEXCOORD1DARBPROC glMultiTexCoord1dARB = NULL;
PFNGLMULTITEXCOORD1DVARBPROC glMultiTexCoord1dvARB = NULL;
PFNGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB = NULL;
PFNGLMULTITEXCOORD1FVARBPROC glMultiTexCoord1fvARB = NULL;
PFNGLMULTITEXCOORD1IARBPROC glMultiTexCoord1iARB = NULL;
PFNGLMULTITEXCOORD1IVARBPROC glMultiTexCoord1ivARB = NULL;
PFNGLMULTITEXCOORD1SARBPROC glMultiTexCoord1sARB = NULL;
PFNGLMULTITEXCOORD1SVARBPROC glMultiTexCoord1svARB = NULL;
PFNGLMULTITEXCOORD2DARBPROC glMultiTexCoord2dARB = NULL;
PFNGLMULTITEXCOORD2DVARBPROC glMultiTexCoord2dvARB = NULL;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB = NULL;
PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB = NULL;
PFNGLMULTITEXCOORD2IARBPROC glMultiTexCoord2iARB = NULL;
PFNGLMULTITEXCOORD2IVARBPROC glMultiTexCoord2ivARB = NULL;
PFNGLMULTITEXCOORD2SARBPROC glMultiTexCoord2sARB = NULL;
PFNGLMULTITEXCOORD2SVARBPROC glMultiTexCoord2svARB = NULL;
PFNGLMULTITEXCOORD3DARBPROC glMultiTexCoord3dARB = NULL;
PFNGLMULTITEXCOORD3DVARBPROC glMultiTexCoord3dvARB = NULL;
PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB = NULL;
PFNGLMULTITEXCOORD3FVARBPROC glMultiTexCoord3fvARB = NULL;
PFNGLMULTITEXCOORD3IARBPROC glMultiTexCoord3iARB = NULL;
PFNGLMULTITEXCOORD3IVARBPROC glMultiTexCoord3ivARB = NULL;
PFNGLMULTITEXCOORD3SARBPROC glMultiTexCoord3sARB = NULL;
PFNGLMULTITEXCOORD3SVARBPROC glMultiTexCoord3svARB = NULL;
PFNGLMULTITEXCOORD4DARBPROC glMultiTexCoord4dARB = NULL;
PFNGLMULTITEXCOORD4DVARBPROC glMultiTexCoord4dvARB = NULL;
PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB = NULL;
PFNGLMULTITEXCOORD4FVARBPROC glMultiTexCoord4fvARB = NULL;
PFNGLMULTITEXCOORD4IARBPROC glMultiTexCoord4iARB = NULL;
PFNGLMULTITEXCOORD4IVARBPROC glMultiTexCoord4ivARB = NULL;
PFNGLMULTITEXCOORD4SARBPROC glMultiTexCoord4sARB = NULL;
PFNGLMULTITEXCOORD4SVARBPROC glMultiTexCoord4svARB = NULL;
PFNGLTEXIMAGE3DEXTPROC glTexImage3DEXT = NULL;
PFNGLTEXSUBIMAGE3DEXTPROC glTexSubImage3DEXT = NULL;
PFNGLAREPROGRAMSRESIDENTNVPROC glAreProgramsResidentNV = NULL;
PFNGLBINDPROGRAMNVPROC glBindProgramNV = NULL;
PFNGLDELETEPROGRAMSNVPROC glDeleteProgramsNV = NULL;
PFNGLEXECUTEPROGRAMNVPROC glExecuteProgramNV = NULL;
PFNGLGENPROGRAMSNVPROC glGenProgramsNV = NULL;
PFNGLGETPROGRAMPARAMETERDVNVPROC glGetProgramParameterdvNV = NULL;
PFNGLGETPROGRAMPARAMETERFVNVPROC glGetProgramParameterfvNV = NULL;
PFNGLGETPROGRAMIVNVPROC glGetProgramivNV = NULL;
PFNGLGETPROGRAMSTRINGNVPROC glGetProgramStringNV = NULL;
PFNGLGETTRACKMATRIXIVNVPROC glGetTrackMatrixivNV = NULL;
PFNGLGETVERTEXATTRIBDVNVPROC glGetVertexAttribdvNV = NULL;
PFNGLGETVERTEXATTRIBFVNVPROC glGetVertexAttribfvNV = NULL;
PFNGLGETVERTEXATTRIBIVNVPROC glGetVertexAttribivNV = NULL;
PFNGLGETVERTEXATTRIBPOINTERVNVPROC glGetVertexAttribPointervNV = NULL;
PFNGLISPROGRAMNVPROC glIsProgramNV = NULL;
PFNGLLOADPROGRAMNVPROC glLoadProgramNV = NULL;
PFNGLPROGRAMPARAMETER4DNVPROC glProgramParameter4dNV = NULL;
PFNGLPROGRAMPARAMETER4DVNVPROC glProgramParameter4dvNV = NULL;
PFNGLPROGRAMPARAMETER4FNVPROC glProgramParameter4fNV = NULL;
PFNGLPROGRAMPARAMETER4FVNVPROC glProgramParameter4fvNV = NULL;
PFNGLPROGRAMPARAMETERS4DVNVPROC glProgramParameters4dvNV = NULL;
PFNGLPROGRAMPARAMETERS4FVNVPROC glProgramParameters4fvNV = NULL;
PFNGLREQUESTRESIDENTPROGRAMSNVPROC glRequestResidentProgramsNV = NULL;
PFNGLTRACKMATRIXNVPROC glTrackMatrixNV = NULL;
PFNGLVERTEXATTRIBPOINTERNVPROC glVertexAttribPointerNV = NULL;
PFNGLVERTEXATTRIB1DNVPROC glVertexAttrib1dNV = NULL;
PFNGLVERTEXATTRIB1DVNVPROC glVertexAttrib1dvNV = NULL;
PFNGLVERTEXATTRIB1FNVPROC glVertexAttrib1fNV = NULL;
PFNGLVERTEXATTRIB1FVNVPROC glVertexAttrib1fvNV = NULL;
PFNGLVERTEXATTRIB1SNVPROC glVertexAttrib1sNV = NULL;
PFNGLVERTEXATTRIB1SVNVPROC glVertexAttrib1svNV = NULL;
PFNGLVERTEXATTRIB2DNVPROC glVertexAttrib2dNV = NULL;
PFNGLVERTEXATTRIB2DVNVPROC glVertexAttrib2dvNV = NULL;
PFNGLVERTEXATTRIB2FNVPROC glVertexAttrib2fNV = NULL;
PFNGLVERTEXATTRIB2FVNVPROC glVertexAttrib2fvNV = NULL;
PFNGLVERTEXATTRIB2SNVPROC glVertexAttrib2sNV = NULL;
PFNGLVERTEXATTRIB2SVNVPROC glVertexAttrib2svNV = NULL;
PFNGLVERTEXATTRIB3DNVPROC glVertexAttrib3dNV = NULL;
PFNGLVERTEXATTRIB3DVNVPROC glVertexAttrib3dvNV = NULL;
PFNGLVERTEXATTRIB3FNVPROC glVertexAttrib3fNV = NULL;
PFNGLVERTEXATTRIB3FVNVPROC glVertexAttrib3fvNV = NULL;
PFNGLVERTEXATTRIB3SNVPROC glVertexAttrib3sNV = NULL;
PFNGLVERTEXATTRIB3SVNVPROC glVertexAttrib3svNV = NULL;
PFNGLVERTEXATTRIB4DNVPROC glVertexAttrib4dNV = NULL;
PFNGLVERTEXATTRIB4DVNVPROC glVertexAttrib4dvNV = NULL;
PFNGLVERTEXATTRIB4FNVPROC glVertexAttrib4fNV = NULL;
PFNGLVERTEXATTRIB4FVNVPROC glVertexAttrib4fvNV = NULL;
PFNGLVERTEXATTRIB4SNVPROC glVertexAttrib4sNV = NULL;
PFNGLVERTEXATTRIB4SVNVPROC glVertexAttrib4svNV = NULL;
PFNGLVERTEXATTRIB4UBVNVPROC glVertexAttrib4ubvNV = NULL;
PFNGLVERTEXATTRIBS1DVNVPROC glVertexAttribs1dvNV = NULL;
PFNGLVERTEXATTRIBS1FVNVPROC glVertexAttribs1fvNV = NULL;
PFNGLVERTEXATTRIBS1SVNVPROC glVertexAttribs1svNV = NULL;
PFNGLVERTEXATTRIBS2DVNVPROC glVertexAttribs2dvNV = NULL;
PFNGLVERTEXATTRIBS2FVNVPROC glVertexAttribs2fvNV = NULL;
PFNGLVERTEXATTRIBS2SVNVPROC glVertexAttribs2svNV = NULL;
PFNGLVERTEXATTRIBS3DVNVPROC glVertexAttribs3dvNV = NULL;
PFNGLVERTEXATTRIBS3FVNVPROC glVertexAttribs3fvNV = NULL;
PFNGLVERTEXATTRIBS3SVNVPROC glVertexAttribs3svNV = NULL;
PFNGLVERTEXATTRIBS4DVNVPROC glVertexAttribs4dvNV = NULL;
PFNGLVERTEXATTRIBS4FVNVPROC glVertexAttribs4fvNV = NULL;
PFNGLVERTEXATTRIBS4SVNVPROC glVertexAttribs4svNV = NULL;
PFNGLVERTEXATTRIBS4UBVNVPROC glVertexAttribs4ubvNV = NULL;
PFNGLFLUSHVERTEXARRAYRANGENVPROC glFlushVertexArrayRangeNV = NULL;
PFNGLVERTEXARRAYRANGENVPROC glVertexArrayRangeNV = NULL;
PFNWGLALLOCATEMEMORYNVPROC wglAllocateMemoryNV = NULL;
PFNWGLFREEMEMORYNVPROC wglFreeMemoryNV = NULL;
PFNGLDELETEFENCESNVPROC glDeleteFencesNV = NULL;
PFNGLGENFENCESNVPROC glGenFencesNV = NULL;
PFNGLISFENCENVPROC glIsFenceNV = NULL;
PFNGLTESTFENCENVPROC glTestFenceNV = NULL;
PFNGLGETFENCEIVNVPROC glGetFenceivNV = NULL;
PFNGLFINISHFENCENVPROC glFinishFenceNV = NULL;
PFNGLSETFENCENVPROC glSetFenceNV = NULL;

int previewWindowWidth = 0;
int previewWindowHeight = 0;
bool previewFullscreen = false;
HWND previewWindow = NULL;
HDC previewDC = NULL;
HGLRC previewGLContext = NULL;
/* Window information block; cleared at startup and not otherwise used. */
char previewWindowInfo[128] = { 0 };

#define GL_MAX_TEXTURE_UNITS_ARB 0x84E2

#define LOAD_GL_PROC(name, type) name = (type)wglGetProcAddress( #name )

/* User controls that refresh preview textures and gamma. */

/*
=============
GL_ToggleTextureFilter

Switch lightmaps between nearest and linear filtering and refresh their
textures when visualization is enabled.
=============
*/
void GL_ToggleTextureFilter() {
    if ( options.visual ) {
        /* Exchange the two supported filters. */
        previewLightmapFilter = GL_NEAREST + GL_LINEAR - previewLightmapFilter;
        if ( numLightmaps )
            GL_UpdateLightmaps();
    }
}

/*
=============
GL_ToggleGamma

Toggle between linear and bright display gamma, refresh lightmaps and redraw
the preview. Apply the new ramp to all three display color channels.
=============
*/
void GL_ToggleGamma() {
    unsigned short ramp[3][256];
    int i;
    if ( !options.visual )
        return;
    previewBrightGamma = !previewBrightGamma;
    if ( previewBrightGamma ) {
        /* Double the linear 16-bit ramp, saturating its upper half. */
        for ( i = 0; i < 128; i++ )
            ramp[0][i] = i * 514;
        for ( ; i < 256; i++ )
            ramp[0][i] = 65535;
    } else {
        for ( i = 0; i < 256; i++ )
            ramp[0][i] = i * 257;
    }
    for ( i = 0; i < 256; i++ ) {
        ramp[1][i] = ramp[0][i];
        ramp[2][i] = ramp[0][i];
    }
    GL_UpdateLightmaps();
    GL_DrawFrame();
    SetDeviceGammaRamp( previewDC, ramp );
}

/*
=============
GL_WindowProc

Dispatch window keyboard, mouse and focus events to preview input handling.
Convert wheel motion into a momentary binding and post a quit message on
close.
=============
*/
LRESULT CALLBACK GL_WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam ) {
    switch ( message ) {
    case WM_CLOSE:
        PostQuitMessage( 0 );
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        Key_Event( wParam, false );
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        Key_Event( wParam, true );
        return 0;
    case WM_KILLFOCUS:
        Key_ReleaseMouse();
        return 0;
    case WM_SETFOCUS:
        Key_CaptureMouse( window );
        return 0;
    case WM_MOUSEMOVE:
        if ( GetCapture() == window )
            Key_MouseEvent( LOWORD( lParam ), HIWORD( lParam ) );
        return 0;
    case WM_LBUTTONUP:
        Key_Event( VK_LBUTTON, false );
        return 0;
    case WM_LBUTTONDOWN:
        Key_Event( VK_LBUTTON, true );
        return 0;
    case WM_RBUTTONUP:
        Key_Event( VK_RBUTTON, false );
        return 0;
    case WM_RBUTTONDOWN:
        Key_Event( VK_RBUTTON, true );
        return 0;
    case WM_MBUTTONUP:
        Key_Event( VK_MBUTTON, false );
        return 0;
    case WM_MBUTTONDOWN:
        Key_Event( VK_MBUTTON, true );
        return 0;
    case WM_MOUSEWHEEL: {
        /* Wheel bindings are momentary buttons at the end of the key table. */
        int key = (short)HIWORD( wParam ) < 0 ? 255 : 254;
        Key_Event( key, true );
        Key_Event( key, false );
        return 0;
    }
    }
    return DefWindowProc( window, message, wParam, lParam );
}

/*
=============
GL_RegisterWindowClass

Register the preview window class with its message handler and a private
device context.
=============
*/
void GL_RegisterWindowClass() {
    WNDCLASSEX wc;
    memset( &wc, 0, sizeof( wc ) );
    wc.cbSize = sizeof( wc );
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = GL_WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)GetCurrentProcess();
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.lpszClassName = previewWindowClass;
    RegisterClassEx( &wc );
}

/*
=============
GL_ChoosePixelFormat

Find an accelerated, double-buffered OpenGL pixel format with suitable color
and depth precision. Prefer formats without extra stencil or accumulation
bits.
=============
*/
int GL_ChoosePixelFormat() {
    PIXELFORMATDESCRIPTOR pfd;
    const DWORD requiredFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    int bestStencil = INT_MAX, bestAccum = INT_MAX;
    int bestFormat = 0;
    int format = 1;
    int bestDepth = 0, bestColor = 0;
    while ( DescribePixelFormat( previewDC, format, sizeof( pfd ), &pfd ) ) {
        /* Favor color/depth precision without extra stencil or accumulation bits. */
        if ( !(~pfd.dwFlags & requiredFlags) &&
             !(pfd.dwFlags & (PFD_GENERIC_FORMAT | PFD_SWAP_COPY)) &&
             pfd.cDepthBits >= 16 && pfd.cColorBits >= 15 &&
             pfd.cDepthBits >= bestDepth && pfd.cStencilBits <= bestStencil &&
             pfd.cColorBits >= bestColor && pfd.cAccumBits <= bestAccum ) {
            bestFormat = format;
            bestDepth = pfd.cDepthBits;
            bestStencil = pfd.cStencilBits;
            bestColor = pfd.cColorBits;
            bestAccum = pfd.cAccumBits;
        }
        format++;
    }
    return bestFormat;
}

/*
=============
GL_DeleteContext

Delete the preview OpenGL context and clear its stored handle.
=============
*/
inline void GL_DeleteContext() {
    wglDeleteContext( previewGLContext );
    previewGLContext = NULL;
}

/*
=============
GL_DestroyWindow

Shut down preview input, restore fullscreen display settings and destroy the
context and window.
=============
*/
void GL_DestroyWindow() {
    Key_Shutdown();
    if ( previewFullscreen )
        ChangeDisplaySettings( NULL, 0 );
    GL_DeleteContext();
    DestroyWindow( previewWindow );
}

/*
=============
GL_HasExtension

Return whether the OpenGL extension list contains the complete extension name.
=============
*/
bool GL_HasExtension( const char *name ) {
    const char *extensions = (const char *)glGetString( GL_EXTENSIONS );
    int length = strlen( name );
    const char *match = strstr( extensions, name );
    while ( match ) {
        if ( (match == extensions || isspace( match[-1] )) &&
             (!match[length] || isspace( match[length] )) )
            return true;
        match = strstr( match + 1, name );
    }
    return false;
}

/*
=============
GL_ProcessMessages

Dispatch pending preview window messages. Restore linear gamma and exit when a
quit message is received.
=============
*/
void GL_ProcessMessages() {
    MSG message;
    while ( PeekMessage( &message, previewWindow, 0, 0, PM_NOREMOVE ) ) {
        if ( !GetMessage( &message, previewWindow, 0, 0 ) ) {
            GL_ResetGamma();
            exit( 0 );
        }
        DispatchMessage( &message );
    }
}

/* Pass a rectangle to Win32 by address. The window-size setup fills its fields. */
class WindowRect : public RECT {
public:
    WindowRect() {}
    operator LPRECT();
};

/*
=============
GL_InitExtensions

Load entry points for supported vertex-array, multitexture and NVIDIA
extensions.
=============
*/
void GL_InitExtensions() {
    if ( GL_HasExtension( "GL_EXT_compiled_vertex_array" ) ) {
        LOAD_GL_PROC( glLockArraysEXT, PFNGLLOCKARRAYSEXTPROC );
        LOAD_GL_PROC( glUnlockArraysEXT, PFNGLUNLOCKARRAYSEXTPROC );
    }
    if ( GL_HasExtension( "GL_ARB_multitexture" ) ) {
        glActiveTextureARB = (void (APIENTRY *)(GLenum))wglGetProcAddress( "glActiveTextureARB" );
        glClientActiveTextureARB = (void (APIENTRY *)(GLenum))wglGetProcAddress( "glClientActiveTextureARB" );
        LOAD_GL_PROC( glMultiTexCoord1dARB, PFNGLMULTITEXCOORD1DARBPROC );
        LOAD_GL_PROC( glMultiTexCoord1dvARB, PFNGLMULTITEXCOORD1DVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord1fARB, PFNGLMULTITEXCOORD1FARBPROC );
        LOAD_GL_PROC( glMultiTexCoord1fvARB, PFNGLMULTITEXCOORD1FVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord1iARB, PFNGLMULTITEXCOORD1IARBPROC );
        LOAD_GL_PROC( glMultiTexCoord1ivARB, PFNGLMULTITEXCOORD1IVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord1sARB, PFNGLMULTITEXCOORD1SARBPROC );
        LOAD_GL_PROC( glMultiTexCoord1svARB, PFNGLMULTITEXCOORD1SVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord2dARB, PFNGLMULTITEXCOORD2DARBPROC );
        LOAD_GL_PROC( glMultiTexCoord2dvARB, PFNGLMULTITEXCOORD2DVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord2fARB, PFNGLMULTITEXCOORD2FARBPROC );
        LOAD_GL_PROC( glMultiTexCoord2fvARB, PFNGLMULTITEXCOORD2FVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord2iARB, PFNGLMULTITEXCOORD2IARBPROC );
        LOAD_GL_PROC( glMultiTexCoord2ivARB, PFNGLMULTITEXCOORD2IVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord2sARB, PFNGLMULTITEXCOORD2SARBPROC );
        LOAD_GL_PROC( glMultiTexCoord2svARB, PFNGLMULTITEXCOORD2SVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord3dARB, PFNGLMULTITEXCOORD3DARBPROC );
        LOAD_GL_PROC( glMultiTexCoord3dvARB, PFNGLMULTITEXCOORD3DVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord3fARB, PFNGLMULTITEXCOORD3FARBPROC );
        LOAD_GL_PROC( glMultiTexCoord3fvARB, PFNGLMULTITEXCOORD3FVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord3iARB, PFNGLMULTITEXCOORD3IARBPROC );
        LOAD_GL_PROC( glMultiTexCoord3ivARB, PFNGLMULTITEXCOORD3IVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord3sARB, PFNGLMULTITEXCOORD3SARBPROC );
        LOAD_GL_PROC( glMultiTexCoord3svARB, PFNGLMULTITEXCOORD3SVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord4dARB, PFNGLMULTITEXCOORD4DARBPROC );
        LOAD_GL_PROC( glMultiTexCoord4dvARB, PFNGLMULTITEXCOORD4DVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord4fARB, PFNGLMULTITEXCOORD4FARBPROC );
        LOAD_GL_PROC( glMultiTexCoord4fvARB, PFNGLMULTITEXCOORD4FVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord4iARB, PFNGLMULTITEXCOORD4IARBPROC );
        LOAD_GL_PROC( glMultiTexCoord4ivARB, PFNGLMULTITEXCOORD4IVARBPROC );
        LOAD_GL_PROC( glMultiTexCoord4sARB, PFNGLMULTITEXCOORD4SARBPROC );
        LOAD_GL_PROC( glMultiTexCoord4svARB, PFNGLMULTITEXCOORD4SVARBPROC );
    }
    if ( GL_HasExtension( "GL_EXT_texture3D" ) ) {
        LOAD_GL_PROC( glTexImage3DEXT, PFNGLTEXIMAGE3DEXTPROC );
        LOAD_GL_PROC( glTexSubImage3DEXT, PFNGLTEXSUBIMAGE3DEXTPROC );
    }
    if ( GL_HasExtension( "GL_NV_vertex_program" ) ) {
        LOAD_GL_PROC( glAreProgramsResidentNV, PFNGLAREPROGRAMSRESIDENTNVPROC );
        LOAD_GL_PROC( glBindProgramNV, PFNGLBINDPROGRAMNVPROC );
        LOAD_GL_PROC( glDeleteProgramsNV, PFNGLDELETEPROGRAMSNVPROC );
        LOAD_GL_PROC( glExecuteProgramNV, PFNGLEXECUTEPROGRAMNVPROC );
        LOAD_GL_PROC( glGenProgramsNV, PFNGLGENPROGRAMSNVPROC );
        LOAD_GL_PROC( glGetProgramParameterdvNV, PFNGLGETPROGRAMPARAMETERDVNVPROC );
        LOAD_GL_PROC( glGetProgramParameterfvNV, PFNGLGETPROGRAMPARAMETERFVNVPROC );
        LOAD_GL_PROC( glGetProgramivNV, PFNGLGETPROGRAMIVNVPROC );
        LOAD_GL_PROC( glGetProgramStringNV, PFNGLGETPROGRAMSTRINGNVPROC );
        LOAD_GL_PROC( glGetTrackMatrixivNV, PFNGLGETTRACKMATRIXIVNVPROC );
        LOAD_GL_PROC( glGetVertexAttribdvNV, PFNGLGETVERTEXATTRIBDVNVPROC );
        LOAD_GL_PROC( glGetVertexAttribfvNV, PFNGLGETVERTEXATTRIBFVNVPROC );
        LOAD_GL_PROC( glGetVertexAttribivNV, PFNGLGETVERTEXATTRIBIVNVPROC );
        LOAD_GL_PROC( glGetVertexAttribPointervNV, PFNGLGETVERTEXATTRIBPOINTERVNVPROC );
        LOAD_GL_PROC( glIsProgramNV, PFNGLISPROGRAMNVPROC );
        LOAD_GL_PROC( glLoadProgramNV, PFNGLLOADPROGRAMNVPROC );
        LOAD_GL_PROC( glProgramParameter4dNV, PFNGLPROGRAMPARAMETER4DNVPROC );
        LOAD_GL_PROC( glProgramParameter4dvNV, PFNGLPROGRAMPARAMETER4DVNVPROC );
        LOAD_GL_PROC( glProgramParameter4fNV, PFNGLPROGRAMPARAMETER4FNVPROC );
        LOAD_GL_PROC( glProgramParameter4fvNV, PFNGLPROGRAMPARAMETER4FVNVPROC );
        LOAD_GL_PROC( glProgramParameters4dvNV, PFNGLPROGRAMPARAMETERS4DVNVPROC );
        LOAD_GL_PROC( glProgramParameters4fvNV, PFNGLPROGRAMPARAMETERS4FVNVPROC );
        LOAD_GL_PROC( glRequestResidentProgramsNV, PFNGLREQUESTRESIDENTPROGRAMSNVPROC );
        LOAD_GL_PROC( glTrackMatrixNV, PFNGLTRACKMATRIXNVPROC );
        LOAD_GL_PROC( glVertexAttribPointerNV, PFNGLVERTEXATTRIBPOINTERNVPROC );
        LOAD_GL_PROC( glVertexAttrib1dNV, PFNGLVERTEXATTRIB1DNVPROC );
        LOAD_GL_PROC( glVertexAttrib1dvNV, PFNGLVERTEXATTRIB1DVNVPROC );
        LOAD_GL_PROC( glVertexAttrib1fNV, PFNGLVERTEXATTRIB1FNVPROC );
        LOAD_GL_PROC( glVertexAttrib1fvNV, PFNGLVERTEXATTRIB1FVNVPROC );
        LOAD_GL_PROC( glVertexAttrib1sNV, PFNGLVERTEXATTRIB1SNVPROC );
        LOAD_GL_PROC( glVertexAttrib1svNV, PFNGLVERTEXATTRIB1SVNVPROC );
        LOAD_GL_PROC( glVertexAttrib2dNV, PFNGLVERTEXATTRIB2DNVPROC );
        LOAD_GL_PROC( glVertexAttrib2dvNV, PFNGLVERTEXATTRIB2DVNVPROC );
        LOAD_GL_PROC( glVertexAttrib2fNV, PFNGLVERTEXATTRIB2FNVPROC );
        LOAD_GL_PROC( glVertexAttrib2fvNV, PFNGLVERTEXATTRIB2FVNVPROC );
        LOAD_GL_PROC( glVertexAttrib2sNV, PFNGLVERTEXATTRIB2SNVPROC );
        LOAD_GL_PROC( glVertexAttrib2svNV, PFNGLVERTEXATTRIB2SVNVPROC );
        LOAD_GL_PROC( glVertexAttrib3dNV, PFNGLVERTEXATTRIB3DNVPROC );
        LOAD_GL_PROC( glVertexAttrib3dvNV, PFNGLVERTEXATTRIB3DVNVPROC );
        LOAD_GL_PROC( glVertexAttrib3fNV, PFNGLVERTEXATTRIB3FNVPROC );
        LOAD_GL_PROC( glVertexAttrib3fvNV, PFNGLVERTEXATTRIB3FVNVPROC );
        LOAD_GL_PROC( glVertexAttrib3sNV, PFNGLVERTEXATTRIB3SNVPROC );
        LOAD_GL_PROC( glVertexAttrib3svNV, PFNGLVERTEXATTRIB3SVNVPROC );
        LOAD_GL_PROC( glVertexAttrib4dNV, PFNGLVERTEXATTRIB4DNVPROC );
        LOAD_GL_PROC( glVertexAttrib4dvNV, PFNGLVERTEXATTRIB4DVNVPROC );
        LOAD_GL_PROC( glVertexAttrib4fNV, PFNGLVERTEXATTRIB4FNVPROC );
        LOAD_GL_PROC( glVertexAttrib4fvNV, PFNGLVERTEXATTRIB4FVNVPROC );
        LOAD_GL_PROC( glVertexAttrib4sNV, PFNGLVERTEXATTRIB4SNVPROC );
        LOAD_GL_PROC( glVertexAttrib4svNV, PFNGLVERTEXATTRIB4SVNVPROC );
        LOAD_GL_PROC( glVertexAttrib4ubvNV, PFNGLVERTEXATTRIB4UBVNVPROC );
        LOAD_GL_PROC( glVertexAttribs1dvNV, PFNGLVERTEXATTRIBS1DVNVPROC );
        LOAD_GL_PROC( glVertexAttribs1fvNV, PFNGLVERTEXATTRIBS1FVNVPROC );
        LOAD_GL_PROC( glVertexAttribs1svNV, PFNGLVERTEXATTRIBS1SVNVPROC );
        LOAD_GL_PROC( glVertexAttribs2dvNV, PFNGLVERTEXATTRIBS2DVNVPROC );
        LOAD_GL_PROC( glVertexAttribs2fvNV, PFNGLVERTEXATTRIBS2FVNVPROC );
        LOAD_GL_PROC( glVertexAttribs2svNV, PFNGLVERTEXATTRIBS2SVNVPROC );
        LOAD_GL_PROC( glVertexAttribs3dvNV, PFNGLVERTEXATTRIBS3DVNVPROC );
        LOAD_GL_PROC( glVertexAttribs3fvNV, PFNGLVERTEXATTRIBS3FVNVPROC );
        LOAD_GL_PROC( glVertexAttribs3svNV, PFNGLVERTEXATTRIBS3SVNVPROC );
        LOAD_GL_PROC( glVertexAttribs4dvNV, PFNGLVERTEXATTRIBS4DVNVPROC );
        LOAD_GL_PROC( glVertexAttribs4fvNV, PFNGLVERTEXATTRIBS4FVNVPROC );
        LOAD_GL_PROC( glVertexAttribs4svNV, PFNGLVERTEXATTRIBS4SVNVPROC );
        LOAD_GL_PROC( glVertexAttribs4ubvNV, PFNGLVERTEXATTRIBS4UBVNVPROC );
    }
    if ( GL_HasExtension( "GL_NV_vertex_array_range" ) ) {
        LOAD_GL_PROC( glFlushVertexArrayRangeNV, PFNGLFLUSHVERTEXARRAYRANGENVPROC );
        LOAD_GL_PROC( glVertexArrayRangeNV, PFNGLVERTEXARRAYRANGENVPROC );
        LOAD_GL_PROC( wglAllocateMemoryNV, PFNWGLALLOCATEMEMORYNVPROC );
        LOAD_GL_PROC( wglFreeMemoryNV, PFNWGLFREEMEMORYNVPROC );
    }
    if ( GL_HasExtension( "GL_NV_fence" ) ) {
        LOAD_GL_PROC( glDeleteFencesNV, PFNGLDELETEFENCESNVPROC );
        LOAD_GL_PROC( glGenFencesNV, PFNGLGENFENCESNVPROC );
        LOAD_GL_PROC( glIsFenceNV, PFNGLISFENCENVPROC );
        LOAD_GL_PROC( glTestFenceNV, PFNGLTESTFENCENVPROC );
        LOAD_GL_PROC( glGetFenceivNV, PFNGLGETFENCEIVNVPROC );
        LOAD_GL_PROC( glFinishFenceNV, PFNGLFINISHFENCENVPROC );
        LOAD_GL_PROC( glSetFenceNV, PFNGLSETFENCENVPROC );
    }
}

/*
=============
GL_CreateOpenGL

Choose the pixel format, create the preview context and initialize extensions
and cached state. Report setup failures and list the available extensions.
=============
*/
bool GL_CreateOpenGL() {
    PIXELFORMATDESCRIPTOR pfd;
    int format;
    const char *extensions;
    char *copy, *space;
    previewDC = GetDC( previewWindow );
    format = GL_ChoosePixelFormat();
    if ( !format ) {
        Q3Map_Warning( 1, "GL_CreateOpenGL: No valid pixel formats available\n" );
        return false;
    }
    if ( !SetPixelFormat( previewDC, format, &pfd ) ) {
        Q3Map_Warning( 1, "GL_CreateOpenGL: SetPixelFormat failed\n" );
        return false;
    }
    previewGLContext = wglCreateContext( previewDC );
    if ( !previewGLContext ) {
        Q3Map_Warning( 1, "GL_CreateOpenGL: Couldn't create OpenGL context\n" );
        return false;
    }
    wglMakeCurrent( previewDC, previewGLContext );
    GL_InitExtensions();
    glEnable( GL_TEXTURE_2D );
    glEnable( GL_DEPTH_TEST );
    /* Unit 0 uses 2D texturing; depth testing and RGB/depth writes are enabled. */
    previewState = 0x98001;
    previewActiveTexture = 0;
    previewClientTexture = 0;
    glGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &previewNumTextureUnits );
    /* The state cache has four texture-unit slots. */
    if ( previewNumTextureUnits > 4 )
        previewNumTextureUnits = 4;
    extensions = (const char *)glGetString( GL_EXTENSIONS );
    copy = new char[strlen( extensions ) + 1];
    if ( !copy )
        Q3Map_Error( "Couldn't allocate storage to copy extensions string (%i bytes)\n", strlen( extensions ) + 1 );
    strcpy( copy, extensions );
    for ( space = strchr( copy, ' ' ); space; space = strchr( space + 1, ' ' ) )
        *space = '\n';
    Q3Map_Verbose( "GL_EXTENSIONS:\n--------------\n%s\n--------------\n", copy );
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
    return true;
}

/*
=============
GL_CreateWindow

Create the requested window or fullscreen display and initialize OpenGL and
input. Return false if window or context creation fails.
=============
*/
bool GL_CreateWindow( int width, int height, bool fullscreen ) {
    DEVMODE mode;
    WindowRect rect;
    DWORD exStyle, style;
    int x, y;
    GL_RegisterWindowClass();
    if ( fullscreen ) {
        memset( &mode, 0, sizeof( mode ) );
        mode.dmSize = sizeof( mode );
        mode.dmBitsPerPel = 32;
        mode.dmPelsWidth = width;
        mode.dmPelsHeight = height;
        mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
        ChangeDisplaySettings( &mode, CDS_FULLSCREEN );
    }
    previewWindowWidth = width;
    previewWindowHeight = height;
    previewFullscreen = fullscreen;
    if ( fullscreen ) {
        y = x = 0;
        style = WS_POPUP | WS_VISIBLE;
        exStyle = WS_EX_TOPMOST;
    } else {
        y = x = 16;
        style = WS_VISIBLE | WS_CAPTION | WS_SYSMENU;
        exStyle = WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME;
        rect.top = rect.left = 0;
        rect.right = width;
        rect.bottom = height;
        AdjustWindowRectEx( &rect, style, FALSE, exStyle );
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
    }
    previewWindow = CreateWindowEx( exStyle, previewWindowClass, previewWindowTitle,
        style, x, y, width, height, NULL, NULL, (HINSTANCE)GetCurrentProcess(), NULL );
    if ( !previewWindow ) {
        Q3Map_Warning( 1, "GL_CreateWindow: CreateWindowEx failed\n" );
        return false;
    }
    if ( !GL_CreateOpenGL() ) {
        DestroyWindow( previewWindow );
        previewWindow = NULL;
        return false;
    }
    Key_Init( previewWindow );
    return true;
}

/*
=============
WindowRect::operator LPRECT

Return this rectangle as a Win32 rectangle pointer.
=============
*/
WindowRect::operator LPRECT() {
    return this;
}
