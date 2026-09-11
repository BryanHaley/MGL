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
 * framebuffers.c
 * MGL
 *
 */

#include <stdio.h>
#include <string.h>

#include "glm_context.h"
#include "pixel_utils.h"
#include "utils.h"
#include "mgl_log.h"

#define RENDBUF_STATE(_val_)    ctx->state.renderbuffer->_val_

extern GLuint textureIndexFromTarget(GLMContext ctx, GLenum target);
extern Texture *newTexObj(GLMContext ctx, GLenum target);
extern Texture *findTexture(GLMContext ctx, GLuint texture);


#pragma mark renderbuffer logic

static Renderbuffer *newRenderbuffer(GLMContext ctx, GLuint renderbuffer)
{
    Renderbuffer *ptr;

    ptr = (Renderbuffer *)malloc(sizeof(Renderbuffer));
    assert(ptr);

    bzero(ptr, sizeof(Renderbuffer));

    ptr->name = renderbuffer;

    return ptr;
}

static Renderbuffer *getRenderbuffer(GLMContext ctx, GLuint renderbuffer)
{
    Renderbuffer *ptr;

    ptr = (Renderbuffer *)searchHashTable(&STATE(renderbuffer_table), renderbuffer);

    if (!ptr)
    {
        ptr = newRenderbuffer(ctx, renderbuffer);

        insertHashElement(&STATE(renderbuffer_table), renderbuffer, ptr);
    }

    return ptr;
}

static int isRenderBuffer(GLMContext ctx, GLuint renderbuffer)
{
    Renderbuffer *ptr;

    ptr = (Renderbuffer *)searchHashTable(&STATE(renderbuffer_table), renderbuffer);

    if (ptr)
        return 1;

    return 0;
}

Renderbuffer *findRenderbuffer(GLMContext ctx, GLuint renderbuffer)
{
    Renderbuffer *ptr;

    ptr = (Renderbuffer *)searchHashTable(&STATE(renderbuffer_table), renderbuffer);

    return ptr;
}

Framebuffer *currentFBOForType(GLMContext ctx, GLenum target)
{
    switch(target)
    {
        case GL_FRAMEBUFFER:
        case GL_DRAW_FRAMEBUFFER:
            return ctx->state.framebuffer;
            break;

        case GL_READ_FRAMEBUFFER:
            return ctx->state.readbuffer;
            break;

        default: ERROR_RETURN_VALUE(GL_INVALID_ENUM, NULL);
    }
}

#pragma mark framebuffer logic
static Framebuffer *newFramebuffer(GLMContext ctx, GLuint framebuffer)
{
    Framebuffer *ptr;

    ptr = (Framebuffer *)malloc(sizeof(Framebuffer));
    assert(ptr);

    bzero(ptr, sizeof(Framebuffer));

    ptr->name = framebuffer;
    ptr->draw_buffer = GL_COLOR_ATTACHMENT0;

    return ptr;
}

static Framebuffer *getFramebuffer(GLMContext ctx, GLuint framebuffer)
{
    Framebuffer *ptr;

    ptr = (Framebuffer *)searchHashTable(&STATE(framebuffer_table), framebuffer);

    if (!ptr)
    {
        ptr = newFramebuffer(ctx, framebuffer);

        insertHashElement(&STATE(framebuffer_table), framebuffer, ptr);
    }

    return ptr;
}

static int isFramebuffer(GLMContext ctx, GLuint framebuffer)
{
    Framebuffer *ptr;

    ptr = (Framebuffer *)searchHashTable(&STATE(framebuffer_table), framebuffer);

    if (ptr)
        return 1;

    return 0;
}

Framebuffer *findFrameBuffer(GLMContext ctx, GLuint framebuffer)
{
    Framebuffer *ptr;

    ptr = (Framebuffer *)searchHashTable(&STATE(framebuffer_table), framebuffer);

    return ptr;
}

#pragma mark Framebuffer calls
GLboolean mglIsFramebuffer(GLMContext ctx, GLuint framebuffer)
{
    return isFramebuffer(ctx, framebuffer);
}

void mglGenFramebuffers(GLMContext ctx, GLsizei n, GLuint *framebuffers)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    assert(framebuffers);

    while(n--)
    {
        *framebuffers++ = getNewName(&STATE(framebuffer_table));
    }
}

void mglBindFramebuffer(GLMContext ctx, GLenum target, GLuint framebuffer)
{
    Framebuffer *ptr;

    if(framebuffer)
    {
        ptr = getFramebuffer(ctx, framebuffer);
        MGL_INFO("MGL: glBindFramebuffer target=%x fbo=%u ptr=%p\n", target, framebuffer, ptr);
    }
    else
    {
        ptr = NULL;
        MGL_INFO("MGL: glBindFramebuffer target=%x fbo=0 (default framebuffer)\n", target);
    }

    switch(target) {
        case GL_DRAW_FRAMEBUFFER:
            ctx->state.framebuffer = ptr;
            break;

        case GL_READ_FRAMEBUFFER:
            ctx->state.readbuffer = ptr;
            break;

        case GL_FRAMEBUFFER:
            ctx->state.framebuffer = ptr;
            ctx->state.readbuffer = ptr;
            break;
    }
    
    // the draw buffer belongs to the framebuffer, so swap in the new one's
    if (ctx->state.framebuffer)
        STATE(draw_buffer) = ctx->state.framebuffer->draw_buffer;
    else
        STATE(draw_buffer) = STATE(default_draw_buffer);

    STATE(dirty_bits) |= DIRTY_FBO;

    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
        STATE(var.draw_framebuffer_binding) = framebuffer;

    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)
        STATE(var.read_framebuffer_binding) = framebuffer;
}

