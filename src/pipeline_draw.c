/*
 * Copyright (C) The MooGL Project
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
 * pipeline_draw.c
 * MGL
 *
 * Drawing with a program pipeline. Metal wants every stage in one pipeline
 * built together, so the stages the pipeline names are linked into one hidden
 * program, which stands in as the current program for the length of each draw.
 * Its uniforms and block bindings are copied over from the stage programs
 * before every draw, since those are what the application sets.
 */

#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "mgl_log.h"

GLuint mglCreateProgram(GLMContext ctx);
void mglDeleteProgram(GLMContext ctx, GLuint program);
void mglLinkProgram(GLMContext ctx, GLuint program);
void mglAttachShader(GLMContext ctx, GLuint program, GLuint shader);
GLuint mglCreateShader(GLMContext ctx, GLenum type);
void mglDeleteShader(GLMContext ctx, GLuint shader);
void mglShaderSource(GLMContext ctx, GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
void mglCompileShader(GLMContext ctx, GLuint shader);
void mglTransformFeedbackVaryings(GLMContext ctx, GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode);
void mglBindAttribLocation(GLMContext ctx, GLuint program, GLuint index, const GLchar *name);
void mglBindFragDataLocationIndexed(GLMContext ctx, GLuint program, GLuint colorNumber, GLuint index, const GLchar *name);
void mglUniformBlockBinding(GLMContext ctx, GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
void mglShaderStorageBlockBinding(GLMContext ctx, GLuint program, GLuint storageBlockIndex, GLuint storageBlockBinding);
GLuint mglGetProgramResourceIndex(GLMContext ctx, GLuint program, GLenum programInterface, const GLchar *name);
void mglReadUniform(GLMContext ctx, Program *pp, GLint location, void *params, GLenum as);
GLint mglUniformBlockBindingOf(Program *ptr, GLuint index);
void mglProgramUniform1fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglProgramUniform2fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglProgramUniform3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglProgramUniform4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLfloat *value);
void mglProgramUniform1iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value);
void mglProgramUniform2iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value);
void mglProgramUniform3iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value);
void mglProgramUniform4iv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLint *value);
void mglProgramUniform1uiv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLuint *value);
void mglProgramUniform2uiv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLuint *value);
void mglProgramUniform3uiv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLuint *value);
void mglProgramUniform4uiv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLuint *value);
void mglProgramUniform1dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value);
void mglProgramUniform2dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value);
void mglProgramUniform3dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value);
void mglProgramUniform4dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, const GLdouble *value);
void mglProgramUniformMatrix2fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix2x3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix3x2fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix2x4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix4x2fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix3x4fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix4x3fv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
void mglProgramUniformMatrix2dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
void mglProgramUniformMatrix3dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
void mglProgramUniformMatrix4dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
void mglProgramUniformMatrix2x3dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
void mglProgramUniformMatrix3x2dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
void mglProgramUniformMatrix2x4dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
void mglProgramUniformMatrix4x2dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
void mglProgramUniformMatrix3x4dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);
void mglProgramUniformMatrix4x3dv(GLMContext ctx, GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLdouble *value);

extern Program *findProgram(GLMContext ctx, GLuint program);

// What GL's pipeline validation would object to, or NULL when nothing.
const char *mglPipelineProblem(const ProgramPipeline *pp)
{
    bool any = false;

    for (int st = 0; st < _MAX_SHADER_TYPES; st++)
    {
        const Program *p = pp->stage_programs[st];

        if (p == NULL)
            continue;

        any = true;

        if (p->link_status != GL_TRUE)
            return "a stage's program did not link";

        if (!p->linked_separable)
            return "a stage's program is not separable";

        // in use for every stage it was linked with, or for none of them
        for (int other = 0; other < _MAX_SHADER_TYPES; other++)
            if (p->stage_src[other] && pp->stage_programs[other] != p)
                return "a program is in use for only some of its stages";
    }

    return any ? NULL : "no stage has a program";
}

void mglDropPipelineProgram(GLMContext ctx, ProgramPipeline *pp)
{
    if (pp->merged)
    {
        ctx->error_suppress++;
        mglDeleteProgram(ctx, pp->merged->name);
        ctx->error_suppress--;
    }

    pp->merged = NULL;
    memset(pp->merged_from, 0, sizeof(pp->merged_from));
}

