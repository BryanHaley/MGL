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
 * buffers.c
 * MGL
 *
 */

#include <mach/mach_vm.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>

#include "glm_context.h"
#include "buffers.h"
#include "pixel_utils.h"
#include "mgl_format_table.h"
#include "pixel_convert.h"
#include "mgl_safety.h"
#include "mgl_log.h"

// Used to recover from a corrupted context pointer (e.g. small non-NULL values like 0x2f)
extern void mgl_lazy_init(void);
extern GLMContext _ctx;

#pragma mark Utility Functions

GLuint bufferIndexFromTarget(GLMContext ctx, GLenum target)
{
    switch(target)
    {
        case GL_ARRAY_BUFFER: return _ARRAY_BUFFER;
        case GL_ELEMENT_ARRAY_BUFFER: return _ELEMENT_ARRAY_BUFFER;
        case GL_UNIFORM_BUFFER: return _UNIFORM_BUFFER;
        case GL_TEXTURE_BUFFER: return _TEXTURE_BUFFER;
        case GL_TRANSFORM_FEEDBACK_BUFFER: return _TRANSFORM_FEEDBACK_BUFFER;
        case GL_QUERY_BUFFER: return _QUERY_BUFFER;
        case GL_PIXEL_PACK_BUFFER: return _PIXEL_PACK_BUFFER;
        case GL_PIXEL_UNPACK_BUFFER: return _PIXEL_UNPACK_BUFFER;
        case GL_ATOMIC_COUNTER_BUFFER: return _ATOMIC_COUNTER_BUFFER;
        case GL_COPY_READ_BUFFER: return _COPY_READ_BUFFER;
        case GL_COPY_WRITE_BUFFER: return _COPY_WRITE_BUFFER;
        case GL_DISPATCH_INDIRECT_BUFFER: return _DISPATCH_INDIRECT_BUFFER;
        case GL_DRAW_INDIRECT_BUFFER: return _DRAW_INDIRECT_BUFFER;
        case GL_SHADER_STORAGE_BUFFER: return _SHADER_STORAGE_BUFFER;

        default:
            break;
    }

    // callers gate on checkTarget, so this means an unsupported target slipped through
    return 0xFFFFFFFF;
}

Buffer *newBuffer(GLMContext ctx, GLenum target, GLuint name)
{
    Buffer *ptr;

    ptr = (Buffer *)malloc(sizeof(Buffer));

    if (ptr == NULL)
    {
        MGL_ERR("MGL Error: %s: out of memory allocating a Buffer\n", __FUNCTION__);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

    bzero(ptr, sizeof(Buffer));

    ptr->name = name;
    ptr->target = target;

    // create buffers doesn't provide a target
    if (target)
    {
        ptr->index = bufferIndexFromTarget(ctx, target);
    }

    return ptr;
}

Buffer *getBuffer(GLMContext ctx, GLenum target, GLuint buffer)
{
    Buffer *ptr;

    ptr = (Buffer *)searchHashTable(&STATE(buffer_table), buffer);

    if (!ptr)
    {
        ptr = newBuffer(ctx, target, buffer);

        insertHashElement(&STATE(buffer_table), buffer, ptr);
    }

    return ptr;
}

bool isBuffer(GLMContext ctx, GLuint buffer)
{
    Buffer *ptr;

    ptr = (Buffer *)searchHashTable(&STATE(buffer_table), buffer);

    if (ptr)
        return true;

    return false;
}

Buffer *findBuffer(GLMContext ctx, GLuint buffer)
{
    Buffer *ptr;

    ptr = (Buffer *)searchHashTable(&STATE(buffer_table), buffer);

    return ptr;
}

bool checkTarget(GLMContext ctx, GLenum target)
{
    switch(target)
    {
        case GL_ARRAY_BUFFER:
        case GL_ELEMENT_ARRAY_BUFFER:
        case GL_UNIFORM_BUFFER:
        case GL_TEXTURE_BUFFER:
        case GL_TRANSFORM_FEEDBACK_BUFFER:
        case GL_QUERY_BUFFER:
        case GL_PIXEL_PACK_BUFFER:
        case GL_PIXEL_UNPACK_BUFFER:
        case GL_ATOMIC_COUNTER_BUFFER:
        case GL_COPY_READ_BUFFER:
        case GL_COPY_WRITE_BUFFER:
        case GL_DISPATCH_INDIRECT_BUFFER:
        case GL_DRAW_INDIRECT_BUFFER:
        case GL_SHADER_STORAGE_BUFFER:
            return true;
    }

    return false;
}

bool checkUsage(GLMContext ctx, GLenum usage)
{
    switch(usage)
    {
        case GL_STREAM_DRAW:
        case GL_STREAM_READ:
        case GL_STREAM_COPY:
        case GL_STATIC_DRAW:
        case GL_STATIC_READ:
        case GL_STATIC_COPY:
        case GL_DYNAMIC_DRAW:
        case GL_DYNAMIC_READ:
        case GL_DYNAMIC_COPY:
            return true;
    }

    return false;
}

static inline bool mgl_range_ok_glsize(GLintptr offset, GLsizeiptr size, GLsizeiptr total)
{
    if (offset < 0 || size < 0 || total < 0)
        return false;

    uint64_t uoffset = (uint64_t)offset;
    uint64_t usize = (uint64_t)size;
    uint64_t utotal = (uint64_t)total;

    if (uoffset > utotal)
        return false;
    if (usize > utotal - uoffset)
        return false;

    return true;
}

static inline bool mgl_range_ok_size_t(GLintptr offset, GLsizeiptr size, size_t total)
{
    if (offset < 0 || size < 0)
        return false;

    uint64_t uoffset = (uint64_t)offset;
    uint64_t usize = (uint64_t)size;
    uint64_t utotal = (uint64_t)total;

    if (uoffset > utotal)
        return false;
    if (usize > utotal - uoffset)
        return false;

    return true;
}

static inline GLMContext mgl_sanitize_ctx(GLMContext ctx, const char *func)
{
    // A valid heap pointer will never be a tiny value like 0x2f.
    if (ctx != NULL && (uintptr_t)ctx >= 0x10000u)
        return ctx;

    MGL_ERR("MGL ERROR: %s received invalid ctx=%p; attempting to recover\n", func, (void *)ctx);

    mgl_lazy_init();

    if (_ctx != NULL && (uintptr_t)_ctx >= 0x10000u)
        return _ctx;

    MGL_ERR("MGL ERROR: %s recovery failed; dropping call\n", func);
    return NULL;
}

size_t page_size_align(size_t size)
{
    // Ask for the real page size rather than assuming 4K. Apple silicon uses
    // 16K, and newBufferWithBytesNoCopy wants a length that is a whole number
    // of pages -- a 4K-rounded one is not, and the GPU ends up with a mapping
    // that does not match the allocation behind it.
    size_t page = (size_t)getpagesize();

    if (size & (page - 1))
        size += page - (size & (page - 1));

    return size;
}

void *getBufferData(GLMContext ctx, Buffer *ptr)
{
    void *buffer_data;

    // this used to throw the caller's buffer away and read whatever was bound
    // to GL_PIXEL_UNPACK_BUFFER instead
    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_OPERATION, NULL);

    // a persistent mapping stays mapped while the GPU reads through it
    ERROR_CHECK_RETURN_VALUE(ptr->mapped == false ||
                             (ptr->access & GL_MAP_PERSISTENT_BIT),
                             GL_INVALID_OPERATION, NULL);

    buffer_data = (void *)ptr->data.buffer_data;

    ERROR_CHECK_RETURN_VALUE(buffer_data, GL_INVALID_OPERATION, NULL);

    return buffer_data;
}

