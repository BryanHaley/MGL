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
 * program.c
 * MGL
 *
 */

#include <stdio.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <glslang_c_interface.h>
#include <glslang_c_shader_types.h>
#include "spirv-tools/libspirv.h"
#include "spirv_cross_c.h"
#include "spirv.h"

#include <stdlib.h>
#include "glm_context.h"

// in mgl_spirv_opt.cpp -- folds called functions into their callers
extern bool mglInlineSpirv(const unsigned int *words, size_t count,
                           unsigned int **out_words, size_t *out_count);
extern int mglCountAtomicCounters(const unsigned int *words, size_t count);
static void assignPipeLocations(Program *ptr, spvc_compiler compiler, int stage);
// in mgl_spirv_opt.cpp -- moves atomic counters into buffer blocks
extern int mglLowerAtomicCounters(const unsigned int *words, size_t count,
                                  unsigned int **out_words, size_t *out_count,
                                  unsigned int *block_ids, unsigned int *block_bindings,
                                  int max_blocks);
#include "shaders.h"
#include "buffers.h"
#include "mgl_log.h"

// A block declared in two stages is one block to GL. Shared with uniforms.c's
// enumeration so a member's block index matches the block queries.
bool programResourceSeenEarlier(Program *ptr, int res_type, int stage, GLuint b)
{
    const char *name = ptr->spirv_resources_list[stage][res_type].list[b].name;

    if (!name)
        return false;

    for (int prev = _VERTEX_SHADER; prev <= stage; prev++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[prev][res_type];
        GLuint limit = (prev == stage) ? b : list->count;

        for (GLuint k = 0; k < limit; k++)
            if (list->list[k].name && !strcmp(list->list[k].name, name))
                return true;
    }

    return false;
}

bool programBlockSeenEarlier(Program *ptr, int stage, GLuint b)
{
    return programResourceSeenEarlier(ptr, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, stage, b);
}


// Program Pipeline management
ProgramPipeline *newProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    ProgramPipeline *ptr;

    ptr = (ProgramPipeline *)malloc(sizeof(ProgramPipeline));

    if (ptr == NULL)
    {
        MGL_ERR("MGL Error: %s: out of memory allocating a ProgramPipeline\n", __FUNCTION__);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

    bzero(ptr, sizeof(ProgramPipeline));
    ptr->name = pipeline;

    return ptr;
}

ProgramPipeline *findProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    return (ProgramPipeline *)searchHashTable(&STATE(program_pipeline_table), pipeline);
}

ProgramPipeline *getProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    ProgramPipeline *ptr = findProgramPipeline(ctx, pipeline);

    if (!ptr)
    {
        ptr = newProgramPipeline(ctx, pipeline);
        insertHashElement(&STATE(program_pipeline_table), pipeline, ptr);
    }

    return ptr;
}