void mglDeleteFramebuffers(GLMContext ctx, GLsizei n, const GLuint *framebuffers)
{
    for (GLsizei i = 0; i < n; i++)
    {
        if (framebuffers[i] == 0)
            continue;
            
        Framebuffer *fbo = findFrameBuffer(ctx, framebuffers[i]);
        if (!fbo)
            continue;
            
        // Unbind if currently bound
        if (ctx->state.framebuffer == fbo)
            ctx->state.framebuffer = NULL;
        if (ctx->state.readbuffer == fbo)
            ctx->state.readbuffer = NULL;
            
        // Remove from hash table
        deleteHashElement(&STATE(framebuffer_table), framebuffers[i]);
        
        // Free the framebuffer
        free(fbo);
    }
    
    STATE(dirty_bits) |= DIRTY_FBO;
}

GLenum  mglCheckFramebufferStatus(GLMContext ctx, GLenum target)
{
    Framebuffer *fbo;
    Texture *tex;
    GLuint level;
    GLuint width, height;

    fbo = currentFBOForType(ctx, target);

    // the default framebuffer is always complete; a bad target returns 0
    if (!fbo)
        return (ctx->state.error == GL_INVALID_ENUM) ? 0 : GL_FRAMEBUFFER_COMPLETE;

    if (fbo->color_attachments[0].textarget == GL_RENDERBUFFER)
    {
        tex = fbo->color_attachments[0].buf.rbo->tex;
    }
    else
    {
        tex = fbo->color_attachments[0].buf.tex;
    }

    level = fbo->color_attachments[0].level;
    width = tex->faces[0].levels[level].width;
    height = tex->faces[0].levels[level].height;

    for(int i=1; i<STATE(max_color_attachments);i++)
    {
        if (fbo->color_attachments[i].textarget == GL_RENDERBUFFER)
        {
            tex = fbo->color_attachments[i].buf.rbo->tex;
        }
        else
        {
            tex = fbo->color_attachments[i].buf.tex;
        }

        if (tex)
        {
            level = fbo->color_attachments[i].level;
            width = tex->faces[0].levels[level].width;
            height = tex->faces[0].levels[level].height;
        }
    }

    DEBUG_PRINT("%s need to fix this function %d, %d, %d\n", __FUNCTION__, width, height, level);

    return GL_FRAMEBUFFER_COMPLETE;
}

#pragma mark Renderbuffer calls
GLboolean mglIsRenderbuffer(GLMContext ctx, GLuint renderbuffer)
{
    return isRenderBuffer(ctx, renderbuffer);
}

void mglGenRenderbuffers(GLMContext ctx, GLsizei n, GLuint *renderbuffers)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    assert(renderbuffers);

    while(n--)
    {
        *renderbuffers++ = getNewName(&STATE(renderbuffer_table));
    }
}

void mglBindRenderbuffer(GLMContext ctx, GLenum target, GLuint renderbuffer)
{
    Renderbuffer    *ptr;

    // if (ctx->state.framebuffer == NULL)
    // {
    //     // no fbo bound..
    //     assert(0);
    // }

    ERROR_CHECK_RETURN(target == GL_RENDERBUFFER, GL_INVALID_ENUM);

    if (renderbuffer)
    {
        ptr = getRenderbuffer(ctx, renderbuffer);

        ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    }
    else
    {
        ptr = NULL;
    }

    ctx->state.renderbuffer = ptr;
    // no dirty state
}

static void detachRenderbuffer(Framebuffer *fbo, Renderbuffer *rbo)
{
    FBOAttachment *points[MAX_COLOR_ATTACHMENTS + 2];
    int count = 0;

    for (int i = 0; i < MAX_COLOR_ATTACHMENTS; i++)
        points[count++] = &fbo->color_attachments[i];

    points[count++] = &fbo->depth;
    points[count++] = &fbo->stencil;

    for (int i = 0; i < count; i++)
    {
        FBOAttachment *a = points[i];

        if (a->textarget == GL_RENDERBUFFER && a->buf.rbo == rbo)
        {
            a->buf.rbo = NULL;
            a->textarget = 0;
            a->texture = 0;
            a->dirty_bits |= DIRTY_FBO_BINDING;
        }
    }

    fbo->dirty_bits |= DIRTY_FBO_BINDING;
}

void mglDeleteRenderbuffers(GLMContext ctx, GLsizei n, const GLuint *renderbuffers)
{
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    if (n == 0)
        return;

    ERROR_CHECK_RETURN(renderbuffers, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < n; i++)
    {
        Renderbuffer *rbo;

        // GL ignores 0 and any name that was never generated
        if (renderbuffers[i] == 0)
            continue;

        rbo = findRenderbuffer(ctx, renderbuffers[i]);

        if (!rbo)
            continue;

        if (ctx->state.renderbuffer == rbo)
            ctx->state.renderbuffer = NULL;

        // a deleted renderbuffer detaches itself from the bound framebuffers
        if (ctx->state.framebuffer)
            detachRenderbuffer(ctx->state.framebuffer, rbo);

        if (ctx->state.readbuffer && ctx->state.readbuffer != ctx->state.framebuffer)
            detachRenderbuffer(ctx->state.readbuffer, rbo);

        if (rbo->tex)
        {
            if (rbo->tex->mtl_data)
            {
                ctx->mtl_funcs.mtlDeleteMTLObj(ctx, rbo->tex->mtl_data);
                rbo->tex->mtl_data = NULL;
            }

            free(rbo->tex);
            rbo->tex = NULL;
        }

        deleteHashElement(&STATE(renderbuffer_table), renderbuffers[i]);

        free(rbo);
    }

    STATE(dirty_bits) |= DIRTY_FBO;
}

