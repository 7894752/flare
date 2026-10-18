/*
===========================================================================
flare (Call of Duty, 2003).

Distributed under the terms in COPYING (GPL version 3 or later,
with id Software's additional terms).

OpenGL extension entry points loaded by the preview window.
===========================================================================
*/
#ifndef FLARE_PREVIEW_GL_H
#define FLARE_PREVIEW_GL_H

#include <windows.h>
#include <GL/gl.h>

#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB 0x84C0
#endif

/* GL_EXT_compiled_vertex_array */
typedef void (APIENTRY *PFNGLLOCKARRAYSEXTPROC)(GLint first, GLsizei count);
typedef void (APIENTRY *PFNGLUNLOCKARRAYSEXTPROC)(void);

/* GL_ARB_multitexture */
typedef void (APIENTRY *PFNGLACTIVETEXTUREARBPROC)(GLenum texture);
typedef void (APIENTRY *PFNGLCLIENTACTIVETEXTUREARBPROC)(GLenum texture);
typedef void (APIENTRY *PFNGLMULTITEXCOORD1DARBPROC)(GLenum target, GLdouble s);
typedef void (APIENTRY *PFNGLMULTITEXCOORD1DVARBPROC)(GLenum target, const GLdouble *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD1FARBPROC)(GLenum target, GLfloat s);
typedef void (APIENTRY *PFNGLMULTITEXCOORD1FVARBPROC)(GLenum target, const GLfloat *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD1IARBPROC)(GLenum target, GLint s);
typedef void (APIENTRY *PFNGLMULTITEXCOORD1IVARBPROC)(GLenum target, const GLint *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD1SARBPROC)(GLenum target, GLshort s);
typedef void (APIENTRY *PFNGLMULTITEXCOORD1SVARBPROC)(GLenum target, const GLshort *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD2DARBPROC)(GLenum target, GLdouble s, GLdouble t);
typedef void (APIENTRY *PFNGLMULTITEXCOORD2DVARBPROC)(GLenum target, const GLdouble *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD2FARBPROC)(GLenum target, GLfloat s, GLfloat t);
typedef void (APIENTRY *PFNGLMULTITEXCOORD2FVARBPROC)(GLenum target, const GLfloat *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD2IARBPROC)(GLenum target, GLint s, GLint t);
typedef void (APIENTRY *PFNGLMULTITEXCOORD2IVARBPROC)(GLenum target, const GLint *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD2SARBPROC)(GLenum target, GLshort s, GLshort t);
typedef void (APIENTRY *PFNGLMULTITEXCOORD2SVARBPROC)(GLenum target, const GLshort *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD3DARBPROC)(GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (APIENTRY *PFNGLMULTITEXCOORD3DVARBPROC)(GLenum target, const GLdouble *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD3FARBPROC)(GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (APIENTRY *PFNGLMULTITEXCOORD3FVARBPROC)(GLenum target, const GLfloat *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD3IARBPROC)(GLenum target, GLint s, GLint t, GLint r);
typedef void (APIENTRY *PFNGLMULTITEXCOORD3IVARBPROC)(GLenum target, const GLint *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD3SARBPROC)(GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (APIENTRY *PFNGLMULTITEXCOORD3SVARBPROC)(GLenum target, const GLshort *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD4DARBPROC)(GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (APIENTRY *PFNGLMULTITEXCOORD4DVARBPROC)(GLenum target, const GLdouble *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD4FARBPROC)(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (APIENTRY *PFNGLMULTITEXCOORD4FVARBPROC)(GLenum target, const GLfloat *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD4IARBPROC)(GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (APIENTRY *PFNGLMULTITEXCOORD4IVARBPROC)(GLenum target, const GLint *v);
typedef void (APIENTRY *PFNGLMULTITEXCOORD4SARBPROC)(GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (APIENTRY *PFNGLMULTITEXCOORD4SVARBPROC)(GLenum target, const GLshort *v);

/* GL_EXT_texture3D */
typedef void (APIENTRY *PFNGLTEXIMAGE3DEXTPROC)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRY *PFNGLTEXSUBIMAGE3DEXTPROC)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels);

/* GL_NV_vertex_program */
typedef GLboolean (APIENTRY *PFNGLAREPROGRAMSRESIDENTNVPROC)(GLsizei n, const GLuint *ids, GLboolean *residences);
typedef void (APIENTRY *PFNGLBINDPROGRAMNVPROC)(GLenum target, GLuint id);
typedef void (APIENTRY *PFNGLDELETEPROGRAMSNVPROC)(GLsizei n, const GLuint *ids);
typedef void (APIENTRY *PFNGLEXECUTEPROGRAMNVPROC)(GLenum target, GLuint id, const GLfloat *params);
typedef void (APIENTRY *PFNGLGENPROGRAMSNVPROC)(GLsizei n, GLuint *ids);
typedef void (APIENTRY *PFNGLGETPROGRAMPARAMETERDVNVPROC)(GLenum target, GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRY *PFNGLGETPROGRAMPARAMETERFVNVPROC)(GLenum target, GLuint index, GLenum pname, GLfloat *params);
typedef void (APIENTRY *PFNGLGETPROGRAMIVNVPROC)(GLuint id, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETPROGRAMSTRINGNVPROC)(GLuint id, GLenum pname, GLubyte *program);
typedef void (APIENTRY *PFNGLGETTRACKMATRIXIVNVPROC)(GLenum target, GLuint address, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETVERTEXATTRIBDVNVPROC)(GLuint index, GLenum pname, GLdouble *params);
typedef void (APIENTRY *PFNGLGETVERTEXATTRIBFVNVPROC)(GLuint index, GLenum pname, GLfloat *params);
typedef void (APIENTRY *PFNGLGETVERTEXATTRIBIVNVPROC)(GLuint index, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETVERTEXATTRIBPOINTERVNVPROC)(GLuint index, GLenum pname, GLvoid **pointer);
typedef GLboolean (APIENTRY *PFNGLISPROGRAMNVPROC)(GLuint id);
typedef void (APIENTRY *PFNGLLOADPROGRAMNVPROC)(GLenum target, GLuint id, GLsizei len, const GLubyte *program);
typedef void (APIENTRY *PFNGLPROGRAMPARAMETER4DNVPROC)(GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY *PFNGLPROGRAMPARAMETER4DVNVPROC)(GLenum target, GLuint index, const GLdouble *v);
typedef void (APIENTRY *PFNGLPROGRAMPARAMETER4FNVPROC)(GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY *PFNGLPROGRAMPARAMETER4FVNVPROC)(GLenum target, GLuint index, const GLfloat *v);
typedef void (APIENTRY *PFNGLPROGRAMPARAMETERS4DVNVPROC)(GLenum target, GLuint index, GLuint count, const GLdouble *v);
typedef void (APIENTRY *PFNGLPROGRAMPARAMETERS4FVNVPROC)(GLenum target, GLuint index, GLuint count, const GLfloat *v);
typedef void (APIENTRY *PFNGLREQUESTRESIDENTPROGRAMSNVPROC)(GLsizei n, const GLuint *ids);
typedef void (APIENTRY *PFNGLTRACKMATRIXNVPROC)(GLenum target, GLuint address, GLenum matrix, GLenum transform);
typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTERNVPROC)(GLuint index, GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY *PFNGLVERTEXATTRIB1DNVPROC)(GLuint index, GLdouble x);
typedef void (APIENTRY *PFNGLVERTEXATTRIB1DVNVPROC)(GLuint index, const GLdouble *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB1FNVPROC)(GLuint index, GLfloat x);
typedef void (APIENTRY *PFNGLVERTEXATTRIB1FVNVPROC)(GLuint index, const GLfloat *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB1SNVPROC)(GLuint index, GLshort x);
typedef void (APIENTRY *PFNGLVERTEXATTRIB1SVNVPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB2DNVPROC)(GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRY *PFNGLVERTEXATTRIB2DVNVPROC)(GLuint index, const GLdouble *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB2FNVPROC)(GLuint index, GLfloat x, GLfloat y);
typedef void (APIENTRY *PFNGLVERTEXATTRIB2FVNVPROC)(GLuint index, const GLfloat *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB2SNVPROC)(GLuint index, GLshort x, GLshort y);
typedef void (APIENTRY *PFNGLVERTEXATTRIB2SVNVPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB3DNVPROC)(GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (APIENTRY *PFNGLVERTEXATTRIB3DVNVPROC)(GLuint index, const GLdouble *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB3FNVPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY *PFNGLVERTEXATTRIB3FVNVPROC)(GLuint index, const GLfloat *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB3SNVPROC)(GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (APIENTRY *PFNGLVERTEXATTRIB3SVNVPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB4DNVPROC)(GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY *PFNGLVERTEXATTRIB4DVNVPROC)(GLuint index, const GLdouble *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB4FNVPROC)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY *PFNGLVERTEXATTRIB4FVNVPROC)(GLuint index, const GLfloat *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB4SNVPROC)(GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (APIENTRY *PFNGLVERTEXATTRIB4SVNVPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIB4UBVNVPROC)(GLuint index, const GLubyte *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS1DVNVPROC)(GLuint index, GLsizei n, const GLdouble *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS1FVNVPROC)(GLuint index, GLsizei n, const GLfloat *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS1SVNVPROC)(GLuint index, GLsizei n, const GLshort *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS2DVNVPROC)(GLuint index, GLsizei n, const GLdouble *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS2FVNVPROC)(GLuint index, GLsizei n, const GLfloat *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS2SVNVPROC)(GLuint index, GLsizei n, const GLshort *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS3DVNVPROC)(GLuint index, GLsizei n, const GLdouble *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS3FVNVPROC)(GLuint index, GLsizei n, const GLfloat *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS3SVNVPROC)(GLuint index, GLsizei n, const GLshort *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS4DVNVPROC)(GLuint index, GLsizei n, const GLdouble *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS4FVNVPROC)(GLuint index, GLsizei n, const GLfloat *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS4SVNVPROC)(GLuint index, GLsizei n, const GLshort *v);
typedef void (APIENTRY *PFNGLVERTEXATTRIBS4UBVNVPROC)(GLuint index, GLsizei n, const GLubyte *v);

/* GL_NV_vertex_array_range */
typedef void (APIENTRY *PFNGLFLUSHVERTEXARRAYRANGENVPROC)(void);
typedef void (APIENTRY *PFNGLVERTEXARRAYRANGENVPROC)(GLsizei length, const GLvoid *pointer);
typedef void * (APIENTRY *PFNWGLALLOCATEMEMORYNVPROC)(GLsizei size, GLfloat readFrequency, GLfloat writeFrequency, GLfloat priority);
typedef void (APIENTRY *PFNWGLFREEMEMORYNVPROC)(void *pointer);

/* GL_NV_fence */
typedef void (APIENTRY *PFNGLDELETEFENCESNVPROC)(GLsizei n, const GLuint *fences);
typedef void (APIENTRY *PFNGLGENFENCESNVPROC)(GLsizei n, GLuint *fences);
typedef GLboolean (APIENTRY *PFNGLISFENCENVPROC)(GLuint fence);
typedef GLboolean (APIENTRY *PFNGLTESTFENCENVPROC)(GLuint fence);
typedef void (APIENTRY *PFNGLGETFENCEIVNVPROC)(GLuint fence, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLFINISHFENCENVPROC)(GLuint fence);
typedef void (APIENTRY *PFNGLSETFENCENVPROC)(GLuint fence, GLenum condition);

/* Extension entry points (preview_window.cpp). */

/* GL_EXT_compiled_vertex_array */
extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;

/* GL_ARB_multitexture */
extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
extern PFNGLMULTITEXCOORD1DARBPROC glMultiTexCoord1dARB;
extern PFNGLMULTITEXCOORD1DVARBPROC glMultiTexCoord1dvARB;
extern PFNGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB;
extern PFNGLMULTITEXCOORD1FVARBPROC glMultiTexCoord1fvARB;
extern PFNGLMULTITEXCOORD1IARBPROC glMultiTexCoord1iARB;
extern PFNGLMULTITEXCOORD1IVARBPROC glMultiTexCoord1ivARB;
extern PFNGLMULTITEXCOORD1SARBPROC glMultiTexCoord1sARB;
extern PFNGLMULTITEXCOORD1SVARBPROC glMultiTexCoord1svARB;
extern PFNGLMULTITEXCOORD2DARBPROC glMultiTexCoord2dARB;
extern PFNGLMULTITEXCOORD2DVARBPROC glMultiTexCoord2dvARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;
extern PFNGLMULTITEXCOORD2IARBPROC glMultiTexCoord2iARB;
extern PFNGLMULTITEXCOORD2IVARBPROC glMultiTexCoord2ivARB;
extern PFNGLMULTITEXCOORD2SARBPROC glMultiTexCoord2sARB;
extern PFNGLMULTITEXCOORD2SVARBPROC glMultiTexCoord2svARB;
extern PFNGLMULTITEXCOORD3DARBPROC glMultiTexCoord3dARB;
extern PFNGLMULTITEXCOORD3DVARBPROC glMultiTexCoord3dvARB;
extern PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB;
extern PFNGLMULTITEXCOORD3FVARBPROC glMultiTexCoord3fvARB;
extern PFNGLMULTITEXCOORD3IARBPROC glMultiTexCoord3iARB;
extern PFNGLMULTITEXCOORD3IVARBPROC glMultiTexCoord3ivARB;
extern PFNGLMULTITEXCOORD3SARBPROC glMultiTexCoord3sARB;
extern PFNGLMULTITEXCOORD3SVARBPROC glMultiTexCoord3svARB;
extern PFNGLMULTITEXCOORD4DARBPROC glMultiTexCoord4dARB;
extern PFNGLMULTITEXCOORD4DVARBPROC glMultiTexCoord4dvARB;
extern PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB;
extern PFNGLMULTITEXCOORD4FVARBPROC glMultiTexCoord4fvARB;
extern PFNGLMULTITEXCOORD4IARBPROC glMultiTexCoord4iARB;
extern PFNGLMULTITEXCOORD4IVARBPROC glMultiTexCoord4ivARB;
extern PFNGLMULTITEXCOORD4SARBPROC glMultiTexCoord4sARB;
extern PFNGLMULTITEXCOORD4SVARBPROC glMultiTexCoord4svARB;

/* GL_EXT_texture3D */
extern PFNGLTEXIMAGE3DEXTPROC glTexImage3DEXT;
extern PFNGLTEXSUBIMAGE3DEXTPROC glTexSubImage3DEXT;

/* GL_NV_vertex_program */
extern PFNGLAREPROGRAMSRESIDENTNVPROC glAreProgramsResidentNV;
extern PFNGLBINDPROGRAMNVPROC glBindProgramNV;
extern PFNGLDELETEPROGRAMSNVPROC glDeleteProgramsNV;
extern PFNGLEXECUTEPROGRAMNVPROC glExecuteProgramNV;
extern PFNGLGENPROGRAMSNVPROC glGenProgramsNV;
extern PFNGLGETPROGRAMPARAMETERDVNVPROC glGetProgramParameterdvNV;
extern PFNGLGETPROGRAMPARAMETERFVNVPROC glGetProgramParameterfvNV;
extern PFNGLGETPROGRAMIVNVPROC glGetProgramivNV;
extern PFNGLGETPROGRAMSTRINGNVPROC glGetProgramStringNV;
extern PFNGLGETTRACKMATRIXIVNVPROC glGetTrackMatrixivNV;
extern PFNGLGETVERTEXATTRIBDVNVPROC glGetVertexAttribdvNV;
extern PFNGLGETVERTEXATTRIBFVNVPROC glGetVertexAttribfvNV;
extern PFNGLGETVERTEXATTRIBIVNVPROC glGetVertexAttribivNV;
extern PFNGLGETVERTEXATTRIBPOINTERVNVPROC glGetVertexAttribPointervNV;
extern PFNGLISPROGRAMNVPROC glIsProgramNV;
extern PFNGLLOADPROGRAMNVPROC glLoadProgramNV;
extern PFNGLPROGRAMPARAMETER4DNVPROC glProgramParameter4dNV;
extern PFNGLPROGRAMPARAMETER4DVNVPROC glProgramParameter4dvNV;
extern PFNGLPROGRAMPARAMETER4FNVPROC glProgramParameter4fNV;
extern PFNGLPROGRAMPARAMETER4FVNVPROC glProgramParameter4fvNV;
extern PFNGLPROGRAMPARAMETERS4DVNVPROC glProgramParameters4dvNV;
extern PFNGLPROGRAMPARAMETERS4FVNVPROC glProgramParameters4fvNV;
extern PFNGLREQUESTRESIDENTPROGRAMSNVPROC glRequestResidentProgramsNV;
extern PFNGLTRACKMATRIXNVPROC glTrackMatrixNV;
extern PFNGLVERTEXATTRIBPOINTERNVPROC glVertexAttribPointerNV;
extern PFNGLVERTEXATTRIB1DNVPROC glVertexAttrib1dNV;
extern PFNGLVERTEXATTRIB1DVNVPROC glVertexAttrib1dvNV;
extern PFNGLVERTEXATTRIB1FNVPROC glVertexAttrib1fNV;
extern PFNGLVERTEXATTRIB1FVNVPROC glVertexAttrib1fvNV;
extern PFNGLVERTEXATTRIB1SNVPROC glVertexAttrib1sNV;
extern PFNGLVERTEXATTRIB1SVNVPROC glVertexAttrib1svNV;
extern PFNGLVERTEXATTRIB2DNVPROC glVertexAttrib2dNV;
extern PFNGLVERTEXATTRIB2DVNVPROC glVertexAttrib2dvNV;
extern PFNGLVERTEXATTRIB2FNVPROC glVertexAttrib2fNV;
extern PFNGLVERTEXATTRIB2FVNVPROC glVertexAttrib2fvNV;
extern PFNGLVERTEXATTRIB2SNVPROC glVertexAttrib2sNV;
extern PFNGLVERTEXATTRIB2SVNVPROC glVertexAttrib2svNV;
extern PFNGLVERTEXATTRIB3DNVPROC glVertexAttrib3dNV;
extern PFNGLVERTEXATTRIB3DVNVPROC glVertexAttrib3dvNV;
extern PFNGLVERTEXATTRIB3FNVPROC glVertexAttrib3fNV;
extern PFNGLVERTEXATTRIB3FVNVPROC glVertexAttrib3fvNV;
extern PFNGLVERTEXATTRIB3SNVPROC glVertexAttrib3sNV;
extern PFNGLVERTEXATTRIB3SVNVPROC glVertexAttrib3svNV;
extern PFNGLVERTEXATTRIB4DNVPROC glVertexAttrib4dNV;
extern PFNGLVERTEXATTRIB4DVNVPROC glVertexAttrib4dvNV;
extern PFNGLVERTEXATTRIB4FNVPROC glVertexAttrib4fNV;
extern PFNGLVERTEXATTRIB4FVNVPROC glVertexAttrib4fvNV;
extern PFNGLVERTEXATTRIB4SNVPROC glVertexAttrib4sNV;
extern PFNGLVERTEXATTRIB4SVNVPROC glVertexAttrib4svNV;
extern PFNGLVERTEXATTRIB4UBVNVPROC glVertexAttrib4ubvNV;
extern PFNGLVERTEXATTRIBS1DVNVPROC glVertexAttribs1dvNV;
extern PFNGLVERTEXATTRIBS1FVNVPROC glVertexAttribs1fvNV;
extern PFNGLVERTEXATTRIBS1SVNVPROC glVertexAttribs1svNV;
extern PFNGLVERTEXATTRIBS2DVNVPROC glVertexAttribs2dvNV;
extern PFNGLVERTEXATTRIBS2FVNVPROC glVertexAttribs2fvNV;
extern PFNGLVERTEXATTRIBS2SVNVPROC glVertexAttribs2svNV;
extern PFNGLVERTEXATTRIBS3DVNVPROC glVertexAttribs3dvNV;
extern PFNGLVERTEXATTRIBS3FVNVPROC glVertexAttribs3fvNV;
extern PFNGLVERTEXATTRIBS3SVNVPROC glVertexAttribs3svNV;
extern PFNGLVERTEXATTRIBS4DVNVPROC glVertexAttribs4dvNV;
extern PFNGLVERTEXATTRIBS4FVNVPROC glVertexAttribs4fvNV;
extern PFNGLVERTEXATTRIBS4SVNVPROC glVertexAttribs4svNV;
extern PFNGLVERTEXATTRIBS4UBVNVPROC glVertexAttribs4ubvNV;

/* GL_NV_vertex_array_range */
extern PFNGLFLUSHVERTEXARRAYRANGENVPROC glFlushVertexArrayRangeNV;
extern PFNGLVERTEXARRAYRANGENVPROC glVertexArrayRangeNV;
extern PFNWGLALLOCATEMEMORYNVPROC wglAllocateMemoryNV;
extern PFNWGLFREEMEMORYNVPROC wglFreeMemoryNV;

/* GL_NV_fence */
extern PFNGLDELETEFENCESNVPROC glDeleteFencesNV;
extern PFNGLGENFENCESNVPROC glGenFencesNV;
extern PFNGLISFENCENVPROC glIsFenceNV;
extern PFNGLTESTFENCENVPROC glTestFenceNV;
extern PFNGLGETFENCEIVNVPROC glGetFenceivNV;
extern PFNGLFINISHFENCENVPROC glFinishFenceNV;
extern PFNGLSETFENCENVPROC glSetFenceNV;

#define FLARE_NUM_GL_PROCS 112

extern GLenum previewBlendSource, previewBlendDest, previewCullFace;
extern GLuint previewBoundTextures[4];
extern HDC previewDC;
extern HGLRC previewGLContext;
void GL_CullFace( GLenum face );

void Key_Init( HWND window );
void Key_CaptureMouse( HWND window );

#endif