// Transform Feedback management
TransformFeedback *newTransformFeedback(GLMContext ctx, GLuint name)
{
    TransformFeedback *ptr;

    ptr = (TransformFeedback *)malloc(sizeof(TransformFeedback));

    if (ptr == NULL)
    {
        MGL_ERR("MGL Error: %s: out of memory allocating a TransformFeedback\n", __FUNCTION__);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

    bzero(ptr, sizeof(TransformFeedback));
    ptr->name = name;
    ptr->target = GL_TRANSFORM_FEEDBACK;
    ptr->active = GL_FALSE;
    ptr->paused = GL_FALSE;
    ptr->primitive_mode = GL_NONE;

    return ptr;
}

TransformFeedback *findTransformFeedback(GLMContext ctx, GLuint name)
{
    return (TransformFeedback *)searchHashTable(&STATE(transform_feedback_table), name);
}

TransformFeedback *getTransformFeedback(GLMContext ctx, GLuint name)
{
    TransformFeedback *ptr = findTransformFeedback(ctx, name);

    if (!ptr)
    {
        ptr = newTransformFeedback(ctx, name);
        insertHashElement(&STATE(transform_feedback_table), name, ptr);
    }

    return ptr;
}

Program *newProgram(GLMContext ctx, GLuint program)
{
    Program *ptr;

    ptr = (Program *)malloc(sizeof(Program));

    if (ptr == NULL)
    {
        MGL_ERR("MGL Error: %s: out of memory allocating a Program\n", __FUNCTION__);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

    bzero(ptr, sizeof(Program));

    ptr->name = program;

    return ptr;
}

Program *getProgram(GLMContext ctx, GLuint program)
{
    Program *ptr;

    ptr = (Program *)searchHashTable(&STATE(program_table), program);

    if (!ptr)
    {
        ptr = newProgram(ctx, program);

        insertHashElement(&STATE(program_table), program, ptr);
    }

    return ptr;
}

int isProgram(GLMContext ctx, GLuint program)
{
    Program *ptr;

    ptr = (Program *)searchHashTable(&STATE(program_table), program);

    if (ptr)
        return 1;

    return 0;
}

Program *findProgram(GLMContext ctx, GLuint program)
{
    Program *ptr;

    ptr = (Program *)searchHashTable(&STATE(program_table), program);

    return ptr;
}

GLuint mglCreateProgram(GLMContext ctx)
{
    GLuint program;

    program = getNewName(&STATE(program_table));

    getProgram(ctx, program);

    return program;
}

static void freeSyntheticGeometry(Program *pptr);

void mglDropPipelineProgram(GLMContext ctx, ProgramPipeline *pp);

void mglFreeProgram(GLMContext ctx, Program *ptr)
{
    if (ptr->linked_glsl_program)
    {
        glslang_program_delete(ptr->linked_glsl_program);
        ptr->linked_glsl_program = NULL;
    }

    if (ptr->mtl_data)
    {
        ctx->mtl_funcs.mtlDeleteMTLObj(ctx, ptr->mtl_data);
    }

    for (int s = 0; s < _MAX_SHADER_TYPES; s++)
        free(ptr->stage_src[s]);

    // no pipeline may go on pointing at it
    for (size_t k = 0; k < STATE(program_pipeline_table).size; k++)
    {
        ProgramPipeline *pp = (ProgramPipeline *)STATE(program_pipeline_table).keys[k].data;
        bool used = false;

        if (pp == NULL)
            continue;

        for (int s = 0; s < _MAX_SHADER_TYPES; s++)
            if (pp->stage_programs[s] == ptr)
            {
                pp->stage_programs[s] = NULL;
                used = true;
            }

        if (pp->active == ptr)
            pp->active = NULL;

        if (pp->merged == ptr)
            pp->merged = NULL;
        else if (used)
            mglDropPipelineProgram(ctx, pp);
    }

    // what the geometry, subroutine and transform feedback rewrites left here
    mglFreeGeometryInfo(&ptr->geom);
    mglFreeCaptureInfo(&ptr->xfb);
    mglFreeResourceTable(&ptr->resources);
    freeSyntheticGeometry(ptr);

    for (GLint i = 0; i < ptr->attrib_bind_count; i++)
        free(ptr->attrib_binds[i].name);

    for (GLint i = 0; i < ptr->frag_bind_count; i++)
        free(ptr->frag_binds[i].name);

    ptr->attrib_bind_count = ptr->frag_bind_count = 0;

    free(ptr->gs_passthrough.ir);
    free(ptr->gs_passthrough.msl_str);
    free(ptr->gs_passthrough.entry_point);

    if (ptr->gs_passthrough.mtl_function)
        CFRelease(ptr->gs_passthrough.mtl_function);

    if (ptr->gs_passthrough.mtl_library)
        CFRelease(ptr->gs_passthrough.mtl_library);

    memset(&ptr->gs_passthrough, 0, sizeof(ptr->gs_passthrough));

    mglFreeCullInfo(&ptr->cull);

    {
        Spirv *cull[] = { &ptr->cull_capture, &ptr->cull_kernel };

        for (int c = 0; c < 2; c++)
        {
            free(cull[c]->ir);
            free(cull[c]->msl_str);
            free(cull[c]->entry_point);

            if (cull[c]->mtl_function)
                CFRelease(cull[c]->mtl_function);

            if (cull[c]->mtl_library)
                CFRelease(cull[c]->mtl_library);

            memset(cull[c], 0, sizeof(*cull[c]));
        }
    }

    for (GLsizei i = 0; i < ptr->xfb_varying_count; i++)
        free(ptr->xfb_varyings[i]);

    free(ptr->xfb_varyings);
    ptr->xfb_varyings = NULL;
    ptr->xfb_varying_count = 0;

    for(int i=0; i<_MAX_SHADER_TYPES; i++)
    {
        mglFreeSubroutineInfo(&ptr->subroutines[i]);
        free(ptr->subroutine_values[i]);
        ptr->subroutine_values[i] = NULL;

        // CRITICAL FIX: Add NULL checks before all free/release operations to prevent double-frees
        if (ptr->spirv[i].ir) {
            free(ptr->spirv[i].ir);
            ptr->spirv[i].ir = NULL;
        }
        if (ptr->spirv[i].msl_str) {
            free(ptr->spirv[i].msl_str);
            ptr->spirv[i].msl_str = NULL;
        }
        if (ptr->spirv[i].entry_point) {
            free(ptr->spirv[i].entry_point);
            ptr->spirv[i].entry_point = NULL;
        }
        if (ptr->spirv[i].mtl_function) {
            CFRelease(ptr->spirv[i].mtl_function);
            ptr->spirv[i].mtl_function = NULL;
        }
        if (ptr->spirv[i].mtl_library) {
            CFRelease(ptr->spirv[i].mtl_library);
            ptr->spirv[i].mtl_library = NULL;
        }
        
        for(int j=0; j<MAX_SPVC_RESOURCE_TYPES; j++)
        {
            // CRITICAL FIX: Add NULL checks and clear pointers to prevent double-frees
            if (ptr->spirv_resources_list[i][j].list) {
                free(ptr->spirv_resources_list[i][j].list);
                ptr->spirv_resources_list[i][j].list = NULL;
            }
        }
        
        if (ptr->shader_slots[i])
        {
            Shader *sptr = ptr->shader_slots[i];
            sptr->refcount--;
            if (sptr->refcount == 0 && sptr->delete_status)
            {
                deleteHashElement(&STATE(shader_table), sptr->name);
                mglFreeShader(ctx, sptr);
            }
        }
    }

    free(ptr);
}

void mglDeleteProgram(GLMContext ctx, GLuint program)
{
    Program *ptr;

    ptr = findProgram(ctx, program);

    if (!ptr)
    {
        // 0 is silently ignored, any other unknown name is an error
        if (program != 0)
            ERROR_RETURN(GL_INVALID_VALUE);

        return;
    }

    deleteHashElement(&STATE(program_table), program);
    
    ptr->delete_status = GL_TRUE;
    
    if (ptr->refcount == 0)
    {
        mglFreeProgram(ctx, ptr);
    }
}

GLboolean mglIsProgram(GLMContext ctx, GLuint program)
{
    if (isProgram(ctx, program))
        return GL_TRUE;

    return GL_FALSE;
}

void mglAttachShader(GLMContext ctx, GLuint program, GLuint shader)
{
    Program *pptr;
    Shader *sptr;
    GLuint index;

    sptr = findShader(ctx, shader);

    if (!sptr)
    {
        // CRITICAL FIX: Handle missing shader gracefully instead of crashing
        MGL_ERR("MGL ERROR: Shader %u not found in attach shader\n", shader);
        STATE(error) = GL_INVALID_VALUE;
        return;
    }

    pptr = findProgram(ctx, program);

    if (!pptr)
    {
        // CRITICAL FIX: Handle error gracefully instead of crashing
        MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
        STATE(error) = GL_INVALID_OPERATION;

        return;
    }

    index = sptr->glm_type;

    pptr->shader_slots[index] = sptr;
    sptr->refcount++;
    pptr->dirty_bits |= DIRTY_PROGRAM;
}

void mglDetachShader(GLMContext ctx, GLuint program, GLuint shader)
{
    Program *pptr;
    Shader *sptr;
    GLuint index;

    pptr = findProgram(ctx, program);
    if (!pptr)
    {
        // CRITICAL FIX: Handle error gracefully instead of crashing
        MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
        STATE(error) = GL_INVALID_OPERATION;
        return;
    }

    sptr = findShader(ctx, shader);

    if (!sptr)
    {
        // If not found in hash table, check if it is attached to the program
        for (int i=0; i<_MAX_SHADER_TYPES; i++) {
            if (pptr->shader_slots[i] && pptr->shader_slots[i]->name == shader) {
                sptr = pptr->shader_slots[i];
                break;
            }
        }
    }

    if (!sptr)
    {
        // CRITICAL FIX: Handle error gracefully instead of crashing
        MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
        STATE(error) = GL_INVALID_OPERATION;
        return;
    }

    index = sptr->glm_type;

    if (pptr->shader_slots[index] != sptr)
    {
        return;
    }

    pptr->shader_slots[index] = NULL;
    sptr->refcount--;
    
    if (sptr->refcount == 0 && sptr->delete_status)
    {
        deleteHashElement(&STATE(shader_table), sptr->name);
        mglFreeShader(ctx, sptr);
    }
    
    pptr->dirty_bits |= DIRTY_PROGRAM;
}

void error_callback(void *userdata, const char *error)
{
    // SPIRV-Cross always hands us a message; print nothing rather than crash
    if (error)
        DEBUG_PRINT("parseSPIRVShader error:%s\n", error);
}


static_assert(_VERTEX_SHADER == GLSLANG_STAGE_VERTEX, "_VERTEX_SHADER == GLSLANG_STAGE_VERTEX failed");
static_assert(_TESS_CONTROL_SHADER == GLSLANG_STAGE_TESSCONTROL, "_TESS_CONTROL_SHADER == GLSLANG_STAGE_TESSCONTROL failed");
static_assert(_TESS_EVALUATION_SHADER == GLSLANG_STAGE_TESSEVALUATION, "_TESS_EVALUATION_SHADER == GLSLANG_STAGE_TESSEVALUATION failed");
static_assert(_GEOMETRY_SHADER == GLSLANG_STAGE_GEOMETRY, "_GEOMETRY_SHADER == GLSLANG_STAGE_GEOMETRY failed");
static_assert(_FRAGMENT_SHADER == GLSLANG_STAGE_FRAGMENT, "_FRAGMENT_SHADER == GLSLANG_STAGE_FRAGMENT failed");
static_assert(_COMPUTE_SHADER == GLSLANG_STAGE_COMPUTE, "_COMPUTE_SHADER == GLSLANG_STAGE_COMPUTE failed");

bool addShadersToProgram(GLMContext ctx, Program *pptr, glslang_program_t *glsl_program)
{
    // add shaders
    for(int i=0;i<_MAX_SHADER_TYPES; i++)
    {
        Shader *ptr;

        ptr = pptr->shader_slots[i];

        if(ptr && ptr->compiled_glsl_shader)
        {
            glslang_program_add_shader(glsl_program, ptr->compiled_glsl_shader);
        }
        else if(ptr)
        {
            // attached but never compiled -- let the link report it
            MGL_ERR("MGL Error: shader %d attached to program %d never compiled\n", ptr->name, pptr->name);

            return false;
        }
    }

    return true;
}
// Sampler and image types, by dimension, arrayed-ness, depth and sample type.
static GLenum glSamplerTypeFromSpirv(spvc_compiler compiler, spvc_type type)
{
    (void)compiler;

    SpvDim dim = spvc_type_get_image_dimension(type);
    spvc_bool arrayed = spvc_type_get_image_arrayed(type);
    spvc_bool ms = spvc_type_get_image_multisampled(type);
    spvc_bool depth = spvc_type_get_image_is_depth(type);

    switch (dim)
    {
        case SpvDim1D:     return arrayed ? GL_SAMPLER_1D_ARRAY : GL_SAMPLER_1D;
        case SpvDim2D:
            if (ms)        return arrayed ? GL_SAMPLER_2D_MULTISAMPLE_ARRAY : GL_SAMPLER_2D_MULTISAMPLE;
            if (depth)     return arrayed ? GL_SAMPLER_2D_ARRAY_SHADOW : GL_SAMPLER_2D_SHADOW;
            return arrayed ? GL_SAMPLER_2D_ARRAY : GL_SAMPLER_2D;
        case SpvDim3D:     return GL_SAMPLER_3D;
        case SpvDimCube:
            if (depth)     return GL_SAMPLER_CUBE_SHADOW;
            return arrayed ? GL_SAMPLER_CUBE_MAP_ARRAY : GL_SAMPLER_CUBE;
        case SpvDimRect:   return depth ? GL_SAMPLER_2D_RECT_SHADOW : GL_SAMPLER_2D_RECT;
        case SpvDimBuffer: return GL_SAMPLER_BUFFER;
        default:           return GL_NONE;
    }
}

// Turn a SPIR-V type into the GL enum glGetActiveUniform is supposed to report.
// Without this every uniform reads back as GL_NONE, and MGL cannot see the
// size mismatch the spec requires it to reject.
static GLenum glTypeFromSpirv(spvc_compiler compiler, spvc_type_id type_id, GLint *array_size_out)
{
    spvc_type type = spvc_compiler_get_type_handle(compiler, type_id);

    if (array_size_out)
        *array_size_out = 1;

    if (!type)
        return GL_NONE;

    if (array_size_out && spvc_type_get_num_array_dimensions(type) > 0)
    {
        unsigned dim = spvc_type_get_array_dimension(type, 0);

        // a zero dimension is an unsized array, which GL reports as one element
        *array_size_out = dim ? (GLint)dim : 1;
    }

    unsigned vec = spvc_type_get_vector_size(type);
    unsigned cols = spvc_type_get_columns(type);

    switch (spvc_type_get_basetype(type))
    {
        case SPVC_BASETYPE_FP32:
            if (cols > 1)
            {
                // GL names matrices columns x rows, square ones by one number
                if (cols == 2) return vec == 2 ? GL_FLOAT_MAT2 : (vec == 3 ? GL_FLOAT_MAT2x3 : GL_FLOAT_MAT2x4);
                if (cols == 3) return vec == 3 ? GL_FLOAT_MAT3 : (vec == 2 ? GL_FLOAT_MAT3x2 : GL_FLOAT_MAT3x4);
                if (cols == 4) return vec == 4 ? GL_FLOAT_MAT4 : (vec == 2 ? GL_FLOAT_MAT4x2 : GL_FLOAT_MAT4x3);
                return GL_NONE;
            }
            if (vec == 1) return GL_FLOAT;
            if (vec == 2) return GL_FLOAT_VEC2;
            if (vec == 3) return GL_FLOAT_VEC3;
            if (vec == 4) return GL_FLOAT_VEC4;
            return GL_NONE;

        case SPVC_BASETYPE_FP64:
            if (cols > 1)
            {
                if (cols == 2) return vec == 2 ? GL_DOUBLE_MAT2 : (vec == 3 ? GL_DOUBLE_MAT2x3 : GL_DOUBLE_MAT2x4);
                if (cols == 3) return vec == 3 ? GL_DOUBLE_MAT3 : (vec == 2 ? GL_DOUBLE_MAT3x2 : GL_DOUBLE_MAT3x4);
                if (cols == 4) return vec == 4 ? GL_DOUBLE_MAT4 : (vec == 2 ? GL_DOUBLE_MAT4x2 : GL_DOUBLE_MAT4x3);
                return GL_NONE;
            }
            if (vec == 1) return GL_DOUBLE;
            if (vec == 2) return GL_DOUBLE_VEC2;
            if (vec == 3) return GL_DOUBLE_VEC3;
            if (vec == 4) return GL_DOUBLE_VEC4;
            return GL_NONE;

        case SPVC_BASETYPE_INT32:
            if (vec == 1) return GL_INT;
            if (vec == 2) return GL_INT_VEC2;
            if (vec == 3) return GL_INT_VEC3;
            if (vec == 4) return GL_INT_VEC4;
            return GL_NONE;

        case SPVC_BASETYPE_UINT32:
            if (vec == 1) return GL_UNSIGNED_INT;
            if (vec == 2) return GL_UNSIGNED_INT_VEC2;
            if (vec == 3) return GL_UNSIGNED_INT_VEC3;
            if (vec == 4) return GL_UNSIGNED_INT_VEC4;
            return GL_NONE;

        case SPVC_BASETYPE_BOOLEAN:
            if (vec == 1) return GL_BOOL;
            if (vec == 2) return GL_BOOL_VEC2;
            if (vec == 3) return GL_BOOL_VEC3;
            if (vec == 4) return GL_BOOL_VEC4;
            return GL_NONE;

        case SPVC_BASETYPE_SAMPLED_IMAGE:
        case SPVC_BASETYPE_IMAGE:
            return glSamplerTypeFromSpirv(compiler, type);

        default:
            return GL_NONE;
    }
}


// glslang lowers a bool in a uniform block to uint, because SPIR-V gives bools
// no memory layout. The GLSL source is the only place the real type survives,
// so look the member up there before reporting a type.
static bool scanDeclSaysBool(const char *src, const char *keyword, const char *block,
                             const char *member, int *vecsize)
{
    const char *p = src;
    size_t blen = block ? strlen(block) : 0;
    size_t klen = strlen(keyword);

    *vecsize = 1;

    if (!src || !member || !member[0])
        return false;

    while ((p = strstr(p, keyword)) != NULL)
    {
        const char *open, *close, *q;

        p += klen;

        // the block name, if we were given one, has to be the next word
        if (blen)
        {
            const char *w = p;

            while (*w == ' ' || *w == '\t' || *w == '\n' || *w == '\r') w++;

            if (strncmp(w, block, blen) != 0)
                continue;
        }

        open = strchr(p, '{');
        if (!open)
            break;

        close = strchr(open, '}');
        if (!close)
            break;

        for (q = open; q < close; q++)
        {
            int n = 0;

            if (!strncmp(q, "bool", 4) && (q == open + 1 || !isalnum((unsigned char)q[-1])))
                n = 1;
            else if (!strncmp(q, "bvec", 4) && q[4] >= '2' && q[4] <= '4')
                n = q[4] - '0';
            else
                continue;

            {
                const char *r = q + (n == 1 ? 4 : 5);
                size_t mlen = strlen(member);

                // every identifier up to the semicolon shares this type
                while (r < close && *r != ';')
                {
                    while (r < close && !isalnum((unsigned char)*r) && *r != '_') r++;

                    if (r < close && !strncmp(r, member, mlen) &&
                        !isalnum((unsigned char)r[mlen]) && r[mlen] != '_')
                    {
                        *vecsize = n;
                        return true;
                    }

                    while (r < close && (isalnum((unsigned char)*r) || *r == '_')) r++;

                    if (r < close && *r != ',' && *r != ';')
                        break;

                    if (r < close && *r == ',') r++;
                }
            }

            q += 4;
        }

        p = close;
    }

    return false;
}

// A bool member may be declared in the block itself or in a struct the block
// uses, so look in both before giving up and calling it a uint.
// glslang hands every default block uniform a location decoration of its own,
// numbered per stage, so the vertex and fragment stages both start at zero.
// Only a layout(location=) the source actually wrote is binding on GL, so look
// for that and let the linker number the rest.
static GLint explicitLayout(const char *src, const char *name, const char *key, const char *storage);

static GLint explicitUniformLocation(const char *src, const char *name)
{
    return explicitLayout(src, name, "location", "uniform");
}

static GLint explicitUniformLayout(const char *src, const char *name, const char *key)
{
    return explicitLayout(src, name, key, "uniform");
}

// A declaration's layout(key = N), in any of the number forms GLSL allows, or
// -1. storage is the word the declaration has to carry: uniform, in or out.
static GLint explicitLayout(const char *src, const char *name, const char *key, const char *storage)
{
    size_t slen = strlen(storage);
    size_t nlen = name ? strlen(name) : 0;
    size_t klen = strlen(key);
    const char *p = src;

    if (!src || !nlen)
        return -1;

    while ((p = strstr(p, name)) != NULL)
    {
        const char *stmt = p;
        const char *q;
        bool has_layout = false, has_uniform = false, has_location = false;
        const char *loc = NULL;

        if ((p != src && (isalnum((unsigned char)p[-1]) || p[-1] == '_')) ||
            isalnum((unsigned char)p[nlen]) || p[nlen] == '_')
        {
            p += nlen;
            continue;
        }

        while (stmt > src && stmt[-1] != ';' && stmt[-1] != '}' && stmt[-1] != '{')
            stmt--;

        for (q = stmt; q < p; q++)
        {
            if (!strncmp(q, "layout", 6))   has_layout = true;
            if (!strncmp(q, storage, slen) && (q == stmt || !isalnum((unsigned char)q[-1])) &&
                !isalnum((unsigned char)q[slen]) && q[slen] != '_')
                has_uniform = true;
            if (!strncmp(q, key, klen) && !isalnum((unsigned char)q[klen]) && q[klen] != '_')
            {
                has_location = true;
                loc = q;
            }
        }

        if (has_layout && has_uniform && has_location && loc)
        {
            q = loc + klen;

            while (q < p && *q != '=') q++;

            if (q < p)
            {
                q++;
                while (q < p && (*q == ' ' || *q == '\t')) q++;
                if (q < p && isdigit((unsigned char)*q))
                    return (GLint)strtol(q, NULL, 0);
            }
        }

        p += nlen;
    }

    return -1;
}

static bool sourceSaysBool(const char *src, const char *owner, const char *member, int *vecsize)
{
    if (scanDeclSaysBool(src, "uniform", owner, member, vecsize))
        return true;

    return scanDeclSaysBool(src, "struct", owner, member, vecsize);
}

static GLenum boolTypeForVecSize(int n)
{
    switch (n)
    {
        case 2: return GL_BOOL_VEC2;
        case 3: return GL_BOOL_VEC3;
        case 4: return GL_BOOL_VEC4;
    }

    return GL_BOOL;
}

// GL 4.6 section 7.3.1: a struct inside a uniform block is not a uniform of
// its own; its leaves are, named "Block.s.leaf". Walk down to them.
// How many leaves a struct flattens into, so the list can be sized for them.
static GLuint countBlockLeaves(spvc_compiler compiler, spvc_type st, unsigned depth)
{
    unsigned members = spvc_type_get_num_member_types(st);
    GLuint n = 0;

    if (depth > 8)
        return 0;

    for (unsigned m = 0; m < members; m++)
    {
        spvc_type mt = spvc_compiler_get_type_handle(compiler, spvc_type_get_member_type(st, m));

        if (!mt)
            continue;

        if (spvc_type_get_basetype(mt) == SPVC_BASETYPE_STRUCT)
        {
            spvc_type et = (spvc_type_get_num_array_dimensions(mt) > 0)
                         ? spvc_compiler_get_type_handle(compiler, spvc_type_get_base_type_id(mt))
                         : mt;
            GLuint elems = 1;

            if (spvc_type_get_num_array_dimensions(mt) > 0)
            {
                unsigned dim = spvc_type_get_array_dimension(mt, 0);

                elems = dim ? dim : 1;
            }

            if (et)
                n += elems * countBlockLeaves(compiler, et, depth + 1);
        }
        else
            n++;
    }

    return n;
}

static void flattenBlockMembers(spvc_compiler compiler, Program *ptr, int stage,
                                spvc_type st, spvc_type_id st_id,
                                const char *prefix, unsigned depth,
                                GLint block_index, GLint base_offset,
                                GLuint *out, GLuint total)
{
    unsigned members = spvc_type_get_num_member_types(st);

    if (depth > 8)
        return;

    for (unsigned m = 0; m < members && *out < total; m++)
    {
        spvc_type_id mtid = spvc_type_get_member_type(st, m);
        spvc_type mt = spvc_compiler_get_type_handle(compiler, mtid);
        const char *mname = spvc_compiler_get_member_name(compiler, st_id, m);
        char name[512];
        unsigned off = 0, astride = 0, mstride = 0;
        GLint asize = 1;
        GLenum gl_type;

        if (!mt)
            continue;

        if (prefix && prefix[0])
            snprintf(name, sizeof name, "%s.%s", prefix, mname ? mname : "");
        else
            snprintf(name, sizeof name, "%s", mname ? mname : "");

        if (spvc_type_get_basetype(mt) == SPVC_BASETYPE_STRUCT)
        {
            // The array type carries no member names; its element type does.
            spvc_type_id etid = (spvc_type_get_num_array_dimensions(mt) > 0)
                              ? spvc_type_get_base_type_id(mt) : mtid;
            spvc_type et = spvc_compiler_get_type_handle(compiler, etid);
            unsigned sub = 0;
            GLint here = (spvc_compiler_type_struct_member_offset(compiler, st, m, &sub) == SPVC_SUCCESS)
                       ? base_offset + (GLint)sub : base_offset;
            GLint elems = 1, stride = 0;
            bool is_array = spvc_type_get_num_array_dimensions(mt) > 0;

            if (!et)
                continue;

            if (is_array)
            {
                unsigned dim = spvc_type_get_array_dimension(mt, 0);
                unsigned as = 0;

                elems = dim ? (GLint)dim : 1;

                if (spvc_compiler_type_struct_member_array_stride(compiler, st, m, &as) == SPVC_SUCCESS)
                    stride = (GLint)as;
            }

            // GL enumerates every element of an array of structures
            for (GLint e = 0; e < elems && *out < total; e++)
            {
                char child[512];

                // an array of one is still an array: GL names it "l[0].mA"
                if (is_array)
                    snprintf(child, sizeof child, "%s[%d]", name, e);
                else
                    snprintf(child, sizeof child, "%s", name);

                flattenBlockMembers(compiler, ptr, stage, et, etid, child, depth + 1,
                                    block_index, here + e * stride, out, total);
            }

            continue;
        }

        gl_type = glTypeFromSpirv(compiler, mtid, &asize);

        if (gl_type == 0)
            continue;

        // a uint that the source declared bool is a bool to GL
        if (gl_type == GL_UNSIGNED_INT || gl_type == GL_UNSIGNED_INT_VEC2 ||
            gl_type == GL_UNSIGNED_INT_VEC3 || gl_type == GL_UNSIGNED_INT_VEC4)
        {
            Shader *sh = ptr->shader_slots[stage];
            int n = 1;

            // scope the search to the declaration this member belongs to:
            // the same short name turns up in several structs
            const char *owner = spvc_compiler_get_name(compiler, (SpvId)st_id);

            if (sh && sh->src && mname && sourceSaysBool(sh->src, owner, mname, &n))
                gl_type = boolTypeForVecSize(n);
        }

        // GL names an array member "g[0]" even when the array holds one
        // element, and reports its size as that count
        if (spvc_type_get_num_array_dimensions(mt) > 0)
        {
            size_t l = strlen(name);

            if (l + 4 < sizeof name)
                snprintf(name + l, sizeof name - l, "[0]");
        }

        // a block declared in two stages is still one set of uniforms
        for (int prev = _VERTEX_SHADER; prev <= stage; prev++)
        {
            SpirvResourceList *pl = &ptr->block_uniforms[prev];
            GLuint limit = (prev == stage) ? *out : pl->count;

            for (GLuint k = 0; k < limit; k++)
                if (pl->list[k].name && !strcmp(pl->list[k].name, name))
                    return;
        }

        {
            SpirvResource *dst = &ptr->block_uniforms[stage].list[*out];

            dst->type_id = mtid;
            dst->gl_type = gl_type;
            dst->array_size = asize;
            dst->name = strdup(name);
            dst->block_index = block_index;
            dst->offset = (spvc_compiler_type_struct_member_offset(compiler, st, m, &off) == SPVC_SUCCESS)
                        ? base_offset + (GLint)off : -1;
            dst->array_stride = (spvc_compiler_type_struct_member_array_stride(compiler, st, m, &astride) == SPVC_SUCCESS)
                        ? (GLint)astride : 0;
            dst->matrix_stride = (spvc_compiler_type_struct_member_matrix_stride(compiler, st, m, &mstride) == SPVC_SUCCESS)
                        ? (GLint)mstride : 0;
            dst->is_row_major = spvc_compiler_has_member_decoration(compiler, st_id, m, SpvDecorationRowMajor)
                        ? GL_TRUE : GL_FALSE;
        }

        (*out)++;
    }
}




/* ---- plain struct uniforms ---------------------------------------------
 *
 * "uniform S s;" is one SPIR-V variable and one Metal buffer, but GL calls
 * every leaf inside it an active uniform of its own, with its own location.
 * SPIRV-Cross emits the struct with Metal's ordinary C layout and no Offset
 * decorations, so the offsets have to be worked out the same way Metal does.
 */
static void mslTypeLayout(spvc_compiler compiler, spvc_type_id tid,
                          unsigned *out_size, unsigned *out_align, unsigned depth);

static unsigned mslScalarSize(spvc_basetype bt)
{
    switch (bt)
    {
        case SPVC_BASETYPE_BOOLEAN: return 1;
        case SPVC_BASETYPE_INT8:
        case SPVC_BASETYPE_UINT8:   return 1;
        case SPVC_BASETYPE_INT16:
        case SPVC_BASETYPE_UINT16:
        case SPVC_BASETYPE_FP16:    return 2;
        case SPVC_BASETYPE_INT64:
        case SPVC_BASETYPE_UINT64:
        case SPVC_BASETYPE_FP64:    return 8;
        default:                    return 4;
    }
}

static void mslTypeLayout(spvc_compiler compiler, spvc_type_id tid,
                          unsigned *out_size, unsigned *out_align, unsigned depth)
{
    spvc_type t = spvc_compiler_get_type_handle(compiler, tid);
    unsigned size = 4, align = 4;

    *out_size = 0;
    *out_align = 1;

    if (!t || depth > 8)
        return;

    if (spvc_type_get_basetype(t) == SPVC_BASETYPE_STRUCT)
    {
        unsigned members = spvc_type_get_num_member_types(t);
        unsigned offset = 0;

        align = 1;

        for (unsigned m = 0; m < members; m++)
        {
            unsigned ms = 0, ma = 1;

            mslTypeLayout(compiler, spvc_type_get_member_type(t, m), &ms, &ma, depth + 1);

            if (ma > align) align = ma;

            offset = (offset + ma - 1) & ~(ma - 1);
            offset += ms;
        }

        size = (offset + align - 1) & ~(align - 1);
    }
    else
    {
        unsigned scalar = mslScalarSize(spvc_type_get_basetype(t));
        unsigned vec = spvc_type_get_vector_size(t);
        unsigned cols = spvc_type_get_columns(t);
        unsigned col_size, col_align;

        if (vec == 0) vec = 1;
        if (cols == 0) cols = 1;

        // Metal pads a three component vector out to four
        col_size = scalar * (vec == 3 ? 4 : vec);
        col_align = col_size;

        size = col_size * cols;
        align = col_align;
    }

    // an array multiplies the element, which is already a multiple of its own
    // alignment
    {
        unsigned dims = spvc_type_get_num_array_dimensions(t);

        for (unsigned d = 0; d < dims; d++)
        {
            unsigned n = spvc_type_get_array_dimension(t, d);

            size *= (n ? n : 1);
        }
    }

    *out_size = size;
    *out_align = align;
}

static GLuint countStructLeaves(spvc_compiler compiler, spvc_type st, unsigned depth)
{
    unsigned members = spvc_type_get_num_member_types(st);
    GLuint n = 0;

    if (depth > 8)
        return 0;

    for (unsigned m = 0; m < members; m++)
    {
        spvc_type mt = spvc_compiler_get_type_handle(compiler, spvc_type_get_member_type(st, m));

        if (!mt)
            continue;

        if (spvc_type_get_basetype(mt) == SPVC_BASETYPE_STRUCT)
        {
            spvc_type et = (spvc_type_get_num_array_dimensions(mt) > 0)
                         ? spvc_compiler_get_type_handle(compiler, spvc_type_get_base_type_id(mt))
                         : mt;
            GLuint elems = 1;

            if (spvc_type_get_num_array_dimensions(mt) > 0)
            {
                unsigned dim = spvc_type_get_array_dimension(mt, 0);

                elems = dim ? dim : 1;
            }

            if (et)
                n += elems * countStructLeaves(compiler, et, depth + 1);
        }
        else
            n++;
    }

    return n;
}

typedef struct {
    char     name[256];
    GLenum   gl_type;
    GLint    array_size;
    GLint    array_stride;
    GLint    offset;
} MGLStructLeaf;

static void flattenStructLeaves(spvc_compiler compiler, Program *ptr, int stage,
                                spvc_type st, spvc_type_id st_id, const char *prefix,
                                unsigned depth, GLint base_offset,
                                MGLStructLeaf *out, GLuint *count, GLuint max)
{
    unsigned members = spvc_type_get_num_member_types(st);
    unsigned offset = 0;

    if (depth > 8)
        return;

    for (unsigned m = 0; m < members && *count < max; m++)
    {
        spvc_type_id mtid = spvc_type_get_member_type(st, m);
        spvc_type mt = spvc_compiler_get_type_handle(compiler, mtid);
        const char *mname = spvc_compiler_get_member_name(compiler, st_id, m);
        unsigned ms = 0, ma = 1;
        char name[256];

        if (!mt)
            continue;

        mslTypeLayout(compiler, mtid, &ms, &ma, 0);
        offset = (offset + ma - 1) & ~(ma - 1);

        if (prefix && prefix[0])
            snprintf(name, sizeof name, "%s.%s", prefix, mname ? mname : "");
        else
            snprintf(name, sizeof name, "%s", mname ? mname : "");

        if (spvc_type_get_basetype(mt) == SPVC_BASETYPE_STRUCT)
        {
            bool is_array = spvc_type_get_num_array_dimensions(mt) > 0;
            spvc_type_id etid = is_array ? spvc_type_get_base_type_id(mt) : mtid;
            spvc_type et = spvc_compiler_get_type_handle(compiler, etid);
            GLint elems = 1;
            unsigned es = 0, ea = 1;

            if (et)
            {
                mslTypeLayout(compiler, etid, &es, &ea, 0);

                if (is_array)
                {
                    unsigned dim = spvc_type_get_array_dimension(mt, 0);

                    elems = dim ? (GLint)dim : 1;
                }

                for (GLint e = 0; e < elems && *count < max; e++)
                {
                    char child[256];

                    if (is_array)
                        snprintf(child, sizeof child, "%s[%d]", name, e);
                    else
                        snprintf(child, sizeof child, "%s", name);

                    flattenStructLeaves(compiler, ptr, stage, et, etid, child, depth + 1,
                                        base_offset + (GLint)offset + e * (GLint)es,
                                        out, count, max);
                }
            }

            offset += ms;
            continue;
        }

        {
            GLint asize = 1;
            GLenum gl_type = glTypeFromSpirv(compiler, mtid, &asize);
            unsigned stride = ms;

            if (gl_type == 0)
            {
                offset += ms;
                continue;
            }

            if (asize > 1 || spvc_type_get_num_array_dimensions(mt) > 0)
            {
                stride = (asize > 0) ? ms / (unsigned)asize : ms;

                if (strlen(name) + 4 < sizeof name)
                    strcat(name, "[0]");
            }

            // a uint the source declared bool is a bool to GL
            if (gl_type == GL_UNSIGNED_INT || gl_type == GL_UNSIGNED_INT_VEC2 ||
                gl_type == GL_UNSIGNED_INT_VEC3 || gl_type == GL_UNSIGNED_INT_VEC4)
            {
                Shader *sh = ptr->shader_slots[stage];
                int n = 1;
                const char *owner = spvc_compiler_get_name(compiler, (SpvId)st_id);

                if (sh && sh->src && mname && sourceSaysBool(sh->src, owner, mname, &n))
                    gl_type = boolTypeForVecSize(n);
            }

            snprintf(out[*count].name, sizeof out[*count].name, "%s", name);
            out[*count].gl_type = gl_type;
            out[*count].array_size = asize > 0 ? asize : 1;
            out[*count].array_stride = (GLint)stride;
            out[*count].offset = base_offset + (GLint)offset;
            (*count)++;
        }

        offset += ms;
    }
}

// Append the leaves of every plain struct uniform to the stage's resource list.
// Each keeps the owner's location in binding so a write knows which buffer it
// belongs to, and msl_index stays MGL_NO_LOCATION so the renderer does not try
// to bind a buffer for it.
static void addStructUniformLeaves(spvc_compiler compiler, Program *ptr, int stage)
{
    SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];
    GLuint original = list->count;

    for (GLuint i = 0; i < original; i++)
    {
        spvc_type t = spvc_compiler_get_type_handle(compiler, list->list[i].type_id);
        spvc_type_id st_id;
        spvc_type st;
        MGLStructLeaf *leaves;
        GLuint n = 0, total;
        unsigned ssize = 0, salign = 1;
        SpirvResource *grown;

        if (!t || spvc_type_get_basetype(t) != SPVC_BASETYPE_STRUCT)
            continue;

        st_id = list->list[i].base_type_id;
        st = spvc_compiler_get_type_handle(compiler, st_id);

        if (!st)
            continue;

        total = countStructLeaves(compiler, st, 0);

        if (total == 0 || total > 512)
            continue;

        leaves = (MGLStructLeaf *)calloc(total, sizeof(MGLStructLeaf));

        if (!leaves)
            continue;

        flattenStructLeaves(compiler, ptr, stage, st, st_id, list->list[i].name,
                            0, 0, leaves, &n, total);

        mslTypeLayout(compiler, list->list[i].type_id, &ssize, &salign, 0);

        grown = (SpirvResource *)realloc(list->list, (list->count + n) * sizeof(SpirvResource));

        if (!grown)
        {
            free(leaves);
            continue;
        }

        list->list = grown;

        for (GLuint k = 0; k < n; k++)
        {
            SpirvResource *dst = &list->list[list->count];

            memset(dst, 0, sizeof *dst);
            dst->name = strdup(leaves[k].name);
            dst->gl_type = leaves[k].gl_type;
            dst->array_size = leaves[k].array_size;
            dst->array_stride = leaves[k].array_stride;
            dst->offset = leaves[k].offset;
            dst->block_index = -1;
            dst->block_size = (GLint)ssize;
            dst->binding = list->list[i].location;   // filled in once locations are numbered
            dst->location = MGL_NO_LOCATION;
            dst->msl_index = MGL_NO_LOCATION;
            dst->_id = list->list[i]._id;
            list->count++;
        }

        free(leaves);
    }
}

// The control stage writes a buffer the evaluation stage reads back, and
// SPIRV-Cross lays each side out from that stage's own declarations. An
// output the evaluation shader never declares would shift everything after
// it, so it is told about every one, and pads its side to match. Built-ins
// are handled by mglTouchTessInputs instead.

static void addTessInterfaceVar(spvc_compiler c, bool input, unsigned location, unsigned vecsize,
                                SpvBuiltIn builtin, bool patch)
{
    spvc_msl_shader_interface_var_2 v;

    spvc_msl_shader_interface_var_init_2(&v);
    v.location = location;
    v.vecsize = vecsize;
    v.builtin = builtin;
    v.rate = patch ? SPVC_MSL_SHADER_VARIABLE_RATE_PER_PATCH : SPVC_MSL_SHADER_VARIABLE_RATE_PER_VERTEX;

    if (input)
        spvc_compiler_msl_add_shader_input_2(c, &v);
    else
        spvc_compiler_msl_add_shader_output_2(c, &v);
}

static void declareTessInterface(spvc_compiler compiler_msl, Program *pptr, int stage)
{
    bool input = stage == _TESS_EVALUATION_SHADER;
    Spirv *tcs = &pptr->spirv[_TESS_CONTROL_SHADER];

    if (!input || !pptr->tess.has_control || tcs->ir == NULL)
        return;

    // the control stage pins its own built-ins; the evaluation stage learns
    // about everything from the control stage's SPIR-V
    spvc_context ctx = NULL;
    spvc_parsed_ir ir = NULL;
    spvc_compiler c = NULL;
    spvc_resources res = NULL;
    const spvc_reflected_resource *list = NULL;
    size_t count = 0;

    if (spvc_context_create(&ctx) != SPVC_SUCCESS ||
        spvc_context_parse_spirv(ctx, tcs->ir, tcs->size, &ir) != SPVC_SUCCESS ||
        spvc_context_create_compiler(ctx, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &c) != SPVC_SUCCESS ||
        spvc_compiler_create_shader_resources(c, &res) != SPVC_SUCCESS ||
        spvc_resources_get_resource_list_for_type(res, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, &list, &count) != SPVC_SUCCESS)
    {
        if (ctx)
            spvc_context_destroy(ctx);
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        spvc_type type = spvc_compiler_get_type_handle(c, list[i].base_type_id);
        bool patch = spvc_compiler_has_decoration(c, list[i].id, SpvDecorationPatch);
        bool block = spvc_type_get_basetype(type) == SPVC_BASETYPE_STRUCT;

        if (block)
        {
            unsigned members = spvc_type_get_num_member_types(type);
            unsigned base = spvc_compiler_has_decoration(c, list[i].id, SpvDecorationLocation)
                          ? spvc_compiler_get_decoration(c, list[i].id, SpvDecorationLocation) : 0;
            unsigned next = base;

            for (unsigned m = 0; m < members; m++)
            {
                spvc_type mt = spvc_compiler_get_type_handle(c, spvc_type_get_member_type(type, m));
                unsigned vec = spvc_type_get_vector_size(mt);
                unsigned cols = spvc_type_get_columns(mt);
                unsigned n = 1;

                for (unsigned d = 0; d < spvc_type_get_num_array_dimensions(mt); d++)
                    n *= spvc_type_get_array_dimension(mt, d) ? spvc_type_get_array_dimension(mt, d) : 1;

                if (spvc_compiler_has_member_decoration(c, list[i].base_type_id, m, SpvDecorationBuiltIn))
                    continue;

                if (spvc_compiler_has_member_decoration(c, list[i].base_type_id, m, SpvDecorationLocation))
                    next = spvc_compiler_get_member_decoration(c, list[i].base_type_id, m, SpvDecorationLocation);

                for (unsigned k = 0; k < cols * n; k++)
                    addTessInterfaceVar(compiler_msl, true, next++, vec, SpvBuiltInMax, patch);
            }

            continue;
        }

        if (!spvc_compiler_has_decoration(c, list[i].id, SpvDecorationLocation))
            continue;

        unsigned loc = spvc_compiler_get_decoration(c, list[i].id, SpvDecorationLocation);
        unsigned vec = spvc_type_get_vector_size(type);
        unsigned cols = spvc_type_get_columns(type);
        unsigned dims = spvc_type_get_num_array_dimensions(type);
        unsigned n = 1;

        // a per-vertex output's outermost dimension is the vertex, not a location
        for (unsigned d = 0; d < dims; d++)
        {
            if (!patch && d == dims - 1)
                break;

            n *= spvc_type_get_array_dimension(type, d) ? spvc_type_get_array_dimension(type, d) : 1;
        }

        for (unsigned k = 0; k < cols * n; k++)
            addTessInterfaceVar(compiler_msl, true, loc + k, vec, SpvBuiltInMax, patch);
    }

    spvc_context_destroy(ctx);
}

// `sp` is where the SPIR-V comes from and where the entry point name is left;
// it is ptr->spirv[stage] for a stage the application wrote, and somewhere else
// for one MGL generated. `entry_override` names a generated stage, which has no
// shader object to take a name from.
char *parseSPIRVShaderToMetal(GLMContext ctx, Program *ptr, int stage, Spirv *sp,
                              const char *entry_override)
{
    const SpvId *spirv;
    size_t word_count;
    char *str_ret;
    int parse_res;

    spvc_context context = NULL;
    spvc_parsed_ir ir = NULL;
    spvc_compiler compiler_msl = NULL;
    spvc_compiler_options options = NULL;
    spvc_resources resources = NULL;
    const spvc_reflected_resource *list = NULL;
    const char *result = NULL;
    size_t count;
    size_t i;
    unsigned int *lowered = NULL;
    size_t lowered_count = 0;
    unsigned int atomic_ids[MAX_ATOMIC_COUNTER_BUFFER_BINDINGS];
    unsigned int atomic_bindings[MAX_ATOMIC_COUNTER_BUFFER_BINDINGS];
    int atomic_blocks = 0;

    if (sp == NULL)
        sp = &ptr->spirv[stage];

    spirv = sp->ir;
    word_count = sp->size;

    // the second of these used to check spirv again rather than the size
    if (spirv == NULL || word_count == 0)
    {
        MGL_ERR("MGL Error: %s: stage %d has no SPIR-V to translate\n", __FUNCTION__, stage);
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // Create context.
    if (spvc_context_create(&context) != SPVC_SUCCESS || context == NULL)
    {
        MGL_ERR("MGL Error: could not create a SPIRV-Cross context\n");

        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

    // Set debug callback.
    spvc_context_set_error_callback(context, error_callback, ctx);

    // Metal can only count atomically inside a buffer, so the counters are
    // moved into one before SPIRV-Cross sees them.
    atomic_blocks = mglLowerAtomicCounters(spirv, word_count, &lowered, &lowered_count,
                                           atomic_ids, atomic_bindings,
                                           MAX_ATOMIC_COUNTER_BUFFER_BINDINGS);

    // GL 4.6 section 7.3: a stage with more counters, or more counter buffers,
    // than its limits does not link
    if (atomic_blocks > 0)
    {
        GLuint max_counters = STATE_VAR(max_compute_atomic_counters);
        GLuint max_buffers = STATE_VAR(max_compute_atomic_counter_buffers);

        switch (stage)
        {
            case _VERTEX_SHADER:
                max_counters = STATE_VAR(max_vertex_atomic_counters);
                max_buffers = STATE_VAR(max_vertex_atomic_counter_buffers);
                break;
            case _TESS_CONTROL_SHADER:
                max_counters = STATE_VAR(max_tess_control_atomic_counters);
                max_buffers = STATE_VAR(max_tess_control_atomic_counter_buffers);
                break;
            case _TESS_EVALUATION_SHADER:
                max_counters = STATE_VAR(max_tess_evaluation_atomic_counters);
                max_buffers = STATE_VAR(max_tess_evaluation_atomic_counter_buffers);
                break;
            case _GEOMETRY_SHADER:
                max_counters = STATE_VAR(max_geometry_atomic_counters);
                max_buffers = STATE_VAR(max_geometry_atomic_counter_buffers);
                break;
            case _FRAGMENT_SHADER:
                max_counters = STATE_VAR(max_fragment_atomic_counters);
                max_buffers = STATE_VAR(max_fragment_atomic_counter_buffers);
                break;
        }

        if ((GLuint)atomic_blocks > max_buffers ||
            (GLuint)mglCountAtomicCounters(spirv, word_count) > max_counters)
        {
            MGL_ERR("MGL Error: stage %d has more atomic counters than it is allowed\n", stage);
            free(lowered);
            spvc_context_destroy(context);
            return NULL;
        }
    }

    // Parse the SPIR-V.
    if (atomic_blocks > 0)
        parse_res = spvc_context_parse_spirv(context, lowered, lowered_count, &ir);
    else
        parse_res = spvc_context_parse_spirv(context, spirv, word_count, &ir);

    free(lowered);
    lowered = NULL;

    if (parse_res != SPVC_SUCCESS)
    {
        MGL_ERR("MGL Error: SPIR-V parse failed for stage %d: %s\n", stage,
                spvc_context_get_last_error_string(context));
        spvc_context_destroy(context);

        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // Hand it off to a compiler instance and give it ownership of the IR.
    if (spvc_context_create_compiler(context, SPVC_BACKEND_MSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler_msl) != SPVC_SUCCESS
        || compiler_msl == NULL)
    {
        MGL_ERR("MGL Error: could not create an MSL compiler for stage %d: %s\n", stage,
                spvc_context_get_last_error_string(context));
        spvc_context_destroy(context);

        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }
    // Metal has no cull distance and SPIRV-Cross's MSL backend writes broken
    // code for it, so the builtin never reaches the output. MGL culls in a
    // compute pass of its own instead.
    spvc_compiler_mask_stage_output_by_builtin(compiler_msl, SpvBuiltInCullDistance);

    // ERROR_CHECK_RETURN_VALUE(spvc_compiler_msl_add_discrete_descriptor_set(compiler_msl, 3) == SPVC_SUCCESS, GL_INVALID_OPERATION, NULL);
    if (spvc_compiler_msl_add_discrete_descriptor_set(compiler_msl, 3) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_msl_add_discrete_descriptor_set failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // Modify options.
    // ERROR_CHECK_RETURN_VALUE(spvc_compiler_create_compiler_options(compiler_msl, &options) == SPVC_SUCCESS, GL_INVALID_OPERATION, NULL);
    if (spvc_compiler_create_compiler_options(compiler_msl, &options) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_create_compiler_options failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // A program with no fragment stage can only be drawn with the raster off,
    // and Metal wants a vertex function that returns nothing for that.
    if (stage != _FRAGMENT_SHADER && stage != _COMPUTE_SHADER && ptr &&
        ptr->shader_slots[_FRAGMENT_SHADER] == NULL)
    {
        if (spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_DISABLE_RASTERIZATION, SPVC_TRUE) != SPVC_SUCCESS) {
            MGL_ERR("MGL Error: spvc_compiler_options_set_bool(SPVC_COMPILER_OPTION_MSL_DISABLE_RASTERIZATION) failed\n");
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
        }
    }

    // ERROR_CHECK_RETURN_VALUE(spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_FALSE) == SPVC_SUCCESS, GL_INVALID_OPERATION, NULL);
    if (spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_FALSE) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_options_set_bool(SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS) failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // ERROR_CHECK_RETURN_VALUE(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, SPVC_MAKE_MSL_VERSION(3,1,0)) == SPVC_SUCCESS, GL_INVALID_OPERATION, NULL);
    if (spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_VERSION, SPVC_MAKE_MSL_VERSION(3,1,0)) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_options_set_uint(SPVC_COMPILER_OPTION_MSL_VERSION) failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // GL clips z to [-1,1]; Metal clips to [0,1]. Without this the whole near
    // half of every GL projection is thrown away before rasterisation.
    if (spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_FIXUP_DEPTH_CONVENTION, SPVC_TRUE) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_options_set_bool(SPVC_COMPILER_OPTION_FIXUP_DEPTH_CONVENTION) failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // Metal has no double. SPIRV-Cross carries every double as three floats
    // that add up to it, which is close enough for GL's fp64 rules.
    if (spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_FP64_MODE, 2) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_options_set_uint(SPVC_COMPILER_OPTION_MSL_FP64_MODE) failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // GL is happy for a fragment shader to write a vec3 into an RGBA target and
    // fills alpha in itself. Metal refuses the pipeline outright, so the output
    // gets padded out to four components here.
    if (spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_PAD_FRAGMENT_OUTPUT_COMPONENTS, SPVC_TRUE) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_options_set_bool(SPVC_COMPILER_OPTION_MSL_PAD_FRAGMENT_OUTPUT_COMPONENTS) failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // 1D textures live in Metal as 2D ones (see createMTLTextureFromGLTexture)
    if (spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_TEXTURE_1D_AS_2D, SPVC_TRUE) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_options_set_bool(SPVC_COMPILER_OPTION_MSL_TEXTURE_1D_AS_2D) failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // The buffers the geometry emulation uses have to sit above whatever the
    // vertex descriptor and the uniform blocks are using, or the attribute
    // fetch and the capture write end up in the same Metal slot.
    // Same collision as the geometry buffers: the capture blocks must not land
    // where the vertex descriptor is already fetching attributes.
    if (stage == _VERTEX_SHADER)
    {
        for (int b = 0; b < MGL_XFB_MAX_BUFFERS; b++)
        {
            spvc_msl_resource_binding rb;

            spvc_msl_resource_binding_init(&rb);
            rb.stage = SpvExecutionModelVertex;
            rb.desc_set = 1;
            rb.binding = MGL_XFB_FIRST_BINDING + b;
            rb.msl_buffer = MGL_XFB_FIRST_MSL_SLOT + b;

            spvc_compiler_msl_add_resource_binding(compiler_msl, &rb);
        }
    }

    // the cull pass has buffers of its own, pinned the same way
    if (ptr->cull.building)
    {
        int count = ptr->cull.building == 1 ? 1 : 5;

        for (int b = 0; b < count; b++)
        {
            spvc_msl_resource_binding rb;

            spvc_msl_resource_binding_init(&rb);
            rb.stage = spvc_compiler_get_execution_model(compiler_msl);
            rb.desc_set = 1;
            rb.binding = MGL_CULL_FIRST_BINDING + b;
            rb.msl_buffer = MGL_CULL_FIRST_MSL_SLOT + b;

            spvc_compiler_msl_add_resource_binding(compiler_msl, &rb);
        }

        // the capture pass only fills a buffer, so Metal wants it to rasterise
        // nothing and return void
        if (ptr->cull.building == 1)
            spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_DISABLE_RASTERIZATION, SPVC_TRUE);
    }

    if (ptr->geom_shader && stage != _TESS_EVALUATION_SHADER)
    {
        static const struct { unsigned binding, msl; } pins[] = {
            { MGL_GS_IN_BINDING,    MGL_GS_IN_MSL_SLOT },
            { MGL_GS_OUT_BINDING,   MGL_GS_OUT_MSL_SLOT },
            { MGL_GS_INDEX_BINDING, MGL_GS_INDEX_MSL_SLOT },
        };

        for (size_t b = 0; b < sizeof(pins) / sizeof(pins[0]); b++)
        {
            spvc_msl_resource_binding rb;

            spvc_msl_resource_binding_init(&rb);

            // storage buffers were moved to their own descriptor set above
            rb.stage = spvc_compiler_get_execution_model(compiler_msl);
            rb.desc_set = 1;
            rb.binding = pins[b].binding;
            rb.msl_buffer = pins[b].msl;

            spvc_compiler_msl_add_resource_binding(compiler_msl, &rb);
        }
    }

    // With a geometry shader in the way, the vertex stage only fills a buffer.
    // Metal wants a vertex function that rasterises nothing to return void,
    // which is what this asks SPIRV-Cross for.
    if (ptr->geom_shader &&
        (ptr->tess.active ? stage == _TESS_EVALUATION_SHADER : stage == _VERTEX_SHADER))
        spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_DISABLE_RASTERIZATION, SPVC_TRUE);

    // Tessellation. Metal has the tessellator but reaches it differently: the
    // vertex and control stages run as compute and hand their results to the
    // evaluation stage, which becomes the vertex function of the draw.
    if (ptr->tess.active)
    {
        switch (stage)
        {
            case _VERTEX_SHADER:
                spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_VERTEX_FOR_TESSELLATION, SPVC_TRUE);
                spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_CAPTURE_OUTPUT_TO_BUFFER, SPVC_TRUE);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_SHADER_OUTPUT_BUFFER_INDEX, MGL_TESS_VERTEX_OUT_INDEX);
                break;

            case _TESS_CONTROL_SHADER:
                // triangle patches get a smaller factor struct than quads, and
                // the control shader's SPIR-V never says which this is
                if (ptr->tess.patch_kind)
                    spvc_compiler_set_execution_mode(compiler_msl, (SpvExecutionMode)ptr->tess.patch_kind);

                spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_MULTI_PATCH_WORKGROUP, SPVC_TRUE);
                spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_CAPTURE_OUTPUT_TO_BUFFER, SPVC_TRUE);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_SHADER_INPUT_BUFFER_INDEX, MGL_TESS_VERTEX_OUT_INDEX);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_SHADER_OUTPUT_BUFFER_INDEX, MGL_TESS_CONTROL_OUT_INDEX);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_SHADER_PATCH_OUTPUT_BUFFER_INDEX, MGL_TESS_PATCH_OUT_INDEX);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_SHADER_TESS_FACTOR_OUTPUT_BUFFER_INDEX, MGL_TESS_LEVEL_INDEX);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_INDIRECT_PARAMS_BUFFER_INDEX, MGL_TESS_PARAMS_INDEX);
                break;

            case _TESS_EVALUATION_SHADER:
                spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_RAW_BUFFER_TESE_INPUT, SPVC_TRUE);
                declareTessInterface(compiler_msl, ptr, stage);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_SHADER_INPUT_BUFFER_INDEX, MGL_TESS_CONTROL_OUT_INDEX);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_SHADER_PATCH_INPUT_BUFFER_INDEX, MGL_TESS_PATCH_OUT_INDEX);
                spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_SHADER_TESS_FACTOR_OUTPUT_BUFFER_INDEX, MGL_TESS_LEVEL_INDEX);
                spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_TESS_DOMAIN_ORIGIN_LOWER_LEFT,
                                               ptr->tess.lower_left ? SPVC_TRUE : SPVC_FALSE);

                // the evaluation shader's SPIR-V does not say how many control
                // points a patch has; that is the control shader's business
                if (ptr->tess.out_control_points)
                    spvc_compiler_set_execution_mode_with_arguments(compiler_msl,
                        SpvExecutionModeOutputVertices, ptr->tess.out_control_points, 0, 0);
                break;
        }
    }

    // A shader that reads textures by handle gets them through tables of its
    // own, one descriptor set each, which Metal sees as argument buffers
    ptr->bindless[stage].count = 0;
    {
        static const spvc_resource_type kinds[] = {
            SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, SPVC_RESOURCE_TYPE_STORAGE_IMAGE
        };
        spvc_resources br;

        if (spvc_compiler_create_shader_resources(compiler_msl, &br) == SPVC_SUCCESS)
        {
            for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++)
            {
                const spvc_reflected_resource *list = NULL;
                size_t count = 0;

                if (spvc_resources_get_resource_list_for_type(br, kinds[k], &list, &count) != SPVC_SUCCESS)
                    continue;

                for (size_t r = 0; r < count; r++)
                {
                    MglBindlessSets *bs = &ptr->bindless[stage];

                    if (strncmp(list[r].name, "mglBindless", 11) != 0)
                        continue;

                    if (bs->count >= MGL_BINDLESS_MAX_SETS)
                    {
                        MGL_ERR("MGL Error: this shader reads more kinds of texture by handle than MGL can bind\n");
                        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
                    }

                    unsigned set = spvc_compiler_get_decoration(compiler_msl, list[r].id, SpvDecorationDescriptorSet);
                    spvc_msl_resource_binding rb;

                    bs->slot[bs->count] = MGL_BINDLESS_FIRST_MSL_SLOT + bs->count;
                    bs->is_sampler[bs->count] = kinds[k] == SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS;

                    spvc_msl_resource_binding_init(&rb);
                    rb.stage = spvc_compiler_get_execution_model(compiler_msl);
                    rb.desc_set = set;
                    rb.binding = SPVC_MSL_ARGUMENT_BUFFER_BINDING;
                    rb.msl_buffer = bs->slot[bs->count];
                    spvc_compiler_msl_add_resource_binding(compiler_msl, &rb);
                    spvc_compiler_msl_set_argument_buffer_device_address_space(compiler_msl, set, SPVC_TRUE);
                    bs->count++;
                }
            }
        }

        if (ptr->bindless[stage].count > 0)
        {
            spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_TRUE);
            spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS_TIER, 1);
            spvc_compiler_msl_add_discrete_descriptor_set(compiler_msl, 0);
            spvc_compiler_msl_add_discrete_descriptor_set(compiler_msl, 1);
        }
    }

    //ERROR_CHECK_RETURN_VALUE(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 4.5) == SPVC_SUCCESS, GL_INVALID_OPERATION, NULL);
    // ERROR_CHECK_RETURN_VALUE(spvc_compiler_install_compiler_options(compiler_msl, options) == SPVC_SUCCESS, GL_INVALID_OPERATION, NULL);
    if (spvc_compiler_install_compiler_options(compiler_msl, options) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_install_compiler_options failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // GL 4.6 section 11.1.1: a declared location wins, then one the
    // application bound, and whatever is left gets the lowest free slots.
    // glslang's own numbering knows nothing of the middle rule, and can put an
    // undeclared input on top of a declared one.
    // By what the shader is, not which slot it was built for: the cull and
    // capture passes are vertex shaders too, reading the same attributes.
    {
        SpvExecutionModel model = spvc_compiler_get_execution_model(compiler_msl);

        if (model == SpvExecutionModelVertex && stage != _TESS_EVALUATION_SHADER)
            assignPipeLocations(ptr, compiler_msl, _VERTEX_SHADER);
        else if (model == SpvExecutionModelFragment)
            assignPipeLocations(ptr, compiler_msl, _FRAGMENT_SHADER);
    }

    // GL numbers uniform blocks and storage blocks separately, but glslang puts
    // both in descriptor set 0 with their GL binding, so a UBO and an SSBO that
    // share a number look like one resource. SPIRV-Cross then aliases them into
    // a single Metal buffer and Metal rejects the cast between constant and
    // device. Moving storage buffers to their own set keeps them apart.
    {
        spvc_resources pre_resources;

        if (spvc_compiler_create_shader_resources(compiler_msl, &pre_resources) == SPVC_SUCCESS)
        {
            const spvc_reflected_resource *sb_list = NULL;
            size_t sb_count = 0;

            if (spvc_resources_get_resource_list_for_type(pre_resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER,
                                                          &sb_list, &sb_count) == SPVC_SUCCESS)
            {
                for (size_t sb = 0; sb < sb_count; sb++)
                    spvc_compiler_set_decoration(compiler_msl, sb_list[sb].id, SpvDecorationDescriptorSet, 1);
            }
        }
    }

    
    // What the domain looks like is in the execution modes, and the render
    // pipeline has to be told the same thing Metal's tessellator will do.
    if (ptr->tess.active)
    {
        const SpvExecutionMode *modes = NULL;
        size_t mode_count = 0;

        if (stage == _TESS_CONTROL_SHADER)
            ptr->tess.out_control_points =
                spvc_compiler_get_execution_mode_argument(compiler_msl, SpvExecutionModeOutputVertices);

        if (spvc_compiler_get_execution_modes(compiler_msl, &modes, &mode_count) == SPVC_SUCCESS)
        {
            for (size_t m = 0; m < mode_count; m++)
            {
                switch (modes[m])
                {
                    case SpvExecutionModeTriangles:
                    case SpvExecutionModeQuads:
                    case SpvExecutionModeIsolines:
                        if (stage == _TESS_EVALUATION_SHADER)
                            ptr->tess.patch_kind = modes[m];
                        break;

                    case SpvExecutionModeSpacingEqual:
                    case SpvExecutionModeSpacingFractionalEven:
                    case SpvExecutionModeSpacingFractionalOdd:
                        ptr->tess.partition = modes[m];
                        break;

                    case SpvExecutionModeVertexOrderCw:
                    case SpvExecutionModeVertexOrderCcw:
                        ptr->tess.winding = modes[m];
                        break;

                    case SpvExecutionModePointMode:
                        ptr->tess.point_mode = GL_TRUE;
                        break;

                    default:
                        break;
                }
            }
        }
    }

    // create an entry point for metal based on the shader type and name
    char entry_point[128];

    // Taken from the module rather than from the GL stage: a geometry shader
    // arrives here as a compute module, because Metal has no geometry stage.
    SpvExecutionModel model = spvc_compiler_get_execution_model(compiler_msl);

    if (entry_override)
    {
        snprintf(entry_point, sizeof(entry_point), "%s", entry_override);
    }
    else
    {
        GLuint name = ptr->shader_slots[stage]->name;

        switch(stage)
        {
            case _VERTEX_SHADER: snprintf(entry_point, sizeof(entry_point), "vertex_%d_main",name); break;
            case _TESS_CONTROL_SHADER: snprintf(entry_point, sizeof(entry_point), "tess_control_%d_main",name); break;
            case _TESS_EVALUATION_SHADER: snprintf(entry_point, sizeof(entry_point), "tess_evaluation_%d_main",name); break;
            case _GEOMETRY_SHADER: snprintf(entry_point, sizeof(entry_point), "geometry_%d",name); break;
            case _FRAGMENT_SHADER: snprintf(entry_point, sizeof(entry_point), "fragment_%d",name); break;
            case _COMPUTE_SHADER: snprintf(entry_point, sizeof(entry_point), "compute_%d",name); break;
            default: // CRITICAL FIX: Handle error gracefully instead of crashing
            MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
            STATE(error) = GL_INVALID_OPERATION;
        }
    }

    const char *cleansed_entry_point;
    cleansed_entry_point = spvc_compiler_get_cleansed_entry_point_name(compiler_msl, "main", model);

    spvc_result err;
    err = spvc_compiler_rename_entry_point(compiler_msl, cleansed_entry_point, entry_point, model);

    if (err != SPVC_SUCCESS)
    {
        MGL_ERR("MGL Error: could not rename the entry point for stage %d: %s\n", stage,
                spvc_context_get_last_error_string(context));
        spvc_context_destroy(context);

        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    // set the entry point for metal
    if (entry_override == NULL)
        ptr->shader_slots[stage]->entry_point = strdup(entry_point);

    free(sp->entry_point);
    sp->entry_point = strdup(entry_point);

    // compute shader
    if (stage == _COMPUTE_SHADER)
    {
        spvc_result res;
        const spvc_entry_point *entry_points;
        size_t num_entry_points;

        res = spvc_compiler_get_entry_points(compiler_msl, &entry_points, &num_entry_points);

        if (res != SPVC_SUCCESS)
        {
            MGL_ERR("MGL Error: could not read compute entry points: %s\n",
                    spvc_context_get_last_error_string(context));
            spvc_context_destroy(context);

            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
        }
        
        for(int i=0; i<num_entry_points; i++)
        {
            DEBUG_PRINT("Entry point: %s Execution Model: %d\n", entry_points[i].name, entry_points[i].execution_model);
        }

        ptr->local_workgroup_size.x = spvc_compiler_get_execution_mode_argument_by_index(compiler_msl, SpvExecutionModeLocalSize, 0);
        ptr->local_workgroup_size.y = spvc_compiler_get_execution_mode_argument_by_index(compiler_msl, SpvExecutionModeLocalSize, 1);
        ptr->local_workgroup_size.z = spvc_compiler_get_execution_mode_argument_by_index(compiler_msl, SpvExecutionModeLocalSize, 2);
    }
    
    // Do some basic reflection.
    spvc_compiler_create_shader_resources(compiler_msl, &resources);
    // The exact types MGL consumes, listed rather than ranged: the enum's
    // numbering has changed upstream and a range silently misses the new ones.
    static const spvc_resource_type reflected_types[] = {
        SPVC_RESOURCE_TYPE_UNIFORM_BUFFER,
        SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT,
        SPVC_RESOURCE_TYPE_STORAGE_BUFFER,
        SPVC_RESOURCE_TYPE_STAGE_INPUT,
        SPVC_RESOURCE_TYPE_STAGE_OUTPUT,
        SPVC_RESOURCE_TYPE_SUBPASS_INPUT,
        SPVC_RESOURCE_TYPE_STORAGE_IMAGE,
        SPVC_RESOURCE_TYPE_SAMPLED_IMAGE,
        SPVC_RESOURCE_TYPE_ATOMIC_COUNTER,
        SPVC_RESOURCE_TYPE_PUSH_CONSTANT,
        SPVC_RESOURCE_TYPE_SEPARATE_IMAGE,
        SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS,
    };

    for (size_t rt_index = 0; rt_index < sizeof(reflected_types) / sizeof(reflected_types[0]); rt_index++)
    {
        int res_type = (int)reflected_types[rt_index];
#if DEBUG
        const char *res_name[] = {"NONE", "UNIFORM_BUFFER", "UNIFORM_CONSTANT", "STORAGE_BUFFER", "STAGE_INPUT", "STAGE_OUTPUT",
            "SUBPASS_INPUT", "STORAGE_INPUT", "SAMPLED_IMAGE", "ATOMIC_COUNTER", "PUSH_CONSTANT", "SEPARATE_IMAGE",
            "SEPARATE_SAMPLERS", "ACCELERATION_STRUCTURE", "RAY_QUERY"};
#endif
        
        spvc_resources_get_resource_list_for_type(resources, res_type, &list, &count);

        ptr->spirv_resources_list[stage][res_type].count = (GLuint)count;

        // CRITICAL SECURITY FIX: Prevent integer overflow in resource allocation
        // Check if count * sizeof(SpirvResource) would overflow size_t
        if (count > SIZE_MAX / sizeof(SpirvResource)) {
            MGL_ERR("MGL SECURITY ERROR: Resource count %zu would cause allocation overflow\n", count);
            ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
        }

        size_t alloc_size = count * sizeof(SpirvResource);
        // zeroed, not raw: msl_index is filled in only after the MSL is emitted,
        // and anything read before that must be a slot number, not heap garbage
        ptr->spirv_resources_list[stage][res_type].list = (SpirvResource *)calloc(count ? count : 1, sizeof(SpirvResource));
        if (!ptr->spirv_resources_list[stage][res_type].list) {
            MGL_ERR("MGL SECURITY ERROR: Failed to allocate %zu bytes for resource list\n", alloc_size);
            ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
        }

        for (i = 0; i < count; i++)
        {
            DEBUG_PRINT("res_type: %d ID: %u, BaseTypeID: %u, TypeID: %u, Name: %s ", res_type, list[i].id, list[i].base_type_id, list[i].type_id,
                   list[i].name);
            
            switch(res_type)
            {
                case SPVC_RESOURCE_TYPE_UNIFORM_BUFFER:
                case SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT:
                case SPVC_RESOURCE_TYPE_STORAGE_BUFFER:
                case SPVC_RESOURCE_TYPE_ATOMIC_COUNTER:
                    DEBUG_PRINT("Set: %u, Binding: %u Uniform: %d offset: %d\n",
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationDescriptorSet),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationBinding),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationUniform),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationOffset));
                    break;

                case SPVC_RESOURCE_TYPE_STAGE_INPUT:
                case SPVC_RESOURCE_TYPE_STAGE_OUTPUT:
                case SPVC_RESOURCE_TYPE_SUBPASS_INPUT:
                    DEBUG_PRINT("Set: %u, Location: %d Index: %d, offset: %d\n",
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationDescriptorSet),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationLocation),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationIndex),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationOffset));
                    break;
                    
                case SPVC_RESOURCE_TYPE_SAMPLED_IMAGE:
                case SPVC_RESOURCE_TYPE_SEPARATE_IMAGE:
                    DEBUG_PRINT("Set: %u, Location: %d\n",
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationDescriptorSet),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationLocation));
                    break;

                default:
                    DEBUG_PRINT("Set: %u, Binding: %u Location: %d Index: %d, Uniform: %d offset: %d\n",
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationDescriptorSet),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationBinding),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationLocation),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationIndex),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationUniform),
                           spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationOffset));
                    break;
            }
            
            ptr->spirv_resources_list[stage][res_type].list[i]._id = list[i].id;
            ptr->spirv_resources_list[stage][res_type].list[i].base_type_id = list[i].base_type_id;
            ptr->spirv_resources_list[stage][res_type].list[i].type_id = list[i].type_id;
            ptr->spirv_resources_list[stage][res_type].list[i].name = strdup(list[i].name);
            ptr->spirv_resources_list[stage][res_type].list[i].set = spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationDescriptorSet);
            ptr->spirv_resources_list[stage][res_type].list[i].binding = spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationBinding);
            if (res_type == SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT ||
                res_type == SPVC_RESOURCE_TYPE_SAMPLED_IMAGE ||
                res_type == SPVC_RESOURCE_TYPE_SEPARATE_IMAGE ||
                res_type == SPVC_RESOURCE_TYPE_STORAGE_IMAGE ||
                res_type == SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS)
            {
                Shader *sh = ptr->shader_slots[stage];
                GLint explicit_loc = (sh && sh->src)
                                   ? explicitUniformLocation(sh->src, list[i].name) : -1;

                ptr->spirv_resources_list[stage][res_type].list[i].location =
                    (explicit_loc >= 0) ? (GLuint)explicit_loc : MGL_NO_LOCATION;
                ptr->spirv_resources_list[stage][res_type].list[i].explicit_location = explicit_loc >= 0;
            }
            else
                ptr->spirv_resources_list[stage][res_type].list[i].location = spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationLocation);
            // GL reads the texture unit out of the sampler uniform's value. It
            // starts at the binding the shader declared, or at 0 -- not at
            // whatever binding glslang handed out -- and glUniform1i replaces it.
            {
                Shader *sh = ptr->shader_slots[stage];
                GLint declared = (sh && sh->src) ? explicitUniformLayout(sh->src, list[i].name, "binding") : -1;

                ptr->spirv_resources_list[stage][res_type].list[i].tex_unit = declared >= 0 ? declared : 0;
            }

            // The CTS sizes its buffer from GL_UNIFORM_BLOCK_DATA_SIZE, so a
            // block that reports 0 gets no data written into it at all.
            if (res_type == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER ||
                res_type == SPVC_RESOURCE_TYPE_STORAGE_BUFFER)
            {
                // "uniform Block { ... } b[3];" is three blocks to GL, named
                // Block[0] through Block[2].
                spvc_type var_type = spvc_compiler_get_type_handle(compiler_msl, list[i].type_id);

                ptr->spirv_resources_list[stage][res_type].list[i].array_size = 1;

                if (var_type && spvc_type_get_num_array_dimensions(var_type) == 1)
                {
                    unsigned n = (unsigned)spvc_type_get_array_dimension(var_type, 0);

                    if (n > 1)
                        ptr->spirv_resources_list[stage][res_type].list[i].array_size = (GLint)n;
                }

                spvc_type block_type = spvc_compiler_get_type_handle(compiler_msl, list[i].base_type_id);
                size_t block_size = 0;

                if (block_type &&
                    spvc_compiler_get_declared_struct_size(compiler_msl, block_type, &block_size) == SPVC_SUCCESS)
                {
                    // std140 rounds the block out to a multiple of 16
                    block_size = (block_size + 15) & ~(size_t)15;
                    ptr->spirv_resources_list[stage][res_type].list[i].block_size = (GLint)block_size;
                    ptr->spirv_resources_list[stage][res_type].list[i].member_count =
                        (GLint)spvc_type_get_num_member_types(block_type);
                }
            }
            if (getenv("MGL_DEBUG_RESOURCES"))
                MGL_INFO("MGLRES stage=%d type=%d name=%s id=%u basetype=%u set=%u binding=%u location=%u\n",
                        stage, res_type, list[i].name, list[i].id, list[i].base_type_id,
                        ptr->spirv_resources_list[stage][res_type].list[i].set,
                        ptr->spirv_resources_list[stage][res_type].list[i].binding,
                        ptr->spirv_resources_list[stage][res_type].list[i].location);
        }
    }

    // A failure here means SPIRV-Cross could not turn the SPIR-V into MSL.
    // Returning NULL fails the link, which is the honest answer -- ignoring it
    // hands Metal broken source and the program silently draws nothing.
    if (spvc_compiler_compile(compiler_msl, &result) != SPVC_SUCCESS)
    {
        const char *why = spvc_context_get_last_error_string(context);

        if (!why) why = "SPIRV-Cross could not generate MSL";

        MGL_ERR("MGL Error: MSL generation failed for stage %d: %s\n", stage, why);

        if (ptr->log) free(ptr->log);
        ptr->log = strdup(why);

        spvc_context_destroy(context);

        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    DEBUG_PRINT("\n%s\n", result);

    // The counter blocks made before parsing reflect as storage buffers. GL
    // knows them as atomic counter buffers, bound through
    // GL_ATOMIC_COUNTER_BUFFER at the binding the counters declared.
    if (atomic_blocks > 0)
    {
        SpirvResourceList *ssbo = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_STORAGE_BUFFER];
        SpirvResourceList *ac = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_ATOMIC_COUNTER];
        SpirvResource *grown = (SpirvResource *)realloc(ac->list,
                                    (ac->count + atomic_blocks) * sizeof(SpirvResource));

        if (grown)
        {
            GLuint kept = 0;

            ac->list = grown;

            for (GLuint k = 0; k < ssbo->count; k++)
            {
                int hit = -1;

                for (int a = 0; a < atomic_blocks; a++)
                    if (ssbo->list[k]._id == atomic_ids[a])
                        hit = a;

                if (hit < 0)
                {
                    ssbo->list[kept++] = ssbo->list[k];
                    continue;
                }

                ac->list[ac->count] = ssbo->list[k];
                ac->list[ac->count].binding = atomic_bindings[hit];
                ac->count++;
            }

            ssbo->count = kept;
        }
    }

    // the bindless tables are bound by the draw path, not through GL units
    for (int res_type = 0; res_type < MAX_SPVC_RESOURCE_TYPES; res_type++)
    {
        SpirvResourceList *rlist = &ptr->spirv_resources_list[stage][res_type];
        GLuint kept = 0;

        for (GLuint i = 0; i < rlist->count; i++)
        {
            if (rlist->list[i].name && strncmp(rlist->list[i].name, "mglBindless", 11) == 0)
            {
                free((void *)rlist->list[i].name);
                continue;
            }

            rlist->list[kept++] = rlist->list[i];
        }

        rlist->count = kept;
    }

    // MSL slots are not a dense run -- an arrayed uniform reserves one index
    // per element -- so ask for each slot rather than counting. They are only
    // assigned once the shader has actually been emitted, hence after compile.
    // Walk the whole array, not MGL's own shorter enum: SPIRV-Cross numbers
    // plain GL uniforms 15, so a _MAX_SPIRV_RES bound skips them and leaves
    // their msl_index holding whatever the allocator handed back.
    for (int res_type = 0; res_type < MAX_SPVC_RESOURCE_TYPES; res_type++)
    {
        SpirvResourceList *rlist = &ptr->spirv_resources_list[stage][res_type];

        for (unsigned i = 0; i < rlist->count; i++)
        {
            unsigned idx = spvc_compiler_msl_get_automatic_resource_binding(compiler_msl, rlist->list[i]._id);

            // A plain uniform the emitted MSL never reads gets no slot. Falling
            // back to its SPIR-V binding handed every one of them slot 0, so
            // setting an unused uniform overwrote whichever one lives there.
            // The same goes for a texture or image, which would otherwise land
            // on a slot a used one owns.
            if (idx == (unsigned)-1 && (res_type == SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT ||
                                        res_type == SPVC_RESOURCE_TYPE_SAMPLED_IMAGE ||
                                        res_type == SPVC_RESOURCE_TYPE_STORAGE_IMAGE))
                rlist->list[i].msl_index = (GLuint)-1;
            else
                rlist->list[i].msl_index = (idx == (unsigned)-1) ? rlist->list[i].binding : idx;

            // a combined sampler's texture and sampler are numbered apart
            {
                unsigned sidx = spvc_compiler_msl_get_automatic_resource_binding_secondary(compiler_msl,
                                                                                         rlist->list[i]._id);

                rlist->list[i].msl_sampler_index = (sidx == (unsigned)-1) ? rlist->list[i].msl_index : sidx;
            }

            rlist->list[i].gl_type = glTypeFromSpirv(compiler_msl, rlist->list[i].type_id,
                                                     &rlist->list[i].array_size);
            rlist->list[i].block_index = -1;
            rlist->list[i].offset = -1;

            // A block declared as an instance array is one SPIR-V resource but
            // several GL blocks. Each gets its own binding, and SPIRV-Cross
            // gives each its own Metal buffer slot running on from the first.
            if (res_type == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER ||
                res_type == SPVC_RESOURCE_TYPE_STORAGE_BUFFER)
            {
                spvc_type rt = spvc_compiler_get_type_handle(compiler_msl, rlist->list[i].type_id);

                // even a one element array is an array to GL: the block is
                // named "Block[0]", not "Block"
                if (rt && spvc_type_get_num_array_dimensions(rt) > 0)
                {
                    GLint n = rlist->list[i].array_size > 0 ? rlist->list[i].array_size : 1;

                    free(rlist->list[i].element_binding);
                    rlist->list[i].element_binding = (GLuint *)calloc((size_t)n, sizeof(GLuint));

                    if (rlist->list[i].element_binding)
                        for (GLint e = 0; e < n; e++)
                            rlist->list[i].element_binding[e] = rlist->list[i].binding + (GLuint)e;
                }
            }
        }
    }

    // a plain uniform of struct type is one buffer but many GL uniforms
    addStructUniformLeaves(compiler_msl, ptr, stage);

    // GL treats the members of a uniform block as active uniforms of their own,
    // with offsets and strides the app needs to lay its buffer out. Collect them.
    {
        SpirvResourceList *blocks = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_BUFFER];
        GLuint total = 0;
        GLuint block_base = 0;

        // uniformBlockAt numbers unique block names across every stage, so
        // members have to point at the same global index the block queries use
        for (int prev = _VERTEX_SHADER; prev < stage; prev++)
        {
            SpirvResourceList *pl = &ptr->spirv_resources_list[prev][SPVC_RESOURCE_TYPE_UNIFORM_BUFFER];

            // an instance array is several GL blocks, so it takes that many
            // indices -- members used to point at the wrong block entirely
            for (GLuint pb = 0; pb < pl->count; pb++)
                if (!programBlockSeenEarlier(ptr, prev, pb))
                    block_base += (pl->list[pb].array_size > 1) ? (GLuint)pl->list[pb].array_size : 1;
        }

        for (GLuint b = 0; b < blocks->count; b++)
        {
            spvc_type bt = spvc_compiler_get_type_handle(compiler_msl, blocks->list[b].base_type_id);

            // a struct member flattens into its leaves, so count those
            total += bt ? countBlockLeaves(compiler_msl, bt, 0)
                        : (GLuint)(blocks->list[b].member_count > 0 ? blocks->list[b].member_count : 0);
        }

        ptr->block_uniforms[stage].count = 0;
        ptr->block_uniforms[stage].list = (SpirvResource *)calloc(total ? total : 1, sizeof(SpirvResource));

        if (ptr->block_uniforms[stage].list)
        {
            GLuint out = 0;

            GLuint local = 0;

            for (GLuint b = 0; b < blocks->count; b++)
            {
                spvc_type bt = spvc_compiler_get_type_handle(compiler_msl, blocks->list[b].base_type_id);

                if (!bt)
                    continue;

                // a block already seen in an earlier stage keeps that index
                if (programBlockSeenEarlier(ptr, stage, b))
                    continue;

                {
                    const char *block_name = spvc_compiler_get_name(compiler_msl, (SpvId)blocks->list[b].base_type_id);
                    const char *inst_name = spvc_compiler_get_name(compiler_msl, (SpvId)blocks->list[b]._id);
                    bool has_instance = inst_name && inst_name[0] &&
                                        !(block_name && !strcmp(inst_name, block_name));
                    const char *prefix = (has_instance && block_name && block_name[0]) ? block_name : NULL;

                    flattenBlockMembers(compiler_msl, ptr, stage, bt,
                                        (spvc_type_id)blocks->list[b].base_type_id,
                                        prefix, 0, (GLint)(block_base + local), 0,
                                        &out, total);
                }

                local += (blocks->list[b].array_size > 1) ? (GLuint)blocks->list[b].array_size : 1;
            }

            ptr->block_uniforms[stage].count = out;
        }
    }

    if (getenv("MGL_DEBUG_RESOURCES"))
        for (int res_type = 0; res_type < MAX_SPVC_RESOURCE_TYPES; res_type++)
        {
            SpirvResourceList *rlist = &ptr->spirv_resources_list[stage][res_type];

            for (unsigned i = 0; i < rlist->count; i++)
                MGL_INFO("MGLMSL stage=%d type=%d name=%s msl_index=%u\n",
                         stage, res_type, rlist->list[i].name, rlist->list[i].msl_index);
        }

    if (getenv("MGL_DEBUG_MSL"))
        MGL_INFO("---- MSL stage %d ----\n%s\n", stage, result);

    str_ret = strdup(result);

    // Frees all memory we allocated so far.
    spvc_context_destroy(context);

    return str_ret;
}