void bufferStorage(GLMContext ctx, Buffer *ptr, GLenum target, GLuint index, GLsizeiptr size, const void *data, GLbitfield storage_flags, GLenum usage)
{
    kern_return_t err;
    vm_address_t buffer_data;
    size_t buffer_size;

    buffer_size = page_size_align(size);

    // Allocate directly from VM
    err = vm_allocate((vm_map_t) mach_task_self(),
                      (vm_address_t*) &buffer_data,
                      buffer_size,
                      VM_FLAGS_ANYWHERE);
    if (err)
    {
        ERROR_RETURN(GL_OUT_OF_MEMORY);
    }

    ptr->data.buffer_data = buffer_data;

    // copy to new buffer
    if (data)
    {
        memcpy((void *)ptr->data.buffer_data, data, size);

        ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;
    }

    // init
    ptr->data.buffer_data = buffer_data;
    ptr->data.buffer_size = buffer_size;
    ptr->index = index;
    ptr->target = target;
    ptr->size = size;
    if (usage == 0)
    {
        ptr->immutable_storage = BUFFER_IMMUTABLE_STORAGE_FLAG;
        ptr->usage = usage;
    }
    else
    {
        ptr->immutable_storage = 0;
        ptr->usage = usage;
    }

    ptr->mapped = GL_FALSE;
    ptr->access = 0;
    ptr->access_flags = 0;
    ptr->storage_flags = storage_flags;

    ptr->data.dirty_bits |= DIRTY_BUFFER_ADDR;

    if (data)
    {
        if (storage_flags & (GL_CLIENT_STORAGE_BIT | GL_DYNAMIC_STORAGE_BIT))
        {
            memcpy((void *)ptr->data.buffer_data, data, ptr->size);
        }
        else
        {
            ctx->mtl_funcs.mtlBufferSubData(ctx, ptr, 0, ptr->size, data);
        }
    }
}

// The sized internal formats glClearBuffer*Data accepts (GL 4.6 table 8.19).
static bool checkClearInternalFormat(GLenum internalformat)
{
    switch(internalformat)
    {
        case GL_R8_SNORM:
        case GL_R16_SNORM:
        case GL_RG8_SNORM:
        case GL_RG16_SNORM:
        case GL_RGB8_SNORM:
        case GL_RGB16_SNORM:
        case GL_RGBA8_SNORM:
        case GL_RGBA16_SNORM:
        case GL_RGB10_A2:
        case GL_RGB10_A2UI:
        case GL_R11F_G11F_B10F:
        case GL_SRGB8:
        case GL_SRGB8_ALPHA8:
        case GL_RGB8:
        case GL_RGB16:
        case GL_RGB16F:
        case GL_RGB8I:
        case GL_RGB8UI:
        case GL_RGB16I:
        case GL_RGB16UI:
        case GL_R8:
        case GL_R16:
        case GL_R16F:
        case GL_R32F:
        case GL_R8I:
        case GL_R16I:
        case GL_R32I:
        case GL_R8UI:
        case GL_R16UI:
        case GL_R32UI:
        case GL_RG8:
        case GL_RG16:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RG8I:
        case GL_RG16I:
        case GL_RG32I:
        case GL_RG8UI:
        case GL_RG16UI:
        case GL_RG32UI:
        case GL_RGB32F:
        case GL_RGB32I:
        case GL_RGB32UI:
        case GL_RGBA8:
        case GL_RGBA16:
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_RGBA8I:
        case GL_RGBA16I:
        case GL_RGBA32I:
        case GL_RGBA8UI:
        case GL_RGBA16UI:
        case GL_RGBA32UI:
            return true;
    }

    return false;
}

// Fills [offset, offset+size) with one texel, converted from the client's
// format/type into the layout internalformat lives in, repeated over the range.
static bool clearBufferData(GLMContext ctx, Buffer *ptr, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
    ERROR_CHECK_RETURN_VALUE(checkClearInternalFormat(internalformat), GL_INVALID_ENUM, false);

    MGLNativeFormat native = mglNativeFormatForGLInternalFormat(internalformat);
    ERROR_CHECK_RETURN_VALUE(native != MGL_NF_UNKNOWN, GL_INVALID_ENUM, false);

    GLuint native_size = mglNativeFormatBytesPerPixel(native);
    ERROR_CHECK_RETURN_VALUE(native_size > 0 && native_size <= 16, GL_INVALID_ENUM, false);

    // A buffer texel is packed the way GL counts the format's own channels,
    // with no padding. Metal's nearest format may be wider -- RGB32F lives in
    // an RGBA32Float -- and a buffer must not inherit that extra channel.
    const MGLFormatDesc *fd = mglFormatDesc(internalformat);
    GLuint texel_bits = (GLuint)fd->bits[0] + fd->bits[1] + fd->bits[2] + fd->bits[3];
    GLuint texel_size = texel_bits ? (texel_bits + 7) / 8 : native_size;

    if (texel_size > native_size)
        texel_size = native_size;

    ERROR_CHECK_RETURN_VALUE(texel_size > 0, GL_INVALID_ENUM, false);

    // format/type has to describe a legal client pixel
    ERROR_CHECK_RETURN_VALUE(mglPackedPixelSize(format, type) > 0, GL_INVALID_ENUM, false);

    ERROR_CHECK_RETURN_VALUE(ptr->size >= 0, GL_INVALID_OPERATION, false);

    if (!mgl_range_ok_glsize(offset, size, ptr->size))
    {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
    }

    // the range has to land on whole texels
    if ((offset % texel_size) || (size % texel_size))
    {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
    }

    if (ptr->mapped && !(ptr->access & GL_MAP_PERSISTENT_BIT))
    {
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    // a null data pointer means clear to zero
    GLubyte texel[16] = { 0 };

    if (data)
    {
        if (!mglConvertPixelsToNative(data, 0, format, type, texel, 0, native, 1, 1))
        {
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
        }
    }

    if (size == 0)
        return true;

    ERROR_CHECK_RETURN_VALUE(ptr->data.buffer_data, GL_INVALID_OPERATION, false);

    if (!mgl_range_ok_size_t(offset, size, ptr->data.buffer_size))
    {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
    }

    GLubyte *dst = (GLubyte *)ptr->data.buffer_data + offset;

    for(GLsizeiptr i=0; i<size; i+=texel_size)
    {
        memcpy(dst + i, texel, texel_size);
    }

    // the backing aliases the Metal buffer once one exists, so this only has to
    // tell Metal the bytes moved
    if (ptr->data.mtl_data)
    {
        ctx->mtl_funcs.mtlFlushBufferRange(ctx, ptr, offset, size);
    }
    else
    {
        ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;
        ctx->state.dirty_bits |= DIRTY_BUFFER;
    }

    return true;
}


#pragma mark GL Buffer Functions
void mglGenBuffers(GLMContext ctx, GLsizei n, GLuint *buffers)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    while(n--)
    {
        *buffers++ = getNewName(&STATE(buffer_table));
    }
}

void mglCreateBuffers(GLMContext ctx, GLsizei n, GLuint *buffers)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    GLuint name;

    while(n--)
    {
        name = getNewName(&STATE(buffer_table));

        // create an unbound buffer
        getBuffer(ctx, 0, name);

        *buffers++ = name;
    }
}