#ifdef MGL_GL_CORE

static const int graphics_stages[] = {
    _VERTEX_SHADER, _TESS_CONTROL_SHADER, _TESS_EVALUATION_SHADER, _GEOMETRY_SHADER, _FRAGMENT_SHADER
};

static GLenum glStage(int stage)
{
    switch (stage)
    {
        case _VERTEX_SHADER:          return GL_VERTEX_SHADER;
        case _TESS_CONTROL_SHADER:    return GL_TESS_CONTROL_SHADER;
        case _TESS_EVALUATION_SHADER: return GL_TESS_EVALUATION_SHADER;
        case _GEOMETRY_SHADER:        return GL_GEOMETRY_SHADER;
        case _FRAGMENT_SHADER:        return GL_FRAGMENT_SHADER;
        default:                      return GL_COMPUTE_SHADER;
    }
}

// The program that writes the vertices the rasteriser sees: its recorded
// feedback varyings are the ones that apply.
static Program *lastVertexStage(ProgramPipeline *pp)
{
    static const int order[] = { _GEOMETRY_SHADER, _TESS_EVALUATION_SHADER, _VERTEX_SHADER };

    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); i++)
        if (pp->stage_programs[order[i]])
            return pp->stage_programs[order[i]];

    return NULL;
}

static Program *linkMerged(GLMContext ctx, ProgramPipeline *pp)
{
    GLuint name = mglCreateProgram(ctx);
    Program *merged = findProgram(ctx, name);
    GLuint shaders[_MAX_SHADER_TYPES] = { 0 };

    if (merged == NULL)
        return NULL;

    for (unsigned i = 0; i < sizeof(graphics_stages) / sizeof(graphics_stages[0]); i++)
    {
        int s = graphics_stages[i];
        Program *from = pp->stage_programs[s];

        if (from == NULL || from->stage_src[s] == NULL)
            continue;

        const GLchar *src = from->stage_src[s];

        shaders[s] = mglCreateShader(ctx, glStage(s));
        mglShaderSource(ctx, shaders[s], 1, &src, NULL);
        mglCompileShader(ctx, shaders[s]);
        mglAttachShader(ctx, name, shaders[s]);
    }

    Program *last = lastVertexStage(pp);

    if (last && last->xfb_varying_count > 0)
        mglTransformFeedbackVaryings(ctx, name, last->xfb_varying_count,
                                     (const GLchar *const *)last->xfb_varyings, last->xfb_buffer_mode);

    Program *vs = pp->stage_programs[_VERTEX_SHADER];
    Program *fs = pp->stage_programs[_FRAGMENT_SHADER];

    for (GLint i = 0; vs && i < vs->attrib_bind_count; i++)
        mglBindAttribLocation(ctx, name, vs->attrib_binds[i].location, vs->attrib_binds[i].name);

    for (GLint i = 0; fs && i < fs->frag_bind_count; i++)
        mglBindFragDataLocationIndexed(ctx, name, fs->frag_binds[i].location, fs->frag_binds[i].index,
                                       fs->frag_binds[i].name);

    mglLinkProgram(ctx, name);

    for (int s = 0; s < _MAX_SHADER_TYPES; s++)
        if (shaders[s])
            mglDeleteShader(ctx, shaders[s]);

    if (merged->link_status != GL_TRUE)
    {
        MGL_ERR("MGL Error: program pipeline %u does not link as one program:\n%s\n",
                pp->name, merged->log ? merged->log : "");
        mglDeleteProgram(ctx, name);
        return NULL;
    }

    return merged;
}

static SpirvResource *sameUniform(Program *p, const char *name)
{
    for (int s = 0; s < _MAX_SHADER_TYPES; s++)
    {
        SpirvResourceList *list = &p->spirv_resources_list[s][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];

        for (GLuint i = 0; i < list->count; i++)
            if (list->list[i].name && list->list[i].location != MGL_NO_LOCATION &&
                !strcmp(list->list[i].name, name))
                return &list->list[i];
    }

    return NULL;
}

