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
 * sparse_texture.c
 * MGL
 *
 * GL_ARB_sparse_texture. A sparse texture lives in a Metal sparse heap; a GL
 * page is 2x2 Metal tiles, which makes it the 64 KB standard page size of
 * GL_ARB_sparse_texture2 for every format that has one.
 */

#include "glcorearb.h"
#include "glm_context.h"

Texture *getTex(GLMContext ctx, GLuint name, GLenum target);
Texture *findTexture(GLMContext ctx, GLuint texture);

// The standard page of GL_ARB_sparse_texture2 table 8.X, by format
static bool standardPage(GLenum internalformat, GLint *x, GLint *y)
{
    switch (internalformat)
    {
        case GL_R8: case GL_R8_SNORM: case GL_R8I: case GL_R8UI:
            *x = 256; *y = 256; return true;

        case GL_R16: case GL_R16_SNORM: case GL_RG8: case GL_RG8_SNORM: case GL_RGB565:
        case GL_R16F: case GL_R16I: case GL_R16UI: case GL_RG8I: case GL_RG8UI:
        // beyond the table, with pages of the same size
        case GL_DEPTH_COMPONENT16:
            *x = 256; *y = 128; return true;

        case GL_DEPTH_COMPONENT32F:
            *x = 128; *y = 128; return true;

        case GL_RG16: case GL_RG16_SNORM: case GL_RGBA8: case GL_RGBA8_SNORM: case GL_RGB10_A2:
        case GL_RGB10_A2UI: case GL_RG16F: case GL_R32F: case GL_R11F_G11F_B10F: case GL_RGB9_E5:
        case GL_R32I: case GL_R32UI: case GL_RG16I: case GL_RG16UI: case GL_RGBA8I: case GL_RGBA8UI:
            *x = 128; *y = 128; return true;

        case GL_RGBA16: case GL_RGBA16_SNORM: case GL_RGBA16F: case GL_RG32F: case GL_RG32I:
        case GL_RG32UI: case GL_RGBA16I: case GL_RGBA16UI:
            *x = 128; *y = 64; return true;

        case GL_RGBA32F: case GL_RGBA32I: case GL_RGBA32UI:
            *x = 64; *y = 64; return true;
    }

    return false;
}

bool mglSparseTarget(GLenum target)
{
    switch (target)
    {
        case GL_TEXTURE_2D: case GL_TEXTURE_2D_ARRAY: case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_CUBE_MAP_ARRAY: case GL_TEXTURE_3D: case GL_TEXTURE_RECTANGLE:
        // GL_ARB_sparse_texture2
        case GL_TEXTURE_2D_MULTISAMPLE: case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
            return true;
    }

    return false;
}

// The page size of a format, or false when it cannot be sparse. Queries name
// any target and get the standard size; a texture that is really allocated
// also needs Metal's tiles to fit the page. A query of a multisample target
// does not say how many samples, so every count is checked.
bool mglSparsePageSize(GLMContext ctx, GLenum target, GLenum internalformat, GLsizei samples, GLint size[3])
{
    bool ms = target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
    GLsizei lo = ms ? (samples ? samples : 2) : 1, hi = ms ? (samples ? samples : 4) : 1;
    GLuint tile[3];

    if (!standardPage(internalformat, &size[0], &size[1]))
        return false;

    size[2] = 1;

    if (!mglSparseTarget(target))
        return true;

    for (GLsizei n = lo; n <= hi; n *= 2)
    {
        if (!ctx->mtl_funcs.mtlSparseTileSize(ctx, target, internalformat, n, tile))
            return false;

        if (size[0] % tile[0] || size[1] % tile[1] || tile[2] != 1)
            return false;
    }

    return true;
}

GLint mglSparseFormatQuery(GLMContext ctx, GLenum target, GLenum internalformat, GLenum pname)
{
    GLint size[3];
    bool ok = mglSparsePageSize(ctx, target, internalformat, 0, size);

    switch (pname)
    {
        case GL_NUM_VIRTUAL_PAGE_SIZES_ARB: return ok ? 1 : 0;
        case GL_VIRTUAL_PAGE_SIZE_X_ARB: return ok ? size[0] : 0;
        case GL_VIRTUAL_PAGE_SIZE_Y_ARB: return ok ? size[1] : 0;
        case GL_VIRTUAL_PAGE_SIZE_Z_ARB: return ok ? size[2] : 0;
    }

    return 0;
}

// TEXTURE_SPARSE_ARB and VIRTUAL_PAGE_SIZE_INDEX_ARB. True when pname is one
// of them; *err says what went wrong.
bool mglSparseTexParam(Texture *tex, GLenum pname, GLint v, GLenum *err)
{
    *err = GL_NO_ERROR;

    if (pname != GL_TEXTURE_SPARSE_ARB && pname != GL_VIRTUAL_PAGE_SIZE_INDEX_ARB)
        return false;

    if (tex->immutable_storage)
    {
        *err = GL_INVALID_OPERATION;
        return true;
    }

    if (pname == GL_TEXTURE_SPARSE_ARB)
    {
        if (v && !mglSparseTarget(tex->target))
            *err = GL_INVALID_VALUE;
        else
            tex->sparse = v ? GL_TRUE : GL_FALSE;
    }
    else if (v < 0)
        *err = GL_INVALID_VALUE;
    else
        tex->page_index = v;

    return true;
}

