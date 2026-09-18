/*
 * test_tess_geometry.c
 * Copyright (C) The Moogle Project
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
 * All five stages in one program: point-mode isolines feeding a geometry
 * shader that turns each point into a square. Where the squares land says
 * which points the tessellator made.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

#ifndef GL_PATCHES
#define GL_PATCHES 0x000E
#endif

static const char *VS =
    "#version 430 core\n"
    "out vec4 vs_tcs;\n"
    "void main() { vs_tcs = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static const char *TCS =
    "#version 430 core\n"
    "layout(vertices = 1) out;\n"
    "in vec4 vs_tcs[];\n"
    "out vec4 tcs_tes[];\n"
    "uniform float segments;\n"
    "void main() {\n"
    "    tcs_tes[gl_InvocationID] = vs_tcs[0];\n"
    "    gl_TessLevelOuter[0] = 1.0;\n"
    "    gl_TessLevelOuter[1] = segments;\n"
    "    gl_TessLevelOuter[2] = 1.0;\n"
    "    gl_TessLevelOuter[3] = 1.0;\n"
    "    gl_TessLevelInner[0] = 1.0;\n"
    "    gl_TessLevelInner[1] = 1.0;\n"
    "}\n";

static const char *TES =
    "#version 430 core\n"
    "layout(isolines, point_mode) in;\n"
    "in vec4 tcs_tes[];\n"
    "out vec4 tes_gs;\n"
    "void main() {\n"
    "    tes_gs = tcs_tes[0];\n"
    "    gl_Position = vec4(gl_TessCoord.x * 1.6 - 0.8, 0.0, 0.0, 1.0);\n"
    "}\n";

// the evaluation stage alone, with the levels left to glPatchParameterfv
static const char *TES_ALONE =
    "#version 430 core\n"
    "layout(isolines, point_mode) in;\n"
    "out vec4 tes_gs;\n"
    "void main() {\n"
    "    tes_gs = vec4(0.0, 1.0, 0.0, 1.0);\n"
    "    gl_Position = vec4(gl_TessCoord.x * 1.6 - 0.8, 0.0, 0.0, 1.0);\n"
    "}\n";

static const char *GS =
    "#version 430 core\n"
    "layout(points) in;\n"
    "layout(triangle_strip, max_vertices = 4) out;\n"
    "in vec4 tes_gs[];\n"
    "out vec4 gs_fs;\n"
    "void main() {\n"
    "    vec4 c = gl_in[0].gl_Position;\n"
    "    for (int i = 0; i < 4; i++) {\n"
    "        vec2 d = vec2((i & 1) != 0 ? 0.1 : -0.1, (i & 2) != 0 ? 0.1 : -0.1);\n"
    "        gs_fs = tes_gs[0];\n"
    "        gl_Position = vec4(c.xy + d, 0.0, 1.0);\n"
    "        EmitVertex();\n"
    "    }\n"
    "    EndPrimitive();\n"
    "}\n";

static const char *FS =
    "#version 430 core\n"
    "in vec4 gs_fs;\n"
    "out vec4 o;\n"
    "void main() { o = gs_fs; }\n";

static GLuint linkAll(const char *tcs, const char *tes, char *log, int log_size)
{
    const char *srcs[5] = { VS, tcs, tes, GS, FS };
    const GLenum stages[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER,
                               GL_TESS_EVALUATION_SHADER, GL_GEOMETRY_SHADER,
                               GL_FRAGMENT_SHADER };
    GLuint prog = glCreateProgram();
    GLint ok = 0;

    log[0] = 0;

    for (int i = 0; i < 5; i++)
    {
        if (srcs[i] == NULL)
            continue;

        GLuint sh = glCreateShader(stages[i]);

        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

        if (!ok)
        {
            glGetShaderInfoLog(sh, log_size, NULL, log);
            return 0;
        }

        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok)
    {
        glGetProgramInfoLog(prog, log_size, NULL, log);
        glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

// Green at each clip x listed, and black halfway between neighbours.
static void checkSquares(const MGLTestTarget *t, const float *xs, int n)
{
    unsigned char *px = mgl_read_rgba8(t);
    unsigned char rgba[4];

    CHECK(px != NULL);
    if (px == NULL)
        return;

    for (int i = 0; i < n; i++)
    {
        int x = (int)((xs[i] * 0.5f + 0.5f) * (float)t->width);

        mgl_pixel_at(px, t, x, t->height / 2, rgba);
        CHECK_MSG(rgba[1] > 200 && rgba[0] < 60, "no square at clip x %.2f: %u %u %u",
                  xs[i], rgba[0], rgba[1], rgba[2]);

        if (i + 1 < n)
        {
            x = (int)(((xs[i] + xs[i + 1]) * 0.25f + 0.5f) * (float)t->width);
            mgl_pixel_at(px, t, x, t->height / 2, rgba);
            CHECK_MSG(rgba[1] < 60, "stray colour between squares at pixel %d: %u %u %u",
                      x, rgba[0], rgba[1], rgba[2]);
        }
    }

    free(px);
}

static void drawPatch(GLuint prog, const MGLTestTarget *t)
{
    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    mgl_target_bind(t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 1);
    glDrawArrays(GL_PATCHES, 0, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    glFinish();
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(tess_geometry, isoline_points_reach_the_geometry_shader)
{
    char log[4096];
    MGLTestTarget t;
    GLuint prog = linkAll(TCS, TES, log, sizeof log);

    CHECK_MSG(prog != 0, "five-stage program did not link: %s", log);
    if (!prog)
        return;

    CHECK(mgl_target_create(&t, 64, 16, GL_RGBA8, 0));

    // one segment: the two ends of the line
    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "segments"), 1.0f);
    drawPatch(prog, &t);
    checkSquares(&t, (const float[]){ -0.8f, 0.8f }, 2);

    // two segments add the midpoint
    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "segments"), 2.0f);
    drawPatch(prog, &t);
    checkSquares(&t, (const float[]){ -0.8f, 0.0f, 0.8f }, 3);

    glUseProgram(0);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(tess_geometry, default_levels_without_a_control_shader)
{
    char log[4096];
    MGLTestTarget t;
    GLuint prog = linkAll(NULL, TES_ALONE, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return;

    CHECK(mgl_target_create(&t, 64, 16, GL_RGBA8, 0));

    GLfloat outer[4] = { 1.0f, 2.0f, 1.0f, 1.0f };

    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
    drawPatch(prog, &t);
    checkSquares(&t, (const float[]){ -0.8f, 0.0f, 0.8f }, 3);

    outer[1] = 1.0f;
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);

    glUseProgram(0);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// Point-mode isolines with no geometry shader, recorded by transform feedback.
static const char *TES_POINTS =
    "#version 430 core\n"
    "layout(isolines, point_mode) in;\n"
    "out float u;\n"
    "void main() {\n"
    "    u = gl_TessCoord.x;\n"
    "    gl_Position = vec4(gl_TessCoord.x, 0.0, 0.0, 1.0);\n"
    "}\n";

static const char *FS_U =
    "#version 430 core\n"
    "in float u;\n"
    "out vec4 o;\n"
    "void main() { o = vec4(u); }\n";

GPU_TEST(tess_geometry, isoline_points_are_recorded_without_a_geometry_shader)
{
    const char *srcs[3] = { VS, TES_POINTS, FS_U };
    const GLenum stages[3] = { GL_VERTEX_SHADER, GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER };
    const char *names[] = { "u" };
    char log[4096] = "";
    GLuint prog = glCreateProgram();
    GLint ok = 0;

    for (int i = 0; i < 3; i++)
    {
        GLuint sh = glCreateShader(stages[i]);

        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glTransformFeedbackVaryings(prog, 1, names, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok)
        glGetProgramInfoLog(prog, sizeof log, NULL, log);

    CHECK_MSG(ok, "program did not link: %s", log);
    if (!ok)
    {
        glDeleteProgram(prog);
        return;
    }

    GLfloat outer[4] = { 1.0f, 2.0f, 1.0f, 1.0f };
    GLfloat got[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
    GLuint buf = 0, vao = 0;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof got, got, GL_DYNAMIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 1);
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_PATCHES, 0, 1);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);

    // three points along the one line, and nothing written past them
    CHECK_MSG(got[0] == 0.0f && got[1] == 0.5f && got[2] == 1.0f && got[3] == -1.0f,
              "recorded %g %g %g %g, want 0 0.5 1 then untouched", got[0], got[1], got[2], got[3]);

    outer[1] = 1.0f;
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

// A geometry shader lays out its own capture with xfb_offset, across two buffers.
GPU_TEST(tess_geometry, geometry_shader_records_its_own_layout)
{
    static const char *vs =
        "#version 430 core\n"
        "layout(location = 0) in float id;\n"
        "out float vs_id;\n"
        "void main() { vs_id = id; gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 440 core\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 2) out;\n"
        "in float vs_id[];\n"
        "layout(xfb_buffer = 0, xfb_offset = 4) out float first;\n"
        "layout(xfb_buffer = 0, xfb_offset = 0) out float second;\n"
        "layout(xfb_buffer = 1, xfb_offset = 0) out vec2 pair;\n"
        "void main() {\n"
        "    for (int i = 0; i < 2; i++) {\n"
        "        first = vs_id[0] * 10.0 + float(i);\n"
        "        second = -first;\n"
        "        pair = vec2(first, 100.0);\n"
        "        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "        EmitVertex();\n"
        "    }\n"
        "}\n";
    static const char *fs =
        "#version 430 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    const char *srcs[3] = { vs, gs, fs };
    const GLenum stages[3] = { GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    char log[4096] = "";
    GLuint prog = glCreateProgram();
    GLint ok = 0;

    for (int i = 0; i < 3; i++)
    {
        GLuint sh = glCreateShader(stages[i]);

        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok)
        glGetProgramInfoLog(prog, sizeof log, NULL, log);

    CHECK_MSG(ok, "program did not link: %s", log);
    if (!ok)
    {
        glDeleteProgram(prog);
        return;
    }

    static const GLfloat ids[2] = { 1.0f, 2.0f };
    GLfloat a[8], b[8];
    GLuint bufs[3], vao = 0;

    for (int i = 0; i < 8; i++)
        a[i] = b[i] = -1.0f;

    glGenBuffers(3, bufs);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, bufs[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof ids, ids, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, NULL);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, bufs[0]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof a, a, GL_DYNAMIC_READ);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, bufs[1]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof b, b, GL_DYNAMIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, bufs[0]);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, bufs[1]);

    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    glDrawArrays(GL_POINTS, 0, 2);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, bufs[0]);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof a, a);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, bufs[1]);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof b, b);

    // buffer 0 holds (second, first) per vertex; buffer 1 holds pair
    static const GLfloat want_a[8] = { -10, 10, -11, 11, -20, 20, -21, 21 };
    static const GLfloat want_b[8] = { 10, 100, 11, 100, 20, 100, 21, 100 };

    for (int i = 0; i < 8; i++)
    {
        CHECK_MSG(a[i] == want_a[i], "buffer 0 word %d is %g, want %g", i, a[i], want_a[i]);
        CHECK_MSG(b[i] == want_b[i], "buffer 1 word %d is %g, want %g", i, b[i], want_b[i]);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(3, bufs);
    glDeleteVertexArrays(1, &vao);
}