// One uniform value from one program to the other, whatever its type.
static void copyUniform(GLMContext ctx, Program *from, GLint src, Program *to, GLint dst, GLenum type)
{
    GLfloat f[16];
    GLint i[4];
    GLuint u[4];
    GLdouble d[16];
    GLuint b = to->name;

    switch (type)
    {
        case GL_FLOAT:        mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniform1fv(ctx, b, dst, 1, f); return;
        case GL_FLOAT_VEC2:   mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniform2fv(ctx, b, dst, 1, f); return;
        case GL_FLOAT_VEC3:   mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniform3fv(ctx, b, dst, 1, f); return;
        case GL_FLOAT_VEC4:   mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniform4fv(ctx, b, dst, 1, f); return;
        case GL_INT_VEC2:
        case GL_BOOL_VEC2:    mglReadUniform(ctx, from, src, i, GL_INT); mglProgramUniform2iv(ctx, b, dst, 1, i); return;
        case GL_INT_VEC3:
        case GL_BOOL_VEC3:    mglReadUniform(ctx, from, src, i, GL_INT); mglProgramUniform3iv(ctx, b, dst, 1, i); return;
        case GL_INT_VEC4:
        case GL_BOOL_VEC4:    mglReadUniform(ctx, from, src, i, GL_INT); mglProgramUniform4iv(ctx, b, dst, 1, i); return;
        case GL_UNSIGNED_INT: mglReadUniform(ctx, from, src, u, GL_UNSIGNED_INT); mglProgramUniform1uiv(ctx, b, dst, 1, u); return;
        case GL_UNSIGNED_INT_VEC2: mglReadUniform(ctx, from, src, u, GL_UNSIGNED_INT); mglProgramUniform2uiv(ctx, b, dst, 1, u); return;
        case GL_UNSIGNED_INT_VEC3: mglReadUniform(ctx, from, src, u, GL_UNSIGNED_INT); mglProgramUniform3uiv(ctx, b, dst, 1, u); return;
        case GL_UNSIGNED_INT_VEC4: mglReadUniform(ctx, from, src, u, GL_UNSIGNED_INT); mglProgramUniform4uiv(ctx, b, dst, 1, u); return;
        case GL_DOUBLE:       mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniform1dv(ctx, b, dst, 1, d); return;
        case GL_DOUBLE_VEC2:  mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniform2dv(ctx, b, dst, 1, d); return;
        case GL_DOUBLE_VEC3:  mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniform3dv(ctx, b, dst, 1, d); return;
        case GL_DOUBLE_VEC4:  mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniform4dv(ctx, b, dst, 1, d); return;
        case GL_FLOAT_MAT2:   mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix2fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_FLOAT_MAT3:   mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix3fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_FLOAT_MAT4:   mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix4fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_FLOAT_MAT2x3: mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix2x3fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_FLOAT_MAT3x2: mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix3x2fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_FLOAT_MAT2x4: mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix2x4fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_FLOAT_MAT4x2: mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix4x2fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_FLOAT_MAT3x4: mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix3x4fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_FLOAT_MAT4x3: mglReadUniform(ctx, from, src, f, GL_FLOAT); mglProgramUniformMatrix4x3fv(ctx, b, dst, 1, GL_FALSE, f); return;
        case GL_DOUBLE_MAT2:   mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix2dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_DOUBLE_MAT3:   mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix3dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_DOUBLE_MAT4:   mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix4dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_DOUBLE_MAT2x3: mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix2x3dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_DOUBLE_MAT3x2: mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix3x2dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_DOUBLE_MAT2x4: mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix2x4dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_DOUBLE_MAT4x2: mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix4x2dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_DOUBLE_MAT3x4: mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix3x4dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_DOUBLE_MAT4x3: mglReadUniform(ctx, from, src, d, GL_DOUBLE); mglProgramUniformMatrix4x3dv(ctx, b, dst, 1, GL_FALSE, d); return;
        case GL_UNSIGNED_INT_ATOMIC_COUNTER: return;
        default:
            // int, bool, and every sampler and image, which hold a unit
            mglReadUniform(ctx, from, src, i, GL_INT);
            mglProgramUniform1iv(ctx, b, dst, 1, i);
            return;
    }
}