static void assignPipeLocations(Program *ptr, spvc_compiler compiler, int stage)
{
    Shader *sh = ptr->shader_slots[stage];
    bool inputs = stage == _VERTEX_SHADER;
    spvc_resources res = NULL;
    const spvc_reflected_resource *list = NULL;
    size_t count = 0;
    bool used[256] = { false };

    if (sh == NULL || sh->src == NULL)
        return;

    if (spvc_compiler_create_shader_resources(compiler, &res) != SPVC_SUCCESS ||
        spvc_resources_get_resource_list_for_type(res, inputs ? SPVC_RESOURCE_TYPE_STAGE_INPUT
                                                              : SPVC_RESOURCE_TYPE_STAGE_OUTPUT,
                                                  &list, &count) != SPVC_SUCCESS)
        return;

    GLint want[64];
    GLuint slots[64];

    if (count > 64)
        count = 64;

    for (size_t i = 0; i < count; i++)
    {
        spvc_type t = spvc_compiler_get_type_handle(compiler, list[i].type_id);
        GLuint n = 1;

        if (t && spvc_type_get_num_array_dimensions(t) > 0 && spvc_type_array_dimension_is_literal(t, 0))
            n = spvc_type_get_array_dimension(t, 0);

        // a matrix input takes one location per column
        if (inputs && t && spvc_type_get_columns(t) > 1)
            n *= spvc_type_get_columns(t);

        slots[i] = n ? n : 1;
        want[i] = explicitLayout(sh->src, list[i].name, "location", inputs ? "in" : "out");

        if (want[i] < 0)
        {
            __typeof__(ptr->attrib_binds[0]) *binds = inputs ? ptr->attrib_binds : ptr->frag_binds;
            GLint nb = inputs ? ptr->attrib_bind_count : ptr->frag_bind_count;

            for (GLint b = 0; b < nb; b++)
                if (!strcmp(binds[b].name, list[i].name))
                    want[i] = (GLint)binds[b].location;
        }

        if (want[i] >= 0)
            for (GLuint k = 0; k < slots[i] && want[i] + k < 256; k++)
                used[want[i] + k] = true;
    }

    for (size_t i = 0; i < count; i++)
    {
        GLint at = want[i];

        if (at < 0)
        {
            for (at = 0; at + (GLint)slots[i] <= 256; at++)
            {
                bool free_run = true;

                for (GLuint k = 0; k < slots[i]; k++)
                    if (used[at + k])
                        free_run = false;

                if (free_run)
                    break;
            }

            for (GLuint k = 0; k < slots[i] && at + k < 256; k++)
                used[at + k] = true;
        }

        spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationLocation, (unsigned)at);
    }
}

