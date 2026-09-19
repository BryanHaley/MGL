/*
 * Copyright (C) Michael Larson on on 1/6/25.
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
 * uniforms.c
 * MGL
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "spirv_cross_c.h"

#include "shaders.h"
#include "programs.h"
#include "buffers.h"
#include "glm_context.h"
#include "mgl_log.h"


#pragma mark uniforms

static int opaqueCount(Program *ptr);
static SpirvResource *opaqueAt(Program *ptr, GLuint index);
// "u[0]" and "u" name the same uniform
static bool mglUniformBaseNameIs(const char *stored, const char *base)
{
    size_t sl;

    if (!stored || !base)
        return false;

    sl = strlen(stored);

    if (sl >= 3 && !strcmp(stored + sl - 3, "[0]"))
        sl -= 3;

    return sl == strlen(base) && !strncmp(stored, base, sl);
}



GLint  mglGetUniformLocation(GLMContext ctx, GLuint program, const GLchar *name)
{
    if (isProgram(ctx, program) == GL_FALSE)
    {
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, 0); // also may be GL_INVALID_VALUE ????

        return -1;
    }

    Program *ptr;

    ptr = getProgram(ctx, program);

    // this used to assert on the name and then dereference the pointer
    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_OPERATION, -1);

    if (ptr->linked_glsl_program == NULL)
    {
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, 0);

        return -1;
    }

    {
        // "u[3]" names the fourth element of an array uniform, whose location
        // is the array's plus three
        char base[256];
        GLint element = 0;
        size_t len = strlen(name);
        const char *open = (len && name[len - 1] == ']') ? strrchr(name, '[') : NULL;

        snprintf(base, sizeof base, "%s", name);

        if (open && open != name)
        {
            element = (GLint)strtol(open + 1, NULL, 10);
            snprintf(base, sizeof base, "%.*s", (int)(open - name), name);
        }

        for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        {
            SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];

            for (GLuint i = 0; i < list->count; i++)
            {
                SpirvResource *r = &list->list[i];
                GLint n;

                // a struct is not a uniform; only the leaves inside it are
                if (r->gl_type == 0 || r->name == NULL || r->location == MGL_NO_LOCATION)
                    continue;

                if (!strcmp(r->name, name))
                    return (GLint)r->location;

                n = r->array_size > 1 ? r->array_size : 1;

                if (element >= 0 && element < n && mglUniformBaseNameIs(r->name, base))
                    return (GLint)r->location + element;
            }
        }
    }

    // samplers and images are uniforms too, and the app needs a location to
    // point them at a texture unit with glUniform1i
    {
        char base[256];
        GLint element = 0;
        size_t len = strlen(name);
        const char *open = (len && name[len - 1] == ']') ? strrchr(name, '[') : NULL;
        int n = opaqueCount(ptr);

        snprintf(base, sizeof base, "%s", name);

        if (open && open != name)
        {
            element = (GLint)strtol(open + 1, NULL, 10);
            snprintf(base, sizeof base, "%.*s", (int)(open - name), name);
        }

        for (int i = 0; i < n; i++)
        {
            SpirvResource *res = opaqueAt(ptr, (GLuint)i);
            GLint size;

            if (res == NULL || res->name == NULL || res->location == MGL_NO_LOCATION)
                continue;

            if (!strcmp(res->name, name))
                return (GLint)res->location;

            size = res->array_size > 1 ? res->array_size : 1;

            if (element >= 0 && element < size && mglUniformBaseNameIs(res->name, base))
                return (GLint)res->location + element;
        }
    }

    return -1;
}

Program *findProgram(GLMContext ctx, GLuint program);

// GL exposes samplers and images as ordinary uniforms whose value names a
// texture unit. SPIRV-Cross files them under the opaque resource types, so
// they need their own pass over the reflection.
static const int mgl_opaque_res_types[] = {
    SPVC_RESOURCE_TYPE_SAMPLED_IMAGE,
    SPVC_RESOURCE_TYPE_SEPARATE_IMAGE,
    SPVC_RESOURCE_TYPE_STORAGE_IMAGE,
    SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS,
};
#define MGL_OPAQUE_RES_COUNT (int)(sizeof(mgl_opaque_res_types)/sizeof(mgl_opaque_res_types[0]))

static int opaqueCount(Program *ptr)
{
    int n = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        for (int t = 0; t < MGL_OPAQUE_RES_COUNT; t++)
            n += ptr->spirv_resources_list[stage][mgl_opaque_res_types[t]].count;

    return n;
}

static SpirvResource *opaqueAt(Program *ptr, GLuint index)
{
    GLuint seen = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        for (int t = 0; t < MGL_OPAQUE_RES_COUNT; t++)
        {
            SpirvResourceList *list = &ptr->spirv_resources_list[stage][mgl_opaque_res_types[t]];

            if (index < seen + list->count)
                return &list->list[index - seen];

            seen += list->count;
        }

    return NULL;
}

SpirvResource *mglOpaqueUniformByLocation(Program *ptr, GLint location)
{
    int n = opaqueCount(ptr);

    for (int i = 0; i < n; i++)
    {
        SpirvResource *res = opaqueAt(ptr, (GLuint)i);
        GLint size;

        if (res == NULL || res->location == MGL_NO_LOCATION)
            continue;

        size = res->array_size > 1 ? res->array_size : 1;

        if (location >= (GLint)res->location && location < (GLint)res->location + size)
            return res;
    }

    return NULL;
}



// Walks every plain uniform the linker saw, across all stages, in a stable order.
// index is what glGetUniformIndices and friends hand back.
// A plain uniform of struct type is not itself a GL uniform -- its leaves are,
// and they sit in the same list.
static bool uniformIsStructOwner(const SpirvResource *r)
{
    return r->gl_type == 0 && r->offset < 0;
}

static GLuint plainUniformCount(Program *ptr, int stage)
{
    SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];
    GLuint n = 0;

    for (GLuint i = 0; i < list->count; i++)
        if (!uniformIsStructOwner(&list->list[i]))
            n++;

    return n;
}

static SpirvResource *plainUniformAt(Program *ptr, int stage, GLuint index)
{
    SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];
    GLuint n = 0;

    for (GLuint i = 0; i < list->count; i++)
    {
        if (uniformIsStructOwner(&list->list[i]))
            continue;

        if (n == index)
            return &list->list[i];

        n++;
    }

    return NULL;
}

static int uniformCount(Program *ptr)
{
    int n = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        n += plainUniformCount(ptr, stage);

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        n += ptr->block_uniforms[stage].count;

    return n + opaqueCount(ptr);
}

static SpirvResource *uniformAt(Program *ptr, GLuint index)
{
    GLuint seen = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        GLuint n = plainUniformCount(ptr, stage);

        if (index < seen + n)
            return plainUniformAt(ptr, stage, index - seen);

        seen += n;
    }

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->block_uniforms[stage];

        if (index < seen + list->count)
            return &list->list[index - seen];

        seen += list->count;
    }

    return opaqueAt(ptr, index - seen);
}

// GL 4.6 section 7.3.1: an array uniform is named with "[0]" on the end, and
// either spelling finds it. Compare with the suffix ignored on both sides.
static bool uniformNameMatches(const char *stored, const char *want)
{
    size_t sl, wl;

    if (!stored || !want)
        return false;

    sl = strlen(stored);
    wl = strlen(want);

    if (sl >= 3 && !strcmp(stored + sl - 3, "[0]")) sl -= 3;
    if (wl >= 3 && !strcmp(want + wl - 3, "[0]"))   wl -= 3;

    return sl == wl && !strncmp(stored, want, sl);
}

static int uniformIndexByName(Program *ptr, const char *name)
{
    GLuint seen = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        GLuint n = plainUniformCount(ptr, stage);

        for (GLuint i = 0; i < n; i++)
        {
            SpirvResource *r = plainUniformAt(ptr, stage, i);

            if (r && uniformNameMatches(r->name, name))
                return (int)(seen + i);
        }

        seen += n;
    }

    // a name may also be a member of a uniform block
    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->block_uniforms[stage];

        for (GLuint i = 0; i < list->count; i++)
            if (uniformNameMatches(list->list[i].name, name))
                return (int)(seen + i);

        seen += list->count;
    }

    {
        int n = opaqueCount(ptr);

        for (int i = 0; i < n; i++)
        {
            SpirvResource *res = opaqueAt(ptr, (GLuint)i);

            if (res && uniformNameMatches(res->name, name))
                return (int)(seen + i);
        }
    }

    return -1;
}

// A block declared in two stages is one block to GL, not two. Counting it
// twice made every later block's index disagree with its own name.
static bool blockSeenEarlier(Program *ptr, int stage, GLuint b)
{
    const char *name = ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_BUFFER].list[b].name;

    if (!name)
        return false;

    for (int prev = _VERTEX_SHADER; prev <= stage; prev++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[prev][SPVC_RESOURCE_TYPE_UNIFORM_BUFFER];
        GLuint limit = (prev == stage) ? b : list->count;

        for (GLuint k = 0; k < limit; k++)
            if (list->list[k].name && !strcmp(list->list[k].name, name))
                return true;
    }

    return false;
}

// "uniform Block { ... } b[3];" is three blocks to GL, one per element.
// A block declared as an array is several GL blocks, even when the array has
// one element -- GL names that one "Block[0]".
static GLint blockInstances(const SpirvResource *r)
{
    if (!r->element_binding)
        return 1;

    return r->array_size > 1 ? r->array_size : 1;
}

static int uniformBlockCount(Program *ptr)
{
    int n = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_BUFFER];

        for (GLuint b = 0; b < list->count; b++)
            if (!blockSeenEarlier(ptr, stage, b))
                n += blockInstances(&list->list[b]);
    }

    return n;
}

// The resource holding block `index`, and which element of an instance array
// it is. element is -1 when the block is not an array.
static SpirvResource *uniformBlockAtElement(Program *ptr, GLuint index, GLint *element)
{
    GLuint seen = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_BUFFER];

        for (GLuint b = 0; b < list->count; b++)
        {
            GLint n;

            if (blockSeenEarlier(ptr, stage, b))
                continue;

            n = blockInstances(&list->list[b]);

            if (index < seen + (GLuint)n)
            {
                if (element)
                    *element = list->list[b].element_binding ? (GLint)(index - seen) : -1;

                return &list->list[b];
            }

            seen += (GLuint)n;
        }
    }

    return NULL;
}

static SpirvResource *uniformBlockAt(Program *ptr, GLuint index)
{
    return uniformBlockAtElement(ptr, index, NULL);
}

// The name GL answers with: "Block" on its own, or "Block[n]" for an element.
static void blockNameAt(Program *ptr, GLuint index, char *out, size_t size)
{
    GLint element = -1;
    SpirvResource *blk = uniformBlockAtElement(ptr, index, &element);

    if (!blk || !blk->name)
    {
        if (size) out[0] = 0;
        return;
    }

    if (element >= 0)
        snprintf(out, size, "%s[%d]", blk->name, element);
    else
        snprintf(out, size, "%s", blk->name);
}

static void copyName(const char *src, GLsizei bufSize, GLsizei *length, GLchar *dst)
{
    GLsizei n = 0;

    if (dst && bufSize > 0)
    {
        while (n < bufSize - 1 && src[n])
        {
            dst[n] = src[n];
            n++;
        }

        dst[n] = 0;
    }

    if (length)
        *length = n;
}

static void getUniformTyped(GLMContext ctx, GLuint program, GLint location,
                            GLsizei bufSize, void *params, GLenum dst_type);
static SpirvResource *plainUniformByLocation(Program *ptr, GLint location, GLint *element);
static GLsizei glTypeSizeBytes(GLenum type);
static size_t storedElementSize(Program *pp, SpirvResource *res);

void mglGetUniformfv(GLMContext ctx, GLuint program, GLint location, GLfloat *params)
{
    Program *ptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(location >= 0 && location < MAX_UNIFORM_LOCATIONS, GL_INVALID_OPERATION);

    // a sampler or image reads back as the unit it points at
    {
        SpirvResource *res = mglOpaqueUniformByLocation(ptr, location);

        if (res)
        {
            params[0] = res->tex_unit;
            return;
        }
    }

    getUniformTyped(ctx, program, location, -1, params, GL_FLOAT);
}

void mglGetUniformiv(GLMContext ctx, GLuint program, GLint location, GLint *params)
{
    Program *ptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(location >= 0 && location < MAX_UNIFORM_LOCATIONS, GL_INVALID_OPERATION);

    // a sampler or image reads back as the unit it points at
    {
        SpirvResource *res = mglOpaqueUniformByLocation(ptr, location);

        if (res)
        {
            params[0] = res->tex_unit;
            return;
        }
    }

    getUniformTyped(ctx, program, location, -1, params, GL_INT);
}


void mglGetUniformIndices(GLMContext ctx, GLuint program, GLsizei uniformCountArg, const GLchar *const*uniformNames, GLuint *uniformIndices)
{
    Program *ptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformCountArg >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformNames && uniformIndices, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < uniformCountArg; i++)
    {
        int idx = uniformNames[i] ? uniformIndexByName(ptr, uniformNames[i]) : -1;

        uniformIndices[i] = (idx < 0) ? GL_INVALID_INDEX : (GLuint)idx;
    }
}

void mglGetActiveUniformsiv(GLMContext ctx, GLuint program, GLsizei uniformCountArg, const GLuint *uniformIndices, GLenum pname, GLint *params)
{
    Program *ptr = findProgram(ctx, program);
    int total;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformCountArg >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformIndices && params, GL_INVALID_VALUE);

    total = uniformCount(ptr);

    for (GLsizei i = 0; i < uniformCountArg; i++)
    {
        SpirvResource *res;

        ERROR_CHECK_RETURN(uniformIndices[i] < (GLuint)total, GL_INVALID_VALUE);

        res = uniformAt(ptr, uniformIndices[i]);

        switch(pname)
        {
            case GL_UNIFORM_NAME_LENGTH:
                params[i] = (GLint)strlen(res->name) + 1;
                break;

            case GL_UNIFORM_SIZE:
                params[i] = res->array_size ? res->array_size : 1;
                break;

            case GL_UNIFORM_BLOCK_INDEX:
                params[i] = res->block_index;
                break;

            case GL_UNIFORM_OFFSET:
                params[i] = res->offset;
                break;

            case GL_UNIFORM_ARRAY_STRIDE:
                params[i] = res->array_stride;
                break;

            case GL_UNIFORM_MATRIX_STRIDE:
                params[i] = res->matrix_stride;
                break;

            case GL_UNIFORM_IS_ROW_MAJOR:
                params[i] = res->is_row_major;
                break;

            case GL_UNIFORM_TYPE:
                params[i] = (GLint)res->gl_type;
                break;

            default:
                ERROR_RETURN(GL_INVALID_ENUM);
        }
    }
}

void mglGetActiveUniformName(GLMContext ctx, GLuint program, GLuint uniformIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformName)
{
    Program *ptr = findProgram(ctx, program);
    SpirvResource *res;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformIndex < (GLuint)uniformCount(ptr), GL_INVALID_VALUE);

    res = uniformAt(ptr, uniformIndex);

    copyName(res->name, bufSize, length, uniformName);
}

void mglGetActiveUniform(GLMContext ctx, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    Program *ptr = findProgram(ctx, program);
    SpirvResource *res;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(index < (GLuint)uniformCount(ptr), GL_INVALID_VALUE);

    res = uniformAt(ptr, index);

    ERROR_CHECK_RETURN(res, GL_INVALID_VALUE);

    if (size) *size = res->array_size ? res->array_size : 1;
    if (type) *type = res->gl_type;

    if (name) copyName(res->name, bufSize, length, name);
    else if (length) *length = 0;
}

GLuint  mglGetUniformBlockIndex(GLMContext ctx, GLuint program, const GLchar *uniformBlockName)
{
    if (isProgram(ctx, program) == GL_FALSE)
    {
        ERROR_RETURN_VALUE(GL_INVALID_VALUE, GL_INVALID_INDEX);
    }

    Program *ptr;

    ptr = getProgram(ctx, program);

    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_VALUE, GL_INVALID_INDEX);
    ERROR_CHECK_RETURN_VALUE(ptr->linked_glsl_program, GL_INVALID_OPERATION, GL_INVALID_INDEX);

    // GL wants the block's index, which is what the block queries take
    for (GLuint i = 0; i < (GLuint)uniformBlockCount(ptr); i++)
    {
        char nm[256];

        blockNameAt(ptr, i, nm, sizeof nm);

        if (nm[0] && !strcmp(nm, uniformBlockName))
            return i;
    }

    return GL_INVALID_INDEX;
}

// A block is referenced by a stage if that stage's reflection lists it.
static GLint uniformBlockInStage(Program *ptr, SpirvResource *blk, int stage)
{
    SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_BUFFER];

    for (GLuint i = 0; i < list->count; i++)
        if (list->list[i].name && blk->name && !strcmp(list->list[i].name, blk->name))
            return GL_TRUE;

    return GL_FALSE;
}

// what ACTIVE_UNIFORMS and ACTIVE_UNIFORM_INDICES must agree on
static GLint blockMemberCount(Program *ptr, GLuint block_index)
{
    GLint n = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->block_uniforms[stage];

        for (GLuint i = 0; i < list->count; i++)
            if (list->list[i].block_index == (GLint)block_index)
                n++;
    }

    return n;
}

void mglGetActiveUniformBlockiv(GLMContext ctx, GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params)
{
    Program *ptr = findProgram(ctx, program);
    SpirvResource *blk;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformBlockIndex < (GLuint)uniformBlockCount(ptr), GL_INVALID_VALUE);

    blk = uniformBlockAt(ptr, uniformBlockIndex);

    switch(pname)
    {
        case GL_UNIFORM_BLOCK_BINDING:
        {
            GLint element = -1;

            uniformBlockAtElement(ptr, uniformBlockIndex, &element);

            if (element >= 0 && blk->element_binding)
                *params = (GLint)blk->element_binding[element];
            else
                *params = (GLint)blk->binding;
            break;
        }

        case GL_UNIFORM_BLOCK_NAME_LENGTH:
        {
            char nm[256];

            blockNameAt(ptr, uniformBlockIndex, nm, sizeof nm);
            *params = (GLint)strlen(nm) + 1;
            break;
        }

        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS:
            *params = blockMemberCount(ptr, uniformBlockIndex);
            break;

        case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES:
        {
            // the caller's array is sized from ACTIVE_UNIFORMS above
            GLuint plain = 0, seen = 0, out = 0;
            GLuint room = (GLuint)blockMemberCount(ptr, uniformBlockIndex);

            for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
                plain += ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT].count;

            for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
            {
                SpirvResourceList *list = &ptr->block_uniforms[stage];

                for (GLuint i = 0; i < list->count && out < room; i++)
                    if (list->list[i].block_index == (GLint)uniformBlockIndex)
                        params[out++] = (GLint)(plain + seen + i);

                seen += list->count;
            }
            break;
        }

        case GL_UNIFORM_BLOCK_DATA_SIZE:
            *params = blk->block_size;
            break;

        case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
            *params = uniformBlockInStage(ptr, blk, _VERTEX_SHADER);
            break;

        case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
            *params = uniformBlockInStage(ptr, blk, _FRAGMENT_SHADER);
            break;

        case GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER:
            *params = uniformBlockInStage(ptr, blk, _COMPUTE_SHADER);
            break;

        case GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER:
            *params = uniformBlockInStage(ptr, blk, _GEOMETRY_SHADER);
            break;

        case GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER:
            *params = uniformBlockInStage(ptr, blk, _TESS_CONTROL_SHADER);
            break;

        case GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER:
            *params = uniformBlockInStage(ptr, blk, _TESS_EVALUATION_SHADER);
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetActiveUniformBlockName(GLMContext ctx, GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName)
{
    Program *ptr = findProgram(ctx, program);
    SpirvResource *blk;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformBlockIndex < (GLuint)uniformBlockCount(ptr), GL_INVALID_VALUE);

    {
        char nm[256];

        (void)blk;
        blockNameAt(ptr, uniformBlockIndex, nm, sizeof nm);
        copyName(nm, bufSize, length, uniformBlockName);
    }
}

// The binding uniform block `index` has now, or -1.
GLint mglUniformBlockBindingOf(Program *ptr, GLuint index)
{
    GLint element = -1;
    SpirvResource *blk;

    if (index >= (GLuint)uniformBlockCount(ptr))
        return -1;

    blk = uniformBlockAtElement(ptr, index, &element);

    if (blk == NULL)
        return -1;

    if (element >= 0 && blk->element_binding)
        return (GLint)blk->element_binding[element];

    return (GLint)blk->binding;
}

void mglUniformBlockBinding(GLMContext ctx, GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    Program *ptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformBlockIndex < (GLuint)uniformBlockCount(ptr), GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(uniformBlockBinding < MAX_UNIFORM_BUFFER_BINDINGS, GL_INVALID_VALUE);

    {
        GLint element = -1;
        SpirvResource *blk = uniformBlockAtElement(ptr, uniformBlockIndex, &element);

        // every element of an instance array is its own GL block, so writing
        // the resource's single binding made them all share one buffer
        if (blk && element >= 0 && blk->element_binding)
            blk->element_binding[element] = uniformBlockBinding;
        else if (blk)
            blk->binding = uniformBlockBinding;
    }

    ptr->dirty_bits |= DIRTY_PROGRAM;
}

// glUniform* writes the current program, or with none, the program a bound
// pipeline was told to take them with glActiveShaderProgram
static Program *uniformTarget(GLMContext ctx)
{
    if (ctx->state.program)
        return ctx->state.program;

    return ctx->state.program_pipeline ? ctx->state.program_pipeline->active : NULL;
}

bool checkUniformParams(GLMContext ctx, GLint location)
{
    Program* ptr = uniformTarget(ctx);
    
    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_OPERATION, false);

    ERROR_CHECK_RETURN_VALUE(location >= 0, GL_INVALID_OPERATION, false);
        
    ERROR_CHECK_RETURN_VALUE(location < MAX_UNIFORM_LOCATIONS, GL_INVALID_OPERATION, false);

    return true;
}

// uniform values live on the program. They used to live on the context keyed
// by location, so two programs that both used location 0 overwrote each other.
Program *programForUniform(GLMContext ctx, GLuint program)
{
    Program *pptr = findProgram(ctx, program);

    // MGL numbers shaders and programs apart, so a program's old name may
    // also be some shader's; it is still no program
    ERROR_CHECK_RETURN_VALUE(pptr, GL_INVALID_VALUE, NULL);

    ERROR_CHECK_RETURN_VALUE(pptr->link_status == GL_TRUE && pptr->linked_glsl_program,
                             GL_INVALID_OPERATION, NULL);

    return pptr;
}

void mglUniformD(GLMContext ctx, GLint location, void *ptr, GLsizei size);

// How many bytes one element of a GL uniform type occupies.
static GLsizei glTypeSizeBytes(GLenum type)
{
    switch (type)
    {
        case GL_FLOAT: case GL_INT: case GL_UNSIGNED_INT: case GL_BOOL:     return 4;
        case GL_FLOAT_VEC2: case GL_INT_VEC2: case GL_UNSIGNED_INT_VEC2:
        case GL_BOOL_VEC2:                                                  return 8;
        case GL_FLOAT_VEC3: case GL_INT_VEC3: case GL_UNSIGNED_INT_VEC3:
        case GL_BOOL_VEC3:                                                  return 12;
        case GL_FLOAT_VEC4: case GL_INT_VEC4: case GL_UNSIGNED_INT_VEC4:
        case GL_BOOL_VEC4:                                                  return 16;
        case GL_FLOAT_MAT2:                                                 return 16;
        case GL_FLOAT_MAT2x3: case GL_FLOAT_MAT3x2:                         return 24;
        case GL_FLOAT_MAT2x4: case GL_FLOAT_MAT4x2:                         return 32;
        case GL_FLOAT_MAT3:                                                 return 36;
        case GL_FLOAT_MAT3x4: case GL_FLOAT_MAT4x3:                         return 48;
        case GL_FLOAT_MAT4:                                                 return 64;
        case GL_DOUBLE:                                                     return 8;
        case GL_DOUBLE_VEC2:                                                return 16;
        case GL_DOUBLE_VEC3:                                                return 24;
        case GL_DOUBLE_VEC4:                                                return 32;
        case GL_DOUBLE_MAT2:                                                return 32;
        case GL_DOUBLE_MAT2x3: case GL_DOUBLE_MAT3x2:                       return 48;
        case GL_DOUBLE_MAT2x4: case GL_DOUBLE_MAT4x2:                       return 64;
        case GL_DOUBLE_MAT3:                                                return 72;
        case GL_DOUBLE_MAT3x4: case GL_DOUBLE_MAT4x3:                       return 96;
        case GL_DOUBLE_MAT4:                                                return 128;
        default:                                                            return 0;   // samplers and the unknown
    }
}

static bool isDoubleType(GLenum type)
{
    switch (type)
    {
        case GL_DOUBLE: case GL_DOUBLE_VEC2: case GL_DOUBLE_VEC3: case GL_DOUBLE_VEC4:
        case GL_DOUBLE_MAT2: case GL_DOUBLE_MAT3: case GL_DOUBLE_MAT4:
        case GL_DOUBLE_MAT2x3: case GL_DOUBLE_MAT2x4: case GL_DOUBLE_MAT3x2:
        case GL_DOUBLE_MAT3x4: case GL_DOUBLE_MAT4x2: case GL_DOUBLE_MAT4x3:
            return true;
        default:
            return false;
    }
}

// Bytes one element takes where MGL keeps it. A double uniform the reflection
// typed as its float twin still holds doubles.
static size_t storedElementSize(Program *pp, SpirvResource *res)
{
    size_t size = (size_t)glTypeSizeBytes(res->gl_type);

    if (pp->uniform_constants.elem_size[res->location] == sizeof(GLdouble) && !isDoubleType(res->gl_type))
        size *= 2;

    return size;
}

// The uniform declared at this location, or NULL if the program has none.
static SpirvResource *uniformByLocation(Program *ptr, GLint location)
{
    if (location < 0)
        return NULL;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];

        for (GLuint i = 0; i < list->count; i++)
            if (list->list[i].location == (GLuint)location)
                return &list->list[i];
    }

    return NULL;
}

// glUniform1f on a vec4 is GL_INVALID_OPERATION, and so is any other write whose
// width does not fit the declared type. Sampler uniforms take an int and are
// left to the existing path.
static bool uniformWriteFits(Program *ptr, GLint location, GLsizei size)
{
    SpirvResource *res = uniformByLocation(ptr, location);

    if (!res || res->gl_type == GL_NONE)
        return true;                        // nothing recorded, nothing to check

    GLsizei elem = glTypeSizeBytes(res->gl_type);

    if (elem == 0)
        return true;                        // a sampler, or a type we do not size

    // Only the too-narrow case is policed. That is the one the spec names --
    // glUniform1f on a vec4 -- and the one that silently corrupts a uniform.
    // The upper bound would need array reflection this does not have yet, and
    // guessing it wrong rejects writes that are perfectly legal.
    return size >= elem;
}

// The plain uniform that owns a location, or NULL. An array uniform owns
// array_size consecutive locations, one per element.
static SpirvResource *plainUniformByLocation(Program *ptr, GLint location, GLint *element)
{
    if (element)
        *element = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];

        for (GLuint i = 0; i < list->count; i++)
        {
            GLint base = (GLint)list->list[i].location;
            GLint n = list->list[i].array_size > 1 ? list->list[i].array_size : 1;

            if (list->list[i].location == MGL_NO_LOCATION)
                continue;

            if (location >= base && location < base + n)
            {
                if (element)
                    *element = location - base;

                return &list->list[i];
            }
        }
    }

    return NULL;
}

static GLuint boolComponentsFor(GLenum gl_type)
{
    switch (gl_type)
    {
        case GL_BOOL:      return 1;
        case GL_BOOL_VEC2: return 2;
        case GL_BOOL_VEC3: return 3;
        case GL_BOOL_VEC4: return 4;
        default:           return 0;
    }
}

// GL hands a bool uniform four bytes per component; Metal's bool is one byte,
// and a bool vector is two or four. Repack, or the shader reads three zeroes
// out of the first int and every bvec compares false.
static GLsizei packBoolUniform(GLenum gl_type, const void *src, GLsizei size,
                               GLubyte *out, GLsizei out_max)
{
    GLuint comps = boolComponentsFor(gl_type);
    GLuint stride = (comps == 1) ? 1u : ((comps == 2) ? 2u : 4u);
    const GLuint *in = (const GLuint *)src;
    GLsizei elements, total;

    if (comps == 0 || size <= 0 || (size % (GLsizei)(4 * comps)) != 0)
        return 0;

    elements = size / (GLsizei)(4 * comps);
    total = elements * (GLsizei)stride;
    total = (total + 3) & ~3;

    if (total > out_max)
        return 0;

    memset(out, 0, (size_t)total);

    for (GLsizei e = 0; e < elements; e++)
        for (GLuint c = 0; c < comps; c++)
            out[e * (GLsizei)stride + (GLsizei)c] = in[e * (GLsizei)comps + c] ? 1 : 0;

    return total;
}

// A uniform inside a plain struct shares one buffer with the rest of the
// struct, so it is written in place rather than replacing the whole thing.
static bool writeStructLeaf(GLMContext ctx, Program *pptr, SpirvResource *res,
                            GLint element, const void *ptr, GLsizei size)
{
    GLint owner = (GLint)res->binding;
    Buffer *buf;
    GLubyte packed[256];
    GLsizei packed_size = 0;
    GLint offset;

    if (owner < 0 || owner >= MAX_UNIFORM_LOCATIONS || res->block_size <= 0)
        return false;

    buf = pptr->uniform_constants.buffers[owner].buf;

    if (buf == NULL)
    {
        buf = newBuffer(ctx, GL_UNIFORM_BUFFER, owner);

        if (buf == NULL)
            return false;

        pptr->uniform_constants.buffers[owner].buf = buf;
    }

    // the whole struct has to be there before a member lands in the middle
    if (buf->data.buffer_data == 0 || buf->size < res->block_size)
        initBufferData(ctx, buf, res->block_size, NULL, true);

    if (buf->data.buffer_data == 0)
        return false;

    buf->size = res->block_size;

    if (boolComponentsFor(res->gl_type))
    {
        packed_size = packBoolUniform(res->gl_type, ptr, size, packed, (GLsizei)sizeof packed);

        if (packed_size)
        {
            ptr = packed;
            size = packed_size;
        }
    }

    offset = res->offset + element * res->array_stride;

    if (offset < 0 || offset + size > res->block_size)
        return false;

    memcpy((void *)(buf->data.buffer_data + offset), ptr, (size_t)size);
    buf->data.dirty_bits |= DIRTY_BUFFER_DATA;

    if (pptr->uniform_constants.elem_size[owner] == 0)
        pptr->uniform_constants.elem_size[owner] = sizeof(GLfloat);

    return true;
}

void programUniformWrite(GLMContext ctx, Program *pptr, GLint location, const void *ptr, GLsizei size);

// Where a uniform of this name ended up, across every stage. Unlike
// glGetUniformLocation this asks nothing about the program's link state, so
// the driver can find its own uniforms while the link is still going on.
GLint mglFindUniformByName(Program *pptr, const char *name)
{
    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &pptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];

        for (GLuint i = 0; i < list->count; i++)
        {
            SpirvResource *r = &list->list[i];

            if (r->gl_type && r->name && r->location != MGL_NO_LOCATION &&
                !strcmp(r->name, name))
                return (GLint)r->location;
        }
    }

    return -1;
}

// Look up the uniform the gl_NumSamples rewrite created. Called once per link;
// -1 means the fragment shader never mentioned it.
GLint mglFindNumSamplesLocation(Program *pptr)
{
    return mglFindUniformByName(pptr, MGL_NUM_SAMPLES_NAME);
}

// Write one int into a program's uniform storage without going through the
// current-program checks; the driver's own uniforms are not the application's.
void mglWriteProgramUniform(GLMContext ctx, Program *pptr, GLint location, GLint value)
{
    if (pptr == NULL || location < 0)
        return;

    programUniformWrite(ctx, pptr, location, &value, sizeof(GLint));
}

// GL's gl_NumSamples is the draw framebuffer's sample count, so it is written
// at draw time rather than by the application.
void mglWriteNumSamples(GLMContext ctx, Program *pptr, GLint samples)
{
    if (pptr == NULL || pptr->num_samples_loc < 0)
        return;

    programUniformWrite(ctx, pptr, pptr->num_samples_loc, &samples, sizeof(GLint));
}

void programUniformWrite(GLMContext ctx, Program *pptr, GLint location, const void *ptr, GLsizei size)
{
    Buffer *buf;

    // GL says an unknown uniform is silently ignored
    if (location == -1)
        return;

    ERROR_CHECK_RETURN(location >= 0, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(location < MAX_UNIFORM_LOCATIONS, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(size > 0, GL_INVALID_VALUE);

    {
        GLint element = 0;
        SpirvResource *leaf = plainUniformByLocation(pptr, location, &element);

        if (leaf && leaf->offset >= 0 &&
            writeStructLeaf(ctx, pptr, leaf, element, ptr, size))
            return;
    }

    GLint element = 0;
    SpirvResource *res = plainUniformByLocation(pptr, location, &element);
    size_t offset = 0;

    // an array lives in its first element's buffer, packed tight, so a write
    // that starts further in lands part way along it
    if (res && storedElementSize(pptr, res) > 0)
    {
        offset = (size_t)element * storedElementSize(pptr, res);
        location = (GLint)res->location;
    }

    buf = pptr->uniform_constants.buffers[location].buf;

    if (buf == NULL)
    {
        buf = newBuffer(ctx, GL_UNIFORM_BUFFER, location);
        ERROR_CHECK_RETURN(buf, GL_OUT_OF_MEMORY);
        pptr->uniform_constants.buffers[location].buf = buf;
    }

    {
        GLubyte packed[256];
        GLsizei packed_size = 0;

        if (res && boolComponentsFor(res->gl_type))
            packed_size = packBoolUniform(res->gl_type, ptr, size, packed, (GLsizei)sizeof packed);

        const void *data = packed_size ? (const void *)packed : ptr;
        size_t n = packed_size ? (size_t)packed_size : (size_t)size;
        size_t have = buf->data.buffer_data ? (size_t)buf->size : 0;

        if (offset == 0 && n >= have)
        {
            initBufferData(ctx, buf, n, (void *)data, true);
        }
        else
        {
            // keep the elements this write does not touch
            size_t total = offset + n > have ? offset + n : have;
            GLubyte *merged = (GLubyte *)calloc(1, total);

            ERROR_CHECK_RETURN(merged, GL_OUT_OF_MEMORY);

            if (have)
                memcpy(merged, (const void *)buf->data.buffer_data, have);

            memcpy(merged + offset, data, n);
            initBufferData(ctx, buf, total, merged, true);
            free(merged);
        }
    }

    if (pptr->uniform_constants.elem_size[location] == 0)
        pptr->uniform_constants.elem_size[location] = sizeof(GLfloat);
}

// Same write, but flags the location as holding doubles.
void programUniformWriteD(GLMContext ctx, Program *pptr, GLint location, const void *ptr, GLsizei size)
{
    GLint element = 0;
    SpirvResource *res = plainUniformByLocation(pptr, location, &element);

    // the flag goes where the values go, an array's first element, and before
    // the write so it sizes the elements as doubles
    GLint base = res ? (GLint)res->location : location;

    if (base >= 0 && base < MAX_UNIFORM_LOCATIONS)
        pptr->uniform_constants.elem_size[base] = sizeof(GLdouble);

    programUniformWrite(ctx, pptr, location, ptr, size);
}

void mglUniformD(GLMContext ctx, GLint location, void *ptr, GLsizei size)
{
    Program *pptr = uniformTarget(ctx);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_OPERATION);

    programUniformWriteD(ctx, pptr, location, ptr, size);
}

// A uniform the app never wrote still reads as zero in GL, so hand the draw a
// zero filled buffer instead of failing it. 256 bytes covers every uniform
// type the MSL side can ask for, including a mat4 array.
Buffer *programUniformDefaultBuffer(GLMContext ctx, Program *pptr, GLint location)
{
    static const GLubyte zeros[256] = {0};
    Buffer *buf;

    if (pptr == NULL)
        return NULL;

    if (location < 0 || location >= MAX_UNIFORM_LOCATIONS)
        return NULL;

    buf = pptr->uniform_constants.buffers[location].buf;

    if (buf)
        return buf;

    buf = newBuffer(ctx, GL_UNIFORM_BUFFER, location);

    if (buf == NULL)
        return NULL;

    pptr->uniform_constants.buffers[location].buf = buf;

    initBufferData(ctx, buf, sizeof(zeros), (void *)zeros, true);

    return buf;
}

void mglUniform(GLMContext ctx, GLint location, void *ptr, GLsizei size)
{
    Program *pptr = uniformTarget(ctx);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_OPERATION);

    programUniformWrite(ctx, pptr, location, ptr, size);
}

// The fixed width setters name their own component count, so a write narrower
// than the uniform it lands on is an error rather than a partial write --
// glUniform1f on a vec4. The v forms carry a count and may be filling an array,
// which MGL cannot size yet, so they go through mglUniform unchecked.
// Setting a sampler uniform picks a texture unit; it does not write into the
// uniform buffer the way a float or a vec4 does.
static bool writeOpaqueUniform(GLMContext ctx, Program *pptr, GLint location, void *ptr, GLsizei size)
{
    SpirvResource *res = mglOpaqueUniformByLocation(pptr, location);

    if (!res)
        return false;

    if (size < (GLsizei)sizeof(GLint))
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_OPERATION);
        return true;
    }

    GLint unit = *(GLint *)ptr;

    if (unit < 0 || unit >= TEXTURE_UNITS)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_VALUE);
        return true;
    }

    // the same sampler in another stage is the same uniform
    {
        int n = opaqueCount(pptr);

        for (int i = 0; i < n; i++)
        {
            SpirvResource *other = opaqueAt(pptr, (GLuint)i);

            if (other && other->location == res->location)
                other->tex_unit = unit;
        }
    }

    pptr->dirty_bits |= DIRTY_PROGRAM;

    return true;
}

static void mglUniformFixed(GLMContext ctx, GLint location, void *ptr, GLsizei size)
{
    Program *pptr = uniformTarget(ctx);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_OPERATION);

    if (writeOpaqueUniform(ctx, pptr, location, ptr, size))
        return;

    ERROR_CHECK_RETURN(uniformWriteFits(pptr, location, size), GL_INVALID_OPERATION);

    programUniformWrite(ctx, pptr, location, ptr, size);
}

static void mglUniformFixedD(GLMContext ctx, GLint location, void *ptr, GLsizei size)
{
    Program *pptr = uniformTarget(ctx);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(uniformWriteFits(pptr, location, size), GL_INVALID_OPERATION);

    programUniformWriteD(ctx, pptr, location, ptr, size);
}

void mglUniform1d(GLMContext ctx, GLint location, GLdouble x)
{
    mglUniformFixedD(ctx, location, &x, sizeof(GLdouble));
}

void mglUniform1dv(GLMContext ctx, GLint location, GLsizei count, const GLdouble *value)
{
    mglUniformD(ctx, location, (void *)value, count * sizeof(GLdouble));
}

void mglUniform1f(GLMContext ctx, GLint location, GLfloat v0)
{
    mglUniformFixed(ctx, location, &v0, sizeof(GLfloat));
}

void mglUniform1fv(GLMContext ctx, GLint location, GLsizei count, const GLfloat *value)
{
    mglUniform(ctx, location, (void *)value, count * sizeof(GLfloat));
}

void mglUniform1i(GLMContext ctx, GLint location, GLint v0)
{
    mglUniformFixed(ctx, location, &v0, sizeof(GLint));
}

void mglUniform1iv(GLMContext ctx, GLint location, GLsizei count, const GLint *value)
{
    mglUniform(ctx, location, (void *)value, count * sizeof(GLint));
}

void mglUniform1ui(GLMContext ctx, GLint location, GLuint v0)
{
    mglUniformFixed(ctx, location, &v0, sizeof(GLuint));
}

void mglUniform1uiv(GLMContext ctx, GLint location, GLsizei count, const GLuint *value)
{
    mglUniform(ctx, location, (void *)value, count * sizeof(GLuint));
}

void mglUniform2d(GLMContext ctx, GLint location, volatile GLdouble x, volatile GLdouble y)
{
    GLdouble data[] = {x, y};
    
    mglUniformFixedD(ctx, location, data, 2 * sizeof(GLdouble));
}

void mglUniform2dv(GLMContext ctx, GLint location, GLsizei count, const GLdouble *value)
{
    mglUniformD(ctx, location, (void *)value, 2 * count * sizeof(GLdouble));
}

void mglUniform2f(GLMContext ctx, GLint location, GLfloat v0, GLfloat v1)
{
    GLfloat data[] = {v0, v1};
    
    mglUniformFixed(ctx, location, data, 2 * sizeof(GLfloat));
}

void mglUniform2fv(GLMContext ctx, GLint location, GLsizei count, const GLfloat *value)
{
    mglUniform(ctx, location, (void *)value, 2 * count * sizeof(GLfloat));
}

void mglUniform2i(GLMContext ctx, GLint location, GLint v0, GLint v1)
{
    GLint data[] = {v0, v1};
    
    mglUniformFixed(ctx, location, data, 2 * sizeof(GLint));
}

void mglUniform2iv(GLMContext ctx, GLint location, GLsizei count, const GLint *value)
{
    mglUniform(ctx, location, (void *)value, 2 * count * sizeof(GLint));
}

void mglUniform2ui(GLMContext ctx, GLint location, GLuint v0, GLuint v1)
{
    GLuint data[] = {v0, v1};
    
    mglUniformFixed(ctx, location, data, 2 * sizeof(GLuint));
}

void mglUniform2uiv(GLMContext ctx, GLint location, GLsizei count, const GLuint *value)
{
    mglUniform(ctx, location, (void *)value, 2 * count * sizeof(GLuint));
}

void mglUniform3d(GLMContext ctx, GLint location, GLdouble x, GLdouble y, GLdouble z)
{
    GLdouble data[] = {x, y, z};
    
    mglUniformFixedD(ctx, location, data, 3 * sizeof(GLdouble));
}

void mglUniform3dv(GLMContext ctx, GLint location, GLsizei count, const GLdouble *value)
{
    mglUniformD(ctx, location, (void *)value, 3 * count * sizeof(GLdouble));
}

void mglUniform3f(GLMContext ctx, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    GLfloat data[] = {v0, v1, v2};
    
    mglUniformFixed(ctx, location, data, 3 * sizeof(GLfloat));
}

void mglUniform3fv(GLMContext ctx, GLint location, GLsizei count, const GLfloat *value)
{
    mglUniform(ctx, location, (void *)value, 3 * count * sizeof(GLfloat));
}

void mglUniform3i(GLMContext ctx, GLint location, GLint v0, GLint v1, GLint v2)
{
    GLint data[] = {v0, v1, v2};
    
    mglUniformFixed(ctx, location, data, 3 * sizeof(GLint));
}

void mglUniform3iv(GLMContext ctx, GLint location, GLsizei count, const GLint *value)
{
    mglUniform(ctx, location, (void *)value, 3 * count * sizeof(GLint));
}

void mglUniform3ui(GLMContext ctx, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    GLuint data[] = {v0, v1, v2};
    
    mglUniformFixed(ctx, location, (void *)data, 3 * sizeof(GLuint));
}

void mglUniform3uiv(GLMContext ctx, GLint location, GLsizei count, const GLuint *value)
{
    mglUniform(ctx, location, (void *)value, 3 * count * sizeof(GLuint));
}

void mglUniform4d(GLMContext ctx, GLint location, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
    GLdouble data[] = {x, y, z, w};
    
    mglUniformFixedD(ctx, location, data, 4 * sizeof(GLdouble));
}

void mglUniform4dv(GLMContext ctx, GLint location, GLsizei count, const GLdouble *value)
{
    mglUniformD(ctx, location, (void *)value, 4 * count * sizeof(GLdouble));
}

void mglUniform4f(GLMContext ctx, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    GLfloat data[] = {v0, v1, v2, v3};
    
    mglUniformFixed(ctx, location, (void *)data, 4 * sizeof(GLfloat));
}

void mglUniform4fv(GLMContext ctx, GLint location, GLsizei count, const GLfloat *value)
{
    mglUniform(ctx, location, (void *)value, 4 * count * sizeof(GLfloat));
}

void mglUniform4i(GLMContext ctx, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    GLint data[] = {v0, v1, v2, v3};
    
    mglUniformFixed(ctx, location, data, 4 * sizeof(GLint));
}

void mglUniform4iv(GLMContext ctx, GLint location, GLsizei count, const GLint *value)
{
    mglUniform(ctx, location, (void *)value, 4 * count * sizeof(GLint));
}

void mglUniform4ui(GLMContext ctx, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    GLuint data[] = {v0, v1, v2, v3};
    
    mglUniformFixed(ctx, location, data, 4 * sizeof(GLuint));
}

void mglUniform4uiv(GLMContext ctx, GLint location, GLsizei count, const GLuint *value)
{
    mglUniform(ctx, location, (void *)value, 4 * count * sizeof(GLuint));
}


// Macro to define matrix types
#define DEFINE_MATRIX_TYPE(_type_, _rows_, _cols_, _name_) \
typedef struct { \
    _type_ d[_rows_][_cols_]; \
} _name_;

// Macro to define transpose functions
#define DEFINE_TRANSPOSE_FUNC(_type_, _rows_, _cols_, _name_, _transposed_name_) \
void _name_##Transpose (const _name_ *matrix, _transposed_name_ *result) { \
    for (int i = 0; i < _rows_; i++) { \
        for (int j = 0; j < _cols_; j++) { \
            result->d[j][i] = matrix->d[i][j]; \
        } \
    } \
}

// Generalized function for uniform matrix upload
#define HANDLE_MATRIX_TRANSPOSE(_type_, _src_type_, _dst_type_, _transpose_func_) \
    /* a negative count is a bad argument, ahead of any state check */ \
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE); \
    if (count == 0) return; \
    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE); \
    /* double matrices must be flagged as such or readback reads them as floats */ \
    void (*_wr_)(GLMContext, GLint, void *, GLsizei) = \
        (sizeof(_type_) == sizeof(GLdouble)) ? mglUniformD : mglUniform; \
    if (transpose) { \
        const _src_type_ *src = (const _src_type_ *)value; \
        /* CRITICAL SECURITY FIX: Prevent integer overflow in uniform matrix allocation */ \
        if (count > SIZE_MAX / sizeof(_dst_type_)) { \
            MGL_ERR("MGL SECURITY ERROR: Uniform matrix count %d would cause allocation overflow\n", count); \
            STATE(error) = GL_OUT_OF_MEMORY; \
            return; \
        } \
        size_t alloc_size = count * sizeof(_dst_type_); \
        _dst_type_ *dst = (_dst_type_ *)malloc(alloc_size); \
        if (!dst) { \
            MGL_ERR("MGL SECURITY ERROR: Failed to allocate %zu bytes for uniform matrix\n", alloc_size); \
            STATE(error) = GL_OUT_OF_MEMORY; \
            return; \
        } \
        for (int i = 0; i < count; i++) { \
            _transpose_func_(&src[i], &dst[i]); \
        } \
        _wr_(ctx, location, (void *)dst, count * sizeof(_dst_type_)); \
        free(dst); \
    } else { \
        _wr_(ctx, location, (void *)value, count * sizeof(_src_type_)); \
    }

