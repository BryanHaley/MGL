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
 * fence.c
 * MGL
 *
 */

#include <strings.h>

#include "glm_context.h"
#include "mgl_log.h"

Sync *newSync(GLMContext ctx)
{
    Sync *ptr;

    ptr = (Sync *)malloc(sizeof(Sync));
    // CRITICAL SECURITY FIX: Check malloc result instead of using assert()
    if (!ptr) {
        MGL_ERR("MGL SECURITY ERROR: Failed to allocate memory for Sync\n");
        return NULL;
    }

    bzero(ptr, sizeof(Sync));

    ptr->name = STATE(sync_name)++;

    ptr->next = STATE(sync_list);
    STATE(sync_list) = ptr;

    return ptr;
}

// The only safe test is whether we handed this pointer out and still own it.
// Comparing sync->name would dereference whatever the caller passed.
int isSync(GLMContext ctx, GLsync sync)
{
    if (sync == NULL)
        return 0;

    for (Sync *s = STATE(sync_list); s; s = s->next)
        if (s == sync)
            return 1;

    return 0;
}

static void unlinkSync(GLMContext ctx, GLsync sync)
{
    for (Sync **p = &STATE(sync_list); *p; p = &(*p)->next)
    {
        if (*p == sync)
        {
            *p = sync->next;
            return;
        }
    }
}

GLsync mglFenceSync(GLMContext ctx, GLenum condition, GLbitfield flags)
{
    Sync *ptr;

    switch(condition)
    {
        case GL_SYNC_GPU_COMMANDS_COMPLETE:
            break;

        default:
            // CRITICAL FIX: Handle unknown fence conditions gracefully instead of crashing
            MGL_ERR("MGL ERROR: Unknown fence sync condition 0x%x, defaulting to GPU_COMMANDS_COMPLETE\n", condition);
            condition = GL_SYNC_GPU_COMMANDS_COMPLETE;
            break;
    }

    // must be zero
    if (flags != 0) {
        // CRITICAL FIX: Handle invalid flags gracefully instead of crashing
        MGL_ERR("MGL ERROR: Fence sync flags must be zero, got 0x%x, continuing with zero\n", flags);
        flags = 0;
    }

    ptr = newSync(ctx);

    ctx->mtl_funcs.mtlGetSync(ctx, ptr);

    return ptr;
}


GLboolean mglIsSync(GLMContext ctx, GLsync sync)
{
    if (sync == NULL)
    {
        return false;
    }

    return isSync(ctx, sync);
}

void mglDeleteSync(GLMContext ctx, GLsync sync)
{
    // deleting a zero sync is a documented no-op
    if (sync == NULL)
        return;

    if (isSync(ctx, sync) == GL_FALSE)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    unlinkSync(ctx, sync);

    if (sync->mtl_event)
    {
        ctx->mtl_funcs.mtlWaitForSync(ctx, sync);

        // should be null - but handle gracefully if not
        if (sync->mtl_event != NULL) {
            MGL_ERR("MGL WARNING: sync->mtl_event should be NULL after wait, but is %p\n", sync->mtl_event);
        }
    }

    free(sync);
}

GLenum  mglClientWaitSync(GLMContext ctx, GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    if (flags & ~GL_SYNC_FLUSH_COMMANDS_BIT)
    {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, GL_WAIT_FAILED);
    }

    if (isSync(ctx, sync) == GL_FALSE)
    {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, GL_WAIT_FAILED);
    }

    if (sync->mtl_event == NULL)
    {
        return GL_ALREADY_SIGNALED;
    }

    ctx->mtl_funcs.mtlWaitForSync(ctx, sync);

    return GL_CONDITION_SATISFIED;
}

void mglWaitSync(GLMContext ctx, GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    if (isSync(ctx, sync) == GL_FALSE)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (timeout != GL_TIMEOUT_IGNORED) {
        // CRITICAL FIX: Handle invalid timeout gracefully instead of crashing
        MGL_ERR("MGL ERROR: Server wait sync timeout must be GL_TIMEOUT_IGNORED, got 0x%llx\n", timeout);
        // Continue with GL_TIMEOUT_IGNORED behavior
    }

    ctx->mtl_funcs.mtlWaitForSync(ctx, sync);

    // Handle gracefully if event is not null after wait
    if (sync->mtl_event != NULL) {
        MGL_ERR("MGL WARNING: sync->mtl_event should be NULL after server wait, but is %p\n", sync->mtl_event);
    }
}

void mglGetSynciv(GLMContext ctx, GLsync sync, GLenum pname, GLsizei count, GLsizei *length, GLint *values)
{
    if (isSync(ctx, sync) == GL_FALSE)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // count is the size of the caller's buffer, not a number of values to write.
    // Every pname here yields exactly one value.
    if (count < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    GLint v;

    switch(pname)
    {
        case GL_OBJECT_TYPE:
            v = GL_SYNC_FENCE;
            break;

        case GL_SYNC_STATUS:
            v = sync->mtl_event ? GL_UNSIGNALED : GL_SIGNALED;
            break;

        case GL_SYNC_CONDITION:
            v = GL_SYNC_GPU_COMMANDS_COMPLETE;
            break;

        case GL_SYNC_FLAGS:
            v = 0;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    if (count > 0 && values)
    {
        *values = v;

        if (length) *length = 1;
    }
    else if (length)
    {
        *length = 0;
    }
}

void mglTextureBarrier(GLMContext ctx)
{
    // CRITICAL FIX: Handle unimplemented function gracefully instead of crashing
    MGL_ERR("MGL WARNING: mglTextureBarrier is not yet implemented in MGL\n");
    // No-op implementation - this is optional functionality
}

void mglMemoryBarrier(GLMContext ctx, GLbitfield barriers)
{
    if (barriers & ~(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT |  GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | GL_PIXEL_BUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT | GL_TRANSFORM_FEEDBACK_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT))
    {
        // extra bits...
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (ctx->mtl_funcs.mtlMemoryBarrier)
        ctx->mtl_funcs.mtlMemoryBarrier(ctx, barriers);
}

void mglMemoryBarrierByRegion(GLMContext ctx, GLbitfield barriers)
{

    if (barriers & ~(GL_ATOMIC_COUNTER_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT))
    {
        // extra bits...
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // Metal has no by-region variant; the full barrier is a legal superset
    if (ctx->mtl_funcs.mtlMemoryBarrier)
        ctx->mtl_funcs.mtlMemoryBarrier(ctx, barriers);
}

