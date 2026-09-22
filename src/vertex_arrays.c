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
 * vertex_arrays.c
 * MGL
 *
 */

#include "pixel_convert.h"
#include <math.h>
#include <strings.h>

#include "glm_context.h"
#include "mgl_log.h"

Buffer *findBuffer(GLMContext ctx, GLuint buffer);

static int readAttribConstant(GLMContext ctx, GLuint index, GLdouble *out);
void setBindingDivisor(GLMContext ctx, VertexArray *vao, GLuint bindingindex, GLuint divisor);

GLsizei typeSize(GLenum type)
{
    switch(type)
    {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return sizeof(char);

        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return sizeof(short);

        case GL_INT:
        case GL_UNSIGNED_INT:
            return sizeof(int);

        case GL_FLOAT:
            return sizeof(float);

        case GL_DOUBLE:
            return sizeof(float);

        case GL_HALF_FLOAT:
            return sizeof(float) >> 1;

        case GL_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
            return sizeof(int);

        default:
            return 0;
    }

    return 0;
}

GLsizei genStrideFromTypeSize(GLenum type, GLint size)
{
    return typeSize(type) * size;
}


VertexArray *newVAO(GLMContext ctx, GLuint vao)
{
    VertexArray *ptr;

    ptr = (VertexArray *)malloc(sizeof(VertexArray));

    if (ptr == NULL)
    {
        MGL_ERR("MGL Error: %s: out of memory allocating a VertexArray\n", __FUNCTION__);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

    bzero((void *)ptr, sizeof(VertexArray));

    ptr->name = vao;

    for(int i=0; i<MAX_ATTRIBS; i++)
    {
        ptr->attrib[i].size = 4;
        ptr->attrib[i].type = GL_FLOAT;
        ptr->attrib[i].stride = 0;
        ptr->attrib[i].relativeoffset = 0;
        // GL starts every attribute on the binding of the same number
        ptr->attrib[i].buffer_bindingindex = i;
    }

    return ptr;
}

VertexArray *getVAO(GLMContext ctx, GLuint vao)
{
    VertexArray *ptr;

    ptr = (VertexArray *)searchHashTable(&STATE(vao_table), vao);

    if (!ptr)
    {
        ptr = newVAO(ctx, vao);

        insertHashElement(&STATE(vao_table), vao, ptr);
    }

    return ptr;
}

// the DSA entry points only accept a name that already exists
VertexArray *namedVAO(GLMContext ctx, GLuint vao)
{
    return (VertexArray *)searchHashTable(&STATE(vao_table), vao);
}

int isVAO(GLMContext ctx, GLuint vao)
{
    VertexArray *ptr;

    ptr = (VertexArray *)searchHashTable(&STATE(vao_table), vao);

    if (ptr)
        return 1;

    return 0;
}

void mglGenVertexArrays(GLMContext ctx, GLsizei n, GLuint *arrays)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    while(n--)
    {
        *arrays++ = getNewName(&STATE(vao_table));
    }
}

void mglBindVertexArray(GLMContext ctx, GLuint array)
{
    VertexArray *ptr;

    if (array == 0)
    {
        ptr = NULL;
    }
    else
    {
        //ERROR_CHECK_RETURN(isVAO(ctx, array), GL_INVALID_VALUE);

        ptr = getVAO(ctx, array);

        ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    }

    if (STATE(vao) != ptr)
    {
        STATE(vao) = ptr;
        STATE(dirty_bits) |= DIRTY_VAO;
    }
}

void mglDeleteVertexArrays(GLMContext ctx, GLsizei n, const GLuint *arrays)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    GLuint vao;

    while(n--)
    {
        vao = *arrays++;

        if (isVAO(ctx, vao))
        {
            VertexArray *ptr;

            ptr = (VertexArray *)searchHashTable(&STATE(vao_table), vao);

            if (ptr)
            {
                // remove current VAO if bound
                if (ptr == STATE(vao))
                {
                    mglBindVertexArray(ctx, 0);
                }

                // delete any mtl_data
            }

            deleteHashElement(&STATE(vao_table), vao);
        }
    }
}


GLboolean mglIsVertexArray(GLMContext ctx, GLuint array)
{
    return isVAO(ctx, array);
}

