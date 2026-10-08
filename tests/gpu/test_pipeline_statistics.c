/*
 * test_pipeline_statistics.c
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
 * The eleven pipeline statistics queries. Each one was refused with
 * GL_INVALID_ENUM, though the extension string said they were there.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static const char *VS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

static const char *FS =
    "#version 460 core\n"
    "out vec4 o;\n"
    "void main() { o = vec4(0.0, 1.0, 0.0, 1.0); }\n";

// Runs draw() inside a query on target and returns what it counted.
static GLuint64 counted(GLenum target, void (*draw)(void *), void *arg)
{
    GLuint q;
    GLuint64 n = 0xdeadbeef;

    glGenQueries(1, &q);
    glBeginQuery(target, q);
    draw(arg);
    glEndQuery(target);
    glGetQueryObjectui64v(q, GL_QUERY_RESULT, &n);
    glDeleteQueries(1, &q);

    return n;
}

typedef struct {
    GLenum mode;
    GLsizei count, instances;
    bool elements;
} Draw;

static void drawIt(void *arg)
{
    Draw *d = (Draw *)arg;

    if (d->elements)
        glDrawElementsInstanced(d->mode, d->count, GL_UNSIGNED_SHORT, NULL, d->instances);
    else
        glDrawArraysInstanced(d->mode, 0, d->count, d->instances);
}

static void setUp(MGLTestTarget *t, GLuint *prog, GLuint *vao, GLuint *bufs)
{
    // a fan of eight vertices around the centre, and indices with a restart
    static const GLfloat v[16] = { 0,0, 1,0, 1,1, 0,1, -1,1, -1,0, -1,-1, 0,-1 };
    static const GLushort idx[8] = { 0, 1, 2, 3, 0xFFFF, 4, 5, 6 };
    char log[1024];

    *prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(*prog != 0, "program did not link: %s", log);

    mgl_target_create(t, 32, 32, GL_RGBA8, 0);
    mgl_target_bind(t);
    glViewport(0, 0, 32, 32);

    glGenVertexArrays(1, vao);
    glBindVertexArray(*vao);
    glGenBuffers(2, bufs);
    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
    glUseProgram(*prog);
}

static void tearDown(MGLTestTarget *t, GLuint prog, GLuint vao, GLuint *bufs)
{
    glUseProgram(0);
    glBindVertexArray(0);
    glDeleteBuffers(2, bufs);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    mgl_target_destroy(t);
}

// What a draw was asked to make: vertices, primitives, and a vertex stage run
// for each vertex. A restart index splits a strip in two.
GPU_TEST(pipeline_statistics, submitted_counts_follow_the_draw)
{
    MGLTestTarget t;
    GLuint prog, vao, bufs[2];
    GLint bits = 0;

    setUp(&t, &prog, &vao, bufs);
    if (!prog)
        return;

    glGetQueryiv(GL_VERTICES_SUBMITTED, GL_QUERY_COUNTER_BITS, &bits);
    CHECK_MSG(bits > 0, "counter bits %d", bits);

    Draw strip = { GL_TRIANGLE_STRIP, 6, 3, false };

    CHECK_EQ_INT((int)counted(GL_VERTICES_SUBMITTED, drawIt, &strip), 18);
    CHECK_EQ_INT((int)counted(GL_PRIMITIVES_SUBMITTED, drawIt, &strip), 12);
    CHECK_EQ_INT((int)counted(GL_VERTEX_SHADER_INVOCATIONS, drawIt, &strip), 18);
    CHECK_EQ_INT((int)counted(GL_CLIPPING_INPUT_PRIMITIVES, drawIt, &strip), 12);
    CHECK((int)counted(GL_CLIPPING_OUTPUT_PRIMITIVES, drawIt, &strip) >= 12);

    // two runs of four and three: two triangles and one
    Draw restarted = { GL_TRIANGLE_STRIP, 8, 1, true };

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    CHECK_EQ_INT((int)counted(GL_VERTICES_SUBMITTED, drawIt, &restarted), 7);
    CHECK_EQ_INT((int)counted(GL_PRIMITIVES_SUBMITTED, drawIt, &restarted), 3);
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

    // too few vertices for a triangle
    Draw short_one = { GL_TRIANGLES, 2, 1, false };

    CHECK_EQ_INT((int)counted(GL_PRIMITIVES_SUBMITTED, drawIt, &short_one), 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    tearDown(&t, prog, vao, bufs);
}

// Fragments are counted while an occlusion query counts the same draw, and
// each gets its own number.
GPU_TEST(pipeline_statistics, fragments_count_beside_an_occlusion_query)
{
    MGLTestTarget t;
    GLuint prog, vao, bufs[2], q[2];
    GLuint64 frags = 0, samples = 0;

    setUp(&t, &prog, &vao, bufs);
    if (!prog)
        return;

    glGenQueries(2, q);
    glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS, q[0]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBeginQuery(GL_SAMPLES_PASSED, q[1]);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glEndQuery(GL_SAMPLES_PASSED);
    glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS);
    glGetQueryObjectui64v(q[0], GL_QUERY_RESULT, &frags);
    glGetQueryObjectui64v(q[1], GL_QUERY_RESULT, &samples);
    glDeleteQueries(2, q);

    // the fan covers the top right quarter of a 32 x 32 target
    CHECK_MSG(samples == 256, "occlusion counted %llu, want 256", (unsigned long long)samples);
    CHECK_MSG(frags >= 512, "fragments counted %llu, want at least 512", (unsigned long long)frags);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    tearDown(&t, prog, vao, bufs);
}

// Occlusion counter slots went back to nobody, so after 256 render passes
// with a query running every count came back zero.
GPU_TEST(pipeline_statistics, occlusion_slots_are_reused)
{
    MGLTestTarget t;
    GLuint prog, vao, bufs[2], q;
    GLuint64 samples = 0;

    setUp(&t, &prog, &vao, bufs);
    if (!prog)
        return;

    glGenQueries(1, &q);

    for (int i = 0; i < 300; i++)
    {
        glBeginQuery(GL_SAMPLES_PASSED, q);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glEndQuery(GL_SAMPLES_PASSED);
        glGetQueryObjectui64v(q, GL_QUERY_RESULT, &samples);

        if (samples != 256)
            break;
    }

    glDeleteQueries(1, &q);
    CHECK_MSG(samples == 256, "occlusion counted %llu, want 256", (unsigned long long)samples);

    tearDown(&t, prog, vao, bufs);
}

static void dispatchIt(void *arg)
{
    (void)arg;
    glDispatchCompute(3, 2, 1);
}

GPU_TEST(pipeline_statistics, compute_invocations_are_threads)
{
    static const char *CS =
        "#version 460 core\n"
        "layout(local_size_x = 4, local_size_y = 2) in;\n"
        "layout(std430, binding = 0) buffer B { uint n; };\n"
        "void main() { atomicAdd(n, 1u); }\n";
    char log[1024];
    GLuint prog = mgl_build_compute_program(CS, log, sizeof log), buf;
    GLuint zero = 0;

    CHECK_MSG(prog != 0, "kernel did not build: %s", log);
    if (!prog)
        return;

    glGenBuffers(1, &buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4, &zero, GL_DYNAMIC_COPY);
    glUseProgram(prog);
    CHECK_EQ_INT((int)counted(GL_COMPUTE_SHADER_INVOCATIONS, dispatchIt, NULL), 48);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
}

// A geometry shader running twice per point and emitting two triangles a run.
GPU_TEST(pipeline_statistics, geometry_stage_counts)
{
    static const char *GS =
        "#version 460 core\n"
        "layout(points, invocations = 2) in;\n"
        "layout(triangle_strip, max_vertices = 6) out;\n"
        "void main() {\n"
        "  for (int p = 0; p < 2; p++) {\n"
        "    gl_Position = vec4(-1, -1, 0, 1); EmitVertex();\n"
        "    gl_Position = vec4( 1, -1, 0, 1); EmitVertex();\n"
        "    gl_Position = vec4(-1,  1, 0, 1); EmitVertex();\n"
        "    EndPrimitive();\n"
        "  }\n"
        "}\n";
    MGLTestTarget t;
    GLuint prog, vao, bufs[2], v, g, f;
    GLint ok = 0;

    setUp(&t, &prog, &vao, bufs);
    if (!prog)
        return;

    glDeleteProgram(prog);
    prog = glCreateProgram();
    v = glCreateShader(GL_VERTEX_SHADER);
    g = glCreateShader(GL_GEOMETRY_SHADER);
    f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(v, 1, &VS, NULL);
    glShaderSource(g, 1, &GS, NULL);
    glShaderSource(f, 1, &FS, NULL);
    glCompileShader(v);
    glCompileShader(g);
    glCompileShader(f);
    glAttachShader(prog, v);
    glAttachShader(prog, g);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(g);
    glDeleteShader(f);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    glUseProgram(prog);

    Draw points = { GL_POINTS, 3, 1, false };

    CHECK_EQ_INT((int)counted(GL_GEOMETRY_SHADER_INVOCATIONS, drawIt, &points), 6);
    CHECK_EQ_INT((int)counted(GL_GEOMETRY_SHADER_PRIMITIVES_EMITTED, drawIt, &points), 12);
    CHECK_EQ_INT((int)counted(GL_CLIPPING_INPUT_PRIMITIVES, drawIt, &points), 12);
    CHECK_EQ_INT((int)counted(GL_PRIMITIVES_SUBMITTED, drawIt, &points), 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    tearDown(&t, prog, vao, bufs);
}

// One patch per three vertices through the control stage, and the evaluation
// stage run once for each vertex the levels make: a triangle at level 1 has
// three.
GPU_TEST(pipeline_statistics, tessellation_counts)
{
    static const char *TCS =
        "#version 460 core\n"
        "layout(vertices = 3) out;\n"
        "void main() {\n"
        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "  gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0;\n"
        "  gl_TessLevelInner[0] = 1.0;\n"
        "}\n";
    static const char *TES =
        "#version 460 core\n"
        "layout(triangles, equal_spacing, ccw) in;\n"
        "void main() {\n"
        "  gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position +\n"
        "                gl_TessCoord.z * gl_in[2].gl_Position;\n"
        "}\n";
    MGLTestTarget t;
    GLuint prog, vao, bufs[2], sh[4];
    const char *src[4] = { VS, TCS, TES, FS };
    GLenum types[4] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER };
    GLint ok = 0;

    setUp(&t, &prog, &vao, bufs);
    if (!prog)
        return;

    glDeleteProgram(prog);
    prog = glCreateProgram();

    for (int i = 0; i < 4; i++)
    {
        sh[i] = glCreateShader(types[i]);
        glShaderSource(sh[i], 1, &src[i], NULL);
        glCompileShader(sh[i]);
        glAttachShader(prog, sh[i]);
    }

    glLinkProgram(prog);

    for (int i = 0; i < 4; i++)
        glDeleteShader(sh[i]);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    Draw patches = { GL_PATCHES, 6, 1, false };

    CHECK_EQ_INT((int)counted(GL_TESS_CONTROL_SHADER_PATCHES, drawIt, &patches), 2);
    CHECK_EQ_INT((int)counted(GL_TESS_EVALUATION_SHADER_INVOCATIONS, drawIt, &patches), 6);
    CHECK_EQ_INT((int)counted(GL_CLIPPING_INPUT_PRIMITIVES, drawIt, &patches), 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    tearDown(&t, prog, vao, bufs);
}

// Every primitive type through every draw entry point, with the fragment
// count running and read back at once. An indirect draw took its offset in
// commands rather than bytes, so Metal read its arguments out of the vertex
// data, ran a draw that never ended, and the read waited on it for good.
GPU_TEST(pipeline_statistics, every_draw_finishes_with_fragments_counted)
{
    static const float verts[16] = { 0.0f, 0.75f, -0.75f, -0.75f, 0.75f, -0.75f, 0.3f, 0.7f,
                                     -0.4f, 0.2f, 0.6f, -0.3f, -0.3f, -0.7f, 0.0f, 0.0f };
    static const GLuint idx[8] = { 0, 2, 1, 3, 4, 5, 6, 7 };
    static const struct { GLenum mode; GLuint count; } prims[] = {
        { GL_POINTS, 1 }, { GL_LINE_LOOP, 2 }, { GL_LINE_STRIP, 2 }, { GL_LINES, 2 },
        { GL_LINES_ADJACENCY, 4 }, { GL_TRIANGLE_FAN, 3 }, { GL_TRIANGLE_STRIP, 3 },
        { GL_TRIANGLES, 3 }, { GL_TRIANGLES_ADJACENCY, 6 },
    };
    const GLintptr idx_at = sizeof verts, arrays_at = idx_at + sizeof idx, elements_at = arrays_at + 16;
    MGLTestTarget t;
    GLuint prog, vao, bufs[2], vbo, q;
    setUp(&t, &prog, &vao, bufs);
    if (!prog)
        return;

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, elements_at + 20, NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof verts, verts);
    glBufferSubData(GL_ARRAY_BUFFER, idx_at, sizeof idx, idx);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glGenQueries(1, &q);

    for (size_t p = 0; p < sizeof prims / sizeof prims[0]; p++)
        for (int call = 0; call < 12; call++)
        {
            GLuint n = prims[p].count;
            GLuint arrays[4] = { n, 4, 0, 1 };
            GLuint elements[5] = { n, 4, (GLuint)(idx_at / 4), 1, 1 };
            const void *at = (const void *)idx_at;
            GLenum m = prims[p].mode;
            GLuint64 frags = 0;

            glBufferSubData(GL_ARRAY_BUFFER, arrays_at, sizeof arrays, arrays);
            glBufferSubData(GL_ARRAY_BUFFER, elements_at, sizeof elements, elements);
            glClear(GL_COLOR_BUFFER_BIT);
            glBeginQuery(GL_FRAGMENT_SHADER_INVOCATIONS, q);

            switch (call)
            {
                case 0:  glDrawArrays(m, 0, n); break;
                case 1:  glDrawArraysIndirect(m, (const void *)arrays_at); break;
                case 2:  glDrawArraysInstanced(m, 0, n, 4); break;
                case 3:  glDrawArraysInstancedBaseInstance(m, 0, n, 4, 1); break;
                case 4:  glDrawElements(m, n, GL_UNSIGNED_INT, at); break;
                case 5:  glDrawElementsBaseVertex(m, n, GL_UNSIGNED_INT, at, 1); break;
                case 6:  glDrawElementsIndirect(m, GL_UNSIGNED_INT, (const void *)elements_at); break;
                case 7:  glDrawElementsInstanced(m, n, GL_UNSIGNED_INT, at, 4); break;
                case 8:  glDrawElementsInstancedBaseInstance(m, n, GL_UNSIGNED_INT, at, 4, 1); break;
                case 9:  glDrawElementsInstancedBaseVertexBaseInstance(m, n, GL_UNSIGNED_INT, at, 4, 1, 1); break;
                case 10: glDrawRangeElements(m, 0, 8, n, GL_UNSIGNED_INT, at); break;
                case 11: glDrawRangeElementsBaseVertex(m, 0, n - 1, n, GL_UNSIGNED_INT, at, 1); break;
            }

            glEndQuery(GL_FRAGMENT_SHADER_INVOCATIONS);
            glGetQueryObjectui64v(q, GL_QUERY_RESULT, &frags);
            CHECK_MSG(mgl_drain_errors() == GL_NO_ERROR, "mode 0x%x draw %d raised an error", m, call);
            CHECK_MSG(frags >= 1, "mode 0x%x draw %d shaded no fragments", m, call);
        }

    glDeleteQueries(1, &q);
    glDeleteBuffers(1, &vbo);
    tearDown(&t, prog, vao, bufs);
}
