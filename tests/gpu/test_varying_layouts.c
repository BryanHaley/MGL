/*
 * test_varying_layouts.c
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Shapes of varying that Metal's stage interface cannot hold directly and
 * that have to be taken apart on one side and put back on the other.
 */

#include <stdlib.h>
#include "mgl_test.h"
#include "harness.h"

#define W 32
#define H 32

static GLuint compileOne(GLenum stage, const char *src, char *log, int log_size)
{
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (!ok && log && log_size)
        glGetShaderInfoLog(sh, log_size, NULL, log);

    return ok ? sh : 0;
}

static GLuint linkVF(const char *vs, const char *fs, char *log, int log_size)
{
    GLuint prog = glCreateProgram();
    GLuint v, f;
    GLint ok = 0;

    if (log && log_size)
        log[0] = 0;

    v = compileOne(GL_VERTEX_SHADER, vs, log, log_size);
    f = compileOne(GL_FRAGMENT_SHADER, fs, log, log_size);

    if (!v || !f)
        return 0;

    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok && log && log_size)
        glGetProgramInfoLog(prog, log_size, NULL, log);

    return ok ? prog : 0;
}

/* ---------- an array of matrices ---------- */

// Metal has no matrix in a stage interface, so a matrix travels as one member
// per column. An array of them was refused outright; it is one member per
// column per element, and the index has to split back the same way.
GPU_TEST(varying_layouts, an_array_of_matrices_crosses_the_stage_interface)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "layout(location = 0) flat out mat2x3 m[2];\n"
        "void main() {\n"
        "  m[0] = mat2x3(0.9, 0.0, 0.0,  0.0, 0.8, 0.0);\n"
        "  m[1] = mat2x3(0.0, 0.0, 0.7,  0.1, 0.2, 0.3);\n"
        "  gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    // each channel comes from a different element and column, so reading any
    // of the four members from the wrong slot changes the colour
    static const char *fs =
        "#version 460 core\n"
        "layout(location = 0) flat in mat2x3 m[2];\n"
        "out vec4 o;\n"
        "void main() { o = vec4(m[0][0].x, m[0][1].y, m[1][0].z, m[1][1].z); }\n";
    static const float tri[] = { -1.0f, -1.0f,  3.0f, -1.0f,  -1.0f, 3.0f };
    MGLTestTarget t;
    GLuint prog, vao = 0, vbo = 0;
    unsigned char *px, c[4];
    char log[2048];

    prog = linkVF(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "array-of-matrices program did not link: %s", log);

    if (!prog) return;

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) { glDeleteProgram(prog); SKIP("no target"); }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    mgl_target_bind(&t);
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, W / 2, H / 2, c);

        // 0.9, 0.8, 0.7, 0.3 as bytes, with a little room for rounding
        CHECK_MSG(abs(c[0] - 230) <= 2 && abs(c[1] - 204) <= 2 &&
                  abs(c[2] - 179) <= 2 && abs(c[3] - 77) <= 2,
                  "centre = %d,%d,%d,%d - want 230,204,179,77", c[0], c[1], c[2], c[3]);
        free(px);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- doubles across the stage interface ---------- */

// A double crosses as its raw bits, one uint2 per component. Arrays of them
// were given the emulation's struct type, which Metal will not carry, and
// every component took a location of its own, so a dvec2 trod on whatever
// came after it. Now two doubles share a location the way GL lays them out.
GPU_TEST(varying_layouts, doubles_cross_in_arrays_and_beside_other_varyings)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "layout(location = 0) flat out dvec2 d;\n"
        "layout(location = 1) flat out double s[2];\n"
        "layout(location = 3) flat out float after;\n"
        "void main() {\n"
        "  d = dvec2(0.25lf, 0.5lf); s[0] = 0.75lf; s[1] = 1.0lf; after = 0.125;\n"
        "  gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "layout(location = 0) flat in dvec2 d;\n"
        "layout(location = 1) flat in double s[2];\n"
        "layout(location = 3) flat in float after;\n"
        "out vec4 o;\n"
        "void main() {\n"
        "  // every value checked, and the float after them, so any slot read\n"
        "  // from its neighbour's location shows\n"
        "  bool ok = d.x == 0.25lf && d.y == 0.5lf && s[0] == 0.75lf && s[1] == 1.0lf && after == 0.125;\n"
        "  o = ok ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    static const float tri[] = { -1.0f, -1.0f,  3.0f, -1.0f,  -1.0f, 3.0f };
    MGLTestTarget t;
    GLuint prog, vao = 0, vbo = 0;
    unsigned char *px, c[4];
    char log[2048];

    prog = linkVF(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "double varying program did not link: %s", log);

    if (!prog) return;

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) { glDeleteProgram(prog); SKIP("no target"); }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    mgl_target_bind(&t);
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, W / 2, H / 2, c);
        CHECK_MSG(c[1] > 200 && c[0] < 60, "centre = %d,%d,%d - a double arrived wrong", c[0], c[1], c[2]);
        free(px);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- a vertex stage with no outputs at all ---------- */

// Writing only a buffer, the vertex function comes out of SPIRV-Cross
// returning void, and Metal refuses to build a pipeline that rasterises from
// one -- so the draw failed even though nothing was ever going to be drawn.
GPU_TEST(varying_layouts, a_vertex_stage_that_only_writes_a_buffer_still_runs)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(std430, binding = 0) buffer Hits { uint hits[]; };\n"
        "void main() { atomicAdd(hits[0], 1u); }\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    GLuint prog, vao = 0, ssbo = 0;
    GLuint zero = 0, got = 0;
    char log[2048];

    prog = linkVF(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "buffer-only vertex program did not link: %s", log);

    if (!prog) return;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, &zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glDrawArrays(GL_POINTS, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, &got);
    CHECK_MSG(got == 3, "the vertex stage ran %u times for 3 vertices", got);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ssbo);
}