void mglGetVertexAttribdv(GLMContext ctx, GLuint index, GLenum pname, GLdouble *params)
{
    VertexArray *vao;
    Buffer *buf;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(index < ctx->state.max_vertex_attribs, GL_INVALID_VALUE);

    // the current value is context state and readable with no VAO bound
    if (pname == GL_CURRENT_VERTEX_ATTRIB)
    {
        readAttribConstant(ctx, index, params);

        return;
    }

    // a bad pname is rejected before the "is a VAO bound" test
    switch(pname)
    {
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
        case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
        case GL_VERTEX_ATTRIB_ARRAY_LONG:
        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
        case GL_VERTEX_ATTRIB_BINDING:
        case GL_VERTEX_ATTRIB_RELATIVE_OFFSET:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(ctx->state.vao, GL_INVALID_OPERATION);

    vao = ctx->state.vao;
    buf = VAO_BINDING(vao, index)->buffer;

    switch(pname)
    {
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
            if (buf)
            {
                *params = buf->name;
            }
            else
            {
                *params = 0;
            }
            break;

        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            if (vao->enabled_attribs & (0x1 << index))
            {
                *params = GL_TRUE;
            }
            else
            {
                *params = GL_FALSE;
            }
            break;

        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            *params = vao->attrib[index].size;
            break;

        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            *params = vao->attrib[index].stride;
            break;

        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
            *params = vao->attrib[index].type;
            break;

        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *params = vao->attrib[index].normalized;
            break;

        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
            *params = VAO_BINDING(vao, index)->divisor;
            break;

        case GL_VERTEX_ATTRIB_BINDING:
            *params = vao->attrib[index].buffer_bindingindex;
            break;

        case GL_VERTEX_ATTRIB_RELATIVE_OFFSET:
            *params = vao->attrib[index].relativeoffset;
            break;

        // MGL does not track these flags separately yet
        case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
        case GL_VERTEX_ATTRIB_ARRAY_LONG:
            *params = GL_FALSE;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetVertexAttribiv(GLMContext ctx, GLuint index, GLenum pname, GLint *params)
{
    double dparams[4];

    if (params == NULL)
        return;

    mglGetVertexAttribdv(ctx, index, pname, dparams);

    if (pname == GL_CURRENT_VERTEX_ATTRIB)
    {
        for(int i=0; i<4; i++)
            params[i] = (GLint)dparams[i];
    }
    else
    {
        *params = (GLint)dparams[0];
    }
}

void mglGetVertexAttribfv(GLMContext ctx, GLuint index, GLenum pname, GLfloat *params)
{
    double dparams[4];

    if (params == NULL)
        return;

    mglGetVertexAttribdv(ctx, index, pname, dparams);

    if (pname == GL_CURRENT_VERTEX_ATTRIB)
    {
        for(int i=0; i<4; i++)
            params[i] = (GLfloat)dparams[i];
    }
    else
    {
        *params = (GLfloat)dparams[0];
    }
}

void setVertexAttrib(GLMContext ctx, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
    GLsizei walk_stride = stride;

    // zero means tightly packed, so work the real one out from the format
    if (walk_stride == 0)
        walk_stride = genStrideFromTypeSize(type, size);

    ERROR_CHECK_RETURN(walk_stride, GL_INVALID_ENUM);

    ERROR_CHECK_RETURN(index < MAX_BINDABLE_BUFFERS, GL_INVALID_VALUE);

    VAO_ATTRIB_STATE(index).size = size;
    VAO_ATTRIB_STATE(index).type = type;
    VAO_ATTRIB_STATE(index).normalized = normalized;
    // GL hands these two straight back as they came in
    VAO_ATTRIB_STATE(index).stride = stride;
    VAO_ATTRIB_STATE(index).pointer = (GLubyte *)pointer - (GLubyte *)NULL;
    VAO_ATTRIB_STATE(index).relativeoffset = 0;
    VAO_ATTRIB_STATE(index).buffer_bindingindex = index;

    // this call is the binding API underneath: the pointer is the binding's
    // offset and the array buffer is what the binding holds
    BufferBinding *binding = &VAO_STATE(bindings[index]);

    binding->buffer = STATE(buffers[_ARRAY_BUFFER]);
    ERROR_CHECK_RETURN(binding->buffer, GL_INVALID_OPERATION);

    binding->offset = (GLubyte *)pointer - (GLubyte *)NULL;
    binding->stride = walk_stride;

    VAO_STATE(dirty_bits) |= DIRTY_VAO | DIRTY_VAO_BUFFER_BASE;
}

void mglVertexAttribPointer(GLMContext ctx, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)
{
    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(VAO(), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(stride >= 0, GL_INVALID_VALUE);

    // GL_INVALID_OPERATION is generated if zero is bound to the GL_ARRAY_BUFFER buffer object binding point and the pointer argument is not NULL.

    if (pointer != NULL)
    {
        Buffer *ptr;

        ptr = STATE(buffers[_ARRAY_BUFFER]);

        ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    }

    switch(type)
    {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_HALF_FLOAT:
        case GL_FLOAT:
        case GL_DOUBLE:
        case GL_FIXED:
        case GL_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    setVertexAttrib(ctx, index, size, type, normalized, stride, pointer);
}

void mglVertexAttribIPointer(GLMContext ctx, GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(VAO(), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(stride >= 0, GL_INVALID_VALUE);

    // GL_INVALID_OPERATION is generated if zero is bound to the GL_ARRAY_BUFFER buffer object binding point and the pointer argument is not NULL.

    if (pointer != NULL)
    {
        Buffer *ptr;

        ptr = STATE(buffers[_ARRAY_BUFFER]);

        ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    }

    // integer handling: only the integer types are legal here
    switch(type)
    {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    setVertexAttrib(ctx, index, size, type, 0, stride, pointer);
}


void mglVertexAttribLPointer(GLMContext ctx, GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)
{
    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(VAO(), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(stride >= 0, GL_INVALID_VALUE);

    // GL_INVALID_OPERATION is generated if zero is bound to the GL_ARRAY_BUFFER buffer object binding point and the pointer argument is not NULL.

    if (pointer != NULL)
    {
        Buffer *ptr;

        ptr = STATE(buffers[_ARRAY_BUFFER]);

        ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    }

    switch(type)
    {
        case GL_DOUBLE:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    setVertexAttrib(ctx, index, size, type, 0, stride, pointer);
}

void mglGetVertexAttribPointerv(GLMContext ctx, GLuint index, GLenum pname, void **pointer)
{
    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(VAO(), GL_INVALID_OPERATION);

    switch(pname)
    {
        case GL_VERTEX_ATTRIB_ARRAY_POINTER:
            *pointer = (void *)VAO_ATTRIB_STATE(index).pointer;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
            break;
    }
}

/*
 glVertexAttribPointer, glVertexAttribIPointer and glVertexAttribLPointer
 specify the location and data format of the array of generic vertex attributes
 at index index to use when rendering. size specifies the number of components
 per attribute and must be 1, 2, 3, 4, or GL_BGRA. type specifies the data type
 of each component, and stride specifies the byte stride from one attribute to
 the next, allowing vertices and attributes to be packed into a single array
 or stored in separate arrays.
*/

void mglEnableVertexArrayAttrib(GLMContext ctx, GLuint vaobj, GLuint index)
{
    VertexArray *ptr;

    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    ptr = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    ptr->enabled_attribs |= (0x1 << index);

    ptr->dirty_bits |= DIRTY_VAO_ATTRIB;
}

void mglDisableVertexArrayAttrib(GLMContext ctx, GLuint vaobj, GLuint index)
{
    VertexArray *ptr;

    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    ptr = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    ptr->enabled_attribs &= ~(0x1 << index);

    ptr->dirty_bits |= DIRTY_VAO_ATTRIB;
}

void mglEnableVertexAttribArray(GLMContext ctx, GLuint index)
{
    ERROR_CHECK_RETURN(VAO(), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    STATE(vao)->enabled_attribs |= (0x1 << index);

    VAO_STATE(dirty_bits) |= DIRTY_VAO;
}

void mglDisableVertexAttribArray(GLMContext ctx, GLuint index)
{
    ERROR_CHECK_RETURN(VAO(), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    STATE(vao)->enabled_attribs &= ~(0x1 << index);

    VAO_STATE(dirty_bits) |= DIRTY_VAO;
}

/*
 glCreateVertexArrays returns n previously unused vertex array object
names in arrays, each representing a new vertex array object initialized to the default state.
 */
void mglCreateVertexArrays(GLMContext ctx, GLsizei n, GLuint *arrays)
{
    ERROR_CHECK_RETURN(arrays, GL_INVALID_VALUE);

    mglGenVertexArrays(ctx, n, arrays);

    for(int i=0; i<n; i++)
    {
        VertexArray *ptr;

        ptr = getVAO(ctx, arrays[i]);

        ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    }
}

/*
 glVertexArrayElementBuffer binds a buffer object with id buffer to the
 element array buffer bind point of a vertex array object with id vaobj.
 If buffer is zero, any existing element array buffer binding to vaobj is removed.
 */
void mglVertexArrayElementBuffer(GLMContext ctx, GLuint vaobj, GLuint buffer)
{
    VertexArray *ptr;
    Buffer *buf_ptr;

    ptr = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    if (buffer == 0)
    {
        ptr->element_array.buffer = NULL;
        return;
    }

    buf_ptr = findBuffer(ctx, buffer);
    ERROR_CHECK_RETURN(buf_ptr, GL_INVALID_VALUE);

    ptr->element_array.buffer = buf_ptr;

    buf_ptr->data.dirty_bits |= DIRTY_BUFFER;
    ptr->dirty_bits |= DIRTY_FBO_BINDING;
}

void setVertexBindingIndex(GLMContext ctx, VertexArray *vao, GLuint attribindex, GLuint bindingindex)
{
    ERROR_CHECK_RETURN(attribindex < MAX_ATTRIBS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bindingindex < MAX_BINDABLE_BUFFERS, GL_INVALID_VALUE);

    vao->attrib[attribindex].buffer_bindingindex = bindingindex;

    vao->dirty_bits |= DIRTY_VAO_ATTRIB | DIRTY_VAO_BUFFER_BASE;
}

void mglVertexAttribBinding(GLMContext ctx, GLuint attribindex, GLuint bindingindex)
{
    VertexArray *ptr;

    ptr = ctx->state.vao;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setVertexBindingIndex(ctx, ptr, attribindex, bindingindex);
}

void mglVertexArrayAttribBinding(GLMContext ctx, GLuint vaobj, GLuint attribindex, GLuint bindingindex)
{
    VertexArray *ptr;

    ptr = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setVertexBindingIndex(ctx, ptr, attribindex, bindingindex);
}

void setAttribFormat(GLMContext ctx, VertexArray *vao, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
    ERROR_CHECK_RETURN(attribindex < MAX_ATTRIBS, GL_INVALID_VALUE);

    switch(type)
    {
        case GL_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            // these pack four components into one word
            ERROR_CHECK_RETURN(size == 4 || size == GL_BGRA, GL_INVALID_OPERATION);
            break;

        case GL_UNSIGNED_INT_10F_11F_11F_REV:
            ERROR_CHECK_RETURN(size == 3, GL_INVALID_OPERATION);
            break;

        case GL_UNSIGNED_BYTE:
            ERROR_CHECK_RETURN((size >= 1 && size <= 4) || size == GL_BGRA, GL_INVALID_VALUE);
            break;

        case GL_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FIXED:
        case GL_FLOAT:
        case GL_HALF_FLOAT:
        case GL_DOUBLE:
            ERROR_CHECK_RETURN((size >= 1 && size <=4), GL_INVALID_VALUE);
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    vao->attrib[attribindex].size = size;
    vao->attrib[attribindex].type = type;
    vao->attrib[attribindex].normalized = normalized;
    vao->attrib[attribindex].relativeoffset = relativeoffset;

    vao->dirty_bits |= DIRTY_VAO_ATTRIB;
}

void mglVertexAttribFormat(GLMContext ctx, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
    VertexArray *ptr;

    ptr = ctx->state.vao;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setAttribFormat(ctx, ptr, attribindex, size, type, normalized, relativeoffset);
}

void mglVertexArrayAttribFormat(GLMContext ctx, GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset)
{
    VertexArray *ptr;

    ptr = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setAttribFormat(ctx, ptr, attribindex, size, type, normalized, relativeoffset);
}

void setAttribIFormat(GLMContext ctx, VertexArray *vao, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    ERROR_CHECK_RETURN(attribindex < MAX_ATTRIBS, GL_INVALID_VALUE);

    switch(type)
    {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
            ERROR_CHECK_RETURN((size >= 1 && size <=4), GL_INVALID_VALUE);
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    vao->attrib[attribindex].size = size;
    vao->attrib[attribindex].type = type;
    vao->attrib[attribindex].normalized = 0;
    vao->attrib[attribindex].relativeoffset = relativeoffset;

    vao->dirty_bits |= DIRTY_VAO_ATTRIB;
}

void mglVertexAttribIFormat(GLMContext ctx, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    VertexArray *ptr;

    ptr = ctx->state.vao;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setAttribIFormat(ctx, ptr, attribindex, size, type, relativeoffset);
}

void mglVertexArrayAttribIFormat(GLMContext ctx, GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    VertexArray *ptr;

    ptr = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setAttribIFormat(ctx, ptr, attribindex, size, type, relativeoffset);
}

void setAttribLFormat(GLMContext ctx, VertexArray *vao, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    ERROR_CHECK_RETURN(attribindex < MAX_ATTRIBS, GL_INVALID_VALUE);

    switch(type)
    {
        case GL_DOUBLE:
            ERROR_CHECK_RETURN((size >= 1 && size <=4), GL_INVALID_VALUE);
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    vao->attrib[attribindex].size = size;
    vao->attrib[attribindex].type = type;
    vao->attrib[attribindex].normalized = 0;
    vao->attrib[attribindex].relativeoffset = relativeoffset;

    vao->dirty_bits |= DIRTY_VAO_ATTRIB;
}

void mglVertexAttribLFormat(GLMContext ctx, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    VertexArray *ptr;

    ptr = ctx->state.vao;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setAttribLFormat(ctx, ptr, attribindex, size, type, relativeoffset);
}

void mglVertexArrayAttribLFormat(GLMContext ctx, GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset)
{
    VertexArray *ptr;

    ptr = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setAttribLFormat(ctx, ptr, attribindex, size, type, relativeoffset);
}

void mglVertexAttribDivisor(GLMContext ctx, GLuint index, GLuint divisor)
{
    VertexArray *ptr;

    ERROR_CHECK_RETURN(index < MAX_ATTRIBS, GL_INVALID_VALUE);

    ptr = ctx->state.vao;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    // this one is the binding call too, on the binding of the same number
    ptr->attrib[index].buffer_bindingindex = index;
    setBindingDivisor(ctx, ptr, index, divisor);
}

void setBindingDivisor(GLMContext ctx, VertexArray *vao, GLuint bindingindex, GLuint divisor)
{
    ERROR_CHECK_RETURN(bindingindex < MAX_BINDABLE_BUFFERS, GL_INVALID_VALUE);

    vao->bindings[bindingindex].divisor = divisor;

    vao->dirty_bits |= DIRTY_VAO_ATTRIB | DIRTY_VAO_BUFFER_BASE;
}

void mglVertexBindingDivisor(GLMContext ctx, GLuint bindingindex, GLuint divisor)
{
    VertexArray *ptr;

    ptr = ctx->state.vao;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setBindingDivisor(ctx, ptr, bindingindex, divisor);
}

void mglVertexArrayBindingDivisor(GLMContext ctx, GLuint vaobj, GLuint bindingindex, GLuint divisor)
{
    VertexArray *ptr;

    ptr = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    setBindingDivisor(ctx, ptr, bindingindex, divisor);
}



void mglGetVertexArrayiv(GLMContext ctx, GLuint vaobj, GLenum pname, GLint *param)
{
    VertexArray *vao = namedVAO(ctx, vaobj);

    ERROR_CHECK_RETURN(vao, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(pname == GL_ELEMENT_ARRAY_BUFFER_BINDING, GL_INVALID_ENUM);

    *param = vao->element_array.buffer ? (GLint)vao->element_array.buffer->name : 0;
}

static bool vertexArrayIndexedParam(GLMContext ctx, VertexArray *vao, GLuint index, GLenum pname, GLint64 *out)
{
    VertexAttrib *att;

    ERROR_CHECK_RETURN_VALUE(vao, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(index < MAX_ATTRIBS, GL_INVALID_VALUE, false);

    att = &vao->attrib[index];

    switch(pname)
    {
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            *out = (vao->enabled_attribs & (0x1 << index)) ? GL_TRUE : GL_FALSE;
            return true;

        case GL_VERTEX_ATTRIB_ARRAY_SIZE:      *out = att->size; return true;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:      *out = att->type; return true;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:    *out = att->stride; return true;
        case GL_VERTEX_ATTRIB_RELATIVE_OFFSET: *out = att->relativeoffset; return true;
        case GL_VERTEX_ATTRIB_BINDING:         *out = att->buffer_bindingindex; return true;

        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
            *out = VAO_BINDING(vao, index)->divisor;
            return true;

        // these four are asked by binding number, not by attribute number
        case GL_VERTEX_BINDING_BUFFER:
        case GL_VERTEX_BINDING_OFFSET:
        case GL_VERTEX_BINDING_STRIDE:
        case GL_VERTEX_BINDING_DIVISOR:
        {
            ERROR_CHECK_RETURN_VALUE(index < MAX_BINDABLE_BUFFERS, GL_INVALID_VALUE, false);

            BufferBinding *binding = &vao->bindings[index];

            switch(pname)
            {
                case GL_VERTEX_BINDING_BUFFER:
                    *out = binding->buffer ? (GLint64)binding->buffer->name : 0;
                    break;
                case GL_VERTEX_BINDING_OFFSET:  *out = binding->offset;  break;
                case GL_VERTEX_BINDING_STRIDE:  *out = binding->stride;  break;
                default:                        *out = binding->divisor; break;
            }
            return true;
        }

        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
            *out = att->normalized ? GL_TRUE : GL_FALSE;
            return true;

        case GL_VERTEX_ATTRIB_ARRAY_INTEGER:
        case GL_VERTEX_ATTRIB_ARRAY_LONG:
            *out = GL_FALSE;
            return true;

        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        {
            Buffer *buf = VAO_BINDING(vao, index)->buffer;
            *out = buf ? (GLint64)buf->name : 0;
            return true;
        }

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }
}

void mglGetVertexArrayIndexediv(GLMContext ctx, GLuint vaobj, GLuint index, GLenum pname, GLint *param)
{
    GLint64 value = 0;

    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    if (vertexArrayIndexedParam(ctx, namedVAO(ctx, vaobj), index, pname, &value))
        *param = (GLint)value;
}

void mglGetVertexArrayIndexed64iv(GLMContext ctx, GLuint vaobj, GLuint index, GLenum pname, GLint64 *param)
{
    GLint64 value = 0;

    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(pname == GL_VERTEX_BINDING_OFFSET, GL_INVALID_ENUM);

    if (vertexArrayIndexedParam(ctx, namedVAO(ctx, vaobj), index, pname, &value))
        *param = value;
}


/* ---------- constant vertex attribute values ---------- */

// GL feeds these to the shader when an attribute array is disabled. Missing
// components default to 0, except the fourth which defaults to 1.
static void setAttribConstant(GLMContext ctx, GLuint index, AttribConstType type, const void *src, int comps)
{
    AttribConstant *ac;

    ERROR_CHECK_RETURN(index < ctx->state.max_vertex_attribs, GL_INVALID_VALUE);

    ctx->state.attrib_constant[index].d_valid = GL_FALSE;

    ac = &ctx->state.attrib_constant[index];

    for (int i = 0; i < 4; i++)
    {
        GLfloat fdef = (i == 3) ? 1.0f : 0.0f;
        GLuint  idef = (i == 3) ? 1u : 0u;

        switch (type)
        {
            case _ATTRIB_CONST_INT:
                ac->v.i[i] = (i < comps) ? ((const GLint *)src)[i] : (GLint)idef;
                break;

            case _ATTRIB_CONST_UINT:
                ac->v.u[i] = (i < comps) ? ((const GLuint *)src)[i] : idef;
                break;

            default:
                ac->v.f[i] = (i < comps) ? ((const GLfloat *)src)[i] : fdef;
                break;
        }
    }

    ac->type = type;

    ctx->state.attrib_constant_dirty |= (1 << index);
    ctx->state.dirty_bits |= DIRTY_STATE;
}

static void setAttribFloats(GLMContext ctx, GLuint index, const GLfloat *v, int comps)
{
    setAttribConstant(ctx, index, _ATTRIB_CONST_FLOAT, v, comps);
}

// glVertexAttribL*: keep the host doubles as well as the float copy
static void setAttribDoubles(GLMContext ctx, GLuint index, const GLdouble *v, int comps)
{
    GLfloat f[4];

    for (int i = 0; i < comps; i++)
        f[i] = (GLfloat)v[i];

    setAttribConstant(ctx, index, _ATTRIB_CONST_FLOAT, f, comps);

    if (index >= ctx->state.max_vertex_attribs)
        return;

    for (int i = 0; i < 4; i++)
        ctx->state.attrib_constant[index].d[i] = (i < comps) ? v[i] : ((i == 3) ? 1.0 : 0.0);

    ctx->state.attrib_constant[index].d_valid = GL_TRUE;
}

void mglVertexAttrib1f(GLMContext ctx, GLuint index, GLfloat x)
{
    GLfloat f[4] = { (GLfloat)x };

    setAttribFloats(ctx, index, f, 1);
}

void mglVertexAttrib2f(GLMContext ctx, GLuint index, GLfloat x, GLfloat y)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y };

    setAttribFloats(ctx, index, f, 2);
}

void mglVertexAttrib3f(GLMContext ctx, GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y, (GLfloat)z };

    setAttribFloats(ctx, index, f, 3);
}

void mglVertexAttrib4f(GLMContext ctx, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w };

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib1d(GLMContext ctx, GLuint index, GLdouble x)
{
    GLfloat f[4] = { (GLfloat)x };

    setAttribFloats(ctx, index, f, 1);
}

void mglVertexAttrib2d(GLMContext ctx, GLuint index, GLdouble x, GLdouble y)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y };

    setAttribFloats(ctx, index, f, 2);
}

void mglVertexAttrib3d(GLMContext ctx, GLuint index, GLdouble x, GLdouble y, GLdouble z)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y, (GLfloat)z };

    setAttribFloats(ctx, index, f, 3);
}

void mglVertexAttrib4d(GLMContext ctx, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w };

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib1s(GLMContext ctx, GLuint index, GLshort x)
{
    GLfloat f[4] = { (GLfloat)x };

    setAttribFloats(ctx, index, f, 1);
}

void mglVertexAttrib2s(GLMContext ctx, GLuint index, GLshort x, GLshort y)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y };

    setAttribFloats(ctx, index, f, 2);
}

void mglVertexAttrib3s(GLMContext ctx, GLuint index, GLshort x, GLshort y, GLshort z)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y, (GLfloat)z };

    setAttribFloats(ctx, index, f, 3);
}

void mglVertexAttrib4s(GLMContext ctx, GLuint index, GLshort x, GLshort y, GLshort z, GLshort w)
{
    GLfloat f[4] = { (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)w };

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib1fv(GLMContext ctx, GLuint index, const GLfloat *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0];

    setAttribFloats(ctx, index, f, 1);
}

void mglVertexAttrib2fv(GLMContext ctx, GLuint index, const GLfloat *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1];

    setAttribFloats(ctx, index, f, 2);
}

void mglVertexAttrib3fv(GLMContext ctx, GLuint index, const GLfloat *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1]; f[2] = (GLfloat)v[2];

    setAttribFloats(ctx, index, f, 3);
}

void mglVertexAttrib4fv(GLMContext ctx, GLuint index, const GLfloat *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1]; f[2] = (GLfloat)v[2]; f[3] = (GLfloat)v[3];

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib1dv(GLMContext ctx, GLuint index, const GLdouble *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0];

    setAttribFloats(ctx, index, f, 1);
}

void mglVertexAttrib2dv(GLMContext ctx, GLuint index, const GLdouble *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1];

    setAttribFloats(ctx, index, f, 2);
}

void mglVertexAttrib3dv(GLMContext ctx, GLuint index, const GLdouble *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1]; f[2] = (GLfloat)v[2];

    setAttribFloats(ctx, index, f, 3);
}

