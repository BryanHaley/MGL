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
 * tex_params.c
 * MGL
 *
 */

#include "glm_context.h"
#include "mgl_format_table.h"
#include "pixel_utils.h"

extern GLuint textureIndexFromTarget(GLMContext ctx, GLenum target);
extern Texture *currentTexture(GLMContext ctx, GLuint index);
extern Texture *findTexture(GLMContext ctx, GLuint texture);
extern Texture *newTexture(GLMContext ctx, GLenum target, GLuint texture);
Texture *getTex(GLMContext ctx, GLuint name, GLenum target);

// the texture a glTexture*() call names. A name handed out by glGenTextures
// but never bound has no object yet, so make one, the same way a bind would.
static Texture *dsaTex(GLMContext ctx, GLuint texture)
{
    Texture *tex;

    ERROR_CHECK_RETURN_VALUE(texture, GL_INVALID_OPERATION, NULL);

    tex = findTexture(ctx, texture);

    if (!tex && texture < STATE(texture_table.current_name))
    {
        tex = newTexture(ctx, GL_TEXTURE_2D, texture);

        if (tex)
            insertHashElement(&STATE(texture_table), texture, tex);
    }

    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, NULL);

    return tex;
}

#pragma mark set params
// Metal only needs a swizzled view when the channels are not the identity.
// GL names only these six as swizzle sources; anything else is an error
static bool swizzleValueIsLegal(GLint v)
{
    switch (v)
    {
        case GL_RED:
        case GL_GREEN:
        case GL_BLUE:
        case GL_ALPHA:
        case GL_ZERO:
        case GL_ONE:
            return true;
        default:
            return false;
    }
}

static void refreshSwizzled(TextureParameter *tex_params)
{

    tex_params->swizzled = (tex_params->swizzle_r != GL_RED)   ||
                           (tex_params->swizzle_g != GL_GREEN) ||
                           (tex_params->swizzle_b != GL_BLUE)  ||
                           (tex_params->swizzle_a != GL_ALPHA);
}

