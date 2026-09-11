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
 * rendering.c
 * MGL
 *
 */

#include <mach/mach_vm.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>

#include "mgl.h"

#include "pixel_utils.h"
#include "glm_context.h"
#include "mgl_log.h"

void mglClear(GLMContext ctx, GLbitfield mask)
{
    if (mask & ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT))
    {
        MGL_ERR("MGL Error: mglClear: invalid mask 0x%x\n", mask);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    ctx->state.clear_bitmask = mask;

    // Do it now. Deferring means a second glClear overwrites the first, and the
    // clear colour and scissor get sampled whenever the pass happens to be built
    // rather than at the point of the call.
    if (ctx->mtl_funcs.mtlClearBuffer)
        ctx->mtl_funcs.mtlClearBuffer(ctx, 0, mask);
}

void mglClearColor(GLMContext ctx, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    ctx->state.color_clear_value[0] = red;
    ctx->state.color_clear_value[1] = green;
    ctx->state.color_clear_value[2] = blue;
    ctx->state.color_clear_value[3] = alpha;

    ctx->state.dirty_bits |= DIRTY_STATE;
}

void mglClearStencil(GLMContext ctx, GLint s)
{
    ctx->state.var.stencil_clear_value = s;

    ctx->state.dirty_bits |= DIRTY_STATE;
}

void mglClearDepth(GLMContext ctx, GLdouble depth)
{
    ctx->state.var.depth_clear_value = depth;

    ctx->state.dirty_bits |= DIRTY_STATE;
}

// The window's framebuffer has no Framebuffer object behind it, so a
// glClearBuffer* aimed at it goes the same way glClear does.
static bool clearDefaultFramebuffer(GLMContext ctx, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
    GLbitfield mask;

    switch (buffer) {
        case GL_COLOR:
            ERROR_CHECK_RETURN_VALUE(drawbuffer == 0, GL_INVALID_VALUE, false);
            ctx->state.color_clear_value[0] = value[0];
            ctx->state.color_clear_value[1] = value[1];
            ctx->state.color_clear_value[2] = value[2];
            ctx->state.color_clear_value[3] = value[3];
            mask = GL_COLOR_BUFFER_BIT;
            break;

        case GL_DEPTH:
            ctx->state.var.depth_clear_value = value[0];
            mask = GL_DEPTH_BUFFER_BIT;
            break;

        case GL_STENCIL:
            ctx->state.var.stencil_clear_value = (GLint)value[0];
            mask = GL_STENCIL_BUFFER_BIT;
            break;

        case GL_DEPTH_STENCIL:
            ctx->state.var.depth_clear_value = value[0];
            ctx->state.var.stencil_clear_value = (GLint)value[1];
            mask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
            break;

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }

    ctx->state.clear_bitmask = mask;

    if (ctx->mtl_funcs.mtlClearBuffer)
        ctx->mtl_funcs.mtlClearBuffer(ctx, 0, mask);

    return true;
}

void mglClearBufferfv(GLMContext ctx, GLenum buffer, GLint drawbuffer, const GLfloat *value)
{
    Framebuffer * fbo = ctx->state.framebuffer;
    FBOAttachment * fboa;

    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    if (fbo == NULL)
    {
        clearDefaultFramebuffer(ctx, buffer, drawbuffer, value);
        return;
    }

    switch (buffer) {
        case GL_COLOR:
            ERROR_CHECK_RETURN(drawbuffer >= 0 && drawbuffer < MAX_COLOR_ATTACHMENTS, GL_INVALID_VALUE);
            fboa = &fbo->color_attachments[drawbuffer];
            fboa->clear_bitmask |= GL_COLOR_BUFFER_BIT;
            fboa->clear_color[0] = value[0];
            fboa->clear_color[1] = value[1];
            fboa->clear_color[2] = value[2];
            fboa->clear_color[3] = value[3];
            break;
        case GL_DEPTH:
            fboa = &fbo->depth;
            fboa->clear_bitmask |= GL_DEPTH_BUFFER_BIT;
            fboa->clear_color[0] = value[0];
            break;
        case GL_STENCIL:
            fboa = &fbo->stencil;
            fboa->clear_bitmask |= GL_STENCIL_BUFFER_BIT;
            fboa->clear_color[0] = value[0];
            break;
        default:
            MGL_ERR("MGL Error: mglClearBufferfv: invalid buffer 0x%x\n", buffer);
            ERROR_RETURN(GL_INVALID_ENUM);
            break;
    }
}

