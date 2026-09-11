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
#include "shaders.h"
#include "buffers.h"
#include "mgl_log.h"

// Program Pipeline management
ProgramPipeline *newProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    ProgramPipeline *ptr;

    ptr = (ProgramPipeline *)malloc(sizeof(ProgramPipeline));
    assert(ptr);

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
    assert(ptr);

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
    assert(ptr);

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

    for(int i=0; i<_MAX_SHADER_TYPES; i++)
    {
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
        mglFreeShader(ctx, sptr);
    }
    
    pptr->dirty_bits |= DIRTY_PROGRAM;
}

void error_callback(void *userdata, const char *error)
{
    assert(error);
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

char *parseSPIRVShaderToMetal(GLMContext ctx, Program *ptr, int stage)
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

    spirv = ptr->spirv[stage].ir;
    assert(spirv);
    word_count = ptr->spirv[stage].size;
    assert(spirv);

    // Create context.
    if (spvc_context_create(&context) != SPVC_SUCCESS || context == NULL)
    {
        MGL_ERR("MGL Error: could not create a SPIRV-Cross context\n");

        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, NULL);
    }

    // Set debug callback.
    spvc_context_set_error_callback(context, error_callback, ctx);

    // Parse the SPIR-V.
    parse_res = spvc_context_parse_spirv(context, spirv, word_count, &ir);

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

    //ERROR_CHECK_RETURN_VALUE(spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 4.5) == SPVC_SUCCESS, GL_INVALID_OPERATION, NULL);
    // ERROR_CHECK_RETURN_VALUE(spvc_compiler_install_compiler_options(compiler_msl, options) == SPVC_SUCCESS, GL_INVALID_OPERATION, NULL);
    if (spvc_compiler_install_compiler_options(compiler_msl, options) != SPVC_SUCCESS) {
        MGL_ERR("MGL Error: spvc_compiler_install_compiler_options failed\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, NULL);
    }

    
    // create an entry point for metal based on the shader type and name
    GLuint name;
    char entry_point[128];
    name = ptr->shader_slots[stage]->name;

    SpvExecutionModel model = SpvExecutionModelVertex; // CRITICAL FIX: Initialize with safe default
    switch(stage)
    {
        case _VERTEX_SHADER: model = SpvExecutionModelVertex; break;
        case _TESS_CONTROL_SHADER: model = SpvExecutionModelTessellationControl; break;
        case _TESS_EVALUATION_SHADER: model = SpvExecutionModelTessellationEvaluation; break;
        case _GEOMETRY_SHADER: model = SpvExecutionModelGeometry; break;
        case _FRAGMENT_SHADER: model = SpvExecutionModelFragment; break;
        case _COMPUTE_SHADER: model = SpvExecutionModelGLCompute; break;
        default: // CRITICAL FIX: Handle error gracefully instead of crashing
            MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
            STATE(error) = GL_INVALID_OPERATION;
            return NULL;
    }

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
    ptr->shader_slots[stage]->entry_point = strdup(entry_point);
    ptr->spirv[stage].entry_point = strdup(entry_point);

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
            ptr->spirv_resources_list[stage][res_type].list[i].location = spvc_compiler_get_decoration(compiler_msl, list[i].id, SpvDecorationLocation);
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

            rlist->list[i].msl_index = (idx == (unsigned)-1) ? rlist->list[i].binding : idx;
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

bool linkAndCompileProgramToMetal(GLMContext ctx, Program *pptr, int stage)
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
    assert(glsl_program);
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
    if (!glslang_program_map_io(glsl_program))
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

    // compile SPIRV to Metal
    MGL_INFO("MGL DEBUG: About to parse SPIRV to Metal\n");
    pptr->spirv[stage].msl_str = parseSPIRVShaderToMetal(ctx, pptr, stage);
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

    int stages_linked = 0;

    for (int stage=0; stage<_MAX_SHADER_TYPES; stage++)
    {
        pptr->spirv[stage].msl_str = 0;

        if (pptr->shader_slots[stage])
        {
            if (linkAndCompileProgramToMetal(ctx, pptr, stage) == false)
                pptr->link_status = GL_FALSE;
            else
                stages_linked++;
        }
    }

    // a program with no stage, or one whose stage failed, did not link
    if (stages_linked == 0)
        pptr->link_status = GL_FALSE;

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

    /* Only call mtlBindProgram if Metal functions are initialized */
    if (ctx->mtl_funcs.mtlBindProgram) {
        ctx->mtl_funcs.mtlBindProgram(ctx, pptr);
    } else {
        MGL_INFO("WARNING: Metal functions not initialized, skipping mtlBindProgram\n");
    }

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

void mglBindAttribLocation(GLMContext ctx, GLuint program, GLuint index, const GLchar *name)
{
    // Unimplemented function
    // CRITICAL FIX: Handle error gracefully instead of crashing
        MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
        STATE(error) = GL_INVALID_OPERATION;
}

void mglGetActiveAttrib(GLMContext ctx, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)
{
    // Unimplemented function
    // CRITICAL FIX: Handle error gracefully instead of crashing
        MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
        STATE(error) = GL_INVALID_OPERATION;
}

// GetActiveUniform lives in uniforms.c, next to the reflection helpers

void mglGetAttachedShaders(GLMContext ctx, GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)
{
    // Unimplemented function
    // CRITICAL FIX: Handle error gracefully instead of crashing
        MGL_ERR("MGL ERROR: Critical error in program.c at line %d\n", __LINE__);
        STATE(error) = GL_INVALID_OPERATION;
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
	assert(program);

	if (ptr->linked_glsl_program == NULL)
	{
		ERROR_RETURN_VALUE(GL_INVALID_OPERATION, 0);

		return -1;
	}

	for (int stage=_VERTEX_SHADER; stage<_MAX_SHADER_TYPES; stage++)
	{
		int count;

		count = ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_STAGE_INPUT].count;

		for (int i=0; i<count; i++)
		{
			const char *str = ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_STAGE_INPUT].list[i].name;

			if (!strcmp(str, name))
			{
				GLuint location;

				location = ptr->spirv_resources_list[stage][SPVC_RESOURCE_TYPE_STAGE_INPUT].list[i].location;

				return location;
			}
		}
	}
	
	return -1;
}

