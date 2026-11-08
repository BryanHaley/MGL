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
 * mesh_shader.c
 * MGL
 *
 * GL_EXT_mesh_shader draws. The task and mesh stages run as Metal's object
 * and mesh functions.
 */

#include <string.h>
#include "glcorearb.h"
#include "glm_context.h"

#define CMD_SIZE (3 * sizeof(GLuint))

static GLenum meshProgramError(GLMContext ctx)
{
    return mglProgramIsMesh(ctx->state.program) ? GL_NO_ERROR : GL_INVALID_OPERATION;
}

static GLenum indirectError(GLMContext ctx, GLintptr indirect, GLsizei drawcount, GLsizei stride)
{
    Buffer *buf = ctx->state.buffers[_DRAW_INDIRECT_BUFFER];
    size_t step = stride ? (size_t)stride : CMD_SIZE;

    if (indirect < 0 || (indirect & 3))
        return GL_INVALID_VALUE;

    if (!buf || (size_t)indirect + (size_t)(drawcount - 1) * step + CMD_SIZE > (size_t)buf->size)
        return GL_INVALID_OPERATION;

    return GL_NO_ERROR;
}

void mglDrawMeshTasks(GLMContext ctx, GLuint x, GLuint y, GLuint z)
{
    bool task = ctx->state.program && ctx->state.program->spirv[_TASK_SHADER].msl_str;
    GLuint total = task ? MGL_MAX_TASK_GROUP_TOTAL : MGL_MAX_MESH_GROUP_TOTAL;

    ERROR_CHECK_RETURN(meshProgramError(ctx) == GL_NO_ERROR, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(x <= MGL_MAX_MESH_GROUP_COUNT && y <= MGL_MAX_MESH_GROUP_COUNT &&
                       z <= MGL_MAX_MESH_GROUP_COUNT, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN((uint64_t)x * y * z <= total, GL_INVALID_VALUE);

    if (x == 0 || y == 0 || z == 0 || mglConditionalRenderSkips(ctx))
        return;

    ctx->mtl_funcs.mtlDrawMeshTasks(ctx, x, y, z, -1, 0);
}

void mglMultiDrawMeshTasksIndirect(GLMContext ctx, GLintptr indirect, GLsizei drawcount, GLsizei stride)
{
    GLenum err;

    ERROR_CHECK_RETURN(meshProgramError(ctx) == GL_NO_ERROR, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(drawcount > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(stride >= 0 && stride % 4 == 0 && (stride == 0 || (size_t)stride >= CMD_SIZE),
                       GL_INVALID_VALUE);

    err = indirectError(ctx, indirect, drawcount, stride);
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (mglConditionalRenderSkips(ctx))
        return;

    for (GLsizei i = 0; i < drawcount; i++)
        ctx->mtl_funcs.mtlDrawMeshTasks(ctx, 0, 0, 0, indirect + (GLintptr)i * (stride ? stride : CMD_SIZE), i);
}

void mglDrawMeshTasksIndirect(GLMContext ctx, GLintptr indirect)
{
    GLenum err;

    ERROR_CHECK_RETURN(meshProgramError(ctx) == GL_NO_ERROR, GL_INVALID_OPERATION);

    err = indirectError(ctx, indirect, 1, 0);
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (!mglConditionalRenderSkips(ctx))
        ctx->mtl_funcs.mtlDrawMeshTasks(ctx, 0, 0, 0, indirect, 0);
}

void mglMultiDrawMeshTasksIndirectCount(GLMContext ctx, GLintptr indirect, GLintptr drawcount,
                                        GLsizei maxdrawcount, GLsizei stride)
{
    Buffer *param = ctx->state.buffers[_PARAMETER_BUFFER];
    GLenum err;
    GLuint count = 0;

    ERROR_CHECK_RETURN(meshProgramError(ctx) == GL_NO_ERROR, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(param, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(drawcount >= 0 && (drawcount & 3) == 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN((size_t)drawcount + sizeof(GLuint) <= (size_t)param->size, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(maxdrawcount > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(stride >= 0 && stride % 4 == 0 && (stride == 0 || (size_t)stride >= CMD_SIZE),
                       GL_INVALID_VALUE);

    err = indirectError(ctx, indirect, maxdrawcount, stride);
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (mglConditionalRenderSkips(ctx) || param->data.buffer_data == 0)
        return;

    ctx->mtl_funcs.mtlFlush(ctx, true);
    memcpy(&count, (const void *)(param->data.buffer_data + drawcount), sizeof count);

    for (GLuint i = 0; i < count && i < (GLuint)maxdrawcount; i++)
        ctx->mtl_funcs.mtlDrawMeshTasks(ctx, 0, 0, 0, indirect + (GLintptr)i * (stride ? stride : CMD_SIZE), i);
}

// GL_NV_mesh_shader: one row of groups from first, and indirect commands of
// { count, first }. Metal reads three counts, so the commands are read here.
#define NV_CMD_SIZE (2 * sizeof(GLuint))

static void drawNv(GLMContext ctx, GLuint first, GLuint count, GLuint draw)
{
    if (count == 0)
        return;

    mglWriteNvMeshFirst(ctx, ctx->state.program, first);
    ctx->mtl_funcs.mtlDrawMeshTasks(ctx, count, 1, 1, -1, draw);
}

static GLenum nvIndirectError(GLMContext ctx, GLintptr indirect, GLsizei drawcount, GLsizei stride)
{
    Buffer *buf = ctx->state.buffers[_DRAW_INDIRECT_BUFFER];
    size_t step = stride ? (size_t)stride : NV_CMD_SIZE;

    if (indirect < 0 || (indirect & 3))
        return GL_INVALID_VALUE;

    if (!buf || (size_t)indirect + (size_t)(drawcount - 1) * step + NV_CMD_SIZE > (size_t)buf->size)
        return GL_INVALID_OPERATION;

    return GL_NO_ERROR;
}

static void drawNvIndirect(GLMContext ctx, GLintptr indirect, GLuint drawcount, GLsizei stride)
{
    Buffer *buf = ctx->state.buffers[_DRAW_INDIRECT_BUFFER];
    size_t step = stride ? (size_t)stride : NV_CMD_SIZE;

    if (buf->data.buffer_data == 0)
        return;

    ctx->mtl_funcs.mtlFlush(ctx, true);

    for (GLuint i = 0; i < drawcount; i++)
    {
        GLuint cmd[2];

        memcpy(cmd, (const void *)(buf->data.buffer_data + indirect + i * step), sizeof cmd);
        if (cmd[0] <= MGL_MAX_MESH_GROUP_COUNT)
            drawNv(ctx, cmd[1], cmd[0], i);
    }
}

void mglDrawMeshTasksNV(GLMContext ctx, GLuint first, GLuint count)
{
    ERROR_CHECK_RETURN(meshProgramError(ctx) == GL_NO_ERROR, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(count <= MGL_MAX_MESH_GROUP_COUNT, GL_INVALID_VALUE);

    if (!mglConditionalRenderSkips(ctx))
        drawNv(ctx, first, count, 0);
}

void mglDrawMeshTasksIndirectNV(GLMContext ctx, GLintptr indirect)
{
    GLenum err;

    ERROR_CHECK_RETURN(meshProgramError(ctx) == GL_NO_ERROR, GL_INVALID_OPERATION);

    err = nvIndirectError(ctx, indirect, 1, 0);
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (!mglConditionalRenderSkips(ctx))
        drawNvIndirect(ctx, indirect, 1, 0);
}

void mglMultiDrawMeshTasksIndirectNV(GLMContext ctx, GLintptr indirect, GLsizei drawcount, GLsizei stride)
{
    GLenum err;

    ERROR_CHECK_RETURN(meshProgramError(ctx) == GL_NO_ERROR, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(drawcount >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(stride >= 0 && stride % 4 == 0 && (stride == 0 || (size_t)stride >= NV_CMD_SIZE),
                       GL_INVALID_VALUE);

    if (drawcount == 0)
        return;

    err = nvIndirectError(ctx, indirect, drawcount, stride);
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (!mglConditionalRenderSkips(ctx))
        drawNvIndirect(ctx, indirect, (GLuint)drawcount, stride);
}

void mglMultiDrawMeshTasksIndirectCountNV(GLMContext ctx, GLintptr indirect, GLintptr drawcount,
                                          GLsizei maxdrawcount, GLsizei stride)
{
    Buffer *param = ctx->state.buffers[_PARAMETER_BUFFER];
    GLenum err;
    GLuint count = 0;

    ERROR_CHECK_RETURN(meshProgramError(ctx) == GL_NO_ERROR, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(param, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(drawcount >= 0 && (drawcount & 3) == 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN((size_t)drawcount + sizeof(GLuint) <= (size_t)param->size, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(maxdrawcount >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(stride >= 0 && stride % 4 == 0 && (stride == 0 || (size_t)stride >= NV_CMD_SIZE),
                       GL_INVALID_VALUE);

    if (maxdrawcount == 0)
        return;

    err = nvIndirectError(ctx, indirect, maxdrawcount, stride);
    ERROR_CHECK_RETURN(err == GL_NO_ERROR, err);

    if (mglConditionalRenderSkips(ctx) || param->data.buffer_data == 0)
        return;

    ctx->mtl_funcs.mtlFlush(ctx, true);
    memcpy(&count, (const void *)(param->data.buffer_data + drawcount), sizeof count);

    drawNvIndirect(ctx, indirect, count < (GLuint)maxdrawcount ? count : (GLuint)maxdrawcount, stride);
}

#ifdef MGL_GL_CORE
extern GLMContext _ctx;
extern void mgl_lazy_init(void);

void glDrawMeshTasksEXT(GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
    mgl_lazy_init();
    mglDrawMeshTasks(_ctx, num_groups_x, num_groups_y, num_groups_z);
}

void glDrawMeshTasksIndirectEXT(GLintptr indirect)
{
    mgl_lazy_init();
    mglDrawMeshTasksIndirect(_ctx, indirect);
}

void glMultiDrawMeshTasksIndirectEXT(GLintptr indirect, GLsizei drawcount, GLsizei stride)
{
    mgl_lazy_init();
    mglMultiDrawMeshTasksIndirect(_ctx, indirect, drawcount, stride);
}

void glMultiDrawMeshTasksIndirectCountEXT(GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    mgl_lazy_init();
    mglMultiDrawMeshTasksIndirectCount(_ctx, indirect, drawcount, maxdrawcount, stride);
}

void glDrawMeshTasksNV(GLuint first, GLuint count)
{
    mgl_lazy_init();
    mglDrawMeshTasksNV(_ctx, first, count);
}

void glDrawMeshTasksIndirectNV(GLintptr indirect)
{
    mgl_lazy_init();
    mglDrawMeshTasksIndirectNV(_ctx, indirect);
}

void glMultiDrawMeshTasksIndirectNV(GLintptr indirect, GLsizei drawcount, GLsizei stride)
{
    mgl_lazy_init();
    mglMultiDrawMeshTasksIndirectNV(_ctx, indirect, drawcount, stride);
}

void glMultiDrawMeshTasksIndirectCountNV(GLintptr indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride)
{
    mgl_lazy_init();
    mglMultiDrawMeshTasksIndirectCountNV(_ctx, indirect, drawcount, maxdrawcount, stride);
}
#endif