// GL locations are per program, not per stage. Give every plain uniform the
// linker left unlocated one of its own, and let the same name in two stages
// share a location the way GL says it must.
// Plain uniforms first, then samplers and images: all of them are GL uniforms
// and share one run of locations.
static const int location_res_types[] = {
    SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT,
    SPVC_RESOURCE_TYPE_SAMPLED_IMAGE,
    SPVC_RESOURCE_TYPE_SEPARATE_IMAGE,
    SPVC_RESOURCE_TYPE_STORAGE_IMAGE,
    SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS,
};

// GL 4.6 section 7.3: two different uniforms at one location, or a location
// past the limit, fail the link. Returns the reason, or NULL.
static const char *uniformLocationProblem(Program *ptr)
{
    int kinds = (int)(sizeof(location_res_types) / sizeof(location_res_types[0]));

    for (int k = 0; k < kinds; k++)
        for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        {
            SpirvResourceList *list = &ptr->spirv_resources_list[stage][location_res_types[k]];

            for (GLuint i = 0; i < list->count; i++)
            {
                SpirvResource *a = &list->list[i];
                GLuint an = a->array_size > 1 ? (GLuint)a->array_size : 1;

                if (!a->explicit_location || a->name == NULL)
                    continue;

                if (a->location + an > MAX_UNIFORM_LOCATIONS)
                    return "link failed: a uniform location is past GL_MAX_UNIFORM_LOCATIONS";

                for (int k2 = 0; k2 < kinds; k2++)
                    for (int st2 = _VERTEX_SHADER; st2 < _MAX_SHADER_TYPES; st2++)
                    {
                        SpirvResourceList *l2 = &ptr->spirv_resources_list[st2][location_res_types[k2]];

                        for (GLuint j = 0; j < l2->count; j++)
                        {
                            SpirvResource *b = &l2->list[j];
                            GLuint bn = b->array_size > 1 ? (GLuint)b->array_size : 1;

                            if (b == a || b->name == NULL || b->location == MGL_NO_LOCATION ||
                                !strcmp(a->name, b->name) || b->gl_type == 0)
                                continue;

                            // a struct's leaves share their owner's buffer, not its location
                            if (b->offset >= 0 || a->offset >= 0)
                                continue;

                            if (a->location < b->location + bn && b->location < a->location + an)
                                return "link failed: two uniforms were given the same location";
                        }
                    }
            }
        }

    return NULL;
}

static void assignUniformLocations(Program *ptr)
{
    GLuint next = 0;
    int kinds = (int)(sizeof(location_res_types) / sizeof(location_res_types[0]));

    for (int k = 0; k < kinds; k++)
        for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        {
            SpirvResourceList *list = &ptr->spirv_resources_list[stage][location_res_types[k]];

            for (GLuint i = 0; i < list->count; i++)
            {
                GLuint n = list->list[i].array_size > 1 ? (GLuint)list->list[i].array_size : 1;

                if (list->list[i].location != MGL_NO_LOCATION && list->list[i].location + n > next)
                    next = list->list[i].location + n;
            }
        }

    // a location declared in one stage holds for the same uniform in every stage
    for (int k = 0; k < kinds; k++)
        for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        {
            SpirvResourceList *list = &ptr->spirv_resources_list[stage][location_res_types[k]];

            for (GLuint i = 0; i < list->count; i++)
            {
                if (list->list[i].location != MGL_NO_LOCATION || list->list[i].name == NULL)
                    continue;

                for (int other = _VERTEX_SHADER; other < _MAX_SHADER_TYPES; other++)
                {
                    SpirvResourceList *ol = &ptr->spirv_resources_list[other][location_res_types[k]];

                    for (GLuint j = 0; j < ol->count; j++)
                        if (other != stage && ol->list[j].name && ol->list[j].location != MGL_NO_LOCATION &&
                            ol->list[j].explicit_location && !strcmp(ol->list[j].name, list->list[i].name))
                            list->list[i].location = ol->list[j].location;
                }
            }
        }

    for (int k = 0; k < kinds; k++)
    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[stage][location_res_types[k]];

        for (GLuint i = 0; i < list->count; i++)
        {
            bool shared = false;

            // A struct is not a GL uniform -- only its leaves are -- but it
            // still owns the Metal buffer they all live in, and the renderer
            // keys that off a location.
            if (list->list[i].location != MGL_NO_LOCATION)
                continue;

            // the same name in an earlier stage is the same uniform
            for (int prev = _VERTEX_SHADER; prev <= stage && !shared; prev++)
            {
                SpirvResourceList *pl = &ptr->spirv_resources_list[prev][location_res_types[k]];
                GLuint limit = (prev == stage) ? i : pl->count;

                for (GLuint j = 0; j < limit; j++)
                    if (pl->list[j].name && list->list[i].name &&
                        !strcmp(pl->list[j].name, list->list[i].name) &&
                        pl->list[j].location != MGL_NO_LOCATION)
                    {
                        list->list[i].location = pl->list[j].location;
                        shared = true;
                        break;
                    }
            }

            if (!shared)
            {
                GLint n = list->list[i].array_size > 1 ? list->list[i].array_size : 1;

                list->list[i].location = next;
                next += (GLuint)n;
            }
        }
    }

    // a struct leaf writes into the buffer its owner holds, so it needs the
    // owner's location once that is settled
    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];

        for (GLuint i = 0; i < list->count; i++)
        {
            if (list->list[i].offset < 0)
                continue;

            for (GLuint k = 0; k < list->count; k++)
                if (list->list[k].offset < 0 && list->list[k]._id == list->list[i]._id)
                {
                    list->list[i].binding = list->list[k].location;
                    break;
                }
        }
    }
}

// SPIR-V says what the tessellation domain looks like in its OpExecutionMode
// instructions. Both tessellation stages have to be read before either can be
// turned into MSL: the control shader's factor layout depends on the domain,
// which only the evaluation shader declares, and the evaluation shader needs
// the control point count, which only the control shader declares.
static void scanTessExecutionModes(Program *pptr, int stage)
{
    const unsigned *ir = pptr->spirv[stage].ir;
    size_t words = pptr->spirv[stage].size;
    size_t i = 5;   // past the SPIR-V header

    if (ir == NULL || words <= 5)
        return;

    while (i < words)
    {
        unsigned len = ir[i] >> 16;
        unsigned op = ir[i] & 0xFFFF;

        if (len == 0)
            break;

        // 16 is OpExecutionMode; its second operand is the mode itself
        if (op == 16 && len >= 3)
        {
            unsigned mode = ir[i + 2];

            switch (mode)
            {
                case SpvExecutionModeTriangles:
                case SpvExecutionModeQuads:
                case SpvExecutionModeIsolines:
                    if (stage == _TESS_EVALUATION_SHADER)
                        pptr->tess.patch_kind = mode;
                    break;

                case SpvExecutionModeSpacingEqual:
                case SpvExecutionModeSpacingFractionalEven:
                case SpvExecutionModeSpacingFractionalOdd:
                    pptr->tess.partition = mode;
                    break;

                case SpvExecutionModeVertexOrderCw:
                case SpvExecutionModeVertexOrderCcw:
                    pptr->tess.winding = mode;
                    break;

                case SpvExecutionModePointMode:
                    pptr->tess.point_mode = GL_TRUE;
                    break;

                case SpvExecutionModeOutputVertices:
                    if (stage == _TESS_CONTROL_SHADER && len >= 4)
                        pptr->tess.out_control_points = ir[i + 3];
                    break;

                default:
                    break;
            }
        }

        i += len;
    }
}

// ---------------------------------------------------------------------------
// Geometry shaders.
//
// geometry_shaders.c turned the stage into a compute shader and generated the
// vertex shader that draws what it writes. Both are plain GLSL, so they go
// through glslang here rather than through glCompileShader -- the application
// never saw them and has no shader object for them.
// ---------------------------------------------------------------------------

void initGLSLInput(GLMContext ctx, GLuint type, const char *src, glslang_input_t *input);

// Compiles one generated source into SPIR-V and then MSL, filling `out`.
static glslang_stage_t glslangStageFor(int stage)
{
    switch (stage)
    {
        case _TESS_CONTROL_SHADER:    return GLSLANG_STAGE_TESSCONTROL;
        case _TESS_EVALUATION_SHADER: return GLSLANG_STAGE_TESSEVALUATION;
        case _GEOMETRY_SHADER:        return GLSLANG_STAGE_GEOMETRY;
        case _FRAGMENT_SHADER:        return GLSLANG_STAGE_FRAGMENT;
        case _COMPUTE_SHADER:         return GLSLANG_STAGE_COMPUTE;
        default:                      return GLSLANG_STAGE_VERTEX;
    }
}

// glslang's default numbering hands each stage its locations in declaration
// order, so a control shader and an evaluation shader that list the same
// varyings in a different order never match. Its GLSL mapper matches them by
// name instead.
static bool mapProgramIO(glslang_program_t *prog, glslang_stage_t stage)
{
    glslang_mapper_t *mapper = glslang_glsl_mapper_create();
    glslang_resolver_t *resolver = mapper ? glslang_glsl_resolver_create(prog, stage) : NULL;
    bool ok;

    if (mapper == NULL || resolver == NULL)
    {
        glslang_glsl_mapper_delete(mapper);
        return glslang_program_map_io(prog) != 0;
    }

    ok = glslang_program_map_io_with_resolver_and_mapper(prog, resolver, mapper) != 0;
    glslang_glsl_resolver_delete(resolver);
    glslang_glsl_mapper_delete(mapper);

    return ok;
}

// A fresh compile of a stage's source, for linking a generated stage against.
// The real shader object has been linked already, and glslang will not link
// the same intermediate twice without complaint.
static glslang_shader_t *companionShader(GLMContext ctx, GLenum gl_type, const char *src)
{
    glslang_input_t input;
    glslang_shader_t *shader;

    if (src == NULL)
        return NULL;

    initGLSLInput(ctx, gl_type, src, &input);
    shader = glslang_shader_create(&input);

    if (shader == NULL)
        return NULL;

    glslang_shader_set_options(shader, GLSLANG_SHADER_VULKAN_RULES_RELAXED |
                                       GLSLANG_SHADER_AUTO_MAP_LOCATIONS |
                                       GLSLANG_SHADER_AUTO_MAP_BINDINGS);

    if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input))
    {
        glslang_shader_delete(shader);
        return NULL;
    }

    return shader;
}

static bool buildGeneratedStageInto(GLMContext ctx, Program *pptr, GLenum gl_type,
                                    int spirv_slot, Spirv *dest, const char *src, const char *entry,
                                    GLenum companion_type, const char *companion_src)
{
    glslang_input_t input;
    glslang_shader_t *shader;
    glslang_shader_t *companion = companionShader(ctx, companion_type, companion_src);
    glslang_program_t *prog;
    glslang_stage_t stage;

    initGLSLInput(ctx, gl_type, src, &input);
    stage = input.stage;

    shader = glslang_shader_create(&input);

    if (shader == NULL)
    {
        glslang_shader_delete(companion);
        return false;
    }

    glslang_shader_set_options(shader, GLSLANG_SHADER_VULKAN_RULES_RELAXED |
                                       GLSLANG_SHADER_AUTO_MAP_LOCATIONS |
                                       GLSLANG_SHADER_AUTO_MAP_BINDINGS);

    if (!glslang_shader_preprocess(shader, &input) || !glslang_shader_parse(shader, &input))
    {
        MGL_ERR("MGL Error: generated %s shader would not compile:\n%s\n%s\n",
                gl_type == GL_COMPUTE_SHADER ? "compute" : "vertex",
                glslang_shader_get_info_log(shader), src);
        glslang_shader_delete(shader);
        glslang_shader_delete(companion);

        return false;
    }

    prog = glslang_program_create();

    if (prog == NULL)
    {
        glslang_shader_delete(shader);
        glslang_shader_delete(companion);
        return false;
    }

    glslang_program_add_shader(prog, shader);

    // linked with the stage it reads from, so glslang numbers the varyings
    // between them the same way it did for the real program
    if (companion)
        glslang_program_add_shader(prog, companion);

    if (!glslang_program_link(prog, GLSLANG_MSG_DEFAULT_BIT) || !mapProgramIO(prog, stage))
    {
        MGL_ERR("MGL Error: generated shader would not link: %s\n",
                glslang_program_get_info_log(prog));
        glslang_program_delete(prog);
        glslang_shader_delete(shader);
        glslang_shader_delete(companion);

        return false;
    }

    glslang_program_SPIRV_generate(prog, stage);

    Spirv *sp = dest ? dest : (spirv_slot >= 0 ? &pptr->spirv[spirv_slot] : &pptr->gs_passthrough);

    free(sp->ir);
    free(sp->msl_str);
    free(sp->entry_point);
    sp->ir = NULL;
    sp->msl_str = NULL;
    sp->entry_point = NULL;

    sp->stage = (GLuint)stage;
    sp->size = glslang_program_SPIRV_get_size(prog);
    sp->ir = (unsigned int *)malloc(sp->size * sizeof(unsigned));

    if (sp->ir == NULL)
    {
        glslang_program_delete(prog);
        glslang_shader_delete(shader);
        glslang_shader_delete(companion);

        return false;
    }

    glslang_program_SPIRV_get(prog, sp->ir);

    // Fold the application's own main back into the generated one. Left as a
    // separate function, SPIRV-Cross hands it every uniform it touches as a
    // parameter, and Metal will not bind one of those across address spaces.
    {
        unsigned int *inlined = NULL;
        size_t n = 0;

        if (mglInlineSpirv(sp->ir, sp->size, &inlined, &n))
        {
            free(sp->ir);
            sp->ir = inlined;
            sp->size = (unsigned int)n;
        }
    }

    // parseSPIRVShaderToMetal reads the shader slot for its entry point name,
    // which a generated stage does not have, so it is compiled here instead
    sp->msl_str = parseSPIRVShaderToMetal(ctx, pptr, spirv_slot >= 0 ? spirv_slot : _COMPUTE_SHADER,
                                          sp, entry);

    glslang_program_delete(prog);
    glslang_shader_delete(shader);
    glslang_shader_delete(companion);

    if (sp->msl_str == NULL)
        return false;

    return true;
}