DEFINE_MATRIX_TYPE(GLdouble, 2, 2, Mat2x2dv)       // 2x2 matrix type
DEFINE_MATRIX_TYPE(GLdouble, 2, 2, Mat2x2dvTrans) // Transposed matrix type (same dimensions for 2x2)
DEFINE_TRANSPOSE_FUNC(GLdouble, 2, 2, Mat2x2dv, Mat2x2dvTrans)

void mglUniformMatrix2dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,        // Element type
                            Mat2x2dv,          // Source matrix type
                            Mat2x2dvTrans,     // Destination matrix type
                            Mat2x2dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 2, 2, Mat2x2fv)       // 2x2 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 2, 2, Mat2x2fvTrans) // Transposed matrix type (same dimensions for 2x2)
DEFINE_TRANSPOSE_FUNC(GLfloat, 2, 2, Mat2x2fv, Mat2x2fvTrans)

void mglUniformMatrix2fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat2x2fv,          // Source matrix type
                            Mat2x2fvTrans,     // Destination matrix type
                            Mat2x2fvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLdouble, 2, 3, Mat2x3dv)       // 2x3 matrix type
DEFINE_MATRIX_TYPE(GLdouble, 2, 3, Mat2x3dvTrans) // Transposed matrix type (same dimensions for 2x3)
DEFINE_TRANSPOSE_FUNC(GLdouble, 2, 3, Mat2x3dv, Mat2x3dvTrans)