void mglDeleteBuffers(GLMContext ctx, GLsizei n, const GLuint *buffers)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    GLuint buffer;

    if (n < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
        return;
    }

    while(n--)
    {
        buffer = *buffers++;

        if (isBuffer(ctx, buffer))
        {
            Buffer *ptr;

            ptr = (Buffer *)searchHashTable(&STATE(buffer_table), buffer);

            if (ptr->data.buffer_data)
            {
                if (ptr->storage_flags & GL_CLIENT_STORAGE_BIT)
                {
                    if (ptr->data.mtl_data)
                    {
                        // the mtl buffer has a deallocator for the vm allocate
                        ctx->mtl_funcs.mtlDeleteMTLObj(ctx, ptr->data.mtl_data);
                    }
                    else
                    {
                        vm_deallocate(mach_task_self(), ptr->data.buffer_data, ptr->data.buffer_size);
                    }
                }
                else
                {
                    if (ptr->data.mtl_data)
                    {
                        ctx->mtl_funcs.mtlDeleteMTLObj(ctx, ptr->data.mtl_data);
                    }
                }

                ptr->data.buffer_data = 0;
            }

            deleteHashElement(&STATE(buffer_table), buffer);

            // Drop every reference to this buffer. ptr->target is a GL enum, so
            // it can never be used directly as an array index.
            for(int t=0; t<_MAX_BUFFER_TYPES; t++)
            {
                if (STATE(buffers[t]) == ptr)
                    STATE(buffers[t]) = NULL;
            }

            if (VAO())
            {
                for(int a=0; a<MAX_BINDABLE_BUFFERS; a++)
                {
                    if (VAO_STATE(bindings[a]).buffer == ptr)
                        VAO_STATE(bindings[a]).buffer = NULL;
                }

                if (VAO()->element_array.buffer == ptr)
                    VAO()->element_array.buffer = NULL;
            }

            for(int b=0; b<_MAX_BUFFER_TYPES; b++)
            {
                for(int i=0; i<MAX_BUFFER_BASE_BINDINGS; i++)
                {
                    if (ctx->state.buffer_base[b].buffers[i].buf == ptr)
                    {
                        ctx->state.buffer_base[b].buffers[i].buf = NULL;
                        ctx->state.buffer_base[b].buffers[i].buffer = 0;
                    }
                }
            }

            // the per-stage maps hand these straight to Metal on the next draw,
            // so a deleted buffer left in one is bound after it is freed
            BufferMapList *maps[] = {
                &ctx->state.vertex_buffer_map_list,
                &ctx->state.fragment_buffer_map_list,
                &ctx->state.compute_buffer_map_list,
                &ctx->state.tess_control_buffer_map_list,
                &ctx->state.tess_eval_buffer_map_list,
                &ctx->state.geometry_buffer_map_list,
            };

            for(int m=0; m<6; m++)
                for(int i=0; i<maps[m]->count; i++)
                    if (maps[m]->buffers[i].buf == ptr)
                        maps[m]->buffers[i].buf = NULL;

            free(ptr);
        } // if (isBuffer(ctx, buffer))
    } // while(--n)
}

GLboolean mglIsBuffer(GLMContext ctx, GLuint buffer)
{
    if (isBuffer(ctx, buffer))
    {
        return GL_TRUE;
    }

    return GL_FALSE;
}

void mglBindBuffer(GLMContext ctx, GLenum target, GLuint buffer)
{
    GLint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    if (buffer)
    {
        ptr = getBuffer(ctx, target, buffer);
        ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    }
    else
    {
        ptr = NULL;
    }

    index = bufferIndexFromTarget(ctx, target);

    if (STATE(buffers[index]) != ptr)
    {
        STATE(buffers[index]) = ptr;
        STATE(dirty_bits) |= DIRTY_BUFFER;
    }

    // the element array binding belongs to the VAO, not the context
    if (target == GL_ELEMENT_ARRAY_BUFFER && VAO())
    {
        if (VAO()->element_array.buffer != ptr)
        {
            VAO()->element_array.buffer = ptr;
            VAO()->dirty_bits |= DIRTY_VAO_BUFFER_BASE;

            if (ptr)
                ptr->data.dirty_bits |= DIRTY_BUFFER;
        }
    }
}

// the targets that have an array of indexed binding points
static bool checkIndexedTarget(GLenum target)
{
    switch(target)
    {
        case GL_UNIFORM_BUFFER:
        case GL_TRANSFORM_FEEDBACK_BUFFER:
        case GL_SHADER_STORAGE_BUFFER:
        case GL_ATOMIC_COUNTER_BUFFER:
            return true;
    }

    return false;
}

// Each indexed target has its own binding space with its own ceiling.
GLuint maxBindingsForIndexedTarget(GLenum target)
{
    switch(target)
    {
        case GL_UNIFORM_BUFFER:           return MAX_UNIFORM_BUFFER_BINDINGS;
        case GL_SHADER_STORAGE_BUFFER:    return MAX_SHADER_STORAGE_BUFFER_BINDINGS;
        case GL_ATOMIC_COUNTER_BUFFER:    return MAX_ATOMIC_COUNTER_BUFFER_BINDINGS;
        case GL_TRANSFORM_FEEDBACK_BUFFER:return MAX_TRANSFORM_FEEDBACK_BUFFERS;
    }

    return 0;
}

// glBindBuffersBase / glBindBuffersRange reject a bad name with
// GL_INVALID_OPERATION, and check the whole array before binding anything.
static bool checkBindBuffersNames(GLMContext ctx, GLsizei count, const GLuint *buffers)
{
    if (!buffers)
        return true;

    for (GLsizei i = 0; i < count; i++)
    {
        if (buffers[i] && !isBuffer(ctx, buffers[i]))
            return false;
    }

    return true;
}

static bool checkBindBuffersSpan(GLenum target, GLuint first, GLsizei count)
{
    GLuint max = maxBindingsForIndexedTarget(target);

    if (first > max)
        return false;

    return (GLuint)count <= max - first;
}

