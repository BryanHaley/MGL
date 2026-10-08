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
 * draw_buffers.c
 * MGL
 *
 */

#include <mach/mach_vm.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <string.h>

#include "glm_context.h"
#include "mgl_log.h"

extern Buffer *newBuffer(GLMContext ctx, GLenum target, GLuint name);
extern kern_return_t initBufferData(GLMContext ctx, Buffer *ptr, GLsizeiptr size, const void *data, bool isUniformConstant);

static size_t indexTypeSize(GLenum type)
{
    switch(type)
    {
        case GL_UNSIGNED_BYTE:  return 1;
        case GL_UNSIGNED_SHORT: return 2;
        case GL_UNSIGNED_INT:   return 4;
    }

    return 0;
}

// A core profile has no client-side arrays, but plenty of code -- the
// conformance suite included -- still hands a draw its indices as a plain
// pointer. Metal can only read them out of a buffer, so copy them into one
// of MGL's own and bind that for the draw.
//
// Returns true when it staged something, and the caller then reads from
// offset zero and must call endClientIndices when the draw is over.
static bool beginClientIndices(GLMContext ctx, GLsizei count, GLenum type, const void **indices)
{
    size_t elem = indexTypeSize(type);
    size_t bytes;

    if (!ctx->state.vao || ctx->state.vao->element_array.buffer)
        return false;

    if (*indices == NULL || count <= 0 || elem == 0)
        return false;

    bytes = (size_t)count * elem;

    if (ctx->state.client_indices == NULL)
    {
        ctx->state.client_indices = newBuffer(ctx, GL_ELEMENT_ARRAY_BUFFER, 0);

        if (ctx->state.client_indices == NULL)
            return false;
    }

    if (initBufferData(ctx, ctx->state.client_indices, (GLsizeiptr)bytes, *indices, false) != 0)
        return false;

    ctx->state.vao->element_array.buffer = ctx->state.client_indices;
    *indices = NULL;

    return true;
}

static void endClientIndices(GLMContext ctx, bool staged)
{
    if (staged && ctx->state.vao)
        ctx->state.vao->element_array.buffer = NULL;
}

// Primitives a run of n vertices makes in this mode.
static GLuint64 primitivesIn(GLMContext ctx, GLenum mode, GLuint64 n)
{
    switch (mode)
    {
        case GL_POINTS:                     return n;
        case GL_LINES:                      return n / 2;
        case GL_LINE_LOOP:                  return n > 2 ? n : n == 2 ? 1 : 0;
        case GL_LINE_STRIP:                 return n > 1 ? n - 1 : 0;
        case GL_TRIANGLES:                  return n / 3;
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:               return n > 2 ? n - 2 : 0;
        case GL_LINES_ADJACENCY:            return n / 4;
        case GL_LINE_STRIP_ADJACENCY:       return n > 3 ? n - 3 : 0;
        case GL_TRIANGLES_ADJACENCY:        return n / 6;
        case GL_TRIANGLE_STRIP_ADJACENCY:   return n >= 6 ? (n - 4) / 2 : 0;
        case GL_PATCHES:
            return ctx->state.var.patch_vertices > 0 ? n / (GLuint64)ctx->state.var.patch_vertices : 0;
    }

    return 0;
}

