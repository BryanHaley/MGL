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
 * texture_buffer.c
 * MGL
 *
 * Buffer textures (GL_TEXTURE_BUFFER). A buffer texture is a 1D texture whose
 * storage is a buffer object, reinterpreted through a sized internal format,
 * read in a shader with texelFetch on a samplerBuffer.
 */

#include "glm_context.h"
#include "mgl_log.h"

/* functions from buffers.c and textures.c that we call directly */
extern Buffer *findBuffer(GLMContext ctx, GLuint buffer);
extern Texture *findTexture(GLMContext ctx, GLuint texture);
extern Texture *currentTexture(GLMContext ctx, GLuint index);
extern Texture *getTex(GLMContext ctx, GLuint texture, GLenum target);

/* ---- valid sized internal formats for glTexBuffer / glTexBufferRange ----
   Per OpenGL 4.6 Core spec section 8.17 (Buffer Textures), table 8.16.
   Compressed, depth and stencil formats are not accepted.               */
static const GLenum kValidTexBufferFormats[] = {
    GL_R8, GL_R8_SNORM, GL_R16, GL_R16_SNORM, GL_R16F, GL_R32F,
    GL_R8I, GL_R16I, GL_R32I, GL_R8UI, GL_R16UI, GL_R32UI,

    GL_RG8, GL_RG8_SNORM, GL_RG16, GL_RG16_SNORM, GL_RG16F, GL_RG32F,
    GL_RG8I, GL_RG16I, GL_RG32I, GL_RG8UI, GL_RG16UI, GL_RG32UI,

    GL_RGB8, GL_RGB8_SNORM, GL_RGB16, GL_RGB16_SNORM, GL_RGB16F, GL_RGB32F,
    GL_RGB8I, GL_RGB16I, GL_RGB32I, GL_RGB8UI, GL_RGB16UI, GL_RGB32UI,
    GL_SRGB8,

    GL_RGBA8, GL_RGBA8_SNORM, GL_RGBA16, GL_RGBA16_SNORM, GL_RGBA16F, GL_RGBA32F,
    GL_RGBA8I, GL_RGBA16I, GL_RGBA32I, GL_RGBA8UI, GL_RGBA16UI, GL_RGBA32UI,
    GL_SRGB8_ALPHA8,

    GL_RGB10_A2, GL_RGB10_A2UI,
    GL_R11F_G11F_B10F,
    GL_RGB9_E5,
    GL_RGB565,

    GL_R3_G3_B2, GL_RGB4, GL_RGB5, GL_RGB10, GL_RGB12,
    GL_RGBA2, GL_RGBA4, GL_RGB5_A1, GL_RGBA12,
};

static bool is_valid_texbuffer_format(GLenum internalformat)
{
    for (size_t i = 0; i < sizeof(kValidTexBufferFormats) / sizeof(kValidTexBufferFormats[0]); i++)
        if (kValidTexBufferFormats[i] == internalformat)
            return true;

    return false;
}

/* ---- per-texture buffer binding side table ----
   The Texture struct lives in glm_context.h which we cannot modify, so buffer
   texture state is kept here, keyed by the texture name.                   */

#define MAX_BUFFER_TEXTURE_BINDINGS 128

typedef struct BufferTextureBinding_t {
    GLuint      texture;        /* texture object name */
    GLuint      buffer;         /* buffer object name (0 = detached) */
    GLenum      internalformat;
    GLintptr    offset;         /* byte offset into the buffer */
    GLsizeiptr  size;           /* byte count; -1 means "whole buffer" */
    bool        in_use;
} BufferTextureBinding;

static BufferTextureBinding s_buf_tex_bindings[MAX_BUFFER_TEXTURE_BINDINGS];

static BufferTextureBinding *find_buf_tex_binding(GLuint texture)
{
    for (int i = 0; i < MAX_BUFFER_TEXTURE_BINDINGS; i++)
        if (s_buf_tex_bindings[i].in_use && s_buf_tex_bindings[i].texture == texture)
            return &s_buf_tex_bindings[i];

    return NULL;
}

static BufferTextureBinding *alloc_buf_tex_binding(GLuint texture)
{
    BufferTextureBinding *bt = find_buf_tex_binding(texture);
    if (bt)
        return bt;

    for (int i = 0; i < MAX_BUFFER_TEXTURE_BINDINGS; i++)
    {
        if (!s_buf_tex_bindings[i].in_use)
        {
            s_buf_tex_bindings[i].in_use = true;
            s_buf_tex_bindings[i].texture = texture;
            s_buf_tex_bindings[i].buffer = 0;
            s_buf_tex_bindings[i].internalformat = 0;
            s_buf_tex_bindings[i].offset = 0;
            s_buf_tex_bindings[i].size = -1;
            return &s_buf_tex_bindings[i];
        }
    }

    MGL_ERR("MGL: buffer texture binding table exhausted\n");
    return NULL;
}

/* ---- internal helper shared by all four entry points ---- */