static int programResourceCount(Program *ptr, int res_type)
{
    int n = 0;

    for (int stage = _VERTEX_SHADER; stage < _MAX_SHADER_TYPES; stage++)
        n += ptr->spirv_resources_list[stage][res_type].count;

    return n;
}

static int programUniformCount(Program *ptr)
{
    return programResourceCount(ptr, SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT);
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
    return ptr ? GL_TRUE : GL_FALSE;
}

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
        }
        
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
        STATE(dirty_bits) |= DIRTY_PROGRAM;
        return;
    }
    
    ProgramPipeline *ptr = getProgramPipeline(ctx, pipeline);
    STATE(program_pipeline) = ptr;
    STATE(dirty_bits) |= DIRTY_PROGRAM;
}

void mglUseProgramStages(GLMContext ctx, GLuint pipeline, GLbitfield stages, GLuint program)
{
    ProgramPipeline *pipe_ptr = findProgramPipeline(ctx, pipeline);
    if (!pipe_ptr)
    {
        STATE(error) = GL_INVALID_OPERATION;
        return;
    }
    
    Program *prog_ptr = NULL;
    if (program != 0)
    {
        prog_ptr = findProgram(ctx, program);
        if (!prog_ptr)
        {
            STATE(error) = GL_INVALID_VALUE;
            return;
        }
    }
    
    // Attach program to specified stages
    if (stages & GL_VERTEX_SHADER_BIT)
        pipe_ptr->stage_programs[_VERTEX_SHADER] = prog_ptr;
    if (stages & GL_FRAGMENT_SHADER_BIT)
        pipe_ptr->stage_programs[_FRAGMENT_SHADER] = prog_ptr;
    if (stages & GL_GEOMETRY_SHADER_BIT)
        pipe_ptr->stage_programs[_GEOMETRY_SHADER] = prog_ptr;
    if (stages & GL_TESS_CONTROL_SHADER_BIT)
        pipe_ptr->stage_programs[_TESS_CONTROL_SHADER] = prog_ptr;
    if (stages & GL_TESS_EVALUATION_SHADER_BIT)
        pipe_ptr->stage_programs[_TESS_EVALUATION_SHADER] = prog_ptr;
    if (stages & GL_COMPUTE_SHADER_BIT)
        pipe_ptr->stage_programs[_COMPUTE_SHADER] = prog_ptr;
        
    pipe_ptr->validated = GL_FALSE;
    STATE(dirty_bits) |= DIRTY_PROGRAM;
}

/* ---------- subroutines ---------- */

// MGL's linker captures no subroutines, so every program has zero of them.
// These answer as a program with none would, rather than pretending to work.

void mglGetActiveSubroutineUniformiv(GLMContext ctx, GLuint program, GLenum shadertype, GLuint index, GLenum pname, GLint *values)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(values, GL_INVALID_VALUE);

    // no active subroutine uniforms, so any index is out of range
    ERROR_RETURN(GL_INVALID_VALUE);
}

void mglGetActiveSubroutineUniformName(GLMContext ctx, GLuint program, GLenum shadertype, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    ERROR_RETURN(GL_INVALID_VALUE);
}