void mglUniformMatrix2x3dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,         // Element type
                            Mat2x3dv,          // Source matrix type
                            Mat2x3dvTrans,     // Destination matrix type
                            Mat2x3dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 2, 3, Mat2x3fv)       // 2x3 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 2, 3, Mat2x3fvTrans) // Transposed matrix type (same dimensions for 2x3)
DEFINE_TRANSPOSE_FUNC(GLfloat, 2, 3, Mat2x3fv, Mat2x3fvTrans)

void mglUniformMatrix2x3fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat2x3fv,          // Source matrix type
                            Mat2x3fvTrans,     // Destination matrix type
                            Mat2x3fvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLdouble, 2, 4, Mat2x4dv)       // 2x4 matrix type
DEFINE_MATRIX_TYPE(GLdouble, 2, 4, Mat2x4dvTrans) // Transposed matrix type (same dimensions for 2x4)
DEFINE_TRANSPOSE_FUNC(GLdouble, 2, 4, Mat2x4dv, Mat2x4dvTrans)

void mglUniformMatrix2x4dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,        // Element type
                            Mat2x4dv,          // Source matrix type
                            Mat2x4dvTrans,     // Destination matrix type
                            Mat2x4dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 2, 4, Mat2x4fv)       // 2x4 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 2, 4, Mat2x4fvTrans) // Transposed matrix type (same dimensions for 2x4)
