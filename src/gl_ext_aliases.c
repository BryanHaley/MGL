/*
 * Copyright (C) The MooGL Project
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
 * gl_ext_aliases.c
 * MGL
 *
 * The ARB/EXT/KHR-suffixed names of entry points MGL already has under their
 * core names. An extension MGL lists has to answer to its own names too:
 * a loader that asks a library for glDrawArraysInstancedARB and does not
 * find it may go on to the system OpenGL and get one that draws nothing here.
 */

#ifdef MGL_GL_CORE

#include "glcorearb.h"

void glDrawArraysInstancedARB(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
    glDrawArraysInstanced(mode, first, count, instancecount);
}

void glDrawElementsInstancedARB(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)
{
    glDrawElementsInstanced(mode, count, type, indices, instancecount);
}

void glMultiDrawArraysIndirectCountARB(GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    glMultiDrawArraysIndirectCount(mode, indirect, drawcount, maxdrawcount, stride);
}

void glMultiDrawElementsIndirectCountARB(GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    glMultiDrawElementsIndirectCount(mode, type, indirect, drawcount, maxdrawcount, stride);
}

void glSpecializeShaderARB(GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue)
{
    glSpecializeShader(shader, pEntryPoint, numSpecializationConstants, pConstantIndex, pConstantValue);
}

void glVertexAttribDivisorARB(GLuint index, GLuint divisor)
{
    glVertexAttribDivisor(index, divisor);
}

#endif
