/*
 * test_draw_indirect.c
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
 * What a vertex shader sees of the draw it belongs to: gl_DrawID, and the
 * instance counters, under the indirect and multi-draw commands.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

// gl_InstanceID counts from zero in every draw, gl_BaseInstance carries the
// command's base instance, and gl_DrawID numbers the commands of a multi-draw.
// Metal's instance id starts at the base instance, and gl_DrawID read a
// buffer slot nothing filled.
GPU_TEST(draw_indirect, draw_parameters_under_multi_draw)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std430, binding = 0) buffer Out { ivec4 rec[4]; };\n"
        "void main() {\n"
        "    rec[gl_DrawID * 2 + gl_InstanceID] = ivec4(gl_DrawID, gl_InstanceID, gl_BaseInstance, 1);\n"
        "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "    gl_PointSize = 1.0;\n"
        "}\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    // count, instanceCount, first, baseInstance
    static const GLuint cmds[8] = { 1, 2, 0, 5,   1, 1, 0, 9 };
    GLint rec[16];
    char log[2048] = { 0 };
    MGLTestTarget t;
    GLuint prog, vao, ssbo, ind;

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);

    memset(rec, 0, sizeof rec);
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof rec, rec, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glGenBuffers(1, &ind);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, ind);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof cmds, cmds, GL_STATIC_DRAW);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glMultiDrawArraysIndirect(GL_POINTS, 0, 2, 0);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof rec, rec);

    // draw 0, instances 0 and 1, base instance 5; draw 1, instance 0, base 9
    CHECK_MSG(rec[0] == 0 && rec[1] == 0 && rec[2] == 5 && rec[3] == 1,
              "draw 0 instance 0: %d %d %d %d", rec[0], rec[1], rec[2], rec[3]);
    CHECK_MSG(rec[4] == 0 && rec[5] == 1 && rec[6] == 5 && rec[7] == 1,
              "draw 0 instance 1: %d %d %d %d", rec[4], rec[5], rec[6], rec[7]);
    CHECK_MSG(rec[8] == 1 && rec[9] == 0 && rec[10] == 9 && rec[11] == 1,
              "draw 1 instance 0: %d %d %d %d", rec[8], rec[9], rec[10], rec[11]);

    glDisable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ssbo);
    glDeleteBuffers(1, &ind);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// The count draws take their number of commands from GL_PARAMETER_BUFFER,
// capped at maxdrawcount. MGL had no such target and refused them.
GPU_TEST(draw_indirect, multi_draw_count_reads_the_parameter_buffer)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std430, binding = 0) buffer Out { int hits[4]; };\n"
        "void main() {\n"
        "    atomicAdd(hits[gl_DrawID], 1);\n"
        "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "    gl_PointSize = 1.0;\n"
        "}\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    // three one-point draws; the parameter buffer says to run two of them
    static const GLuint cmds[12] = { 1, 1, 0, 0,   1, 1, 0, 0,   1, 1, 0, 0 };
    static const GLuint count[2] = { 0, 2 };
    GLint hits[4] = { 0, 0, 0, 0 };
    GLint bound = 0;
    char log[2048] = { 0 };
    MGLTestTarget t;
    GLuint prog, vao, ssbo, ind, param;

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof hits, hits, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glGenBuffers(1, &ind);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, ind);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof cmds, cmds, GL_STATIC_DRAW);

    glGenBuffers(1, &param);
    glBindBuffer(GL_PARAMETER_BUFFER, param);
    glBufferData(GL_PARAMETER_BUFFER, sizeof count, count, GL_STATIC_DRAW);
    glGetIntegerv(GL_PARAMETER_BUFFER_BINDING, &bound);
    CHECK_EQ_INT(bound, (GLint)param);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // the count is the second word of the parameter buffer
    glMultiDrawArraysIndirectCount(GL_POINTS, 0, 4, 3, 0);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof hits, hits);
    CHECK_MSG(hits[0] == 1 && hits[1] == 1 && hits[2] == 0,
              "draws ran %d %d %d times, want 1 1 0", hits[0], hits[1], hits[2]);

    // maxdrawcount caps it
    memset(hits, 0, sizeof hits);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof hits, hits);
    glMultiDrawArraysIndirectCount(GL_POINTS, 0, 4, 1, 0);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof hits, hits);
    CHECK_MSG(hits[0] == 1 && hits[1] == 0, "capped draws ran %d %d times, want 1 0", hits[0], hits[1]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDisable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(0);
    glBindBuffer(GL_PARAMETER_BUFFER, 0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ssbo);
    glDeleteBuffers(1, &ind);
    glDeleteBuffers(1, &param);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// gl_BaseVertex is the draw's base vertex, and zero for glDrawArrays even with
// a non-zero first. Metal reported the first vertex.
GPU_TEST(draw_indirect, base_vertex_is_zero_for_array_draws)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std430, binding = 0) buffer Out { int seen[2]; };\n"
        "uniform int slot;\n"
        "void main() {\n"
        "    seen[slot] = gl_BaseVertex + 100;\n"
        "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "    gl_PointSize = 1.0;\n"
        "}\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    static const GLubyte idx[1] = { 0 };
    GLint seen[2] = { 0, 0 };
    char log[2048] = { 0 };
    MGLTestTarget t;
    GLuint prog, vao, ssbo, ebo;

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof seen, seen, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);

    glUseProgram(prog);
    glEnable(GL_PROGRAM_POINT_SIZE);

    glUniform1i(glGetUniformLocation(prog, "slot"), 0);
    glDrawArrays(GL_POINTS, 2, 1);

    glUniform1i(glGetUniformLocation(prog, "slot"), 1);
    glDrawElementsBaseVertex(GL_POINTS, 1, GL_UNSIGNED_BYTE, NULL, 3);

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof seen, seen);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG(seen[0] == 100, "glDrawArrays saw gl_BaseVertex %d, want 0", seen[0] - 100);
    CHECK_MSG(seen[1] == 103, "glDrawElementsBaseVertex saw gl_BaseVertex %d, want 3", seen[1] - 100);

    // the driver's own uniform stays out of the application's view
    {
        GLint n = 0;

        glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &n);
        CHECK_EQ_INT(n, 1);
    }

    glDisable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ssbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// A shader that only enables the ARB extension has gl_BaseVertexARB and not
// gl_BaseVertex, so the rewrite has to keep the name it found.
GPU_TEST(draw_indirect, arb_base_vertex_still_compiles)
{
    static const char *VS =
        "#version 450 core\n"
        "#extension GL_ARB_shader_draw_parameters : require\n"
        "out flat int bv;\n"
        "void main() {\n"
        "    bv = gl_BaseVertexARB;\n"
        "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    static const char *FS =
        "#version 450 core\n"
        "in flat int bv;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(float(bv)); }\n";
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "link: %s", log);
    glDeleteProgram(prog);
}