DEFINE_TRANSPOSE_FUNC(GLfloat, 2, 4, Mat2x4fv, Mat2x4fvTrans)

void mglUniformMatrix2x4fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat2x4fv,          // Source matrix type
                            Mat2x4fvTrans,     // Destination matrix type
                            Mat2x4fvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLdouble, 3, 3, Mat3x3dv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLdouble, 3, 3, Mat3x3dvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLdouble, 3, 3, Mat3x3dv, Mat3x3dvTrans)

void mglUniformMatrix3dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,        // Element type
                            Mat3x3dv,          // Source matrix type
                            Mat3x3dvTrans,     // Destination matrix type
                            Mat3x3dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 3, 3, Mat3x3fv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 3, 3, Mat3x3fvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLfloat, 3, 3, Mat3x3fv, Mat3x3fvTrans)

void mglUniformMatrix3fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat3x3fv,          // Source matrix type
                            Mat3x3fvTrans,     // Destination matrix type
                            Mat3x3fvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLdouble, 3, 2, Mat3x2dv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLdouble, 3, 2, Mat3x2dvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLdouble, 3, 2, Mat3x2dv, Mat3x2dvTrans)

void mglUniformMatrix3x2dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,        // Element type
                            Mat3x2dv,          // Source matrix type
                            Mat3x2dvTrans,     // Destination matrix type
                            Mat3x2dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 3, 2, Mat3x2fv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 3, 2, Mat3x2fvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLfloat, 3, 2, Mat3x2fv, Mat3x2fvTrans)

