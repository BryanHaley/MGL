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
 * get.c
 * MGL
 *
 */

#include <stdio.h>
#include "glm_context.h"
#include "mgl_log.h"
#include "pixel_utils.h"
#include "mgl_format_table.h"

void mglGetIntegeri_v(GLMContext ctx, GLenum target, GLuint index, GLint *data);

// these cast a void ptr to a type and value
#define RET_BOOL(__value__) *((GLboolean *)data) = (__value__) ? GL_TRUE : GL_FALSE; break;
#define RET_INT(__value__) *((GLint *)data) = (GLint)__value__; break;
#define RET_FLOAT(__value__) *((GLfloat *)data) = (GLfloat)__value__; break;
#define RET_DOUBLE(__value__) *((GLdouble *)data) = (GLdouble)__value__; break;

// same as RET_TYPE_VAR but for a limit we answer with a constant
#define RET_TYPE_CONST(__TYPE__, __VALUE__) \
switch(type) {  \
    case kBool: RET_BOOL(__VALUE__)    \
    case kInt: RET_INT(__VALUE__)    \
    case kFloat: RET_FLOAT(__VALUE__)    \
    case kDouble: RET_DOUBLE(__VALUE__)    \
}

enum {
    kBool, kInt, kFloat, kDouble
};

// set value based on type from ctx->state.var
#define RET_TYPE_VAR(__TYPE__, __VALUE__) \
switch(type) {  \
case kBool: RET_BOOL(ctx->state.var.__VALUE__);   \
    case kInt: RET_INT(ctx->state.var.__VALUE__)    \
    case kFloat: RET_FLOAT(ctx->state.var.__VALUE__)    \
    case kDouble: RET_DOUBLE(ctx->state.var.__VALUE__)    \
}

// set count values based on type
#define RET_TYPE_VAR_COUNT(__TYPE__, __VALUE__, __COUNT__) \
for(int i=0, counts[]={1,4,4,8};i<__COUNT__; data+=counts[__TYPE__], i++) \
    switch(type) {  \
        case kBool: RET_BOOL(ctx->state.var.__VALUE__[i])    \
        case kInt: RET_INT(ctx->state.var.__VALUE__[i])    \
        case kFloat: RET_FLOAT(ctx->state.var.__VALUE__[i])    \
        case kDouble: RET_DOUBLE(ctx->state.var.__VALUE__[i])    \
}

// two fixed numbers, for the ranges GL reports as a pair
#define RET_PAIR(__A__, __B__) \
{ \
    const GLdouble pair_[2] = { (__A__), (__B__) }; \
    for (int i = 0; i < 2; i++) \
        switch (type) { \
            case kBool:   ((GLboolean *)data)[i] = pair_[i] != 0 ? GL_TRUE : GL_FALSE; break; \
            case kInt:    ((GLint *)data)[i] = (GLint)pair_[i]; break; \
            case kFloat:  ((GLfloat *)data)[i] = (GLfloat)pair_[i]; break; \
            case kDouble: ((GLdouble *)data)[i] = pair_[i]; break; \
        } \
}

// set value based on type from ctx->state not ctx->state.var
#define RET_TYPE(__TYPE__, __VALUE__) \
switch(type) {  \
    case kBool: RET_BOOL(ctx->state.__VALUE__)    \
    case kInt: RET_INT(ctx->state.__VALUE__)    \
    case kFloat: RET_FLOAT(ctx->state.__VALUE__)    \
    case kDouble: RET_DOUBLE(ctx->state.__VALUE__)    \
}

// set count values based on type
#define RET_TYPE_COUNT(__TYPE__, __VALUE__, __COUNT__) \
for(int i=0, counts[]={1,4,4,8};i<__COUNT__; data+=counts[__TYPE__], i++) \
    switch(type) {  \
        case kBool: RET_BOOL(ctx->state.__VALUE__[i])    \
        case kInt: RET_INT(ctx->state.__VALUE__[i])    \
        case kFloat: RET_FLOAT(ctx->state.__VALUE__[i])    \
        case kDouble: RET_DOUBLE(ctx->state.__VALUE__[i])    \
}


// which set of indexed buffer binding points a pname reads, or -1 for a pname
// that is not one of them
static int bufferBaseForPname(GLenum pname)
{
    switch(pname)
    {
        case GL_UNIFORM_BUFFER_BINDING:
        case GL_UNIFORM_BUFFER_START:
        case GL_UNIFORM_BUFFER_SIZE:
            return _UNIFORM_BUFFER;

        case GL_SHADER_STORAGE_BUFFER_BINDING:
        case GL_SHADER_STORAGE_BUFFER_START:
        case GL_SHADER_STORAGE_BUFFER_SIZE:
            return _SHADER_STORAGE_BUFFER;

        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
        case GL_ATOMIC_COUNTER_BUFFER_START:
        case GL_ATOMIC_COUNTER_BUFFER_SIZE:
            return _ATOMIC_COUNTER_BUFFER;

        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        case GL_TRANSFORM_FEEDBACK_BUFFER_START:
        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
            return _TRANSFORM_FEEDBACK_BUFFER;
    }

    return -1;
}

// viewport, scissor and depth range are per-index state. Index 0 is what the
// plain glViewport / glScissor / glDepthRange calls write, so the indexed and
// non-indexed getters can share one lookup.
int mglIndexedStateValues(GLMContext ctx, GLenum pname, GLuint index, GLdouble *out)
{
    int buffer_base = bufferBaseForPname(pname);

    if (buffer_base >= 0)
    {
        const BufferBaseTarget *bound;

        if (index >= MAX_BUFFER_BASE_BINDINGS) return -1;

        bound = &ctx->state.buffer_base[buffer_base].buffers[index];

        switch(pname)
        {
            case GL_UNIFORM_BUFFER_START:
            case GL_SHADER_STORAGE_BUFFER_START:
            case GL_ATOMIC_COUNTER_BUFFER_START:
            case GL_TRANSFORM_FEEDBACK_BUFFER_START:
                out[0] = (GLdouble)bound->offset;
                break;

            case GL_UNIFORM_BUFFER_SIZE:
            case GL_SHADER_STORAGE_BUFFER_SIZE:
            case GL_ATOMIC_COUNTER_BUFFER_SIZE:
            case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
                out[0] = (GLdouble)bound->size;
                break;

            default:
                out[0] = (GLdouble)bound->buffer;
                break;
        }

        return 1;
    }

    switch(pname)
    {
        case GL_SAMPLER_BINDING:
            if (index >= TEXTURE_UNITS) return -1;
            out[0] = ctx->state.texture_samplers[index] ?
                     ctx->state.texture_samplers[index]->name : 0;
            return 1;

        case GL_VIEWPORT:
            if (index >= MAX_VIEWPORTS) return -1;
            out[0] = ctx->state.viewport[index].x;
            out[1] = ctx->state.viewport[index].y;
            out[2] = ctx->state.viewport[index].w;
            out[3] = ctx->state.viewport[index].h;
            return 4;

        case GL_SCISSOR_BOX:
            if (index >= MAX_VIEWPORTS) return -1;
            out[0] = ctx->state.scissor[index].x;
            out[1] = ctx->state.scissor[index].y;
            out[2] = ctx->state.scissor[index].width;
            out[3] = ctx->state.scissor[index].height;
            return 4;

        case GL_DEPTH_RANGE:
            if (index >= MAX_VIEWPORTS) return -1;
            out[0] = ctx->state.depth_range[index].znear;
            out[1] = ctx->state.depth_range[index].zfar;
            return 2;

        case GL_VERTEX_BINDING_BUFFER:
        case GL_VERTEX_BINDING_OFFSET:
        case GL_VERTEX_BINDING_STRIDE:
        case GL_VERTEX_BINDING_DIVISOR:
        {
            if (!ctx->state.vao) return -1;
            if (index >= MAX_BINDABLE_BUFFERS) return -1;

            BufferBinding *binding = &ctx->state.vao->bindings[index];

            switch(pname)
            {
                case GL_VERTEX_BINDING_BUFFER:
                    out[0] = binding->buffer ? (GLdouble)binding->buffer->name : 0.0;
                    break;
                case GL_VERTEX_BINDING_OFFSET:  out[0] = (GLdouble)binding->offset;  break;
                case GL_VERTEX_BINDING_STRIDE:  out[0] = (GLdouble)binding->stride;  break;
                default:                        out[0] = (GLdouble)binding->divisor; break;
            }
            return 1;
        }

        case GL_CURRENT_VERTEX_ATTRIB:
            if (index >= ctx->state.max_vertex_attribs) return -1;
            for(int i=0; i<4; i++)
            {
                switch(ctx->state.attrib_constant[index].type)
                {
                    case _ATTRIB_CONST_INT:  out[i] = ctx->state.attrib_constant[index].v.i[i]; break;
                    case _ATTRIB_CONST_UINT: out[i] = ctx->state.attrib_constant[index].v.u[i]; break;
                    default:                 out[i] = ctx->state.attrib_constant[index].v.f[i]; break;
                }
            }
            return 4;
    }

    return 0;
}

void mglWriteTypedValues(void *data, GLuint type, const GLdouble *v, int count)
{
    for(int i=0; i<count; i++)
    {
        switch(type)
        {
            case kBool:   ((GLboolean *)data)[i] = (v[i] != 0.0); break;
            case kInt:    ((GLint *)data)[i]     = (GLint)v[i];   break;
            case kFloat:  ((GLfloat *)data)[i]   = (GLfloat)v[i]; break;
            case kDouble: ((GLdouble *)data)[i]  = v[i];          break;
        }
    }
}

