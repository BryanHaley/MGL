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
 * samplers.c
 * MGL
 *
 */

#include <strings.h>
#include "glm_context.h"
#include "mgl_log.h"

Sampler *newSampler(GLMContext ctx, GLuint sampler)
{
    Sampler *ptr;

    ptr = (Sampler *)malloc(sizeof(Sampler));

    if (ptr == NULL)
    {
        MGL_ERR("MGL Error: %s: out of memory allocating a Sampler\n", __FUNCTION__);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

    bzero(ptr, sizeof(Sampler));

    ptr->name = sampler;

    // sampler object defaults, OpenGL 4.6 core table 23.18
    ptr->params.wrap_s = GL_REPEAT;
    ptr->params.wrap_t = GL_REPEAT;
    ptr->params.wrap_r = GL_REPEAT;
    ptr->params.min_filter = GL_NEAREST_MIPMAP_LINEAR;
    ptr->params.mag_filter = GL_LINEAR;
    ptr->params.min_lod = -1000.0f;
    ptr->params.max_lod = 1000.0f;
    ptr->params.lod_bias = 0.0f;
    ptr->params.max_anisotropy = 1.0f;
    ptr->params.compare_mode = GL_NONE;
    ptr->params.compare_func = GL_LEQUAL;

    // not sampler state, but the struct is shared with textures
    ptr->params.depth_stencil_mode = GL_DEPTH_COMPONENT;
    ptr->params.base_level = 0;
    ptr->params.max_level = 1000;
    ptr->params.swizzle_r = GL_RED;
    ptr->params.swizzle_g = GL_GREEN;
    ptr->params.swizzle_b = GL_BLUE;
    ptr->params.swizzle_a = GL_ALPHA;

    return ptr;
}

Sampler *getSampler(GLMContext ctx, GLuint sampler)
{
    Sampler *ptr;

    ptr = (Sampler *)searchHashTable(&STATE(sampler_table), sampler);

    if (!ptr)
    {
        ptr = newSampler(ctx, sampler);

        if (ptr)
            insertHashElement(&STATE(sampler_table), sampler, ptr);
    }

    return ptr;
}

bool isSampler(GLMContext ctx, GLuint sampler)
{
    Sampler *ptr;

    ptr = (Sampler *)searchHashTable(&STATE(sampler_table), sampler);

    if (ptr)
        return true;

    return false;
}

Sampler *findSampler(GLMContext ctx, GLuint sampler)
{
    Sampler *ptr;

    ptr = (Sampler *)searchHashTable(&STATE(sampler_table), sampler);

    return ptr;
}

GLboolean mglIsSampler(GLMContext ctx, GLuint sampler)
{
    return isSampler(ctx, sampler);
}

void mglGenSamplers(GLMContext ctx, GLsizei count, GLuint *samplers)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (samplers == NULL)
        return;

    // GenSamplers hands back new objects already holding the default state
    while(count--)
    {
        GLuint name;

        name = getNewName(&ctx->state.sampler_table);

        if (!getSampler(ctx, name))
        {
            MGL_ERR("MGL Error: %s: could not create sampler %u\n", __FUNCTION__, name);
            ERROR_RETURN(GL_OUT_OF_MEMORY);
        }

        *samplers++ = name;
    }
}

void mglBindSampler(GLMContext ctx, GLuint unit, GLuint sampler)
{
    Sampler *ptr;

    // glBindSampler takes a unit index, not a GL_TEXTUREi enum
    if (unit >= STATE_VAR(max_combined_texture_image_units))
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (sampler)
    {
        ptr = findSampler(ctx, sampler);

        ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    }
    else
    {
        ptr = NULL;
    }

    ctx->state.texture_samplers[unit] = ptr;
    ctx->state.dirty_bits  |= DIRTY_SAMPLER;
}

void mglDeleteSamplers(GLMContext ctx, GLsizei count, const GLuint *samplers)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (samplers == NULL)
        return;

    while(count--)
    {
        GLuint sampler;
        Sampler *ptr;

        sampler = *samplers++;

        ptr = findSampler(ctx, sampler);

        if (ptr == NULL)
            continue;

        // remove any references to this sampler
        for(int i=0; i<TEXTURE_UNITS; i++)
        {
            if (ctx->state.texture_samplers[i] == ptr)
            {
                ctx->state.texture_samplers[i] = NULL;
            }
        }

        deleteHashElement(&ctx->state.sampler_table, sampler);

        if (ptr->mtl_data)
        {
            ctx->mtl_funcs.mtlDeleteMTLObj(ctx, ptr->mtl_data);
        }

        free(ptr);
    }
}

void mglCreateSamplers(GLMContext ctx, GLsizei n, GLuint *samplers)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    mglGenSamplers(ctx, n, samplers);
}