void mglUniformMatrix3x2fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat3x2fv,          // Source matrix type
                            Mat3x2fvTrans,     // Destination matrix type
                            Mat3x2fvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLdouble, 3, 4, Mat3x4dv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLdouble, 3, 4, Mat3x4dvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLdouble, 3, 4, Mat3x4dv, Mat3x4dvTrans)

void mglUniformMatrix3x4dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,        // Element type
                            Mat3x4dv,          // Source matrix type
                            Mat3x4dvTrans,     // Destination matrix type
                            Mat3x4dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 3, 4, Mat3x4fv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 3, 4, Mat3x4fvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLfloat, 3, 4, Mat3x4fv, Mat3x4fvTrans)

void mglUniformMatrix3x4fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat3x4fv,          // Source matrix type
                            Mat3x4fvTrans,     // Destination matrix type
                            Mat3x4fvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLdouble, 4, 4, Mat4x4dv)
DEFINE_MATRIX_TYPE(GLdouble, 4, 4, Mat4x4dvTrans)
DEFINE_TRANSPOSE_FUNC(GLdouble, 4, 4, Mat4x4dv, Mat4x4dvTrans)

void mglUniformMatrix4dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,        // Element type
                            Mat4x4dv,          // Source matrix type
                            Mat4x4dvTrans,     // Destination matrix type
                            Mat4x4dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 4, 4, Mat4x4fv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 4, 4, Mat4x4fvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLfloat, 4, 4, Mat4x4fv, Mat4x4fvTrans)