static void renderbufferStorage(GLMContext ctx, Renderbuffer *rbo, GLsizei samples,
                                GLenum internalformat, GLsizei width, GLsizei height)
{
    Texture *tex;

    ERROR_CHECK_RETURN(rbo, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(samples >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width <= (GLsizei)STATE(var.max_renderbuffer_size), GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height <= (GLsizei)STATE(var.max_renderbuffer_size), GL_INVALID_VALUE);

    // re-specifying replaces the old image
    if (rbo->tex)
    {
        if (rbo->tex->mtl_data)
        {
            ctx->mtl_funcs.mtlDeleteMTLObj(ctx, rbo->tex->mtl_data);
            rbo->tex->mtl_data = NULL;
        }

        free(rbo->tex);
        rbo->tex = NULL;
    }

    tex = newTexObj(ctx, GL_RENDERBUFFER);

    ERROR_CHECK_RETURN(tex, GL_OUT_OF_MEMORY);

    createTextureLevel(ctx, tex, 0, 0, false, internalformat, width, height, 1, 0, 0, NULL, false);

    tex->access = GL_READ_WRITE;
    tex->is_render_target = true;
    tex->samples = samples;

    rbo->tex = tex;
    rbo->dirty_bits |= DIRTY_FBO_BINDING;

    STATE(dirty_bits) |= DIRTY_FBO;
}

void mglRenderbufferStorage(GLMContext ctx, GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    ERROR_CHECK_RETURN(target == GL_RENDERBUFFER, GL_INVALID_ENUM);

    renderbufferStorage(ctx, ctx->state.renderbuffer, 0, internalformat, width, height);
}

static void getRenderbufferParameter(GLMContext ctx, Renderbuffer *rbo, GLenum pname, GLint *params)
{
    Texture *tex;

    ERROR_CHECK_RETURN(rbo, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    tex = rbo->tex;

    // legal to query before storage is given; the image is just 0x0
    if (!tex)
    {
        switch(pname)
        {
            case GL_RENDERBUFFER_WIDTH:
            case GL_RENDERBUFFER_HEIGHT:
            case GL_RENDERBUFFER_SAMPLES:
                *params = 0;
                return;

            case GL_RENDERBUFFER_INTERNAL_FORMAT:
                *params = GL_RGBA;
                return;

            case GL_RENDERBUFFER_RED_SIZE:
            case GL_RENDERBUFFER_GREEN_SIZE:
            case GL_RENDERBUFFER_BLUE_SIZE:
            case GL_RENDERBUFFER_ALPHA_SIZE:
            case GL_RENDERBUFFER_DEPTH_SIZE:
            case GL_RENDERBUFFER_STENCIL_SIZE:
                *params = 0;
                return;

            default:
                ERROR_RETURN(GL_INVALID_ENUM);
        }
    }

    switch(pname)
    {
        case GL_RENDERBUFFER_WIDTH:
            *params = tex->width; return;

        case GL_RENDERBUFFER_HEIGHT:
            *params = tex->height; return;

        case GL_RENDERBUFFER_INTERNAL_FORMAT:
            *params = tex->internalformat; return;

        case GL_RENDERBUFFER_SAMPLES:
            *params = tex->samples; return;

        case GL_RENDERBUFFER_RED_SIZE:
            *params = bitcountForInternalFormat(tex->internalformat, GL_RED); return;

        case GL_RENDERBUFFER_GREEN_SIZE:
            *params = bitcountForInternalFormat(tex->internalformat, GL_GREEN); return;

        case GL_RENDERBUFFER_BLUE_SIZE:
            *params = bitcountForInternalFormat(tex->internalformat, GL_BLUE); return;

        case GL_RENDERBUFFER_ALPHA_SIZE:
            *params = bitcountForInternalFormat(tex->internalformat, GL_ALPHA); return;

        case GL_RENDERBUFFER_DEPTH_SIZE:
            *params = bitcountForInternalFormat(tex->internalformat, GL_DEPTH_COMPONENT); return;

        case GL_RENDERBUFFER_STENCIL_SIZE:
            *params = bitcountForInternalFormat(tex->internalformat, GL_STENCIL_INDEX); return;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetRenderbufferParameteriv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    ERROR_CHECK_RETURN(target == GL_RENDERBUFFER, GL_INVALID_ENUM);

    getRenderbufferParameter(ctx, ctx->state.renderbuffer, pname, params);
}


#pragma mark Framebuffer Texture Bind calls
FBOAttachment *getFBOAttachment(GLMContext ctx, Framebuffer *fbo, GLenum attachment)
{
    switch(attachment)
    {
        case GL_DEPTH_ATTACHMENT:
        case GL_DEPTH_STENCIL_ATTACHMENT:
            return &fbo->depth;
            break;

        case GL_STENCIL_ATTACHMENT:
            return &fbo->stencil;
            break;

        default:
            attachment = attachment - GL_COLOR_ATTACHMENT0;

            // anything that isn't a real colour attachment lands here too,
            // so bound it rather than indexing off the end of the array
            if (attachment >= MAX_COLOR_ATTACHMENTS)
                return NULL;

            return &fbo->color_attachments[attachment];
            break;
    }
}

bool isColorAttachment(GLMContext ctx, GLuint attachment)
{
    return ((attachment >= GL_COLOR_ATTACHMENT0) &&
            (attachment <= (GL_COLOR_ATTACHMENT0 + STATE(max_color_attachments))));
}

bool isCubeMapTarget(GLMContext ctx, GLuint textarget)
{
    return ((textarget >= GL_TEXTURE_CUBE_MAP_POSITIVE_X) &&
            (textarget <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z));
}

void framebufferTexture(GLMContext ctx, GLenum target, GLenum attachment_type, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer)
{
    Framebuffer *fbo;
    Texture *tex;
    FBOAttachment *fbo_attachment_ptr;

    fbo = currentFBOForType(ctx, target);

    // NULL means a bad target, or the default framebuffer, which has no attachments
    ERROR_CHECK_RETURN(fbo, GL_INVALID_OPERATION);
    
    // Log FBO texture attachments for large textures (framebuffer size)
    if (texture != 0) {
        Texture *t = findTexture(ctx, texture);
        if (t && t->width >= 640 && t->height >= 400) {
            MGL_INFO("MGL DEBUG: FBO attach tex %u (%dx%d) to FBO %u attachment 0x%x\n",
                    texture, t->width, t->height, fbo ? fbo->name : 0, attachment);
        }
    }

    switch(attachment)
    {
        case GL_DEPTH_ATTACHMENT:
        case GL_STENCIL_ATTACHMENT:
        case GL_DEPTH_STENCIL_ATTACHMENT:
            break;

        default:
            if (isColorAttachment(ctx, attachment))
            {
                GLuint index;

                index = attachment - GL_COLOR_ATTACHMENT0;
                if (texture)
                {
                    fbo->color_attachment_bitfield |= (0x1 << index);
                }
                else
                {
                    fbo->color_attachment_bitfield &= ~(0x1 << index);
                }
                break;
            }

            ERROR_RETURN(GL_INVALID_ENUM);
    }

    if (texture)
    {
        tex = findTexture(ctx, texture);

        ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

        // glFramebufferTexture has no textarget, so take the texture's own
        if (textarget == GL_NONE)
            textarget = tex->target;

        switch(textarget)
        {
            case GL_TEXTURE_BUFFER:
            case GL_TEXTURE_1D:
            case GL_TEXTURE_2D:
            case GL_TEXTURE_3D:
            case GL_TEXTURE_RECTANGLE:
            case GL_TEXTURE_1D_ARRAY:
            case GL_TEXTURE_2D_ARRAY:
            case GL_TEXTURE_2D_MULTISAMPLE:
            case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
            case GL_TEXTURE_CUBE_MAP:
            case GL_TEXTURE_CUBE_MAP_ARRAY:
                break;

            default:
                if (tex->target == GL_TEXTURE_CUBE_MAP)
                {
                    if (isCubeMapTarget(ctx, textarget))
                    {
                        break;
                    }
                }

                STATE(error) = GL_INVALID_OPERATION;
                return;

                break;
        }

        if (level < 0)
        {
            STATE(error) = GL_INVALID_VALUE;
            return;
        }

        // A texture may legally be attached before it has storage allocated; that should
        // make the FBO incomplete, not raise GL_INVALID_VALUE.
        if (tex->mipmap_levels != 0 && level >= (GLint)tex->mipmap_levels)
        {
            STATE(error) = GL_INVALID_VALUE;
            return;
        }

        if (level > 0)
        {
            switch(textarget)
            {
                // If textarget is GL_TEXTURE_RECTANGLE, GL_TEXTURE_2D_MULTISAMPLE, or GL_TEXTURE_2D_MULTISAMPLE_ARRAY, then level must be zero.
                case GL_TEXTURE_RECTANGLE:
                case GL_TEXTURE_2D_MULTISAMPLE:
                case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
                    STATE(error) = GL_INVALID_VALUE;
                    return;

                // if textarget is GL_TEXTURE_3D, then level must be greater than or equal to zero and less than or equal to $log_2$ of the value of GL_MAX_3D_TEXTURE_SIZE.
                case GL_TEXTURE_3D:
                    if (level >= ilog2(STATE_VAR(max_texture_size)))
                    {
                        ERROR_RETURN(GL_INVALID_VALUE);
                        return;
                    }
                    break;

                default:
                    if (tex->target == GL_TEXTURE_CUBE_MAP)
                    {
                        // if textarget is one of GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, or GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, then level must be greater than or equal to zero and less than or equal to $log_2$ of the value of GL_MAX_CUBE_MAP_TEXTURE_SIZE.

                        if (isCubeMapTarget(ctx, textarget))
                        {
                            if (level >=0 && level <= ilog2(STATE_VAR(max_texture_size)))
                            {
                                break;
                            }

                            ERROR_RETURN(GL_INVALID_VALUE);
                            return;
                        }
                    }
                    else if (level >= ilog2(STATE_VAR(max_texture_size)))
                    {
                        // For all other values of textarget, level must be greater than or equal to zero and less than or equal to $log_2$ of the value of GL_MAX_TEXTURE_SIZE.


                        ERROR_RETURN(GL_INVALID_VALUE);
                        return;
                    }
                    break;
            }
        }
    }
    else
    {
        // ignore all error checking
        tex = NULL;
    }

    fbo_attachment_ptr = getFBOAttachment(ctx, fbo, attachment);

    fbo_attachment_ptr->texture = texture;
    fbo_attachment_ptr->textarget = textarget;
    fbo_attachment_ptr->level = level;
    fbo_attachment_ptr->layer = layer;
    fbo_attachment_ptr->clear_bitmask = 0;
    fbo_attachment_ptr->clear_color[0] = 0.f;
    fbo_attachment_ptr->clear_color[1] = 0.f;
    fbo_attachment_ptr->clear_color[2] = 0.f;
    fbo_attachment_ptr->clear_color[3] = 0.f;
    fbo_attachment_ptr->buf.tex = tex;

    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        fbo->stencil = fbo->depth;
    }

    fbo->dirty_bits |= DIRTY_FBO_BINDING;
}

/*
 target
 Specifies the target to which the framebuffer is bound for all commands except glNamedFramebufferTexture.

 framebuffer
 Specifies the name of the framebuffer object for glNamedFramebufferTexture.

 attachment
 Specifies the attachment point of the framebuffer.

 textarget
 For glFramebufferTexture1D, glFramebufferTexture2D and glFramebufferTexture3D, specifies what type of texture is expected in the texture parameter, or for cube map textures, which face is to be attached.

 texture
 Specifies the name of an existing texture object to attach.

 level
 Specifies the mipmap level of the texture object to attach.
 */

void mglFramebufferTexture(GLMContext ctx, GLenum target, GLenum attachment, GLuint texture, GLint level)
{
    framebufferTexture(ctx, target, GL_NONE, attachment, GL_NONE, texture, level, 0);
}


void mglFramebufferTexture1D(GLMContext ctx, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    assert(textarget == GL_TEXTURE_1D);

    framebufferTexture(ctx, target, GL_TEXTURE_1D, attachment, textarget, texture, level, 0);
}

void mglFramebufferTexture2D(GLMContext ctx, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    switch(textarget)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE:
        case GL_TEXTURE_2D_MULTISAMPLE:
            break;

        default:
            if (isCubeMapTarget(ctx, textarget))
            {
                break;
            }

            ERROR_RETURN(GL_INVALID_ENUM);
    }

    framebufferTexture(ctx, target, GL_TEXTURE_2D, attachment, textarget, texture, level, 0);
}

void mglFramebufferTexture3D(GLMContext ctx, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
    assert(textarget == GL_TEXTURE_3D);

    framebufferTexture(ctx, target, GL_TEXTURE_3D, attachment, textarget, texture, level, zoffset);
}

void mglFramebufferTextureLayer(GLMContext ctx, GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    framebufferTexture(ctx, target, GL_TEXTURE_3D, attachment, GL_TEXTURE_3D, texture, level, layer);
}


void mglFramebufferRenderbuffer(GLMContext ctx, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    Framebuffer *fbo;
    Renderbuffer *rbo;
    FBOAttachment *fbo_attachment_ptr;

    fbo = currentFBOForType(ctx, target);

    // NULL means a bad target, or the default framebuffer, which has no attachments
    ERROR_CHECK_RETURN(fbo, GL_INVALID_OPERATION);

    switch(attachment)
    {
        case GL_DEPTH_ATTACHMENT:
        case GL_DEPTH_STENCIL_ATTACHMENT:
        case GL_STENCIL_ATTACHMENT:
            break;

        default:
            if (isColorAttachment(ctx, attachment))
            {
                GLuint index;

                index = attachment - GL_COLOR_ATTACHMENT0;
                if (renderbuffer)
                {
                    fbo->color_attachment_bitfield |= (0x1 << index);
                }
                else
                {
                    fbo->color_attachment_bitfield &= ~(0x1 << index);
                }
                break;
            }

            ERROR_RETURN(GL_INVALID_ENUM);
    }

    if (renderbuffer)
    {
        rbo = findRenderbuffer(ctx, renderbuffer);

        ERROR_CHECK_RETURN(rbo, GL_INVALID_OPERATION);
    }
    else
    {
        rbo = NULL;
    }

    fbo_attachment_ptr = getFBOAttachment(ctx, fbo, attachment);

    fbo_attachment_ptr->textarget = GL_RENDERBUFFER;
    fbo_attachment_ptr->texture = renderbuffer;
    fbo_attachment_ptr->level = 0;
    fbo_attachment_ptr->buf.rbo = rbo;

    if (rbo)
        rbo->is_draw_buffer = GL_FALSE;

    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        fbo->stencil = fbo->depth;
    }

    fbo->dirty_bits |= DIRTY_FBO_BINDING;
}

