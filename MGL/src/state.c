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
 * state.c
 * MGL
 *
 */

#include "mgl.h"
#include "glm_context.h"
#include "mgl_log.h"

static bool validBlendFactor(GLenum f);
static bool validBlendEquation(GLenum mode);

#define ENABLE_CAP(_cap_)   ctx->state.caps._cap_ = true; break
#define DISABLE_CAP(_cap_)   ctx->state.caps._cap_ = false; break

// glEnable(CLIP_DISTANCEi) is the same switch as glEnablei(CLIP_DISTANCE0, i);
// the index rides in the enum. Returns false when cap is not a clip distance.
static bool clipDistanceCap(GLMContext ctx, GLenum cap, GLuint *index)
{
    if (cap < GL_CLIP_DISTANCE0 || cap > GL_CLIP_DISTANCE7)
        return false;

    *index = (GLuint)(cap - GL_CLIP_DISTANCE0);
    return true;
}

void mglDisable(GLMContext ctx, GLenum cap)
{
    GLuint cd;

    if (clipDistanceCap(ctx, cap, &cd))
    {
        ctx->state.caps.clip_distances[cd] = false;
        ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
        return;
    }

    switch(cap)
    {
        case GL_BLEND: DISABLE_CAP(blend);
        case GL_LINE_SMOOTH: DISABLE_CAP(line_smooth);
        case GL_POLYGON_SMOOTH: DISABLE_CAP(polygon_smooth);
        case GL_CULL_FACE: DISABLE_CAP(cull_face);
        case GL_DEPTH_TEST: DISABLE_CAP(depth_test);
        case GL_STENCIL_TEST: DISABLE_CAP(stencil_test);
        case GL_DITHER: DISABLE_CAP(dither);
        case GL_SCISSOR_TEST: DISABLE_CAP(scissor_test);
        case GL_COLOR_LOGIC_OP: DISABLE_CAP(color_logic_op);
        case GL_POLYGON_OFFSET_POINT: DISABLE_CAP(polygon_offset_point);
        case GL_POLYGON_OFFSET_LINE: DISABLE_CAP(polygon_offset_line);
        case GL_POLYGON_OFFSET_FILL: DISABLE_CAP(polygon_offset_fill);
        case GL_MULTISAMPLE: DISABLE_CAP(multisample);
        case GL_SAMPLE_ALPHA_TO_COVERAGE: DISABLE_CAP(sample_alpha_to_coverage);
        case GL_SAMPLE_ALPHA_TO_ONE: DISABLE_CAP(sample_alpha_to_one);
        case GL_SAMPLE_COVERAGE: DISABLE_CAP(sample_coverage);
        case GL_RASTERIZER_DISCARD: DISABLE_CAP(rasterizer_discard);
        case GL_FRAMEBUFFER_SRGB: DISABLE_CAP(framebuffer_srgb);
        case GL_PRIMITIVE_RESTART: DISABLE_CAP(primitive_restart);
        case GL_DEPTH_CLAMP: DISABLE_CAP(depth_clamp);
        case GL_TEXTURE_CUBE_MAP_SEAMLESS: DISABLE_CAP(texture_cube_map_seamless);
        case GL_SAMPLE_MASK: DISABLE_CAP(sample_mask);
        case GL_SAMPLE_SHADING: DISABLE_CAP(sample_shading);
        case GL_PRIMITIVE_RESTART_FIXED_INDEX: DISABLE_CAP(primitive_restart_fixed_index);
        case GL_DEBUG_OUTPUT_SYNCHRONOUS: DISABLE_CAP(debug_output_synchronous);
        case GL_DEBUG_OUTPUT: DISABLE_CAP(debug_output);
        case GL_PROGRAM_POINT_SIZE: DISABLE_CAP(program_point_size);
        case GL_TEXTURE_2D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
            // Legacy texture enable/disable - no-op in core profile
            break;
        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_RENDER_STATE | DIRTY_ALPHA_STATE;
}

void mglEnable(GLMContext ctx, GLenum cap)
{
    GLuint cd;

    if (clipDistanceCap(ctx, cap, &cd))
    {
        ctx->state.caps.clip_distances[cd] = true;
        ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
        return;
    }

    switch(cap)
    {
        case GL_BLEND: ENABLE_CAP(blend);
        case GL_LINE_SMOOTH: ENABLE_CAP(line_smooth);
        case GL_POLYGON_SMOOTH: ENABLE_CAP(polygon_smooth);
        case GL_CULL_FACE: ENABLE_CAP(cull_face);
        case GL_DEPTH_TEST: ENABLE_CAP(depth_test);
        case GL_STENCIL_TEST: ENABLE_CAP(stencil_test);
        case GL_DITHER: ENABLE_CAP(dither);
        case GL_SCISSOR_TEST: ENABLE_CAP(scissor_test);
        case GL_COLOR_LOGIC_OP: ENABLE_CAP(color_logic_op);
        case GL_POLYGON_OFFSET_POINT: ENABLE_CAP(polygon_offset_point);
        case GL_POLYGON_OFFSET_LINE: ENABLE_CAP(polygon_offset_line);
        case GL_POLYGON_OFFSET_FILL: ENABLE_CAP(polygon_offset_fill);
        case GL_PROGRAM_POINT_SIZE: ENABLE_CAP(program_point_size);
        case GL_MULTISAMPLE: ENABLE_CAP(multisample);
        case GL_SAMPLE_ALPHA_TO_COVERAGE: ENABLE_CAP(sample_alpha_to_coverage);
        case GL_SAMPLE_ALPHA_TO_ONE: ENABLE_CAP(sample_alpha_to_one);
        case GL_SAMPLE_COVERAGE: ENABLE_CAP(sample_coverage);
        case GL_RASTERIZER_DISCARD: ENABLE_CAP(rasterizer_discard);
        case GL_FRAMEBUFFER_SRGB: ENABLE_CAP(framebuffer_srgb);
        case GL_PRIMITIVE_RESTART: ENABLE_CAP(primitive_restart);
        case GL_DEPTH_CLAMP: ENABLE_CAP(depth_clamp);
        case GL_TEXTURE_CUBE_MAP_SEAMLESS: ENABLE_CAP(texture_cube_map_seamless);
        case GL_SAMPLE_MASK: ENABLE_CAP(sample_mask);
        case GL_SAMPLE_SHADING: ENABLE_CAP(sample_shading);
        case GL_PRIMITIVE_RESTART_FIXED_INDEX: ENABLE_CAP(primitive_restart_fixed_index);
        case GL_DEBUG_OUTPUT_SYNCHRONOUS: ENABLE_CAP(debug_output_synchronous);
        case GL_DEBUG_OUTPUT: ENABLE_CAP(debug_output);
        case GL_TEXTURE_2D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
            // Legacy texture enable/disable - no-op in core profile
            // virglrenderer may call these for compatibility
            break;
        default:
            // the legacy texture enables above are deliberately ignored; anything
            // else really is a bad enum
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_RENDER_STATE | DIRTY_ALPHA_STATE;
}

void mglCullFace(GLMContext ctx, GLenum mode)
{
    switch(mode)
    {
        case GL_FRONT:
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            ctx->state.var.cull_face_mode = mode;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglFrontFace(GLMContext ctx, GLenum mode)
{
    switch(mode)
    {
        case GL_CW:
        case GL_CCW:
            ctx->state.var.front_face = mode;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

#define HINT(_target_) ctx->state.hints._target_ = mode; break;
void mglHint(GLMContext ctx, GLenum target, GLenum mode)
{
    switch(target)
    {
        case GL_LINE_SMOOTH_HINT: HINT(line_smooth_hint);
        case GL_POLYGON_SMOOTH_HINT: HINT(polygon_smooth_hint)
        case GL_TEXTURE_COMPRESSION_HINT: HINT(texture_compression_hint);
        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT: HINT(fragment_shader_derivative_hint);
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_STATE;
}

void mglLineWidth(GLMContext ctx, GLfloat width)
{
    ERROR_CHECK_RETURN(width > 0, GL_INVALID_VALUE);

    ctx->state.var.line_width = width;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglPointSize(GLMContext ctx, GLfloat size)
{
    ERROR_CHECK_RETURN(size > 0, GL_INVALID_VALUE);

    ctx->state.var.point_size = size;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglPolygonMode(GLMContext ctx, GLenum face, GLenum mode)
{
    ERROR_CHECK_RETURN(face == GL_FRONT_AND_BACK, GL_INVALID_VALUE);

    switch(mode)
    {
        case GL_POINT:
        case GL_LINE:
        case GL_FILL:
            ctx->state.var.polygon_mode = mode;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglScissor(GLMContext ctx, GLint x, GLint y, GLsizei width, GLsizei height)
{
    ERROR_CHECK_RETURN(width >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height >= 0, GL_INVALID_VALUE);

    ctx->state.scissor[0].x = x;
    ctx->state.scissor[0].y = y;
    ctx->state.scissor[0].width = width;
    ctx->state.scissor[0].height = height;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglLogicOp(GLMContext ctx, GLenum opcode)
{
    switch(opcode)
    {
        case GL_CLEAR:
        case GL_SET:
        case GL_COPY:
        case GL_COPY_INVERTED:
        case GL_NOOP:
        case GL_AND:
        case GL_NAND:
        case GL_OR:
        case GL_NOR:
        case GL_XOR:
        case GL_EQUIV:
        case GL_AND_REVERSE:
        case GL_AND_INVERTED:
        case GL_OR_REVERSE:
        case GL_OR_INVERTED:
            ctx->state.var.logic_op = opcode;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglStencilFunc(GLMContext ctx, GLenum func, GLint ref, GLuint mask)
{
    switch(func)
    {
        case GL_LEQUAL:
        case GL_GEQUAL:
        case GL_LESS:
        case GL_GREATER:
        case GL_EQUAL:
        case GL_NOTEQUAL:
        case GL_ALWAYS:
        case GL_NEVER:
            ctx->state.var.stencil_func = func;
            ctx->state.var.stencil_back_func = func;
            ctx->state.var.stencil_ref = ref;
            ctx->state.var.stencil_back_ref = ref;
            ctx->state.var.stencil_value_mask = mask;
            ctx->state.var.stencil_back_value_mask = mask;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

static bool validStencilOpSeparate(GLMContext ctx, GLenum op)
{
    switch(op)
    {
        case GL_KEEP:
        case GL_ZERO:
        case GL_REPLACE:
        case GL_INCR:
        case GL_INCR_WRAP:
        case GL_DECR:
        case GL_DECR_WRAP:
        case GL_INVERT:
            return true;
    }

    return false;
}

void mglStencilOp(GLMContext ctx, GLenum fail, GLenum zfail, GLenum zpass)
{
    ERROR_CHECK_RETURN(validStencilOpSeparate(ctx, fail), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validStencilOpSeparate(ctx, zfail), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validStencilOpSeparate(ctx, zpass), GL_INVALID_ENUM);

    ctx->state.var.stencil_fail = fail;
    ctx->state.var.stencil_pass_depth_fail = zfail;
    ctx->state.var.stencil_pass_depth_pass = zpass;
    ctx->state.var.stencil_back_fail = fail;
    ctx->state.var.stencil_back_pass_depth_fail = zfail;
    ctx->state.var.stencil_back_pass_depth_pass = zpass;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}


void mglStencilMask(GLMContext ctx, GLuint mask)
{
    ctx->state.var.stencil_writemask = mask;
    ctx->state.var.stencil_back_writemask = mask;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglColorMask(GLMContext ctx, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    // The flag is just a fast path for "some channel is off". The values have to
    // be written either way, or glGetBooleanv keeps handing back the old mask.
    bool masked = (red == false || green == false || blue == false || alpha == false);

    for(int i=0; i<MAX_COLOR_ATTACHMENTS; i++)
    {
        STATE(caps.use_color_mask[i]) = masked;

        ctx->state.var.color_writemask[i][0] = red;
        ctx->state.var.color_writemask[i][1] = green;
        ctx->state.var.color_writemask[i][2] = blue;
        ctx->state.var.color_writemask[i][3] = alpha;
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE | DIRTY_ALPHA_STATE;
}

void mglDepthMask(GLMContext ctx, GLboolean flag)
{
    ctx->state.var.depth_writemask = flag;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglStencilOpSeparate(GLMContext ctx, GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)
{
    ERROR_CHECK_RETURN(validStencilOpSeparate(ctx, sfail), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validStencilOpSeparate(ctx, dpfail), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validStencilOpSeparate(ctx, dppass), GL_INVALID_ENUM);

    switch(face)
    {
        case GL_FRONT:
            ctx->state.var.stencil_fail = sfail;
            ctx->state.var.stencil_pass_depth_fail = dpfail;
            ctx->state.var.stencil_pass_depth_pass = dppass;
            break;

        case GL_BACK:
            ctx->state.var.stencil_back_fail = sfail;
            ctx->state.var.stencil_back_pass_depth_fail = dpfail;
            ctx->state.var.stencil_back_pass_depth_pass = dppass;
            break;

        case GL_FRONT_AND_BACK:
            ctx->state.var.stencil_fail = sfail;
            ctx->state.var.stencil_pass_depth_fail = dpfail;
            ctx->state.var.stencil_pass_depth_pass = dppass;
            ctx->state.var.stencil_back_fail = sfail;
            ctx->state.var.stencil_back_pass_depth_fail = dpfail;
            ctx->state.var.stencil_back_pass_depth_pass = dppass;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglStencilFuncSeparate(GLMContext ctx, GLenum face, GLenum func, GLint ref, GLuint mask)
{
    switch(func)
    {
        case GL_LEQUAL:
        case GL_GEQUAL:
        case GL_LESS:
        case GL_GREATER:
        case GL_EQUAL:
        case GL_NOTEQUAL:
        case GL_ALWAYS:
        case GL_NEVER:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    switch(face)
    {
        case GL_FRONT:
            ctx->state.var.stencil_func = func;
            ctx->state.var.stencil_ref = ref;
            ctx->state.var.stencil_value_mask = mask;
            break;

        case GL_BACK:
            ctx->state.var.stencil_back_func = func;
            ctx->state.var.stencil_back_ref = ref;
            ctx->state.var.stencil_back_value_mask = mask;
            break;

        case GL_FRONT_AND_BACK:
            ctx->state.var.stencil_func = func;
            ctx->state.var.stencil_ref = ref;
            ctx->state.var.stencil_value_mask = mask;
            ctx->state.var.stencil_back_func = func;
            ctx->state.var.stencil_back_ref = ref;
            ctx->state.var.stencil_back_value_mask = mask;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglStencilMaskSeparate(GLMContext ctx, GLenum face, GLuint mask)
{
    switch(face)
    {
        case GL_FRONT:
            ctx->state.var.stencil_writemask = mask;
            break;

        case GL_BACK:
            ctx->state.var.stencil_back_writemask = mask;
            break;

        case GL_FRONT_AND_BACK:
            ctx->state.var.stencil_writemask = mask;
            ctx->state.var.stencil_back_writemask = mask;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglDepthFunc(GLMContext ctx, GLenum func)
{
    switch(func)
    {
        case GL_LEQUAL:
        case GL_GEQUAL:
        case GL_LESS:
        case GL_GREATER:
        case GL_EQUAL:
        case GL_NOTEQUAL:
        case GL_ALWAYS:
        case GL_NEVER:
            ctx->state.var.depth_func = func;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

static GLdouble _clamp(GLdouble a)
{
    if (a < 0.0)
        a = 0.0;
    else if (a > 1.0)
        a = 1.0;

    return a;
}

void mglDepthRange(GLMContext ctx, GLdouble n, GLdouble f)
{
    n = _clamp(n);
    f = _clamp(f);

    ctx->state.depth_range[0].znear = n;
    ctx->state.depth_range[0].zfar = f;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglViewport(GLMContext ctx, GLint x, GLint y, GLsizei width, GLsizei height)
{
    // only a negative size is an error; zero is legal and the CTS uses it
    ERROR_CHECK_RETURN(width >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height >= 0, GL_INVALID_VALUE);

    ctx->state.viewport[0].x = (GLfloat)x;
    ctx->state.viewport[0].y = (GLfloat)y;
    ctx->state.viewport[0].w = (GLfloat)width;
    ctx->state.viewport[0].h = (GLfloat)height;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

#define RET_VAR(_VAR_, _DEFAULT_)  return (ctx->state.var._VAR_ == _DEFAULT_)
#define RET_CAP(_CAP_)  return ctx->state.caps._CAP_

GLboolean mglIsEnabled(GLMContext ctx, GLenum cap)
{
    GLuint cd;

    if (clipDistanceCap(ctx, cap, &cd))
        return ctx->state.caps.clip_distances[cd] ? GL_TRUE : GL_FALSE;

    switch(cap)
    {
        case GL_BLEND: RET_CAP(blend);
        case GL_COLOR_LOGIC_OP: RET_CAP(color_logic_op);
        case GL_CULL_FACE: RET_CAP(cull_face);
        case GL_DEPTH_CLAMP: RET_CAP(depth_clamp);
        case GL_DEBUG_OUTPUT: RET_CAP(debug_output);
        case GL_DEBUG_OUTPUT_SYNCHRONOUS: RET_CAP(debug_output_synchronous);
        case GL_DEPTH_TEST: RET_CAP(depth_test);
        case GL_DITHER: RET_CAP(dither);
        case GL_FRAMEBUFFER_SRGB: RET_CAP(framebuffer_srgb);
        case GL_LINE_SMOOTH: RET_CAP(line_smooth);
        case GL_MULTISAMPLE: RET_CAP(multisample);
        case GL_POLYGON_SMOOTH: RET_CAP(polygon_smooth);
        case GL_POLYGON_OFFSET_FILL: RET_CAP(polygon_offset_fill);
        case GL_POLYGON_OFFSET_LINE: RET_CAP(polygon_offset_line);
        case GL_POLYGON_OFFSET_POINT: RET_CAP(polygon_offset_point);
        case GL_PROGRAM_POINT_SIZE: RET_CAP(program_point_size);
        case GL_PRIMITIVE_RESTART: RET_CAP(primitive_restart);
        case GL_SAMPLE_ALPHA_TO_COVERAGE: RET_CAP(sample_alpha_to_coverage);
        case GL_SAMPLE_ALPHA_TO_ONE: RET_CAP(sample_alpha_to_one);
        case GL_SAMPLE_COVERAGE: RET_CAP(sample_coverage);
        case GL_SAMPLE_MASK: RET_CAP(sample_mask);
        case GL_SCISSOR_TEST: RET_CAP(scissor_test);
        case GL_STENCIL_TEST: RET_CAP(stencil_test);
        case GL_TEXTURE_CUBE_MAP_SEAMLESS: RET_CAP(texture_cube_map_seamless);

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, GL_FALSE);
    }

    return false;
}

void mglEnablei(GLMContext ctx, GLenum target, GLuint index)
{
    if (target >= GL_CLIP_DISTANCE0 &&
        target <= GL_CLIP_DISTANCE7)
    {
        if (index < MAX_CLIP_DISTANCES)
        {
            ctx->state.caps.clip_distances[index] = true;

            ctx->state.dirty_bits |= DIRTY_RENDER_STATE;

            return;
        }

        ERROR_RETURN(GL_INVALID_VALUE);
    }

    ERROR_RETURN(GL_INVALID_ENUM);
}

void mglDisablei(GLMContext ctx, GLenum target, GLuint index)
{
    if (target >= GL_CLIP_DISTANCE0 &&
        target <= GL_CLIP_DISTANCE7)
    {
        if (index < MAX_CLIP_DISTANCES)
        {
            ctx->state.caps.clip_distances[index] = false;

            ctx->state.dirty_bits |= DIRTY_RENDER_STATE;

            return;
        }

        ERROR_RETURN(GL_INVALID_VALUE);
    }

    ERROR_RETURN(GL_INVALID_ENUM);
}

GLboolean mglIsEnabledi(GLMContext ctx, GLenum target, GLuint index)
{
    if (target >= GL_CLIP_DISTANCE0 &&
        target <= GL_CLIP_DISTANCE7)
    {
        if (index < MAX_CLIP_DISTANCES)
        {
            return ctx->state.caps.clip_distances[index];
        }

        ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
    }

    ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
}

void mglClearDepthf(GLMContext ctx, GLfloat d)
{
    ctx->state.var.depth_clear_value = d;
}

void mglBlendColor(GLMContext ctx, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    ctx->state.var.blend_color[0] = red;
    ctx->state.var.blend_color[1] = green;
    ctx->state.var.blend_color[2] = blue;
    ctx->state.var.blend_color[3] = alpha;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}

void mglBlendEquation(GLMContext ctx, GLenum mode)
{
    switch(mode)
    {
        case GL_FUNC_ADD:
        case GL_FUNC_SUBTRACT:
        case GL_FUNC_REVERSE_SUBTRACT:
        case GL_MIN:
        case GL_MAX:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    for(int i=0; i<MAX_COLOR_ATTACHMENTS; i++)
    {
        ctx->state.var.blend_equation_rgb[i] = mode;
        ctx->state.var.blend_equation_alpha[i] = mode;
    }

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}

void mglBlendEquationi(GLMContext ctx, GLuint buf, GLenum mode)
{
    switch(mode)
    {
        case GL_FUNC_ADD:
        case GL_FUNC_SUBTRACT:
        case GL_FUNC_REVERSE_SUBTRACT:
        case GL_MIN:
        case GL_MAX:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(buf >=0 && buf < MAX_COLOR_ATTACHMENTS, GL_INVALID_VALUE);

    ctx->state.var.blend_equation_rgb[buf] = mode;
    ctx->state.var.blend_equation_alpha[buf] = mode;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}

void mglBlendEquationSeparatei(GLMContext ctx, GLuint buf, GLenum modeRGB, GLenum modeAlpha)
{
    switch(modeRGB)
    {
        case GL_FUNC_ADD:
        case GL_FUNC_SUBTRACT:
        case GL_FUNC_REVERSE_SUBTRACT:
        case GL_MIN:
        case GL_MAX:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    switch(modeAlpha)
    {
        case GL_FUNC_ADD:
        case GL_FUNC_SUBTRACT:
        case GL_FUNC_REVERSE_SUBTRACT:
        case GL_MIN:
        case GL_MAX:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(buf >= 0 && buf < MAX_COLOR_ATTACHMENTS, GL_INVALID_VALUE);

    ctx->state.var.blend_equation_rgb[buf] = modeRGB;
    ctx->state.var.blend_equation_alpha[buf] = modeAlpha;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}

void mglBlendFunc(GLMContext ctx, GLenum sfactor, GLenum dfactor)
{
    ERROR_CHECK_RETURN(validBlendFactor(sfactor), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendFactor(dfactor), GL_INVALID_ENUM);

    for(int i=0; i<MAX_COLOR_ATTACHMENTS; i++)
    {
        ctx->state.var.blend_src_rgb[i] = sfactor;
        ctx->state.var.blend_src_alpha[i] = sfactor;
        ctx->state.var.blend_dst_rgb[i] = dfactor;
        ctx->state.var.blend_dst_alpha[i] = dfactor;
    }

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}

void mglBlendFunci(GLMContext ctx, GLuint buf, GLenum sfactor, GLenum dfactor)
{
    ERROR_CHECK_RETURN(validBlendFactor(sfactor), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendFactor(dfactor), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(buf >=0 && buf < MAX_COLOR_ATTACHMENTS, GL_INVALID_VALUE);

    ctx->state.var.blend_src_rgb[buf] = sfactor;
    ctx->state.var.blend_src_alpha[buf] = sfactor;
    ctx->state.var.blend_dst_rgb[buf] = dfactor;
    ctx->state.var.blend_dst_alpha[buf] = dfactor;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}

static bool validBlendFactor(GLenum f)
{
    switch(f)
    {
        case GL_ZERO:
        case GL_ONE:
        case GL_SRC_COLOR:
        case GL_ONE_MINUS_SRC_COLOR:
        case GL_DST_COLOR:
        case GL_ONE_MINUS_DST_COLOR:
        case GL_SRC_ALPHA:
        case GL_ONE_MINUS_SRC_ALPHA:
        case GL_DST_ALPHA:
        case GL_ONE_MINUS_DST_ALPHA:
        case GL_CONSTANT_COLOR:
        case GL_ONE_MINUS_CONSTANT_COLOR:
        case GL_CONSTANT_ALPHA:
        case GL_ONE_MINUS_CONSTANT_ALPHA:
        case GL_SRC_ALPHA_SATURATE:
        case GL_SRC1_COLOR:
        case GL_ONE_MINUS_SRC1_COLOR:
        case GL_SRC1_ALPHA:
        case GL_ONE_MINUS_SRC1_ALPHA:
            return true;
    }

    return false;
}

static bool validBlendEquation(GLenum mode)
{
    switch(mode)
    {
        case GL_FUNC_ADD:
        case GL_FUNC_SUBTRACT:
        case GL_FUNC_REVERSE_SUBTRACT:
        case GL_MIN:
        case GL_MAX:
            return true;
    }

    return false;
}

void mglBlendFuncSeparatei(GLMContext ctx, GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    ERROR_CHECK_RETURN(validBlendFactor(srcRGB), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendFactor(dstRGB), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendFactor(srcAlpha), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendFactor(dstAlpha), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(buf < MAX_COLOR_ATTACHMENTS, GL_INVALID_VALUE);

    ctx->state.var.blend_src_rgb[buf] = srcRGB;
    ctx->state.var.blend_dst_rgb[buf] = dstRGB;
    ctx->state.var.blend_src_alpha[buf] = srcAlpha;
    ctx->state.var.blend_dst_alpha[buf] = dstAlpha;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}

void mglBlendEquationSeparate(GLMContext ctx, GLenum modeRGB, GLenum modeAlpha)
{
    ERROR_CHECK_RETURN(validBlendEquation(modeRGB), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendEquation(modeAlpha), GL_INVALID_ENUM);

    for(int i=0; i<MAX_COLOR_ATTACHMENTS; i++)
    {
        ctx->state.var.blend_equation_rgb[i] = modeRGB;
        ctx->state.var.blend_equation_alpha[i] = modeAlpha;
    }

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}


void mglGetPointerv(GLMContext ctx, GLenum pname, void **params)
{
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    switch(pname)
    {
        case GL_DEBUG_CALLBACK_FUNCTION:
            *params = (void *)(uintptr_t)ctx->state.debug_callback;
            break;

        case GL_DEBUG_CALLBACK_USER_PARAM:
            *params = (void *)ctx->state.debug_user_param;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglPolygonOffset(GLMContext ctx, GLfloat factor, GLfloat units)
{
    ctx->state.var.polygon_offset_factor = factor;
    ctx->state.var.polygon_offset_units = units;
    ctx->state.var.polygon_offset_clamp = 0.0f;

    ctx->state.dirty_bits |= DIRTY_STATE;
}

void mglPolygonOffsetClamp(GLMContext ctx, GLfloat factor, GLfloat units, GLfloat clamp)
{
    ctx->state.var.polygon_offset_factor = factor;
    ctx->state.var.polygon_offset_units = units;
    ctx->state.var.polygon_offset_clamp = clamp;

    ctx->state.dirty_bits |= DIRTY_STATE;
}

void mglBlendFuncSeparate(GLMContext ctx, GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)
{
    ERROR_CHECK_RETURN(validBlendFactor(sfactorRGB), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendFactor(dfactorRGB), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendFactor(sfactorAlpha), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(validBlendFactor(dfactorAlpha), GL_INVALID_ENUM);

    for(int i=0; i<MAX_COLOR_ATTACHMENTS; i++)
    {
        ctx->state.var.blend_src_rgb[i] = sfactorRGB;
        ctx->state.var.blend_dst_rgb[i] = dfactorRGB;
        ctx->state.var.blend_src_alpha[i] = sfactorAlpha;
        ctx->state.var.blend_dst_alpha[i] = dfactorAlpha;
    }

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_ALPHA_STATE;
}

static void pointParameter(GLMContext ctx, GLenum pname, GLfloat fval, GLint ival, bool is_int)
{
    switch(pname)
    {
        case GL_POINT_FADE_THRESHOLD_SIZE:
        {
            GLfloat v = is_int ? (GLfloat)ival : fval;

            ERROR_CHECK_RETURN(v >= 0.0f, GL_INVALID_VALUE);

            ctx->state.var.point_fade_threshold_size = v;
            break;
        }

        case GL_POINT_SPRITE_COORD_ORIGIN:
        {
            GLenum v = is_int ? (GLenum)ival : (GLenum)fval;

            ERROR_CHECK_RETURN(v == GL_LOWER_LEFT || v == GL_UPPER_LEFT, GL_INVALID_VALUE);

            ctx->state.var.point_sprite_coord_origin = v;
            break;
        }

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_STATE;
}

void mglPointParameterf(GLMContext ctx, GLenum pname, GLfloat param)
{
    pointParameter(ctx, pname, param, 0, false);
}

void mglPointParameterfv(GLMContext ctx, GLenum pname, const GLfloat *params)
{
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    pointParameter(ctx, pname, params[0], 0, false);
}

void mglPointParameteri(GLMContext ctx, GLenum pname, GLint param)
{
    pointParameter(ctx, pname, 0.0f, param, true);
}

void mglPointParameteriv(GLMContext ctx, GLenum pname, const GLint *params)
{
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    pointParameter(ctx, pname, 0.0f, params[0], true);
}



/* ---------- indexed viewport / scissor / depth range ---------- */

// first + count can wrap if first is large, so check them apart
static bool viewportRangeOK(GLMContext ctx, GLuint first, GLsizei count)
{
    ERROR_CHECK_RETURN_VALUE(count >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(first < MAX_VIEWPORTS, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE((GLuint)count <= MAX_VIEWPORTS - first, GL_INVALID_VALUE, false);

    return true;
}

static GLdouble clampDepth(GLdouble d)
{
    if (d < 0.0) return 0.0;
    if (d > 1.0) return 1.0;
    return d;
}

void mglViewportArrayv(GLMContext ctx, GLuint first, GLsizei count, const GLfloat *v)
{
    if (viewportRangeOK(ctx, first, count) == false)
        return;

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    // check the whole set before writing any of it
    for (GLsizei i = 0; i < count; i++)
        ERROR_CHECK_RETURN(v[i * 4 + 2] >= 0.0f && v[i * 4 + 3] >= 0.0f, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < count; i++)
    {
        ctx->state.viewport[first + i].x = v[i * 4 + 0];
        ctx->state.viewport[first + i].y = v[i * 4 + 1];
        ctx->state.viewport[first + i].w = v[i * 4 + 2];
        ctx->state.viewport[first + i].h = v[i * 4 + 3];
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglViewportIndexedf(GLMContext ctx, GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h)
{
    ERROR_CHECK_RETURN(index < MAX_VIEWPORTS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(w >= 0.0f && h >= 0.0f, GL_INVALID_VALUE);

    ctx->state.viewport[index].x = x;
    ctx->state.viewport[index].y = y;
    ctx->state.viewport[index].w = w;
    ctx->state.viewport[index].h = h;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglViewportIndexedfv(GLMContext ctx, GLuint index, const GLfloat *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    mglViewportIndexedf(ctx, index, v[0], v[1], v[2], v[3]);
}

void mglScissorArrayv(GLMContext ctx, GLuint first, GLsizei count, const GLint *v)
{
    if (viewportRangeOK(ctx, first, count) == false)
        return;

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < count; i++)
        ERROR_CHECK_RETURN(v[i * 4 + 2] >= 0 && v[i * 4 + 3] >= 0, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < count; i++)
    {
        ctx->state.scissor[first + i].x = v[i * 4 + 0];
        ctx->state.scissor[first + i].y = v[i * 4 + 1];
        ctx->state.scissor[first + i].width = v[i * 4 + 2];
        ctx->state.scissor[first + i].height = v[i * 4 + 3];
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglScissorIndexed(GLMContext ctx, GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height)
{
    ERROR_CHECK_RETURN(index < MAX_VIEWPORTS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0, GL_INVALID_VALUE);

    ctx->state.scissor[index].x = left;
    ctx->state.scissor[index].y = bottom;
    ctx->state.scissor[index].width = width;
    ctx->state.scissor[index].height = height;

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglScissorIndexedv(GLMContext ctx, GLuint index, const GLint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    mglScissorIndexed(ctx, index, v[0], v[1], v[2], v[3]);
}

void mglDepthRangeArrayv(GLMContext ctx, GLuint first, GLsizei count, const GLdouble *v)
{
    if (viewportRangeOK(ctx, first, count) == false)
        return;

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < count; i++)
    {
        ctx->state.depth_range[first + i].znear = clampDepth(v[i * 2 + 0]);
        ctx->state.depth_range[first + i].zfar  = clampDepth(v[i * 2 + 1]);
    }

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

void mglDepthRangeIndexed(GLMContext ctx, GLuint index, GLdouble n, GLdouble f)
{
    ERROR_CHECK_RETURN(index < MAX_VIEWPORTS, GL_INVALID_VALUE);

    ctx->state.depth_range[index].znear = clampDepth(n);
    ctx->state.depth_range[index].zfar  = clampDepth(f);

    ctx->state.dirty_bits |= DIRTY_RENDER_STATE;
}

/* ---------- tessellation patch parameters ---------- */

// MGL has no tessellation stage yet, but the patch state is plain GL state and
// is legal to set and read back regardless.

void mglPatchParameteri(GLMContext ctx, GLenum pname, GLint value)
{
    ERROR_CHECK_RETURN(pname == GL_PATCH_VERTICES, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(value > 0, GL_INVALID_VALUE);

    ctx->state.var.patch_vertices = value;

    ctx->state.dirty_bits |= DIRTY_STATE;
}

void mglPatchParameterfv(GLMContext ctx, GLenum pname, const GLfloat *values)
{
    ERROR_CHECK_RETURN(values, GL_INVALID_VALUE);

    switch (pname)
    {
        case GL_PATCH_DEFAULT_INNER_LEVEL:
            memcpy(ctx->state.var.patch_default_inner, values, 2 * sizeof(GLfloat));
            break;

        case GL_PATCH_DEFAULT_OUTER_LEVEL:
            memcpy(ctx->state.var.patch_default_outer, values, 4 * sizeof(GLfloat));
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ctx->state.dirty_bits |= DIRTY_STATE;
}

/* ---------- misc state ---------- */

Query *findQuery(GLMContext ctx, GLuint name);

void mglClipControl(GLMContext ctx, GLenum origin, GLenum depth)
{
    ERROR_CHECK_RETURN(origin == GL_LOWER_LEFT || origin == GL_UPPER_LEFT, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(depth == GL_NEGATIVE_ONE_TO_ONE || depth == GL_ZERO_TO_ONE, GL_INVALID_ENUM);

    ctx->state.var.clip_origin = origin;
    ctx->state.var.clip_depth = depth;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_RENDER_STATE;
}

void mglClampColor(GLMContext ctx, GLenum target, GLenum clamp)
{
    ERROR_CHECK_RETURN(target == GL_CLAMP_READ_COLOR, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(clamp == GL_TRUE || clamp == GL_FALSE || clamp == GL_FIXED_ONLY, GL_INVALID_ENUM);

    ctx->state.var.clamp_read_color = clamp;

    ctx->state.dirty_bits |= DIRTY_STATE;
}

void mglPrimitiveRestartIndex(GLMContext ctx, GLuint index)
{
    ctx->state.var.primitive_restart_index = index;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_RENDER_STATE;
}

void mglMinSampleShading(GLMContext ctx, GLfloat value)
{
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    ctx->state.var.min_sample_shading = value;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_RENDER_STATE;
}

void mglSampleMaski(GLMContext ctx, GLuint maskNumber, GLbitfield mask)
{
    // one 32 bit word covers every sample count MGL supports
    ERROR_CHECK_RETURN(maskNumber == 0, GL_INVALID_VALUE);

    ctx->state.var.sample_mask_value = mask;

    ctx->state.dirty_bits |= DIRTY_STATE | DIRTY_RENDER_STATE;
}

void mglBeginConditionalRender(GLMContext ctx, GLuint id, GLenum mode)
{
    Query *q;

    switch (mode)
    {
        case GL_QUERY_WAIT:
        case GL_QUERY_NO_WAIT:
        case GL_QUERY_BY_REGION_WAIT:
        case GL_QUERY_BY_REGION_NO_WAIT:
        case GL_QUERY_WAIT_INVERTED:
        case GL_QUERY_NO_WAIT_INVERTED:
        case GL_QUERY_BY_REGION_WAIT_INVERTED:
        case GL_QUERY_BY_REGION_NO_WAIT_INVERTED:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(ctx->state.var.conditional_render_query == 0, GL_INVALID_OPERATION);

    q = findQuery(ctx, id);

    ERROR_CHECK_RETURN(q, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(q->active == GL_FALSE, GL_INVALID_OPERATION);

    ctx->state.var.conditional_render_query = id;
    ctx->state.var.conditional_render_mode = mode;
}

void mglEndConditionalRender(GLMContext ctx)
{
    ERROR_CHECK_RETURN(ctx->state.var.conditional_render_query != 0, GL_INVALID_OPERATION);

    ctx->state.var.conditional_render_query = 0;
    ctx->state.var.conditional_render_mode = 0;
}

GLenum mglGetGraphicsResetStatus(GLMContext ctx)
{
    // MGL has no robustness context, so it never resets
    return GL_NO_ERROR;
}