// The pipeline statistics a draw adds, from what it was asked to draw. In an
// element draw a restart index ends one run of primitives and starts another.
static void countDraw(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices,
                      GLsizei instances)
{
    GLuint64 verts = (GLuint64)count, prims;
    Buffer *eb = type && ctx->state.vao ? ctx->state.vao->element_array.buffer : NULL;
    size_t elem = indexTypeSize(type), at = (size_t)(uintptr_t)indices;
    uint32_t restart = ctx->state.var.primitive_restart_index;
    bool restarts = ctx->state.caps.primitive_restart || ctx->state.caps.primitive_restart_fixed_index;

    if (!mglAnyStatisticActive(ctx) || count <= 0 || instances <= 0)
        return;

    if (ctx->state.caps.primitive_restart_fixed_index)
        restart = elem == 1 ? 0xFFu : elem == 2 ? 0xFFFFu : 0xFFFFFFFFu;

    prims = primitivesIn(ctx, mode, verts);

    if (restarts && eb && eb->data.buffer_data && elem && at + (size_t)count * elem <= (size_t)eb->size)
    {
        const uint8_t *from = (const uint8_t *)eb->data.buffer_data + at;
        GLuint64 run = 0;

        verts = prims = 0;

        for (GLsizei k = 0; k < count; k++)
        {
            uint32_t v = elem == 1 ? from[k] : elem == 2 ? ((const uint16_t *)from)[k] : ((const uint32_t *)from)[k];

            if (v == restart)
            {
                prims += primitivesIn(ctx, mode, run);
                run = 0;
                continue;
            }

            verts++;
            run++;
        }

        prims += primitivesIn(ctx, mode, run);
    }

    mglCountDrawStatistics(ctx, verts, prims, instances);
}

// The same for each command an indirect draw reads, once anything the GPU
// was writing into them has landed.
static void countIndirect(GLMContext ctx, GLenum mode, GLenum type, const void *indirect,
                          GLsizei drawcount, GLsizei stride)
{
    Buffer *buf = STATE(buffers[_DRAW_INDIRECT_BUFFER]);
    size_t size = type ? sizeof(DrawElementsIndirectCommand) : sizeof(DrawArraysIndirectCommand);
    size_t step = stride ? (size_t)stride : size;

    if (!mglAnyStatisticActive(ctx) || buf == NULL || buf->data.buffer_data == 0)
        return;

    ctx->mtl_funcs.mtlFlush(ctx, true);

    for (GLsizei i = 0; i < drawcount; i++)
    {
        const uint8_t *at = (const uint8_t *)buf->data.buffer_data + (uintptr_t)indirect + (size_t)i * step;

        if (type)
        {
            DrawElementsIndirectCommand cmd;

            memcpy(&cmd, at, sizeof cmd);
            countDraw(ctx, mode, (GLsizei)cmd.count, type,
                      (const void *)(uintptr_t)(cmd.first * indexTypeSize(type)), (GLsizei)cmd.instanceCount);
        }
        else
        {
            DrawArraysIndirectCommand cmd;

            memcpy(&cmd, at, sizeof cmd);
            countDraw(ctx, mode, (GLsizei)cmd.count, 0, NULL, (GLsizei)cmd.instanceCount);
        }
    }
}

bool check_draw_modes(GLenum mode)
{
    switch(mode)
    {
        case GL_POINTS:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
        case GL_LINES:
        case GL_LINE_STRIP_ADJACENCY:
        case GL_LINES_ADJACENCY:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP_ADJACENCY:
        case GL_TRIANGLES_ADJACENCY:
        case GL_PATCHES:
            return true;
    }

    // need to verify against geometry shaders when I get there

    return false;
}

bool check_element_type(GLenum mode)
{
    switch(mode)
    {
        // all three are legal in GL. Metal has no uint8 index type, so that one
        // is refused further down with GL_INVALID_OPERATION -- refusing the enum
        // here would be telling the application it wrote something it did not.
        case GL_UNSIGNED_BYTE:
        case GL_UNSIGNED_SHORT:
        case GL_UNSIGNED_INT:
            return true;
    }

    return false;
}