static bool buildGeneratedStage(GLMContext ctx, Program *pptr, GLenum gl_type,
                                int spirv_slot, const char *src, const char *entry)
{
    return buildGeneratedStageInto(ctx, pptr, gl_type, spirv_slot, NULL, src, entry, 0, NULL);
}

GLint mglGetUniformLocation(GLMContext ctx, GLuint program, const GLchar *name);

// Rewrites the vertex stage to copy the recorded varyings into the feedback
// buffers, and rebuilds it. Leaves capture off rather than wrong when the
// shader records something MGL cannot lay out.
static GLint mslSlotForName(Program *pptr, int stage, const char *name);

// Which feedback buffers the capturing stage's own xfb qualifiers write,
// whichever stage that is.
static void noteShaderCapture(Program *pptr)
{
    static const int order[] = { _GEOMETRY_SHADER, _TESS_EVALUATION_SHADER, _VERTEX_SHADER };
    MglXfbItem items[64];
    GLint strides[MGL_XFB_MAX_BUFFERS];

    pptr->xfb_shader_buffers = 0;
    memset(pptr->xfb_shader_strides, 0, sizeof(pptr->xfb_shader_strides));

    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); i++)
    {
        Shader *sh = pptr->shader_slots[order[i]];

        if (sh == NULL)
            continue;

        int n = mglXfbLayout(sh->compiled_glsl_shader, items, 64, strides, MGL_XFB_MAX_BUFFERS);

        for (int b = 0; n > 0 && b < MGL_XFB_MAX_BUFFERS; b++)
            pptr->xfb_shader_strides[b] = strides[b];

        // a buffer with only a stride and nothing captured into it needs no buffer bound
        for (int k = 0; k < n; k++)
            if (items[k].buffer >= 0 && items[k].buffer < MGL_XFB_MAX_BUFFERS)
                pptr->xfb_shader_buffers |= 1u << items[k].buffer;
        break;
    }
}

static void linkTransformCapture(GLMContext ctx, Program *pptr)
{
    Shader *vs = pptr->shader_slots[_VERTEX_SHADER];
    char entry[128];

    mglFreeCaptureInfo(&pptr->xfb);

    noteShaderCapture(pptr);

    if (vs == NULL || vs->src == NULL)
        return;

    // the stage that records is the last one before the rasteriser; MGL
    // captures from the vertex stage, which is where it is when there is no
    // tessellation or geometry stage in the way
    if (pptr->tess.active || pptr->geom_shader)
    {
        MGL_INFO("MGL INFO: transform feedback past a tessellation or geometry "
                 "stage is not captured yet\n");
        return;
    }

    if (!mglBuildTransformCapture(vs->pp_src ? vs->pp_src : vs->src, vs->compiled_glsl_shader,
                                  pptr->xfb_varyings, pptr->xfb_varying_count,
                                  pptr->xfb_buffer_mode, "gl_VertexID - mglBase", &pptr->xfb))
        return;

    snprintf(entry, sizeof(entry), "vertex_%d_main", vs->name);

    if (!buildGeneratedStage(ctx, pptr, GL_VERTEX_SHADER, _VERTEX_SHADER,
                             pptr->xfb.rewritten_src, entry))
    {
        mglFreeCaptureInfo(&pptr->xfb);
        return;
    }

    for (int b = 0; b < pptr->xfb.buffer_count; b++)
    {
        char name[64];

        snprintf(name, sizeof(name), "MglXfbB%d", b);
        pptr->xfb.buffer_slot[b] = mslSlotForName(pptr, _VERTEX_SHADER, name);
    }
}

// The capture's own uniforms only have locations once the whole program has
// been numbered, so this runs after assignUniformLocations rather than with
// the rewrite above.
static void resolveTransformCaptureUniforms(Program *pptr)
{
    if (pptr->xfb.rewritten_src == NULL)
        return;

    pptr->xfb.on_loc = mglFindUniformByName(pptr, "mglXfbOnU");
    pptr->xfb.base_loc = mglFindUniformByName(pptr, "mglXfbBaseU");
}

// Where SPIRV-Cross put a generated storage block in a stage's Metal buffer
// slots, or -1 when the shader turned out not to use it.
static GLint mslSlotForName(Program *pptr, int stage, const char *name)
{
    SpirvResourceList *l = &pptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_STORAGE_BUFFER];

    for (GLuint i = 0; i < l->count; i++)
        if (l->list[i].name && !strcmp(l->list[i].name, name))
            return (GLint)l->list[i].msl_index;

    return -1;
}

// Builds the two extra shaders a program that writes gl_CullDistance needs:
// the vertex stage that records the distances, and the compute pass that reads
// them back and keeps the primitives that survived.
static void linkCullProgram(GLMContext ctx, Program *pptr)
{
    Shader *vs = pptr->shader_slots[_VERTEX_SHADER];
    char entry[128];

    mglFreeCullInfo(&pptr->cull);

    if (vs == NULL || vs->src == NULL)
        return;

    // a geometry or tessellation stage would have to do the recording instead,
    // and neither of those is wired up for it yet
    if (pptr->geom_shader || pptr->tess.active)
        return;

    // the application's own macros can hide the array's size, so the rewrite
    // works from the preprocessed source when there is one
    if (!mglBuildCullShaders(vs->pp_src ? vs->pp_src : vs->src, &pptr->cull))
        return;

    snprintf(entry, sizeof(entry), "cull_capture_%d", pptr->name);
    pptr->cull.building = 1;

    if (!buildGeneratedStageInto(ctx, pptr, GL_VERTEX_SHADER, _GEOMETRY_SHADER,
                                 &pptr->cull_capture, pptr->cull.capture_src, entry, 0, NULL))
    {
        pptr->cull.building = 0;
        mglFreeCullInfo(&pptr->cull);
        return;
    }

    snprintf(entry, sizeof(entry), "cull_kernel_%d", pptr->name);
    pptr->cull.building = 2;

    if (!buildGeneratedStageInto(ctx, pptr, GL_COMPUTE_SHADER, _COMPUTE_SHADER,
                                 &pptr->cull_kernel, pptr->cull.kernel_src, entry, 0, NULL))
    {
        pptr->cull.building = 0;
        mglFreeCullInfo(&pptr->cull);
        return;
    }

    pptr->cull.building = 0;

    pptr->cull.cap_out_slot = mslSlotForName(pptr, _GEOMETRY_SHADER, "MglCullB");
    pptr->cull.k_cull_slot  = mslSlotForName(pptr, _COMPUTE_SHADER, "MglCullB");
    pptr->cull.k_src_slot   = mslSlotForName(pptr, _COMPUTE_SHADER, "MglCullSrcB");
    pptr->cull.k_out_slot   = mslSlotForName(pptr, _COMPUTE_SHADER, "MglCullIdxB");
    pptr->cull.k_arg_slot   = mslSlotForName(pptr, _COMPUTE_SHADER, "MglCullArgB");
    pptr->cull.k_cfg_slot   = mslSlotForName(pptr, _COMPUTE_SHADER, "MglCullCfgB");

    if (pptr->cull.cap_out_slot < 0 || pptr->cull.k_cull_slot < 0 ||
        pptr->cull.k_src_slot < 0 || pptr->cull.k_out_slot < 0 ||
        pptr->cull.k_arg_slot < 0 || pptr->cull.k_cfg_slot < 0)
    {
        MGL_ERR("MGL Error: the cull distance pass lost one of its buffers\n");
        mglFreeCullInfo(&pptr->cull);
    }
}

bool linkAndCompileProgramToMetal(GLMContext ctx, Program *pptr, int stage, bool modes_only);

// Transform feedback behind a geometry stage: work out what to copy out of
// the emitted vertices, and into which buffers.
static void linkGeometryCapture(Program *pptr)
{
    // a stand-in geometry stage records what the evaluation stage wrote
    Shader *gs = pptr->synthetic_gs ? pptr->shader_slots[_TESS_EVALUATION_SHADER] : pptr->geom_shader;
    MglXfbItem items[64];
    GLint strides[MGL_XFB_MAX_BUFFERS];
    GLboolean from_shader = GL_FALSE;
    GLuint table[3 * 256];
    int n, words;

    mglFreeCaptureInfo(&pptr->xfb);
    noteShaderCapture(pptr);

    if (gs == NULL)
        return;

    n = mglTransformCaptureItems(gs->pp_src ? gs->pp_src : gs->src, gs->compiled_glsl_shader,
                                 pptr->xfb_varyings, pptr->xfb_varying_count, pptr->xfb_buffer_mode,
                                 items, 64, strides, &from_shader);

    if (n <= 0)
        return;

    words = mglGsGatherTable(&pptr->geom, items, n, table, 256);

    if (words <= 0)
    {
        MGL_INFO("MGL INFO: transform feedback of this geometry shader's outputs is not supported\n");
        return;
    }

    pptr->xfb.gather = (GLuint *)malloc(sizeof(GLuint) * 3 * (size_t)words);

    if (pptr->xfb.gather == NULL)
        return;

    memcpy(pptr->xfb.gather, table, sizeof(GLuint) * 3 * (size_t)words);
    pptr->xfb.gather_words = words;
    pptr->xfb.varying_count = n;
    pptr->xfb.from_shader = from_shader;

    for (int b = 0; b < MGL_XFB_MAX_BUFFERS; b++)
    {
        pptr->xfb.stride_bytes[b] = strides[b];

        if (strides[b] > 0)
            pptr->xfb.buffer_count = b + 1;
    }
}

// With tessellation ahead of it, the evaluation stage feeds the geometry stage
// instead: isolines as points or line segments from a quad grid, triangles and
// quads from the CPU tessellator.
static bool linkTessGeometryProgram(GLMContext ctx, Program *pptr)
{
    Shader *tes = pptr->shader_slots[_TESS_EVALUATION_SHADER];
    char entry[128];
    char *captured;
    bool ok;

    const char *tsrc = tes->pp_src ? tes->pp_src : tes->src;
    int kind = mglTesIsolineKind(tsrc);
    int domain, spacing;
    bool cw, points;

    mglTesLayout(tsrc, &domain, &spacing, &cw, &points);

    // point mode feeds a points shader, isolines a lines one, and triangles
    // and quads a triangles one
    GLenum wants = points ? GL_POINTS : domain == 2 ? GL_LINES : GL_TRIANGLES;

    if (pptr->geom.in_primitive != wants)
    {
        MGL_ERR("MGL Error: the geometry shader's input does not match what tessellation makes\n");
        return false;
    }

    bool general = domain >= 0;

    {
        Shader *w = pptr->tess.has_control ? pptr->shader_slots[_TESS_CONTROL_SHADER]
                                           : pptr->shader_slots[_VERTEX_SHADER];
        const char *wsrc = w ? (w->pp_src ? w->pp_src : w->src) : NULL;
        char *touched = wsrc ? mglTouchTessInputs(tsrc, wsrc, pptr->tess.has_control) : NULL;

        captured = general ? mglAddTessGeneralCapture(touched ? touched : tsrc, &pptr->geom)
                           : mglAddTessPointCapture(touched ? touched : tsrc, &pptr->geom);
        free(touched);
    }

    if (captured == NULL)
    {
        MGL_ERR("MGL Error: the evaluation shader could not be rewritten for the geometry stage\n");
        return false;
    }

    pptr->tess.isoline_segments = kind == 2;
    pptr->tess.general = general;
    pptr->tess.gen_domain = domain;
    pptr->tess.gen_spacing = spacing;
    pptr->tess.gen_cw = cw;
    pptr->tess.gen_points = points;

    // the control shader writes its levels in quad layout, which the CPU reads
    if (general)
        pptr->tess.patch_kind = SpvExecutionModeQuads;

    if (pptr->tess.has_control)
        linkAndCompileProgramToMetal(ctx, pptr, _TESS_CONTROL_SHADER, true);
    else
        pptr->tess.out_control_points = (GLuint)ctx->state.var.patch_vertices;

    pptr->tess.patch_kind = SpvExecutionModeQuads;
    pptr->tess.quad_isolines = !general;

    snprintf(entry, sizeof(entry), "tess_evaluation_%d_main", tes->name);
    {
        Shader *before = pptr->tess.has_control ? pptr->shader_slots[_TESS_CONTROL_SHADER]
                                                : pptr->shader_slots[_VERTEX_SHADER];

        ok = buildGeneratedStageInto(ctx, pptr, GL_TESS_EVALUATION_SHADER, _TESS_EVALUATION_SHADER, NULL,
                                     captured, entry,
                                     pptr->tess.has_control ? GL_TESS_CONTROL_SHADER : GL_VERTEX_SHADER,
                                     before ? (before->pp_src ? before->pp_src : before->src) : NULL);
    }
    free(captured);

    if (!ok)
        return false;

    scanTessExecutionModes(pptr, _TESS_EVALUATION_SHADER);

    if (!linkAndCompileProgramToMetal(ctx, pptr, _VERTEX_SHADER, false))
        return false;

    if (pptr->tess.has_control &&
        !linkAndCompileProgramToMetal(ctx, pptr, _TESS_CONTROL_SHADER, false))
        return false;

    snprintf(entry, sizeof(entry), "geometry_%d", pptr->geom_shader->name);

    if (!buildGeneratedStage(ctx, pptr, GL_COMPUTE_SHADER, _GEOMETRY_SHADER,
                             pptr->geom.compute_src, entry))
        return false;

    snprintf(entry, sizeof(entry), "gs_passthrough_%d", pptr->name);

    if (!buildGeneratedStage(ctx, pptr, GL_VERTEX_SHADER, -1,
                             pptr->geom.passthrough_src, entry))
        return false;

    pptr->geom.vs_in_slot     = mslSlotForName(pptr, _TESS_EVALUATION_SHADER, "MglGsInB");
    pptr->geom.tes_gen_slot   = general ? mslSlotForName(pptr, _TESS_EVALUATION_SHADER, "MglTessGenB") : -1;
    pptr->geom.gs_in_slot     = mslSlotForName(pptr, _GEOMETRY_SHADER, "MglGsInB");
    pptr->geom.gs_out_slot    = mslSlotForName(pptr, _GEOMETRY_SHADER, "MglGsOutB");
    pptr->geom.gs_index_slot  = mslSlotForName(pptr, _GEOMETRY_SHADER, "MglGsIdxB");
    pptr->geom.pass_out_slot  = mslSlotForName(pptr, _COMPUTE_SHADER, "MglGsOutB");

    linkGeometryCapture(pptr);

    return pptr->geom.vs_in_slot >= 0 && pptr->geom.gs_in_slot >= 0 &&
           pptr->geom.gs_out_slot >= 0 && pptr->geom.pass_out_slot >= 0;
}

// Builds the three pieces a geometry program needs: the vertex stage with its
// capture, the geometry stage as compute, and the vertex stage that draws the
// result. Returns false if any of them will not build.
static bool linkGeometryProgram(GLMContext ctx, Program *pptr)
{
    Shader *gs = pptr->geom_shader;
    Shader *vs = pptr->shader_slots[_VERTEX_SHADER];
    char entry[128];
    char *captured;
    bool ok;

    if (gs == NULL || gs->src == NULL)
        return false;

    mglFreeGeometryInfo(&pptr->geom);

    // the preprocessed source, like the cull rewrite above -- macros decide
    // max_vertices and which #ifdef branch of the varyings is real
    if (!mglRewriteGeometryShader(gs->pp_src ? gs->pp_src : gs->src, &pptr->geom))
    {
        MGL_ERR("MGL Error: geometry shader %u is not a shape MGL can rewrite:\n%s\n", gs->name,
                gs->pp_src ? gs->pp_src : gs->src);
        return false;
    }

    if (vs == NULL || vs->src == NULL)
    {
        MGL_ERR("MGL Error: a geometry shader needs a vertex shader to feed it\n");
        return false;
    }

    if (pptr->tess.active)
        return linkTessGeometryProgram(ctx, pptr);

    // the vertex stage writes its output where the geometry stage will read it
    captured = mglAddGeometryCapture(vs->pp_src ? vs->pp_src : vs->src, &pptr->geom);

    if (captured == NULL)
        return false;

    snprintf(entry, sizeof(entry), "vertex_%d_main", vs->name);
    ok = buildGeneratedStage(ctx, pptr, GL_VERTEX_SHADER, _VERTEX_SHADER, captured, entry);
    free(captured);

    if (!ok)
        return false;

    snprintf(entry, sizeof(entry), "geometry_%d", gs->name);

    if (!buildGeneratedStage(ctx, pptr, GL_COMPUTE_SHADER, _GEOMETRY_SHADER,
                             pptr->geom.compute_src, entry))
        return false;

    snprintf(entry, sizeof(entry), "gs_passthrough_%d", pptr->name);

    if (!buildGeneratedStage(ctx, pptr, GL_VERTEX_SHADER, -1,
                             pptr->geom.passthrough_src, entry))
        return false;

    // find where each generated buffer ended up, so the draw can bind them
    pptr->geom.vs_in_slot     = mslSlotForName(pptr, _VERTEX_SHADER, "MglGsInB");
    pptr->geom.gs_in_slot     = mslSlotForName(pptr, _GEOMETRY_SHADER, "MglGsInB");
    pptr->geom.gs_out_slot    = mslSlotForName(pptr, _GEOMETRY_SHADER, "MglGsOutB");
    pptr->geom.gs_index_slot  = mslSlotForName(pptr, _GEOMETRY_SHADER, "MglGsIdxB");
    pptr->geom.pass_out_slot  = mslSlotForName(pptr, _COMPUTE_SHADER, "MglGsOutB");

    linkGeometryCapture(pptr);

    return pptr->geom.vs_in_slot >= 0 && pptr->geom.gs_in_slot >= 0 &&
           pptr->geom.gs_out_slot >= 0 && pptr->geom.pass_out_slot >= 0;
}