void mglBindBufferBase(GLMContext ctx, GLenum target, GLuint index, GLuint buffer)
{
    Buffer  *ptr;
    GLuint buffer_index;

    ERROR_CHECK_RETURN(checkIndexedTarget(target), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(index < maxBindingsForIndexedTarget(target), GL_INVALID_VALUE);

    buffer_index = bufferIndexFromTarget(ctx, target);

    if (buffer)
    {
        // "Buffer objects may be created and bound to indexed targets": a name
        // straight out of glGenBuffers is fine here, and getBuffer makes it real.
        ptr = getBuffer(ctx, target, buffer);
        ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

        // A buffer with no storage yet is a legal thing to bind -- callers
        // routinely bind first and call glBufferData afterwards.
        ctx->state.buffer_base[buffer_index].buffers[index].buffer = buffer;
        ctx->state.buffer_base[buffer_index].buffers[index].offset = 0;
        ctx->state.buffer_base[buffer_index].buffers[index].size = ptr->size;
        ctx->state.buffer_base[buffer_index].buffers[index].buf = ptr;

        ptr->target = target;
    }
    else
    {
        ptr = NULL;
        bzero(&ctx->state.buffer_base[buffer_index].buffers[index], sizeof(BufferBaseTarget));
    }

    // BindBufferBase also binds to the generic target, so a following
    // glBufferData on that target lands on this buffer.
    if (STATE(buffers[buffer_index]) != ptr)
        STATE(buffers[buffer_index]) = ptr;

    ctx->state.dirty_bits |= (DIRTY_BUFFER | DIRTY_BUFFER_BASE_STATE);
}


void mglBindBuffersBase(GLMContext ctx, GLenum target, GLuint first, GLsizei count, const GLuint *buffers)
{
    ERROR_CHECK_RETURN(checkIndexedTarget(target), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(checkBindBuffersSpan(target, first, count), GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(checkBindBuffersNames(ctx, count, buffers), GL_INVALID_OPERATION);

    for (GLsizei i = 0; i < count; i++)
    {
        // a null array unbinds the whole span
        mglBindBufferBase(ctx, target, first + i, buffers ? buffers[i] : 0);
    }
}

void mglBindBufferRange(GLMContext ctx, GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    Buffer  *ptr;
    GLuint  buffer_index;

    ERROR_CHECK_RETURN(checkIndexedTarget(target), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(index < maxBindingsForIndexedTarget(target), GL_INVALID_VALUE);

    buffer_index = bufferIndexFromTarget(ctx, target);

    // unbinding takes no notice of offset or size
    if (buffer == 0)
    {
        bzero(&ctx->state.buffer_base[buffer_index].buffers[index], sizeof(BufferBaseTarget));
        STATE(buffers[buffer_index]) = NULL;
        ctx->state.dirty_bits |= (DIRTY_BUFFER | DIRTY_BUFFER_BASE_STATE);
        return;
    }

    // ERROR_CHECK_RETURN(offset >= 0, GL_INVALID_VALUE);
    if (offset < 0) {
        MGL_ERR("MGL Error: mglBindBufferRange: offset < 0 (%ld)\n", offset);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // ERROR_CHECK_RETURN(size > 0, GL_INVALID_VALUE);
    if (size <= 0) {
        MGL_ERR("MGL Error: mglBindBufferRange: size <= 0 (%ld)\n", size);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    ptr = getBuffer(ctx, target, buffer);
    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    // ERROR_CHECK_RETURN(ptr->data.buffer_data, GL_INVALID_VALUE);
    if (!ptr->data.buffer_data) {
         MGL_ERR("MGL Error: mglBindBufferRange: buffer_data is NULL\n");
         ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (!mgl_range_ok_size_t(offset, size, ptr->data.buffer_size)) {
        MGL_ERR("MGL Error: mglBindBufferRange: range overflow (offset=%ld size=%ld buffer_size=%ld)\n", offset, size, (long)ptr->data.buffer_size);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    ctx->state.buffer_base[buffer_index].buffers[index].buffer = buffer;
    ctx->state.buffer_base[buffer_index].buffers[index].offset = offset;
    ctx->state.buffer_base[buffer_index].buffers[index].size = size;
    ctx->state.buffer_base[buffer_index].buffers[index].buf = ptr;

    ptr->target = target;

    // like BindBufferBase, this also takes the generic binding
    STATE(buffers[buffer_index]) = ptr;

    ctx->state.dirty_bits |= (DIRTY_BUFFER | DIRTY_BUFFER_BASE_STATE);
}

#pragma mark GL Buffer Data Functions
kern_return_t initBufferData(GLMContext ctx, Buffer *ptr, GLsizeiptr size, const void *data, bool isUniformConstant)
{
    kern_return_t err;
    vm_address_t buffer_data;
    size_t buffer_size;

    if (ptr->data.buffer_data)
    {
        if (isUniformConstant)
        {
            // if its a uniform constant then lets not delete and create a buffer
            // check the old size.. then if it can fit new size then reuse.
            if (size <= ptr->data.buffer_size)
            {
                // the renderer binds ptr->size bytes, so keep it current
                ptr->size = size;

                if (data)
                {
                    memcpy((void *)ptr->data.buffer_data, data, size);

                    ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;
                }

                return 0;
            }
        }

        if (ptr->storage_flags & GL_CLIENT_STORAGE_BIT)
        {
            if (ptr->data.mtl_data)
            {
                // the mtl buffer has a deallocator for the vm allocate
                ctx->mtl_funcs.mtlDeleteMTLObj(ctx, ptr->data.mtl_data);

                // Always forget it. Keeping the pointer after releasing it lets
                // the next delete release it a second time, and the buffer is
                // freed while a command buffer is still holding it.
                ptr->data.mtl_data = NULL;
            }
            else
            {
                vm_deallocate(mach_task_self(), ptr->data.buffer_data, ptr->data.buffer_size);
            }
            
            ptr->data.buffer_data = 0;
            ptr->data.buffer_size = 0;
        }
        else
        {
            if (ptr->data.mtl_data)
            {
                ctx->mtl_funcs.mtlDeleteMTLObj(ctx, ptr->data.mtl_data);
                ptr->data.mtl_data = NULL;
            }

            
            ptr->data.buffer_data = 0;
            ptr->data.buffer_size = 0;
        }
    }

    buffer_size = page_size_align(size);

    // Allocate directly from VM
    err = vm_allocate((vm_map_t) mach_task_self(),
                      (vm_address_t*) &buffer_data,
                      buffer_size,
                      VM_FLAGS_ANYWHERE);
    if (err)
    {
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, err);
    }

    ptr->size = size;
    ptr->data.buffer_data = buffer_data;
    ptr->data.buffer_size = buffer_size;

    ptr->data.dirty_bits |= DIRTY_BUFFER_ADDR;

    // copy to new buffer
    if (data)
    {
        memcpy((void *)ptr->data.buffer_data, data, size);

        ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;
    }

    return err;
}

void mglBufferData(GLMContext ctx, GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    // GL_INVALID_ENUM is generated if target is not one of the allowable values.
    ERROR_CHECK_RETURN(checkUsage(ctx, usage), GL_INVALID_ENUM);

    if (size < 0)
    {
        MGL_ERR("MGL Error: mglBufferData: size < 0 (%ld)\n", size);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    index = bufferIndexFromTarget(ctx, target);

    ptr = STATE(buffers[index]);
    if (ptr == NULL)
    {
        // Some apps call glBufferData without binding a buffer first
        // ERROR_RETURN(GL_INVALID_OPERATION);
        return;
    }

    // buffer was created via buffer storage call for immutable storage
    if (ptr->immutable_storage)
    {
        // ERROR_RETURN(GL_INVALID_OPERATION);
        // return;
        // Workaround: Allow re-allocation even if immutable, to support guests that violate spec
        // MGL_ERR("MGL WARNING: glBufferData called on immutable buffer %d\n", ptr->name);
    }

    initBufferData(ctx, ptr, size, data, false);

    // init fields local to buffer data
    ptr->index = index;
    ptr->target = target;
    ptr->usage = usage;
    ptr->storage_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT;
 }

void mglNamedBufferData(GLMContext ctx, GLuint buffer, GLsizeiptr size, const void *data, GLenum usage)
{
    Buffer *ptr;

    ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN((ptr != NULL), GL_INVALID_OPERATION);

    if (size < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // GL_INVALID_ENUM is generated if target is not one of the allowable values.
    ERROR_CHECK_RETURN(checkUsage(ctx, usage), GL_INVALID_ENUM);


    // buffer was created via buffer storage call for immutable storage
    if (ptr->immutable_storage & BUFFER_IMMUTABLE_STORAGE_FLAG)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    initBufferData(ctx, ptr, size, data,false);

    // init fields local to buffer data
    ptr->usage = usage;
    ptr->storage_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT;
}

void mglBufferSubData(GLMContext ctx, GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
    // Absolute first check - validate ctx before ANY access
    ctx = mgl_sanitize_ctx(ctx, __FUNCTION__);
    if (!ctx)
        return;
    
    GLuint index;
    Buffer * volatile ptr;  // volatile to prevent compiler optimizations

    // GL_INVALID_ENUM is generated if target is not supported.
    if (!checkTarget(ctx, target)) {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_ENUM);
        return;
    }

    // Early exit for zero-size operations
    if (size == 0)
    {
        return;
    }

    // Validate data pointer for non-zero size
    if (data == NULL)
    {
        // MGL_ERR("MGL WARNING: mglBufferSubData: data is NULL\n");
        return;  // Silent return for NULL data
    }

    if (offset < 0 || size < 0)
    {
        MGL_ERR("MGL Error: mglBufferSubData: offset (%ld) or size (%ld) < 0\n", offset, size);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    index = bufferIndexFromTarget(ctx, target);
    if (index >= _MAX_BUFFER_TYPES) {
        MGL_ERR("MGL Error: mglBufferSubData: invalid target index %d\n", index);
        ERROR_RETURN(GL_INVALID_ENUM);
    }
    
    ptr = ctx->state.buffers[index];

    // COMPREHENSIVE BUFFER SAFETY: Validate buffer pointer and get size safely (void function)
    GLsizeiptr buffer_size;
    MGL_GET_BUFFER_SIZE_SAFE_VOID(ptr, buffer_size, "mglBufferSubData");

    if (!mgl_range_ok_glsize(offset, size, buffer_size)) {
        MGL_ERR("MGL Error: mglBufferSubData out of bounds: offset %ld size %ld buffer size %ld\n", offset, size, buffer_size);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (ptr->mapped && !(ptr->access & GL_MAP_PERSISTENT_BIT))
    {
        MGL_ERR("MGL Error: mglBufferSubData: buffer is mapped\n");
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    // GL_INVALID_OPERATION for immutable storage without dynamic bit
    if ((ptr->immutable_storage & BUFFER_IMMUTABLE_STORAGE_FLAG) &&
        !(ptr->storage_flags & GL_DYNAMIC_STORAGE_BIT))
    {
        MGL_ERR("MGL Error: mglBufferSubData: immutable storage without dynamic bit\n");
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (ptr->storage_flags & (GL_CLIENT_STORAGE_BIT | GL_DYNAMIC_STORAGE_BIT))
    {
        // CRITICAL SECURITY FIX: Proper NULL pointer check with correct type handling
        // buffer_data is vm_address_t (unsigned long), not void*
        if (ptr->data.buffer_data == 0)  // Compare to 0 for vm_address_t
        {
            MGL_ERR("MGL Error: mglBufferSubData: buffer_data is NULL (buffer %u, target 0x%x)\n", ptr->name, target);
            ERROR_RETURN(GL_INVALID_OPERATION);
        }

        if (!mgl_range_ok_size_t(offset, size, ptr->data.buffer_size))
        {
            MGL_ERR("MGL Error: mglBufferSubData out of backing store bounds: offset %ld size %ld backing %ld\n",
                    offset, size, (long)ptr->data.buffer_size);
            ERROR_RETURN(GL_INVALID_VALUE);
        }
        
        memcpy((char*)ptr->data.buffer_data + offset, data, size);
        ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;
        ctx->state.dirty_bits |= DIRTY_BUFFER;
    }
    else
    {
        if (ctx->mtl_funcs.mtlBufferSubData)
        {
            ctx->mtl_funcs.mtlBufferSubData(ctx, ptr, offset, size, data);
        }
    }
}

void mglNamedBufferSubData(GLMContext ctx, GLuint buffer, GLintptr offset, GLsizeiptr size, const void *data)
{
    Buffer *ptr;

    ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN((ptr != NULL), GL_INVALID_OPERATION);

    if (offset < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (size < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (!mgl_range_ok_glsize(offset, size, ptr->size))
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (ptr->mapped && !(ptr->access & GL_MAP_PERSISTENT_BIT))
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    // GL_INVALID_OPERATION is generated if the value of the GL_BUFFER_IMMUTABLE_STORAGE flag of the buffer object is GL_TRUE and the value of GL_BUFFER_STORAGE_FLAGS for the buffer object does not have the GL_DYNAMIC_STORAGE_BIT bit set.
    if ((ptr->immutable_storage & BUFFER_IMMUTABLE_STORAGE_FLAG) &&
        !(ptr->storage_flags & GL_DYNAMIC_STORAGE_BIT))
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (ptr->storage_flags & (GL_CLIENT_STORAGE_BIT | GL_DYNAMIC_STORAGE_BIT))
    {
        // CRITICAL SECURITY FIX: Proper NULL pointer validation for vm_address_t
        // buffer_data is vm_address_t (unsigned long), not void*
        if (ptr->data.buffer_data == 0)
        {
            MGL_ERR("MGL WARNING: mglNamedBufferSubData - buffer %u has NULL buffer_data\n", ptr->name);
            ERROR_RETURN(GL_INVALID_OPERATION);
        }

        if (!mgl_range_ok_size_t(offset, size, ptr->data.buffer_size))
        {
            MGL_ERR("MGL Error: mglNamedBufferSubData out of backing store bounds: offset %ld size %ld backing %ld\n",
                    offset, size, (long)ptr->data.buffer_size);
            ERROR_RETURN(GL_INVALID_VALUE);
        }
        
        // copy it to the backing and use processGLState to upload new data
        memcpy((char*)ptr->data.buffer_data + offset, data, size);

        if (ptr->data.mtl_data)
        {
            // use use metal to do the subdata call
            ctx->mtl_funcs.mtlBufferSubData(ctx, ptr, offset, size, data);
        }
        else
        {
            ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;

            // probably shouldn't have to do this... if its not bound its an excess
            ctx->state.dirty_bits |= DIRTY_BUFFER;
        }
    }
    else
    {
        // use use metal to do the subdata call
        ctx->mtl_funcs.mtlBufferSubData(ctx, ptr, offset, size, data);
    }
}

void copyBufferSubData(GLMContext ctx, Buffer *src_buf, Buffer *dst_buf, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    GLuint *src_data, *dst_data;

    if (size < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (!mgl_range_ok_glsize(readOffset, size, src_buf->size))
    {
        MGL_ERR("MGL Error: copyBufferSubData: read overflow (readOffset=%ld size=%ld src_size=%ld)\n", readOffset, size, src_buf->size);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (!mgl_range_ok_glsize(writeOffset, size, dst_buf->size))
    {
        MGL_ERR("MGL Error: copyBufferSubData: write overflow (writeOffset=%ld size=%ld dst_size=%ld)\n", writeOffset, size, dst_buf->size);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (src_buf == dst_buf)
    {
        if (readOffset == writeOffset)
        {
            ERROR_RETURN(GL_INVALID_VALUE);
        }

        uint64_t r0 = (uint64_t)readOffset;
        uint64_t w0 = (uint64_t)writeOffset;
        uint64_t usize = (uint64_t)size;

        if (writeOffset > readOffset)
        {
            if (r0 + usize > w0)
            {
                ERROR_RETURN(GL_INVALID_VALUE);
            }
        }
        else
        {
            if (w0 + usize > r0)
            {
                ERROR_RETURN(GL_INVALID_VALUE);
            }
        }
    }

    if (src_buf->mapped && !(src_buf->access & GL_MAP_PERSISTENT_BIT))
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (dst_buf->mapped && !(dst_buf->access & GL_MAP_PERSISTENT_BIT))
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (size == 0)
        return;

    src_data = ctx->mtl_funcs.mtlMapUnmapBuffer(ctx, src_buf, readOffset, size, GL_READ_ONLY, true);
    assert(src_data);

    dst_data = ctx->mtl_funcs.mtlMapUnmapBuffer(ctx, dst_buf, writeOffset, size, GL_WRITE_ONLY, true);
    assert(dst_data);

    memcpy(dst_data, src_data, size);

    ctx->mtl_funcs.mtlMapUnmapBuffer(ctx, dst_buf, writeOffset, size, GL_WRITE_ONLY, false);
}

void mglCopyBufferSubData(GLMContext ctx, GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    GLuint index;
    Buffer *src_buf, *dst_buf;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, readTarget), GL_INVALID_ENUM);

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, writeTarget), GL_INVALID_ENUM);

    index = bufferIndexFromTarget(ctx, readTarget);
    src_buf = STATE(buffers[index]);

    if (src_buf == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    index = bufferIndexFromTarget(ctx, writeTarget);
    dst_buf = STATE(buffers[index]);

    if (dst_buf == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    copyBufferSubData(ctx, src_buf, dst_buf, readOffset, writeOffset, size);
}

void mglCopyNamedBufferSubData(GLMContext ctx, GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)
{
    Buffer *src_buf, *dst_buf;

    if (readBuffer == 0)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (writeBuffer == 0)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    src_buf = findBuffer(ctx, readBuffer);

    if (src_buf == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    dst_buf = findBuffer(ctx, writeBuffer);

    if (dst_buf == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    // the DSA form takes buffer names, so the buffers need no target at all

    copyBufferSubData(ctx, src_buf, dst_buf, readOffset, writeOffset, size);
}

void mglClearBufferData(GLMContext ctx, GLenum target, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    index = bufferIndexFromTarget(ctx, target);
    ptr = STATE(buffers[index]);

    if (ptr == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    clearBufferData(ctx, ptr, internalformat, 0, ptr->size, format, type, data);
}

void mglClearBufferSubData(GLMContext ctx, GLenum target, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    index = bufferIndexFromTarget(ctx, target);
    ptr = STATE(buffers[index]);

    if (ptr == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    clearBufferData(ctx, ptr, internalformat, offset, size, format, type, data);
}

void mglClearNamedBufferData(GLMContext ctx, GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data)
{
    Buffer *ptr;

    ptr = findBuffer(ctx, buffer);

    if (ptr == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    clearBufferData(ctx, ptr, internalformat, 0, ptr->size, format, type, data);
}

void mglClearNamedBufferSubData(GLMContext ctx, GLuint buffer, GLenum internalformat, GLintptr offset, GLsizeiptr size, GLenum format, GLenum type, const void *data)
{
    Buffer *ptr;

    ptr = findBuffer(ctx, buffer);

    if (ptr == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    clearBufferData(ctx, ptr, internalformat, offset, size, format, type, data);
}

#pragma mark GL Buffer Map Functions
static void *mapBufferRange(GLMContext ctx, Buffer *ptr, GLintptr offset, GLsizeiptr length, GLbitfield access_flags);

/* A read of buffer contents has to see everything already submitted. The CPU
   copy is only current once the GPU is done writing it. */
static void waitForGPUWrites(GLMContext ctx)
{
    ctx->mtl_funcs.mtlFlush(ctx, true);
}

void *mglMapBuffer(GLMContext ctx, GLenum target, GLenum access)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN_VALUE(checkTarget(ctx, target), GL_INVALID_ENUM, NULL);

    switch(access)
    {
        case GL_READ_ONLY:
        case GL_WRITE_ONLY:
        case GL_READ_WRITE:
            break;

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, NULL);
    }

    index = bufferIndexFromTarget(ctx, target);
    ptr = STATE(buffers[index]);

    ERROR_CHECK_RETURN_VALUE((ptr != NULL), GL_INVALID_OPERATION, NULL);

    if (ptr->mapped)
    {
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    if (access == GL_READ_ONLY || access == GL_READ_WRITE)
        waitForGPUWrites(ctx);

    ptr->mapped = GL_TRUE;
    ptr->access = access;
    ptr->access_flags = 0;
    ptr->mapped_offset = 0;
    ptr->mapped_length = ptr->size;

    return ctx->mtl_funcs.mtlMapUnmapBuffer(ctx, ptr, 0, ptr->size, access, true);
}

void *mglMapNamedBuffer(GLMContext ctx, GLuint buffer, GLenum access)
{
    Buffer *ptr = findBuffer(ctx, buffer);
    GLbitfield flags;

    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_OPERATION, NULL);

    switch(access)
    {
        case GL_READ_ONLY:  flags = GL_MAP_READ_BIT; break;
        case GL_WRITE_ONLY: flags = GL_MAP_WRITE_BIT; break;
        case GL_READ_WRITE: flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT; break;

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, NULL);
    }

    return mapBufferRange(ctx, ptr, 0, ptr->size, flags);
}

GLboolean mglUnmapBuffer(GLMContext ctx, GLenum target)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN_VALUE(checkTarget(ctx, target), GL_INVALID_ENUM, GL_FALSE);

    index = bufferIndexFromTarget(ctx, target);
    ptr = STATE(buffers[index]);

    ERROR_CHECK_RETURN_VALUE((ptr != NULL), GL_INVALID_OPERATION, GL_FALSE);

    // GL says unmapping a buffer that is NOT mapped is the error. Unmapping
    // one that is mapped is what the call is for, and this test had it
    // backwards, so every persistent unmap failed.
    if (ptr->mapped == GL_FALSE)
    {
        MGL_ERR("MGL Error: %s: buffer %u is not mapped\n", __FUNCTION__, ptr->name);
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, GL_FALSE);
    }

    if ((ptr->storage_flags & GL_MAP_PERSISTENT_BIT) &&
        (ptr->access & GL_MAP_PERSISTENT_BIT))
    {
        // the app wrote through a live pointer, so the buffer has to go back
        // to the GPU before the next draw reads it
        ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;

        ptr->mapped = GL_FALSE;
        ptr->access = 0;
        ptr->access_flags = 0;
        ptr->mapped_offset = 0;
        ptr->mapped_length = 0;

        return GL_TRUE;
    }

    ptr->mapped = GL_FALSE;
    ptr->access = 0;
    ptr->access_flags = 0;
    ptr->mapped_offset = 0;
    ptr->mapped_length = 0;

    ctx->mtl_funcs.mtlMapUnmapBuffer(ctx, ptr, 0, ptr->size, 0, false);

    return GL_TRUE;
}

GLboolean mglUnmapNamedBuffer(GLMContext ctx, GLuint buffer)
{
    Buffer *ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_OPERATION, GL_FALSE);
    ERROR_CHECK_RETURN_VALUE(ptr->mapped, GL_INVALID_OPERATION, GL_FALSE);

    ptr->mapped = GL_FALSE;
    ptr->access = 0;
    ptr->access_flags = 0;
    ptr->mapped_offset = 0;
    ptr->mapped_length = 0;

    ctx->mtl_funcs.mtlMapUnmapBuffer(ctx, ptr, 0, 0, 0, false);

    return GL_TRUE;
}

static void *mapBufferRange(GLMContext ctx, Buffer *ptr, GLintptr offset, GLsizeiptr length, GLbitfield access_flags)
{
    if (offset < 0)
    {
        MGL_ERR("MGL Error: mglMapBufferRange: offset < 0 (%ld)\n", offset);
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, NULL);
    }

    if (length < 0)
    {
        MGL_ERR("MGL Error: mglMapBufferRange: length < 0 (%ld)\n", length);
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, NULL);
    }

    ERROR_CHECK_RETURN_VALUE((ptr != NULL), GL_INVALID_OPERATION, NULL);

    if (offset + length > ptr->size)
    {
        MGL_ERR("MGL Error: mglMapBufferRange: range overflow (offset=%ld length=%ld buffer_size=%ld)\n", offset, length, ptr->size);
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, NULL);
    }

    if (access_flags & ~(GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT |
                   GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT |
                   GL_MAP_UNSYNCHRONIZED_BIT))
    {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, NULL);
    }

    if (ptr->mapped)
    {
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    ptr->access = 0;
    ptr->mapped_offset = offset;
    ptr->mapped_length = length;

    if (access_flags & GL_MAP_PERSISTENT_BIT)
    {
        if (ptr->storage_flags & GL_MAP_PERSISTENT_BIT)
        {
            ptr->access_flags = access_flags;
            ptr->access = access_flags;

            ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;

            // A persistent mapping is still a mapping: GL_BUFFER_MAPPED reads
            // true and glUnmapBuffer has to work. What it does not do is stop
            // the buffer being used, which the checks elsewhere allow for.
            ptr->mapped = GL_TRUE;

            return (void *)ptr->data.buffer_data;
        }

        // if buffer was not marked with GL_MAP_PERSISTENT_BIT in storage flags
        // it is an error to map with persistant
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }
    else if (access_flags & GL_MAP_COHERENT_BIT)
    {
        // GL_MAP_PERSISTENT_BIT and GL_MAP_COHERENT_BIT need to be together
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    if ((access_flags & GL_MAP_READ_BIT) && !(access_flags & GL_MAP_UNSYNCHRONIZED_BIT))
        waitForGPUWrites(ctx);

    ptr->access_flags = access_flags;
    ptr->mapped = GL_TRUE;

    return ctx->mtl_funcs.mtlMapUnmapBuffer(ctx, ptr, offset, length, access_flags, true);
}

void *mglMapBufferRange(GLMContext ctx, GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access_flags)
{
    ERROR_CHECK_RETURN_VALUE(checkTarget(ctx, target), GL_INVALID_ENUM, NULL);

    return mapBufferRange(ctx, STATE(buffers[bufferIndexFromTarget(ctx, target)]), offset, length, access_flags);
}


void *mglMapNamedBufferRange(GLMContext ctx, GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    Buffer *ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_OPERATION, NULL);

    return mapBufferRange(ctx, ptr, offset, length, access);
}


void mglFlushMappedBufferRange(GLMContext ctx, GLenum target, GLintptr offset, GLsizeiptr length)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    if (offset < 0)
    {
        MGL_ERR("MGL Error: mglFlushMappedBufferRange: offset < 0 (%ld)\n", offset);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (length < 0)
    {
        MGL_ERR("MGL Error: mglFlushMappedBufferRange: length < 0 (%ld)\n", length);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    index = bufferIndexFromTarget(ctx, target);
    ptr = STATE(buffers[index]);

    ERROR_CHECK_RETURN((ptr != NULL), GL_INVALID_OPERATION);

    bool isCoherent = ptr->access & GL_MAP_COHERENT_BIT;

    if ((ptr->mapped == false) && (isCoherent == false))
    {
        // not mapped by map buffer range
        MGL_ERR("MGL Error: mglFlushMappedBufferRange: buffer not mapped\n");
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (offset < ptr->mapped_offset)
    {
        MGL_ERR("MGL Error: mglFlushMappedBufferRange: offset (%ld) < mapped_offset (%ld)\n", offset, ptr->mapped_offset);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (offset + length > ptr->mapped_length)
    {
        MGL_ERR("MGL Error: mglFlushMappedBufferRange: range overflow (offset=%ld length=%ld mapped_length=%ld)\n", offset, length, ptr->mapped_length);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (ptr->access_flags & GL_MAP_FLUSH_EXPLICIT_BIT)
    {
        ctx->mtl_funcs.mtlFlushBufferRange(ctx, ptr, offset, length);
    }
    else
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }
}

void mglFlushMappedNamedBufferRange(GLMContext ctx, GLuint buffer, GLintptr offset, GLsizeiptr length)
{
    Buffer *ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(offset >= 0 && length >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ptr->mapped, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(ptr->access_flags & GL_MAP_FLUSH_EXPLICIT_BIT, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(offset + length <= ptr->mapped_length, GL_INVALID_VALUE);

    ctx->mtl_funcs.mtlFlushBufferRange(ctx, ptr, ptr->mapped_offset + offset, length);
}

void mglBindBuffersRange(GLMContext ctx, GLenum target, GLuint first, GLsizei count, const GLuint *buffers, const GLintptr *offsets, const GLsizeiptr *sizes)
{
    ERROR_CHECK_RETURN(checkIndexedTarget(target), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(checkBindBuffersSpan(target, first, count), GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(checkBindBuffersNames(ctx, count, buffers), GL_INVALID_OPERATION);

    for (GLsizei i = 0; i < count; i++)
    {
        // a null array unbinds the whole span
        if (!buffers)
        {
            mglBindBufferRange(ctx, target, first + i, 0, 0, 0);
            continue;
        }

        mglBindBufferRange(ctx, target, first + i, buffers[i],
                           offsets ? offsets[i] : 0,
                           sizes ? sizes[i] : 0);
    }
}

#pragma mark GL Buffer Storage Functions
void mglBufferStorage(GLMContext ctx, GLenum target, GLsizeiptr size, const void *data, GLbitfield storage_flags)
{
    Buffer *ptr;
    GLuint index;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    if (size < 0)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (storage_flags & ~(GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT |
                  GL_CLIENT_STORAGE_BIT))
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    index = bufferIndexFromTarget(ctx, target);

    ptr = STATE(buffers[index]);

    ERROR_CHECK_RETURN((ptr != NULL), GL_INVALID_OPERATION);

    bufferStorage(ctx, ptr, target, index, size, data, storage_flags, 0);
}

void mglNamedBufferStorage(GLMContext ctx, GLuint buffer, GLsizeiptr size, const void *data, GLbitfield storage_flags)
{
    Buffer *ptr;

    ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN((ptr != NULL), GL_INVALID_OPERATION);

    if (size < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (storage_flags & ~(GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT |
                  GL_CLIENT_STORAGE_BIT))
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    bufferStorage(ctx, ptr, 0, 0, size, data, storage_flags, 0);
}

void mglInvalidateBufferData(GLMContext ctx, GLuint buffer)
{
    Buffer *ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ptr->mapped == GL_FALSE, GL_INVALID_OPERATION);

    // contents become undefined; keeping them is a legal choice
    ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;
}

void mglInvalidateBufferSubData(GLMContext ctx, GLuint buffer, GLintptr offset, GLsizeiptr length)
{
    Buffer *ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(offset >= 0 && length >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(offset + length <= ptr->size, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ptr->mapped == GL_FALSE, GL_INVALID_OPERATION);

    ptr->data.dirty_bits |= DIRTY_BUFFER_DATA;
}

#pragma mark GL Buffer Get Functions
void mglGetBufferParameteriv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    ERROR_CHECK_RETURN((params != NULL), GL_INVALID_OPERATION);

    index = bufferIndexFromTarget(ctx, target);
    ptr = STATE(buffers[index]);

    ERROR_CHECK_RETURN((ptr != NULL), GL_INVALID_OPERATION);

    switch(pname)
    {
        case GL_BUFFER_ACCESS:
            *params = ptr->access;
            break;

        case GL_BUFFER_ACCESS_FLAGS:
            *params = ptr->access_flags;
            break;

        case GL_BUFFER_IMMUTABLE_STORAGE:
            *params = (ptr->immutable_storage & BUFFER_IMMUTABLE_STORAGE_FLAG) ? GL_TRUE : GL_FALSE;
            break;

        case GL_BUFFER_MAPPED:
            *params = ptr->mapped;
            break;

        case GL_BUFFER_MAP_LENGTH:
            assert(ptr->mapped_length < INT_MAX);
            *params = (GLint)ptr->mapped_length;
            break;

        case GL_BUFFER_MAP_OFFSET:
            assert(ptr->mapped_offset < INT_MAX);
            *params = (GLint)ptr->mapped_offset;
            break;

        case GL_BUFFER_SIZE:
            assert(ptr->size < INT_MAX);
            *params = (GLint)ptr->size;
            break;

        case GL_BUFFER_STORAGE_FLAGS:
            *params = ptr->storage_flags;
            break;

        case GL_BUFFER_USAGE:
            *params = ptr->usage;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
            break;
    }
}

void mglGetBufferPointerv(GLMContext ctx, GLenum target, GLenum pname, void **params)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    switch(pname)
    {
        case GL_BUFFER_MAP_POINTER:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
            break;
    }

    ERROR_CHECK_RETURN((params != NULL), GL_INVALID_OPERATION);

    index = bufferIndexFromTarget(ctx, target);
    ptr = STATE(buffers[index]);

    ERROR_CHECK_RETURN((ptr != NULL), GL_INVALID_OPERATION);

    if (ptr->mapped) {
        *params = (void *)ptr->data.buffer_data;
    } else {
        *params = NULL;
    }
}

void mglGetBufferSubData(GLMContext ctx, GLenum target, GLintptr offset, GLsizeiptr size, void *data)
{
    GLuint index;
    Buffer *ptr;

    // GL_INVALID_ENUM is generated if target is not supported.
    ERROR_CHECK_RETURN(checkTarget(ctx, target), GL_INVALID_ENUM);

    if (offset < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (size < 0)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    index = bufferIndexFromTarget(ctx, target);
    ptr = STATE(buffers[index]);

    if (ptr == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!mgl_range_ok_glsize(offset, size, ptr->size))
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (ptr->mapped && !(ptr->access & GL_MAP_PERSISTENT_BIT))
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (data == NULL)
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (ptr->data.buffer_data == 0)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (!mgl_range_ok_size_t(offset, size, ptr->data.buffer_size))
    {
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    waitForGPUWrites(ctx);

    memcpy(data, (const uint8_t *)((uintptr_t)ptr->data.buffer_data) + (uintptr_t)offset, (size_t)size);
}

void mglGetNamedBufferParameteriv(GLMContext ctx, GLuint buffer, GLenum pname, GLint *params)
{
    Buffer *ptr;

    ptr = findBuffer(ctx, buffer);
    if (ptr == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }


    ERROR_CHECK_RETURN(params != NULL, GL_INVALID_VALUE);

    switch(pname)
    {
        case GL_BUFFER_ACCESS:
            *params = ptr->access;
            break;

        case GL_BUFFER_ACCESS_FLAGS:
            *params = ptr->access_flags;
            break;

        case GL_BUFFER_IMMUTABLE_STORAGE:
            *params = (ptr->immutable_storage & BUFFER_IMMUTABLE_STORAGE_FLAG) ? GL_TRUE : GL_FALSE;
            break;

        case GL_BUFFER_MAPPED:
            *params = ptr->mapped;
            break;

        case GL_BUFFER_MAP_LENGTH:
            *params = (GLint)ptr->mapped_length;
            break;

        case GL_BUFFER_MAP_OFFSET:
            *params = (GLint)ptr->mapped_offset;
            break;

        case GL_BUFFER_SIZE:
            *params = (GLint)ptr->size;
            break;

        case GL_BUFFER_STORAGE_FLAGS:
            *params = ptr->storage_flags;
            break;

        case GL_BUFFER_USAGE:
            *params = ptr->usage;
            break;

        default:
            ERROR_RETURN(GL_INVALID_OPERATION);
    }
}

void mglGetNamedBufferParameteri64v(GLMContext ctx, GLuint buffer, GLenum pname, GLint64 *params)
{
    Buffer *ptr;

    ptr = findBuffer(ctx, buffer);
    if (ptr == NULL)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }


    ERROR_CHECK_RETURN(params != NULL, GL_INVALID_VALUE);

    switch(pname)
    {
        case GL_BUFFER_ACCESS:
            *params = ptr->access;
            break;

        case GL_BUFFER_ACCESS_FLAGS:
            *params = ptr->access_flags;
            break;

        case GL_BUFFER_IMMUTABLE_STORAGE:
            *params = (ptr->immutable_storage & BUFFER_IMMUTABLE_STORAGE_FLAG) ? GL_TRUE : GL_FALSE;
            break;

        case GL_BUFFER_STORAGE_FLAGS:
            *params = ptr->storage_flags;
            break;

        case GL_BUFFER_MAPPED:
            *params = ptr->mapped;
            break;

        case GL_BUFFER_MAP_LENGTH:
            *params = ptr->mapped_length;
            break;

        case GL_BUFFER_MAP_OFFSET:
            *params = ptr->mapped_offset;
            break;

        case GL_BUFFER_SIZE:
            *params = ptr->size;
            break;

        case GL_BUFFER_USAGE:
            *params = ptr->usage;
            break;

        default:
            ERROR_RETURN(GL_INVALID_OPERATION);
    }
}

void mglGetNamedBufferPointerv(GLMContext ctx, GLuint buffer, GLenum pname, void **params)
{
    Buffer *ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(pname == GL_BUFFER_MAP_POINTER, GL_INVALID_ENUM);

    *params = ptr->mapped ? (void *)(ptr->data.buffer_data + ptr->mapped_offset) : NULL;
}

void mglGetNamedBufferSubData(GLMContext ctx, GLuint buffer, GLintptr offset, GLsizeiptr size, void *data)
{
    Buffer *ptr = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(offset >= 0 && size >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(offset + size <= ptr->size, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ptr->mapped == GL_FALSE, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(data || size == 0, GL_INVALID_VALUE);

    if (size == 0)
        return;

    ERROR_CHECK_RETURN(ptr->data.buffer_data, GL_INVALID_OPERATION);

    waitForGPUWrites(ctx);

    memcpy(data, (void *)(ptr->data.buffer_data + offset), (size_t)size);
}


void mglGetBufferParameteri64v(GLMContext ctx, GLenum target, GLenum pname, GLint64 *params)
{
    GLint tmp = 0;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    mglGetBufferParameteriv(ctx, target, pname, &tmp);

    *params = tmp;
}