// Everything the application set on the stage programs, onto the stand-in.
static void copyStageState(GLMContext ctx, ProgramPipeline *pp, Program *merged)
{
    for (unsigned k = 0; k < sizeof(graphics_stages) / sizeof(graphics_stages[0]); k++)
    {
        int s = graphics_stages[k];
        Program *from = pp->stage_programs[s];

        if (from == NULL || from == merged)
            continue;

        SpirvResourceList *list = &from->spirv_resources_list[s][SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT];

        for (GLuint i = 0; i < list->count; i++)
        {
            SpirvResource *r = &list->list[i];
            SpirvResource *to;

            if (r->name == NULL || r->gl_type == 0 || r->location == MGL_NO_LOCATION)
                continue;

            to = sameUniform(merged, r->name);

            if (to == NULL)
                continue;

            for (GLint e = 0; e < (r->array_size > 0 ? r->array_size : 1); e++)
                copyUniform(ctx, from, (GLint)(r->location + e), merged, (GLint)(to->location + e), r->gl_type);
        }

        // block bindings, by name, for the blocks this stage uses
        static const GLenum ifaces[2] = { GL_UNIFORM_BLOCK, GL_SHADER_STORAGE_BLOCK };
        static const MglResourceKind kinds[2] = { MGL_RES_UNIFORM_BLOCK, MGL_RES_STORAGE_BLOCK };

        for (int t = 0; t < 2; t++)
            for (GLint b = 0; b < from->resources.count[kinds[t]]; b++)
            {
                MglResource *res = &from->resources.list[kinds[t]][b];
                GLint binding = t == 0 ? mglUniformBlockBindingOf(from, (GLuint)b) : res->binding;
                GLuint idx;

                if (!(res->stages & (1u << s)))
                    continue;

                idx = mglGetProgramResourceIndex(ctx, merged->name, ifaces[t], res->name);

                if (binding < 0 || idx == GL_INVALID_INDEX)
                    continue;

                if (t == 0)
                    mglUniformBlockBinding(ctx, merged->name, idx, (GLuint)binding);
                else
                    mglShaderStorageBlockBinding(ctx, merged->name, idx, (GLuint)binding);
            }
    }
}

// The program a draw with this pipeline runs, or NULL when there is none.
static Program *pipelineProgram(GLMContext ctx, ProgramPipeline *pp)
{
    Program *only = NULL;
    bool one = true;

    for (unsigned i = 0; i < sizeof(graphics_stages) / sizeof(graphics_stages[0]); i++)
    {
        Program *p = pp->stage_programs[graphics_stages[i]];

        if (p == NULL)
            continue;

        if (only && p != only)
            one = false;

        only = p;
    }

    if (only == NULL)
        return NULL;

    // one program for every stage is simply that program
    if (one)
        return only->link_status == GL_TRUE ? only : NULL;

    bool fresh = pp->merged != NULL;

    for (unsigned i = 0; fresh && i < sizeof(graphics_stages) / sizeof(graphics_stages[0]); i++)
    {
        int s = graphics_stages[i];
        GLuint serial = pp->stage_programs[s] ? pp->stage_programs[s]->link_serial : 0;

        if (pp->merged_from[s] != serial)
            fresh = false;
    }

    if (!fresh)
    {
        mglDropPipelineProgram(ctx, pp);
        pp->merged = linkMerged(ctx, pp);

        for (unsigned i = 0; i < sizeof(graphics_stages) / sizeof(graphics_stages[0]); i++)
        {
            int s = graphics_stages[i];

            pp->merged_from[s] = pp->stage_programs[s] ? pp->stage_programs[s]->link_serial : 0;
        }
    }

    if (pp->merged)
        copyStageState(ctx, pp, pp->merged);

    return pp->merged;
}

enum { RUN, RUN_SWAPPED, SKIP };