#pragma mark =====

void getFramebufferAttachmentParameteriv(GLMContext ctx, GLuint framebuffer, GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
    Framebuffer *fbo;
    FBOAttachment *fbo_attachment_ptr;

    switch(target)
    {
        case GL_DRAW_FRAMEBUFFER:
        case GL_FRAMEBUFFER:
            fbo = ctx->state.framebuffer;
            break;

        case GL_READ_FRAMEBUFFER:
            fbo = ctx->state.readbuffer;
            break;

        default:
            // target is zero for the Named variant, which passes a name instead
            ERROR_CHECK_RETURN(target == 0, GL_INVALID_ENUM);

            fbo = findFrameBuffer(ctx, framebuffer);

            ERROR_CHECK_RETURN(fbo, GL_INVALID_OPERATION);
            break;
    }

    if (fbo)
    {
        GLuint level __attribute__((unused));
        Texture *tex;
        GLenum target;

        if (attachment == GL_DEPTH_STENCIL_ATTACHMENT)
        {
            FBOAttachment *depth_attachment_ptr;
            FBOAttachment *stencil_attachment_ptr;

            depth_attachment_ptr = getFBOAttachment(ctx, fbo, GL_DEPTH_ATTACHMENT);
            stencil_attachment_ptr = getFBOAttachment(ctx, fbo, GL_STENCIL_ATTACHMENT);

            if ((depth_attachment_ptr != NULL) &&
                (stencil_attachment_ptr != NULL) &&
                (depth_attachment_ptr == stencil_attachment_ptr))
            {
                *params = GL_NONE;

                return;
            }

        }

        fbo_attachment_ptr = getFBOAttachment(ctx, fbo, attachment);

        if (fbo_attachment_ptr == NULL)
        {
            *params = GL_NONE;
            return;
        }

        level = fbo_attachment_ptr->level;
        target = fbo_attachment_ptr->textarget;

        if (target == GL_RENDERBUFFER)
        {
            tex = fbo_attachment_ptr->buf.rbo->tex;
        }
        else
        {
            tex = fbo_attachment_ptr->buf.tex;
        }

        switch(pname)
        {
            case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
                *params = bitcountForInternalFormat(tex->internalformat, GL_RED);
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
                *params = bitcountForInternalFormat(tex->internalformat, GL_GREEN);
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
                *params = bitcountForInternalFormat(tex->internalformat, GL_BLUE);
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
                *params = bitcountForInternalFormat(tex->internalformat, GL_ALPHA);
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
            case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
                *params = bitcountForInternalFormat(tex->internalformat, GL_NONE);
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
                *params = GL_UNSIGNED_NORMALIZED;
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
                *params = GL_LINEAR;
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                *params = (fbo_attachment_ptr->textarget == GL_RENDERBUFFER)
                        ? GL_RENDERBUFFER : GL_TEXTURE;
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                *params = fbo_attachment_ptr->texture;
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
                *params = fbo_attachment_ptr->level;
                return;

            case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
                *params = fbo_attachment_ptr->layer;
                return;

            default:
                ERROR_RETURN(GL_INVALID_ENUM);
        }
    }
    else
    {
        // default framebuffer
        switch(attachment)
        {
            case GL_FRONT_LEFT:
            case GL_FRONT_RIGHT:
            case GL_BACK_LEFT:
            case GL_BACK_RIGHT:
            case GL_DEPTH:
            case GL_STENCIL:
                switch(pname)
                {
                    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                        *params = GL_FRAMEBUFFER_DEFAULT;
                        return;

                    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
                        *params = 0;
                        return;

                    case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
                        *params = GL_UNSIGNED_NORMALIZED;
                        return;

                    case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
                        *params = GL_LINEAR;
                        return;

                    case GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
                    case GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
                    case GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
                    case GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
                        *params = 8;
                        return;

                    case GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
                        *params = (attachment == GL_DEPTH) ? 24 : 0;
                        return;

                    case GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
                        *params = (attachment == GL_STENCIL) ? 8 : 0;
                        return;

                    default:
                        ERROR_RETURN(GL_INVALID_ENUM);
                }

            default:
                ERROR_RETURN(GL_INVALID_ENUM);
        }
    }
}