bool linkAndCompileProgramToMetal(GLMContext ctx, Program *pptr, int stage, bool modes_only)
{
    glslang_program_t *glsl_program;
    int err;

    /* Safety check: ensure we have a shader for this stage */
    if (!pptr->shader_slots[stage]) {
        return false;
    }

    // Clean up old resources
    if (pptr->spirv[stage].ir) {
        free(pptr->spirv[stage].ir);
        pptr->spirv[stage].ir = NULL;
    }
    if (pptr->spirv[stage].msl_str) {
        free(pptr->spirv[stage].msl_str);
        pptr->spirv[stage].msl_str = NULL;
    }
    if (pptr->spirv[stage].entry_point) {
        free(pptr->spirv[stage].entry_point);
        pptr->spirv[stage].entry_point = NULL;
    }
    if (pptr->spirv[stage].mtl_function) {
        CFRelease(pptr->spirv[stage].mtl_function);
        pptr->spirv[stage].mtl_function = NULL;
    }
    if (pptr->spirv[stage].mtl_library) {
        CFRelease(pptr->spirv[stage].mtl_library);
        pptr->spirv[stage].mtl_library = NULL;
    }

    MGL_INFO("MGL DEBUG: Creating glslang program for stage %d\n", stage);
    glsl_program = glslang_program_create();

    if (glsl_program == NULL)
    {
        MGL_ERR("MGL Error: %s: glslang would not create a program\n", __FUNCTION__);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, false);
    }
    MGL_INFO("MGL DEBUG: Created glslang program %p\n", (void*)glsl_program);

    // shaders to glsl program
    MGL_INFO("MGL DEBUG: Adding shaders to program\n");
    if (!addShadersToProgram(ctx, pptr, glsl_program))
    {
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }
    MGL_INFO("MGL DEBUG: Shaders added\n");

    // link
    MGL_INFO("MGL DEBUG: About to link program\n");
    err = glslang_program_link(glsl_program, GLSLANG_MSG_DEFAULT_BIT);
    MGL_INFO("MGL DEBUG: Program link returned %d\n", err);
    if (!err)
    {
        // this is useful.. but information after this failure isn't that interesting
        MGL_ERR("MGL Error: glslang_program_link failed err: %d\n", err);
        MGL_ERR("MGL Error: glslang_program_SPIRV_get_messages:\n%s\n", glslang_program_SPIRV_get_messages(glsl_program));
        MGL_ERR("MGL Error: glslang_program_get_info_log:\n%s\n", glslang_program_get_info_log(glsl_program));
        MGL_ERR("MGL Error: glslang_program_get_info_debug_log:\n%s\n", glslang_program_get_info_debug_log(glsl_program));

        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    // hands out the locations auto-map left pending, matching them across stages
    if (!mapProgramIO(glsl_program, glslangStageFor(stage)))
    {
        MGL_ERR("MGL Error: glslang_program_map_io failed\n");
        MGL_ERR("MGL Error: glslang_program_get_info_log:\n%s\n", glslang_program_get_info_log(glsl_program));

        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    // generate SPIVR
    MGL_INFO("MGL DEBUG: Generating SPIRV for stage %d\n", stage);
    glslang_program_SPIRV_generate(glsl_program, stage);
    MGL_INFO("MGL DEBUG: SPIRV generated\n");

    if (glslang_program_SPIRV_get_messages(glsl_program))
    {
        DEBUG_PRINT("%s\n", glslang_program_SPIRV_get_messages(glsl_program));

        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    // save SPIRV code
    MGL_INFO("MGL DEBUG: Getting SPIRV size\n");
    pptr->spirv[stage].size = glslang_program_SPIRV_get_size(glsl_program);
    MGL_INFO("MGL DEBUG: SPIRV size: %zu\n", pptr->spirv[stage].size);

    // CRITICAL SECURITY FIX: Prevent integer overflow in SPIRV allocation
    // Check if size * sizeof(unsigned) would overflow size_t
    if (pptr->spirv[stage].size > SIZE_MAX / sizeof(unsigned)) {
        MGL_ERR("MGL SECURITY ERROR: SPIRV size %zu would cause allocation overflow\n", pptr->spirv[stage].size);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, false);
    }

    size_t alloc_size = pptr->spirv[stage].size * sizeof(unsigned);
    pptr->spirv[stage].ir = (unsigned int *)malloc(alloc_size);
    if (!pptr->spirv[stage].ir) {
        MGL_ERR("MGL SECURITY ERROR: Failed to allocate %zu bytes for SPIRV\n", alloc_size);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, false);
    }
    MGL_INFO("MGL DEBUG: Getting SPIRV IR\n");
    glslang_program_SPIRV_get(glsl_program, pptr->spirv[stage].ir);
    MGL_INFO("MGL DEBUG: SPIRV IR obtained\n");

    // Any function the application wrote that touches a uniform hits the same
    // wall as the generated ones: SPIRV-Cross passes the uniform in, and Metal
    // will not take it across address spaces. Folded into main, there is no
    // call to make.
    {
        unsigned int *inlined = NULL;
        size_t n = 0;

        if (mglInlineSpirv(pptr->spirv[stage].ir, pptr->spirv[stage].size, &inlined, &n))
        {
            free(pptr->spirv[stage].ir);
            pptr->spirv[stage].ir = inlined;
            pptr->spirv[stage].size = n;
        }
    }

    if (modes_only)
    {
        scanTessExecutionModes(pptr, stage);

        return true;
    }

    // compile SPIRV to Metal
    MGL_INFO("MGL DEBUG: About to parse SPIRV to Metal\n");
    pptr->spirv[stage].msl_str = parseSPIRVShaderToMetal(ctx, pptr, stage, NULL, NULL);
    MGL_INFO("MGL DEBUG: SPIRV parsed to Metal\n");
    // ERROR_CHECK_RETURN_VALUE(pptr->spirv[stage].msl_str, GL_INVALID_OPERATION, false);
    if (pptr->spirv[stage].msl_str == NULL) {
        MGL_ERR("MGL Error: parseSPIRVShaderToMetal failed for stage %d\n", stage);
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    // the program owns this now; mglFreeProgram deletes it. Deleting it here as
    // well left linked_glsl_program dangling and crashed on glDeleteProgram.
    pptr->linked_glsl_program = glsl_program;
    pptr->dirty_bits |= DIRTY_PROGRAM;

    return true;
}

static void freeSyntheticGeometry(Program *pptr)
{
    if (pptr->synthetic_gs)
    {
        free((char *)pptr->synthetic_gs->src);
        free(pptr->synthetic_gs);
        pptr->synthetic_gs = NULL;
    }

    pptr->geom_shader = NULL;
}

// What the program interface queries answer from, worked out once the link
// has succeeded.
static void reflectProgram(Program *pptr)
{
    void *shaders[_MAX_SHADER_TYPES] = { 0 };
    const char *sources[_MAX_SHADER_TYPES] = { 0 };

    mglFreeResourceTable(&pptr->resources);

    if (pptr->link_status != GL_TRUE)
        return;

    for (int s = 0; s < _MAX_SHADER_TYPES; s++)
    {
        Shader *sh = pptr->shader_slots[s];

        if (sh && sh->compiled_glsl_shader && sh->spirv_binary == NULL)
        {
            shaders[s] = sh->compiled_glsl_shader;
            sources[s] = sh->pp_src ? sh->pp_src : sh->src;
        }
    }

    mglReflectProgram(shaders, sources, _MAX_SHADER_TYPES, &pptr->resources);
}

void mglLinkProgram(GLMContext ctx, GLuint program)
{
    Program *pptr;

    pptr = findProgram(ctx, program);

    if (!pptr)
    {
        // CRITICAL FIX: Handle error gracefully instead of crashing
        MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
        STATE(error) = GL_INVALID_OPERATION;

        return;
    }

    if (pptr->log)
    {
        free(pptr->log);
        pptr->log = NULL;
    }

    pptr->link_status = GL_TRUE;
    pptr->validate_status = GL_FALSE;
    mglFreeResourceTable(&pptr->resources);

    // every link gets a number no other link has had
    static GLuint serial;
    pptr->link_serial = ++serial;
    pptr->linked_separable = pptr->separable;

    for (int s = 0; s < _MAX_SHADER_TYPES; s++)
    {
        free(pptr->stage_src[s]);
        pptr->stage_src[s] = NULL;

        if (pptr->shader_slots[s] && pptr->shader_slots[s]->src)
            pptr->stage_src[s] = strdup(pptr->shader_slots[s]->src);
    }

    // GL 4.6 section 7.3: a program with one tessellation stage and not the
    // other does not link. Both together are the tessellation pipeline.
    memset(&pptr->tess, 0, sizeof(pptr->tess));

    if (pptr->shader_slots[_TESS_CONTROL_SHADER] || pptr->shader_slots[_TESS_EVALUATION_SHADER])
    {
        // An evaluation shader on its own is legal -- the control stage is
        // then fixed function, passing the patch through with the levels set
        // by glPatchParameterfv. A control shader without one is not, unless
        // the program is separable, which is allowed to hold a single stage.
        if (pptr->shader_slots[_TESS_EVALUATION_SHADER] == NULL && !pptr->separable)
        {
            pptr->link_status = GL_FALSE;
            pptr->validate_status = GL_FALSE;
            pptr->log = strdup("link failed: a tessellation control shader needs an "
                               "evaluation shader to go with it");

            return;
        }

        pptr->tess.active = pptr->shader_slots[_TESS_EVALUATION_SHADER] != NULL;
        pptr->tess.has_control = pptr->shader_slots[_TESS_CONTROL_SHADER] != NULL;

        if (pptr->tess.active)
        {
            Shader *tes = pptr->shader_slots[_TESS_EVALUATION_SHADER];
            const char *tsrc = tes->pp_src ? tes->pp_src : tes->src;
            int domain = -1, spacing = 0;
            bool cw = false, points = false;

            if (tsrc)
                mglTesLayout(tsrc, &domain, &spacing, &cw, &points);

            pptr->tess.gl_domain = domain;
            pptr->tess.gl_spacing = spacing;
            pptr->tess.gl_cw = cw;
            pptr->tess.gl_points = points;
        }
        pptr->tess.lower_left = (ctx->state.var.clip_origin == GL_LOWER_LEFT);

        // a separable control shader has no evaluation stage here to take the
        // domain from, so its own declaration is all there is
        if (!pptr->tess.active && pptr->tess.has_control)
            linkAndCompileProgramToMetal(ctx, pptr, _TESS_CONTROL_SHADER, true);
    }

    int stages_linked = 0;

    // GL 4.6 section 7.3: glLinkProgram raises an error only for a bad program
    // object. A stage that will not build sets LINK_STATUS false instead.
    ctx->error_suppress++;

    // Point-mode isolines on their own get a geometry stage that passes each
    // point through, since Metal cannot draw isolines at all.
    freeSyntheticGeometry(pptr);
    pptr->geom_shader = pptr->shader_slots[_GEOMETRY_SHADER];

    if (pptr->geom_shader == NULL && pptr->tess.active)
    {
        Shader *tes = pptr->shader_slots[_TESS_EVALUATION_SHADER];
        const char *tsrc = tes->pp_src ? tes->pp_src : tes->src;

        int kind = 0, input = 0;

        // Metal cannot tessellate isolines at all, and has no point mode.
        // Those, and triangles and quads when something has to see the
        // primitives one by one for transform feedback, are cut up on the CPU
        // and drawn through a geometry stage that passes them straight on.
        if (tsrc)
        {
            int domain, spacing;
            bool cw, points;

            mglTesLayout(tsrc, &domain, &spacing, &cw, &points);

            if (domain == 2 || points ||
                ((domain == 0 || domain == 1) && (pptr->xfb_varying_count > 0 || strstr(tsrc, "xfb_"))))
            {
                kind = 1;
                input = points ? 0 : domain == 2 ? 1 : 2;
            }
        }

        if (kind)
        {
            Shader *fake = (Shader *)calloc(1, sizeof(Shader));
            char *gsrc = mglPassThroughGeometry(tsrc, input);

            if (fake && gsrc)
            {
                fake->name = tes->name;
                fake->type = GL_GEOMETRY_SHADER;
                fake->src = gsrc;
                pptr->synthetic_gs = fake;
                pptr->geom_shader = fake;
            }
            else
            {
                free(fake);
                free(gsrc);
            }
        }
    }

    // A separable program with a geometry stage but no vertex stage gets that
    // from another program in a pipeline, so there is nothing to build for it
    // on its own; the pipeline links the stages together when it draws.
    if (pptr->geom_shader && pptr->separable && pptr->shader_slots[_VERTEX_SHADER] == NULL)
    {
        assignUniformLocations(pptr);
        pptr->validate_status = pptr->link_status;
        reflectProgram(pptr);
        ctx->error_suppress--;

        return;
    }

    // A geometry shader never reaches Metal as one; it is rewritten into a
    // compute pass with a generated vertex shader in front of the raster.
    if (pptr->geom_shader)
    {
        if (linkGeometryProgram(ctx, pptr) == false)
        {
            pptr->link_status = GL_FALSE;
            pptr->validate_status = GL_FALSE;

            if (pptr->log == NULL)
                pptr->log = strdup("link failed: the geometry shader could not be rewritten");

            ctx->error_suppress--;

            return;
        }

        // the fragment stage is the only one left that still builds normally
        if (pptr->shader_slots[_FRAGMENT_SHADER] &&
            linkAndCompileProgramToMetal(ctx, pptr, _FRAGMENT_SHADER, false) == false)
            pptr->link_status = GL_FALSE;

        assignUniformLocations(pptr);

        if (uniformLocationProblem(pptr))
        {
            pptr->link_status = GL_FALSE;

            if (pptr->log == NULL)
                pptr->log = strdup(uniformLocationProblem(pptr));
        }
        pptr->num_samples_loc = mglFindNumSamplesLocation(pptr);
        pptr->sample_mask_off_loc = mglFindSampleMaskOffLocation(pptr);
        pptr->tess.patches_loc = mglFindUniformByName(pptr, "mglPatchesU");

        pptr->geom.prims_loc   = mglFindUniformByName(pptr, "mglGsPrimsU");
        pptr->geom.indexed_loc = mglFindUniformByName(pptr, "mglGsIndexedU");
        pptr->geom.first_loc   = mglFindUniformByName(pptr, "mglGsFirstU");
        pptr->geom.stride_loc  = mglFindUniformByName(pptr, "mglGsStrideU");

        if (pptr->link_status == GL_TRUE)
        {
            pptr->dirty_bits |= DIRTY_PROGRAM;

            if (ctx->mtl_funcs.mtlBindProgram(ctx, pptr) == false)
            {
                pptr->link_status = GL_FALSE;

                if (pptr->log == NULL)
                    pptr->log = strdup("link failed: Metal rejected the generated MSL");
            }
        }

        pptr->validate_status = pptr->link_status;
        reflectProgram(pptr);
        ctx->error_suppress--;

        return;
    }

    if (pptr->tess.active)
    {
        if (pptr->tess.has_control)
            linkAndCompileProgramToMetal(ctx, pptr, _TESS_CONTROL_SHADER, true);
        else
            // no control shader: the patch reaches the evaluation stage exactly
            // as the application laid it out
            pptr->tess.out_control_points = (GLuint)ctx->state.var.patch_vertices;

        linkAndCompileProgramToMetal(ctx, pptr, _TESS_EVALUATION_SHADER, true);

        // Metal cannot tessellate isolines at all. Refusing the link says so
        // where the application can see it.
        if (pptr->tess.patch_kind == SpvExecutionModeIsolines)
        {
            pptr->link_status = GL_FALSE;
            pptr->validate_status = GL_FALSE;
            pptr->log = strdup("link failed: Metal has no isoline tessellation");
            ctx->error_suppress--;

            return;
        }
    }

    for (int stage=0; stage<_MAX_SHADER_TYPES; stage++)
    {
        pptr->spirv[stage].msl_str = 0;

        if (pptr->shader_slots[stage] == NULL)
            continue;

        // an evaluation shader that ignores a built-in the stage before it
        // writes is rebuilt with a read of it, so the two lay out the buffer
        // between them the same way
        if (stage == _TESS_EVALUATION_SHADER && pptr->tess.active)
        {
            Shader *tes = pptr->shader_slots[stage];
            Shader *w = pptr->tess.has_control ? pptr->shader_slots[_TESS_CONTROL_SHADER]
                                               : pptr->shader_slots[_VERTEX_SHADER];
            const char *tsrc = tes->pp_src ? tes->pp_src : tes->src;
            const char *wsrc = w ? (w->pp_src ? w->pp_src : w->src) : NULL;
            char *touched = (tsrc && wsrc) ? mglTouchTessInputs(tsrc, wsrc, pptr->tess.has_control) : NULL;

            if (touched)
            {
                char entry[128];
                bool ok;

                snprintf(entry, sizeof(entry), "tess_evaluation_%d_main", tes->name);
                ok = buildGeneratedStageInto(ctx, pptr, GL_TESS_EVALUATION_SHADER, stage, NULL, touched, entry,
                                             pptr->tess.has_control ? GL_TESS_CONTROL_SHADER : GL_VERTEX_SHADER, wsrc);
                free(touched);

                if (ok)
                {
                    scanTessExecutionModes(pptr, stage);
                    stages_linked++;
                }
                else
                    pptr->link_status = GL_FALSE;

                continue;
            }
        }

        if (linkAndCompileProgramToMetal(ctx, pptr, stage, false) == false)
            pptr->link_status = GL_FALSE;
        else
            stages_linked++;
    }

    // a program with no stage, or one whose stage failed, did not link
    if (stages_linked == 0)
        pptr->link_status = GL_FALSE;

    // the subroutines the rewrite found belong to the program from here on
    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        if (pptr->shader_slots[stage])
            mglCopySubroutineInfo(&pptr->subroutines[stage], &pptr->shader_slots[stage]->subroutines);

    linkTransformCapture(ctx, pptr);

    if (pptr->link_status == GL_TRUE)
        linkCullProgram(ctx, pptr);

    assignUniformLocations(pptr);

    if (pptr->link_status == GL_TRUE && uniformLocationProblem(pptr))
    {
        pptr->link_status = GL_FALSE;
        pptr->log = strdup(uniformLocationProblem(pptr));
    }

    resolveTransformCaptureUniforms(pptr);

    pptr->num_samples_loc = mglFindNumSamplesLocation(pptr);
    pptr->sample_mask_off_loc = mglFindSampleMaskOffLocation(pptr);
    pptr->tess.patches_loc = mglFindUniformByName(pptr, "mglPatchesU");

    // Hand the MSL to Metal now rather than at the first draw. GL callers expect
    // shader problems at link time, and a program that only fails later reports
    // a successful link and then quietly draws nothing.
    if (pptr->link_status == GL_TRUE)
    {
        pptr->dirty_bits |= DIRTY_PROGRAM;

        if (ctx->mtl_funcs.mtlBindProgram(ctx, pptr) == false)
        {
            pptr->link_status = GL_FALSE;

            if (pptr->log == NULL)
                pptr->log = strdup("link failed: Metal rejected the generated MSL");
        }
    }

    if (pptr->link_status == GL_FALSE && pptr->log == NULL)
        pptr->log = strdup("link failed: no usable shader stage");

    pptr->validate_status = pptr->link_status;
    reflectProgram(pptr);

    /* Only call mtlBindProgram if Metal functions are initialized */
    if (ctx->mtl_funcs.mtlBindProgram) {
        ctx->mtl_funcs.mtlBindProgram(ctx, pptr);
    } else {
        MGL_INFO("WARNING: Metal functions not initialized, skipping mtlBindProgram\n");
    }

    ctx->error_suppress--;

    //ERROR_CHECK_RETURN(pptr->mtl_data, GL_INVALID_OPERATION);
}

void mglUseProgram(GLMContext ctx, GLuint program)
{
    Program *pptr;

    if (program)
    {
        pptr = findProgram(ctx, program);

        if (!pptr)
        {
            MGL_ERR("MGL Error: mglUseProgram program %u not found\n", program);
            // CRITICAL FIX: Handle error gracefully instead of crashing
        MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
        STATE(error) = GL_INVALID_OPERATION;

            return;
        }

        ERROR_CHECK_RETURN(pptr->linked_glsl_program, GL_INVALID_OPERATION);
    }
    else
    {
        pptr = NULL;
    }

    if (ctx->state.program != pptr)
    {
        if (ctx->state.program)
        {
            ctx->state.program->refcount--;
            if (ctx->state.program->refcount == 0 && ctx->state.program->delete_status)
            {
                mglFreeProgram(ctx, ctx->state.program);
            }
        }

        ctx->state.program = pptr;

        // the queryable GL_CURRENT_PROGRAM lives in a separate variable
        STATE_VAR(current_program) = pptr ? pptr->name : 0;

        if (ctx->state.program)
        {
            ctx->state.program->refcount++;
            // Only mark dirty when binding a valid program
            // Don't mark dirty when unbinding (pptr=NULL) to preserve existing pipeline
            ctx->state.dirty_bits |= DIRTY_PROGRAM;
        }
        // When unbinding (pptr=NULL), don't mark dirty - keep existing pipeline state
    }
}

// Copies a name into a caller's buffer the way the GL name queries do.
static void copyResourceName(const char *src, GLsizei bufSize, GLsizei *length, GLchar *dst)
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

static void recordBinding(__typeof__(((Program *)0)->attrib_binds[0]) *binds, GLint *count,
                          const char *name, GLuint location, GLuint index)
{
    for (GLint i = 0; i < *count; i++)
        if (!strcmp(binds[i].name, name))
        {
            binds[i].location = location;
            binds[i].index = index;
            return;
        }

    if (*count >= 32)
        return;

    binds[*count].name = strdup(name);
    binds[*count].location = location;
    binds[*count].index = index;
    (*count)++;
}

void mglBindAttribLocation(GLMContext ctx, GLuint program, GLuint index, const GLchar *name)
{
    Program *ptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(name, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(index < ctx->state.max_vertex_attribs, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(strncmp(name, "gl_", 3) != 0, GL_INVALID_OPERATION);

    // takes effect at the next link, and a later call for the same name wins
    recordBinding(ptr->attrib_binds, &ptr->attrib_bind_count, name, index, 0);
}

void mglGetActiveAttrib(GLMContext ctx, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    Program *ptr = findProgram(ctx, program);
    SpirvResourceList *l;
    SpirvResource *res;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    // vertex attributes are the vertex stage's inputs
    l = &ptr->spirv_resources_list[_VERTEX_SHADER][SPVC_RESOURCE_TYPE_STAGE_INPUT];

    ERROR_CHECK_RETURN(index < l->count, GL_INVALID_VALUE);

    res = &l->list[index];

    if (size) *size = res->array_size ? res->array_size : 1;
    if (type) *type = res->gl_type;

    if (name) copyResourceName(res->name ? res->name : "", bufSize, length, name);
    else if (length) *length = 0;
}

// GetActiveUniform lives in uniforms.c, next to the reflection helpers

void mglGetAttachedShaders(GLMContext ctx, GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)
{
    Program *pptr = findProgram(ctx, program);
    GLsizei n = 0;

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(maxCount >= 0, GL_INVALID_VALUE);

    for (int i = 0; i < _MAX_SHADER_TYPES && n < maxCount; i++)
    {
        if (pptr->shader_slots[i] == NULL)
            continue;

        if (shaders)
            shaders[n] = pptr->shader_slots[i]->name;

        n++;
    }

    if (count)
        *count = n;
}

GLint  mglGetAttribLocation(GLMContext ctx, GLuint program, const GLchar *name)
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

	// An array attribute takes one location per element, and GL lets the
	// caller ask for any of them by index.
	char base[256];
	GLint element = 0;
	const char *bracket = strchr(name, '[');

	if (bracket)
	{
		size_t n = (size_t)(bracket - name);

		if (n >= sizeof(base))
			return -1;

		memcpy(base, name, n);
		base[n] = 0;
		element = atoi(bracket + 1);

		if (element < 0)
			return -1;
	}
	else
	{
		if (strlen(name) >= sizeof(base))
			return -1;

		strcpy(base, name);
	}

	{
		SpirvResourceList *l = &ptr->spirv_resources_list[_VERTEX_SHADER][SPVC_RESOURCE_TYPE_STAGE_INPUT];

		for (GLuint i = 0; i < l->count; i++)
		{
			if (l->list[i].name == NULL || strcmp(l->list[i].name, base))
				continue;

			if (element > 0 && (GLuint)element >= l->list[i].array_size)
				return -1;

			return (GLint)l->list[i].location + element;
		}
	}

	return -1;
}

static int programResourceCount(Program *ptr, int res_type)
{
    int n = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[stage][res_type];

        for (GLuint i = 0; i < list->count; i++)
        {
            // the same block or buffer named in two stages is one resource
            if (res_type == SPVC_RESOURCE_TYPE_UNIFORM_BUFFER ||
                res_type == SPVC_RESOURCE_TYPE_STORAGE_BUFFER)
            {
                if (programResourceSeenEarlier(ptr, res_type, stage, i))
                    continue;

                // an instance array is one block per element
                n += list->list[i].array_size > 1 ? list->list[i].array_size : 1;
                continue;
            }

            n++;
        }
    }

    return n;
}

static int programUniformCount(Program *ptr)
{
    int block_members = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        block_members += ptr->block_uniforms[stage].count;

    // samplers, images and block members are all active uniforms too
    return block_members +
           programResourceCount(ptr, SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT) +
           programResourceCount(ptr, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE) +
           programResourceCount(ptr, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE) +
           programResourceCount(ptr, SPVC_RESOURCE_TYPE_STORAGE_IMAGE) +
           programResourceCount(ptr, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS);
}

static int programLongestName(Program *ptr, int res_type)
{
    int longest = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *list = &ptr->spirv_resources_list[stage][res_type];

        for (GLuint i = 0; i < list->count; i++)
        {
            int len = (int)strlen(list->list[i].name) + 1;

            if (len > longest) longest = len;
        }
    }

    return longest;
}

void mglGetProgramiv(GLMContext ctx, GLuint program, GLenum pname, GLint *params)
{
    Program *pptr = findProgram(ctx, program);
    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    
    switch (pname) {
        case GL_PROGRAM_SEPARABLE:
            *params = pptr->linked_separable;
            break;

        case GL_LINK_STATUS:
            *params = pptr->link_status;
            break;
        case GL_DELETE_STATUS:
            *params = pptr->delete_status;
            break;
        case GL_VALIDATE_STATUS:
            *params = pptr->validate_status;
            break;
        case GL_INFO_LOG_LENGTH:
            *params = pptr->log ? (GLint)strlen(pptr->log) + 1 : 0;
            break;
        case GL_ATTACHED_SHADERS:
            {
                int count = 0;
                for (int i = 0; i < _MAX_SHADER_TYPES; i++) {
                    if (pptr->shader_slots[i]) count++;
                }
                *params = count;
            }
            break;
        case GL_ACTIVE_UNIFORMS:
            *params = programUniformCount(pptr);
            break;

        case GL_ACTIVE_UNIFORM_MAX_LENGTH:
            *params = programLongestName(pptr, SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT);
            break;

        case GL_ACTIVE_ATTRIBUTES:
            *params = programResourceCount(pptr, SPVC_RESOURCE_TYPE_STAGE_INPUT);
            break;

        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
            *params = programLongestName(pptr, SPVC_RESOURCE_TYPE_STAGE_INPUT);
            break;

        case GL_ACTIVE_UNIFORM_BLOCKS:
            *params = programResourceCount(pptr, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER);
            break;

        case GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH:
            *params = programLongestName(pptr, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER);
            break;
        case GL_COMPUTE_WORK_GROUP_SIZE:
            if (pptr->shader_slots[_COMPUTE_SHADER]) {
                /* Return local workgroup size for compute shaders */
                params[0] = pptr->local_workgroup_size.x;
                params[1] = pptr->local_workgroup_size.y;
                params[2] = pptr->local_workgroup_size.z;
            } else {
                params[0] = params[1] = params[2] = 0;
            }
            break;
        // what the tessellation stages declared, read out of their SPIR-V
        case GL_TESS_CONTROL_OUTPUT_VERTICES:
            *params = (GLint)pptr->tess.out_control_points;
            break;

        // as the evaluation shader declared them, whatever Metal is running
        case GL_TESS_GEN_MODE:
            *params = pptr->tess.gl_domain == 1 ? GL_QUADS :
                      pptr->tess.gl_domain == 2 ? GL_ISOLINES : GL_TRIANGLES;
            break;

        case GL_TESS_GEN_SPACING:
            *params = pptr->tess.gl_spacing == 1 ? GL_FRACTIONAL_EVEN :
                      pptr->tess.gl_spacing == 2 ? GL_FRACTIONAL_ODD : GL_EQUAL;
            break;

        case GL_TESS_GEN_VERTEX_ORDER:
            *params = pptr->tess.gl_cw ? GL_CW : GL_CCW;
            break;

        case GL_TESS_GEN_POINT_MODE:
            *params = pptr->tess.gl_points ? GL_TRUE : GL_FALSE;
            break;

        default:
            MGL_INFO("mglGetProgramiv: unhandled pname 0x%x\n", pname);
            *params = 0;
            break;
    }
}

void mglGetProgramInfoLog(GLMContext ctx, GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
    Program *pptr = findProgram(ctx, program);
    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    const char *src = pptr->log ? pptr->log : "";
    GLsizei n = 0;

    if (infoLog && bufSize > 0)
    {
        while (n < bufSize - 1 && src[n])
        {
            infoLog[n] = src[n];
            n++;
        }

        infoLog[n] = 0;
    }

    if (length)
        *length = n;
}



#pragma mark program pipelines
void mglGenProgramPipelines(GLMContext ctx, GLsizei n, GLuint *pipelines)
{
    for (GLsizei i = 0; i < n; i++)
    {
        pipelines[i] = getNewName(&STATE(program_pipeline_table));
        getProgramPipeline(ctx, pipelines[i]);
    }
}

GLboolean mglIsProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    ProgramPipeline *ptr = findProgramPipeline(ctx, pipeline);
    return ptr && ptr->created ? GL_TRUE : GL_FALSE;
}

void mglDropPipelineProgram(GLMContext ctx, ProgramPipeline *pp);
static void setPipelineStage(GLMContext ctx, ProgramPipeline *pp, int stage, Program *p);

void mglDeleteProgramPipelines(GLMContext ctx, GLsizei n, const GLuint *pipelines)
{
    for (GLsizei i = 0; i < n; i++)
    {
        if (pipelines[i] == 0)
            continue;
            
        ProgramPipeline *ptr = findProgramPipeline(ctx, pipelines[i]);
        if (!ptr)
            continue;
            
        // If deleting currently bound pipeline, unbind it
        if (STATE(program_pipeline) && STATE(program_pipeline)->name == pipelines[i])
        {
            STATE(program_pipeline) = NULL;
            STATE(var.program_pipeline_binding) = 0;
        }
        
        mglDropPipelineProgram(ctx, ptr);

        for (int s = 0; s < _MAX_SHADER_TYPES; s++)
            setPipelineStage(ctx, ptr, s, NULL);

        // Remove from hash table and free
        deleteHashElement(&STATE(program_pipeline_table), pipelines[i]);
        free(ptr);
    }
}

void mglBindProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    if (pipeline == 0)
    {
        STATE(program_pipeline) = NULL;
        STATE(var.program_pipeline_binding) = 0;
        STATE(dirty_bits) |= DIRTY_PROGRAM;
        return;
    }
    
    // only a name GenProgramPipelines handed out may be bound
    ProgramPipeline *ptr = findProgramPipeline(ctx, pipeline);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    ptr->created = GL_TRUE;
    STATE(program_pipeline) = ptr;
    STATE(var.program_pipeline_binding) = pipeline;
    STATE(dirty_bits) |= DIRTY_PROGRAM;
}

// A pipeline stage holds its program the way glUseProgram does, so deleting
// the program only flags it until the last stage lets go.
static void setPipelineStage(GLMContext ctx, ProgramPipeline *pp, int stage, Program *p)
{
    Program *old = pp->stage_programs[stage];

    if (old == p)
        return;

    if (p)
        p->refcount++;

    pp->stage_programs[stage] = p;

    if (old && --old->refcount == 0 && old->delete_status)
        mglFreeProgram(ctx, old);
}

// A program for a pipeline stage: zero, or a linked separable program. Raises
// the error GL gives for anything else and returns false.
static bool pipelineStageProgram(GLMContext ctx, GLuint program, bool need_separable, Program **out)
{
    *out = NULL;

    if (program == 0)
        return true;

    Program *pptr = findProgram(ctx, program);

    if (pptr == NULL)
    {
        // a shader's name is the wrong kind of object, anything else no object
        STATE(error) = findShader(ctx, program) ? GL_INVALID_OPERATION : GL_INVALID_VALUE;
        return false;
    }

    if (pptr->link_status != GL_TRUE || (need_separable && !pptr->linked_separable))
    {
        STATE(error) = GL_INVALID_OPERATION;
        return false;
    }

    *out = pptr;
    return true;
}

