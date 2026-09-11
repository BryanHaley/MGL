/*
 * Copyright (C) The Moogle Project
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
 * texture_multisample.c
 * MGL
 *
 * Multisample texture storage and image specification.
 * Covers the six core entry points for 2D / 2D-array multisample
 * textures, in both bind-to-target and DSA forms.
 */

#include "glm_context.h"
#include "mgl_log.h"
#include "pixel_utils.h"

// Helpers shared with textures.c — these live there, we only call them.
extern Texture *getTex(GLMContext ctx, GLuint name, GLenum target);
extern bool    checkInternalFormatForMetal(GLMContext ctx, GLuint internalformat);
extern void    texStorage(GLMContext ctx, Texture *tex, GLuint faces,
                          GLsizei levels, GLboolean is_array,
                          GLenum internalformat, GLsizei width,
                          GLsizei height, GLsizei depth, GLboolean proxy);
extern bool    createTextureLevel(GLMContext ctx, Texture *tex, GLuint face,
                                  GLint level, GLboolean is_array,
                                  GLint internalformat, GLsizei width,
                                  GLsizei height, GLsizei depth,
                                  GLenum format, GLenum type,
                                  void *pixels, GLboolean proxy);

// ---------------------------------------------------------------------------
// Map an internal format to the sample-count limit that applies to it.
// Returns 0 when the format class is unknown (caller handles this).
// ---------------------------------------------------------------------------
static GLsizei maxSamplesForFormat(GLMContext ctx, GLenum internalformat)
{
    switch (internalformat) {
        /* integer formats — GL_MAX_INTEGER_SAMPLES */
        case GL_R8I:   case GL_R8UI:
        case GL_R16I:  case GL_R16UI:
        case GL_R32I:  case GL_R32UI:
        case GL_RG8I:  case GL_RG8UI:
        case GL_RG16I: case GL_RG16UI:
        case GL_RG32I: case GL_RG32UI:
        case GL_RGBA8I:  case GL_RGBA8UI:
        case GL_RGBA16I: case GL_RGBA16UI:
        case GL_RGBA32I: case GL_RGBA32UI:
            return STATE_VAR(max_integer_samples);

        /* depth / stencil formats — GL_MAX_DEPTH_TEXTURE_SAMPLES */
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
        case GL_DEPTH_COMPONENT32F:
        case GL_DEPTH24_STENCIL8:
        case GL_DEPTH32F_STENCIL8:
        case GL_STENCIL_INDEX8:
            return STATE_VAR(max_depth_texture_samples);

        default:
            break;
    }

    /* everything else is a color-renderable format */
    return STATE_VAR(max_color_texture_samples);
}