void mglVertexAttrib4dv(GLMContext ctx, GLuint index, const GLdouble *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1]; f[2] = (GLfloat)v[2]; f[3] = (GLfloat)v[3];

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib1sv(GLMContext ctx, GLuint index, const GLshort *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0];

    setAttribFloats(ctx, index, f, 1);
}

void mglVertexAttrib2sv(GLMContext ctx, GLuint index, const GLshort *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1];

    setAttribFloats(ctx, index, f, 2);
}

void mglVertexAttrib3sv(GLMContext ctx, GLuint index, const GLshort *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1]; f[2] = (GLfloat)v[2];

    setAttribFloats(ctx, index, f, 3);
}

void mglVertexAttrib4sv(GLMContext ctx, GLuint index, const GLshort *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    f[0] = (GLfloat)v[0]; f[1] = (GLfloat)v[1]; f[2] = (GLfloat)v[2]; f[3] = (GLfloat)v[3];

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4bv(GLMContext ctx, GLuint index, const GLbyte *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = (GLfloat)v[i];

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4ubv(GLMContext ctx, GLuint index, const GLubyte *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = (GLfloat)v[i];

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4iv(GLMContext ctx, GLuint index, const GLint *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = (GLfloat)v[i];

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4uiv(GLMContext ctx, GLuint index, const GLuint *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = (GLfloat)v[i];

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4usv(GLMContext ctx, GLuint index, const GLushort *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = (GLfloat)v[i];

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4Nbv(GLMContext ctx, GLuint index, const GLbyte *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = fmaxf((GLfloat)v[i] / 127.0f, -1.0f);

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4Nsv(GLMContext ctx, GLuint index, const GLshort *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = fmaxf((GLfloat)v[i] / 32767.0f, -1.0f);

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4Niv(GLMContext ctx, GLuint index, const GLint *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = fmaxf((GLfloat)v[i] / 2147483647.0f, -1.0f);

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4Nubv(GLMContext ctx, GLuint index, const GLubyte *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = (GLfloat)v[i] / 255.0f;

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4Nusv(GLMContext ctx, GLuint index, const GLushort *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = (GLfloat)v[i] / 65535.0f;

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4Nuiv(GLMContext ctx, GLuint index, const GLuint *v)
{
    GLfloat f[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        f[i] = (GLfloat)v[i] / 4294967295.0f;

    setAttribFloats(ctx, index, f, 4);
}

void mglVertexAttrib4Nub(GLMContext ctx, GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w)
{
    GLubyte v[4] = { x, y, z, w };

    mglVertexAttrib4Nubv(ctx, index, v);
}

/* ---------- integer, double and packed constant attributes ---------- */

static GLboolean unpackPackedAttrib(GLuint value, GLenum type, GLboolean normalized, GLfloat *out)
{
    GLint sx, sy, sz, sw;
    GLuint ux, uy, uz, uw;

    switch (type)
    {
        case GL_INT_2_10_10_10_REV:
            sx = (GLint)(value & 0x3FF);
            sy = (GLint)((value >> 10) & 0x3FF);
            sz = (GLint)((value >> 20) & 0x3FF);
            sw = (GLint)((value >> 30) & 0x3);

            // two's complement in 10 and 2 bit fields
            sx = (sx ^ 0x200) - 0x200;
            sy = (sy ^ 0x200) - 0x200;
            sz = (sz ^ 0x200) - 0x200;
            sw = (sw ^ 0x2) - 0x2;

            if (normalized)
            {
                out[0] = fmaxf((GLfloat)sx / 511.0f, -1.0f);
                out[1] = fmaxf((GLfloat)sy / 511.0f, -1.0f);
                out[2] = fmaxf((GLfloat)sz / 511.0f, -1.0f);
                out[3] = fmaxf((GLfloat)sw, -1.0f);
            }
            else
            {
                out[0] = (GLfloat)sx;
                out[1] = (GLfloat)sy;
                out[2] = (GLfloat)sz;
                out[3] = (GLfloat)sw;
            }

            return GL_TRUE;

        case GL_UNSIGNED_INT_2_10_10_10_REV:
            ux = value & 0x3FF;
            uy = (value >> 10) & 0x3FF;
            uz = (value >> 20) & 0x3FF;
            uw = (value >> 30) & 0x3;

            if (normalized)
            {
                out[0] = (GLfloat)ux / 1023.0f;
                out[1] = (GLfloat)uy / 1023.0f;
                out[2] = (GLfloat)uz / 1023.0f;
                out[3] = (GLfloat)uw / 3.0f;
            }
            else
            {
                out[0] = (GLfloat)ux;
                out[1] = (GLfloat)uy;
                out[2] = (GLfloat)uz;
                out[3] = (GLfloat)uw;
            }

            return GL_TRUE;

        case GL_UNSIGNED_INT_10F_11F_11F_REV:
            out[0] = mglSmallFloatToFloat(value & 0x7FF, 6, 5);
            out[1] = mglSmallFloatToFloat((value >> 11) & 0x7FF, 6, 5);
            out[2] = mglSmallFloatToFloat((value >> 22) & 0x3FF, 5, 5);
            out[3] = 1.0f;

            return GL_TRUE;
    }

    return GL_FALSE;
}

// the current value in whatever type the caller asked for
static int readAttribConstant(GLMContext ctx, GLuint index, GLdouble *out)
{
    AttribConstant *ac = &ctx->state.attrib_constant[index];

    if (ac->d_valid)
    {
        for (int i = 0; i < 4; i++)
            out[i] = ac->d[i];

        return 4;
    }

    for (int i = 0; i < 4; i++)
    {
        switch (ac->type)
        {
            case _ATTRIB_CONST_INT:  out[i] = ac->v.i[i]; break;
            case _ATTRIB_CONST_UINT: out[i] = ac->v.u[i]; break;
            default:                 out[i] = ac->v.f[i]; break;
        }
    }

    return 4;
}

void mglVertexAttribI1i(GLMContext ctx, GLuint index, GLint x)
{
    GLint t[4] = { x };

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, t, 1);
}

void mglVertexAttribI2i(GLMContext ctx, GLuint index, GLint x, GLint y)
{
    GLint t[4] = { x, y };

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, t, 2);
}

void mglVertexAttribI3i(GLMContext ctx, GLuint index, GLint x, GLint y, GLint z)
{
    GLint t[4] = { x, y, z };

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, t, 3);
}

void mglVertexAttribI4i(GLMContext ctx, GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    GLint t[4] = { x, y, z, w };

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, t, 4);
}

void mglVertexAttribI1ui(GLMContext ctx, GLuint index, GLuint x)
{
    GLuint t[4] = { x };

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, t, 1);
}

void mglVertexAttribI2ui(GLMContext ctx, GLuint index, GLuint x, GLuint y)
{
    GLuint t[4] = { x, y };

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, t, 2);
}

void mglVertexAttribI3ui(GLMContext ctx, GLuint index, GLuint x, GLuint y, GLuint z)
{
    GLuint t[4] = { x, y, z };

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, t, 3);
}

void mglVertexAttribI4ui(GLMContext ctx, GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    GLuint t[4] = { x, y, z, w };

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, t, 4);
}

void mglVertexAttribI1iv(GLMContext ctx, GLuint index, const GLint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, v, 1);
}

void mglVertexAttribI2iv(GLMContext ctx, GLuint index, const GLint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, v, 2);
}

void mglVertexAttribI3iv(GLMContext ctx, GLuint index, const GLint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, v, 3);
}

void mglVertexAttribI4iv(GLMContext ctx, GLuint index, const GLint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, v, 4);
}