void mglClearBufferfi(GLMContext ctx, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    Framebuffer * fbo = ctx->state.framebuffer;
    FBOAttachment * fboa;

    if (fbo == NULL)
    {
        GLfloat v[2] = { depth, (GLfloat)stencil };

        clearDefaultFramebuffer(ctx, buffer, drawbuffer, v);
        return;
    }

    switch (buffer) {
        case GL_DEPTH_STENCIL:
            fboa = &fbo->depth;
            fboa->clear_bitmask |= GL_DEPTH_BUFFER_BIT;
            fboa->clear_color[0] = depth;

            fboa = &fbo->stencil;
            fboa->clear_bitmask |= GL_STENCIL_BUFFER_BIT;
            fboa->clear_color[0] = stencil;
            break;
        default:
            MGL_ERR("MGL Error: mglClearBufferfi: invalid buffer 0x%x\n", buffer);
            ERROR_RETURN(GL_INVALID_ENUM);
            break;
    }
}

void mglFinish(GLMContext ctx)
{
    MGL_INFO("MGL: mglFinish called - flushing and waiting for GPU\n");
    ctx->mtl_funcs.mtlFlush(ctx, true);
}

void mglFlush(GLMContext ctx)
{
    ctx->mtl_funcs.mtlFlush(ctx, false);
}

void mglDrawBuffers(GLMContext ctx, GLsizei n, const GLenum *bufs)
{
    for (GLsizei i=0; i<n; ++i) {
        mglDrawBuffer(ctx, bufs[i]);
    }
}