// ---------------------------------------------------------------------------
// Single set of validation rules shared by all six entry points.
// Sets the GL error and returns false on failure.
// ---------------------------------------------------------------------------
static bool validateMultisample(GLMContext ctx, GLenum target, GLsizei samples,
                                GLenum internalformat, GLsizei width,
                                GLsizei height, GLsizei depth, bool is_3d)
{
    /* --- target --- */
    switch (target) {
        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_PROXY_TEXTURE_2D_MULTISAMPLE:
            if (is_3d) goto invalid_enum;
            break;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        case GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY:
            break;
        default:
            goto invalid_enum;
    }

    /* --- samples --- */
    if (samples < 1) {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
    }
    {
        GLsizei limit = maxSamplesForFormat(ctx, internalformat);
        if (limit > 0 && samples > limit) {
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
        }
    }

    /* --- internal format --- */
    if (!checkInternalFormatForMetal(ctx, internalformat)) {
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    /* --- dimensions --- */
    if (width < 1 || height < 1 || depth < 1) {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
    }

    return true;

invalid_enum:
    ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
}

// ===========================================================================
//  glTexStorage2DMultisample
// ===========================================================================
void mglTexStorage2DMultisample(GLMContext ctx, GLenum target, GLsizei samples,
                                GLenum internalformat, GLsizei width,
                                GLsizei height, GLboolean fixedsamplelocations)
{
    Texture *tex;
    GLboolean proxy = false;

    if (!validateMultisample(ctx, target, samples, internalformat,
                              width, height, 1, false))
        return;

    if (target == GL_PROXY_TEXTURE_2D_MULTISAMPLE)
        proxy = true;

    tex = getTex(ctx, 0, target);
    if (!tex) return;

    if (tex->immutable_storage) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex->samples = samples;
    (void)fixedsamplelocations; /* no per-texture field yet */

    texStorage(ctx, tex, 1, 1, false, internalformat, width, height, 1, proxy);
}

// ===========================================================================
//  glTexStorage3DMultisample
// ===========================================================================
void mglTexStorage3DMultisample(GLMContext ctx, GLenum target, GLsizei samples,
                                GLenum internalformat, GLsizei width,
                                GLsizei height, GLsizei depth,
                                GLboolean fixedsamplelocations)
{
    Texture *tex;
    GLboolean is_array = false;
    GLboolean proxy    = false;

    if (!validateMultisample(ctx, target, samples, internalformat,
                              width, height, depth, true))
        return;

    if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        is_array = true;
    if (target == GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        is_array = true;
        proxy    = true;
    }

    tex = getTex(ctx, 0, target);
    if (!tex) return;

    if (tex->immutable_storage) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex->samples = samples;
    (void)fixedsamplelocations;

    texStorage(ctx, tex, 1, 1, is_array, internalformat,
               width, height, depth, proxy);
}

// ===========================================================================
//  glTexImage2DMultisample
// ===========================================================================
void mglTexImage2DMultisample(GLMContext ctx, GLenum target, GLsizei samples,
                              GLenum internalformat, GLsizei width,
                              GLsizei height, GLboolean fixedsamplelocations)
{
    Texture *tex;

    if (!validateMultisample(ctx, target, samples, internalformat,
                              width, height, 1, false))
        return;

    tex = getTex(ctx, 0, target);
    if (!tex) return;

    /* TexImage form does NOT make the texture immutable. */
    if (tex->immutable_storage) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex->samples = samples;
    (void)fixedsamplelocations;

    createTextureLevel(ctx, tex, 0, 0, false, internalformat,
                       width, height, 1, 0, 0, NULL,
                       target == GL_PROXY_TEXTURE_2D_MULTISAMPLE);

    tex->access = GL_READ_ONLY;
    ctx->mtl_funcs.mtlBindTexture(ctx, tex);
}

// ===========================================================================
//  glTexImage3DMultisample
// ===========================================================================
void mglTexImage3DMultisample(GLMContext ctx, GLenum target, GLsizei samples,
                              GLenum internalformat, GLsizei width,
                              GLsizei height, GLsizei depth,
                              GLboolean fixedsamplelocations)
{
    Texture *tex;
    GLboolean is_array = false;

    if (!validateMultisample(ctx, target, samples, internalformat,
                              width, height, depth, true))
        return;

    if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
        is_array = true;

    tex = getTex(ctx, 0, target);
    if (!tex) return;

    if (tex->immutable_storage) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex->samples = samples;
    (void)fixedsamplelocations;

    createTextureLevel(ctx, tex, 0, 0, is_array, internalformat,
                       width, height, depth, 0, 0, NULL,
                       target == GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY);

    tex->access = GL_READ_ONLY;
    ctx->mtl_funcs.mtlBindTexture(ctx, tex);
}

// ===========================================================================
//  glTextureStorage2DMultisample  (DSA)
// ===========================================================================
void mglTextureStorage2DMultisample(GLMContext ctx, GLuint texture,
                                    GLsizei samples, GLenum internalformat,
                                    GLsizei width, GLsizei height,
                                    GLboolean fixedsamplelocations)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);
    if (!tex) return;

    if (tex->target != GL_TEXTURE_2D_MULTISAMPLE) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!validateMultisample(ctx, tex->target, samples, internalformat,
                              width, height, 1, false))
        return;

    if (tex->immutable_storage) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex->samples = samples;
    (void)fixedsamplelocations;

    texStorage(ctx, tex, 1, 1, false, internalformat, width, height, 1, false);
}

// ===========================================================================
//  glTextureStorage3DMultisample  (DSA)
// ===========================================================================
void mglTextureStorage3DMultisample(GLMContext ctx, GLuint texture,
                                    GLsizei samples, GLenum internalformat,
                                    GLsizei width, GLsizei height,
                                    GLsizei depth,
                                    GLboolean fixedsamplelocations)
{
    Texture *tex;
    GLboolean is_array = false;

    tex = getTex(ctx, texture, 0);
    if (!tex) return;

    if (tex->target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) {
        is_array = true;
    } else {
        /* DSA 3D form requires the array target. */
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!validateMultisample(ctx, tex->target, samples, internalformat,
                              width, height, depth, true))
        return;

    if (tex->immutable_storage) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    tex->samples = samples;
    (void)fixedsamplelocations;

    texStorage(ctx, tex, 1, 1, is_array, internalformat,
               width, height, depth, false);
}