void mglUniformMatrix4fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat4x4fv,          // Source matrix type
                            Mat4x4fvTrans,     // Destination matrix type
                            Mat4x4fvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLdouble, 4, 2, Mat4x2dv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLdouble, 4, 2, Mat4x2dvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLdouble, 4, 2, Mat4x2dv, Mat4x2dvTrans)

void mglUniformMatrix4x2dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,        // Element type
                            Mat4x2dv,          // Source matrix type
                            Mat4x2dvTrans,     // Destination matrix type
                            Mat4x2dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 4, 2, Mat4x2fv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 4, 2, Mat4x2fvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLfloat, 4, 2, Mat4x2fv, Mat4x2fvTrans)

void mglUniformMatrix4x2fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat4x2fv,          // Source matrix type
                            Mat4x2fvTrans,     // Destination matrix type
                            Mat4x2fvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLdouble, 4, 3, Mat4x3dv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLdouble, 4, 3, Mat4x3dvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLdouble, 4, 3, Mat4x3dv, Mat4x3dvTrans)

void mglUniformMatrix4x3dv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLdouble,        // Element type
                            Mat4x3dv,          // Source matrix type
                            Mat4x3dvTrans,     // Destination matrix type
                            Mat4x3dvTranspose  // Transpose function
        );
}