static void mglGet(GLMContext ctx, GLenum pname, GLuint type, void *data)
{
    switch(pname)
    {
        case 0x0B11: RET_TYPE_VAR(type, point_size); break; // GL_POINT_SIZE
        case 0x0B12: RET_PAIR(1.0, 511.0); break; // GL_POINT_SIZE_RANGE, what Metal draws
        case 0x0B13: RET_TYPE_VAR(type, point_size_granularity); break; // GL_POINT_SIZE_GRANULARITY
        case 0x0B21: RET_TYPE_VAR(type, line_width); break; // GL_LINE_WIDTH
        case 0x0B22: RET_PAIR(1.0, 1.0); break; // GL_LINE_WIDTH_RANGE, Metal lines are one pixel
        case 0x0B23: RET_TYPE_VAR(type, line_width_granularity); break; // GL_LINE_WIDTH_GRANULARITY
        case 0x0B40: RET_TYPE_VAR(type, polygon_mode); break; // GL_POLYGON_MODE
        case 0x0B45: RET_TYPE_VAR(type, cull_face_mode); break; // GL_CULL_FACE_MODE
        case 0x0B46: RET_TYPE_VAR(type, front_face); break; // GL_FRONT_FACE

        case 0x0B70: { GLdouble v[4]; int n = mglIndexedStateValues(ctx, GL_DEPTH_RANGE, 0, v); mglWriteTypedValues(data, type, v, n); } break; // GL_DEPTH_RANGE

        case 0x0B72: RET_TYPE_VAR(type, depth_writemask); break; // GL_DEPTH_WRITEMASK
        case 0x0B73: RET_TYPE_VAR(type, depth_clear_value); break; // GL_DEPTH_CLEAR_VALUE
        case 0x0B74: RET_TYPE_VAR(type, depth_func); break; // GL_DEPTH_FUNC
        case 0x0B91: RET_TYPE_VAR(type, stencil_clear_value); break; // GL_STENCIL_CLEAR_VALUE
        case 0x0B92: RET_TYPE_VAR(type, stencil_func); break; // GL_STENCIL_FUNC
        case 0x0B93: RET_TYPE_VAR(type, stencil_value_mask); break; // GL_STENCIL_VALUE_MASK
        case 0x0B94: RET_TYPE_VAR(type, stencil_fail); break; // GL_STENCIL_FAIL
        case 0x0B95: RET_TYPE_VAR(type, stencil_pass_depth_fail); break; // GL_STENCIL_PASS_DEPTH_FAIL
        case 0x0B96: RET_TYPE_VAR(type, stencil_pass_depth_pass); break; // GL_STENCIL_PASS_DEPTH_PASS
        case 0x0B97: RET_TYPE_VAR(type, stencil_ref); break; // GL_STENCIL_REF
        case 0x0B98: RET_TYPE_VAR(type, stencil_writemask); break; // GL_STENCIL_WRITEMASK

        case 0x0BA2: { GLdouble v[4]; int n = mglIndexedStateValues(ctx, GL_VIEWPORT, 0, v); mglWriteTypedValues(data, type, v, n); } break; // GL_VIEWPORT

        case 0x0BE0: RET_TYPE_VAR(type, blend_dst_rgb[0]); break; // GL_BLEND_DST
        case 0x0BE1: RET_TYPE_VAR(type, blend_src_rgb[0]); break; // GL_BLEND_SRC

        case 0x0BF0: RET_TYPE_VAR(type, logic_op_mode); break; // GL_LOGIC_OP_MODE
        case 0x0C01: RET_TYPE(type, draw_buffer); break; // GL_DRAW_BUFFER
        case 0x0C02: RET_TYPE(type, read_buffer); break; // GL_READ_BUFFER

        case 0x0C10: { GLdouble v[4]; int n = mglIndexedStateValues(ctx, GL_SCISSOR_BOX, 0, v); mglWriteTypedValues(data, type, v, n); } break; // GL_SCISSOR_BOX

        case 0x0C22: RET_TYPE_COUNT(type, color_clear_value, 4); break; // GL_COLOR_CLEAR_VALUE

        case 0x0C23: RET_TYPE_VAR_COUNT(type, color_writemask[0], 4); break; // GL_COLOR_WRITEMASK

        case 0x0D33: RET_TYPE_VAR(type, max_texture_size); break; // GL_MAX_TEXTURE_SIZE
        // GL_MAX_VIEWPORT_DIMS is two values, not one; the second was left at 0
        case 0x0D3A:
        {
            GLuint d = ctx->state.var.max_viewport_dims ? ctx->state.var.max_viewport_dims : 16384;

            for (int i = 0; i < 2; i++)
            {
                switch(type) {
                    case kBool:   *((GLboolean *)data) = d ? GL_TRUE : GL_FALSE; break;
                    case kInt:    *((GLint *)data)     = (GLint)d;    break;
                    case kFloat:  *((GLfloat *)data)   = (GLfloat)d;  break;
                    case kDouble: *((GLdouble *)data)  = (GLdouble)d; break;
                }

                data += (type == kBool) ? 1 : (type == kDouble ? 8 : 4);
            }

            break;
        }
        case 0x0D50: RET_TYPE_VAR(type, subpixel_bits); break; // GL_SUBPIXEL_BITS
        case 0x0B00: RET_TYPE_VAR(type, current_color); break; // GL_CURRENT_COLOR
        case 0x0B01: RET_TYPE_VAR(type, current_index); break; // GL_CURRENT_INDEX
        case 0x0B02: RET_TYPE_VAR(type, current_normal); break; // GL_CURRENT_NORMAL
        case 0x0B04: RET_TYPE_VAR(type, current_raster_color); break; // GL_CURRENT_RASTER_COLOR
        case 0x0B05: RET_TYPE_VAR(type, current_raster_index); break; // GL_CURRENT_RASTER_INDEX
        case 0x0B06: RET_TYPE_VAR(type, current_raster_texture_coords); break; // GL_CURRENT_RASTER_TEXTURE_COORDS
        case 0x0B07: RET_TYPE_VAR(type, current_raster_position); break; // GL_CURRENT_RASTER_POSITION
        case 0x0B08: RET_TYPE_VAR(type, current_raster_position_valid); break; // GL_CURRENT_RASTER_POSITION_VALID
        case 0x0B09: RET_TYPE_VAR(type, current_raster_distance); break; // GL_CURRENT_RASTER_DISTANCE
        case 0x0B25: RET_TYPE_VAR(type, line_stipple_pattern); break; // GL_LINE_STIPPLE_PATTERN
        case 0x0B26: RET_TYPE_VAR(type, line_stipple_repeat); break; // GL_LINE_STIPPLE_REPEAT
        case 0x0B30: RET_TYPE_VAR(type, list_mode); break; // GL_LIST_MODE
        case 0x0B31: RET_TYPE_VAR(type, max_list_nesting); break; // GL_MAX_LIST_NESTING
        case 0x0B32: RET_TYPE_VAR(type, list_base); break; // GL_LIST_BASE
        case 0x0B33: RET_TYPE_VAR(type, list_index); break; // GL_LIST_INDEX
        case 0x0B43: RET_TYPE_VAR(type, edge_flag); break; // GL_EDGE_FLAG
        case 0x0B54: RET_TYPE_VAR(type, shade_model); break; // GL_SHADE_MODEL
        case 0x0B55: RET_TYPE_VAR(type, color_material_face); break; // GL_COLOR_MATERIAL_FACE
        case 0x0B56: RET_TYPE_VAR(type, color_material_parameter); break; // GL_COLOR_MATERIAL_PARAMETER
        case 0x0B80: RET_TYPE_VAR(type, accum_clear_value); break; // GL_ACCUM_CLEAR_VALUE
        case 0x0BA0: RET_TYPE_VAR(type, matrix_mode); break; // GL_MATRIX_MODE
        case 0x0BA3: RET_TYPE_VAR(type, modelview_stack_depth); break; // GL_MODELVIEW_STACK_DEPTH
        case 0x0BA4: RET_TYPE_VAR(type, projection_stack_depth); break; // GL_PROJECTION_STACK_DEPTH
        case 0x0BA5: RET_TYPE_VAR(type, texture_stack_depth); break; // GL_TEXTURE_STACK_DEPTH
        case 0x0BA6: RET_TYPE_VAR(type, modelview_matrix); break; // GL_MODELVIEW_MATRIX
        case 0x0BA7: RET_TYPE_VAR(type, projection_matrix); break; // GL_PROJECTION_MATRIX
        case 0x0BB0: RET_TYPE_VAR(type, attrib_stack_depth); break; // GL_ATTRIB_STACK_DEPTH
        case 0x0BC1: RET_TYPE_VAR(type, alpha_test_func); break; // GL_ALPHA_TEST_FUNC
        case 0x0BC2: RET_TYPE_VAR(type, alpha_test_ref); break; // GL_ALPHA_TEST_REF
        case 0x0BF1: RET_TYPE_VAR(type, logic_op); break; // GL_LOGIC_OP
        case 0x0C00: RET_TYPE_VAR(type, aux_buffers); break; // GL_AUX_BUFFERS
        case 0x0C20: RET_TYPE_VAR(type, index_clear_value); break; // GL_INDEX_CLEAR_VALUE
        case 0x0C21: RET_TYPE_VAR(type, index_writemask); break; // GL_INDEX_WRITEMASK
        case 0x0C30: RET_TYPE_VAR(type, index_mode); break; // GL_INDEX_MODE
        case 0x0C31: RET_TYPE_VAR(type, rgba_mode); break; // GL_RGBA_MODE
        case 0x0C40: RET_TYPE_VAR(type, render_mode); break; // GL_RENDER_MODE
        case 0x0CB0: RET_TYPE_VAR(type, pixel_map_i_to_i_size); break; // GL_PIXEL_MAP_I_TO_I_SIZE
        case 0x0CB1: RET_TYPE_VAR(type, pixel_map_s_to_s_size); break; // GL_PIXEL_MAP_S_TO_S_SIZE
        case 0x0CB2: RET_TYPE_VAR(type, pixel_map_i_to_r_size); break; // GL_PIXEL_MAP_I_TO_R_SIZE
        case 0x0CB3: RET_TYPE_VAR(type, pixel_map_i_to_g_size); break; // GL_PIXEL_MAP_I_TO_G_SIZE
        case 0x0CB4: RET_TYPE_VAR(type, pixel_map_i_to_b_size); break; // GL_PIXEL_MAP_I_TO_B_SIZE
        case 0x0CB5: RET_TYPE_VAR(type, pixel_map_i_to_a_size); break; // GL_PIXEL_MAP_I_TO_A_SIZE
        case 0x0CB6: RET_TYPE_VAR(type, pixel_map_r_to_r_size); break; // GL_PIXEL_MAP_R_TO_R_SIZE
        case 0x0CB7: RET_TYPE_VAR(type, pixel_map_g_to_g_size); break; // GL_PIXEL_MAP_G_TO_G_SIZE
        case 0x0CB8: RET_TYPE_VAR(type, pixel_map_b_to_b_size); break; // GL_PIXEL_MAP_B_TO_B_SIZE
        case 0x0CB9: RET_TYPE_VAR(type, pixel_map_a_to_a_size); break; // GL_PIXEL_MAP_A_TO_A_SIZE
        case 0x0D16: RET_TYPE_VAR(type, zoom_x); break; // GL_ZOOM_X
        case 0x0D17: RET_TYPE_VAR(type, zoom_y); break; // GL_ZOOM_Y
        case 0x0D30: RET_TYPE_VAR(type, max_eval_order); break; // GL_MAX_EVAL_ORDER
        case 0x0D31: RET_TYPE_VAR(type, max_lights); break; // GL_MAX_LIGHTS
        case 0x0D32: RET_TYPE_VAR(type, max_clip_planes); break; // GL_MAX_CLIP_PLANES
        case 0x0D34: RET_TYPE_VAR(type, max_pixel_map_table); break; // GL_MAX_PIXEL_MAP_TABLE
        case 0x0D35: RET_TYPE_VAR(type, max_attrib_stack_depth); break; // GL_MAX_ATTRIB_STACK_DEPTH
        case 0x0D36: RET_TYPE_VAR(type, max_modelview_stack_depth); break; // GL_MAX_MODELVIEW_STACK_DEPTH
        case 0x0D37: RET_TYPE_VAR(type, max_name_stack_depth); break; // GL_MAX_NAME_STACK_DEPTH
        case 0x0D38: RET_TYPE_VAR(type, max_projection_stack_depth); break; // GL_MAX_PROJECTION_STACK_DEPTH
        case 0x0D39: RET_TYPE_VAR(type, max_texture_stack_depth); break; // GL_MAX_TEXTURE_STACK_DEPTH
        case 0x0D51: RET_TYPE_VAR(type, index_bits); break; // GL_INDEX_BITS
        case 0x0D52: RET_TYPE_VAR(type, red_bits); break; // GL_RED_BITS
        case 0x0D53: RET_TYPE_VAR(type, green_bits); break; // GL_GREEN_BITS
        case 0x0D54: RET_TYPE_VAR(type, blue_bits); break; // GL_BLUE_BITS
        case 0x0D55: RET_TYPE_VAR(type, alpha_bits); break; // GL_ALPHA_BITS
        case 0x0D56: RET_TYPE_VAR(type, depth_bits); break; // GL_DEPTH_BITS
        case 0x0D57: RET_TYPE_VAR(type, stencil_bits); break; // GL_STENCIL_BITS
        case 0x0D58: RET_TYPE_VAR(type, accum_red_bits); break; // GL_ACCUM_RED_BITS
        case 0x0D59: RET_TYPE_VAR(type, accum_green_bits); break; // GL_ACCUM_GREEN_BITS
        case 0x0D5A: RET_TYPE_VAR(type, accum_blue_bits); break; // GL_ACCUM_BLUE_BITS
        case 0x0D5B: RET_TYPE_VAR(type, accum_alpha_bits); break; // GL_ACCUM_ALPHA_BITS
        case 0x0D70: RET_TYPE_VAR(type, name_stack_depth); break; // GL_NAME_STACK_DEPTH
        case 0x0DD0: RET_TYPE_VAR(type, map1_grid_domain); break; // GL_MAP1_GRID_DOMAIN
        case 0x0DD1: RET_TYPE_VAR(type, map1_grid_segments); break; // GL_MAP1_GRID_SEGMENTS
        case 0x0DD2: RET_TYPE_VAR(type, map2_grid_domain); break; // GL_MAP2_GRID_DOMAIN
        case 0x0DD3: RET_TYPE_VAR(type, map2_grid_segments); break; // GL_MAP2_GRID_SEGMENTS
        case 0x2A00: RET_TYPE_VAR(type, polygon_offset_units); break; // GL_POLYGON_OFFSET_UNITS
        case 0x8038: RET_TYPE_VAR(type, polygon_offset_factor); break; // GL_POLYGON_OFFSET_FACTOR
        case 0x8068: RET_TYPE_VAR(type, texture_binding_1d); break; // GL_TEXTURE_BINDING_1D
        case 0x8069: RET_TYPE_VAR(type, texture_binding_2d); break; // GL_TEXTURE_BINDING_2D
        case 0x0BB1: RET_TYPE_VAR(type, client_attrib_stack_depth); break; // GL_CLIENT_ATTRIB_STACK_DEPTH
        case 0x0D3B: RET_TYPE_VAR(type, max_client_attrib_stack_depth); break; // GL_MAX_CLIENT_ATTRIB_STACK_DEPTH
        case 0x0DF1: RET_TYPE_VAR(type, feedback_buffer_size); break; // GL_FEEDBACK_BUFFER_SIZE
        case 0x0DF2: RET_TYPE_VAR(type, feedback_buffer_type); break; // GL_FEEDBACK_BUFFER_TYPE
        case 0x0DF4: RET_TYPE_VAR(type, selection_buffer_size); break; // GL_SELECTION_BUFFER_SIZE
        case 0x807A: RET_TYPE_VAR(type, vertex_array_size); break; // GL_VERTEX_ARRAY_SIZE
        case 0x807B: RET_TYPE_VAR(type, vertex_array_type); break; // GL_VERTEX_ARRAY_TYPE
        case 0x807C: RET_TYPE_VAR(type, vertex_array_stride); break; // GL_VERTEX_ARRAY_STRIDE
        case 0x807E: RET_TYPE_VAR(type, normal_array_type); break; // GL_NORMAL_ARRAY_TYPE
        case 0x807F: RET_TYPE_VAR(type, normal_array_stride); break; // GL_NORMAL_ARRAY_STRIDE
        case 0x8081: RET_TYPE_VAR(type, color_array_size); break; // GL_COLOR_ARRAY_SIZE
        case 0x8082: RET_TYPE_VAR(type, color_array_type); break; // GL_COLOR_ARRAY_TYPE
        case 0x8083: RET_TYPE_VAR(type, color_array_stride); break; // GL_COLOR_ARRAY_STRIDE
        case 0x8085: RET_TYPE_VAR(type, index_array_type); break; // GL_INDEX_ARRAY_TYPE
        case 0x8086: RET_TYPE_VAR(type, index_array_stride); break; // GL_INDEX_ARRAY_STRIDE
        case 0x8088: RET_TYPE_VAR(type, texture_coord_array_size); break; // GL_TEXTURE_COORD_ARRAY_SIZE
        case 0x8089: RET_TYPE_VAR(type, texture_coord_array_type); break; // GL_TEXTURE_COORD_ARRAY_TYPE
        case 0x808A: RET_TYPE_VAR(type, texture_coord_array_stride); break; // GL_TEXTURE_COORD_ARRAY_STRIDE
        case 0x808C: RET_TYPE_VAR(type, edge_flag_array_stride); break; // GL_EDGE_FLAG_ARRAY_STRIDE
        case 0x806A: RET_TYPE_VAR(type, texture_binding_3d); break; // GL_TEXTURE_BINDING_3D
        case 0x8073: RET_TYPE_VAR(type, max_3d_texture_size); break; // GL_MAX_3D_TEXTURE_SIZE
        case 0x80E8: RET_TYPE_VAR(type, max_elements_vertices); break; // GL_MAX_ELEMENTS_VERTICES
        case 0x80E9: RET_TYPE_VAR(type, max_elements_indices); break; // GL_MAX_ELEMENTS_INDICES
        case 0x846E: RET_PAIR(1.0, 1.0); break; // GL_ALIASED_LINE_WIDTH_RANGE
        case 0x846D: RET_PAIR(1.0, 511.0); break; // GL_ALIASED_POINT_SIZE_RANGE
        // Metal restarts strips, never patches
        case 0x8221:  // GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED
            switch (type)
            {
                case kBool: RET_BOOL(0)
                case kInt: RET_INT(0)
                case kFloat: RET_FLOAT(0)
                case kDouble: RET_DOUBLE(0)
            }
            break;
        // GL_ACTIVE_TEXTURE reads back as the GL_TEXTUREi enum, not the unit
        // index we store. Returning the index made every save/restore pair --
        // imgui does one per frame -- feed 0 back to glActiveTexture.
        case 0x84E0: {
            GLuint active_texture_enum = GL_TEXTURE0 + ctx->state.active_texture;
            switch(type) {
                case kBool: RET_BOOL(active_texture_enum)
                case kInt: RET_INT(active_texture_enum)
                case kFloat: RET_FLOAT(active_texture_enum)
                case kDouble: RET_DOUBLE(active_texture_enum)
            }
        } break;
        // GL_SAMPLES / GL_SAMPLE_BUFFERS describe the framebuffer being drawn
        // to, so they are read off its attachments rather than stored.
        case 0x80A9: { // GL_SAMPLES
            GLsizei n = mglDrawFramebufferSamples(ctx);
            switch(type) {
                case kBool: RET_BOOL(n)
                case kInt: RET_INT(n)
                case kFloat: RET_FLOAT(n)
                case kDouble: RET_DOUBLE(n)
            }
        } break;
        case 0x80A8: { // GL_SAMPLE_BUFFERS
            GLint n = mglDrawFramebufferSamples(ctx) > 0 ? 1 : 0;
            switch(type) {
                case kBool: RET_BOOL(n)
                case kInt: RET_INT(n)
                case kFloat: RET_FLOAT(n)
                case kDouble: RET_DOUBLE(n)
            }
        } break;
        case 0x80AA: RET_TYPE_VAR(type, sample_coverage_value); break; // GL_SAMPLE_COVERAGE_VALUE
        case 0x80AB: RET_TYPE_VAR(type, sample_coverage_invert); break; // GL_SAMPLE_COVERAGE_INVERT
        case 0x8514: RET_TYPE_VAR(type, texture_binding_cube_map); break; // GL_TEXTURE_BINDING_CUBE_MAP
        case 0x851C: RET_TYPE_VAR(type, max_cube_map_texture_size); break; // GL_MAX_CUBE_MAP_TEXTURE_SIZE
        // the format table knows which compressed formats Metal takes here
        case 0x86A2: { // GL_NUM_COMPRESSED_TEXTURE_FORMATS
            GLsizei n = mglFormatCompressedFormatList(NULL, 0);
            switch(type) {
                case kBool: RET_BOOL(n)
                case kInt: RET_INT(n)
                case kFloat: RET_FLOAT(n)
                case kDouble: RET_DOUBLE(n)
            }
        } break;
        case 0x86A3: { // GL_COMPRESSED_TEXTURE_FORMATS
            GLenum list[128];
            GLsizei n = mglFormatCompressedFormatList(list, 128);
            for(int i=0, counts[]={1,4,4,8}; i<n; data+=counts[type], i++)
                switch(type) {
                    case kBool: RET_BOOL(list[i])
                    case kInt: RET_INT(list[i])
                    case kFloat: RET_FLOAT(list[i])
                    case kDouble: RET_DOUBLE(list[i])
                }
        } break;

        case 0x80C8: RET_TYPE_VAR(type, blend_dst_rgb[0]); break; // GL_BLEND_DST_RGB
        case 0x80C9: RET_TYPE_VAR(type, blend_src_rgb[0]); break; // GL_BLEND_SRC_RGB
        case 0x80CA: RET_TYPE_VAR(type, blend_dst_alpha[0]); break; // GL_BLEND_DST_ALPHA
        case 0x80CB: RET_TYPE_VAR(type, blend_src_alpha[0]); break; // GL_BLEND_SRC_ALPHA

        case 0x84FD: RET_TYPE_VAR(type, max_texture_lod_bias); break; // GL_MAX_TEXTURE_LOD_BIAS

        case 0x8005: RET_TYPE_VAR_COUNT(type, blend_color,4); break; // GL_BLEND_COLOR

        case 0x8894: RET_TYPE_VAR(type, array_buffer_binding); break; // GL_ARRAY_BUFFER_BINDING
        case 0x8895: RET_TYPE_VAR(type, element_array_buffer_binding); break; // GL_ELEMENT_ARRAY_BUFFER_BINDING
        case 0x8009: RET_TYPE_VAR(type, blend_equation_rgb[0]); break; // GL_BLEND_EQUATION_RGB
        case 0x8800: RET_TYPE_VAR(type, stencil_back_func); break; // GL_STENCIL_BACK_FUNC
        case 0x8801: RET_TYPE_VAR(type, stencil_back_fail); break; // GL_STENCIL_BACK_FAIL
        case 0x8802: RET_TYPE_VAR(type, stencil_back_pass_depth_fail); break; // GL_STENCIL_BACK_PASS_DEPTH_FAIL
        case 0x8803: RET_TYPE_VAR(type, stencil_back_pass_depth_pass); break; // GL_STENCIL_BACK_PASS_DEPTH_PASS
        case 0x8824: RET_TYPE_VAR(type, max_draw_buffers); break; // GL_MAX_DRAW_BUFFERS

        case 0x883D: RET_TYPE_VAR(type, blend_equation_alpha[0]); break; // GL_BLEND_EQUATION_ALPHA

        case 0x8869: RET_TYPE(type, max_vertex_attribs); break; // GL_MAX_VERTEX_ATTRIBS
        case 0x8872: RET_TYPE_VAR(type, max_texture_image_units); break; // GL_MAX_TEXTURE_IMAGE_UNITS
        case 0x8B49: RET_TYPE_VAR(type, max_fragment_uniform_components); break; // GL_MAX_FRAGMENT_UNIFORM_COMPONENTS
        case 0x8B4A: RET_TYPE_VAR(type, max_vertex_uniform_components); break; // GL_MAX_VERTEX_UNIFORM_COMPONENTS
        // GL_MAX_VARYING_FLOATS and GL_MAX_VARYING_COMPONENTS are the same
        // enum and count the same thing, four per vector. The stored value was
        // read back from MGL itself at startup and so was always zero.
        case 0x8B4B: RET_TYPE_VAR(type, max_varying_components); break; // GL_MAX_VARYING_FLOATS / GL_MAX_VARYING_COMPONENTS
        case 0x8B4C: RET_TYPE_VAR(type, max_vertex_texture_image_units); break; // GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS
        case 0x8B4D: RET_TYPE_VAR(type, max_combined_texture_image_units); break; // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
        case 0x8B8D: RET_TYPE_VAR(type, current_program); break; // GL_CURRENT_PROGRAM
        case 0x8CA3: RET_TYPE_VAR(type, stencil_back_ref); break; // GL_STENCIL_BACK_REF
        case 0x8CA4: RET_TYPE_VAR(type, stencil_back_value_mask); break; // GL_STENCIL_BACK_VALUE_MASK
        case 0x8CA5: RET_TYPE_VAR(type, stencil_back_writemask); break; // GL_STENCIL_BACK_WRITEMASK
        case 0x88ED: RET_TYPE_VAR(type, pixel_pack_buffer_binding); break; // GL_PIXEL_PACK_BUFFER_BINDING
        case 0x88EF: RET_TYPE_VAR(type, pixel_unpack_buffer_binding); break; // GL_PIXEL_UNPACK_BUFFER_BINDING
        // Framebuffer-dependent state, so it has to be worked out rather than
        // stored: what ReadPixels will take for whatever is being read now.
        case 0x8B9B: // GL_IMPLEMENTATION_COLOR_READ_FORMAT
        case 0x8B9A: // GL_IMPLEMENTATION_COLOR_READ_TYPE
        {
            GLenum rf, rt;

            mglReadColorFormatAndType(ctx, &rf, &rt);
            RET_TYPE_CONST(type, (pname == 0x8B9B) ? rf : rt);
            break;
        }

        case 0x821B: RET_TYPE_VAR(type, major_version); break; // GL_MAJOR_VERSION
        case 0x821C: RET_TYPE_VAR(type, minor_version); break; // GL_MINOR_VERSION
        case 0x821D: RET_TYPE_VAR(type, num_extensions); break; // GL_NUM_EXTENSIONS
        case 0x821E: RET_TYPE_VAR(type, context_flags); break; // GL_CONTEXT_FLAGS
        case 0x88FF: RET_TYPE_VAR(type, max_array_texture_layers); break; // GL_MAX_ARRAY_TEXTURE_LAYERS
        case 0x8904: RET_TYPE_VAR(type, min_program_texel_offset); break; // GL_MIN_PROGRAM_TEXEL_OFFSET
        case 0x8905: RET_TYPE_VAR(type, max_program_texel_offset); break; // GL_MAX_PROGRAM_TEXEL_OFFSET
        case 0x8C1C: RET_TYPE_VAR(type, texture_binding_1d_array); break; // GL_TEXTURE_BINDING_1D_ARRAY
        case 0x8C1D: RET_TYPE_VAR(type, texture_binding_2d_array); break; // GL_TEXTURE_BINDING_2D_ARRAY
        case 0x84E8: RET_TYPE_VAR(type, max_renderbuffer_size); break; // GL_MAX_RENDERBUFFER_SIZE
        case 0x8CA6: RET_TYPE_VAR(type, draw_framebuffer_binding); break; // GL_DRAW_FRAMEBUFFER_BINDING
        case 0x8CA7: RET_TYPE_VAR(type, renderbuffer_binding); break; // GL_RENDERBUFFER_BINDING
        case 0x8CAA: RET_TYPE_VAR(type, read_framebuffer_binding); break; // GL_READ_FRAMEBUFFER_BINDING
        case 0x85B5: RET_TYPE_VAR(type, vertex_array_binding); break; // GL_VERTEX_ARRAY_BINDING
        case 0x8C2B: RET_TYPE_VAR(type, max_texture_buffer_size); break; // GL_MAX_TEXTURE_BUFFER_SIZE
        case 0x8C2C: RET_TYPE_VAR(type, texture_binding_buffer); break; // GL_TEXTURE_BINDING_BUFFER
        case 0x84F6: RET_TYPE_VAR(type, texture_binding_rectangle); break; // GL_TEXTURE_BINDING_RECTANGLE
        case 0x84F8: RET_TYPE_VAR(type, max_rectangle_texture_size); break; // GL_MAX_RECTANGLE_TEXTURE_SIZE
        case 0x8F9E: RET_TYPE_VAR(type, primitive_restart_index); break; // GL_PRIMITIVE_RESTART_INDEX
        case 0x8A28: RET_TYPE_VAR(type, uniform_buffer_binding); break; // GL_UNIFORM_BUFFER_BINDING
        case 0x8A29: RET_TYPE_VAR(type, uniform_buffer_start); break; // GL_UNIFORM_BUFFER_START
        case 0x8A2A: RET_TYPE_VAR(type, uniform_buffer_size); break; // GL_UNIFORM_BUFFER_SIZE
        case 0x8A2B: RET_TYPE_VAR(type, max_vertex_uniform_blocks); break; // GL_MAX_VERTEX_UNIFORM_BLOCKS
        case 0x8A2C: RET_TYPE_VAR(type, max_geometry_uniform_blocks); break; // GL_MAX_GEOMETRY_UNIFORM_BLOCKS
        case 0x8A2D: RET_TYPE_VAR(type, max_fragment_uniform_blocks); break; // GL_MAX_FRAGMENT_UNIFORM_BLOCKS
        case 0x8A2E: RET_TYPE_VAR(type, max_combined_uniform_blocks); break; // GL_MAX_COMBINED_UNIFORM_BLOCKS
        case 0x8A2F: RET_TYPE_VAR(type, max_uniform_buffer_bindings); break; // GL_MAX_UNIFORM_BUFFER_BINDINGS
        case 0x8A30: RET_TYPE_VAR(type, max_uniform_block_size); break; // GL_MAX_UNIFORM_BLOCK_SIZE
        case 0x8A31: RET_TYPE_VAR(type, max_combined_vertex_uniform_components); break; // GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS
        case 0x8A32: RET_TYPE_VAR(type, max_combined_geometry_uniform_components); break; // GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS
        case 0x8A33: RET_TYPE_VAR(type, max_combined_fragment_uniform_components); break; // GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS
        case 0x8A34: RET_TYPE_VAR(type, uniform_buffer_offset_alignment); break; // GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT
        case 0x8C29: RET_TYPE_VAR(type, max_geometry_texture_image_units); break; // GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS
        case 0x8DDF: RET_TYPE_VAR(type, max_geometry_uniform_components); break; // GL_MAX_GEOMETRY_UNIFORM_COMPONENTS
        case 0x9122: RET_TYPE_VAR(type, max_vertex_output_components); break; // GL_MAX_VERTEX_OUTPUT_COMPONENTS
        case 0x9123: RET_TYPE_VAR(type, max_geometry_input_components); break; // GL_MAX_GEOMETRY_INPUT_COMPONENTS
        case 0x9124: RET_TYPE_VAR(type, max_geometry_output_components); break; // GL_MAX_GEOMETRY_OUTPUT_COMPONENTS
        case 0x9125: RET_TYPE_VAR(type, max_fragment_input_components); break; // GL_MAX_FRAGMENT_INPUT_COMPONENTS
        case 0x9126: RET_TYPE_VAR(type, context_profile_mask); break; // GL_CONTEXT_PROFILE_MASK
        case 0x8E4F: RET_TYPE_VAR(type, provoking_vertex); break; // GL_PROVOKING_VERTEX
        case 0x9111: RET_TYPE_VAR(type, max_server_wait_timeout); break; // GL_MAX_SERVER_WAIT_TIMEOUT
        case 0x8E59: RET_TYPE_VAR(type, max_sample_mask_words); break; // GL_MAX_SAMPLE_MASK_WORDS
        case 0x9104: RET_TYPE_VAR(type, texture_binding_2d_multisample); break; // GL_TEXTURE_BINDING_2D_MULTISAMPLE
        case 0x9105: RET_TYPE_VAR(type, texture_binding_2d_multisample_array); break; // GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY
        case 0x8CDF: RET_TYPE_VAR(type, max_color_attachments); break; // GL_MAX_COLOR_ATTACHMENTS
        case 0x8D57: RET_TYPE_VAR(type, max_samples); break; // GL_MAX_SAMPLES
        // subroutines.c numbers every subroutine in a stage, and the dispatch
        // is a switch, so these are limits of the rewrite rather than the GPU
        case 0x8DE7: RET_TYPE_CONST(type, MAX_SUB_FNS_LIMIT); break; // GL_MAX_SUBROUTINES
        case 0x8DE8: RET_TYPE_CONST(type, MAX_SUB_UNIFORM_LIMIT); break; // GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS
        case 0x8262: RET_TYPE_VAR(type, max_compute_shared_memory_size); break; // GL_MAX_COMPUTE_SHARED_MEMORY_SIZE
        case 0x90DE: RET_TYPE_VAR(type, max_shader_storage_block_size); break; // GL_MAX_SHADER_STORAGE_BLOCK_SIZE
        case 0x910E: RET_TYPE_VAR(type, max_color_texture_samples); break; // GL_MAX_COLOR_TEXTURE_SAMPLES
        case 0x910F: RET_TYPE_VAR(type, max_depth_texture_samples); break; // GL_MAX_DEPTH_TEXTURE_SAMPLES
        case 0x9110: RET_TYPE_VAR(type, max_integer_samples); break; // GL_MAX_INTEGER_SAMPLES
        case 0x88FC: RET_TYPE_VAR(type, max_dual_source_draw_buffers); break; // GL_MAX_DUAL_SOURCE_DRAW_BUFFERS
        case 0x8919: RET_TYPE_VAR(type, sampler_binding); break; // GL_SAMPLER_BINDING
        case 0x8E89: RET_TYPE_VAR(type, max_tess_control_uniform_blocks); break; // GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS
        case 0x8E8A: RET_TYPE_VAR(type, max_tess_evaluation_uniform_blocks); break; // GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS
        case 0x8DFA: RET_TYPE_VAR(type, shader_compiler); break; // GL_SHADER_COMPILER
        case 0x8DF8: RET_TYPE_VAR(type, shader_binary_formats); break; // GL_SHADER_BINARY_FORMATS
        case 0x8DF9: RET_TYPE_VAR(type, num_shader_binary_formats); break; // GL_NUM_SHADER_BINARY_FORMATS
        case 0x8DFB: RET_TYPE_VAR(type, max_vertex_uniform_vectors); break; // GL_MAX_VERTEX_UNIFORM_VECTORS
        case 0x8DFC: RET_TYPE_VAR(type, max_varying_vectors); break; // GL_MAX_VARYING_VECTORS
        case 0x8DFD: RET_TYPE_VAR(type, max_fragment_uniform_vectors); break; // GL_MAX_FRAGMENT_UNIFORM_VECTORS
        case 0x87FE: RET_TYPE_VAR(type, num_program_binary_formats); break; // GL_NUM_PROGRAM_BINARY_FORMATS
        case 0x87FF: RET_TYPE_VAR(type, program_binary_formats); break; // GL_PROGRAM_BINARY_FORMATS
        case 0x825A: RET_TYPE_VAR(type, program_pipeline_binding); break; // GL_PROGRAM_PIPELINE_BINDING
        case 0x825B: RET_TYPE_VAR(type, max_viewports); break; // GL_MAX_VIEWPORTS
        case 0x825C: RET_TYPE_VAR(type, viewport_subpixel_bits); break; // GL_VIEWPORT_SUBPIXEL_BITS
        case 0x825D: RET_TYPE_VAR(type, viewport_bounds_range); break; // GL_VIEWPORT_BOUNDS_RANGE
        case 0x825E: RET_TYPE_VAR(type, layer_provoking_vertex); break; // GL_LAYER_PROVOKING_VERTEX
        case 0x825F: RET_TYPE_VAR(type, viewport_index_provoking_vertex); break; // GL_VIEWPORT_INDEX_PROVOKING_VERTEX
        case 0x90BC: RET_TYPE_VAR(type, min_map_buffer_alignment); break; // GL_MIN_MAP_BUFFER_ALIGNMENT
        case 0x92D2: RET_TYPE_VAR(type, max_vertex_atomic_counters); break; // GL_MAX_VERTEX_ATOMIC_COUNTERS
        case 0x92D3: RET_TYPE_VAR(type, max_tess_control_atomic_counters); break; // GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS
        case 0x92D4: RET_TYPE_VAR(type, max_tess_evaluation_atomic_counters); break; // GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS
        case 0x92D5: RET_TYPE_VAR(type, max_geometry_atomic_counters); break; // GL_MAX_GEOMETRY_ATOMIC_COUNTERS
        case 0x92D6: RET_TYPE_VAR(type, max_fragment_atomic_counters); break; // GL_MAX_FRAGMENT_ATOMIC_COUNTERS
        case 0x92D7: RET_TYPE_VAR(type, max_combined_atomic_counters); break; // GL_MAX_COMBINED_ATOMIC_COUNTERS
        case 0x8D6B: RET_TYPE_VAR(type, max_element_index); break; // GL_MAX_ELEMENT_INDEX
        case 0x91BB: RET_TYPE_VAR(type, max_compute_uniform_blocks); break; // GL_MAX_COMPUTE_UNIFORM_BLOCKS
        case 0x91BC: RET_TYPE_VAR(type, max_compute_texture_image_units); break; // GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS
        case 0x8263: RET_TYPE_VAR(type, max_compute_uniform_components); break; // GL_MAX_COMPUTE_UNIFORM_COMPONENTS
        case 0x8264: RET_TYPE_VAR(type, max_compute_atomic_counter_buffers); break; // GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS
        case 0x8265: RET_TYPE_VAR(type, max_compute_atomic_counters); break; // GL_MAX_COMPUTE_ATOMIC_COUNTERS
        case 0x8266: RET_TYPE_VAR(type, max_combined_compute_uniform_components); break; // GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS
        case 0x90EB: RET_TYPE_VAR(type, max_compute_work_group_invocations); break; // GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS
        case 0x91BE: RET_TYPE_VAR(type, max_compute_work_group_count[0]); break; // GL_MAX_COMPUTE_WORK_GROUP_COUNT
        case 0x91BF: RET_TYPE_VAR(type, max_compute_work_group_size[0]); break; // GL_MAX_COMPUTE_WORK_GROUP_SIZE
        // the live binding is the honest answer; a cached copy drifts the
        // moment glBindBuffer touches one of these targets
#define RET_BOUND_BUFFER(__slot__) \
    RET_TYPE_CONST(type, (GLuint)(STATE(buffers[__slot__]) ? STATE(buffers[__slot__])->name : 0u))

        case 0x90EF: RET_BOUND_BUFFER(_DISPATCH_INDIRECT_BUFFER); break; // GL_DISPATCH_INDIRECT_BUFFER_BINDING
        case 0x8F43: RET_BOUND_BUFFER(_DRAW_INDIRECT_BUFFER); break;     // GL_DRAW_INDIRECT_BUFFER_BINDING

        case 0x935C: RET_TYPE_VAR(type, clip_origin); break;     // GL_CLIP_ORIGIN
        case 0x935D: RET_TYPE_VAR(type, clip_depth); break;      // GL_CLIP_DEPTH_MODE
        case 0x8F36: RET_BOUND_BUFFER(_COPY_READ_BUFFER); break;         // GL_COPY_READ_BUFFER_BINDING
        case 0x8F37: RET_BOUND_BUFFER(_COPY_WRITE_BUFFER); break;        // GL_COPY_WRITE_BUFFER_BINDING
        case 0x9193: RET_BOUND_BUFFER(_QUERY_BUFFER); break;             // GL_QUERY_BUFFER_BINDING
        case 0x8C2A: RET_BOUND_BUFFER(_TEXTURE_BUFFER); break;           // GL_TEXTURE_BUFFER_BINDING
        case 0x826C: RET_TYPE_VAR(type, max_debug_group_stack_depth); break; // GL_MAX_DEBUG_GROUP_STACK_DEPTH
        case 0x826D: RET_TYPE_VAR(type, debug_group_stack_depth); break; // GL_DEBUG_GROUP_STACK_DEPTH
        case 0x82E8: RET_TYPE_VAR(type, max_label_length); break; // GL_MAX_LABEL_LENGTH
        case 0x826E: RET_TYPE_VAR(type, max_uniform_locations); break; // GL_MAX_UNIFORM_LOCATIONS
        case 0x9315: RET_TYPE_VAR(type, max_framebuffer_width); break; // GL_MAX_FRAMEBUFFER_WIDTH
        case 0x9316: RET_TYPE_VAR(type, max_framebuffer_height); break; // GL_MAX_FRAMEBUFFER_HEIGHT
        case 0x9317: RET_TYPE_VAR(type, max_framebuffer_layers); break; // GL_MAX_FRAMEBUFFER_LAYERS
        case 0x9318: RET_TYPE_VAR(type, max_framebuffer_samples); break; // GL_MAX_FRAMEBUFFER_SAMPLES
        case 0x90D3: RET_TYPE_VAR(type, shader_storage_buffer_binding); break; // GL_SHADER_STORAGE_BUFFER_BINDING
        case 0x90D4: RET_TYPE_VAR(type, shader_storage_buffer_start); break; // GL_SHADER_STORAGE_BUFFER_START
        case 0x90D5: RET_TYPE_VAR(type, shader_storage_buffer_size); break; // GL_SHADER_STORAGE_BUFFER_SIZE
        case 0x90D6: RET_TYPE_VAR(type, max_vertex_shader_storage_blocks); break; // GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS
        case 0x90D7: RET_TYPE_VAR(type, max_geometry_shader_storage_blocks); break; // GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS
        case 0x90D8: RET_TYPE_VAR(type, max_tess_control_shader_storage_blocks); break; // GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS
        case 0x90D9: RET_TYPE_VAR(type, max_tess_evaluation_shader_storage_blocks); break; // GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS
        case 0x90DA: RET_TYPE_VAR(type, max_fragment_shader_storage_blocks); break; // GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS
        case 0x90DB: RET_TYPE_VAR(type, max_compute_shader_storage_blocks); break; // GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS
        case 0x90DC: RET_TYPE_VAR(type, max_combined_shader_storage_blocks); break; // GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS
        case 0x90DD: RET_TYPE_VAR(type, max_shader_storage_buffer_bindings); break; // GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS
        case 0x90DF: RET_TYPE_VAR(type, shader_storage_buffer_offset_alignment); break; // GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT
        case 0x919F: RET_TYPE_VAR(type, texture_buffer_offset_alignment); break; // GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT
        case 0x82D6: RET_TYPE_VAR(type, vertex_binding_divisor); break; // GL_VERTEX_BINDING_DIVISOR
        case 0x82D7: RET_TYPE_VAR(type, vertex_binding_offset); break; // GL_VERTEX_BINDING_OFFSET
        case 0x82D8: RET_TYPE_VAR(type, vertex_binding_stride); break; // GL_VERTEX_BINDING_STRIDE
        case 0x82D9: RET_TYPE_VAR(type, max_vertex_attrib_relative_offset); break; // GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET
        case 0x82DA: RET_TYPE_VAR(type, max_vertex_attrib_bindings); break; // GL_MAX_VERTEX_ATTRIB_BINDINGS

        case 0x8C80: RET_TYPE_VAR(type, max_transform_feedback_separate_components); break; // GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS
        case 0x8C8A: RET_TYPE_VAR(type, max_transform_feedback_interleaved_components); break; // GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS
        // this one is a loop bound in the CTS's state reset, so it has to be
        // the number of binding points we actually accept
        case 0x8C8B: RET_TYPE_VAR(type, max_transform_feedback_separate_attribs); break; // GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS
        case 0x8E70: RET_TYPE_VAR(type, max_transform_feedback_buffers); break; // GL_MAX_TRANSFORM_FEEDBACK_BUFFERS

        // GL_TRANSFORM_FEEDBACK_BINDING, and whether it is recording
        case 0x8E25: RET_TYPE_CONST(type, (GLint)(ctx->state.transform_feedback ?
                                          ctx->state.transform_feedback->name : 0u)); break;
        case 0x8E23: RET_TYPE_CONST(type, (GLint)(ctx->state.transform_feedback &&
                                          ctx->state.transform_feedback->paused)); break;
        case 0x8E24: RET_TYPE_CONST(type, (GLint)(ctx->state.transform_feedback &&
                                          ctx->state.transform_feedback->active)); break;

        case 0x9143: RET_TYPE_CONST(type, 1024); break; // GL_MAX_DEBUG_MESSAGE_LENGTH
        case 0x9144: RET_TYPE_CONST(type, 64); break;   // GL_MAX_DEBUG_LOGGED_MESSAGES

        case 0x8E72: RET_TYPE_VAR(type, patch_vertices); break;       // GL_PATCH_VERTICES
        case 0x8E73: RET_TYPE_VAR_COUNT(type, patch_default_inner, 2); break; // GL_PATCH_DEFAULT_INNER_LEVEL
        case 0x8E74: RET_TYPE_VAR_COUNT(type, patch_default_outer, 4); break; // GL_PATCH_DEFAULT_OUTER_LEVEL
        // the geometry limits MGL never answered; the shader rewrite bounds
        // the first two, the rest are the 4.6 floor
        case 0x8DE0: RET_TYPE_VAR(type, max_geometry_output_vertices); break; // GL_MAX_GEOMETRY_OUTPUT_VERTICES
        case 0x92CF: RET_TYPE_VAR(type, max_geometry_atomic_counter_buffers); break; // GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS
        case 0x90CD: RET_TYPE_VAR(type, max_geometry_image_uniforms); break; // GL_MAX_GEOMETRY_IMAGE_UNIFORMS
        case 0x8E71: RET_TYPE_VAR(type, max_vertex_streams); break; // GL_MAX_VERTEX_STREAMS
        case 0x8F39: RET_TYPE_VAR(type, max_combined_shader_output_resources); break; // GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES
        case 0x8E7D: RET_TYPE_VAR(type, max_patch_vertices); break; // GL_MAX_PATCH_VERTICES
        // the rest of the 4.6 tessellation limits, at the floor the spec sets
        case 0x92CD: RET_TYPE_VAR(type, max_tess_control_atomic_counter_buffers); break; // GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS
        case 0x92CE: RET_TYPE_VAR(type, max_tess_evaluation_atomic_counter_buffers); break; // GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS
        case 0x8E81: RET_TYPE_VAR(type, max_tess_control_texture_image_units); break; // GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS
        case 0x8E82: RET_TYPE_VAR(type, max_tess_evaluation_texture_image_units); break; // GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS
        case 0x8E7F: RET_TYPE_VAR(type, max_tess_control_uniform_components); break; // GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS
        case 0x8E80: RET_TYPE_VAR(type, max_tess_evaluation_uniform_components); break; // GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS
        case 0x8E1E: RET_TYPE_VAR(type, max_combined_tess_control_uniform_components); break; // GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS
        case 0x8E1F: RET_TYPE_VAR(type, max_combined_tess_evaluation_uniform_components); break; // GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS
        case 0x90CB: RET_TYPE_VAR(type, max_tess_control_image_uniforms); break; // GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS
        case 0x90CC: RET_TYPE_VAR(type, max_tess_evaluation_image_uniforms); break; // GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS
        case 0x8E7E: RET_TYPE_VAR(type, max_tess_gen_level); break; // GL_MAX_TESS_GEN_LEVEL
        case 0x8E84: RET_TYPE_VAR(type, max_tess_patch_components); break; // GL_MAX_TESS_PATCH_COMPONENTS
        case 0x8E85: RET_TYPE_VAR(type, max_tess_control_total_output_components); break; // GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS
        case 0x886C: RET_TYPE_VAR(type, max_tess_control_input_components); break; // GL_MAX_TESS_CONTROL_INPUT_COMPONENTS
        case 0x8E83: RET_TYPE_VAR(type, max_tess_control_output_components); break; // GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS
        case 0x886D: RET_TYPE_VAR(type, max_tess_evaluation_input_components); break; // GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS
        case 0x8E86: RET_TYPE_VAR(type, max_tess_evaluation_output_components); break; // GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS

        // 0x8F38 is MAX_IMAGE_UNITS, not MAX_IMAGE_SAMPLES -- an earlier pass
        // mislabelled it and answered 1 where the 4.6 floor is 8.
        case 0x8F38: RET_TYPE_VAR(type, max_image_units); break; // GL_MAX_IMAGE_UNITS
        case 0x906D: RET_TYPE_VAR(type, max_image_samples); break; // GL_MAX_IMAGE_SAMPLES
        case 0x90CA: RET_TYPE_VAR(type, max_vertex_image_uniforms); break; // GL_MAX_VERTEX_IMAGE_UNIFORMS
        case 0x90CE: RET_TYPE_VAR(type, max_fragment_image_uniforms); break; // GL_MAX_FRAGMENT_IMAGE_UNIFORMS
        case 0x90CF: RET_TYPE_VAR(type, max_combined_image_uniforms); break; // GL_MAX_COMBINED_IMAGE_UNIFORMS
        case 0x91BD: RET_TYPE_VAR(type, max_compute_image_uniforms); break; // GL_MAX_COMPUTE_IMAGE_UNIFORMS

        case 0x82F9: RET_TYPE_VAR(type, max_cull_distances); break; // GL_MAX_CULL_DISTANCES
        case 0x82FA: RET_TYPE_VAR(type, max_combined_clip_and_cull_distances); break; // GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES
        case 0x82E5: RET_TYPE_VAR(type, max_vertex_attrib_stride); break; // GL_MAX_VERTEX_ATTRIB_STRIDE
        case 0x8E5E: RET_TYPE_VAR(type, min_program_texture_gather_offset); break; // GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET
        case 0x8E5F: RET_TYPE_VAR(type, max_program_texture_gather_offset); break; // GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET
        case 0x92CC: RET_TYPE_VAR(type, max_vertex_atomic_counter_buffers); break; // GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS
        case 0x92D0: RET_TYPE_VAR(type, max_fragment_atomic_counter_buffers); break; // GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS
        case 0x92D1: RET_TYPE_VAR(type, max_combined_atomic_counter_buffers); break; // GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS

        case 0x8E22: RET_TYPE_CONST(type, 0); break;    // GL_TRANSFORM_FEEDBACK

        case 0x8E5A: RET_TYPE_VAR(type, max_geometry_shader_invocations); break; // GL_MAX_GEOMETRY_SHADER_INVOCATIONS
        case 0x8DE1: RET_TYPE_VAR(type, max_geometry_total_output_components); break; // GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS

        case 0x8E5B: RET_TYPE_CONST(type, -0.5); break; // GL_MIN_FRAGMENT_INTERPOLATION_OFFSET
        case 0x8E5C: RET_TYPE_CONST(type, 0.5); break;  // GL_MAX_FRAGMENT_INTERPOLATION_OFFSET
        case 0x8E5D: RET_TYPE_CONST(type, 4); break;    // GL_FRAGMENT_INTERPOLATION_OFFSET_BITS

        case 0x92DC: RET_TYPE_VAR(type, max_atomic_counter_buffer_bindings); break; // GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS
        case 0x92D8: RET_TYPE_VAR(type, max_atomic_counter_buffer_size); break; // GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE
        case 0x92D9: RET_TYPE_CONST(type, MAX_ATOMIC_COUNTER_BUFFER_BINDINGS); break; // GL_ACTIVE_ATOMIC_COUNTER_BUFFERS

        default:
            // Falling out of the switch used to leave the caller's variable
            // untouched and report no error, so it read back whatever was
            // already there and believed it.
            MGL_ERR("MGL: mglGet unhandled pname 0x%x\n", pname);
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetBooleanv(GLMContext ctx, GLenum pname, GLboolean *data)
{
    mglGet(ctx, pname, kBool, (void *)data);
}

void mglGetDoublev(GLMContext ctx, GLenum pname, GLdouble *data)
{
    ERROR_CHECK_RETURN(data, GL_INVALID_VALUE);

    mglGet(ctx, pname, kDouble, (void *)data);
}

void mglGetFloatv(GLMContext ctx, GLenum pname, GLfloat *data)
{
    ERROR_CHECK_RETURN(data, GL_INVALID_VALUE);

    mglGet(ctx, pname, kFloat, (void *)data);
}

void mglGetIntegerv(GLMContext ctx, GLenum pname, GLint *data)
{
    ERROR_CHECK_RETURN(data, GL_INVALID_VALUE);

    mglGet(ctx, pname, kInt, (void *)data);
}

// what GetStringi walks
static const char * const mgl_extensions[] = {
    "GL_ARB_multi_draw_indirect",
    "GL_ARB_base_instance",
    "GL_ARB_draw_elements_base_vertex",
    "GL_ARB_draw_indirect",
    "GL_ARB_draw_instanced",
    "GL_ARB_instanced_arrays",
    "GL_ARB_texture_storage",
    "GL_ARB_texture_storage_multisample",
    "GL_ARB_texture_multisample",
    "GL_ARB_texture_cube_map_array",
    "GL_ARB_texture_buffer_object",
    "GL_ARB_texture_buffer_range",
    "GL_ARB_texture_rectangle",
    "GL_ARB_texture_swizzle",
    "GL_ARB_texture_rg",
    "GL_ARB_texture_float",
    "GL_ARB_texture_compression",
    "GL_ARB_texture_compression_bptc",
    "GL_ARB_depth_texture",
    "GL_ARB_depth_buffer_float",
    "GL_ARB_framebuffer_object",
    "GL_ARB_framebuffer_sRGB",
    "GL_ARB_draw_buffers",
    "GL_ARB_draw_buffers_blend",
    "GL_ARB_blend_func_extended",
    "GL_ARB_vertex_array_object",
    "GL_ARB_vertex_attrib_binding",
    "GL_ARB_vertex_attrib_64bit",
    "GL_ARB_gpu_shader_fp64",
    "GL_ARB_vertex_buffer_object",
    "GL_ARB_uniform_buffer_object",
    "GL_ARB_shader_storage_buffer_object",
    "GL_ARB_shader_atomic_counters",
    "GL_ARB_shader_image_load_store",
    "GL_ARB_compute_shader",
    "GL_ARB_explicit_attrib_location",
    "GL_ARB_explicit_uniform_location",
    "GL_ARB_separate_shader_objects",
    "GL_ARB_get_program_binary",
    "GL_ARB_gl_spirv",
    "GL_ARB_spirv_extensions",
    "GL_ARB_sync",
    "GL_ARB_timer_query",
    "GL_ARB_occlusion_query2",
    "GL_ARB_shader_subroutine",
    "GL_ARB_transform_feedback2",
    "GL_ARB_transform_feedback3",
    "GL_ARB_copy_buffer",
    "GL_ARB_copy_image",
    "GL_ARB_buffer_storage",
    "GL_ARB_map_buffer_range",
    "GL_ARB_invalidate_subdata",
    "GL_ARB_clear_buffer_object",
    "GL_ARB_clear_texture",
    "GL_ARB_direct_state_access",
    "GL_ARB_multi_bind",
    "GL_ARB_sampler_objects",
    "GL_ARB_seamless_cube_map",
    "GL_ARB_polygon_offset_clamp",
    "GL_ARB_clip_control",
    "GL_ARB_viewport_array",
    "GL_ARB_texture_filter_anisotropic",
    "GL_ARB_texture_mirror_clamp_to_edge",
    "GL_ARB_ES2_compatibility",
    "GL_ARB_ES3_compatibility",
    "GL_ARB_ES3_1_compatibility",
    "GL_ARB_bindless_texture",
    "GL_EXT_shader_image_load_formatted",
    "GL_ARB_cull_distance",
    // the rest of what 4.6 core took in, so tests that look for the name run
    "GL_ARB_arrays_of_arrays",
    "GL_ARB_conditional_render_inverted",
    "GL_ARB_derivative_control",
    "GL_ARB_enhanced_layouts",
    "GL_ARB_fragment_layer_viewport",
    "GL_ARB_get_texture_sub_image",
    "GL_ARB_gpu_shader5",
    "GL_ARB_indirect_parameters",
    "GL_ARB_internalformat_query",
    "GL_ARB_internalformat_query2",
    "GL_ARB_pipeline_statistics_query",
    "GL_ARB_program_interface_query",
    "GL_ARB_query_buffer_object",
    "GL_ARB_shader_draw_parameters",
    "GL_ARB_shader_image_size",
    "GL_ARB_shading_language_420pack",
    "GL_ARB_stencil_texturing",
    "GL_ARB_tessellation_shader",
    "GL_ARB_texture_buffer_object_rgb32",
    "GL_ARB_texture_gather",
    "GL_ARB_texture_query_levels",
    "GL_ARB_texture_query_lod",
    "GL_ARB_texture_rgb10_a2ui",
    "GL_ARB_texture_stencil8",
    "GL_ARB_texture_view",
    "GL_ARB_transform_feedback_instanced",
    "GL_ARB_transform_feedback_overflow_query",
    "GL_KHR_debug",
    "GL_KHR_robustness",
    "GL_EXT_texture_filter_anisotropic",
    "GL_EXT_texture_sRGB",
    "GL_EXT_texture_compression_s3tc",
    // Core since 3.0 and 3.3, and named here because tests and applications
    // check for the old string rather than the version.
    "GL_EXT_texture_integer",
    "GL_EXT_texture_shared_exponent",
    "GL_EXT_texture_type_2_10_10_10_REV",
};

static const GLuint mgl_num_extensions = (GLuint)(sizeof(mgl_extensions)/sizeof(mgl_extensions[0]));

GLuint mglNumExtensions(void)
{
    return mgl_num_extensions;
}

const GLubyte *mglGetString(GLMContext ctx, GLenum name)
{
    switch(name)
    {
        case GL_VENDOR:
            return (const GLubyte *)"MGL";

        case GL_RENDERER:
            return (const GLubyte *)"MGL";

        case GL_VERSION:
            return (const GLubyte *)"4.6.0 MGL";

        case GL_SHADING_LANGUAGE_VERSION:
            return (const GLubyte *)"4.60";

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, NULL);
            return NULL;
    }
}

const GLubyte *mglGetStringi(GLMContext ctx, GLenum name, GLuint index)
{
    switch(name)
    {
        case GL_EXTENSIONS:
            ERROR_CHECK_RETURN_VALUE(index < mgl_num_extensions, GL_INVALID_VALUE, NULL);
            return (const GLubyte *)mgl_extensions[index];

        case GL_SHADING_LANGUAGE_VERSION:
            ERROR_CHECK_RETURN_VALUE(index == 0, GL_INVALID_VALUE, NULL);
            return (const GLubyte *)"4.60";

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, NULL);
    }
}

