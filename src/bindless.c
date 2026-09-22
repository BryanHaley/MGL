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
 * bindless.c
 * MGL
 *
 * GL_ARB_bindless_texture. A handle names a slot in a table of Metal texture
 * IDs and a slot in a table of sampler IDs; the shader compiler turns every
 * use of a handle into a read of those tables (see GlslangToSpv.cpp), and the
 * renderer keeps the tables filled and the textures resident.
 */

#include <stdlib.h>
#include <string.h>

#include "glcorearb.h"
#include "glm_context.h"

#ifdef MGL_GL_CORE

extern GLMContext _ctx;
extern void mgl_lazy_init(void);

Texture *findTexture(GLMContext ctx, GLuint texture);
Sampler *findSampler(GLMContext ctx, GLuint sampler);

#define GET_CONTEXT()   (mgl_lazy_init(), _ctx)
#define HANDLE_TAG      0x4D470000u

static void bindlessError(GLMContext ctx, const char *func, GLenum error)
{
    ctx->error_func(ctx, func, error);
}

static MglHandle *findHandle(GLMContext ctx, GLuint64 value)
{
    MglBindless *b = &ctx->bindless;

    if ((GLuint)(value >> 32) >> 16 != HANDLE_TAG >> 16)
        return NULL;

    for (GLuint i = 0; i < b->count; i++)
        if (b->handles[i].value == value && b->handles[i].tex)
            return &b->handles[i];

    return NULL;
}

static MglHandle *newHandle(GLMContext ctx)
{
    MglBindless *b = &ctx->bindless;

    if (b->next_slot == 0)
        b->next_slot = 1;

    if (b->next_slot >= MGL_BINDLESS_TEXTURES)
        return NULL;

    if (b->count == b->cap)
    {
        GLuint cap = b->cap ? b->cap * 2 : 64;
        MglHandle *grown = realloc(b->handles, cap * sizeof(MglHandle));

        if (grown == NULL)
            return NULL;

        b->handles = grown;
        b->cap = cap;
    }

    MglHandle *h = &b->handles[b->count++];

    memset(h, 0, sizeof(*h));
    h->tex_slot = b->next_slot++;

    return h;
}

// a texture only has a handle once it has storage to point at
static bool hasStorage(Texture *tex)
{
    return tex && tex->width > 0;
}

static GLuint64 textureHandle(GLMContext ctx, const char *func, GLuint texture, GLuint sampler)
{
    Texture *tex = texture ? findTexture(ctx, texture) : NULL;
    Sampler *smp = NULL;

    if (tex == NULL)
    {
        bindlessError(ctx, func, GL_INVALID_VALUE);
        return 0;
    }

    if (sampler)
    {
        smp = findSampler(ctx, sampler);

        if (smp == NULL)
        {
            bindlessError(ctx, func, GL_INVALID_VALUE);
            return 0;
        }
    }

    if (!hasStorage(tex))
    {
        bindlessError(ctx, func, GL_INVALID_OPERATION);
        return 0;
    }

    // the same texture and sampler always give back the same handle
    MglBindless *b = &ctx->bindless;

    for (GLuint i = 0; i < b->count; i++)
        if (!b->handles[i].image && b->handles[i].tex == tex && b->handles[i].sampler_name == sampler)
            return b->handles[i].value;

    MglHandle *h = newHandle(ctx);

    if (h == NULL)
    {
        bindlessError(ctx, func, GL_OUT_OF_MEMORY);
        return 0;
    }

    h->tex = tex;
    h->sampler_name = sampler;
    h->smp_slot = ctx->mtl_funcs.mtlBindlessSampler(ctx, smp ? &smp->params : &tex->params, tex->target);
    h->value = ((GLuint64)(HANDLE_TAG | h->smp_slot) << 32) | h->tex_slot;

    return h->value;
}

GLuint64 glGetTextureHandleARB(GLuint texture)
{
    GLMContext ctx = GET_CONTEXT();

    return textureHandle(ctx, __FUNCTION__, texture, 0);
}

GLuint64 glGetTextureSamplerHandleARB(GLuint texture, GLuint sampler)
{
    GLMContext ctx = GET_CONTEXT();

    if (sampler == 0)
    {
        bindlessError(ctx, __FUNCTION__, GL_INVALID_VALUE);
        return 0;
    }

    return textureHandle(ctx, __FUNCTION__, texture, sampler);
}