DEFINE_MATRIX_TYPE(GLfloat, 4, 3, Mat4x3fv)       // 3x3 matrix type
DEFINE_MATRIX_TYPE(GLfloat, 4, 3, Mat4x3fvTrans) // Transposed matrix type (same dimensions for 3x3)
DEFINE_TRANSPOSE_FUNC(GLfloat, 4, 3, Mat4x3fv, Mat4x3fvTrans)

void mglUniformMatrix4x3fv(GLMContext ctx, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    HANDLE_MATRIX_TRANSPOSE(
                            GLfloat,        // Element type
                            Mat4x3fv,          // Source matrix type
                            Mat4x3fvTrans,     // Destination matrix type
                            Mat4x3fvTranspose  // Transpose function
        );
}


/* ---------- glProgramUniform* (DSA) ---------- */

static void puWrite(GLMContext ctx, GLuint program, GLint location, const void *ptr, GLsizei size)
{
    Program *pptr = programForUniform(ctx, program);

    if (pptr == NULL)
        return;

    programUniformWrite(ctx, pptr, location, ptr, size);
}

static void puV(GLMContext ctx, GLuint program, GLint location, GLsizei count, const void *value, int comps, size_t type_size)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    puWrite(ctx, program, location, value, (GLsizei)(count * comps * type_size));
}

static void puVf(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value, int comps)
{
    puV(ctx, program, location, count, value, comps, sizeof(GLfloat));
}

// Metal has no double, so the d forms narrow to float on the way in
static void puVd(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value, int comps)
{
    GLsizei n;
    GLfloat stack[64];
    GLfloat *buf;

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    n = count * comps;

    if (n <= 64)
    {
        buf = stack;
    }
    else
    {
        buf = (GLfloat *)malloc((size_t)n * sizeof(GLfloat));
        ERROR_CHECK_RETURN(buf, GL_OUT_OF_MEMORY);
    }

    for (GLsizei i = 0; i < n; i++)
        buf[i] = (GLfloat)value[i];

    puWrite(ctx, program, location, buf, n * (GLsizei)sizeof(GLfloat));

    if (buf != stack)
        free(buf);
}

// R columns of C rows, column major. Each matrix in the array transposes on
// its own; treating the whole array as one matrix mixes them together.
static void puTranspose(GLfloat *dst, const GLfloat *src, GLsizei count, int R, int C)
{
    for (GLsizei m = 0; m < count; m++)
    {
        const GLfloat *s = src + (size_t)m * R * C;
        GLfloat *d = dst + (size_t)m * R * C;

        for (int i = 0; i < R * C; i++)
            d[i] = s[(i % R) * C + (i / R)];
    }
}

static void puMatrixfv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, int R, int C)
{
    GLsizei n;
    GLfloat stack[64];
    GLfloat *buf;

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    n = count * R * C;

    if (transpose == GL_FALSE)
    {
        puWrite(ctx, program, location, value, n * (GLsizei)sizeof(GLfloat));
        return;
    }

    if (n <= 64)
    {
        buf = stack;
    }
    else
    {
        buf = (GLfloat *)malloc((size_t)n * sizeof(GLfloat));
        ERROR_CHECK_RETURN(buf, GL_OUT_OF_MEMORY);
    }

    puTranspose(buf, value, count, R, C);

    puWrite(ctx, program, location, buf, n * (GLsizei)sizeof(GLfloat));

    if (buf != stack)
        free(buf);
}

static void puMatrixdv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value, int R, int C)
{
    GLsizei n;
    GLfloat stack[64], tmp[64];
    GLfloat *buf, *narrowed;

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(value, GL_INVALID_VALUE);

    n = count * R * C;

    if (n <= 64)
    {
        narrowed = tmp;
        buf = stack;
    }
    else
    {
        narrowed = (GLfloat *)malloc((size_t)n * sizeof(GLfloat));
        ERROR_CHECK_RETURN(narrowed, GL_OUT_OF_MEMORY);

        buf = (GLfloat *)malloc((size_t)n * sizeof(GLfloat));

        if (buf == NULL)
        {
            free(narrowed);
            ERROR_RETURN(GL_OUT_OF_MEMORY);
        }
    }

    for (GLsizei i = 0; i < n; i++)
        narrowed[i] = (GLfloat)value[i];

    if (transpose)
        puTranspose(buf, narrowed, count, R, C);
    else
        memcpy(buf, narrowed, (size_t)n * sizeof(GLfloat));

    puWrite(ctx, program, location, buf, n * (GLsizei)sizeof(GLfloat));

    if (buf != stack)
    {
        free(buf);
        free(narrowed);
    }
}

void mglProgramUniform1f(GLMContext ctx, GLuint program, GLint location, GLfloat v0)
{
    puWrite(ctx, program, location, &v0, sizeof(GLfloat));
}

void mglProgramUniform2f(GLMContext ctx, GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    GLfloat data[] = {v0, v1};

    puWrite(ctx, program, location, data, 2 * sizeof(GLfloat));
}

void mglProgramUniform3f(GLMContext ctx, GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    GLfloat data[] = {v0, v1, v2};

    puWrite(ctx, program, location, data, 3 * sizeof(GLfloat));
}

void mglProgramUniform4f(GLMContext ctx, GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    GLfloat data[] = {v0, v1, v2, v3};

    puWrite(ctx, program, location, data, 4 * sizeof(GLfloat));
}

void mglProgramUniform1fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    puVf(ctx, program, location, count, value, 1);
}

void mglProgramUniform2fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    puVf(ctx, program, location, count, value, 2);
}

void mglProgramUniform3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    puVf(ctx, program, location, count, value, 3);
}

void mglProgramUniform4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    puVf(ctx, program, location, count, value, 4);
}

void mglProgramUniform1i(GLMContext ctx, GLuint program, GLint location, GLint v0)
{
    puWrite(ctx, program, location, &v0, sizeof(GLint));
}

void mglProgramUniform2i(GLMContext ctx, GLuint program, GLint location, GLint v0, GLint v1)
{
    GLint data[] = {v0, v1};

    puWrite(ctx, program, location, data, 2 * sizeof(GLint));
}

void mglProgramUniform3i(GLMContext ctx, GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    GLint data[] = {v0, v1, v2};

    puWrite(ctx, program, location, data, 3 * sizeof(GLint));
}

void mglProgramUniform4i(GLMContext ctx, GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    GLint data[] = {v0, v1, v2, v3};

    puWrite(ctx, program, location, data, 4 * sizeof(GLint));
}

void mglProgramUniform1iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value)
{
    puV(ctx, program, location, count, value, 1, sizeof(GLint));
}

void mglProgramUniform2iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value)
{
    puV(ctx, program, location, count, value, 2, sizeof(GLint));
}

void mglProgramUniform3iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value)
{
    puV(ctx, program, location, count, value, 3, sizeof(GLint));
}