bool mglSparseTexObjectParam(const Texture *tex, GLenum pname, GLint *out)
{
    switch (pname)
    {
        case GL_TEXTURE_SPARSE_ARB: *out = tex->sparse; return true;
        case GL_VIRTUAL_PAGE_SIZE_INDEX_ARB: *out = tex->page_index; return true;
        case GL_NUM_SPARSE_LEVELS_ARB: *out = (GLint)tex->sparse_levels; return true;
    }

    return false;
}

// What TexStorage refuses for a sparse texture. Sets the count of levels that
// can be partly committed when it takes it.
GLenum mglSparseStorageError(GLMContext ctx, Texture *tex, GLsizei levels, GLenum internalformat,
                             GLsizei width, GLsizei height, GLsizei depth)
{
    GLint page[3];
    GLuint n = 0;

    if (!tex->sparse)
        return GL_NO_ERROR;

    if (!mglSparsePageSize(ctx, tex->target, internalformat, tex->samples, page) || tex->page_index >= 1)
        return GL_INVALID_OPERATION;

    if (tex->target == GL_TEXTURE_3D)
    {
        if (width > MGL_MAX_SPARSE_3D_SIZE || height > MGL_MAX_SPARSE_3D_SIZE || depth > MGL_MAX_SPARSE_3D_SIZE)
            return GL_INVALID_VALUE;
    }
    else if (width > MGL_MAX_SPARSE_SIZE || height > MGL_MAX_SPARSE_SIZE)
        return GL_INVALID_VALUE;

    if ((tex->target == GL_TEXTURE_2D_ARRAY || tex->target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
        depth > MGL_MAX_SPARSE_LAYERS)
        return GL_INVALID_VALUE;

    if (width % page[0] || height % page[1] || (tex->target == GL_TEXTURE_3D && depth % page[2]))
        return GL_INVALID_VALUE;

    // GL_ARB_sparse_texture2: every level at least a page in each direction
    while (n < (GLuint)levels && (width >> n) >= page[0] && (height >> n) >= page[1])
        n++;

    tex->sparse_levels = n;
    return GL_NO_ERROR;
}

static GLuint levelLayers(Texture *tex, GLint level)
{
    switch (tex->target)
    {
        case GL_TEXTURE_2D_ARRAY: case GL_TEXTURE_CUBE_MAP_ARRAY:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: return tex->depth;
        case GL_TEXTURE_CUBE_MAP: return 6;
        case GL_TEXTURE_3D: return tex->depth >> level ? tex->depth >> level : 1;
    }

    return 1;
}

static void pageCommitment(GLMContext ctx, Texture *tex, GLint level, GLint x, GLint y, GLint z,
                           GLsizei w, GLsizei h, GLsizei d, GLboolean commit)
{
    GLint page[3];
    GLuint lw, lh, ld;

    ERROR_CHECK_RETURN(tex->immutable_storage && tex->sparse, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(level >= 0 && (GLuint)level < tex->mipmap_levels, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(x >= 0 && y >= 0 && z >= 0 && w >= 0 && h >= 0 && d >= 0, GL_INVALID_VALUE);

    lw = tex->width >> level ? tex->width >> level : 1;
    lh = tex->height >> level ? tex->height >> level : 1;
    ld = levelLayers(tex, level);

    ERROR_CHECK_RETURN((GLuint)(x + w) <= lw && (GLuint)(y + h) <= lh && (GLuint)(z + d) <= ld,
                       GL_INVALID_OPERATION);

    mglSparsePageSize(ctx, tex->target, tex->internalformat, tex->samples, page);

    ERROR_CHECK_RETURN(x % page[0] == 0 && y % page[1] == 0 && z % page[2] == 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(w % page[0] == 0 || (GLuint)(x + w) == lw, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(h % page[1] == 0 || (GLuint)(y + h) == lh, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(d % page[2] == 0 || (GLuint)(z + d) == ld, GL_INVALID_OPERATION);

    if (w == 0 || h == 0 || d == 0)
        return;

    if (!tex->mtl_data)
        ctx->mtl_funcs.mtlBindTexture(ctx, tex);

    ERROR_CHECK_RETURN(ctx->mtl_funcs.mtlSparseCommit(ctx, tex, level, x, y, z, w, h, d, commit),
                       GL_OUT_OF_MEMORY);
}

void mglTexPageCommitment(GLMContext ctx, GLenum target, GLint level, GLint x, GLint y, GLint z,
                          GLsizei w, GLsizei h, GLsizei d, GLboolean commit)
{
    Texture *tex;

    ERROR_CHECK_RETURN(mglSparseTarget(target), GL_INVALID_ENUM);

    tex = getTex(ctx, 0, target);

    if (tex)
        pageCommitment(ctx, tex, level, x, y, z, w, h, d, commit);
}

void mglTexturePageCommitment(GLMContext ctx, GLuint texture, GLint level, GLint x, GLint y, GLint z,
                              GLsizei w, GLsizei h, GLsizei d, GLboolean commit)
{
    Texture *tex = findTexture(ctx, texture);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    pageCommitment(ctx, tex, level, x, y, z, w, h, d, commit);
}

#ifdef MGL_GL_CORE
extern GLMContext _ctx;
extern void mgl_lazy_init(void);

void glTexPageCommitmentARB(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                            GLsizei width, GLsizei height, GLsizei depth, GLboolean commit)
{
    mgl_lazy_init();
    mglTexPageCommitment(_ctx, target, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}

void glTexturePageCommitmentEXT(GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                GLsizei width, GLsizei height, GLsizei depth, GLboolean commit)
{
    mgl_lazy_init();
    mglTexturePageCommitment(_ctx, texture, level, xoffset, yoffset, zoffset, width, height, depth, commit);
}
#endif
