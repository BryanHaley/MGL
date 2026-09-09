/*
 * Copyright (C) Michael Larson on 1/6/2022
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * non_core_unimplemented.c
 * MGL
 *
 */

#include "mgl.h"

void mglArrayElement(GLMContext ctx, GLint i)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglBegin(GLMContext ctx, GLenum mode)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNewList(GLMContext ctx, GLuint list, GLenum mode)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEndList(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglCallList(GLMContext ctx, GLuint list)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglCallLists(GLMContext ctx, GLsizei n, GLenum type, const void *lists)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglDeleteLists(GLMContext ctx, GLuint list, GLsizei range)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

GLuint mglGenLists(GLMContext ctx, GLsizei range)
{
    // not in the core profile
    ERROR_RETURN_VALUE(GL_INVALID_OPERATION, 0);
}

void mglListBase(GLMContext ctx, GLuint base)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglBitmap(GLMContext ctx, GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3b(GLMContext ctx, GLbyte red, GLbyte green, GLbyte blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3bv(GLMContext ctx, const GLbyte *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3d(GLMContext ctx, GLdouble red, GLdouble green, GLdouble blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3f(GLMContext ctx, GLfloat red, GLfloat green, GLfloat blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3i(GLMContext ctx, GLint red, GLint green, GLint blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3s(GLMContext ctx, GLshort red, GLshort green, GLshort blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3ub(GLMContext ctx, GLubyte red, GLubyte green, GLubyte blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3ubv(GLMContext ctx, const GLubyte *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3ui(GLMContext ctx, GLuint red, GLuint green, GLuint blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3uiv(GLMContext ctx, const GLuint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3us(GLMContext ctx, GLushort red, GLushort green, GLushort blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor3usv(GLMContext ctx, const GLushort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4b(GLMContext ctx, GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4bv(GLMContext ctx, const GLbyte *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4d(GLMContext ctx, GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4f(GLMContext ctx, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4i(GLMContext ctx, GLint red, GLint green, GLint blue, GLint alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4s(GLMContext ctx, GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4ub(GLMContext ctx, GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4ubv(GLMContext ctx, const GLubyte *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4ui(GLMContext ctx, GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4uiv(GLMContext ctx, const GLuint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4us(GLMContext ctx, GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColor4usv(GLMContext ctx, const GLushort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEnd(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEdgeFlag(GLMContext ctx, GLboolean flag)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEdgeFlagv(GLMContext ctx, const GLboolean *flag)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexd(GLMContext ctx, GLdouble c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexdv(GLMContext ctx, const GLdouble *c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexf(GLMContext ctx, GLfloat c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexfv(GLMContext ctx, const GLfloat *c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexi(GLMContext ctx, GLint c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexiv(GLMContext ctx, const GLint *c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexs(GLMContext ctx, GLshort c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexsv(GLMContext ctx, const GLshort *c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3b(GLMContext ctx, GLbyte nx, GLbyte ny, GLbyte nz)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3bv(GLMContext ctx, const GLbyte *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3d(GLMContext ctx, GLdouble nx, GLdouble ny, GLdouble nz)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3f(GLMContext ctx, GLfloat nx, GLfloat ny, GLfloat nz)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3i(GLMContext ctx, GLint nx, GLint ny, GLint nz)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3s(GLMContext ctx, GLshort nx, GLshort ny, GLshort nz)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormal3sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos2d(GLMContext ctx, GLdouble x, GLdouble y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos2dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos2f(GLMContext ctx, GLfloat x, GLfloat y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos2fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos2i(GLMContext ctx, GLint x, GLint y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos2iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos2s(GLMContext ctx, GLshort x, GLshort y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos2sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos3d(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos3dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos3f(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos3fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos3i(GLMContext ctx, GLint x, GLint y, GLint z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos3iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos3s(GLMContext ctx, GLshort x, GLshort y, GLshort z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos3sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos4d(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos4dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos4f(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos4fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos4i(GLMContext ctx, GLint x, GLint y, GLint z, GLint w)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos4iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos4s(GLMContext ctx, GLshort x, GLshort y, GLshort z, GLshort w)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRasterPos4sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRectd(GLMContext ctx, GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRectdv(GLMContext ctx, const GLdouble *v1, const GLdouble *v2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRectf(GLMContext ctx, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRectfv(GLMContext ctx, const GLfloat *v1, const GLfloat *v2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRecti(GLMContext ctx, GLint x1, GLint y1, GLint x2, GLint y2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRectiv(GLMContext ctx, const GLint *v1, const GLint *v2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRects(GLMContext ctx, GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRectsv(GLMContext ctx, const GLshort *v1, const GLshort *v2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord1d(GLMContext ctx, GLdouble s)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord1dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord1f(GLMContext ctx, GLfloat s)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord1fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord1i(GLMContext ctx, GLint s)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord1iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord1s(GLMContext ctx, GLshort s)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord1sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord2d(GLMContext ctx, GLdouble s, GLdouble t)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord2dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord2f(GLMContext ctx, GLfloat s, GLfloat t)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord2fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord2i(GLMContext ctx, GLint s, GLint t)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord2iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord2s(GLMContext ctx, GLshort s, GLshort t)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord2sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord3d(GLMContext ctx, GLdouble s, GLdouble t, GLdouble r)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord3dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord3f(GLMContext ctx, GLfloat s, GLfloat t, GLfloat r)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord3fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord3i(GLMContext ctx, GLint s, GLint t, GLint r)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord3iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord3s(GLMContext ctx, GLshort s, GLshort t, GLshort r)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord3sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord4d(GLMContext ctx, GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord4dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord4f(GLMContext ctx, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord4fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord4i(GLMContext ctx, GLint s, GLint t, GLint r, GLint q)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord4iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord4s(GLMContext ctx, GLshort s, GLshort t, GLshort r, GLshort q)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoord4sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex2d(GLMContext ctx, GLdouble x, GLdouble y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex2dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex2f(GLMContext ctx, GLfloat x, GLfloat y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex2fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex2i(GLMContext ctx, GLint x, GLint y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex2iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex2s(GLMContext ctx, GLshort x, GLshort y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex2sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex3d(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex3dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex3f(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex3fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex3i(GLMContext ctx, GLint x, GLint y, GLint z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex3iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex3s(GLMContext ctx, GLshort x, GLshort y, GLshort z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex3sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex4d(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex4dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex4f(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex4fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex4i(GLMContext ctx, GLint x, GLint y, GLint z, GLint w)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex4iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex4s(GLMContext ctx, GLshort x, GLshort y, GLshort z, GLshort w)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertex4sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglClipPlane(GLMContext ctx, GLenum plane, const GLdouble *equation)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColorMaterial(GLMContext ctx, GLenum face, GLenum mode)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogf(GLMContext ctx, GLenum pname, GLfloat param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogfv(GLMContext ctx, GLenum pname, const GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogi(GLMContext ctx, GLenum pname, GLint param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogiv(GLMContext ctx, GLenum pname, const GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLightf(GLMContext ctx, GLenum light, GLenum pname, GLfloat param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLightfv(GLMContext ctx, GLenum light, GLenum pname, const GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLighti(GLMContext ctx, GLenum light, GLenum pname, GLint param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLightiv(GLMContext ctx, GLenum light, GLenum pname, const GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLightModelf(GLMContext ctx, GLenum pname, GLfloat param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLightModelfv(GLMContext ctx, GLenum pname, const GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLightModeli(GLMContext ctx, GLenum pname, GLint param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLightModeliv(GLMContext ctx, GLenum pname, const GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLineStipple(GLMContext ctx, GLint factor, GLushort pattern)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMaterialf(GLMContext ctx, GLenum face, GLenum pname, GLfloat param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMaterialfv(GLMContext ctx, GLenum face, GLenum pname, const GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMateriali(GLMContext ctx, GLenum face, GLenum pname, GLint param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMaterialiv(GLMContext ctx, GLenum face, GLenum pname, const GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPolygonStipple(GLMContext ctx, const GLubyte *mask)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglShadeModel(GLMContext ctx, GLenum mode)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexEnvf(GLMContext ctx, GLenum target, GLenum pname, GLfloat param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexEnvfv(GLMContext ctx, GLenum target, GLenum pname, const GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexEnvi(GLMContext ctx, GLenum target, GLenum pname, GLint param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexEnviv(GLMContext ctx, GLenum target, GLenum pname, const GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexGend(GLMContext ctx, GLenum coord, GLenum pname, GLdouble param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexGendv(GLMContext ctx, GLenum coord, GLenum pname, const GLdouble *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexGenf(GLMContext ctx, GLenum coord, GLenum pname, GLfloat param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexGenfv(GLMContext ctx, GLenum coord, GLenum pname, const GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexGeni(GLMContext ctx, GLenum coord, GLenum pname, GLint param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexGeniv(GLMContext ctx, GLenum coord, GLenum pname, const GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFeedbackBuffer(GLMContext ctx, GLsizei size, GLenum type, GLfloat *buffer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSelectBuffer(GLMContext ctx, GLsizei size, GLuint *buffer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

GLint  mglRenderMode(GLMContext ctx, GLenum mode)
{
    // not in the core profile
    ERROR_RETURN_VALUE(GL_INVALID_OPERATION, 0);
}

void mglPixelMapfv(GLMContext ctx, GLenum map, GLsizei mapsize, const GLfloat *values)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPixelMapuiv(GLMContext ctx, GLenum map, GLsizei mapsize, const GLuint *values)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPixelMapusv(GLMContext ctx, GLenum map, GLsizei mapsize, const GLushort *values)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglCopyPixels(GLMContext ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglDrawPixels(GLMContext ctx, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetClipPlane(GLMContext ctx, GLenum plane, GLdouble *equation)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetLightfv(GLMContext ctx, GLenum light, GLenum pname, GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetLightiv(GLMContext ctx, GLenum light, GLenum pname, GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetMapdv(GLMContext ctx, GLenum target, GLenum query, GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetMapfv(GLMContext ctx, GLenum target, GLenum query, GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetMapiv(GLMContext ctx, GLenum target, GLenum query, GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetMaterialfv(GLMContext ctx, GLenum face, GLenum pname, GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetMaterialiv(GLMContext ctx, GLenum face, GLenum pname, GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetPixelMapfv(GLMContext ctx, GLenum map, GLfloat *values)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetPixelMapuiv(GLMContext ctx, GLenum map, GLuint *values)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetPixelMapusv(GLMContext ctx, GLenum map, GLushort *values)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetPolygonStipple(GLMContext ctx, GLubyte *mask)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetTexEnvfv(GLMContext ctx, GLenum target, GLenum pname, GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetTexEnviv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetTexGendv(GLMContext ctx, GLenum coord, GLenum pname, GLdouble *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetTexGenfv(GLMContext ctx, GLenum coord, GLenum pname, GLfloat *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetTexGeniv(GLMContext ctx, GLenum coord, GLenum pname, GLint *params)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

GLboolean mglIsList(GLMContext ctx, GLuint list)
{
    // not in the core profile
    ERROR_RETURN_VALUE(GL_INVALID_OPERATION, GL_FALSE);
}

void mglFrustum(GLMContext ctx, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLoadIdentity(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLoadMatrixf(GLMContext ctx, const GLfloat *m)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLoadMatrixd(GLMContext ctx, const GLdouble *m)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMatrixMode(GLMContext ctx, GLenum mode)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultMatrixf(GLMContext ctx, const GLfloat *m)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultMatrixd(GLMContext ctx, const GLdouble *m)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglOrtho(GLMContext ctx, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPopMatrix(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPushMatrix(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRotated(GLMContext ctx, GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglRotatef(GLMContext ctx, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglScaled(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglScalef(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTranslated(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTranslatef(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}


void mglInitNames(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLoadName(GLMContext ctx, GLuint name)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPassThrough(GLMContext ctx, GLfloat token)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPopName(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPushName(GLMContext ctx, GLuint name)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglClearAccum(GLMContext ctx, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglClearIndex(GLMContext ctx, GLfloat c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexMask(GLMContext ctx, GLuint mask)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglAccum(GLMContext ctx, GLenum op, GLfloat value)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPopAttrib(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPushAttrib(GLMContext ctx, GLbitfield mask)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMap1d(GLMContext ctx, GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMap1f(GLMContext ctx, GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMap2d(GLMContext ctx, GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMap2f(GLMContext ctx, GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMapGrid1d(GLMContext ctx, GLint un, GLdouble u1, GLdouble u2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMapGrid1f(GLMContext ctx, GLint un, GLfloat u1, GLfloat u2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMapGrid2d(GLMContext ctx, GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMapGrid2f(GLMContext ctx, GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalCoord1d(GLMContext ctx, GLdouble u)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalCoord1dv(GLMContext ctx, const GLdouble *u)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalCoord1f(GLMContext ctx, GLfloat u)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalCoord1fv(GLMContext ctx, const GLfloat *u)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalCoord2d(GLMContext ctx, GLdouble u, GLdouble v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalCoord2dv(GLMContext ctx, const GLdouble *u)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalCoord2f(GLMContext ctx, GLfloat u, GLfloat v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalCoord2fv(GLMContext ctx, const GLfloat *u)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalMesh1(GLMContext ctx, GLenum mode, GLint i1, GLint i2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalPoint1(GLMContext ctx, GLint i)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalMesh2(GLMContext ctx, GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEvalPoint2(GLMContext ctx, GLint i, GLint j)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglAlphaFunc(GLMContext ctx, GLenum func, GLfloat ref)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPixelZoom(GLMContext ctx, GLfloat xfactor, GLfloat yfactor)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPixelTransferf(GLMContext ctx, GLenum pname, GLfloat param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPixelTransferi(GLMContext ctx, GLenum pname, GLint param)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetnPolygonStipple(GLMContext ctx, GLsizei bufSize, GLubyte *pattern)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetnColorTable(GLMContext ctx, GLenum target, GLenum format, GLenum type, GLsizei bufSize, void *table)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetnConvolutionFilter(GLMContext ctx, GLenum target, GLenum format, GLenum type, GLsizei bufSize, void *image)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetnSeparableFilter(GLMContext ctx, GLenum target, GLenum format, GLenum type, GLsizei rowBufSize, void *row, GLsizei columnBufSize, void *column, void *span)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetnHistogram(GLMContext ctx, GLenum target, GLboolean reset, GLenum format, GLenum type, GLsizei bufSize, void *values)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglGetnMinmax(GLMContext ctx, GLenum target, GLboolean reset, GLenum format, GLenum type, GLsizei bufSize, void *values)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglColorPointer(GLMContext ctx, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglDisableClientState(GLMContext ctx, GLenum array)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEdgeFlagPointer(GLMContext ctx, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglEnableClientState(GLMContext ctx, GLenum array)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexPointer(GLMContext ctx, GLenum type, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglInterleavedArrays(GLMContext ctx, GLenum format, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglNormalPointer(GLMContext ctx, GLenum type, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordPointer(GLMContext ctx, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglVertexPointer(GLMContext ctx, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogCoordf(GLMContext ctx, GLfloat coord)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogCoordfv(GLMContext ctx, const GLfloat *coord)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogCoordd(GLMContext ctx, GLdouble coord)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogCoorddv(GLMContext ctx, const GLdouble *coord)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglFogCoordPointer(GLMContext ctx, GLenum type, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3b(GLMContext ctx, GLbyte red, GLbyte green, GLbyte blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3bv(GLMContext ctx, const GLbyte *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3d(GLMContext ctx, GLdouble red, GLdouble green, GLdouble blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3f(GLMContext ctx, GLfloat red, GLfloat green, GLfloat blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3i(GLMContext ctx, GLint red, GLint green, GLint blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3s(GLMContext ctx, GLshort red, GLshort green, GLshort blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3ub(GLMContext ctx, GLubyte red, GLubyte green, GLubyte blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3ubv(GLMContext ctx, const GLubyte *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3ui(GLMContext ctx, GLuint red, GLuint green, GLuint blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3uiv(GLMContext ctx, const GLuint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3us(GLMContext ctx, GLushort red, GLushort green, GLushort blue)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColor3usv(GLMContext ctx, const GLushort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglSecondaryColorPointer(GLMContext ctx, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos2d(GLMContext ctx, GLdouble x, GLdouble y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos2dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos2f(GLMContext ctx, GLfloat x, GLfloat y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos2fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos2i(GLMContext ctx, GLint x, GLint y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos2iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos2s(GLMContext ctx, GLshort x, GLshort y)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos2sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos3d(GLMContext ctx, GLdouble x, GLdouble y, GLdouble z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos3dv(GLMContext ctx, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos3f(GLMContext ctx, GLfloat x, GLfloat y, GLfloat z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos3fv(GLMContext ctx, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos3i(GLMContext ctx, GLint x, GLint y, GLint z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos3iv(GLMContext ctx, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos3s(GLMContext ctx, GLshort x, GLshort y, GLshort z)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglWindowPos3sv(GLMContext ctx, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordP1ui(GLMContext ctx, GLenum type, GLuint coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordP1uiv(GLMContext ctx, GLenum type, const GLuint *coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordP2ui(GLMContext ctx, GLenum type, GLuint coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordP2uiv(GLMContext ctx, GLenum type, const GLuint *coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordP3ui(GLMContext ctx, GLenum type, GLuint coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordP3uiv(GLMContext ctx, GLenum type, const GLuint *coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordP4ui(GLMContext ctx, GLenum type, GLuint coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglTexCoordP4uiv(GLMContext ctx, GLenum type, const GLuint *coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoordP1ui(GLMContext ctx, GLenum texture, GLenum type, GLuint coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoordP1uiv(GLMContext ctx, GLenum texture, GLenum type, const GLuint *coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoordP2ui(GLMContext ctx, GLenum texture, GLenum type, GLuint coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoordP2uiv(GLMContext ctx, GLenum texture, GLenum type, const GLuint *coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoordP3ui(GLMContext ctx, GLenum texture, GLenum type, GLuint coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoordP3uiv(GLMContext ctx, GLenum texture, GLenum type, const GLuint *coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoordP4ui(GLMContext ctx, GLenum texture, GLenum type, GLuint coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoordP4uiv(GLMContext ctx, GLenum texture, GLenum type, const GLuint *coords)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord1d(GLMContext ctx, GLenum target, GLdouble s)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord1dv(GLMContext ctx, GLenum target, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord1f(GLMContext ctx, GLenum target, GLfloat s)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord1fv(GLMContext ctx, GLenum target, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord1i(GLMContext ctx, GLenum target, GLint s)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord1iv(GLMContext ctx, GLenum target, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord1s(GLMContext ctx, GLenum target, GLshort s)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord1sv(GLMContext ctx, GLenum target, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord2d(GLMContext ctx, GLenum target, GLdouble s, GLdouble t)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord2dv(GLMContext ctx, GLenum target, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord2f(GLMContext ctx, GLenum target, GLfloat s, GLfloat t)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord2fv(GLMContext ctx, GLenum target, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord2i(GLMContext ctx, GLenum target, GLint s, GLint t)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord2iv(GLMContext ctx, GLenum target, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord2s(GLMContext ctx, GLenum target, GLshort s, GLshort t)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord2sv(GLMContext ctx, GLenum target, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord3d(GLMContext ctx, GLenum target, GLdouble s, GLdouble t, GLdouble r)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord3dv(GLMContext ctx, GLenum target, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord3f(GLMContext ctx, GLenum target, GLfloat s, GLfloat t, GLfloat r)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord3fv(GLMContext ctx, GLenum target, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord3i(GLMContext ctx, GLenum target, GLint s, GLint t, GLint r)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord3iv(GLMContext ctx, GLenum target, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord3s(GLMContext ctx, GLenum target, GLshort s, GLshort t, GLshort r)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord3sv(GLMContext ctx, GLenum target, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord4d(GLMContext ctx, GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord4dv(GLMContext ctx, GLenum target, const GLdouble *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord4f(GLMContext ctx, GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord4fv(GLMContext ctx, GLenum target, const GLfloat *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord4i(GLMContext ctx, GLenum target, GLint s, GLint t, GLint r, GLint q)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord4iv(GLMContext ctx, GLenum target, const GLint *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord4s(GLMContext ctx, GLenum target, GLshort s, GLshort t, GLshort r, GLshort q)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiTexCoord4sv(GLMContext ctx, GLenum target, const GLshort *v)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLoadTransposeMatrixf(GLMContext ctx, const GLfloat *m)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglLoadTransposeMatrixd(GLMContext ctx, const GLdouble *m)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultTransposeMatrixf(GLMContext ctx, const GLfloat *m)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultTransposeMatrixd(GLMContext ctx, const GLdouble *m)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexub(GLMContext ctx, GLubyte c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglIndexubv(GLMContext ctx, const GLubyte *c)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPopClientAttrib(GLMContext ctx)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglPushClientAttrib(GLMContext ctx, GLbitfield mask)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}

GLboolean mglAreTexturesResident(GLMContext ctx, GLsizei n, const GLuint *textures, GLboolean *residences)
{
    // not in the core profile
    ERROR_RETURN_VALUE(GL_INVALID_OPERATION, GL_FALSE);
}

void mglPrioritizeTextures(GLMContext ctx, GLsizei n, const GLuint *textures, const GLfloat *priorities)
{
    // not in the core profile
    ERROR_RETURN(GL_INVALID_OPERATION);
}


