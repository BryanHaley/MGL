/*
 * Copyright (C) The MooGL Project
 *
 * share_group.c
 * MGL
 *
 * Objects that contexts in a share group have in common (GL 4.6 chapter 5).
 * A context that deletes one unbinds it from itself only; while another
 * context still uses it, it is held here and freed once none does.
 */

#include <stdlib.h>

#include "glm_context.h"

void mglReleaseBufferStorage(GLMContext ctx, Buffer *ptr);
void mglFreeTextureObject(GLMContext ctx, Texture *tex);

static bool attachmentHolds(const FBOAttachment *a, GLenum type, const void *obj)
{
    if (type == GL_RENDERBUFFER)
        return a->textarget == GL_RENDERBUFFER && a->buf.rbo == obj;

    return a->textarget != GL_RENDERBUFFER && a->buf.tex == obj;
}

static bool framebuffersHold(GLMContext c, GLenum type, const void *obj)
{
    HashTable *t = &c->state.framebuffer_table;

    for (size_t k = 0; k < t->size; k++)
    {
        Framebuffer *fb = (Framebuffer *)t->keys[k].data;

        if (fb == NULL)
            continue;

        for (int i = 0; i < MAX_COLOR_ATTACHMENTS; i++)
            if (attachmentHolds(&fb->color_attachments[i], type, obj))
                return true;

        if (attachmentHolds(&fb->depth, type, obj) || attachmentHolds(&fb->stencil, type, obj))
            return true;
    }

    return false;
}

static bool vertexArrayHolds(const VertexArray *vao, const Buffer *buf)
{
    if (vao == NULL)
        return false;

    for (int a = 0; a < MAX_BINDABLE_BUFFERS; a++)
        if (vao->bindings[a].buffer == buf)
            return true;

    return vao->element_array.buffer == buf;
}

static bool bufferUsedBy(GLMContext c, const Buffer *buf)
{
    for (int t = 0; t < _MAX_BUFFER_TYPES; t++)
    {
        if (c->state.buffers[t] == buf)
            return true;

        for (int i = 0; i < MAX_BUFFER_BASE_BINDINGS; i++)
            if (c->state.buffer_base[t].buffers[i].buf == buf)
                return true;
    }

    if (vertexArrayHolds(c->state.vao, buf))
        return true;

    for (size_t k = 0; k < c->state.vao_table.size; k++)
        if (vertexArrayHolds((VertexArray *)c->state.vao_table.keys[k].data, buf))
            return true;

    for (size_t k = 0; k < c->state.transform_feedback_table.size; k++)
    {
        TransformFeedback *xfb = (TransformFeedback *)c->state.transform_feedback_table.keys[k].data;

        for (int i = 0; xfb && i < MAX_TF_BUFFERS; i++)
            if (xfb->buffers[i].buf == buf)
                return true;
    }

    return false;
}

static bool textureUsedBy(GLMContext c, const Texture *tex)
{
    for (int u = 0; u < TEXTURE_UNITS; u++)
    {
        if (c->state.active_textures[u] == tex || c->state.image_units[u].tex == tex)
            return true;

        for (int k = 0; k < _MAX_TEXTURE_TYPES; k++)
            if (c->state.texture_units[u].textures[k] == tex)
                return true;
    }

    return framebuffersHold(c, GL_TEXTURE, tex);
}

static bool usedBy(GLMContext c, GLenum type, const void *obj)
{
    switch (type)
    {
        case GL_BUFFER:
            return bufferUsedBy(c, (const Buffer *)obj);

        case GL_TEXTURE:
            return textureUsedBy(c, (const Texture *)obj);

        case GL_SAMPLER:
            for (int u = 0; u < TEXTURE_UNITS; u++)
                if (c->state.texture_samplers[u] == obj)
                    return true;
            return false;

        case GL_RENDERBUFFER:
            return c->state.renderbuffer == obj || framebuffersHold(c, GL_RENDERBUFFER, obj);
    }

    return false;
}

static void release(GLMContext ctx, GLenum type, void *obj)
{
    switch (type)
    {
        case GL_BUFFER:
            mglReleaseBufferStorage(ctx, (Buffer *)obj);
            free(obj);
            break;

        case GL_TEXTURE:
            mglFreeTextureObject(ctx, (Texture *)obj);
            break;

        case GL_SAMPLER:
        {
            Sampler *smp = (Sampler *)obj;

            if (smp->mtl_data)
                ctx->mtl_funcs.mtlDeleteMTLObj(ctx, smp->mtl_data);
            free(smp);
            break;
        }

        case GL_RENDERBUFFER:
        {
            Renderbuffer *rbo = (Renderbuffer *)obj;

            if (rbo->tex)
                mglFreeTextureObject(ctx, rbo->tex);
            free(rbo);
            break;
        }
    }
}

// Call once ctx has unbound obj itself. True when another context in the
// group still uses it: it is held, and the caller must not free it.
bool mglHoldIfUsedElsewhere(GLMContext ctx, GLenum type, void *obj)
{
    MGLShareGroup *g = ctx->state.share;
    bool used = false;

    if (g->context_count < 2)
        return false;

    pthread_mutex_lock(&g->lock);

    for (GLuint i = 0; i < g->context_count && !used; i++)
        if (g->contexts[i] != ctx)
            used = usedBy(g->contexts[i], type, obj);

    if (used)
    {
        __typeof__(g->held) grown = realloc(g->held, (g->held_count + 1) * sizeof(*g->held));

        // with nowhere to note it, keeping it is safer than freeing it
        if (grown)
        {
            g->held = grown;
            g->held[g->held_count].type = type;
            g->held[g->held_count].obj = obj;
            g->held_count++;
        }
    }

    pthread_mutex_unlock(&g->lock);

    return used;
}

// Frees what no context uses any more
void mglSweepHeldObjects(GLMContext ctx)
{
    MGLShareGroup *g = ctx->state.share;
    GLuint kept = 0;

    if (g->held_count == 0)
        return;

    pthread_mutex_lock(&g->lock);

    for (GLuint i = 0; i < g->held_count; i++)
    {
        bool used = false;

        for (GLuint c = 0; c < g->context_count && !used; c++)
            used = usedBy(g->contexts[c], g->held[i].type, g->held[i].obj);

        if (used)
            g->held[kept++] = g->held[i];
        else
            release(ctx, g->held[i].type, g->held[i].obj);
    }

    g->held_count = kept;
    pthread_mutex_unlock(&g->lock);
}

// The last context of a group is going
void mglFreeHeldObjects(GLMContext ctx)
{
    MGLShareGroup *g = ctx->state.share;

    for (GLuint i = 0; i < g->held_count; i++)
        release(ctx, g->held[i].type, g->held[i].obj);

    free(g->held);
    g->held = NULL;
    g->held_count = 0;
}