bool processVAO(GLMContext ctx)
{
    VertexArray *vao;

    vao = ctx->state.vao;

    if (vao == NULL)
    {
        MGL_ERR("MGL Error: %s: no vertex array bound\n", __FUNCTION__);
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    if (vao->dirty_bits & DIRTY_VAO_BUFFER_BASE)
    {
        // map buffer bindings to vertex array
        for(int i=0; i<ctx->state.max_vertex_attribs; i++)
        {
            if (vao->enabled_attribs & (0x1 << i))
            {
                if (VAO_BINDING(vao, i)->buffer == NULL)
                {
                    // no buffer bound to active attrib...
                    return false;
                }
            }

            // early out
            if ((VAO_STATE(enabled_attribs) >> (i+1)) == 0)
                break;
        }

        // clear buffer base dirty bits as we have mapped buffers to attribs
        vao->dirty_bits &= ~DIRTY_VAO_BUFFER_BASE;
    }

    return true;
}

bool validate_vao(GLMContext ctx, bool uses_elements)
{
    // an incomplete framebuffer is its own error, and it is the one GL keeps
    if (!mglDrawFramebufferComplete(ctx))
        return false;

    if (!VAO()) {
        MGL_ERR("MGL Error: validate_vao: VAO is NULL\n");
        return false;
    }

    // no attribs enabled..
    // if (VAO_STATE(enabled_attribs) == 0)
    //    return false;

    if (ctx->state.vao->dirty_bits)
    {
        if (!processVAO(ctx)) {
            MGL_ERR("MGL Error: validate_vao: processVAO failed\n");
            return false;
        }
    }

    unsigned int enabled_attribs;

    enabled_attribs = ctx->state.vao->enabled_attribs;

    int i=0;
    do
    {
        if (enabled_attribs & 0x1)
        {
            // mapped buffers cannot be used during draw calls
            Buffer *attrib_buffer = VAO_ATTRIB_BINDING(i)->buffer;

            if (attrib_buffer == NULL) {
                MGL_ERR("MGL Error: validate_vao: attrib %d has no buffer\n", i);
                return false;
            }

            // a persistent mapping is meant to stay up while the draw runs
            if (attrib_buffer->mapped && !(attrib_buffer->access & GL_MAP_PERSISTENT_BIT)) {
                MGL_ERR("MGL Error: validate_vao: attrib %d buffer mapped\n", i);
                return false;
            }
        }

        i++;
        enabled_attribs >>= 1;
    } while(enabled_attribs);

    if (uses_elements)
    {
        if (!ctx->state.vao->element_array.buffer) {
            MGL_ERR("MGL Error: validate_vao: element buffer missing\n");
            return false;
        }
    }

    return true;
}

// What a geometry shader's input layout says the draw mode has to be.
static bool mode_feeds_gs(GLenum gs_in, GLenum mode)
{
    switch(gs_in)
    {
        case GL_POINTS:
            return mode == GL_POINTS;

        case GL_LINES:
            return mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP;

        case GL_LINES_ADJACENCY:
            return mode == GL_LINES_ADJACENCY || mode == GL_LINE_STRIP_ADJACENCY;

        case GL_TRIANGLES:
            return mode == GL_TRIANGLES || mode == GL_TRIANGLE_STRIP || mode == GL_TRIANGLE_FAN;

        case GL_TRIANGLES_ADJACENCY:
            return mode == GL_TRIANGLES_ADJACENCY || mode == GL_TRIANGLE_STRIP_ADJACENCY;
    }

    return true;
}

bool validate_program(GLMContext ctx, GLenum mode)
{
    Program *prog = ctx->state.program;

    // Allow NULL program (MGLRenderer handles it by using cached pipeline or program pipeline)
    if (prog == NULL)
        return true;

    // patches only make sense with a tessellation stage, and a tessellation
    // stage only accepts patches
    if (prog->tess.active)
    {
        if (mode != GL_PATCHES)
            return false;
    }
    else if (mode == GL_PATCHES)
    {
        return false;
    }

    // behind tessellation the geometry stage is fed by the evaluation stage
    if (mglProgramHasGeometry(prog) && !prog->tess.active &&
        !mode_feeds_gs(prog->geom.in_primitive, mode))
        return false;

    return true;
}

GLsizei getTypeSize(GLenum type)
{
    switch(type)
    {
        case GL_UNSIGNED_SHORT:
            return sizeof(unsigned short);

        case GL_UNSIGNED_BYTE:
            return sizeof(unsigned char);

        case GL_UNSIGNED_INT:
            return sizeof(unsigned int);
    }

    // callers check for 0 and raise GL_INVALID_ENUM
    return 0;
}

void mglDrawArrays(GLMContext ctx, GLenum mode, GLint first, GLsizei count)
{
    // MGL_INFO("DEBUG: mglDrawArrays ctx=%p prog=%p dirty=%x\n", ctx, ctx->state.program, ctx->state.dirty_bits);

    if (!check_draw_modes(mode)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    // ERROR_CHECK_RETURN(first >= 0, GL_INVALID_VALUE);
    if (first < 0) {
        MGL_ERR("MGL Error: mglDrawArrays: first < 0 (%d)\n", first);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    if (count < 0) {
        MGL_ERR("MGL Error: mglDrawArrays: count < 0 (%d)\n", count);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (count == 0) { return; }

    if(validate_vao(ctx, false) == false)
    {
        MGL_ERR("MGL Error: mglDrawArrays: validate_vao failed\n");
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (!validate_program(ctx, mode)) {
        MGL_ERR("MGL Error: mglDrawArrays: validate_program failed\n");
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_FALSE;
    countDraw(ctx, mode, count, 0, NULL, 1);
    ctx->mtl_funcs.mtlDrawArrays(ctx, mode, first, count);
}

void mglDrawElements(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    if (!check_draw_modes(mode)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    // ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    if (count < 0) {
        MGL_ERR("MGL Error: mglDrawElements: count < 0 (%d)\n", count);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (count == 0) { return; }

    if (!check_element_type(type)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    bool staged = beginClientIndices(ctx, count, type, &indices);

    if(validate_vao(ctx, true) == false)
    {
        endClientIndices(ctx, staged);
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (!validate_program(ctx, mode)) { endClientIndices(ctx, staged); ERROR_RETURN(GL_INVALID_OPERATION); return; }

    if (mglConditionalRenderSkips(ctx))
    {
        endClientIndices(ctx, staged);
        return;
    }

    ctx->state.draw_indexed = GL_TRUE;
    countDraw(ctx, mode, count, type, indices, 1);
    ctx->mtl_funcs.mtlDrawElements(ctx, mode, count, type, indices);

    endClientIndices(ctx, staged);
}

void mglDrawRangeElements(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
    if (!check_draw_modes(mode)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    ERROR_CHECK_RETURN(end >= start, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (count == 0) { return; }

    if (!check_element_type(type)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    bool staged = beginClientIndices(ctx, count, type, &indices);

    if(validate_vao(ctx, true) == false)
    {
        endClientIndices(ctx, staged);
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (!validate_program(ctx, mode)) { endClientIndices(ctx, staged); ERROR_RETURN(GL_INVALID_OPERATION); return; }

    if (mglConditionalRenderSkips(ctx))
    {
        endClientIndices(ctx, staged);
        return;
    }

    ctx->state.draw_indexed = GL_TRUE;
    countDraw(ctx, mode, count, type, indices, 1);
    ctx->mtl_funcs.mtlDrawRangeElements(ctx, mode, start, end, count, type, indices);

    endClientIndices(ctx, staged);
}

void mglDrawArraysInstanced(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLsizei instancecount)
{
    if (!check_draw_modes(mode)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    // ERROR_CHECK_RETURN(first >= 0, GL_INVALID_VALUE);
    if (first < 0) {
        MGL_ERR("MGL Error: mglDrawArraysInstanced: first < 0 (%d)\n", first);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    if (count < 0) {
        MGL_ERR("MGL Error: mglDrawArraysInstanced: count < 0 (%d)\n", count);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (count == 0) { return; }

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    if (instancecount == 0) { return; }

    if(validate_vao(ctx, false) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (!validate_program(ctx, mode)) { ERROR_RETURN(GL_INVALID_OPERATION); return; }

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_FALSE;
    countDraw(ctx, mode, count, 0, NULL, instancecount);
    ctx->mtl_funcs.mtlDrawArraysInstanced(ctx, mode, first, count, instancecount);
}

void mglDrawElementsInstanced(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)
{
    if (!check_draw_modes(mode)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (count == 0) { return; }

    if (!check_element_type(type)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    if (instancecount == 0) { return; }

    bool staged = beginClientIndices(ctx, count, type, &indices);

    if(validate_vao(ctx, true) == false)
    {
        endClientIndices(ctx, staged);
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (!validate_program(ctx, mode)) { endClientIndices(ctx, staged); ERROR_RETURN(GL_INVALID_OPERATION); return; }

    if (mglConditionalRenderSkips(ctx))
    {
        endClientIndices(ctx, staged);
        return;
    }

    ctx->state.draw_indexed = GL_TRUE;
    countDraw(ctx, mode, count, type, indices, instancecount);
    ctx->mtl_funcs.mtlDrawElementsInstanced(ctx, mode, count, type, indices, instancecount);

    endClientIndices(ctx, staged);
}

void mglDrawElementsBaseVertex(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    if (count == 0) return;

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    bool staged = beginClientIndices(ctx, count, type, &indices);

    if(validate_vao(ctx, true) == false)
    {
        endClientIndices(ctx, staged);
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!validate_program(ctx, mode)) { endClientIndices(ctx, staged); ERROR_RETURN(GL_INVALID_OPERATION); }

    if (mglConditionalRenderSkips(ctx))
    {
        endClientIndices(ctx, staged);
        return;
    }

    ctx->state.draw_indexed = GL_TRUE;
    countDraw(ctx, mode, count, type, indices, 1);
    ctx->mtl_funcs.mtlDrawElementsBaseVertex(ctx, mode, count, type, indices, basevertex);

    endClientIndices(ctx, staged);
}

void mglDrawRangeElementsBaseVertex(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(end >= start, GL_INVALID_VALUE);

    if (count == 0) { return; }

    bool staged = beginClientIndices(ctx, count, type, &indices);

    if(validate_vao(ctx, true) == false)
    {
        endClientIndices(ctx, staged);
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!validate_program(ctx, mode)) { endClientIndices(ctx, staged); ERROR_RETURN(GL_INVALID_OPERATION); }

    if (mglConditionalRenderSkips(ctx))
    {
        endClientIndices(ctx, staged);
        return;
    }

    ctx->state.draw_indexed = GL_TRUE;
    countDraw(ctx, mode, count, type, indices, 1);
    ctx->mtl_funcs.mtlDrawRangeElementsBaseVertex(ctx, mode, start, end, count, type, indices, basevertex);

    endClientIndices(ctx, staged);
}

void mglDrawElementsInstancedBaseVertex(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    if (count == 0 || instancecount == 0) { return; }

    bool staged = beginClientIndices(ctx, count, type, &indices);

    if(validate_vao(ctx, true) == false)
    {
        endClientIndices(ctx, staged);
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!validate_program(ctx, mode)) { endClientIndices(ctx, staged); ERROR_RETURN(GL_INVALID_OPERATION); }

    if (mglConditionalRenderSkips(ctx))
    {
        endClientIndices(ctx, staged);
        return;
    }

    ctx->state.draw_indexed = GL_TRUE;
    countDraw(ctx, mode, count, type, indices, instancecount);
    ctx->mtl_funcs.mtlDrawElementsInstancedBaseVertex(ctx, mode, count, type, indices, instancecount, basevertex);

    endClientIndices(ctx, staged);
}

// the offset has to be 4-byte aligned and every command has to sit inside
// the buffer, or the GPU reads past its end
static GLenum indirect_error(GLMContext ctx, const void *indirect, GLsizei drawcount,
                             GLsizei stride, size_t cmd_size)
{
    Buffer *buf = STATE(buffers[_DRAW_INDIRECT_BUFFER]);
    intptr_t offset = (intptr_t)indirect;

    if (!buf) return GL_INVALID_OPERATION;
    if (offset < 0 || (offset & 3)) return GL_INVALID_VALUE;

    size_t step = stride ? (size_t)stride : cmd_size;
    size_t end = (size_t)offset + (size_t)(drawcount - 1) * step + cmd_size;
    if (end > (size_t)buf->size) return GL_INVALID_OPERATION;

    return GL_NO_ERROR;
}

void mglDrawArraysIndirect(GLMContext ctx, GLenum mode, const void *indirect)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    GLenum err = indirect_error(ctx, indirect, 1, 0, 4 * sizeof(GLuint));
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if(validate_vao(ctx, false) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_FALSE;
    countIndirect(ctx, mode, 0, indirect, 1, 0);
    ctx->mtl_funcs.mtlDrawArraysIndirect(ctx, mode, indirect);
}

void mglDrawElementsIndirect(GLMContext ctx, GLenum mode, GLenum type, const void *indirect)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    GLenum err = indirect_error(ctx, indirect, 1, 0, 5 * sizeof(GLuint));
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_TRUE;
    countIndirect(ctx, mode, type, indirect, 1, 0);
    ctx->mtl_funcs.mtlDrawElementsIndirect(ctx, mode, type, indirect);
}

void mglDrawArraysInstancedBaseInstance(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance)
{
    ERROR_CHECK_RETURN(first >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    if (count == 0 || instancecount == 0) { return; }

    if(validate_vao(ctx, false) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_FALSE;
    countDraw(ctx, mode, count, 0, NULL, instancecount);
    ctx->mtl_funcs.mtlDrawArraysInstancedBaseInstance(ctx, mode, first, count, instancecount, baseinstance);
}

void mglDrawElementsInstancedBaseInstance(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    if (count == 0 || instancecount == 0) { return; }

    bool staged = beginClientIndices(ctx, count, type, &indices);

    if(validate_vao(ctx, true) == false)
    {
        endClientIndices(ctx, staged);
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!validate_program(ctx, mode)) { endClientIndices(ctx, staged); ERROR_RETURN(GL_INVALID_OPERATION); }

    if (mglConditionalRenderSkips(ctx))
    {
        endClientIndices(ctx, staged);
        return;
    }

    ctx->state.draw_indexed = GL_TRUE;
    countDraw(ctx, mode, count, type, indices, instancecount);
    ctx->mtl_funcs.mtlDrawElementsInstancedBaseInstance(ctx, mode, count, type, indices, instancecount, baseinstance);

    endClientIndices(ctx, staged);
}

void mglDrawElementsInstancedBaseVertexBaseInstance(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    if (count == 0 || instancecount == 0) { return; }

    bool staged = beginClientIndices(ctx, count, type, &indices);

    if(validate_vao(ctx, true) == false)
    {
        endClientIndices(ctx, staged);
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!validate_program(ctx, mode)) { endClientIndices(ctx, staged); ERROR_RETURN(GL_INVALID_OPERATION); }

    if (mglConditionalRenderSkips(ctx))
    {
        endClientIndices(ctx, staged);
        return;
    }

    ctx->state.draw_indexed = GL_TRUE;
    countDraw(ctx, mode, count, type, indices, instancecount);
    ctx->mtl_funcs.mtlDrawElementsInstancedBaseVertexBaseInstance(ctx, mode, count, type, indices, instancecount, basevertex, baseinstance);

    endClientIndices(ctx, staged);
}

// every entry of a multi-draw count array has to be zero or more
static bool counts_are_positive(const GLsizei *count, GLsizei drawcount)
{
    if (drawcount > 0 && count == NULL)
        return false;

    for (GLsizei i = 0; i < drawcount; i++)
        if (count[i] < 0)
            return false;

    return true;
}

void mglMultiDrawArrays(GLMContext ctx, GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(counts_are_positive(count, drawcount), GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(drawcount == 0 || first, GL_INVALID_VALUE);

    if (drawcount == 0) { return; }

    if(validate_vao(ctx, false) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_FALSE;
    for (GLsizei i = 0; i < drawcount; i++)
        countDraw(ctx, mode, count[i], 0, NULL, 1);
    ctx->mtl_funcs.mtlMultiDrawArrays(ctx, mode, first, count, drawcount);
}

void mglMultiDrawElements(GLMContext ctx, GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(counts_are_positive(count, drawcount), GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(drawcount == 0 || indices, GL_INVALID_VALUE);

    if (drawcount == 0) { return; }

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_TRUE;
    for (GLsizei i = 0; i < drawcount; i++)
        countDraw(ctx, mode, count[i], type, indices[i], 1);
    ctx->mtl_funcs.mtlMultiDrawElements(ctx, mode, count, type, indices, drawcount);
}

void mglMultiDrawElementsBaseVertex(GLMContext ctx, GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(counts_are_positive(count, drawcount), GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(drawcount == 0 || indices, GL_INVALID_VALUE);

    if (drawcount == 0) { return; }

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_TRUE;
    for (GLsizei i = 0; i < drawcount; i++)
        countDraw(ctx, mode, count[i], type, indices[i], 1);
    ctx->mtl_funcs.mtlMultiDrawElementsBaseVertex(ctx, mode, count, type, indices, drawcount, basevertex);
}

void mglMultiDrawArraysIndirect(GLMContext ctx, GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(stride % 4 == 0, GL_INVALID_VALUE);

    if (drawcount == 0) { return; }

    if(validate_vao(ctx, false) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    GLenum err = indirect_error(ctx, indirect, drawcount, stride, 4 * sizeof(GLuint));
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_FALSE;
    countIndirect(ctx, mode, 0, indirect, drawcount, stride);
    ctx->mtl_funcs.mtlMultiDrawArraysIndirect(ctx, mode, indirect, drawcount, stride);
}

void mglMultiDrawElementsIndirect(GLMContext ctx, GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(stride % 4 == 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    if (drawcount == 0) { return; }

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    GLenum err = indirect_error(ctx, indirect, drawcount, stride, 5 * sizeof(GLuint));
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (mglConditionalRenderSkips(ctx))
        return;

    ctx->state.draw_indexed = GL_TRUE;
    countIndirect(ctx, mode, type, indirect, drawcount, stride);
    ctx->mtl_funcs.mtlMultiDrawElementsIndirect(ctx, mode, type, indirect, drawcount, stride);
}


// The draw count sits at byte offset drawcount of the buffer bound to
// GL_PARAMETER_BUFFER. It is read back here, waiting for any GPU work that
// writes it, and the draw then runs as an ordinary multi-draw of that many
// commands, never more than maxdrawcount. Returns -1 after raising an error.
static GLsizei indirectDrawCount(GLMContext ctx, GLenum mode, GLintptr drawcount,
                                 GLsizei maxdrawcount, GLsizei stride)
{
    Buffer *param = STATE(buffers[_PARAMETER_BUFFER]);
    GLuint count;

    ERROR_CHECK_RETURN_VALUE(check_draw_modes(mode), GL_INVALID_ENUM, -1);
    ERROR_CHECK_RETURN_VALUE(drawcount >= 0 && (drawcount & 3) == 0, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(maxdrawcount >= 0, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(stride >= 0 && (stride % 4) == 0, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(STATE(buffers[_DRAW_INDIRECT_BUFFER]), GL_INVALID_OPERATION, -1);
    ERROR_CHECK_RETURN_VALUE(param, GL_INVALID_OPERATION, -1);
    ERROR_CHECK_RETURN_VALUE(drawcount + 4 <= param->size, GL_INVALID_OPERATION, -1);
    ERROR_CHECK_RETURN_VALUE(param->mapped == GL_FALSE || (param->access & GL_MAP_PERSISTENT_BIT),
                             GL_INVALID_OPERATION, -1);

    if (maxdrawcount == 0 || param->data.buffer_data == 0)
        return 0;

    ctx->mtl_funcs.mtlFlush(ctx, true);
    memcpy(&count, (const void *)(param->data.buffer_data + drawcount), sizeof count);

    return count < (GLuint)maxdrawcount ? (GLsizei)count : maxdrawcount;
}

void mglMultiDrawArraysIndirectCount(GLMContext ctx, GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    GLsizei n = indirectDrawCount(ctx, mode, drawcount, maxdrawcount, stride);

    if (n > 0)
        mglMultiDrawArraysIndirect(ctx, mode, indirect, n, stride);
}

void mglMultiDrawElementsIndirectCount(GLMContext ctx, GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    GLsizei n;

    ERROR_CHECK_RETURN(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT ||
                       type == GL_UNSIGNED_INT, GL_INVALID_ENUM);

    n = indirectDrawCount(ctx, mode, drawcount, maxdrawcount, stride);

    if (n > 0)
        mglMultiDrawElementsIndirect(ctx, mode, type, indirect, n, stride);
}