void mglBindSamplers(GLMContext ctx, GLuint first, GLsizei count, const GLuint *samplers)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN((GLuint64)first + (GLuint64)count <= TEXTURE_UNITS, GL_INVALID_OPERATION);

    // every name has to be good before anything is bound
    if (samplers)
    {
        for (GLsizei i = 0; i < count; i++)
        {
            if (samplers[i] && !isSampler(ctx, samplers[i]))
            {
                ERROR_RETURN(GL_INVALID_OPERATION);
            }
        }
    }

    for (GLsizei i = 0; i < count; i++)
    {
        // a null array unbinds the whole span
        ctx->state.texture_samplers[first + i] = samplers && samplers[i]
                                               ? findSampler(ctx, samplers[i])
                                               : NULL;
    }

    ctx->state.dirty_bits |= DIRTY_SAMPLER;
}

#pragma mark sampler parameters

// The parameters a sampler object holds, OpenGL 4.6 core table 23.18. Base
// level, swizzle and the rest of glTexParameter's list belong to the texture,
// not the sampler, so they are rejected here.

static bool sampler_value_ok(GLenum pname, GLint value)
{
    switch(pname)
    {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
            switch(value)
            {
                case GL_CLAMP_TO_EDGE:
                case GL_CLAMP_TO_BORDER:
                case GL_MIRRORED_REPEAT:
                case GL_MIRROR_CLAMP_TO_EDGE:
                case GL_REPEAT:
#ifdef MGL_COMPAT_PROFILE
                case GL_CLAMP:
#endif
                    return true;
            }
            return false;

        case GL_TEXTURE_MIN_FILTER:
            switch(value)
            {
                case GL_NEAREST:
                case GL_LINEAR:
                case GL_NEAREST_MIPMAP_NEAREST:
                case GL_LINEAR_MIPMAP_NEAREST:
                case GL_NEAREST_MIPMAP_LINEAR:
                case GL_LINEAR_MIPMAP_LINEAR:
                    return true;
            }
            return false;

        case GL_TEXTURE_MAG_FILTER:
            return (value == GL_NEAREST) || (value == GL_LINEAR);

        case GL_TEXTURE_COMPARE_MODE:
            return (value == GL_NONE) || (value == GL_COMPARE_REF_TO_TEXTURE);

        case GL_TEXTURE_COMPARE_FUNC:
            switch(value)
            {
                case GL_LEQUAL:
                case GL_GEQUAL:
                case GL_LESS:
                case GL_GREATER:
                case GL_EQUAL:
                case GL_NOTEQUAL:
                case GL_ALWAYS:
                case GL_NEVER:
                    return true;
            }
            return false;
    }

    return true;
}

// caller passes the value in both forms; each parameter picks the one it wants
static bool setSamplerScalar(GLMContext ctx, Sampler *ptr, GLenum pname, GLint iv, GLfloat fv)
{
    TextureParameter *params = &ptr->params;

    switch(pname)
    {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
        case GL_TEXTURE_WRAP_R:
        case GL_TEXTURE_MIN_FILTER:
        case GL_TEXTURE_MAG_FILTER:
        case GL_TEXTURE_COMPARE_MODE:
        case GL_TEXTURE_COMPARE_FUNC:
            if (sampler_value_ok(pname, iv) == false)
                ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
            break;

        case GL_TEXTURE_MIN_LOD:
        case GL_TEXTURE_MAX_LOD:
        case GL_TEXTURE_LOD_BIAS:
            break;

        case GL_TEXTURE_MAX_ANISOTROPY:
            if (fv < 1.0f)
                ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
            break;

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }

    switch(pname)
    {
        case GL_TEXTURE_WRAP_S: params->wrap_s = mglNormalizeWrapMode(iv); break;
        case GL_TEXTURE_WRAP_T: params->wrap_t = mglNormalizeWrapMode(iv); break;
        case GL_TEXTURE_WRAP_R: params->wrap_r = mglNormalizeWrapMode(iv); break;
        case GL_TEXTURE_MIN_FILTER: params->min_filter = iv; break;
        case GL_TEXTURE_MAG_FILTER: params->mag_filter = iv; break;
        case GL_TEXTURE_COMPARE_MODE: params->compare_mode = iv; break;
        case GL_TEXTURE_COMPARE_FUNC: params->compare_func = iv; break;
        case GL_TEXTURE_MIN_LOD: params->min_lod = fv; break;
        case GL_TEXTURE_MAX_LOD: params->max_lod = fv; break;
        case GL_TEXTURE_LOD_BIAS: params->lod_bias = fv; break;
        case GL_TEXTURE_MAX_ANISOTROPY: params->max_anisotropy = fv; break;
    }

    ptr->dirty_bits |= DIRTY_SAMPLER_PARAM;

    return true;
}

// the border colour is one piece of state with a float, a signed and an
// unsigned reading; keep all three in step
static void setSamplerBorderColor(Sampler *ptr, const GLfloat *f, const GLint *i, const GLuint *ui)
{
    TextureParameter *params = &ptr->params;

    for (int n = 0; n < 4; n++)
    {
        params->border_color[n]    = f  ? f[n]  : (i ? (GLfloat)i[n] : (GLfloat)ui[n]);
        params->border_color_i[n]  = i  ? i[n]  : (f ? (GLint)f[n]   : (GLint)ui[n]);
        params->border_color_ui[n] = ui ? ui[n] : (f ? (GLuint)f[n]  : (GLuint)i[n]);
    }

    ptr->dirty_bits |= DIRTY_SAMPLER_PARAM;
}