void mglProgramUniform4iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value)
{
    puV(ctx, program, location, count, value, 4, sizeof(GLint));
}

void mglProgramUniform1ui(GLMContext ctx, GLuint program, GLint location, GLuint v0)
{
    puWrite(ctx, program, location, &v0, sizeof(GLuint));
}

void mglProgramUniform2ui(GLMContext ctx, GLuint program, GLint location, GLuint v0, GLuint v1)
{
    GLuint data[] = {v0, v1};

    puWrite(ctx, program, location, data, 2 * sizeof(GLuint));
}

void mglProgramUniform3ui(GLMContext ctx, GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    GLuint data[] = {v0, v1, v2};

    puWrite(ctx, program, location, data, 3 * sizeof(GLuint));
}

void mglProgramUniform4ui(GLMContext ctx, GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    GLuint data[] = {v0, v1, v2, v3};

    puWrite(ctx, program, location, data, 4 * sizeof(GLuint));
}

void mglProgramUniform1uiv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    puV(ctx, program, location, count, value, 1, sizeof(GLuint));
}

void mglProgramUniform2uiv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    puV(ctx, program, location, count, value, 2, sizeof(GLuint));
}

void mglProgramUniform3uiv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    puV(ctx, program, location, count, value, 3, sizeof(GLuint));
}

void mglProgramUniform4uiv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    puV(ctx, program, location, count, value, 4, sizeof(GLuint));
}

void mglProgramUniform1d(GLMContext ctx, GLuint program, GLint location, GLdouble v0)
{
    puVd(ctx, program, location, 1, &v0, 1);
}

void mglProgramUniform2d(GLMContext ctx, GLuint program, GLint location, GLdouble v0, GLdouble v1)
{
    GLdouble data[] = {v0, v1};

    puVd(ctx, program, location, 1, data, 2);
}

void mglProgramUniform3d(GLMContext ctx, GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2)
{
    GLdouble data[] = {v0, v1, v2};

    puVd(ctx, program, location, 1, data, 3);
}

void mglProgramUniform4d(GLMContext ctx, GLuint program, GLint location, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3)
{
    GLdouble data[] = {v0, v1, v2, v3};

    puVd(ctx, program, location, 1, data, 4);
}

void mglProgramUniform1dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    puVd(ctx, program, location, count, value, 1);
}

void mglProgramUniform2dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    puVd(ctx, program, location, count, value, 2);
}

void mglProgramUniform3dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    puVd(ctx, program, location, count, value, 3);
}

void mglProgramUniform4dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value)
{
    puVd(ctx, program, location, count, value, 4);
}

void mglProgramUniformMatrix2fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 2, 2);
}

void mglProgramUniformMatrix3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 3, 3);
}

void mglProgramUniformMatrix4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 4, 4);
}

void mglProgramUniformMatrix2x3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 2, 3);
}

void mglProgramUniformMatrix3x2fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 3, 2);
}

void mglProgramUniformMatrix2x4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 2, 4);
}

void mglProgramUniformMatrix4x2fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 4, 2);
}

void mglProgramUniformMatrix3x4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 3, 4);
}

void mglProgramUniformMatrix4x3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    puMatrixfv(ctx, program, location, count, transpose, value, 4, 3);
}

void mglProgramUniformMatrix2dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 2, 2);
}

void mglProgramUniformMatrix3dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 3, 3);
}

void mglProgramUniformMatrix4dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 4, 4);
}

void mglProgramUniformMatrix2x3dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 2, 3);
}

void mglProgramUniformMatrix3x2dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 3, 2);
}

void mglProgramUniformMatrix2x4dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 2, 4);
}

void mglProgramUniformMatrix4x2dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 4, 2);
}

void mglProgramUniformMatrix3x4dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 3, 4);
}

void mglProgramUniformMatrix4x3dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value)
{
    puMatrixdv(ctx, program, location, count, transpose, value, 4, 3);
}

/* ---------- uniform value getters ---------- */

// Values are stored as the bytes the app wrote. bufSize, where present, is a
// byte cap and a value that does not fit is an error rather than a truncation.
// One uniform value, element by element. An array lives in the buffer of its
// first element, packed tight, so element N is N values in. A uniform the app
// never wrote reads back as zero.
static void getUniformFrom(GLMContext ctx, Program *pp, GLint location,
                           GLsizei bufSize, void *params, GLenum dst_type);

static void getUniformTyped(GLMContext ctx, GLuint program, GLint location,
                            GLsizei bufSize, void *params, GLenum dst_type)
{
    Program *pp = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pp, GL_INVALID_VALUE);

    getUniformFrom(ctx, pp, location, bufSize, params, dst_type);
}

// A uniform's value straight off a program object, which may already have
// lost its name. Samplers and images read as their unit.
void mglReadUniform(GLMContext ctx, Program *pp, GLint location, void *params, GLenum as)
{
    SpirvResource *res = mglOpaqueUniformByLocation(pp, location);

    if (res)
    {
        ((GLint *)params)[0] = res->tex_unit;
        return;
    }

    getUniformFrom(ctx, pp, location, -1, params, as);
}

static void getUniformFrom(GLMContext ctx, Program *pp, GLint location,
                           GLsizei bufSize, void *params, GLenum dst_type)
{
    ERROR_CHECK_RETURN(location >= 0 && location < MAX_UNIFORM_LOCATIONS, GL_INVALID_OPERATION);

    GLint element = 0;
    SpirvResource *res = plainUniformByLocation(pp, location, &element);
    GLint base = res ? (GLint)res->location : location;
    const Buffer *buf = pp->uniform_constants.buffers[base].buf;
    size_t stored = pp->uniform_constants.elem_size[base] ? pp->uniform_constants.elem_size[base]
                                                         : sizeof(GLfloat);
    size_t per = res ? storedElementSize(pp, res) : 0;
    size_t have = (buf && buf->data.buffer_data) ? (size_t)buf->size : 0;

    ERROR_CHECK_RETURN(res || have, GL_INVALID_OPERATION);

    // a double uniform written through the float path is still sized as doubles
    if (per == 0)
        per = have;

    size_t offset = (size_t)element * per;
    GLsizei n = (GLsizei)(per / stored);
    bool src_is_double = (stored == sizeof(GLdouble));
    GLubyte value[128] = {0};

    if (n <= 0 || per > sizeof(value))
        return;

    // what was written, and zero for the rest
    if (have > offset)
        memcpy(value, (const GLubyte *)buf->data.buffer_data + offset,
               have - offset < per ? have - offset : per);

    const GLubyte *from = value;

    // the cap counts the bytes this call would write, not the bytes stored
    if (bufSize >= 0)
    {
        size_t elem = (dst_type == GL_DOUBLE) ? sizeof(GLdouble) : sizeof(GLfloat);

        ERROR_CHECK_RETURN((size_t)n * elem <= (size_t)bufSize, GL_INVALID_OPERATION);
    }

    const GLfloat *src = (const GLfloat *)from;

    switch (dst_type)
    {
        case GL_FLOAT:
            if (src_is_double)
                for (GLsizei i = 0; i < n; i++)
                    ((GLfloat *)params)[i] = (GLfloat)((const GLdouble *)src)[i];
            else
                memcpy(params, src, (size_t)n * sizeof(GLfloat));
            break;

        case GL_DOUBLE:
            if (src_is_double)
                memcpy(params, src, (size_t)n * sizeof(GLdouble));
            else
                for (GLsizei i = 0; i < n; i++)
                    ((GLdouble *)params)[i] = (GLdouble)src[i];
            break;

        // integer uniforms were stored as integers, so copy the bits straight
        case GL_INT:
        case GL_UNSIGNED_INT:
            memcpy(params, src, (size_t)n * sizeof(GLint));
            break;
    }
}

void mglGetUniformuiv(GLMContext ctx, GLuint program, GLint location, GLuint *params)
{
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    getUniformTyped(ctx, program, location, -1, params, GL_UNSIGNED_INT);
}

void mglGetUniformdv(GLMContext ctx, GLuint program, GLint location, GLdouble *params)
{
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    getUniformTyped(ctx, program, location, -1, params, GL_DOUBLE);
}

void mglGetnUniformfv(GLMContext ctx, GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    getUniformTyped(ctx, program, location, bufSize, params, GL_FLOAT);
}

void mglGetnUniformiv(GLMContext ctx, GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    getUniformTyped(ctx, program, location, bufSize, params, GL_INT);
}

void mglGetnUniformuiv(GLMContext ctx, GLuint program, GLint location, GLsizei bufSize, GLuint *params)
{
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    getUniformTyped(ctx, program, location, bufSize, params, GL_UNSIGNED_INT);
}

void mglGetnUniformdv(GLMContext ctx, GLuint program, GLint location, GLsizei bufSize, GLdouble *params)
{
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    getUniformTyped(ctx, program, location, bufSize, params, GL_DOUBLE);
}