void mglVertexAttribI1uiv(GLMContext ctx, GLuint index, const GLuint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, v, 1);
}

void mglVertexAttribI2uiv(GLMContext ctx, GLuint index, const GLuint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, v, 2);
}

void mglVertexAttribI3uiv(GLMContext ctx, GLuint index, const GLuint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, v, 3);
}

void mglVertexAttribI4uiv(GLMContext ctx, GLuint index, const GLuint *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, v, 4);
}

void mglVertexAttribI4bv(GLMContext ctx, GLuint index, const GLbyte *v)
{
    GLint t[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        t[i] = (GLint)v[i];

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, t, 4);
}

void mglVertexAttribI4sv(GLMContext ctx, GLuint index, const GLshort *v)
{
    GLint t[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        t[i] = (GLint)v[i];

    setAttribConstant(ctx, index, _ATTRIB_CONST_INT, t, 4);
}

void mglVertexAttribI4ubv(GLMContext ctx, GLuint index, const GLubyte *v)
{
    GLuint t[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        t[i] = (GLuint)v[i];

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, t, 4);
}

void mglVertexAttribI4usv(GLMContext ctx, GLuint index, const GLushort *v)
{
    GLuint t[4];

    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    for (int i = 0; i < 4; i++)
        t[i] = (GLuint)v[i];

    setAttribConstant(ctx, index, _ATTRIB_CONST_UINT, t, 4);
}

void mglVertexAttribL1d(GLMContext ctx, GLuint index, GLdouble x)
{
    GLdouble t[4] = { x };

    setAttribDoubles(ctx, index, t, 1);
}

void mglVertexAttribL2d(GLMContext ctx, GLuint index, GLdouble x, GLdouble y)
{
    GLdouble t[4] = { x, y };

    setAttribDoubles(ctx, index, t, 2);
}

void mglVertexAttribL3d(GLMContext ctx, GLuint index, GLdouble x, GLdouble y, GLdouble z)
{
    GLdouble t[4] = { x, y, z };

    setAttribDoubles(ctx, index, t, 3);
}

void mglVertexAttribL4d(GLMContext ctx, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    GLdouble t[4] = { x, y, z, w };

    setAttribDoubles(ctx, index, t, 4);
}

void mglVertexAttribL1dv(GLMContext ctx, GLuint index, const GLdouble *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribDoubles(ctx, index, v, 1);
}

void mglVertexAttribL2dv(GLMContext ctx, GLuint index, const GLdouble *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribDoubles(ctx, index, v, 2);
}

void mglVertexAttribL3dv(GLMContext ctx, GLuint index, const GLdouble *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribDoubles(ctx, index, v, 3);
}

void mglVertexAttribL4dv(GLMContext ctx, GLuint index, const GLdouble *v)
{
    ERROR_CHECK_RETURN(v, GL_INVALID_VALUE);

    setAttribDoubles(ctx, index, v, 4);
}

void mglVertexAttribP1ui(GLMContext ctx, GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    GLfloat t[4];

    ERROR_CHECK_RETURN(unpackPackedAttrib(value, type, normalized, t), GL_INVALID_ENUM);

    setAttribConstant(ctx, index, _ATTRIB_CONST_FLOAT, t, 1);
}

void mglVertexAttribP2ui(GLMContext ctx, GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    GLfloat t[4];

    ERROR_CHECK_RETURN(unpackPackedAttrib(value, type, normalized, t), GL_INVALID_ENUM);

    setAttribConstant(ctx, index, _ATTRIB_CONST_FLOAT, t, 2);
}

void mglVertexAttribP3ui(GLMContext ctx, GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    GLfloat t[4];

    ERROR_CHECK_RETURN(unpackPackedAttrib(value, type, normalized, t), GL_INVALID_ENUM);

    setAttribConstant(ctx, index, _ATTRIB_CONST_FLOAT, t, 3);
}

void mglVertexAttribP4ui(GLMContext ctx, GLuint index, GLenum type, GLboolean normalized, GLuint value)
{
    GLfloat t[4];

    ERROR_CHECK_RETURN(unpackPackedAttrib(value, type, normalized, t), GL_INVALID_ENUM);

    setAttribConstant(ctx, index, _ATTRIB_CONST_FLOAT, t, 4);
}

void mglVertexAttribP1uiv(GLMContext ctx, GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    mglVertexAttribP1ui(ctx, index, type, normalized, value[0]);
}

void mglVertexAttribP2uiv(GLMContext ctx, GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    mglVertexAttribP2ui(ctx, index, type, normalized, value[0]);
}

void mglVertexAttribP3uiv(GLMContext ctx, GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    mglVertexAttribP3ui(ctx, index, type, normalized, value[0]);
}

void mglVertexAttribP4uiv(GLMContext ctx, GLuint index, GLenum type, GLboolean normalized, const GLuint *value)
{
    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    mglVertexAttribP4ui(ctx, index, type, normalized, value[0]);
}

void mglGetVertexAttribIiv(GLMContext ctx, GLuint index, GLenum pname, GLint *params)
{
    GLdouble v[4];
    GLint tmp = 0;

    if (params == NULL)
        return;

    ERROR_CHECK_RETURN(index < ctx->state.max_vertex_attribs, GL_INVALID_VALUE);

    if (pname == GL_CURRENT_VERTEX_ATTRIB)
    {
        readAttribConstant(ctx, index, v);

        for (int i = 0; i < 4; i++)
            params[i] = (GLint)v[i];

        return;
    }

    mglGetVertexAttribiv(ctx, index, pname, &tmp);

    *params = tmp;
}

void mglGetVertexAttribIuiv(GLMContext ctx, GLuint index, GLenum pname, GLuint *params)
{
    GLdouble v[4];
    GLint tmp = 0;

    if (params == NULL)
        return;

    ERROR_CHECK_RETURN(index < ctx->state.max_vertex_attribs, GL_INVALID_VALUE);

    if (pname == GL_CURRENT_VERTEX_ATTRIB)
    {
        readAttribConstant(ctx, index, v);

        for (int i = 0; i < 4; i++)
            params[i] = (GLuint)v[i];

        return;
    }

    mglGetVertexAttribiv(ctx, index, pname, &tmp);

    *params = (GLuint)tmp;
}

void mglGetVertexAttribLdv(GLMContext ctx, GLuint index, GLenum pname, GLdouble *params)
{
    if (params == NULL)
        return;

    ERROR_CHECK_RETURN(index < ctx->state.max_vertex_attribs, GL_INVALID_VALUE);

    if (pname == GL_CURRENT_VERTEX_ATTRIB)
    {
        readAttribConstant(ctx, index, params);

        return;
    }

    mglGetVertexAttribdv(ctx, index, pname, params);
}