static void tex_buffer_impl(GLMContext ctx, Texture *tex, GLenum internalformat,
                            GLuint buffer, GLintptr offset, GLsizeiptr size,
                            bool is_range)
{
    /* detach path: buffer 0 unbinds the storage */
    if (buffer == 0)
    {
        BufferTextureBinding *bt = find_buf_tex_binding(tex->name);
        if (bt)
        {
            bt->buffer = 0;
            bt->internalformat = 0;
            bt->offset = 0;
            bt->size = -1;
        }

        /* release any Metal buffer texture we created */
        if (tex->mtl_data)
        {
            ctx->mtl_funcs.mtlDeleteMTLObj(ctx, tex->mtl_data);
            tex->mtl_data = NULL;
        }

        tex->internalformat = 0;
        tex->width = 0;
        tex->height = 0;
        tex->depth = 0;

        return;
    }

    /* validate the internal format */
    if (!is_valid_texbuffer_format(internalformat))
    {
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    /* look up the buffer -- a name that is not a buffer object is an
       INVALID_OPERATION here, not an INVALID_VALUE (spec 8.9) */
    Buffer *buf = findBuffer(ctx, buffer);
    if (!buf)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    /* a buffer with no storage yet has nothing to texture from */
    if (buf->size == 0 || buf->data.buffer_data == 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    /* size defaults to the rest of the buffer beyond offset */
    if (size == 0 && is_range)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (!is_range || size < 0)
        size = buf->size - offset;

    /* validate the range */
    if (offset < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (offset + size > buf->size)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    /* glTexBufferRange offset alignment */
    if (is_range)
    {
        GLint alignment = STATE_VAR(texture_buffer_offset_alignment);
        if (alignment <= 0)
            alignment = 1;

        if (offset % alignment != 0)
        {
            ERROR_RETURN(GL_INVALID_VALUE);
        }
    }

    /* store the binding in the side table */
    BufferTextureBinding *bt = alloc_buf_tex_binding(tex->name);
    if (!bt)
    {
        ERROR_RETURN(GL_OUT_OF_MEMORY);
    }

    bt->buffer = buffer;
    bt->internalformat = internalformat;
    bt->offset = offset;
    bt->size = size;

    /* record the format and size on the texture object so queries
       (GL_TEXTURE_BUFFER_SIZE, GL_TEXTURE_INTERNAL_FORMAT etc.) work */
    tex->internalformat = internalformat;

    /* width holds the byte count; the renderer and query path derive the
       texel count from the format's element size */
    tex->width  = (GLuint)size;
    tex->height = 1;
    tex->depth  = 1;

    /* clamp to the implementation limit */
    GLint max_texels = STATE_VAR(max_texture_buffer_size);
    if (max_texels > 0 && (GLint)tex->width > max_texels)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    /* Release any previous Metal buffer texture. The renderer creates the
       actual MTLTexture lazily when the texture is bound for drawing, at
       which point the buffer's MTLBuffer is guaranteed to exist. */
    if (tex->mtl_data)
    {
        ctx->mtl_funcs.mtlDeleteMTLObj(ctx, tex->mtl_data);
        tex->mtl_data = NULL;
    }

    ctx->state.dirty_bits |= DIRTY_TEX;
}

// What the renderer needs to build the Metal texture: which buffer, and the
// byte range of it the texture sees. A size of -1 means the whole buffer.
bool mglBufferTextureSource(GLMContext ctx, const Texture *tex, Buffer **buf,
                            GLintptr *offset, GLsizeiptr *size)
{
    BufferTextureBinding *bt = find_buf_tex_binding(tex->name);

    if (bt == NULL || bt->buffer == 0)
        return false;

    *buf = findBuffer(ctx, bt->buffer);

    if (*buf == NULL)
        return false;

    *offset = bt->offset;
    *size = bt->size < 0 ? (GLsizeiptr)(*buf)->size - bt->offset : bt->size;

    return true;
}

/* ---- entry points ---- */

void mglTexBuffer(GLMContext ctx, GLenum target, GLenum internalformat, GLuint buffer)
{
    if (target != GL_TEXTURE_BUFFER)
    {
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    // The spec's error list for TexBuffer has no "nothing bound" case, so the
    // default buffer texture is a legal target here.
    Texture *tex = getTex(ctx, 0, GL_TEXTURE_BUFFER);
    if (!tex)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex_buffer_impl(ctx, tex, internalformat, buffer, 0, 0, false);
}

void mglTexBufferRange(GLMContext ctx, GLenum target, GLenum internalformat,
                       GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    if (target != GL_TEXTURE_BUFFER)
    {
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    Texture *tex = currentTexture(ctx, _TEXTURE_BUFFER_TARGET);
    if (!tex)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    STATE_VAR(texture_binding_buffer) = tex->name;

    tex_buffer_impl(ctx, tex, internalformat, buffer, offset, size, true);
}

void mglTextureBuffer(GLMContext ctx, GLuint texture, GLenum internalformat, GLuint buffer)
{
    Texture *tex = findTexture(ctx, texture);
    if (!tex)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex_buffer_impl(ctx, tex, internalformat, buffer, 0, 0, false);
}

void mglTextureBufferRange(GLMContext ctx, GLuint texture, GLenum internalformat,
                           GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    Texture *tex = findTexture(ctx, texture);
    if (!tex)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex_buffer_impl(ctx, tex, internalformat, buffer, offset, size, true);
}