void mglUseProgramStages(GLMContext ctx, GLuint pipeline, GLbitfield stages, GLuint program)
{
    static const struct { GLbitfield bit; int stage; } map[] = {
        { GL_VERTEX_SHADER_BIT, _VERTEX_SHADER },
        { GL_TESS_CONTROL_SHADER_BIT, _TESS_CONTROL_SHADER },
        { GL_TESS_EVALUATION_SHADER_BIT, _TESS_EVALUATION_SHADER },
        { GL_GEOMETRY_SHADER_BIT, _GEOMETRY_SHADER },
        { GL_FRAGMENT_SHADER_BIT, _FRAGMENT_SHADER },
        { GL_COMPUTE_SHADER_BIT, _COMPUTE_SHADER },
    };
    GLbitfield known = 0;
    ProgramPipeline *pipe_ptr = findProgramPipeline(ctx, pipeline);
    Program *prog_ptr;

    ERROR_CHECK_RETURN(pipe_ptr, GL_INVALID_OPERATION);

    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        known |= map[i].bit;

    ERROR_CHECK_RETURN(stages == GL_ALL_SHADER_BITS || (stages & ~known) == 0, GL_INVALID_VALUE);

    if (!pipelineStageProgram(ctx, program, true, &prog_ptr))
        return;

    pipe_ptr->created = GL_TRUE;

    // a program with nothing for a stage leaves that stage empty
    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (stages & map[i].bit)
            setPipelineStage(ctx, pipe_ptr, map[i].stage,
                             prog_ptr && prog_ptr->stage_src[map[i].stage] ? prog_ptr : NULL);

    pipe_ptr->validated = GL_FALSE;
    STATE(dirty_bits) |= DIRTY_PROGRAM;
}

/* ---------- subroutines ---------- */

// MGL's linker captures no subroutines, so every program has zero of them.
// These answer as a program with none would, rather than pretending to work.