GLuint mglGetSubroutineIndex(GLMContext ctx, GLuint program, GLenum shadertype, const GLchar *name)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN_VALUE(pptr, GL_INVALID_VALUE, GL_INVALID_INDEX);
    ERROR_CHECK_RETURN_VALUE(name, GL_INVALID_VALUE, GL_INVALID_INDEX);

    return GL_INVALID_INDEX;
}

GLint mglGetSubroutineUniformLocation(GLMContext ctx, GLuint program, GLenum shadertype, const GLchar *name)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN_VALUE(pptr, GL_INVALID_VALUE, -1);
    ERROR_CHECK_RETURN_VALUE(name, GL_INVALID_VALUE, -1);

    return -1;
}

void mglGetUniformSubroutineuiv(GLMContext ctx, GLenum shadertype, GLint location, GLuint *params)
{
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ctx->state.program, GL_INVALID_OPERATION);

    // location can only be out of range when there are no subroutine uniforms
    ERROR_RETURN(GL_INVALID_VALUE);
}

void mglUniformSubroutinesuiv(GLMContext ctx, GLenum shadertype, GLsizei count, const GLuint *indices)
{
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ctx->state.program, GL_INVALID_OPERATION);

    // count must equal the active subroutine uniform count, which is zero
    ERROR_CHECK_RETURN(count == 0, GL_INVALID_VALUE);
}

/* ---------- program interface query ---------- */

static int spvcTypeForInterface(GLenum programInterface)
{
    switch (programInterface)
    {
        case GL_UNIFORM:                return SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT;
        case GL_UNIFORM_BLOCK:          return SPVC_RESOURCE_TYPE_UNIFORM_BUFFER;
        case GL_PROGRAM_INPUT:          return SPVC_RESOURCE_TYPE_STAGE_INPUT;
        case GL_PROGRAM_OUTPUT:         return SPVC_RESOURCE_TYPE_STAGE_OUTPUT;
        case GL_BUFFER_VARIABLE:        return SPVC_RESOURCE_TYPE_STORAGE_BUFFER;
        case GL_SHADER_STORAGE_BLOCK:   return SPVC_RESOURCE_TYPE_STORAGE_BUFFER;
    }

    return -1;
}

// resources are numbered across all stages in stage order
static SpirvResource *resourceAt(Program *ptr, int res_type, GLuint index)
{
    for (int stage = 0; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *l = &ptr->spirv_resources_list[stage][res_type];

        if (index < l->count)
            return &l->list[index];

        index -= l->count;
    }

    return NULL;
}

static GLint resourceIndexByName(Program *ptr, int res_type, const char *name)
{
    GLuint base = 0;

    for (int stage = 0; stage < _MAX_SHADER_TYPES; stage++)
    {
        SpirvResourceList *l = &ptr->spirv_resources_list[stage][res_type];

        for (GLuint i = 0; i < l->count; i++)
            if (l->list[i].name && strcmp(l->list[i].name, name) == 0)
                return (GLint)(base + i);

        base += l->count;
    }

    return -1;
}

static int stageListsName(Program *ptr, int stage, int res_type, const char *name)
{
    SpirvResourceList *l = &ptr->spirv_resources_list[stage][res_type];

    for (GLuint i = 0; i < l->count; i++)
        if (l->list[i].name && strcmp(l->list[i].name, name) == 0)
            return 1;

    return 0;
}