bool setTexParmi(GLMContext ctx, TextureParameter *tex_params, GLenum pname, const GLint *param)
{
    switch(pname)
    {
        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            switch(*param)
            {
                case GL_DEPTH_COMPONENT:
                case GL_STENCIL_INDEX:
                    tex_params->depth_stencil_mode = *param;
                    break;

                default:
                    ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
            }
            break;

        case GL_TEXTURE_BASE_LEVEL:
            tex_params->base_level = *param;
            // need to compare this against something...
            break;

        case GL_TEXTURE_COMPARE_FUNC:
            switch(*param)
            {
                case GL_LEQUAL:
                case GL_GEQUAL:
                case GL_LESS:
                case GL_GREATER:
                case GL_EQUAL:
                case GL_NOTEQUAL:
                case GL_ALWAYS:
                case GL_NEVER:
                    tex_params->compare_func = *param;
                    break;

                default:
                    ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
            }
            break;

        case GL_TEXTURE_COMPARE_MODE:
            switch(*param)
            {
                case GL_COMPARE_REF_TO_TEXTURE:
                case GL_NONE:
                    tex_params->compare_mode = *param;
                    break;

                default:
                    ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
            }
            break;

        case GL_TEXTURE_MIN_FILTER:
            switch(*param)
            {
                case GL_NEAREST:
                case GL_LINEAR:
                case GL_NEAREST_MIPMAP_NEAREST:
                case GL_LINEAR_MIPMAP_NEAREST:
                case GL_NEAREST_MIPMAP_LINEAR:
                case GL_LINEAR_MIPMAP_LINEAR:
                    tex_params->min_filter = *param;
                    break;

                default:
                    ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
            }
            break;

        case GL_TEXTURE_MAG_FILTER:
            switch(*param)
            {
                case GL_NEAREST:
                case GL_LINEAR:
                    tex_params->mag_filter = *param;
                    break;

                default:
                    ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
            }
            break;

        case GL_TEXTURE_MIN_LOD:
            tex_params->min_lod = *param;
            break;

        case GL_TEXTURE_MAX_LOD:
            tex_params->max_lod = *param;
            break;

        case GL_TEXTURE_MAX_LEVEL:
            tex_params->max_level = *param;
            break;

        case GL_TEXTURE_SWIZZLE_R:
            ERROR_CHECK_RETURN_VALUE(swizzleValueIsLegal(*param), GL_INVALID_ENUM, false);
            tex_params->swizzle_r = *param;
            refreshSwizzled(tex_params);
            break;

        case GL_TEXTURE_SWIZZLE_G:
            ERROR_CHECK_RETURN_VALUE(swizzleValueIsLegal(*param), GL_INVALID_ENUM, false);
            tex_params->swizzle_g = *param;
            refreshSwizzled(tex_params);
            break;

        case GL_TEXTURE_SWIZZLE_B:
            ERROR_CHECK_RETURN_VALUE(swizzleValueIsLegal(*param), GL_INVALID_ENUM, false);
            tex_params->swizzle_b = *param;
            refreshSwizzled(tex_params);
            break;

        case GL_TEXTURE_SWIZZLE_A:
            ERROR_CHECK_RETURN_VALUE(swizzleValueIsLegal(*param), GL_INVALID_ENUM, false);
            tex_params->swizzle_a = *param;
            refreshSwizzled(tex_params);
            break;

        case GL_TEXTURE_WRAP_S:
            tex_params->wrap_s = *param;
            break;

        case GL_TEXTURE_WRAP_T:
            tex_params->wrap_t = *param;
            break;

        case GL_TEXTURE_WRAP_R:
            tex_params->wrap_r = *param;
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool setTexParamsi(GLMContext ctx, TextureParameter *tex_params, GLenum pname, const GLint *params)
{
    switch(pname)
    {
        case GL_TEXTURE_BORDER_COLOR:
            for(int i=0; i<4; i++)
                tex_params->border_color[i] = (GLint)params[i];
            break;

        case GL_TEXTURE_SWIZZLE_RGBA:
            for (int i = 0; i < 4; i++)
                ERROR_CHECK_RETURN_VALUE(swizzleValueIsLegal(params[i]), GL_INVALID_ENUM, false);
            tex_params->swizzle_r = params[0];
            tex_params->swizzle_g = params[1];
            tex_params->swizzle_b = params[2];
            tex_params->swizzle_a = params[3];
            refreshSwizzled(tex_params);
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool setTexParamsIiv(GLMContext ctx, TextureParameter *tex_params, GLenum pname, const GLint *params)
{
    switch(pname)
    {
        case GL_TEXTURE_BORDER_COLOR:
            for(int i=0; i<4; i++)
                tex_params->border_color_i[i] = params[i];
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool setTexParamsIuiv(GLMContext ctx, TextureParameter *tex_params, GLenum pname, const GLuint *params)
{
    switch(pname)
    {
        case GL_TEXTURE_BORDER_COLOR:
            for(int i=0; i<4; i++)
                tex_params->border_color_ui[i] = params[i];
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool setTexParmf(GLMContext ctx, TextureParameter *tex_params, GLenum pname, const GLfloat *param)
{
    switch(pname)
    {
        case GL_TEXTURE_LOD_BIAS:
            tex_params->lod_bias = *param;
            break;

        case GL_TEXTURE_MAX_ANISOTROPY:
            tex_params->max_anisotropy = *param;
            break;

        case GL_TEXTURE_MIN_LOD:
            tex_params->min_lod = *param;
            break;

        case GL_TEXTURE_MAX_LOD:
            tex_params->max_lod = *param;
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool setTexParamsf(GLMContext ctx, TextureParameter *tex_params, GLenum pname, const GLfloat *params)
{
    switch(pname)
    {
        case GL_TEXTURE_BORDER_COLOR:
            for(int i=0; i<4; i++)
                tex_params->border_color[i] = params[i];
            break;

        case GL_TEXTURE_SWIZZLE_RGBA:
            for (int i = 0; i < 4; i++)
                ERROR_CHECK_RETURN_VALUE(swizzleValueIsLegal((GLint)params[i]), GL_INVALID_ENUM, false);
            tex_params->swizzle_r = (GLint)params[0];
            tex_params->swizzle_g = (GLint)params[1];
            tex_params->swizzle_b = (GLint)params[2];
            tex_params->swizzle_a = (GLint)params[3];
            refreshSwizzled(tex_params);
            break;

        default:
            return false;
            break;
    }

    return true;
}

#pragma mark get params
static bool getTexParmi(GLMContext ctx, TextureParameter *tex_params, const GLenum pname, GLint *ret)
{
    switch(pname)
    {
        case GL_DEPTH_STENCIL_TEXTURE_MODE:
            *ret = tex_params->depth_stencil_mode;
            break;

        case GL_TEXTURE_BASE_LEVEL:
            *ret = tex_params->base_level;
            // need to compare this against something...
            break;

        case GL_TEXTURE_COMPARE_FUNC:
            *ret = tex_params->compare_func;
            break;

        case GL_TEXTURE_COMPARE_MODE:
            *ret = tex_params->compare_mode;
            break;

        case GL_TEXTURE_MIN_FILTER:
            *ret = tex_params->min_filter;
            break;

        case GL_TEXTURE_MAG_FILTER:
            *ret = tex_params->mag_filter;
            break;

        case GL_TEXTURE_MIN_LOD:
            *ret = tex_params->min_lod;
            break;

        case GL_TEXTURE_MAX_LOD:
            *ret = tex_params->max_lod;
            break;

        case GL_TEXTURE_MAX_LEVEL:
            *ret = tex_params->max_level;
            break;

        case GL_TEXTURE_SWIZZLE_R:
            *ret = tex_params->swizzle_r;
            break;

        case GL_TEXTURE_SWIZZLE_G:
            *ret = tex_params->swizzle_g;
            break;

        case GL_TEXTURE_SWIZZLE_B:
            *ret = tex_params->swizzle_b;
            break;

        case GL_TEXTURE_SWIZZLE_A:
            *ret = tex_params->swizzle_a;
            break;

        case GL_TEXTURE_WRAP_S:
            *ret = tex_params->wrap_s;
            break;

        case GL_TEXTURE_WRAP_T:
            *ret = tex_params->wrap_t;
            break;

        case GL_TEXTURE_WRAP_R:
            *ret = tex_params->wrap_r;
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool getTexParamsi(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLint *ret)
{
    switch(pname)
    {
        case GL_TEXTURE_BORDER_COLOR:
            for(int i=0; i<4; i++)
                ret[i] = tex_params->border_color[i];
            break;

        case GL_TEXTURE_SWIZZLE_RGBA:
            *ret++ = tex_params->swizzle_r;
            *ret++ = tex_params->swizzle_g;
            *ret++ = tex_params->swizzle_b;
            *ret++ = tex_params->swizzle_a;
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool getTexParamsIiv(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLint *ret)
{
    switch(pname)
    {
        case GL_TEXTURE_BORDER_COLOR:
            for(int i=0; i<4; i++)
                ret[i] = tex_params->border_color_i[i];
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool getTexParamsIuiv(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLuint *ret)
{
    switch(pname)
    {
        case GL_TEXTURE_BORDER_COLOR:
            for(int i=0; i<4; i++)
                ret[i] = tex_params->border_color_ui[i];
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool getTexParamsf(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLfloat *ret)
{
    switch(pname)
    {
        case GL_TEXTURE_BORDER_COLOR:
            for(int i=0; i<4; i++)
                ret[i] = tex_params->border_color[i];
            break;

        case GL_TEXTURE_SWIZZLE_RGBA:
            *ret++ = tex_params->swizzle_r;
            *ret++ = tex_params->swizzle_g;
            *ret++ = tex_params->swizzle_b;
            *ret++ = tex_params->swizzle_a;
            break;

        default:
            return false;
            break;
    }

    return true;
}

static bool getTexParmf(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLfloat *ret)
{
    switch(pname)
    {
        case GL_TEXTURE_LOD_BIAS:
            *ret = tex_params->lod_bias;
            break;

        case GL_TEXTURE_MAX_ANISOTROPY:
            *ret = tex_params->max_anisotropy;
            break;

        case GL_TEXTURE_MIN_LOD:
            *ret = tex_params->min_lod;
            break;

        case GL_TEXTURE_MAX_LOD:
            *ret = tex_params->max_lod;
            break;

        default:
            return false;
            break;
    }

    return true;
}

bool setParam(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLint iparam, GLfloat fparam)
{
    if (iparam)
    {
        if (setTexParmi(ctx, tex_params, pname, &iparam))
            return true;

        fparam = (float)iparam;
        if (setTexParmf(ctx, tex_params, pname, &fparam))
            return true;
    }
    else
    {
        if (setTexParmf(ctx, tex_params, pname, &fparam))
            return true;

        iparam = (GLint)fparam;
        if (setTexParmi(ctx, tex_params, pname, &iparam))
            return true;
    }

    return false;
}

bool getParam(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLint *iparam, GLfloat *fparam)
{
    if (iparam)
    {
        if (getTexParmi(ctx, tex_params, pname, iparam))
            return true;

        if (getTexParmf(ctx, tex_params, pname, fparam))
            return true;
    }
    else
    {
        if (getTexParmf(ctx, tex_params, pname, fparam))
            return true;

        if (getTexParmi(ctx, tex_params, pname, iparam))
            return true;
    }

    return false;
}

// The tail every setter shares: store the value or complain about the pname.
// Only the four-value parameters raise the dirty bit; the renderer throws the
// texture's pixels away when it sees one, so a plain filter change must not.
static void setTexParamiv(GLMContext ctx, Texture *tex, GLenum pname, const GLint *params)
{
    if (setTexParamsi(ctx, &tex->params, pname, params))
    {
        tex->dirty_bits |= DIRTY_TEXTURE_PARAM;
        // the draw path only rebinds textures when the context says so, so a
        // change between two draws in one pass was invisible until a flush
        ctx->state.dirty_bits |= DIRTY_TEX;

        return;
    }

    if (setParam(ctx, &tex->params, pname, *params, (GLfloat)*params))
    {
        // a single-valued parameter is still state the sampler and the swizzle
        // are built from, so the next draw has to rebind
        tex->dirty_bits |= DIRTY_TEXTURE_PARAM;
        ctx->state.dirty_bits |= DIRTY_TEX;

        return;
    }

    ERROR_RETURN(GL_INVALID_ENUM);
}

static void setTexParamfv(GLMContext ctx, Texture *tex, GLenum pname, const GLfloat *params)
{
    if (setTexParamsf(ctx, &tex->params, pname, params))
    {
        tex->dirty_bits |= DIRTY_TEXTURE_PARAM;
        // the draw path only rebinds textures when the context says so, so a
        // change between two draws in one pass was invisible until a flush
        ctx->state.dirty_bits |= DIRTY_TEX;

        return;
    }

    if (setParam(ctx, &tex->params, pname, 0, *params))
    {
        // a single-valued parameter is still state the sampler and the swizzle
        // are built from, so the next draw has to rebind
        tex->dirty_bits |= DIRTY_TEXTURE_PARAM;
        ctx->state.dirty_bits |= DIRTY_TEX;

        return;
    }

    ERROR_RETURN(GL_INVALID_ENUM);
}

#pragma mark tex param gl calls
void mglTexParameterf(GLMContext ctx, GLenum target, GLenum pname, GLfloat param)
{
    Texture *tex;

    tex = getTex(ctx, 0, target);

    if (tex)
        setTexParamfv(ctx, tex, pname, &param);
}

void mglTexParameterfv(GLMContext ctx, GLenum target, GLenum pname, const GLfloat *params)
{
    Texture *tex;

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (tex)
        setTexParamfv(ctx, tex, pname, params);
}

void mglTexParameteri(GLMContext ctx, GLenum target, GLenum pname, GLint param)
{
    Texture *tex;

    tex = getTex(ctx, 0, target);

    if (tex)
        setTexParamiv(ctx, tex, pname, &param);
}

void mglTexParameteriv(GLMContext ctx, GLenum target, GLenum pname, const GLint *params)
{
    Texture *tex;

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (tex)
        setTexParamiv(ctx, tex, pname, params);
}

void mglTexParameterIiv(GLMContext ctx, GLenum target, GLenum pname, const GLint *params)
{
    Texture *tex;

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (!tex)
        return;

    if (setTexParamsIiv(ctx, &tex->params, pname, params))
    {
        tex->dirty_bits |= DIRTY_TEXTURE_PARAM;
        // the draw path only rebinds textures when the context says so, so a
        // change between two draws in one pass was invisible until a flush
        ctx->state.dirty_bits |= DIRTY_TEX;

        return;
    }

    setTexParamiv(ctx, tex, pname, params);
}

void mglTexParameterIuiv(GLMContext ctx, GLenum target, GLenum pname, const GLuint *params)
{
    Texture *tex;

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (!tex)
        return;

    if (setTexParamsIuiv(ctx, &tex->params, pname, params))
    {
        tex->dirty_bits |= DIRTY_TEXTURE_PARAM;
        // the draw path only rebinds textures when the context says so, so a
        // change between two draws in one pass was invisible until a flush
        ctx->state.dirty_bits |= DIRTY_TEX;

        return;
    }

    setTexParamiv(ctx, tex, pname, (const GLint *)params);
}

void mglTextureParameterf(GLMContext ctx, GLuint texture, GLenum pname, GLfloat param)
{
    Texture *tex;

    tex = dsaTex(ctx, texture);

    if (tex)
        setTexParamfv(ctx, tex, pname, &param);
}

void mglTextureParameterfv(GLMContext ctx, GLuint texture, GLenum pname, const GLfloat *param)
{
    Texture *tex;

    tex = dsaTex(ctx, texture);

    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    if (tex)
        setTexParamfv(ctx, tex, pname, param);
}

void mglTextureParameteri(GLMContext ctx, GLuint texture, GLenum pname, GLint param)
{
    Texture *tex;

    tex = dsaTex(ctx, texture);

    if (tex)
        setTexParamiv(ctx, tex, pname, &param);
}

void mglTextureParameteriv(GLMContext ctx, GLuint texture, GLenum pname, const GLint *param)
{
    Texture *tex;

    tex = dsaTex(ctx, texture);

    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    if (tex)
        setTexParamiv(ctx, tex, pname, param);
}

void mglTextureParameterIiv(GLMContext ctx, GLuint texture, GLenum pname, const GLint *params)
{
    Texture *tex;

    tex = dsaTex(ctx, texture);

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (!tex)
        return;

    if (setTexParamsIiv(ctx, &tex->params, pname, params))
    {
        tex->dirty_bits |= DIRTY_TEXTURE_PARAM;
        // the draw path only rebinds textures when the context says so, so a
        // change between two draws in one pass was invisible until a flush
        ctx->state.dirty_bits |= DIRTY_TEX;

        return;
    }

    setTexParamiv(ctx, tex, pname, params);
}

void mglTextureParameterIuiv(GLMContext ctx, GLuint texture, GLenum pname, const GLuint *params)
{
    Texture *tex;

    tex = dsaTex(ctx, texture);

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (!tex)
        return;

    if (setTexParamsIuiv(ctx, &tex->params, pname, params))
    {
        tex->dirty_bits |= DIRTY_TEXTURE_PARAM;
        // the draw path only rebinds textures when the context says so, so a
        // change between two draws in one pass was invisible until a flush
        ctx->state.dirty_bits |= DIRTY_TEX;

        return;
    }

    setTexParamiv(ctx, tex, pname, (const GLint *)params);
}

#pragma mark get tex param gl calls
void getTexParamfv(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLfloat *params)
{
    GLint iparam = 0;

    if (getTexParamsf(ctx, tex_params, pname, params))
        return;

    if (getTexParmf(ctx, tex_params, pname, params))
        return;

    if (getTexParmi(ctx, tex_params, pname, &iparam))
    {
        *params = (GLfloat)iparam;

        return;
    }

    ERROR_RETURN(GL_INVALID_ENUM);
}

void getTexParamiv(GLMContext ctx, TextureParameter *tex_params, GLenum pname, GLint *params)
{
    GLfloat fparam = 0.0f;

    if (getTexParamsi(ctx, tex_params, pname, params))
        return;

    if (getTexParmi(ctx, tex_params, pname, params))
        return;

    if (getTexParmf(ctx, tex_params, pname, &fparam))
    {
        // float parameters round on the way out to an integer query
        *params = (GLint)(fparam + (fparam < 0.0f ? -0.5f : 0.5f));

        return;
    }

    ERROR_RETURN(GL_INVALID_ENUM);
}

void mglGetTexParameterfv(GLMContext ctx, GLenum target, GLenum pname, GLfloat *params)
{
    Texture *tex;

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (tex)
        getTexParamfv(ctx, &tex->params, pname, params);
}

void mglGetTexParameteriv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    Texture *tex;

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (tex)
        getTexParamiv(ctx, &tex->params, pname, params);
}

static bool getTexLevelParameter(GLMContext ctx, Texture *tex, GLint level, GLenum pname, GLint *out)
{
    TextureLevel *lvl;

    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(level >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE((GLuint)level < tex->num_levels, GL_INVALID_VALUE, false);

    lvl = &tex->faces[0].levels[level];

    switch(pname)
    {
        case GL_TEXTURE_WIDTH:           *out = lvl->width;  return true;
        case GL_TEXTURE_HEIGHT:          *out = lvl->height; return true;
        case GL_TEXTURE_DEPTH:           *out = lvl->depth;  return true;
        case GL_TEXTURE_INTERNAL_FORMAT: *out = tex->internalformat; return true;
        case GL_TEXTURE_SAMPLES:         *out = tex->samples; return true;

        case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
            *out = GL_TRUE;
            return true;

        case GL_TEXTURE_COMPRESSED:
            *out = mglFormatIsCompressed(tex->internalformat) ? GL_TRUE : GL_FALSE;
            return true;

        case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
            // data_size is the padded allocation, not the packed block count
            if (!mglFormatIsCompressed(tex->internalformat))
                ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
            *out = (GLint)mglFormatImageSize(tex->internalformat,
                                             lvl->width, lvl->height, lvl->depth);
            return true;

        case GL_TEXTURE_BUFFER_OFFSET:
        case GL_TEXTURE_BUFFER_SIZE:
            *out = 0;
            return true;

        case GL_TEXTURE_RED_SIZE:
            *out = bitcountForInternalFormat(tex->internalformat, GL_RED); return true;
        case GL_TEXTURE_GREEN_SIZE:
            *out = bitcountForInternalFormat(tex->internalformat, GL_GREEN); return true;
        case GL_TEXTURE_BLUE_SIZE:
            *out = bitcountForInternalFormat(tex->internalformat, GL_BLUE); return true;
        case GL_TEXTURE_ALPHA_SIZE:
            *out = bitcountForInternalFormat(tex->internalformat, GL_ALPHA); return true;
        case GL_TEXTURE_DEPTH_SIZE:
            *out = bitcountForInternalFormat(tex->internalformat, GL_DEPTH_COMPONENT); return true;
        case GL_TEXTURE_STENCIL_SIZE:
            *out = bitcountForInternalFormat(tex->internalformat, GL_STENCIL_INDEX); return true;

        // a channel the format does not have answers GL_NONE
        case GL_TEXTURE_RED_TYPE:
        case GL_TEXTURE_GREEN_TYPE:
        case GL_TEXTURE_BLUE_TYPE:
        case GL_TEXTURE_ALPHA_TYPE:
        case GL_TEXTURE_DEPTH_TYPE:
        {
            GLenum channel = pname == GL_TEXTURE_RED_TYPE   ? GL_RED :
                             pname == GL_TEXTURE_GREEN_TYPE ? GL_GREEN :
                             pname == GL_TEXTURE_BLUE_TYPE  ? GL_BLUE :
                             pname == GL_TEXTURE_ALPHA_TYPE ? GL_ALPHA : GL_DEPTH_COMPONENT;

            *out = bitcountForInternalFormat(tex->internalformat, channel)
                 ? (GLint)mglFormatComponentType(tex->internalformat) : GL_NONE;
            return true;
        }

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }
}

void mglGetTexLevelParameteriv(GLMContext ctx, GLenum target, GLint level, GLenum pname, GLint *params)
{
    GLuint index;
    GLint value = 0;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    index = textureIndexFromTarget(ctx, target);

    ERROR_CHECK_RETURN(index != _MAX_TEXTURE_TYPES, GL_INVALID_ENUM);

    if (getTexLevelParameter(ctx, currentTexture(ctx, index), level, pname, &value))
        *params = value;
}

void mglGetTexLevelParameterfv(GLMContext ctx, GLenum target, GLint level, GLenum pname, GLfloat *params)
{
    GLint value = 0;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    mglGetTexLevelParameteriv(ctx, target, level, pname, &value);

    *params = (GLfloat)value;
}