bool validShaderType(GLenum shadertype)
{
    switch (shadertype)
    {
        case GL_VERTEX_SHADER:
        case GL_FRAGMENT_SHADER:
        case GL_GEOMETRY_SHADER:
        case GL_TESS_CONTROL_SHADER:
        case GL_TESS_EVALUATION_SHADER:
        case GL_COMPUTE_SHADER:
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Subroutines. subroutines.c rewrote them out of the GLSL before glslang saw
// it and left a record of what it found on the shader; these queries answer
// from that record, and the uniform write lands in the int it generated.
// ---------------------------------------------------------------------------

GLuint glShaderTypeToGLMType(GLuint type);
GLint mglGetUniformLocation(GLMContext ctx, GLuint program, const GLchar *name);
void programUniformWrite(GLMContext ctx, Program *pptr, GLint location, const void *ptr, GLsizei size);

static SubroutineInfo *stageSubroutines(Program *pptr, GLenum shadertype)
{
    int stage = glShaderTypeToGLMType(shadertype);

    if (stage < 0 || stage >= _MAX_SHADER_TYPES)
        return NULL;

    // the program's own copy, since the shader it came from may be long gone
    return &pptr->subroutines[stage];
}

static GLuint subroutineSize(const SubroutineInfo *si, GLuint u)
{
    return si->uniform_array_size[u] ? si->uniform_array_size[u] : 1;
}

static bool subroutineExplicitOverlap(const SubroutineInfo *si, GLint at, GLuint n)
{
    for (GLuint j = 0; j < si->uniform_count; j++)
    {
        GLint loc = si->uniform_location ? si->uniform_location[j] : -1;

        if (loc >= 0 && at < loc + (GLint)subroutineSize(si, j) && loc < at + (GLint)n)
            return true;
    }

    return false;
}

// Where uniform u's locations start. A declared location stays put; the rest
// fill the gaps in order.
static GLint subroutineBase(const SubroutineInfo *si, GLuint u)
{
    GLint next = 0;

    if (si->uniform_location && si->uniform_location[u] >= 0)
        return si->uniform_location[u];

    for (GLuint j = 0; j <= u; j++)
    {
        if (si->uniform_location && si->uniform_location[j] >= 0)
            continue;

        while (subroutineExplicitOverlap(si, next, subroutineSize(si, j)))
            next++;

        if (j == u)
            return next;

        next += (GLint)subroutineSize(si, j);
    }

    return next;
}

// GL counts one location per array element, so an array of two takes two.
static GLuint subroutineLocationCount(const SubroutineInfo *si)
{
    GLuint top = 0;

    for (GLuint i = 0; i < si->uniform_count; i++)
    {
        GLuint end = (GLuint)subroutineBase(si, i) + subroutineSize(si, i);

        if (end > top)
            top = end;
    }

    return top;
}

// Which uniform a location belongs to, and which element of it.
static int subroutineUniformAt(const SubroutineInfo *si, GLuint location, GLuint *element)
{
    for (GLuint i = 0; i < si->uniform_count; i++)
    {
        GLuint base = (GLuint)subroutineBase(si, i);

        if (location >= base && location < base + subroutineSize(si, i))
        {
            if (element)
                *element = location - base;

            return (int)i;
        }
    }

    return -1;
}

static GLint subroutineLocationOf(const SubroutineInfo *si, const char *name)
{
    char base[256];
    GLint element = 0;
    const char *open = strchr(name, '[');

    snprintf(base, sizeof(base), "%.*s", open ? (int)(open - name) : (int)strlen(name), name);

    if (open)
        element = atoi(open + 1);

    for (GLuint i = 0; i < si->uniform_count; i++)
    {
        if (si->uniform_names[i] == NULL || strcmp(si->uniform_names[i], base))
            continue;

        if (element < 0 || (GLuint)element >= subroutineSize(si, i))
            return -1;

        return subroutineBase(si, i) + element;
    }

    return -1;
}

void mglGetActiveSubroutineName(GLMContext ctx, GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    Program *pptr = findProgram(ctx, program);
    SubroutineInfo *si;

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(validShaderType(shadertype), GL_INVALID_ENUM);

    si = stageSubroutines(pptr, shadertype);

    ERROR_CHECK_RETURN(si && index < si->fn_count, GL_INVALID_VALUE);

    copyResourceName(si->fn_names[index], bufSize, length, name);
}

void mglGetActiveSubroutineUniformiv(GLMContext ctx, GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint *values)
{
    Program *pptr = findProgram(ctx, program);
    SubroutineInfo *si;

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(values, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(validShaderType(shadertype), GL_INVALID_ENUM);

    si = stageSubroutines(pptr, shadertype);

    ERROR_CHECK_RETURN(si && index < si->uniform_count, GL_INVALID_VALUE);

    switch (pname)
    {
        case GL_NUM_COMPATIBLE_SUBROUTINES:
            values[0] = (GLint)si->uniform_compatible[index];
            break;

        case GL_COMPATIBLE_SUBROUTINES:
            // MGL keeps one dispatcher per uniform and numbers every
            // subroutine in the stage, so the compatible set is the set of
            // functions that dispatcher switches on
            for (GLuint i = 0, n = 0; i < si->fn_count; i++)
                if (mglSubroutineCompatible(si, index, i))
                    values[n++] = (GLint)i;
            break;

        case GL_UNIFORM_SIZE:
            values[0] = (GLint)(si->uniform_array_size[index] ? si->uniform_array_size[index] : 1);
            break;

        case GL_UNIFORM_NAME_LENGTH:
            values[0] = (GLint)strlen(si->uniform_names[index]) + 1;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetActiveSubroutineUniformName(GLMContext ctx, GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    Program *pptr = findProgram(ctx, program);
    SubroutineInfo *si;

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(validShaderType(shadertype), GL_INVALID_ENUM);

    si = stageSubroutines(pptr, shadertype);

    ERROR_CHECK_RETURN(si && index < si->uniform_count, GL_INVALID_VALUE);

    copyResourceName(si->uniform_names[index], bufSize, length, name);
}

GLuint mglGetSubroutineIndex(GLMContext ctx, GLuint program, GLenum shadertype, const GLchar *name)
{
    Program *pptr = findProgram(ctx, program);
    SubroutineInfo *si;

    ERROR_CHECK_RETURN_VALUE(pptr, GL_INVALID_VALUE, GL_INVALID_INDEX);
    ERROR_CHECK_RETURN_VALUE(name, GL_INVALID_VALUE, GL_INVALID_INDEX);
    ERROR_CHECK_RETURN_VALUE(validShaderType(shadertype), GL_INVALID_ENUM, GL_INVALID_INDEX);

    si = stageSubroutines(pptr, shadertype);

    if (si == NULL)
        return GL_INVALID_INDEX;

    for (GLuint i = 0; i < si->fn_count; i++)
        if (si->fn_names[i] && !strcmp(si->fn_names[i], name))
            return i;

    return GL_INVALID_INDEX;
}

GLint mglGetSubroutineUniformLocation(GLMContext ctx, GLuint program, GLenum shadertype, const GLchar *name)
{
    Program *pptr = findProgram(ctx, program);
    SubroutineInfo *si;

    ERROR_CHECK_RETURN_VALUE(pptr, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(name, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(validShaderType(shadertype), GL_INVALID_ENUM, -1);

    si = stageSubroutines(pptr, shadertype);

    return si ? subroutineLocationOf(si, name) : -1;
}

void mglGetUniformSubroutineuiv(GLMContext ctx, GLenum shadertype, GLint location, GLuint *params)
{
    Program *pptr = ctx->state.program;
    SubroutineInfo *si;
    int stage;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(pptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(validShaderType(shadertype), GL_INVALID_ENUM);

    si = stageSubroutines(pptr, shadertype);
    stage = glShaderTypeToGLMType(shadertype);

    ERROR_CHECK_RETURN(si && location >= 0 &&
                       (GLuint)location < subroutineLocationCount(si), GL_INVALID_VALUE);

    params[0] = pptr->subroutine_values[stage]
              ? pptr->subroutine_values[stage][location] : 0;
}

void mglUniformSubroutinesuiv(GLMContext ctx, GLenum shadertype, GLsizei count, const GLuint *indices)
{
    Program *pptr = ctx->state.program;
    SubroutineInfo *si;
    int stage;
    GLuint locations;

    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(pptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(validShaderType(shadertype), GL_INVALID_ENUM);

    si = stageSubroutines(pptr, shadertype);
    stage = glShaderTypeToGLMType(shadertype);
    locations = si ? subroutineLocationCount(si) : 0;

    ERROR_CHECK_RETURN((GLuint)count == locations, GL_INVALID_VALUE);

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(indices, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < count; i++)
        ERROR_CHECK_RETURN(indices[i] < si->fn_count, GL_INVALID_VALUE);

    if (pptr->subroutine_values[stage] == NULL)
    {
        pptr->subroutine_values[stage] = (GLuint *)calloc(locations, sizeof(GLuint));

        ERROR_CHECK_RETURN(pptr->subroutine_values[stage], GL_OUT_OF_MEMORY);
    }

    for (GLsizei i = 0; i < count; i++)
    {
        GLuint element = 0;
        int u = subroutineUniformAt(si, (GLuint)i, &element);
        char sel[128];
        GLint loc;

        pptr->subroutine_values[stage][i] = indices[i];

        if (u < 0)
            continue;

        snprintf(sel, sizeof(sel), "%s%s", si->uniform_names[u], MGL_SUBROUTINE_SUFFIX);

        loc = mglGetUniformLocation(ctx, pptr->name, sel);

        if (loc >= 0)
        {
            GLint v = (GLint)indices[i];

            programUniformWrite(ctx, pptr, loc + (GLint)element, &v, sizeof(GLint));
        }
    }
}

/* ---------- program interface query ---------- */

GLuint mglGetUniformBlockIndex(GLMContext ctx, GLuint program, const GLchar *uniformBlockName);
void mglGetActiveUniformBlockiv(GLMContext ctx, GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);
GLboolean isShader(GLMContext ctx, GLuint shader);

// the six subroutine interfaces, and the stage each belongs to
static GLenum subroutineStage(GLenum iface, bool *uniforms)
{
    *uniforms = false;

    switch (iface)
    {
        case GL_VERTEX_SUBROUTINE_UNIFORM:          *uniforms = true; // fall through
        case GL_VERTEX_SUBROUTINE:                  return GL_VERTEX_SHADER;
        case GL_TESS_CONTROL_SUBROUTINE_UNIFORM:    *uniforms = true; // fall through
        case GL_TESS_CONTROL_SUBROUTINE:            return GL_TESS_CONTROL_SHADER;
        case GL_TESS_EVALUATION_SUBROUTINE_UNIFORM: *uniforms = true; // fall through
        case GL_TESS_EVALUATION_SUBROUTINE:         return GL_TESS_EVALUATION_SHADER;
        case GL_GEOMETRY_SUBROUTINE_UNIFORM:        *uniforms = true; // fall through
        case GL_GEOMETRY_SUBROUTINE:                return GL_GEOMETRY_SHADER;
        case GL_FRAGMENT_SUBROUTINE_UNIFORM:        *uniforms = true; // fall through
        case GL_FRAGMENT_SUBROUTINE:                return GL_FRAGMENT_SHADER;
        case GL_COMPUTE_SUBROUTINE_UNIFORM:         *uniforms = true; // fall through
        case GL_COMPUTE_SUBROUTINE:                 return GL_COMPUTE_SHADER;
    }

    return 0;
}

static int tableKind(GLenum iface)
{
    switch (iface)
    {
        case GL_UNIFORM:                return MGL_RES_UNIFORM;
        case GL_UNIFORM_BLOCK:          return MGL_RES_UNIFORM_BLOCK;
        case GL_PROGRAM_INPUT:          return MGL_RES_PROGRAM_INPUT;
        case GL_PROGRAM_OUTPUT:         return MGL_RES_PROGRAM_OUTPUT;
        case GL_BUFFER_VARIABLE:        return MGL_RES_BUFFER_VARIABLE;
        case GL_SHADER_STORAGE_BLOCK:   return MGL_RES_STORAGE_BLOCK;
        case GL_ATOMIC_COUNTER_BUFFER:  return MGL_RES_ATOMIC_BUFFER;
    }

    return -1;
}

static bool knownInterface(GLenum iface)
{
    bool u;

    return tableKind(iface) >= 0 || subroutineStage(iface, &u) != 0 ||
           iface == GL_TRANSFORM_FEEDBACK_VARYING || iface == GL_TRANSFORM_FEEDBACK_BUFFER;
}

// One resource as the queries see it, whatever list it came from.
typedef struct {
    const char  *name;
    char         name_buf[256];
    MglResource  res;
    GLint        compatible;        // subroutine uniforms
    GLint        xfb_buffer;        // transform feedback varyings
    GLint        xfb_stride;        // transform feedback buffers
    GLint        members[64];       // transform feedback buffers
} PiqItem;

// a shader that lays out its own capture lists its own varyings
static bool xfbFromShader(Program *p)
{
    return p->resources.count[MGL_RES_XFB_BUFFER] > 0;
}

static MglResource *shaderXfbVarying(Program *p, GLint index)
{
    for (GLint v = 0; v < p->resources.count[MGL_RES_CAPTURE_SOURCE]; v++)
    {
        MglResource *r = &p->resources.list[MGL_RES_CAPTURE_SOURCE][v];

        if (r->xfb_offset >= 0 && index-- == 0)
            return r;
    }

    return NULL;
}

static GLint shaderXfbVaryingCount(Program *p)
{
    GLint n = 0;

    while (shaderXfbVarying(p, n))
        n++;

    return n;
}

// the recorded buffers the API varyings fill: one, one each, or one more per
// gl_NextBuffer
static GLint apiXfbBufferCount(Program *p)
{
    GLint n = p->xfb_varying_count > 0 ? 1 : 0;

    for (GLint v = 1; v < p->xfb_varying_count; v++)
        if (p->xfb_buffer_mode == GL_SEPARATE_ATTRIBS || !strcmp(p->xfb_varyings[v - 1], "gl_NextBuffer"))
            n++;

    return n;
}

static bool xfbSpecial(const char *name, GLint *skip)
{
    if (!strcmp(name, "gl_NextBuffer"))
    {
        *skip = 0;
        return true;
    }

    if (!strncmp(name, "gl_SkipComponents", 17) && name[17] >= '1' && name[17] <= '4' && !name[18])
    {
        *skip = name[17] - '0';
        return true;
    }

    return false;
}

static GLint glTypeComponents(GLenum type)
{
    switch (type)
    {
        case GL_FLOAT: case GL_INT: case GL_UNSIGNED_INT: case GL_BOOL: return 1;
        case GL_FLOAT_VEC2: case GL_INT_VEC2: case GL_UNSIGNED_INT_VEC2: case GL_BOOL_VEC2: return 2;
        case GL_FLOAT_VEC3: case GL_INT_VEC3: case GL_UNSIGNED_INT_VEC3: case GL_BOOL_VEC3: return 3;
        case GL_FLOAT_VEC4: case GL_INT_VEC4: case GL_UNSIGNED_INT_VEC4: case GL_BOOL_VEC4: return 4;
        case GL_FLOAT_MAT2: return 4;
        case GL_FLOAT_MAT3: return 9;
        case GL_FLOAT_MAT4: return 16;
        case GL_FLOAT_MAT2x3: case GL_FLOAT_MAT3x2: return 6;
        case GL_FLOAT_MAT2x4: case GL_FLOAT_MAT4x2: return 8;
        case GL_FLOAT_MAT3x4: case GL_FLOAT_MAT4x3: return 12;
        case GL_DOUBLE: return 2;
        case GL_DOUBLE_VEC2: return 4;
        case GL_DOUBLE_VEC3: return 6;
        case GL_DOUBLE_VEC4: return 8;
    }

    return 1;
}

// "name[3]" -> base "name", element 3. GL allows only plain decimal inside
// the brackets: no spaces, no sign, no leading zero.
static bool splitElement(const char *name, char *base, size_t base_len, GLint *element)
{
    size_t n = strlen(name);

    *element = -1;

    if (n < 4 || name[n - 1] != ']')
    {
        snprintf(base, base_len, "%s", name);
        return true;
    }

    const char *open = strrchr(name, '[');

    if (open == NULL || open[1] == ']')
        return false;

    for (const char *d = open + 1; d < name + n - 1; d++)
        if (*d < '0' || *d > '9')
            return false;

    if (open[1] == '0' && open + 2 != name + n - 1)
        return false;

    *element = atoi(open + 1);
    snprintf(base, base_len, "%.*s", (int)(open - name), name);

    return true;
}

static GLint piqCount(Program *p, GLenum iface)
{
    int kind = tableKind(iface);
    bool u;
    GLenum stage = subroutineStage(iface, &u);

    if (p->link_status != GL_TRUE)
        return 0;

    if (kind >= 0)
        return p->resources.count[kind];

    if (stage)
    {
        SubroutineInfo *si = stageSubroutines(p, stage);

        if (si == NULL)
            return 0;

        return (GLint)(u ? si->uniform_count : si->fn_count);
    }

    if (iface == GL_TRANSFORM_FEEDBACK_VARYING)
        return xfbFromShader(p) ? shaderXfbVaryingCount(p) : p->xfb_varying_count;

    if (iface == GL_TRANSFORM_FEEDBACK_BUFFER)
        return xfbFromShader(p) ? p->resources.count[MGL_RES_XFB_BUFFER] : apiXfbBufferCount(p);

    return 0;
}

// Fills *it with resource number index of the interface. False if there is none.
static bool piqItem(Program *p, GLenum iface, GLint index, PiqItem *it)
{
    int kind = tableKind(iface);
    bool u;
    GLenum stage = subroutineStage(iface, &u);

    memset(it, 0, sizeof(*it));
    it->res.location = -1;
    it->res.block_index = -1;
    it->res.atomic_buffer = -1;

    if (index < 0 || index >= piqCount(p, iface))
        return false;

    if (kind >= 0)
    {
        it->res = p->resources.list[kind][index];
        it->name = it->res.name ? it->res.name : "";
        return true;
    }

    if (stage)
    {
        SubroutineInfo *si = stageSubroutines(p, stage);

        if (!u)
        {
            it->name = si->fn_names[index];
            return true;
        }

        GLint size = si->uniform_array_size[index] ? (GLint)si->uniform_array_size[index] : 1;

        if (size > 1)
        {
            snprintf(it->name_buf, sizeof(it->name_buf), "%s[0]", si->uniform_names[index]);
            it->name = it->name_buf;
        }
        else
        {
            it->name = si->uniform_names[index];
        }

        it->res.array_size = size;
        it->compatible = (GLint)si->uniform_compatible[index];
        return true;
    }

    if (iface == GL_TRANSFORM_FEEDBACK_BUFFER)
    {
        GLint vars = piqCount(p, GL_TRANSFORM_FEEDBACK_VARYING);
        GLint number = xfbFromShader(p) ? p->resources.list[MGL_RES_XFB_BUFFER][index].binding : index;
        GLint end = 0;

        it->name = "";
        it->res.binding = number;

        for (GLint v = 0; v < vars; v++)
        {
            PiqItem var;

            if (!piqItem(p, GL_TRANSFORM_FEEDBACK_VARYING, v, &var) || var.xfb_buffer != index)
                continue;

            if (it->res.num_active < 64 && var.res.type != GL_NONE)
                it->members[it->res.num_active++] = v;

            if (var.res.offset >= 0)
            {
                GLint size = var.res.type == GL_NONE ? 0
                           : 4 * glTypeComponents(var.res.type) * (var.res.array_size > 0 ? var.res.array_size : 1);

                if (var.res.offset + size > end)
                    end = var.res.offset + size;
            }
        }

        it->xfb_stride = xfbFromShader(p) && number < MGL_XFB_MAX_BUFFERS
                       ? p->xfb_shader_strides[number] : end;
        return true;
    }

    if (xfbFromShader(p))
    {
        MglResource *r = shaderXfbVarying(p, index);

        it->res = *r;
        it->name = r->name;
        it->res.offset = r->xfb_offset;
        it->xfb_buffer = 0;

        for (GLint b = 0; b < p->resources.count[MGL_RES_XFB_BUFFER]; b++)
            if (p->resources.list[MGL_RES_XFB_BUFFER][b].binding == (r->xfb_buffer >= 0 ? r->xfb_buffer : 0))
                it->xfb_buffer = b;

        return true;
    }

    // transform feedback: the names exactly as recorded, typed from what the
    // capturing stage writes
    {
        const char *name = p->xfb_varyings[index];
        GLint skip = 0;
        GLint buffer = 0, offset = 0;

        it->name = name;

        for (GLint i = 0; i <= index; i++)
        {
            const char *vn = p->xfb_varyings[i];
            GLint vskip = 0, vsize = 1;
            GLenum vtype = GL_NONE;

            if (xfbSpecial(vn, &vskip))
            {
                if (!strcmp(vn, "gl_NextBuffer"))
                {
                    if (i < index) { buffer++; offset = 0; }
                    continue;
                }

                if (i < index)
                    offset += 4 * vskip;
                continue;
            }

            if (p->xfb_buffer_mode == GL_SEPARATE_ATTRIBS && i > 0)
            {
                buffer = i;
                offset = 0;
            }

            char base[256];
            GLint element;
            MglResource *src = NULL;

            if (splitElement(vn, base, sizeof(base), &element))
            {
                for (GLint r = 0; r < p->resources.count[MGL_RES_CAPTURE_SOURCE]; r++)
                {
                    MglResource *cand = &p->resources.list[MGL_RES_CAPTURE_SOURCE][r];
                    char want[300];

                    snprintf(want, sizeof(want), "%s[0]", element >= 0 ? base : vn);

                    if (!strcmp(cand->name, vn) || !strcmp(cand->name, want))
                    {
                        src = cand;
                        break;
                    }
                }
            }

            if (src)
            {
                vtype = src->type;
                vsize = element >= 0 ? 1 : (src->array_size > 0 ? src->array_size : 1);
            }

            if (i == index)
            {
                it->res.type = vtype;
                it->res.array_size = vsize;
                it->res.offset = offset;
                it->xfb_buffer = buffer;
            }
            else
            {
                offset += 4 * glTypeComponents(vtype) * vsize;
            }
        }

        if (xfbSpecial(name, &skip))
        {
            it->res.type = GL_NONE;
            it->res.array_size = skip;
            it->res.offset = -1;
            it->xfb_buffer = -1;
        }

        return true;
    }
}

// The resource a name picks out. "a" finds "a[0]", and a block array's plain
// name finds its first element.
static GLint piqFind(Program *p, GLenum iface, const char *name)
{
    GLint count = piqCount(p, iface);
    char with_zero[300];
    GLint skip;

    if (iface == GL_TRANSFORM_FEEDBACK_VARYING && xfbSpecial(name, &skip))
        return -1;

    snprintf(with_zero, sizeof(with_zero), "%s[0]", name);

    for (GLint pass = 0; pass < 2; pass++)
    {
        for (GLint i = 0; i < count; i++)
        {
            PiqItem it;

            if (!piqItem(p, iface, i, &it))
                continue;

            if (!strcmp(it.name, pass ? with_zero : name))
                return i;
        }
    }

    return -1;
}

// resolve a program name the way every query here must
static Program *piqProgram(GLMContext ctx, GLuint program)
{
    Program *p = findProgram(ctx, program);

    if (p == NULL)
    {
        if (program && isShader(ctx, program))
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);

        ERROR_RETURN_VALUE(GL_INVALID_VALUE, NULL);
    }

    return p;
}

void mglGetProgramInterfaceiv(GLMContext ctx, GLuint program, GLenum programInterface, GLenum pname, GLint *params)
{
    Program *p = piqProgram(ctx, program);
    bool u;
    GLenum stage = subroutineStage(programInterface, &u);
    bool has_members = programInterface == GL_UNIFORM_BLOCK || programInterface == GL_SHADER_STORAGE_BLOCK ||
                       programInterface == GL_ATOMIC_COUNTER_BUFFER ||
                       programInterface == GL_TRANSFORM_FEEDBACK_BUFFER;
    bool nameless = programInterface == GL_ATOMIC_COUNTER_BUFFER ||
                    programInterface == GL_TRANSFORM_FEEDBACK_BUFFER;

    if (p == NULL)
        return;

    ERROR_CHECK_RETURN(knownInterface(programInterface), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    GLint count = piqCount(p, programInterface);

    switch (pname)
    {
        case GL_ACTIVE_RESOURCES:
            *params = count;
            break;

        case GL_MAX_NAME_LENGTH:
        {
            ERROR_CHECK_RETURN(!nameless, GL_INVALID_OPERATION);

            GLint longest = 0;

            for (GLint i = 0; i < count; i++)
            {
                PiqItem it;

                if (piqItem(p, programInterface, i, &it) && (GLint)strlen(it.name) + 1 > longest)
                    longest = (GLint)strlen(it.name) + 1;
            }

            *params = longest;
            break;
        }

        case GL_MAX_NUM_ACTIVE_VARIABLES:
        {
            ERROR_CHECK_RETURN(has_members, GL_INVALID_OPERATION);

            GLint most = 0;

            for (GLint i = 0; i < count; i++)
            {
                PiqItem it;

                if (piqItem(p, programInterface, i, &it) && it.res.num_active > most)
                    most = it.res.num_active;
            }

            *params = most;
            break;
        }

        case GL_MAX_NUM_COMPATIBLE_SUBROUTINES:
        {
            ERROR_CHECK_RETURN(stage && u, GL_INVALID_OPERATION);

            GLint most = 0;

            for (GLint i = 0; i < count; i++)
            {
                PiqItem it;

                if (piqItem(p, programInterface, i, &it) && it.compatible > most)
                    most = it.compatible;
            }

            *params = most;
            break;
        }

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

GLuint mglGetProgramResourceIndex(GLMContext ctx, GLuint program, GLenum programInterface, const GLchar *name)
{
    Program *p = piqProgram(ctx, program);

    if (p == NULL)
        return GL_INVALID_INDEX;

    ERROR_CHECK_RETURN_VALUE(knownInterface(programInterface) &&
                             programInterface != GL_ATOMIC_COUNTER_BUFFER &&
                             programInterface != GL_TRANSFORM_FEEDBACK_BUFFER,
                             GL_INVALID_ENUM, GL_INVALID_INDEX);

    if (name == NULL || name[0] == 0)
        return GL_INVALID_INDEX;

    GLint idx = piqFind(p, programInterface, name);

    return idx < 0 ? GL_INVALID_INDEX : (GLuint)idx;
}

void mglGetProgramResourceName(GLMContext ctx, GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    Program *p = piqProgram(ctx, program);
    PiqItem it;

    if (p == NULL)
        return;

    ERROR_CHECK_RETURN(knownInterface(programInterface) &&
                       programInterface != GL_ATOMIC_COUNTER_BUFFER &&
                       programInterface != GL_TRANSFORM_FEEDBACK_BUFFER, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(piqItem(p, programInterface, (GLint)index, &it), GL_INVALID_VALUE);

    if (name == NULL || bufSize == 0)
    {
        if (length)
            *length = 0;

        return;
    }

    copyResourceName(it.name, bufSize, length, name);
}

// which interfaces a property belongs to
static bool piqPropertyValid(GLenum iface, GLenum prop, bool *known)
{
    bool u;
    bool sub = subroutineStage(iface, &u) != 0;
    bool uni = iface == GL_UNIFORM, bvar = iface == GL_BUFFER_VARIABLE;
    bool io = iface == GL_PROGRAM_INPUT || iface == GL_PROGRAM_OUTPUT;
    bool blk = iface == GL_UNIFORM_BLOCK || iface == GL_SHADER_STORAGE_BLOCK;
    bool acb = iface == GL_ATOMIC_COUNTER_BUFFER, xfbv = iface == GL_TRANSFORM_FEEDBACK_VARYING;
    bool xfbb = iface == GL_TRANSFORM_FEEDBACK_BUFFER;

    *known = true;

    switch (prop)
    {
        case GL_NAME_LENGTH:                return !acb && !xfbb;
        case GL_TYPE:                       return uni || bvar || io || xfbv;
        case GL_ARRAY_SIZE:                 return uni || bvar || io || xfbv || (sub && u);
        case GL_OFFSET:                     return uni || bvar || xfbv;
        case GL_BLOCK_INDEX:
        case GL_ARRAY_STRIDE:
        case GL_MATRIX_STRIDE:
        case GL_IS_ROW_MAJOR:               return uni || bvar;
        case GL_ATOMIC_COUNTER_BUFFER_INDEX: return uni;
        case GL_BUFFER_BINDING:
        case GL_NUM_ACTIVE_VARIABLES:
        case GL_ACTIVE_VARIABLES:           return blk || acb || xfbb;
        case GL_BUFFER_DATA_SIZE:           return blk || acb;
        case GL_NUM_COMPATIBLE_SUBROUTINES:
        case GL_COMPATIBLE_SUBROUTINES:     return sub && u;
        case GL_REFERENCED_BY_VERTEX_SHADER:
        case GL_REFERENCED_BY_TESS_CONTROL_SHADER:
        case GL_REFERENCED_BY_TESS_EVALUATION_SHADER:
        case GL_REFERENCED_BY_GEOMETRY_SHADER:
        case GL_REFERENCED_BY_FRAGMENT_SHADER:
        case GL_REFERENCED_BY_COMPUTE_SHADER: return uni || bvar || io || blk || acb;
        case GL_TOP_LEVEL_ARRAY_SIZE:
        case GL_TOP_LEVEL_ARRAY_STRIDE:     return bvar;
        case GL_LOCATION:                   return uni || io || (sub && u);
        case GL_LOCATION_INDEX:             return iface == GL_PROGRAM_OUTPUT;
        case GL_IS_PER_PATCH:
        case GL_LOCATION_COMPONENT:         return io;
        case GL_TRANSFORM_FEEDBACK_BUFFER_INDEX: return xfbv;
        case GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE: return xfbb;
    }

    *known = false;
    return false;
}

static GLint piqStageUses(const PiqItem *it, int stage)
{
    return (it->res.stages >> stage) & 1u;
}

GLint programResourceLocation(GLMContext ctx, GLuint program, GLenum programInterface, const GLchar *name);

void mglGetProgramResourceiv(GLMContext ctx, GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei count, GLsizei *length, GLint *params)
{
    Program *p = piqProgram(ctx, program);
    PiqItem it;
    GLsizei written = 0;
    bool u;
    GLenum sub_stage = subroutineStage(programInterface, &u);

    if (p == NULL)
        return;

    ERROR_CHECK_RETURN(knownInterface(programInterface), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(propCount > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(props, GL_INVALID_VALUE);

    // an unknown property is reported ahead of a bad index
    for (GLsizei i = 0; i < propCount; i++)
    {
        bool known;

        piqPropertyValid(programInterface, props[i], &known);
        ERROR_CHECK_RETURN(known, GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(piqItem(p, programInterface, (GLint)index, &it), GL_INVALID_VALUE);

    for (GLsizei i = 0; i < propCount; i++)
    {
        bool known;

        ERROR_CHECK_RETURN(piqPropertyValid(programInterface, props[i], &known), GL_INVALID_OPERATION);
    }

    for (GLsizei i = 0; i < propCount && written < count; i++)
    {
        GLint v = 0;

        switch (props[i])
        {
            case GL_NAME_LENGTH:
                v = (GLint)strlen(it.name) + 1;
                break;

            case GL_TYPE:
                v = (GLint)it.res.type;
                break;

            case GL_ARRAY_SIZE:
                v = it.res.array_size;
                break;

            case GL_OFFSET:
                v = it.res.offset;
                break;

            case GL_BLOCK_INDEX:
                v = it.res.block_index;
                break;

            case GL_ARRAY_STRIDE:
                v = it.res.array_stride;
                break;

            case GL_MATRIX_STRIDE:
                v = it.res.matrix_stride;
                break;

            case GL_IS_ROW_MAJOR:
                v = it.res.row_major;
                break;

            case GL_ATOMIC_COUNTER_BUFFER_INDEX:
                v = it.res.atomic_buffer;
                break;

            case GL_BUFFER_BINDING:
                v = it.res.binding >= 0 ? it.res.binding : 0;

                // a uniform block's binding can change after the link
                if (programInterface == GL_UNIFORM_BLOCK)
                {
                    GLuint b = mglGetUniformBlockIndex(ctx, program, it.name);

                    if (b != GL_INVALID_INDEX)
                        mglGetActiveUniformBlockiv(ctx, program, b, GL_UNIFORM_BLOCK_BINDING, &v);
                }
                break;

            case GL_BUFFER_DATA_SIZE:
                v = it.res.data_size;

                if (programInterface == GL_UNIFORM_BLOCK)
                {
                    GLuint b = mglGetUniformBlockIndex(ctx, program, it.name);

                    if (b != GL_INVALID_INDEX)
                        mglGetActiveUniformBlockiv(ctx, program, b, GL_UNIFORM_BLOCK_DATA_SIZE, &v);
                }
                break;

            case GL_NUM_ACTIVE_VARIABLES:
                v = it.res.num_active;
                break;

            case GL_ACTIVE_VARIABLES:
                for (GLint m = 0; m < it.res.num_active && written < count; m++)
                    params[written++] = programInterface == GL_TRANSFORM_FEEDBACK_BUFFER
                                      ? it.members[m] : it.res.active[m];
                continue;

            case GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE:
                v = it.xfb_stride;
                break;

            case GL_NUM_COMPATIBLE_SUBROUTINES:
                v = it.compatible;
                break;

            case GL_COMPATIBLE_SUBROUTINES:
            {
                SubroutineInfo *si = stageSubroutines(p, sub_stage);

                for (GLuint f = 0; si && f < si->fn_count && written < count; f++)
                    if (mglSubroutineCompatible(si, index, f))
                        params[written++] = (GLint)f;
                continue;
            }

            case GL_REFERENCED_BY_VERTEX_SHADER:          v = piqStageUses(&it, _VERTEX_SHADER); break;
            case GL_REFERENCED_BY_TESS_CONTROL_SHADER:    v = piqStageUses(&it, _TESS_CONTROL_SHADER); break;
            case GL_REFERENCED_BY_TESS_EVALUATION_SHADER: v = piqStageUses(&it, _TESS_EVALUATION_SHADER); break;
            case GL_REFERENCED_BY_GEOMETRY_SHADER:        v = piqStageUses(&it, _GEOMETRY_SHADER); break;
            case GL_REFERENCED_BY_FRAGMENT_SHADER:        v = piqStageUses(&it, _FRAGMENT_SHADER); break;
            case GL_REFERENCED_BY_COMPUTE_SHADER:         v = piqStageUses(&it, _COMPUTE_SHADER); break;

            case GL_TOP_LEVEL_ARRAY_SIZE:
                v = it.res.top_level_size;
                break;

            case GL_TOP_LEVEL_ARRAY_STRIDE:
                v = it.res.top_level_stride;
                break;

            case GL_LOCATION:
                v = programResourceLocation(ctx, program, programInterface, it.name);
                break;

            case GL_LOCATION_INDEX:
            {
                // only a fragment output has a colour index
                bool fragment = (it.res.stages >> _FRAGMENT_SHADER) & 1u;

                v = (!fragment || !strncmp(it.name, "gl_", 3)) ? -1 : it.res.location_index;
                break;
            }

            case GL_IS_PER_PATCH:
                v = it.res.per_patch;
                break;

            case GL_LOCATION_COMPONENT:
                v = it.res.component;
                break;

            case GL_TRANSFORM_FEEDBACK_BUFFER_INDEX:
                v = it.xfb_buffer;
                break;

            default:
                v = 0;
                break;
        }

        params[written++] = v;
    }

    if (length)
        *length = written;
}

GLint mglGetAttribLocation(GLMContext ctx, GLuint program, const GLchar *name);
GLint mglGetFragDataLocation(GLMContext ctx, GLuint program, const GLchar *name);

GLint programResourceLocation(GLMContext ctx, GLuint program, GLenum programInterface, const GLchar *name)
{
    Program *p = piqProgram(ctx, program);
    bool u;
    GLenum stage = subroutineStage(programInterface, &u);
    char base[256];
    GLint element;

    if (p == NULL)
        return -1;

    ERROR_CHECK_RETURN_VALUE(programInterface == GL_UNIFORM || programInterface == GL_PROGRAM_INPUT ||
                             programInterface == GL_PROGRAM_OUTPUT || (stage && u),
                             GL_INVALID_ENUM, -1);
    ERROR_CHECK_RETURN_VALUE(name, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(p->link_status == GL_TRUE, GL_INVALID_OPERATION, -1);

    if (!splitElement(name, base, sizeof(base), &element) || !strncmp(name, "gl_", 3))
        return -1;

    // the whole name, or an element of an array named with [0]
    GLint idx = piqFind(p, programInterface, name);
    PiqItem it;

    if (idx < 0 && element >= 0)
        idx = piqFind(p, programInterface, base);

    if (idx < 0 || !piqItem(p, programInterface, idx, &it))
        return -1;

    GLint at = 0;
    size_t nlen = strlen(it.name);
    bool array_name = nlen > 3 && !strcmp(it.name + nlen - 3, "[0]");

    if (element >= 0 && strcmp(it.name, name))
    {
        if (!array_name || element >= it.res.array_size)
            return -1;

        at = element;
    }

    if (programInterface == GL_UNIFORM)
    {
        if (it.res.block_index >= 0 || it.res.type == GL_UNSIGNED_INT_ATOMIC_COUNTER)
            return -1;

        return mglGetUniformLocation(ctx, program, name);
    }

    if (stage)
    {
        char whole[300];

        snprintf(whole, sizeof(whole), "%s", it.name);

        if (array_name)
            whole[nlen - 3] = 0;

        GLint loc = mglGetSubroutineUniformLocation(ctx, program, stage, whole);

        return loc < 0 ? -1 : loc + at;
    }

    // inputs and outputs: a bound or declared location, per element
    {
        char whole[300];
        GLint loc;

        snprintf(whole, sizeof(whole), "%s", it.name);

        if (array_name)
            whole[nlen - 3] = 0;

        if (programInterface == GL_PROGRAM_INPUT && ((it.res.stages >> _VERTEX_SHADER) & 1u))
            loc = mglGetAttribLocation(ctx, program, whole);
        else if (programInterface == GL_PROGRAM_OUTPUT && ((it.res.stages >> _FRAGMENT_SHADER) & 1u))
            loc = mglGetFragDataLocation(ctx, program, whole);
        else
            loc = it.res.location;

        if (loc < 0)
            loc = it.res.location;

        return loc < 0 ? -1 : loc + at;
    }
}

// The fragment colour index, which only fragment outputs have.
GLint programResourceLocationIndex(GLMContext ctx, GLuint program, GLenum programInterface, const GLchar *name)
{
    Program *p = piqProgram(ctx, program);

    if (p == NULL)
        return -1;

    ERROR_CHECK_RETURN_VALUE(programInterface == GL_PROGRAM_OUTPUT, GL_INVALID_ENUM, -1);
    ERROR_CHECK_RETURN_VALUE(name, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(p->link_status == GL_TRUE, GL_INVALID_OPERATION, -1);

    if (programResourceLocation(ctx, program, GL_PROGRAM_OUTPUT, name) < 0)
        return -1;

    char base[256];
    GLint element;
    GLint idx = piqFind(p, GL_PROGRAM_OUTPUT, name);
    PiqItem it;

    if (idx < 0 && splitElement(name, base, sizeof(base), &element))
        idx = piqFind(p, GL_PROGRAM_OUTPUT, base);

    if (idx < 0 || !piqItem(p, GL_PROGRAM_OUTPUT, idx, &it))
        return -1;

    if (!((it.res.stages >> _FRAGMENT_SHADER) & 1u))
        return -1;

    return it.res.location_index;
}

void mglGetProgramStageiv(GLMContext ctx, GLuint program, GLenum shadertype, GLenum pname, GLint *params)
{
    Program *ptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    switch (shadertype)
    {
        case GL_VERTEX_SHADER:
        case GL_FRAGMENT_SHADER:
        case GL_GEOMETRY_SHADER:
        case GL_TESS_CONTROL_SHADER:
        case GL_TESS_EVALUATION_SHADER:
        case GL_COMPUTE_SHADER:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    SubroutineInfo *si = stageSubroutines(ptr, shadertype);

    switch (pname)
    {
        case GL_ACTIVE_SUBROUTINES:
            *params = si ? (GLint)si->fn_count : 0;
            break;

        case GL_ACTIVE_SUBROUTINE_UNIFORMS:
            *params = si ? (GLint)si->uniform_count : 0;
            break;

        case GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS:
            *params = si ? (GLint)subroutineLocationCount(si) : 0;
            break;

        case GL_ACTIVE_SUBROUTINE_MAX_LENGTH:
        {
            GLint m = 0;

            for (GLuint i = 0; si && i < si->fn_count; i++)
            {
                GLint n = (GLint)strlen(si->fn_names[i]) + 1;

                if (n > m) m = n;
            }

            *params = m;
        } break;

        case GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH:
        {
            GLint m = 0;

            for (GLuint i = 0; si && i < si->uniform_count; i++)
            {
                GLint n = (GLint)strlen(si->uniform_names[i]) + 1;

                if (n > m) m = n;
            }

            *params = m;
        } break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

GLint mglGetFragDataLocation(GLMContext ctx, GLuint program, const GLchar *name)
{
    Program *ptr = findProgram(ctx, program);
    SpirvResourceList *l;

    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(name, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(ptr->link_status == GL_TRUE, GL_INVALID_OPERATION, -1);

    l = &ptr->spirv_resources_list[_FRAGMENT_SHADER][SPVC_RESOURCE_TYPE_STAGE_OUTPUT];

    for (GLuint i = 0; i < l->count; i++)
        if (l->list[i].name && strcmp(l->list[i].name, name) == 0)
            return (GLint)l->list[i].location;

    return -1;
}

GLint mglGetFragDataIndex(GLMContext ctx, GLuint program, const GLchar *name)
{
    Program *ptr = findProgram(ctx, program);

    if (mglGetFragDataLocation(ctx, program, name) < 0)
        return -1;

    // declared in the shader, or bound before the link
    Shader *fs = ptr->shader_slots[_FRAGMENT_SHADER];
    GLint declared = (fs && fs->src) ? explicitLayout(fs->src, name, "index", "out") : -1;

    if (declared >= 0)
        return declared;

    for (GLint b = 0; b < ptr->frag_bind_count; b++)
        if (!strcmp(ptr->frag_binds[b].name, name))
            return (GLint)ptr->frag_binds[b].index;

    return 0;
}

void mglBindFragDataLocationIndexed(GLMContext ctx, GLuint program, GLuint colorNumber, GLuint index, const GLchar *name);

void mglBindFragDataLocation(GLMContext ctx, GLuint program, GLuint color, const GLchar *name)
{
    mglBindFragDataLocationIndexed(ctx, program, color, 0, name);
}

void mglBindFragDataLocationIndexed(GLMContext ctx, GLuint program, GLuint colorNumber, GLuint index, const GLchar *name)
{
    Program *ptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(name, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(colorNumber < ctx->state.max_color_attachments, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(index <= 1, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(strncmp(name, "gl_", 3) != 0, GL_INVALID_OPERATION);

    recordBinding(ptr->frag_binds, &ptr->frag_bind_count, name, colorNumber, index);
}

/* ---------- validation and pipeline queries ---------- */

void mglReleaseShaderCompiler(GLMContext ctx)
{
    // glslang is always linked in, so there is nothing to release
}

void mglValidateProgram(GLMContext ctx, GLuint program)
{
    Program *pptr = findProgram(ctx, program);
    bool stage_present = false;

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);

    for (int i = 0; i < _MAX_SHADER_TYPES; i++)
        if (pptr->spirv[i].msl_str)
            stage_present = true;

    if (pptr->link_status == GL_TRUE && stage_present)
    {
        pptr->validate_status = GL_TRUE;

        return;
    }

    pptr->validate_status = GL_FALSE;

    free(pptr->log);
    pptr->log = strdup("validation failed: no linked shader stage");
}

const char *mglPipelineProblem(const ProgramPipeline *pp);

void mglValidateProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    ProgramPipeline *pp = findProgramPipeline(ctx, pipeline);

    ERROR_CHECK_RETURN(pp, GL_INVALID_OPERATION);

    pp->created = GL_TRUE;
    pp->validated = mglPipelineProblem(pp) == NULL ? GL_TRUE : GL_FALSE;
}

static GLint pipelineStageName(ProgramPipeline *pp, int stage)
{
    return pp->stage_programs[stage] ? (GLint)pp->stage_programs[stage]->name : 0;
}

void mglGetProgramPipelineiv(GLMContext ctx, GLuint pipeline, GLenum pname, GLint *params)
{
    ProgramPipeline *pp = findProgramPipeline(ctx, pipeline);

    ERROR_CHECK_RETURN(pp, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    switch (pname)
    {
        case GL_VALIDATE_STATUS:
            *params = pp->validated;
            break;

        case GL_ACTIVE_PROGRAM:
            *params = pp->active ? (GLint)pp->active->name : 0;
            break;

        case GL_VERTEX_SHADER:
            *params = pipelineStageName(pp, _VERTEX_SHADER);
            break;

        case GL_FRAGMENT_SHADER:
            *params = pipelineStageName(pp, _FRAGMENT_SHADER);
            break;

        case GL_GEOMETRY_SHADER:
            *params = pipelineStageName(pp, _GEOMETRY_SHADER);
            break;

        case GL_TESS_CONTROL_SHADER:
            *params = pipelineStageName(pp, _TESS_CONTROL_SHADER);
            break;

        case GL_TESS_EVALUATION_SHADER:
            *params = pipelineStageName(pp, _TESS_EVALUATION_SHADER);
            break;

        case GL_COMPUTE_SHADER:
            *params = pipelineStageName(pp, _COMPUTE_SHADER);
            break;

        case GL_INFO_LOG_LENGTH:
            *params = 0;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglActiveShaderProgram(GLMContext ctx, GLuint pipeline, GLuint program)
{
    ProgramPipeline *pp = findProgramPipeline(ctx, pipeline);
    Program *pptr;

    ERROR_CHECK_RETURN(pp, GL_INVALID_OPERATION);

    if (!pipelineStageProgram(ctx, program, false, &pptr))
        return;

    pp->created = GL_TRUE;
    pp->active = pptr;
}

void mglShaderStorageBlockBinding(GLMContext ctx, GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(storageBlockIndex < (GLuint)pptr->resources.count[MGL_RES_STORAGE_BLOCK], GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(storageBlockBinding < MAX_SHADER_STORAGE_BUFFER_BINDINGS, GL_INVALID_VALUE);

    MglResource *blk = &pptr->resources.list[MGL_RES_STORAGE_BLOCK][storageBlockIndex];
    char base[256];
    GLint element = -1;
    const char *open = strrchr(blk->name, '[');

    blk->binding = (GLint)storageBlockBinding;

    if (open)
    {
        element = atoi(open + 1);
        snprintf(base, sizeof(base), "%.*s", (int)(open - blk->name), blk->name);
    }
    else
    {
        snprintf(base, sizeof(base), "%s", blk->name);
    }

    // the stages' own lists are what the draw binds from
    for (int stage = 0; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *l = &pptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_STORAGE_BUFFER];

        for (GLuint i = 0; i < l->count; i++)
        {
            SpirvResource *r = &l->list[i];

            if (r->name == NULL || strcmp(r->name, base))
                continue;

            if (element >= 0 && r->element_binding && element < r->array_size)
                r->element_binding[element] = storageBlockBinding;
            else
                r->binding = storageBlockBinding;
        }
    }

    pptr->dirty_bits |= DIRTY_PROGRAM;
}

void mglGetActiveAtomicCounterBufferiv(GLMContext ctx, GLuint program, GLuint bufferIndex, GLenum pname, GLint *params)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufferIndex < (GLuint)pptr->resources.count[MGL_RES_ATOMIC_BUFFER], GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    MglResource *b = &pptr->resources.list[MGL_RES_ATOMIC_BUFFER][bufferIndex];

    switch (pname)
    {
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:      *params = b->binding; break;
        case GL_ATOMIC_COUNTER_BUFFER_DATA_SIZE:    *params = b->data_size; break;
        case GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTERS: *params = b->num_active; break;

        case GL_ATOMIC_COUNTER_BUFFER_ACTIVE_ATOMIC_COUNTER_INDICES:
            for (GLint i = 0; i < b->num_active; i++)
                params[i] = b->active[i];
            break;

        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_VERTEX_SHADER:
            *params = (b->stages >> _VERTEX_SHADER) & 1; break;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_CONTROL_SHADER:
            *params = (b->stages >> _TESS_CONTROL_SHADER) & 1; break;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_TESS_EVALUATION_SHADER:
            *params = (b->stages >> _TESS_EVALUATION_SHADER) & 1; break;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_GEOMETRY_SHADER:
            *params = (b->stages >> _GEOMETRY_SHADER) & 1; break;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_FRAGMENT_SHADER:
            *params = (b->stages >> _FRAGMENT_SHADER) & 1; break;
        case GL_ATOMIC_COUNTER_BUFFER_REFERENCED_BY_COMPUTE_SHADER:
            *params = (b->stages >> _COMPUTE_SHADER) & 1; break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

/* ---------- program binaries ---------- */

void mglProgramParameteri(GLMContext ctx, GLuint program, GLenum pname, GLint value)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(value == GL_TRUE || value == GL_FALSE, GL_INVALID_VALUE);

    switch (pname)
    {
        case GL_PROGRAM_SEPARABLE:
            pptr->separable = (GLboolean)value;
            break;

        case GL_PROGRAM_BINARY_RETRIEVABLE_HINT:
            pptr->binary_retrievable_hint = (GLboolean)value;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetProgramBinary(GLMContext ctx, GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    if (length)
        *length = 0;

    // MGL exposes no binary formats, so there is nothing to hand back
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglProgramBinary(GLMContext ctx, GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(length >= 0, GL_INVALID_VALUE);

    // the spec wants the program marked unlinked before the error
    pptr->link_status = GL_FALSE;

    ERROR_RETURN(GL_INVALID_ENUM);
}