void mglGetInteger64v(GLMContext ctx, GLenum pname, GLint64 *data)
{
    GLint tmp = 0;

    ERROR_CHECK_RETURN(data, GL_INVALID_VALUE);

    switch(pname)
    {
        // these are genuinely 64 bit
        case GL_MAX_ELEMENT_INDEX:
            *data = 0xFFFFFFFF;
            return;

        case GL_MAX_SERVER_WAIT_TIMEOUT:
            *data = 0x7FFFFFFFFFFFFFFFLL;
            return;

        case GL_TIMESTAMP:
            *data = 0;
            return;

        default:
            break;
    }

    mglGet(ctx, pname, kInt, (void *)&tmp);

    *data = (GLint64)tmp;
}

void mglGetInteger64i_v(GLMContext ctx, GLenum target, GLuint index, GLint64 *data)
{
    GLdouble v[4];
    int count;
    GLint tmp = 0;

    if (data == NULL)
        return;

    count = mglIndexedStateValues(ctx, target, index, v);

    if (count != 0)
    {
        ERROR_CHECK_RETURN(count > 0, GL_INVALID_VALUE);

        for (int i = 0; i < count; i++)
            data[i] = (GLint64)v[i];

        return;
    }

    mglGetIntegeri_v(ctx, target, index, &tmp);

    *data = tmp;
}

