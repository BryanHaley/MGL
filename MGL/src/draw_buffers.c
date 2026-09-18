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

#include "glm_context.h"
#include "mgl_log.h"

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
                if (vao->attrib[i].buffer == NULL)
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
            if (VAO_ATTRIB_STATE(i).buffer->mapped) {
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

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (!validate_program(ctx, mode)) { ERROR_RETURN(GL_INVALID_OPERATION); return; }

    ctx->mtl_funcs.mtlDrawElements(ctx, mode, count, type, indices);
}

void mglDrawRangeElements(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
    if (!check_draw_modes(mode)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    ERROR_CHECK_RETURN(end >= start, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (count == 0) { return; }

    if (!check_element_type(type)) { ERROR_RETURN(GL_INVALID_ENUM); return; }

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (!validate_program(ctx, mode)) { ERROR_RETURN(GL_INVALID_OPERATION); return; }

    ctx->mtl_funcs.mtlDrawRangeElements(ctx, mode, start, end, count, type, indices);
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

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    if (!validate_program(ctx, mode)) { ERROR_RETURN(GL_INVALID_OPERATION); return; }

    ctx->mtl_funcs.mtlDrawElementsInstanced(ctx, mode, count, type, indices, instancecount);
}

void mglDrawElementsBaseVertex(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    if (count == 0) return;

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlDrawElementsBaseVertex(ctx, mode, count, type, indices, basevertex);
}

void mglDrawRangeElementsBaseVertex(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(end >= start, GL_INVALID_VALUE);

    if (count == 0) { return; }

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlDrawRangeElementsBaseVertex(ctx, mode, start, end, count, type, indices, basevertex);
}

void mglDrawElementsInstancedBaseVertex(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    if (count == 0 || instancecount == 0) { return; }

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlDrawElementsInstancedBaseVertex(ctx, mode, count, type, indices, instancecount, basevertex);
}

void mglDrawArraysIndirect(GLMContext ctx, GLenum mode, const void *indirect)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(STATE(buffers[_DRAW_INDIRECT_BUFFER]), GL_INVALID_OPERATION);

    if(validate_vao(ctx, false) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

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

    ERROR_CHECK_RETURN(STATE(buffers[_DRAW_INDIRECT_BUFFER]), GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlDrawArraysIndirect(ctx, mode, indirect);
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

    ctx->mtl_funcs.mtlDrawArraysInstancedBaseInstance(ctx, mode, first, count, instancecount, baseinstance);
}

void mglDrawElementsInstancedBaseInstance(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    if (count == 0 || instancecount == 0) { return; }

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlDrawElementsInstancedBaseInstance(ctx, mode, count, type, indices, instancecount, baseinstance);
}

void mglDrawElementsInstancedBaseVertexBaseInstance(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(check_element_type(type), GL_INVALID_ENUM);

    if (count == 0 || instancecount == 0) { return; }

    if(validate_vao(ctx, true) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(validate_program(ctx, mode), GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlDrawElementsInstancedBaseVertexBaseInstance(ctx, mode, count, type, indices, instancecount, basevertex, baseinstance);
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

    ERROR_CHECK_RETURN(STATE(buffers[_DRAW_INDIRECT_BUFFER]), GL_INVALID_OPERATION);

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

    ERROR_CHECK_RETURN(STATE(buffers[_DRAW_INDIRECT_BUFFER]), GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlMultiDrawElementsIndirect(ctx, mode, type, indirect, drawcount, stride);
}


// The draw count lives in a buffer bound to GL_PARAMETER_BUFFER, which MGL has
// no target for yet, so these validate and then report that they cannot run.
static void multiDrawIndirectCount(GLMContext ctx, GLenum mode, GLintptr drawcount,
                                   GLsizei maxdrawcount, GLsizei stride)
{
    ERROR_CHECK_RETURN(check_draw_modes(mode), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN((drawcount & 3) == 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(maxdrawcount >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(stride >= 0 && (stride % 4) == 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(STATE(buffers[_DRAW_INDIRECT_BUFFER]), GL_INVALID_OPERATION);

    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglMultiDrawArraysIndirectCount(GLMContext ctx, GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    multiDrawIndirectCount(ctx, mode, drawcount, maxdrawcount, stride);
}

void mglMultiDrawElementsIndirectCount(GLMContext ctx, GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    ERROR_CHECK_RETURN(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT ||
                       type == GL_UNSIGNED_INT, GL_INVALID_ENUM);

    multiDrawIndirectCount(ctx, mode, drawcount, maxdrawcount, stride);
}