void mglGetProgramInterfaceiv(GLMContext ctx, GLuint program, GLenum programInterface, GLenum pname, GLint *params)
{
    Program *ptr = findProgram(ctx, program);
    int res_type = spvcTypeForInterface(programInterface);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(res_type >= 0, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    switch (pname)
    {
        case GL_ACTIVE_RESOURCES:
            *params = programResourceCount(ptr, res_type);
            break;

        case GL_MAX_NAME_LENGTH:
            *params = programLongestName(ptr, res_type);
            break;

        case GL_MAX_NUM_ACTIVE_VARIABLES:
            *params = 0;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

GLuint mglGetProgramResourceIndex(GLMContext ctx, GLuint program, GLenum programInterface, const GLchar *name)
{
    Program *ptr = findProgram(ctx, program);
    int res_type = spvcTypeForInterface(programInterface);
    GLint idx;

    ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_VALUE, GL_INVALID_INDEX);
    ERROR_CHECK_RETURN_VALUE(res_type >= 0, GL_INVALID_ENUM, GL_INVALID_INDEX);
    ERROR_CHECK_RETURN_VALUE(name, GL_INVALID_VALUE, GL_INVALID_INDEX);

    idx = resourceIndexByName(ptr, res_type, name);

    return (idx < 0) ? GL_INVALID_INDEX : (GLuint)idx;
}

void mglGetProgramResourceName(GLMContext ctx, GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name)
{
    Program *ptr = findProgram(ctx, program);
    int res_type = spvcTypeForInterface(programInterface);
    SpirvResource *res;
    GLsizei n;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(res_type >= 0, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    res = resourceAt(ptr, res_type, index);
    ERROR_CHECK_RETURN(res, GL_INVALID_VALUE);

    if (name == NULL || bufSize == 0)
    {
        if (length)
            *length = 0;

        return;
    }

    n = res->name ? (GLsizei)strlen(res->name) : 0;

    if (n > bufSize - 1)
        n = bufSize - 1;

    if (n > 0)
        memcpy(name, res->name, (size_t)n);

    name[n] = '\0';

    if (length)
        *length = n;
}

void mglGetProgramResourceiv(GLMContext ctx, GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei count, GLsizei *length, GLint *params)
{
    Program *ptr = findProgram(ctx, program);
    int res_type = spvcTypeForInterface(programInterface);
    SpirvResource *res;
    GLsizei written = 0;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(res_type >= 0, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(propCount > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(props && params, GL_INVALID_VALUE);

    res = resourceAt(ptr, res_type, index);
    ERROR_CHECK_RETURN(res, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < propCount && written < count; i++)
    {
        switch (props[i])
        {
            case GL_NAME_LENGTH:
                params[written++] = res->name ? (GLint)strlen(res->name) + 1 : 0;
                break;

            case GL_LOCATION:
                params[written++] = (GLint)res->location;
                break;

            case GL_OFFSET:
            case GL_BLOCK_INDEX:
                params[written++] = -1;
                break;

            case GL_ARRAY_SIZE:
                params[written++] = 1;
                break;

            // the linker does not keep GLSL types yet
            case GL_TYPE:
                params[written++] = GL_NONE;
                break;

            case GL_REFERENCED_BY_VERTEX_SHADER:
                params[written++] = stageListsName(ptr, _VERTEX_SHADER, res_type, res->name);
                break;

            case GL_REFERENCED_BY_FRAGMENT_SHADER:
                params[written++] = stageListsName(ptr, _FRAGMENT_SHADER, res_type, res->name);
                break;

            default:
                ERROR_RETURN(GL_INVALID_ENUM);
        }
    }

    if (length)
        *length = written;
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

    switch (pname)
    {
        case GL_ACTIVE_SUBROUTINES:
        case GL_ACTIVE_SUBROUTINE_UNIFORMS:
        case GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS:
        case GL_ACTIVE_SUBROUTINE_MAX_LENGTH:
        case GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH:
            *params = 0;
            break;

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
    // MGL has no dual source blending, so a known output is always index 0
    return (mglGetFragDataLocation(ctx, program, name) < 0) ? -1 : 0;
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

    // output locations come from the GLSL layout qualifier, so there is nothing
    // to rebind; the call is still validated
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

void mglValidateProgramPipeline(GLMContext ctx, GLuint pipeline)
{
    ProgramPipeline *pp = findProgramPipeline(ctx, pipeline);
    bool stage_present = false;

    ERROR_CHECK_RETURN(pp, GL_INVALID_OPERATION);

    for (int i = 0; i < _MAX_SHADER_TYPES; i++)
        if (pp->stage_programs[i] && pp->stage_programs[i]->link_status == GL_TRUE)
            stage_present = true;

    pp->validated = stage_present ? GL_TRUE : GL_FALSE;
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

        // MGL has no separate active program, so the vertex stage stands in
        case GL_ACTIVE_PROGRAM:
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

    ERROR_CHECK_RETURN(pp, GL_INVALID_OPERATION);

    if (program == 0)
        return;

    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(pptr->link_status == GL_TRUE, GL_INVALID_OPERATION);
}

void mglShaderStorageBlockBinding(GLMContext ctx, GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(storageBlockIndex < (GLuint)programResourceCount(pptr, SPVC_RESOURCE_TYPE_STORAGE_BUFFER), GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(storageBlockBinding < MAX_BINDABLE_BUFFERS, GL_INVALID_VALUE);
}

void mglGetActiveAtomicCounterBufferiv(GLMContext ctx, GLuint program, GLuint bufferIndex, GLenum pname, GLint *params)
{
    Program *pptr = findProgram(ctx, program);

    ERROR_CHECK_RETURN(pptr, GL_INVALID_VALUE);

    (void)bufferIndex;
    (void)pname;
    (void)params;

    // the linker captures no atomic counter buffers, so there is no valid index
    ERROR_RETURN(GL_INVALID_VALUE);
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