// Puts the pipeline's program in place for one draw. RUN_SWAPPED means leave()
// has to follow the draw; SKIP means there is nothing GL would draw with.
static int enter(GLMContext ctx, bool compute)
{
    ProgramPipeline *pp = STATE(program_pipeline);
    Program *p;

    if (STATE(program) || pp == NULL)
        return RUN;

    // a pipeline GL would not validate is an error to draw with, except that
    // one with nothing in it just draws nothing
    const char *problem = mglPipelineProblem(pp);

    if (problem && strcmp(problem, "no stage has a program"))
    {
        STATE(error) = GL_INVALID_OPERATION;
        return SKIP;
    }

    // GL leaves a draw without a vertex or fragment stage undefined, but
    // raising an error for it is not allowed
    if (!compute && (pp->stage_programs[_VERTEX_SHADER] == NULL ||
                     pp->stage_programs[_FRAGMENT_SHADER] == NULL))
        return SKIP;

    GLenum err = STATE(error);

    ctx->error_suppress++;
    p = compute ? pp->stage_programs[_COMPUTE_SHADER] : pipelineProgram(ctx, pp);
    ctx->error_suppress--;
    STATE(error) = err;

    if (p == NULL || p->link_status != GL_TRUE)
        return compute ? RUN : SKIP;

    STATE(program) = p;
    STATE(dirty_bits) |= DIRTY_PROGRAM;

    return RUN_SWAPPED;
}

static void leave(GLMContext ctx)
{
    STATE(program) = NULL;
    STATE(dirty_bits) |= DIRTY_PROGRAM;
}

#define WRAP(fn, params, args, compute)               \
    void fn params;                                   \
    static void pipe_##fn params                      \
    {                                                 \
        int how = enter(ctx, compute);                \
        if (how == SKIP)                              \
            return;                                   \
        fn args;                                      \
        if (how == RUN_SWAPPED)                       \
            leave(ctx);                               \
    }

WRAP(mglDrawArrays, (GLMContext ctx, GLenum mode, GLint first, GLsizei count), (ctx, mode, first, count), false)
WRAP(mglDrawElements, (GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices), (ctx, mode, count, type, indices), false)
WRAP(mglDrawRangeElements, (GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices), (ctx, mode, start, end, count, type, indices), false)
WRAP(mglMultiDrawArrays, (GLMContext ctx, GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount), (ctx, mode, first, count, drawcount), false)
WRAP(mglMultiDrawElements, (GLMContext ctx, GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount), (ctx, mode, count, type, indices, drawcount), false)
WRAP(mglDrawArraysInstanced, (GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLsizei instancecount), (ctx, mode, first, count, instancecount), false)
WRAP(mglDrawElementsInstanced, (GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount), (ctx, mode, count, type, indices, instancecount), false)
WRAP(mglDrawElementsBaseVertex, (GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex), (ctx, mode, count, type, indices, basevertex), false)
WRAP(mglDrawRangeElementsBaseVertex, (GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex), (ctx, mode, start, end, count, type, indices, basevertex), false)
WRAP(mglDrawElementsInstancedBaseVertex, (GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex), (ctx, mode, count, type, indices, instancecount, basevertex), false)
WRAP(mglMultiDrawElementsBaseVertex, (GLMContext ctx, GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex), (ctx, mode, count, type, indices, drawcount, basevertex), false)
WRAP(mglDrawArraysIndirect, (GLMContext ctx, GLenum mode, const void *indirect), (ctx, mode, indirect), false)
WRAP(mglDrawElementsIndirect, (GLMContext ctx, GLenum mode, GLenum type, const void *indirect), (ctx, mode, type, indirect), false)
WRAP(mglDrawTransformFeedback, (GLMContext ctx, GLenum mode, GLuint id), (ctx, mode, id), false)
WRAP(mglDrawTransformFeedbackStream, (GLMContext ctx, GLenum mode, GLuint id, GLuint stream), (ctx, mode, id, stream), false)
WRAP(mglDrawArraysInstancedBaseInstance, (GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance), (ctx, mode, first, count, instancecount, baseinstance), false)
WRAP(mglDrawElementsInstancedBaseInstance, (GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance), (ctx, mode, count, type, indices, instancecount, baseinstance), false)
WRAP(mglDrawElementsInstancedBaseVertexBaseInstance, (GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance), (ctx, mode, count, type, indices, instancecount, basevertex, baseinstance), false)
WRAP(mglDrawTransformFeedbackInstanced, (GLMContext ctx, GLenum mode, GLuint id, GLsizei instancecount), (ctx, mode, id, instancecount), false)
WRAP(mglDrawTransformFeedbackStreamInstanced, (GLMContext ctx, GLenum mode, GLuint id, GLuint stream, GLsizei instancecount), (ctx, mode, id, stream, instancecount), false)
WRAP(mglMultiDrawArraysIndirect, (GLMContext ctx, GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride), (ctx, mode, indirect, drawcount, stride), false)
WRAP(mglMultiDrawElementsIndirect, (GLMContext ctx, GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride), (ctx, mode, type, indirect, drawcount, stride), false)
WRAP(mglMultiDrawArraysIndirectCount, (GLMContext ctx, GLenum mode, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride), (ctx, mode, indirect, drawcount, maxdrawcount, stride), false)
WRAP(mglMultiDrawElementsIndirectCount, (GLMContext ctx, GLenum mode, GLenum type, const void *indirect, GLintptr drawcount, GLsizei maxdrawcount, GLsizei stride), (ctx, mode, type, indirect, drawcount, maxdrawcount, stride), false)
WRAP(mglDispatchCompute, (GLMContext ctx, GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z), (ctx, num_groups_x, num_groups_y, num_groups_z), true)
WRAP(mglDispatchComputeIndirect, (GLMContext ctx, GLintptr indirect), (ctx, indirect), true)