void mglGetFramebufferAttachmentParameteriv(GLMContext ctx, GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
    getFramebufferAttachmentParameteriv(ctx, 0, target, attachment, pname, params);
}

void mglGetNamedFramebufferAttachmentParameteriv(GLMContext ctx, GLuint framebuffer, GLenum attachment, GLenum pname, GLint *params)
{
    getFramebufferAttachmentParameteriv(ctx, framebuffer, 0, attachment, pname, params);
}


void mglBlitFramebuffer(GLMContext ctx, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    ERROR_CHECK_RETURN((mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) == 0,
                      GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(filter == GL_NEAREST || filter == GL_LINEAR, GL_INVALID_ENUM);

    MGL_INFO("MGL: glBlitFramebuffer src(%d,%d)-(%d,%d) dst(%d,%d)-(%d,%d) mask=0x%x\n",
            srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask);
    ctx->mtl_funcs.mtlBlitFramebuffer(ctx, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void mglRenderbufferStorageMultisample(GLMContext ctx, GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    ERROR_CHECK_RETURN(target == GL_RENDERBUFFER, GL_INVALID_ENUM);

    renderbufferStorage(ctx, ctx->state.renderbuffer, samples, internalformat, width, height);
}

static void framebufferParameter(GLMContext ctx, Framebuffer *fbo, GLenum pname, GLint param)
{
    // these do not apply to the default framebuffer
    ERROR_CHECK_RETURN(fbo, GL_INVALID_OPERATION);

    switch(pname) {
        case GL_FRAMEBUFFER_DEFAULT_WIDTH:
            if (param < 0) {
                ERROR_RETURN(GL_INVALID_VALUE);
            }
            fbo->default_width = param;
            break;
        case GL_FRAMEBUFFER_DEFAULT_HEIGHT:
            if (param < 0) {
                ERROR_RETURN(GL_INVALID_VALUE);
            }
            fbo->default_height = param;
            break;
        case GL_FRAMEBUFFER_DEFAULT_LAYERS:
            if (param < 0) {
                ERROR_RETURN(GL_INVALID_VALUE);
            }
            fbo->default_layers = param;
            break;
        case GL_FRAMEBUFFER_DEFAULT_SAMPLES:
            if (param < 0) {
                ERROR_RETURN(GL_INVALID_VALUE);
            }
            fbo->default_samples = param;
            break;
        case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:
            fbo->default_fixed_sample_locations = param ? GL_TRUE : GL_FALSE;
            break;
        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static Framebuffer *fboForTarget(GLMContext ctx, GLenum target, bool *ok)
{
    *ok = true;

    switch(target)
    {
        case GL_FRAMEBUFFER:
        case GL_DRAW_FRAMEBUFFER:
            return STATE(framebuffer);

        case GL_READ_FRAMEBUFFER:
            return STATE(readbuffer);
    }

    *ok = false;

    return NULL;
}

void mglFramebufferParameteri(GLMContext ctx, GLenum target, GLenum pname, GLint param)
{
    bool ok;
    Framebuffer *fbo = fboForTarget(ctx, target, &ok);

    ERROR_CHECK_RETURN(ok, GL_INVALID_ENUM);

    framebufferParameter(ctx, fbo, pname, param);
}

static void getFramebufferParameter(GLMContext ctx, Framebuffer *fbo, GLenum pname, GLint *params)
{
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(fbo, GL_INVALID_OPERATION);

    switch(pname)
    {
        case GL_FRAMEBUFFER_DEFAULT_WIDTH:   *params = fbo->default_width;  return;
        case GL_FRAMEBUFFER_DEFAULT_HEIGHT:  *params = fbo->default_height; return;
        case GL_FRAMEBUFFER_DEFAULT_LAYERS:  *params = fbo->default_layers; return;
        case GL_FRAMEBUFFER_DEFAULT_SAMPLES: *params = fbo->default_samples; return;

        case GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS:
            *params = fbo->default_fixed_sample_locations;
            return;

        case GL_DOUBLEBUFFER:            *params = GL_FALSE; return;
        case GL_STEREO:                  *params = GL_FALSE; return;
        case GL_SAMPLES:                 *params = 0;        return;
        case GL_SAMPLE_BUFFERS:          *params = 0;        return;
        case GL_IMPLEMENTATION_COLOR_READ_FORMAT: *params = GL_RGBA;          return;
        case GL_IMPLEMENTATION_COLOR_READ_TYPE:   *params = GL_UNSIGNED_BYTE; return;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetFramebufferParameteriv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    bool ok;
    Framebuffer *fbo = fboForTarget(ctx, target, &ok);

    ERROR_CHECK_RETURN(ok, GL_INVALID_ENUM);

    getFramebufferParameter(ctx, fbo, pname, params);
}

void mglInvalidateFramebuffer(GLMContext ctx, GLenum target, GLsizei numAttachments, const GLenum *attachments)
{
    bool ok;

    fboForTarget(ctx, target, &ok);

    ERROR_CHECK_RETURN(ok, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(numAttachments >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(numAttachments == 0 || attachments, GL_INVALID_VALUE);
}

void mglInvalidateSubFramebuffer(GLMContext ctx, GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    bool ok;

    fboForTarget(ctx, target, &ok);

    ERROR_CHECK_RETURN(ok, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(numAttachments >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(numAttachments == 0 || attachments, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0, GL_INVALID_VALUE);
}

// implemented in draw_buffers.c
void mglDrawBuffer(GLMContext ctx, GLenum buf);
void mglDrawBuffers(GLMContext ctx, GLsizei n, const GLenum *bufs);
void mglReadBuffer(GLMContext ctx, GLenum src);
void mglClearBufferiv(GLMContext ctx, GLenum buffer, GLint drawbuffer, const GLint *value);
void mglClearBufferuiv(GLMContext ctx, GLenum buffer, GLint drawbuffer, const GLuint *value);
void mglClearBufferfv(GLMContext ctx, GLenum buffer, GLint drawbuffer, const GLfloat *value);
void mglClearBufferfi(GLMContext ctx, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
void mglBlitFramebuffer(GLMContext ctx, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

// The bound-state entry points below all work off STATE(framebuffer). The DSA
// forms borrow them by binding, calling, and putting the old binding back.
typedef struct {
    Framebuffer *draw;
    Framebuffer *read;
    GLuint draw_name;
    GLuint read_name;
} SavedFBO;

static bool pushFBO(GLMContext ctx, GLuint framebuffer, SavedFBO *saved)
{
    Framebuffer *fbo = NULL;

    if (framebuffer)
    {
        fbo = findFrameBuffer(ctx, framebuffer);

        if (!fbo)
            return false;
    }

    saved->draw = STATE(framebuffer);
    saved->read = STATE(readbuffer);
    saved->draw_name = STATE(var.draw_framebuffer_binding);
    saved->read_name = STATE(var.read_framebuffer_binding);

    STATE(framebuffer) = fbo;
    STATE(readbuffer)  = fbo;
    STATE(var.draw_framebuffer_binding) = framebuffer;
    STATE(var.read_framebuffer_binding) = framebuffer;

    return true;
}

static void popFBO(GLMContext ctx, SavedFBO *saved)
{
    STATE(framebuffer) = saved->draw;
    STATE(readbuffer)  = saved->read;
    STATE(var.draw_framebuffer_binding) = saved->draw_name;
    STATE(var.read_framebuffer_binding) = saved->read_name;

    STATE(dirty_bits) |= DIRTY_FBO;
}

void mglCreateFramebuffers(GLMContext ctx, GLsizei n, GLuint *framebuffers)
{
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    if (n == 0)
        return;

    ERROR_CHECK_RETURN(framebuffers, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < n; i++)
    {
        framebuffers[i] = getNewName(&STATE(framebuffer_table));

        // DSA creates the object up front, unlike Gen
        getFramebuffer(ctx, framebuffers[i]);
    }
}

void mglNamedFramebufferRenderbuffer(GLMContext ctx, GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglFramebufferRenderbuffer(ctx, GL_FRAMEBUFFER, attachment, renderbuffertarget, renderbuffer);

    popFBO(ctx, &saved);
}

void mglNamedFramebufferParameteri(GLMContext ctx, GLuint framebuffer, GLenum pname, GLint param)
{
    Framebuffer *fbo = findFrameBuffer(ctx, framebuffer);

    ERROR_CHECK_RETURN(fbo, GL_INVALID_OPERATION);

    framebufferParameter(ctx, fbo, pname, param);
}

void mglNamedFramebufferTexture(GLMContext ctx, GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglFramebufferTexture(ctx, GL_FRAMEBUFFER, attachment, texture, level);

    popFBO(ctx, &saved);
}

void mglNamedFramebufferTextureLayer(GLMContext ctx, GLuint framebuffer, GLenum attachment, GLuint texture, GLint level, GLint layer)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglFramebufferTextureLayer(ctx, GL_FRAMEBUFFER, attachment, texture, level, layer);

    popFBO(ctx, &saved);
}

void mglNamedFramebufferDrawBuffer(GLMContext ctx, GLuint framebuffer, GLenum buf)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglDrawBuffer(ctx, buf);

    popFBO(ctx, &saved);
}

void mglNamedFramebufferDrawBuffers(GLMContext ctx, GLuint framebuffer, GLsizei n, const GLenum *bufs)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglDrawBuffers(ctx, n, bufs);

    popFBO(ctx, &saved);
}

void mglNamedFramebufferReadBuffer(GLMContext ctx, GLuint framebuffer, GLenum src)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglReadBuffer(ctx, src);

    popFBO(ctx, &saved);
}

void mglInvalidateNamedFramebufferData(GLMContext ctx, GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments)
{
    ERROR_CHECK_RETURN(numAttachments >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(numAttachments == 0 || attachments, GL_INVALID_VALUE);

    if (framebuffer)
        ERROR_CHECK_RETURN(findFrameBuffer(ctx, framebuffer), GL_INVALID_OPERATION);
}

void mglInvalidateNamedFramebufferSubData(GLMContext ctx, GLuint framebuffer, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)
{
    ERROR_CHECK_RETURN(numAttachments >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(numAttachments == 0 || attachments, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0, GL_INVALID_VALUE);

    if (framebuffer)
        ERROR_CHECK_RETURN(findFrameBuffer(ctx, framebuffer), GL_INVALID_OPERATION);
}

void mglClearNamedFramebufferiv(GLMContext ctx, GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLint *value)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglClearBufferiv(ctx, buffer, drawbuffer, value);

    popFBO(ctx, &saved);
}

void mglClearNamedFramebufferuiv(GLMContext ctx, GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLuint *value)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglClearBufferuiv(ctx, buffer, drawbuffer, value);

    popFBO(ctx, &saved);
}

void mglClearNamedFramebufferfv(GLMContext ctx, GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglClearBufferfv(ctx, buffer, drawbuffer, value);

    popFBO(ctx, &saved);
}

void mglClearNamedFramebufferfi(GLMContext ctx, GLuint framebuffer, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    SavedFBO saved;

    ERROR_CHECK_RETURN(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION);

    mglClearBufferfi(ctx, buffer, drawbuffer, depth, stencil);

    popFBO(ctx, &saved);
}

void mglBlitNamedFramebuffer(GLMContext ctx, GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    Framebuffer *save_draw = STATE(framebuffer);
    Framebuffer *save_read = STATE(readbuffer);
    Framebuffer *rfbo = NULL, *dfbo = NULL;

    if (readFramebuffer)
    {
        rfbo = findFrameBuffer(ctx, readFramebuffer);
        ERROR_CHECK_RETURN(rfbo, GL_INVALID_OPERATION);
    }

    if (drawFramebuffer)
    {
        dfbo = findFrameBuffer(ctx, drawFramebuffer);
        ERROR_CHECK_RETURN(dfbo, GL_INVALID_OPERATION);
    }

    STATE(readbuffer)  = rfbo;
    STATE(framebuffer) = dfbo;

    mglBlitFramebuffer(ctx, srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);

    STATE(framebuffer) = save_draw;
    STATE(readbuffer)  = save_read;
    STATE(dirty_bits) |= DIRTY_FBO;
}

GLenum mglCheckNamedFramebufferStatus(GLMContext ctx, GLuint framebuffer, GLenum target)
{
    SavedFBO saved;
    GLenum status;

    if (framebuffer == 0)
        return GL_FRAMEBUFFER_COMPLETE;

    ERROR_CHECK_RETURN_VALUE(pushFBO(ctx, framebuffer, &saved), GL_INVALID_OPERATION, 0);

    status = mglCheckFramebufferStatus(ctx, target);

    popFBO(ctx, &saved);

    return status;
}

void mglGetNamedFramebufferParameteriv(GLMContext ctx, GLuint framebuffer, GLenum pname, GLint *params)
{
    Framebuffer *fbo = findFrameBuffer(ctx, framebuffer);

    ERROR_CHECK_RETURN(fbo, GL_INVALID_OPERATION);

    getFramebufferParameter(ctx, fbo, pname, params);
}

void mglCreateRenderbuffers(GLMContext ctx, GLsizei n, GLuint *renderbuffers)
{
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    if (n == 0)
        return;

    ERROR_CHECK_RETURN(renderbuffers, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < n; i++)
    {
        renderbuffers[i] = getNewName(&STATE(renderbuffer_table));

        getRenderbuffer(ctx, renderbuffers[i]);
    }
}

void mglNamedRenderbufferStorage(GLMContext ctx, GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height)
{
    Renderbuffer *rbo = findRenderbuffer(ctx, renderbuffer);

    ERROR_CHECK_RETURN(rbo, GL_INVALID_OPERATION);

    renderbufferStorage(ctx, rbo, 0, internalformat, width, height);
}

void mglNamedRenderbufferStorageMultisample(GLMContext ctx, GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
    Renderbuffer *rbo = findRenderbuffer(ctx, renderbuffer);

    ERROR_CHECK_RETURN(rbo, GL_INVALID_OPERATION);

    renderbufferStorage(ctx, rbo, samples, internalformat, width, height);
}

void mglGetNamedRenderbufferParameteriv(GLMContext ctx, GLuint renderbuffer, GLenum pname, GLint *params)
{
    Renderbuffer *rbo = findRenderbuffer(ctx, renderbuffer);

    ERROR_CHECK_RETURN(rbo, GL_INVALID_OPERATION);

    getRenderbufferParameter(ctx, rbo, pname, params);
}