GLuint64 glGetImageHandleARB(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format)
{
    GLMContext ctx = GET_CONTEXT();
    Texture *tex = texture ? findTexture(ctx, texture) : NULL;

    if (tex == NULL || level < 0 || layer < 0)
    {
        bindlessError(ctx, __FUNCTION__, GL_INVALID_VALUE);
        return 0;
    }

    if (!hasStorage(tex))
    {
        bindlessError(ctx, __FUNCTION__, GL_INVALID_OPERATION);
        return 0;
    }

    MglBindless *b = &ctx->bindless;

    for (GLuint i = 0; i < b->count; i++)
    {
        MglHandle *h = &b->handles[i];

        if (h->image && h->tex == tex && h->level == level && h->layered == layered &&
            h->layer == layer && h->format == format)
            return h->value;
    }

    MglHandle *h = newHandle(ctx);

    if (h == NULL)
    {
        bindlessError(ctx, __FUNCTION__, GL_OUT_OF_MEMORY);
        return 0;
    }

    h->tex = tex;
    h->image = GL_TRUE;
    h->level = level;
    h->layered = layered;
    h->layer = layer;
    h->format = format;
    h->value = ((GLuint64)HANDLE_TAG << 32) | h->tex_slot;

    return h->value;
}

static void makeResident(GLMContext ctx, const char *func, GLuint64 handle, bool image, bool resident, GLenum access)
{
    MglHandle *h = findHandle(ctx, handle);

    if (h == NULL || h->image != image || h->resident == resident)
    {
        bindlessError(ctx, func, GL_INVALID_OPERATION);
        return;
    }

    h->resident = resident;
    h->access = access;
    ctx->bindless.serial++;
}

void glMakeTextureHandleResidentARB(GLuint64 handle)
{
    GLMContext ctx = GET_CONTEXT();

    makeResident(ctx, __FUNCTION__, handle, false, true, GL_READ_ONLY);
}

void glMakeTextureHandleNonResidentARB(GLuint64 handle)
{
    GLMContext ctx = GET_CONTEXT();

    makeResident(ctx, __FUNCTION__, handle, false, false, 0);
}

void glMakeImageHandleResidentARB(GLuint64 handle, GLenum access)
{
    GLMContext ctx = GET_CONTEXT();

    if (access != GL_READ_ONLY && access != GL_WRITE_ONLY && access != GL_READ_WRITE)
    {
        bindlessError(ctx, __FUNCTION__, GL_INVALID_ENUM);
        return;
    }

    makeResident(ctx, __FUNCTION__, handle, true, true, access);
}

void glMakeImageHandleNonResidentARB(GLuint64 handle)
{
    GLMContext ctx = GET_CONTEXT();

    makeResident(ctx, __FUNCTION__, handle, true, false, 0);
}

static GLboolean isResident(GLMContext ctx, const char *func, GLuint64 handle, bool image)
{
    MglHandle *h = findHandle(ctx, handle);

    if (h == NULL || h->image != image)
    {
        bindlessError(ctx, func, GL_INVALID_OPERATION);
        return GL_FALSE;
    }

    return h->resident;
}

GLboolean glIsTextureHandleResidentARB(GLuint64 handle)
{
    GLMContext ctx = GET_CONTEXT();

    return isResident(ctx, __FUNCTION__, handle, false);
}

GLboolean glIsImageHandleResidentARB(GLuint64 handle)
{
    GLMContext ctx = GET_CONTEXT();

    return isResident(ctx, __FUNCTION__, handle, true);
}

// A sampler or image uniform declared for handles is a uvec2 underneath, so a
// handle is written the way two unsigned ints are.
void glUniformHandleui64ARB(GLint location, GLuint64 value)
{
    glUniform2ui(location, (GLuint)value, (GLuint)(value >> 32));
}

void glUniformHandleui64vARB(GLint location, GLsizei count, const GLuint64 *value)
{
    glUniform2uiv(location, count, (const GLuint *)value);
}

void glProgramUniformHandleui64ARB(GLuint program, GLint location, GLuint64 value)
{
    glProgramUniform2ui(program, location, (GLuint)value, (GLuint)(value >> 32));
}

void glProgramUniformHandleui64vARB(GLuint program, GLint location, GLsizei count, const GLuint64 *values)
{
    glProgramUniform2uiv(program, location, count, (const GLuint *)values);
}

void glVertexAttribL1ui64ARB(GLuint index, GLuint64EXT x)
{
    glVertexAttribI2ui(index, (GLuint)x, (GLuint)(x >> 32));
}

void glVertexAttribL1ui64vARB(GLuint index, const GLuint64EXT *v)
{
    glVertexAttribI2uiv(index, (const GLuint *)v);
}

void glGetVertexAttribLui64vARB(GLuint index, GLenum pname, GLuint64EXT *params)
{
    GLuint parts[4] = { 0 };

    glGetVertexAttribIuiv(index, pname, parts);

    if (params)
        *params = pname == GL_CURRENT_VERTEX_ATTRIB ? ((GLuint64EXT)parts[1] << 32) | parts[0] : parts[0];
}

#endif

// A deleted texture takes its handles with it
void mglBindlessForgetTexture(GLMContext ctx, Texture *tex)
{
    MglBindless *b = &ctx->bindless;

    for (GLuint i = 0; i < b->count; i++)
    {
        MglHandle *h = &b->handles[i];

        if (h->tex != tex)
            continue;

        ctx->mtl_funcs.mtlBindlessRelease(ctx, h);
        h->tex = NULL;
        h->resident = GL_FALSE;
        b->serial++;
    }
}