void mglDrawBuffer(GLMContext ctx, GLenum buf)
{
    if ((buf >= GL_COLOR_ATTACHMENT0) &&
        (buf <= (GL_COLOR_ATTACHMENT0 + STATE(max_color_attachments))))
    {
        // ok
    }
    else
    switch(buf)
    {
        case GL_FRONT:
            break;

        case GL_NONE:
        case GL_FRONT_LEFT:
        case GL_FRONT_RIGHT:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
        case GL_LEFT:
        case GL_RIGHT:
        case GL_FRONT_AND_BACK:
            // TODO: Implement these buffer modes properly
            MGL_INFO("MGL: mglDrawBuffer called with unimplemented mode 0x%x\n", buf);
            break;

        default:
            MGL_ERR("MGL Error: mglDrawBuffer: invalid enum 0x%x\n", buf);
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    if ((buf >= GL_COLOR_ATTACHMENT0) &&
        (buf <= (GL_COLOR_ATTACHMENT0 + STATE(max_color_attachments))))
    {
        // probably should validate current fbo..
        Framebuffer * fbo = ctx->state.framebuffer;
        if (!fbo || !fbo->color_attachments[buf-GL_COLOR_ATTACHMENT0].buf.rbo)
        {
            MGL_ERR("MGL Error: mglDrawBuffer: missing color attachment %u\n", (unsigned)(buf - GL_COLOR_ATTACHMENT0));
            ERROR_RETURN(GL_INVALID_OPERATION);
            return;
        }
        fbo->color_attachments[buf-GL_COLOR_ATTACHMENT0].buf.rbo->is_draw_buffer = GL_TRUE;
    }

    STATE(draw_buffer) = buf;
    if (ctx->state.framebuffer)
        ctx->state.framebuffer->draw_buffer = buf;
    else
        STATE(default_draw_buffer) = buf;
    STATE(dirty_bits) |= DIRTY_STATE;
}

void mglReadBuffer(GLMContext ctx, GLenum buf)
{
    if ((buf >= GL_COLOR_ATTACHMENT0) &&
        (buf <= (GL_COLOR_ATTACHMENT0 + STATE(max_color_attachments))))
    {
        // ok
    }
    else
    switch(buf)
    {
        case GL_FRONT:
        case GL_BACK:
        case GL_NONE:
        case GL_FRONT_LEFT:
        case GL_FRONT_RIGHT:
        case GL_BACK_LEFT:
        case GL_BACK_RIGHT:
        case GL_LEFT:
        case GL_RIGHT:
        case GL_FRONT_AND_BACK:
            // These read buffer modes are accepted but may not be fully implemented
            break;

        default:
            MGL_ERR("MGL Error: mglReadBuffer: invalid enum 0x%x\n", buf);
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    if ((buf >= GL_COLOR_ATTACHMENT0) &&
        (buf <= (GL_COLOR_ATTACHMENT0 + STATE(max_color_attachments))))
    {
        // probably should validate current fbo..
    }

    STATE(read_buffer) = buf;
    STATE(dirty_bits) |= DIRTY_STATE;
}

void mglPixelStorei(GLMContext ctx, GLenum pname, GLint param)
{
    // ERROR_CHECK_RETURN(param >= 0, GL_INVALID_VALUE);
    if (param < 0) {
        MGL_ERR("MGL Error: mglPixelStorei: param < 0 (%d) for pname 0x%x\n", param, pname);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    switch(pname)
    {
        case GL_PACK_SWAP_BYTES:
            ctx->state.pack.swap_bytes = (param != 0 ? true : false);
            break;

        case GL_PACK_LSB_FIRST:
            ctx->state.pack.lsb_first = (param != 0 ? true : false);
            break;

        case GL_PACK_ROW_LENGTH:
            ctx->state.pack.row_length = param;
            break;

        case GL_PACK_IMAGE_HEIGHT:
            ctx->state.pack.image_height = param;
            break;

        case GL_PACK_SKIP_ROWS:
            ctx->state.pack.skip_rows = param;
            break;

        case GL_PACK_SKIP_PIXELS:
            ctx->state.pack.skip_pixels = param;
            break;

        case GL_PACK_SKIP_IMAGES:
            ctx->state.pack.skip_images = param;
            break;

        case GL_PACK_ALIGNMENT:
            switch(param)
            {
                case 1:
                case 2:
                case 4:
                case 8:
                    ctx->state.pack.alignment = param;
                    break;

                default:
                    MGL_ERR("MGL Error: mglPixelStorei: invalid PACK_ALIGNMENT %d\n", param);
                    ERROR_RETURN(GL_INVALID_VALUE);
                    break;
            }
            break;

        case GL_UNPACK_SWAP_BYTES:
            ctx->state.unpack.swap_bytes = (param != 0 ? true : false);
            break;

        case GL_UNPACK_LSB_FIRST:
            ctx->state.unpack.lsb_first = (param != 0 ? true : false);
            break;

        case GL_UNPACK_ROW_LENGTH:
            ctx->state.unpack.row_length = param;
            break;
        case GL_UNPACK_IMAGE_HEIGHT:
            ctx->state.unpack.image_height = param;
            break;

        case GL_UNPACK_SKIP_ROWS:
            ctx->state.unpack.skip_rows = param;
            break;

        case GL_UNPACK_SKIP_PIXELS:
            ctx->state.unpack.skip_pixels = param;
            break;

        case GL_UNPACK_SKIP_IMAGES:
            ctx->state.unpack.skip_images = param;
            break;

        case GL_UNPACK_ALIGNMENT:
            switch(param)
            {
                case 1:
                case 2:
                case 4:
                case 8:
                    ctx->state.unpack.alignment = param;
                    break;

                default:
                    MGL_ERR("MGL Error: mglPixelStorei: invalid UNPACK_ALIGNMENT %d\n", param);
                    ERROR_RETURN(GL_INVALID_VALUE);
                    break;
            }
            break;
    }
}

void mglPixelStoref(GLMContext ctx, GLenum pname, GLfloat param)
{
    mglPixelStorei(ctx, pname, (GLint)param);
}

void mglReadPixels(GLMContext ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
    GLuint pixel_size;

    pixel_size = sizeForFormatType(format, type);
    // ERROR_CHECK_RETURN(pixel_size != 0, GL_INVALID_ENUM);
    if (pixel_size == 0) {
        MGL_ERR("MGL Error: mglReadPixels: invalid format/type combination (format=0x%x type=0x%x)\n", format, type);
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    // ERROR_CHECK_RETURN(width > 0, GL_INVALID_ENUM);
    if (width < 0) {
        MGL_ERR("MGL Error: mglReadPixels: width < 0 (%d)\n", width);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // ERROR_CHECK_RETURN(height > 0, GL_INVALID_ENUM);
    if (height < 0) {
        MGL_ERR("MGL Error: mglReadPixels: height < 0 (%d)\n", height);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (width == 0 || height == 0)
    {
        return;
    }

    size_t row_length;
    size_t pitch_size;
    GLuint pitch;
    size_t buffer_size;

    row_length = (STATE(pack.row_length) ? (size_t)STATE(pack.row_length) : (size_t)width);
    pitch_size = row_length * (size_t)pixel_size;
    if (pitch_size > UINT_MAX)
    {
        MGL_ERR("MGL Error: mglReadPixels: pitch overflow (row_length=%zu pixel_size=%u)\n", row_length, pixel_size);
        ERROR_RETURN(GL_OUT_OF_MEMORY);
    }
    pitch = (GLuint)pitch_size;

    if (pitch_size > 0 && (size_t)height > (SIZE_MAX / pitch_size))
    {
        MGL_ERR("MGL Error: mglReadPixels: buffer_size overflow (pitch=%zu height=%d)\n", pitch_size, height);
        ERROR_RETURN(GL_OUT_OF_MEMORY);
    }
    buffer_size = pitch_size * (size_t)height;
    if (buffer_size > UINT_MAX)
    {
        MGL_ERR("MGL Error: mglReadPixels: buffer_size exceeds API limit (%zu)\n", buffer_size);
        ERROR_RETURN(GL_OUT_OF_MEMORY);
    }

    switch(format)
    {
        case GL_STENCIL_INDEX:
            ERROR_CHECK_RETURN(ctx->stencil_format.mtl_pixel_format > 0, GL_INVALID_OPERATION);
            break;

        case GL_DEPTH_COMPONENT:
            ERROR_CHECK_RETURN(ctx->depth_format.mtl_pixel_format > 0, GL_INVALID_OPERATION);
            break;

        case GL_DEPTH_STENCIL:
            ERROR_CHECK_RETURN((ctx->depth_format.mtl_pixel_format > 0) ||
                               (ctx->stencil_format.mtl_pixel_format > 0), GL_INVALID_OPERATION);
            switch(type)
            {
                case GL_UNSIGNED_INT_24_8:
                case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
                    break;

                default:
                    ERROR_RETURN(GL_INVALID_ENUM);
                    break;
            }
            break;

        default:
            break;
    }

    switch(type)
    {
        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            // ERROR_CHECK_RETURN(format == GL_RGB || format == GL_BGR, GL_INVALID_OPERATION);
            if (!(format == GL_RGB || format == GL_BGR)) {
                MGL_ERR("MGL Error: mglReadPixels: invalid format for type (format=0x%x type=0x%x)\n", format, type);
                ERROR_RETURN(GL_INVALID_OPERATION);
            }
            break;

        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            // ERROR_CHECK_RETURN(format == GL_RGBA || format == GL_BGRA, GL_INVALID_OPERATION);
            if (!(format == GL_RGBA || format == GL_BGRA)) {
                MGL_ERR("MGL Error: mglReadPixels: invalid format for type (format=0x%x type=0x%x)\n", format, type);
                ERROR_RETURN(GL_INVALID_OPERATION);
            }
            break;
    }

    if (STATE(buffers[_PIXEL_PACK_BUFFER]))
    {
        Buffer *ptr;
        uintptr_t offset;
        uint8_t *base;

        ptr = STATE(buffers[_PIXEL_PACK_BUFFER]);

        // ERROR_CHECK_RETURN(ptr->mapped == false, GL_INVALID_OPERATION);
        if (ptr->mapped) {
            MGL_ERR("MGL Error: mglReadPixels: pixel pack buffer is mapped\n");
            ERROR_RETURN(GL_INVALID_OPERATION);
        }

        if (ptr->size < 0)
        {
            MGL_ERR("MGL Error: mglReadPixels: pixel pack buffer has negative size (%ld)\n", (long)ptr->size);
            ERROR_RETURN(GL_INVALID_OPERATION);
        }

        offset = (uintptr_t)pixels;
        if (pixel_size && (offset % (uintptr_t)pixel_size) != 0)
        {
            MGL_ERR("MGL Error: mglReadPixels: pixel pack buffer offset not aligned (offset=%lu pixel_size=%u)\n", (unsigned long)offset, pixel_size);
            ERROR_RETURN(GL_INVALID_OPERATION);
        }

        if ((size_t)ptr->size < offset || (size_t)ptr->size - offset < buffer_size)
        {
            MGL_ERR("MGL Error: mglReadPixels: pixel pack buffer too small (size=%ld offset=%lu req=%zu)\n",
                    (long)ptr->size, (unsigned long)offset, buffer_size);
            ERROR_RETURN(GL_INVALID_OPERATION);
        }

        base = (uint8_t *)(uintptr_t)ptr->data.buffer_data;
        if (!base)
        {
            MGL_ERR("MGL Error: mglReadPixels: pixel pack buffer has no CPU storage\n");
            ERROR_RETURN(GL_INVALID_OPERATION);
        }

        pixels = (void *)(base + offset);
    }

    if (!STATE(buffers[_PIXEL_PACK_BUFFER]) && pixels == NULL)
    {
        MGL_ERR("MGL Error: mglReadPixels: pixels is NULL with no pixel pack buffer bound\n");
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    // the renderer converts straight into the caller's buffer
    ctx->mtl_funcs.mtlReadPixels(ctx, pixels, pitch, format, type, x, y, width, height);
}


void mglReadnPixels(GLMContext ctx, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data)
{
    GLuint pixel_bytes;

    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0, GL_INVALID_VALUE);

    pixel_bytes = sizeForFormatType(format, type);

    if (pixel_bytes)
    {
        GLint64 needed = (GLint64)width * (GLint64)height * (GLint64)pixel_bytes;

        ERROR_CHECK_RETURN(needed <= (GLint64)bufSize, GL_INVALID_OPERATION);
    }

    mglReadPixels(ctx, x, y, width, height, format, type, data);
}

// integer attachments are cleared with the value as written, not normalized
static void clearBufferInteger(GLMContext ctx, GLenum buffer, GLint drawbuffer,
                               const GLfloat *v, bool allow_stencil)
{
    Framebuffer *fbo = ctx->state.framebuffer;
    FBOAttachment *fboa;

    if (fbo == NULL)
    {
        clearDefaultFramebuffer(ctx, buffer, drawbuffer, v);
        return;
    }

    switch (buffer)
    {
        case GL_COLOR:
            ERROR_CHECK_RETURN(drawbuffer >= 0 && drawbuffer < MAX_COLOR_ATTACHMENTS, GL_INVALID_VALUE);

            fboa = &fbo->color_attachments[drawbuffer];
            fboa->clear_bitmask |= GL_COLOR_BUFFER_BIT;
            memcpy(fboa->clear_color, v, 4 * sizeof(GLfloat));
            break;

        case GL_STENCIL:
            ERROR_CHECK_RETURN(allow_stencil, GL_INVALID_ENUM);
            ERROR_CHECK_RETURN(drawbuffer == 0, GL_INVALID_VALUE);

            fboa = &fbo->stencil;
            fboa->clear_bitmask |= GL_STENCIL_BUFFER_BIT;
            fboa->clear_color[0] = v[0];
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglClearBufferiv(GLMContext ctx, GLenum buffer, GLint drawbuffer, const GLint *value)
{
    GLfloat v[4];

    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        v[i] = (GLfloat)value[i];

    clearBufferInteger(ctx, buffer, drawbuffer, v, true);
}

void mglClearBufferuiv(GLMContext ctx, GLenum buffer, GLint drawbuffer, const GLuint *value)
{
    GLfloat v[4];

    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        v[i] = (GLfloat)value[i];

    clearBufferInteger(ctx, buffer, drawbuffer, v, false);
}
