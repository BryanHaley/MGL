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

#include <CoreFoundation/CoreFoundation.h>
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
    ptr->owner = ctx;

    pthread_mutex_lock(&SHARED(lock));
    ptr->name = SHARED(sync_name)++;
    ptr->next = SHARED(sync_list);
    SHARED(sync_list) = ptr;
    pthread_mutex_unlock(&SHARED(lock));

    return ptr;
}

// The only safe test is whether we handed this pointer out and still own it.
// Comparing sync->name would dereference whatever the caller passed.
int isSync(GLMContext ctx, GLsync sync)
{
    int found = 0;

    if (sync == NULL)
        return 0;

    pthread_mutex_lock(&SHARED(lock));
    for (Sync *s = SHARED(sync_list); s && !found; s = s->next)
        found = s == sync;
    pthread_mutex_unlock(&SHARED(lock));

    return found;
}

static void unlinkSync(GLMContext ctx, GLsync sync)
{
    pthread_mutex_lock(&SHARED(lock));
    for (Sync **p = &SHARED(sync_list); *p; p = &(*p)->next)
    {
        if (*p == sync)
        {
            *p = sync->next;
            break;
        }
    }
    pthread_mutex_unlock(&SHARED(lock));
}

// Frees a sync already out of the list. The context that made it may not be
// this one, so every renderer in the group forgets it.
void mglReleaseSync(GLMContext ctx, Sync *sync)
{
    MGLShareGroup *g = ctx->state.share;

    pthread_mutex_lock(&g->lock);
    for (GLuint i = 0; i < g->context_count; i++)
        if (g->contexts[i]->mtl_funcs.mtlForgetSync && g->contexts[i]->mtl_funcs.mtlObj)
            g->contexts[i]->mtl_funcs.mtlForgetSync(g->contexts[i], sync);
    pthread_mutex_unlock(&g->lock);

    // a group's last context has already left it
    if (g->context_count == 0 && ctx->mtl_funcs.mtlForgetSync)
        ctx->mtl_funcs.mtlForgetSync(ctx, sync);

    if (sync->mtl_event)
        CFRelease(sync->mtl_event);

    mtlSyncRelease(sync);
    free(sync);
}

GLsync mglFenceSync(GLMContext ctx, GLenum condition, GLbitfield flags)
{
    Sync *ptr;

    if (condition != GL_SYNC_GPU_COMMANDS_COMPLETE)
    {
        ERROR_RETURN_VALUE(GL_INVALID_ENUM, NULL);
    }

    // must be zero
    if (flags != 0)
    {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, NULL);
    }

    ptr = newSync(ctx);

    if (ptr == NULL)
    {
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

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

    mglForgetObjectLabel(ctx, 0, 0, sync);
    mglReleaseSync(ctx, sync);
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

    if (mtlSyncSignaled(sync))
        return GL_ALREADY_SIGNALED;

    // the flush bit means "behave as if glFlush came first"; a fence this
    // context made needs its own work sent to go off at all
    if ((flags & GL_SYNC_FLUSH_COMMANDS_BIT) || sync->owner == ctx)
        ctx->mtl_funcs.mtlFlush(ctx, false);

    return mtlSyncWait(sync, timeout) ? GL_CONDITION_SATISFIED : GL_TIMEOUT_EXPIRED;
}

void mglWaitSync(GLMContext ctx, GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    if (isSync(ctx, sync) == GL_FALSE)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // the server wait takes no flags and no timeout of its own
    if (flags != 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (timeout != GL_TIMEOUT_IGNORED)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // one queue runs in order, so only another context's fence needs a wait
    if (sync->owner != ctx && !mtlSyncSignaled(sync))
        mtlServerWaitSync(ctx, sync);
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
            v = mtlSyncSignaled(sync) ? GL_SIGNALED : GL_UNSIGNALED;
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

// Rendering before this is visible to texture fetches after it, which in
// Metal means a new render pass
void mglTextureBarrier(GLMContext ctx)
{
    if (ctx->mtl_funcs.mtlMemoryBarrier)
        ctx->mtl_funcs.mtlMemoryBarrier(ctx, GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

#define MGL_ALL_MEMORY_BARRIERS \
    (GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT | \
     GL_UNIFORM_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT | \
     GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_COMMAND_BARRIER_BIT | \
     GL_PIXEL_BUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT | \
     GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT | \
     GL_QUERY_BUFFER_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT | \
     GL_TRANSFORM_FEEDBACK_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT | \
     GL_SHADER_STORAGE_BARRIER_BIT)

// the by-region form takes a smaller set than the full barrier
#define MGL_BY_REGION_BARRIERS \
    (GL_ATOMIC_COUNTER_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT | \
     GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | \
     GL_TEXTURE_FETCH_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT)

void mglMemoryBarrier(GLMContext ctx, GLbitfield barriers)
{
    if (barriers != GL_ALL_BARRIER_BITS && (barriers & ~MGL_ALL_MEMORY_BARRIERS))
    {
        // extra bits...
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (ctx->mtl_funcs.mtlMemoryBarrier)
        ctx->mtl_funcs.mtlMemoryBarrier(ctx, barriers & MGL_ALL_MEMORY_BARRIERS);
}

void mglMemoryBarrierByRegion(GLMContext ctx, GLbitfield barriers)
{
    if (barriers != GL_ALL_BARRIER_BITS && (barriers & ~MGL_BY_REGION_BARRIERS))
    {
        // extra bits...
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // Metal has no by-region variant; the full barrier is a legal superset
    if (ctx->mtl_funcs.mtlMemoryBarrier)
        ctx->mtl_funcs.mtlMemoryBarrier(ctx, barriers & MGL_BY_REGION_BARRIERS);
}