static bool getSamplerScalar(GLMContext ctx, Sampler *ptr, GLenum pname, GLfloat *fv)
{
    TextureParameter *params = &ptr->params;

    switch(pname)
    {
        case GL_TEXTURE_WRAP_S: *fv = params->wrap_s; break;
        case GL_TEXTURE_WRAP_T: *fv = params->wrap_t; break;
        case GL_TEXTURE_WRAP_R: *fv = params->wrap_r; break;
        case GL_TEXTURE_MIN_FILTER: *fv = params->min_filter; break;
        case GL_TEXTURE_MAG_FILTER: *fv = params->mag_filter; break;
        case GL_TEXTURE_COMPARE_MODE: *fv = params->compare_mode; break;
        case GL_TEXTURE_COMPARE_FUNC: *fv = params->compare_func; break;
        case GL_TEXTURE_MIN_LOD: *fv = params->min_lod; break;
        case GL_TEXTURE_MAX_LOD: *fv = params->max_lod; break;
        case GL_TEXTURE_LOD_BIAS: *fv = params->lod_bias; break;
        case GL_TEXTURE_MAX_ANISOTROPY: *fv = params->max_anisotropy; break;

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }

    return true;
}

static GLint roundToInt(GLfloat f)
{
    return (GLint)(f >= 0.0f ? f + 0.5f : f - 0.5f);
}

void mglSamplerParameterf(GLMContext ctx, GLuint sampler, GLenum pname, GLfloat param)
{
    Sampler *ptr;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    // a border colour needs four values
    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    setSamplerScalar(ctx, ptr, pname, (GLint)param, param);
}

void mglSamplerParameteri(GLMContext ctx, GLuint sampler, GLenum pname, GLint param)
{
    Sampler *ptr;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    setSamplerScalar(ctx, ptr, pname, param, (GLfloat)param);
}

void mglSamplerParameterfv(GLMContext ctx, GLuint sampler, GLenum pname, const GLfloat *param)
{
    Sampler *ptr;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        setSamplerBorderColor(ptr, param, NULL, NULL);

        return;
    }

    setSamplerScalar(ctx, ptr, pname, (GLint)*param, *param);
}

void mglSamplerParameteriv(GLMContext ctx, GLuint sampler, GLenum pname, const GLint *param)
{
    Sampler *ptr;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        setSamplerBorderColor(ptr, NULL, param, NULL);

        return;
    }

    setSamplerScalar(ctx, ptr, pname, *param, (GLfloat)*param);
}

void mglSamplerParameterIiv(GLMContext ctx, GLuint sampler, GLenum pname, const GLint *param)
{
    Sampler *ptr;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        setSamplerBorderColor(ptr, NULL, param, NULL);

        return;
    }

    setSamplerScalar(ctx, ptr, pname, *param, (GLfloat)*param);
}

void mglSamplerParameterIuiv(GLMContext ctx, GLuint sampler, GLenum pname, const GLuint *param)
{
    Sampler *ptr;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        setSamplerBorderColor(ptr, NULL, NULL, param);

        return;
    }

    setSamplerScalar(ctx, ptr, pname, (GLint)*param, (GLfloat)*param);
}

void mglGetSamplerParameterfv(GLMContext ctx, GLuint sampler, GLenum pname, GLfloat *params)
{
    Sampler *ptr;
    GLfloat value = 0.0f;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        for (int i = 0; i < 4; i++)
            params[i] = ptr->params.border_color[i];

        return;
    }

    if (getSamplerScalar(ctx, ptr, pname, &value))
        *params = value;
}

void mglGetSamplerParameteriv(GLMContext ctx, GLuint sampler, GLenum pname, GLint *params)
{
    Sampler *ptr;
    GLfloat value = 0.0f;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        for (int i = 0; i < 4; i++)
            params[i] = roundToInt(ptr->params.border_color[i]);

        return;
    }

    if (getSamplerScalar(ctx, ptr, pname, &value))
        *params = roundToInt(value);
}

void mglGetSamplerParameterIiv(GLMContext ctx, GLuint sampler, GLenum pname, GLint *params)
{
    Sampler *ptr;
    GLfloat value = 0.0f;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        for (int i = 0; i < 4; i++)
            params[i] = ptr->params.border_color_i[i];

        return;
    }

    if (getSamplerScalar(ctx, ptr, pname, &value))
        *params = (GLint)value;
}

void mglGetSamplerParameterIuiv(GLMContext ctx, GLuint sampler, GLenum pname, GLuint *params)
{
    Sampler *ptr;
    GLfloat value = 0.0f;

    ptr = findSampler(ctx, sampler);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        for (int i = 0; i < 4; i++)
            params[i] = ptr->params.border_color_ui[i];

        return;
    }

    if (getSamplerScalar(ctx, ptr, pname, &value))
        *params = (GLuint)value;
}