void mglGetIntegeri_v(GLMContext ctx, GLenum target, GLuint index, GLint *data)
{
    GLdouble v[4];
    int count;

    if (data == NULL)
        return;

    count = mglIndexedStateValues(ctx, target, index, v);

    if (count != 0)
    {
        ERROR_CHECK_RETURN(count > 0, GL_INVALID_VALUE);

        mglWriteTypedValues(data, kInt, v, count);

        return;
    }

    switch(target)
    {
        case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
            if (index < 3)
            {
                *data = ctx->state.var.max_compute_work_group_count[index];
            }
            break;

        case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
            if (index < 3)
            {
                *data = ctx->state.var.max_compute_work_group_size[index];
            }
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static GLboolean internalFormatSupported(GLenum internalformat)
{
    return mtlFormatForGLInternalFormat(internalformat) != 0;
}

static GLint internalFormatQuery(GLMContext ctx, GLenum target, GLenum internalformat, GLenum pname)
{
    switch(pname)
    {
        case GL_INTERNALFORMAT_SUPPORTED:
            return internalFormatSupported(internalformat);

        case GL_SAMPLES:
            return 4;

        case GL_NUM_SAMPLE_COUNTS:
            return 1;

        case GL_INTERNALFORMAT_PREFERRED:
            return internalformat;

        case GL_INTERNALFORMAT_RED_SIZE:
            return bitcountForInternalFormat(internalformat, GL_RED);

        case GL_INTERNALFORMAT_GREEN_SIZE:
            return bitcountForInternalFormat(internalformat, GL_GREEN);

        case GL_INTERNALFORMAT_BLUE_SIZE:
            return bitcountForInternalFormat(internalformat, GL_BLUE);

        case GL_INTERNALFORMAT_ALPHA_SIZE:
            return bitcountForInternalFormat(internalformat, GL_ALPHA);

        case GL_INTERNALFORMAT_DEPTH_SIZE:
            return bitcountForInternalFormat(internalformat, GL_DEPTH_COMPONENT);

        case GL_INTERNALFORMAT_STENCIL_SIZE:
            return bitcountForInternalFormat(internalformat, GL_STENCIL_INDEX);

        case GL_MAX_WIDTH:
        case GL_MAX_HEIGHT:
            return 16384;

        case GL_MAX_DEPTH:
            return (target == GL_TEXTURE_3D) ? 2048 : 1;

        case GL_MAX_LAYERS:
            return 2048;

        case GL_TEXTURE_IMAGE_FORMAT:
        case GL_GET_TEXTURE_IMAGE_FORMAT:
            return mglFormatDesc(internalformat)->upload_format ? mglFormatDesc(internalformat)->upload_format : GL_NONE;

        case GL_TEXTURE_IMAGE_TYPE:
        case GL_GET_TEXTURE_IMAGE_TYPE:
            return mglFormatDesc(internalformat)->upload_type ? mglFormatDesc(internalformat)->upload_type : GL_NONE;

        // what Metal can do with the format on this device
        case GL_COLOR_RENDERABLE:
            return (mglFormatCaps(internalformat) & MGL_FMT_CAP_COLOR_ATT) ? GL_TRUE : GL_FALSE;

        case GL_FRAMEBUFFER_RENDERABLE:
        case GL_FRAMEBUFFER_BLEND:
        {
            uint16_t caps = mglFormatCaps(internalformat);
            uint16_t need = (pname == GL_FRAMEBUFFER_BLEND) ? MGL_FMT_CAP_BLEND
                          : (MGL_FMT_CAP_COLOR_ATT | MGL_FMT_CAP_DS_ATT);
            return (caps & need) ? GL_FULL_SUPPORT : GL_NONE;
        }

        case GL_FILTER:
            return (mglFormatCaps(internalformat) & MGL_FMT_CAP_FILTER) ? GL_FULL_SUPPORT : GL_NONE;

        case GL_SHADER_IMAGE_LOAD:
            return (mglFormatCaps(internalformat) & MGL_FMT_CAP_READ) ? GL_FULL_SUPPORT : GL_NONE;

        case GL_SHADER_IMAGE_STORE:
            return (mglFormatCaps(internalformat) & MGL_FMT_CAP_WRITE) ? GL_FULL_SUPPORT : GL_NONE;

        case GL_SHADER_IMAGE_ATOMIC:
            return (mglFormatCaps(internalformat) & MGL_FMT_CAP_ATOMIC) ? GL_FULL_SUPPORT : GL_NONE;

        case GL_TEXTURE_VIEW:
        case GL_MIPMAP:
            return internalFormatSupported(internalformat) ? GL_FULL_SUPPORT : GL_NONE;

        case GL_TEXTURE_COMPRESSED:
            return mglFormatIsCompressed(internalformat) ? GL_TRUE : GL_FALSE;

        case GL_TEXTURE_COMPRESSED_BLOCK_WIDTH:
            return mglFormatDesc(internalformat)->block_w > 1 ? mglFormatDesc(internalformat)->block_w : 0;

        case GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT:
            return mglFormatDesc(internalformat)->block_h > 1 ? mglFormatDesc(internalformat)->block_h : 0;

        case GL_TEXTURE_COMPRESSED_BLOCK_SIZE:
            return mglFormatIsCompressed(internalformat) ? mglFormatDesc(internalformat)->bytes_per_block * 8 : 0;

        case GL_DEPTH_RENDERABLE:
            return mglFormatDesc(internalformat)->bits[4] ? GL_TRUE : GL_FALSE;

        case GL_STENCIL_RENDERABLE:
            return mglFormatDesc(internalformat)->bits[5] ? GL_TRUE : GL_FALSE;

        default:
            return 0;
    }
}

static GLboolean validInternalFormatTarget(GLenum target)
{
    switch(target)
    {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        case GL_TEXTURE_RECTANGLE:
        case GL_TEXTURE_BUFFER:
        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        case GL_RENDERBUFFER:
            return GL_TRUE;
    }

    return GL_FALSE;
}

void mglGetInternalformativ(GLMContext ctx, GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint *params)
{
    ERROR_CHECK_RETURN(validInternalFormatTarget(target), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params || count == 0, GL_INVALID_VALUE);

    if (count == 0)
        return;

    params[0] = internalFormatQuery(ctx, target, internalformat, pname);

    for(GLsizei i=1; i<count; i++)
        params[i] = 0;
}

void mglGetInternalformati64v(GLMContext ctx, GLenum target, GLenum internalformat, GLenum pname, GLsizei count, GLint64 *params)
{
    ERROR_CHECK_RETURN(validInternalFormatTarget(target), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params || count == 0, GL_INVALID_VALUE);

    if (count == 0)
        return;

    params[0] = (GLint64)internalFormatQuery(ctx, target, internalformat, pname);

    for(GLsizei i=1; i<count; i++)
        params[i] = 0;
}

/* ---------- indexed getters ---------- */

static void getIndexed(GLMContext ctx, GLenum target, GLuint index, GLuint type, void *data)
{
    GLdouble v[4];
    int count;

    if (data == NULL)
        return;

    count = mglIndexedStateValues(ctx, target, index, v);

    ERROR_CHECK_RETURN(count != 0, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(count > 0, GL_INVALID_VALUE);

    mglWriteTypedValues(data, type, v, count);
}

void mglGetBooleani_v(GLMContext ctx, GLenum target, GLuint index, GLboolean *data)
{
    getIndexed(ctx, target, index, kBool, data);
}

void mglGetFloati_v(GLMContext ctx, GLenum target, GLuint index, GLfloat *data)
{
    getIndexed(ctx, target, index, kFloat, data);
}

void mglGetDoublei_v(GLMContext ctx, GLenum target, GLuint index, GLdouble *data)
{
    getIndexed(ctx, target, index, kDouble, data);
}