// Points the draw and dispatch entries at the pipeline-aware versions. Runs
// after the dispatch table is filled in; an entry this API does not have stays
// empty.
#endif

void mglWrapPipelineDraws(GLMContext ctx)
{
#ifdef MGL_GL_CORE
#define SWAP(field, fn) if (ctx->dispatch.field == fn) ctx->dispatch.field = pipe_##fn

    SWAP(draw_arrays, mglDrawArrays);
    SWAP(draw_elements, mglDrawElements);
    SWAP(draw_range_elements, mglDrawRangeElements);
    SWAP(multi_draw_arrays, mglMultiDrawArrays);
    SWAP(multi_draw_elements, mglMultiDrawElements);
    SWAP(draw_arrays_instanced, mglDrawArraysInstanced);
    SWAP(draw_elements_instanced, mglDrawElementsInstanced);
    SWAP(draw_elements_base_vertex, mglDrawElementsBaseVertex);
    SWAP(draw_range_elements_base_vertex, mglDrawRangeElementsBaseVertex);
    SWAP(draw_elements_instanced_base_vertex, mglDrawElementsInstancedBaseVertex);
    SWAP(multi_draw_elements_base_vertex, mglMultiDrawElementsBaseVertex);
    SWAP(draw_arrays_indirect, mglDrawArraysIndirect);
    SWAP(draw_elements_indirect, mglDrawElementsIndirect);
    SWAP(draw_transform_feedback, mglDrawTransformFeedback);
    SWAP(draw_transform_feedback_stream, mglDrawTransformFeedbackStream);
    SWAP(draw_arrays_instanced_base_instance, mglDrawArraysInstancedBaseInstance);
    SWAP(draw_elements_instanced_base_instance, mglDrawElementsInstancedBaseInstance);
    SWAP(draw_elements_instanced_base_vertex_base_instance, mglDrawElementsInstancedBaseVertexBaseInstance);
    SWAP(draw_transform_feedback_instanced, mglDrawTransformFeedbackInstanced);
    SWAP(draw_transform_feedback_stream_instanced, mglDrawTransformFeedbackStreamInstanced);
    SWAP(multi_draw_arrays_indirect, mglMultiDrawArraysIndirect);
    SWAP(multi_draw_elements_indirect, mglMultiDrawElementsIndirect);
    SWAP(multi_draw_arrays_indirect_count, mglMultiDrawArraysIndirectCount);
    SWAP(multi_draw_elements_indirect_count, mglMultiDrawElementsIndirectCount);
    SWAP(dispatch_compute, mglDispatchCompute);
    SWAP(dispatch_compute_indirect, mglDispatchComputeIndirect);

#undef SWAP
#else
    (void)ctx;
#endif
}
