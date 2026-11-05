/*
 * test_tess_geometry.c
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

// Every stage reads an image by its declared unit and records what it saw at
// its own texel of a second image. None may see an empty slot.
#define IMG_DECL "#version 430 core\n" \
                 "layout(binding = 1, rgba8) uniform image2D seen;\n" \
                 "layout(binding = 2, rgba8) uniform image2D goku;\n"
#define IMG_READ(k) "    imageStore(seen, ivec2(" #k ", 0), imageLoad(goku, ivec2(0)));\n"

GPU_TEST(tess_geometry, every_stage_reads_its_images)
{
    const char *srcs[5] = {
        IMG_DECL "out vec4 vs_tcs;\n"
        "void main() {\n" IMG_READ(0) "    vs_tcs = vec4(0, 1, 0, 1);\n}\n",

        IMG_DECL "layout(vertices = 1) out;\n"
        "in vec4 vs_tcs[];\nout vec4 tcs_tes[];\n"
        "void main() {\n" IMG_READ(1)
        "    tcs_tes[gl_InvocationID] = vs_tcs[0];\n"
        "    gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0;\n"
        "    gl_TessLevelOuter[2] = 1.0; gl_TessLevelOuter[3] = 1.0;\n"
        "    gl_TessLevelInner[0] = 1.0; gl_TessLevelInner[1] = 1.0;\n}\n",

        IMG_DECL "layout(isolines, point_mode) in;\n"
        "in vec4 tcs_tes[];\nout vec4 tes_gs;\n"
        "void main() {\n" IMG_READ(2) "    tes_gs = tcs_tes[0];\n}\n",

        IMG_DECL "layout(points) in;\nlayout(triangle_strip, max_vertices = 4) out;\n"
        "in vec4 tes_gs[];\nout vec4 gs_fs;\n"
        "void main() {\n" IMG_READ(3)
        "    gs_fs = tes_gs[0]; gl_Position = vec4(-1, -1, 0, 1); EmitVertex();\n"
        "    gs_fs = tes_gs[0]; gl_Position = vec4(-1,  1, 0, 1); EmitVertex();\n"
        "    gs_fs = tes_gs[0]; gl_Position = vec4( 1, -1, 0, 1); EmitVertex();\n"
        "    gs_fs = tes_gs[0]; gl_Position = vec4( 1,  1, 0, 1); EmitVertex();\n}\n",

        IMG_DECL "in vec4 gs_fs;\nout vec4 color;\n"
        "void main() {\n" IMG_READ(4) "    color = gs_fs;\n}\n",
    };
    const GLenum kinds[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                              GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    const char *names[5] = { "vertex", "control", "evaluation", "geometry", "fragment" };
    GLuint prog = glCreateProgram();
    GLint ok = 0;
    char log[4096] = "";

    for (int i = 0; i < 5; i++)
    {
        GLuint sh = glCreateShader(kinds[i]);

        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok)
            glGetShaderInfoLog(sh, sizeof log, NULL, log);
        CHECK_MSG(ok, "%s did not compile: %s", names[i], log);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
    CHECK_MSG(ok, "did not link: %s", log);
    if (!ok)
    {
        glDeleteProgram(prog);
        return;
    }

    GLuint tex[2], zero[8] = {0}, red[1] = {0x000000ffu}, got[8] = {0};
    MGLTestTarget t;

    glGenTextures(2, tex);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 1, GL_RGBA, GL_UNSIGNED_BYTE, zero);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, red);
    glBindImageTexture(1, tex[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    glBindImageTexture(2, tex[1], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

    CHECK(mgl_target_create(&t, 16, 16, GL_RGBA8, 0));
    drawPatch(prog, &t);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);

    for (int i = 0; i < 5; i++)
        CHECK_MSG(got[i] == red[0], "%s stage read %08x", names[i], got[i]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(2, tex);
    mgl_target_destroy(&t);
}

// Each stage reads the layer count of a multisampled array image and a border
// colour Metal samplers cannot hold
#define PARAM_DECL "#version 430 core\n" \
                   "layout(binding = 1, rgba32i) uniform iimage2D seen;\n" \
                   "layout(binding = 2, rgba32i) uniform iimage2DMSArray ms;\n" \
                   "uniform sampler2D bord;\n"
#define PARAM_READ(k) "    imageStore(seen, ivec2(" #k ", 0), ivec4(imageSize(ms), int(textureLod(bord, vec2(-1.0), 0.0).r * 100.0 + 0.5)));\n"

GPU_TEST(tess_geometry, every_stage_reads_its_texture_params)
{
    const char *srcs[5] = {
        PARAM_DECL "out vec4 vs_tcs;\n"
        "void main() {\n" PARAM_READ(0) "    vs_tcs = vec4(0, 1, 0, 1);\n}\n",

        PARAM_DECL "layout(vertices = 1) out;\n"
        "in vec4 vs_tcs[];\nout vec4 tcs_tes[];\n"
        "void main() {\n" PARAM_READ(1)
        "    tcs_tes[gl_InvocationID] = vs_tcs[0];\n"
        "    gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0;\n"
        "    gl_TessLevelOuter[2] = 1.0; gl_TessLevelOuter[3] = 1.0;\n"
        "    gl_TessLevelInner[0] = 1.0; gl_TessLevelInner[1] = 1.0;\n}\n",

        PARAM_DECL "layout(isolines, point_mode) in;\n"
        "in vec4 tcs_tes[];\nout vec4 tes_gs;\n"
        "void main() {\n" PARAM_READ(2) "    tes_gs = tcs_tes[0];\n}\n",

        PARAM_DECL "layout(points) in;\nlayout(triangle_strip, max_vertices = 4) out;\n"
        "in vec4 tes_gs[];\nout vec4 gs_fs;\n"
        "void main() {\n" PARAM_READ(3)
        "    gs_fs = tes_gs[0]; gl_Position = vec4(-1, -1, 0, 1); EmitVertex();\n"
        "    gs_fs = tes_gs[0]; gl_Position = vec4(-1,  1, 0, 1); EmitVertex();\n"
        "    gs_fs = tes_gs[0]; gl_Position = vec4( 1, -1, 0, 1); EmitVertex();\n"
        "    gs_fs = tes_gs[0]; gl_Position = vec4( 1,  1, 0, 1); EmitVertex();\n}\n",

        PARAM_DECL "in vec4 gs_fs;\nout vec4 color;\n"
        "void main() {\n" PARAM_READ(4) "    color = gs_fs;\n}\n",
    };
    const GLenum kinds[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                              GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    const char *names[5] = { "vertex", "control", "evaluation", "geometry", "fragment" };
    GLuint prog = glCreateProgram();
    GLint ok = 0;
    char log[4096] = "";

    for (int i = 0; i < 5; i++)
    {
        GLuint sh = glCreateShader(kinds[i]);

        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok)
            glGetShaderInfoLog(sh, sizeof log, NULL, log);
        CHECK_MSG(ok, "%s did not compile: %s", names[i], log);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
    CHECK_MSG(ok, "did not link: %s", log);
    if (!ok)
    {
        glDeleteProgram(prog);
        return;
    }

    static const GLfloat border[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
    GLuint tex[3];
    GLint zero[32] = { 0 }, got[32] = { 0 };
    GLubyte white[4] = { 255, 255, 255, 255 };
    MGLTestTarget t;

    CHECK(mgl_target_create(&t, 16, 16, GL_RGBA8, 0));
    glGenTextures(3, tex);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32I, 8, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 1, GL_RGBA_INTEGER, GL_INT, zero);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, tex[1]);
    glTexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 4, GL_RGBA32I, 3, 5, 2, GL_FALSE);
    glBindTexture(GL_TEXTURE_2D, tex[2]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindImageTexture(1, tex[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32I);
    glBindImageTexture(2, tex[1], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32I);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "bord"), 0);
    drawPatch(prog, &t);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);

    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA_INTEGER, GL_INT, got);

    for (int i = 0; i < 5; i++)
    {
        GLint *g = &got[i * 4];

        CHECK_MSG(g[0] == 3 && g[1] == 5 && g[2] == 2 && g[3] == 50,
                  "%s stage read size %d %d %d, border %d; want 3 5 2, 50", names[i], g[0], g[1], g[2], g[3]);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(3, tex);
    mgl_target_destroy(&t);
}

// A rectangle sampler keeps the evaluation stage's body in a helper
// function, which then has to take the stage's input the way it was declared.
static const char *TES_RECT =
    "#version 430 core\n"
    "layout(isolines, point_mode) in;\n"
    "in vec4 tcs_tes[];\n"
    "out vec4 tes_gs;\n"
    "uniform sampler2DRect rect;\n"
    "void main() {\n"
    "    tes_gs = tcs_tes[0] + texture(rect, vec2(0.0)) * 0.0;\n"
    "    gl_Position = vec4(gl_TessCoord.x * 1.6 - 0.8, 0.0, 0.0, 1.0);\n"
    "}\n";

GPU_TEST(tess_geometry, evaluation_stage_samples_a_rectangle)
{
    char log[4096];
    MGLTestTarget t;
    GLuint prog = linkAll(TCS, TES_RECT, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return;

    CHECK(mgl_target_create(&t, 64, 16, GL_RGBA8, 0));
    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "segments"), 1.0f);
    drawPatch(prog, &t);
    checkSquares(&t, (const float[]){ -0.8f, 0.8f }, 2);

    glUseProgram(0);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// A vertex shader that writes gl_PointSize, which the control shader never
// reads, must not shift the control stage's view of later patches.
GPU_TEST(tess_geometry, vertex_point_size_keeps_control_inputs_aligned)
{
    static const char *vs =
        "#version 440\n"
        "layout(location = 0) in vec4 p;\n"
        "layout(location = 1) in vec4 c;\n"
        "out vec4 v_c;\n"
        "void main() { gl_PointSize = 1.0; gl_Position = p; v_c = c; }\n";
    static const char *tcs =
        "#version 440\n"
        "layout(vertices = 1) out;\n"
        "in vec4 v_c[];\n"
        "out vec4 t_c[];\n"
        "void main() {\n"
        "    gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0;\n"
        "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "    t_c[gl_InvocationID] = v_c[gl_InvocationID];\n"
        "}\n";
    static const char *tes =
        "#version 440\n"
        "layout(isolines, point_mode) in;\n"
        "in vec4 t_c[];\n"
        "out vec4 f_c;\n"
        "void main() { gl_Position = gl_in[0].gl_Position; f_c = t_c[0]; }\n";
    static const char *fs =
        "#version 440\n"
        "in vec4 f_c;\n"
        "out vec4 o;\n"
        "void main() { o = f_c; }\n";
    const char *srcs[4] = { vs, tcs, tes, fs };
    const GLenum stages[4] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                               GL_FRAGMENT_SHADER };
    enum { W = 8 };
    static GLfloat v[W * W * 8];
    MGLTestTarget t;
    GLuint prog = glCreateProgram(), vao, vbo;
    unsigned char *px;
    char log[4096] = "";
    GLint ok = 0;
    int bad = 0;

    for (int i = 0; i < 4; i++)
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
    if (!ok || !mgl_target_create(&t, W, W, GL_RGBA8, 0))
    {
        glDeleteProgram(prog);
        return;
    }

    // one point per pixel, odd pixels red and even ones green
    for (int i = 0; i < W * W; i++)
    {
        GLfloat *q = v + i * 8;

        q[0] = ((i % W) + 0.5f) / W * 2 - 1;
        q[1] = ((i / W) + 0.5f) / W * 2 - 1;
        q[2] = 0; q[3] = 1;
        q[4] = (GLfloat)(i & 1); q[5] = (GLfloat)!(i & 1); q[6] = 0; q[7] = 1;
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 32, (void *)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 32, (void *)16);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 1);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, W * W);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    for (int i = 0; i < W * W; i++)
        bad += px[i * 4] != ((i & 1) ? 255 : 0) || px[i * 4 + 1] != ((i & 1) ? 0 : 255) || px[i * 4 + 2] != 0;
    free(px);
    CHECK_MSG(bad == 0, "%d of %d points came out wrong", bad, W * W);

    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);
}

// the geometry stage reads clip distances the evaluation stage wrote
GPU_TEST(tess_geometry, geometry_reads_evaluation_clip_distances)
{
    static const char *srcs[5] = {
        "#version 450\n"
        "void main() { gl_Position = vec4(0, 0, 0, 1); }\n",
        "#version 450\n"
        "layout(vertices = 3) out;\n"
        "void main() {\n"
        "    gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0;\n"
        "    gl_TessLevelInner[0] = 1.0;\n"
        "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "}\n",
        "#version 450\n"
        "layout(triangles) in;\n"
        "void main() {\n"
        "    gl_ClipDistance[0] = 1.0;\n"
        "    gl_Position = gl_in[0].gl_Position;\n"
        "}\n",
        "#version 450\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; ++i) {\n"
        "        gl_ClipDistance[0] = gl_in[i].gl_ClipDistance[0];\n"
        "        gl_Position = gl_in[i].gl_Position;\n"
        "        EmitVertex();\n"
        "    }\n"
        "}\n",
        "#version 450\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n" };
    const GLenum stages[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                               GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    GLuint prog = glCreateProgram();
    GLint ok = 0;
    char log[1024] = "";

    for (int i = 0; i < 5; i++)
    {
        GLuint sh = glCreateShader(stages[i]);

        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(prog, sizeof log, NULL, log);
    CHECK_MSG(ok == GL_TRUE, "did not link: %s", log);
    glDeleteProgram(prog);
    mgl_drain_errors();
}

// An evaluation stage whose primitives the geometry stage does not take
// still links, with every stage's uniforms active; only drawing fails
GPU_TEST(tess_geometry, primitive_mismatch_keeps_uniforms_active)
{
    static const char *srcs[5] = {
        "#version 400\nuniform float vu;\nvoid main() { gl_Position = vec4(vu); }\n",
        "#version 400\nlayout(vertices = 3) out;\nuniform float tcu;\n"
        "void main() { gl_TessLevelOuter[0] = tcu; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0;\n"
        "  gl_TessLevelInner[0] = 1.0; gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position; }\n",
        "#version 400\nlayout(triangles) in;\nuniform float teu;\nvoid main() { gl_Position = gl_in[0].gl_Position + vec4(teu); }\n",
        "#version 400\nlayout(points) in;\nlayout(points, max_vertices = 1) out;\nuniform float gu;\n"
        "void main() { gl_Position = gl_in[0].gl_Position + vec4(gu); EmitVertex(); }\n",
        "#version 400\nuniform float fu;\nout vec4 o;\nvoid main() { o = vec4(fu); }\n" };
    const GLenum stages[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                               GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    static const char *names[5] = { "vu", "tcu", "teu", "gu", "fu" };
    GLuint prog = glCreateProgram(), vao;
    GLint ok = 0;

    for (int i = 0; i < 5; i++)
    {
        GLuint sh = glCreateShader(stages[i]);

        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_TRUE);

    for (int i = 0; i < 5; i++)
        CHECK_MSG(glGetUniformLocation(prog, names[i]) >= 0, "%s has no location", names[i]);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    mgl_drain_errors();
    glDrawArrays(GL_PATCHES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}

// gl_VertexID includes the base vertex when the vertex stage feeds
// tessellation or a geometry stage
GPU_TEST(tess_geometry, base_vertex_reaches_gl_vertex_id)
{
    static const char *vs =
        "#version 430 core\n"
        "layout(location = 0) in vec4 vertex;\n"
        "out vec4 c;\n"
        "void main() { c = vec4(float(gl_VertexID) / 16.0, 0.0, 0.0, 1.0); gl_Position = vertex; }\n";
    static const char *tcs =
        "#version 430 core\n"
        "layout(vertices = 3) out;\n"
        "in vec4 c[];\nout vec4 tc[];\n"
        "void main() { gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "  tc[gl_InvocationID] = c[gl_InvocationID];\n"
        "  gl_TessLevelInner[0] = 1.0; gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0; }\n";
    static const char *tes =
        "#version 430 core\n"
        "layout(triangles, equal_spacing, cw) in;\n"
        "in vec4 tc[];\nout vec4 c;\n"
        "void main() { c = (tc[0] + tc[1] + tc[2]) / 3.0;\n"
        "  gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position; }\n";
    static const char *gs =
        "#version 430 core\n"
        "layout(triangles) in;\nlayout(triangle_strip, max_vertices = 3) out;\n"
        "in vec4 c[];\nout vec4 g;\n"
        "void main() { for (int n = 0; n < 3; n++) { gl_Position = gl_in[n].gl_Position; g = c[n]; EmitVertex(); } EndPrimitive(); }\n";
    static const char *fs_c = "#version 430 core\nin vec4 c;\nout vec4 o;\nvoid main() { o = c; }\n";
    static const char *fs_g = "#version 430 core\nin vec4 g;\nout vec4 o;\nvoid main() { o = g; }\n";
    static const GLuint idx[6] = { 0, 1, 2, 10, 11, 12 };
    GLfloat verts[16 * 4] = { 0 };
    MGLTestTarget target;
    GLuint vao, vbo, ibo;

    for (int v = 0; v < 16; v++)
        verts[v * 4 + 3] = 1.0f;
    // only vertices 10, 11 and 12 make a triangle over the target
    verts[10 * 4 + 0] = -3.0f; verts[10 * 4 + 1] = -1.0f;
    verts[11 * 4 + 0] =  1.0f; verts[11 * 4 + 1] =  3.0f;
    verts[12 * 4 + 0] =  1.0f; verts[12 * 4 + 1] = -1.0f;

    CHECK(mgl_target_create(&target, 4, 4, GL_RGBA8, 0));
    mgl_target_bind(&target);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    for (int combo = 1; combo < 4; combo++)
    {
        int tess = (combo & 2) != 0, geom = (combo & 1) != 0;
        const char *srcs[5] = { vs, tess ? tcs : NULL, tess ? tes : NULL, geom ? gs : NULL, geom ? fs_g : fs_c };
        const GLenum stages[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                                   GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
        GLuint prog = glCreateProgram();
        GLint ok = 0;
        GLubyte px[4] = { 0 };
        unsigned char *img;

        for (int i = 0; i < 5; i++)
        {
            if (!srcs[i])
                continue;
            GLuint sh = glCreateShader(stages[i]);
            glShaderSource(sh, 1, &srcs[i], NULL);
            glCompileShader(sh);
            glAttachShader(prog, sh);
            glDeleteShader(sh);
        }
        glLinkProgram(prog);
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        CHECK_MSG(ok, "combination %d did not link", combo);

        glUseProgram(prog);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElementsBaseVertex(tess ? GL_PATCHES : GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL, 10);
        img = mgl_read_rgba8(&target);
        mgl_pixel_at(img, &target, 2, 2, px);
        free(img);
        // ids 10 to 12 average to 11/16
        CHECK_MSG(px[0] > 160 && px[0] < 190, "tess %d geometry %d drew red %u, want about 175", tess, geom, px[0]);

        // the same three vertices named directly, from later in the index buffer
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawElements(tess ? GL_PATCHES : GL_TRIANGLES, 3, GL_UNSIGNED_INT, (const void *)(3 * sizeof(GLuint)));
        img = mgl_read_rgba8(&target);
        mgl_pixel_at(img, &target, 2, 2, px);
        free(img);
        CHECK_MSG(px[0] > 160 && px[0] < 190, "tess %d geometry %d offset draw red %u, want about 175", tess, geom, px[0]);

        // 16 bit indices
        {
            static const GLushort sidx[3] = { 10, 11, 12 };
            GLuint sbo;

            glGenBuffers(1, &sbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sbo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof sidx, sidx, GL_STATIC_DRAW);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawElements(tess ? GL_PATCHES : GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL);
            img = mgl_read_rgba8(&target);
            mgl_pixel_at(img, &target, 2, 2, px);
            free(img);
            CHECK_MSG(px[0] > 160 && px[0] < 190, "tess %d geometry %d short-index draw red %u, want about 175",
                      tess, geom, px[0]);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            glDeleteBuffers(1, &sbo);
        }

        // and once more as the only draw of a multi-draw
        {
            const GLsizei n = 3;
            const void *at = NULL;
            const GLint base = 10;

            glClear(GL_COLOR_BUFFER_BIT);
            glMultiDrawElementsBaseVertex(tess ? GL_PATCHES : GL_TRIANGLES, &n, GL_UNSIGNED_INT, &at, 1, &base);
            CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
            img = mgl_read_rgba8(&target);
            mgl_pixel_at(img, &target, 2, 2, px);
            free(img);
            CHECK_MSG(px[0] > 160 && px[0] < 190, "tess %d geometry %d multi-draw red %u, want about 175", tess, geom, px[0]);
        }

        glUseProgram(0);
        glDeleteProgram(prog);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&target);
}

static void layerAndViewport(const char *const srcs[5], const char *what)
{
    static const GLfloat verts[9] = { -1, -1, 0,  1, -1, 0,  0, 1, 0 };
    const GLenum stages[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                               GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    static GLuint px[64 * 64 * 2];
    GLuint prog = glCreateProgram(), tex, fbo, vao, vbo;
    GLint ok = 0;

    for (int i = 0; i < 5; i++)
    {
        GLuint sh = glCreateShader(stages[i]);
        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK_MSG(ok, "%s did not link", what);
    if (!ok)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 64, 64, 2);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    glViewportIndexedf(0, 0.0f, 0.0f, 32.0f, 64.0f);
    glViewportIndexedf(1, 32.0f, 0.0f, 32.0f, 64.0f);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glDrawArrays(GL_PATCHES, 0, 3);

    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_MSG(px[48 + 32 * 64 + 64 * 64] == 0xFF00FFFFu, "%s: layer 1 viewport 1 read %08x, want yellow",
              what, px[48 + 32 * 64 + 64 * 64]);
    CHECK_MSG(px[48 + 32 * 64] == 0xFF000000u, "layer 0 read %08x, want black", px[48 + 32 * 64]);
    CHECK_MSG(px[16 + 32 * 64 + 64 * 64] == 0xFF000000u, "viewport 0 of layer 1 read %08x, want black",
              px[16 + 32 * 64 + 64 * 64]);

    glViewport(0, 0, 64, 64);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}

// A geometry stage after tessellation picks the layer and the viewport, also
// with the stages as SPIRV-Cross writes them back from SPIR-V, which redeclare
// gl_PerVertex and the distance arrays
GPU_TEST(tess_geometry, geometry_after_tessellation_picks_layer_and_viewport)
{
    static const char *const vs =
        "#version 450\nlayout(location = 0) in vec3 position;\nlayout(location = 1) out vec4 vColor;\n"
        "void main() { gl_PointSize = 10.0; gl_Position = vec4(position, 1.0); gl_ClipDistance[0] = 1.0; gl_CullDistance[0] = 1.0; vColor = vec4(1.0); }\n";
    static const char *const tcs =
        "#version 450\nlayout(vertices = 3) out;\nlayout(location = 1) in vec4 vColor[];\nlayout(location = 2) out vec4 tcColor[];\n"
        "void main() { tcColor[gl_InvocationID] = vColor[gl_InvocationID]; tcColor[gl_InvocationID].r = float(gl_PatchVerticesIn) / 3;\n"
        "  if (gl_InvocationID == 0) { gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0; gl_TessLevelInner[0] = 1.0; }\n"
        "  gl_out[gl_InvocationID].gl_ClipDistance[0] = gl_in[gl_InvocationID].gl_ClipDistance[0];\n"
        "  gl_out[gl_InvocationID].gl_CullDistance[0] = gl_in[gl_InvocationID].gl_CullDistance[0];\n"
        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position; }\n";
    static const char *const tes =
        "#version 450\nlayout(triangles) in;\nlayout(location = 2) in vec4 tcColor[];\nlayout(location = 3) out vec4 teColor;\n"
        "void main() { teColor = tcColor[0]; gl_ClipDistance[0] = gl_in[0].gl_ClipDistance[0]; gl_CullDistance[0] = gl_in[0].gl_CullDistance[0];\n"
        "  gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position; }\n";
    static const char *const gs =
        "#version 450\nlayout(triangles) in;\nlayout(triangle_strip, max_vertices = 3) out;\n"
        "layout(location = 3) in vec4 teColor[];\nlayout(location = 4) out vec4 gColor;\n"
        "void main() { gColor = teColor[0]; gColor.b = float(gl_PrimitiveIDIn); gl_Layer = 1; gl_ViewportIndex = 1;\n"
        "  for (int i = 0; i < 3; ++i) { gl_ClipDistance[0] = gl_in[i].gl_ClipDistance[0]; gl_CullDistance[0] = gl_in[i].gl_CullDistance[0];\n"
        "    gl_Position = gl_in[i].gl_Position; EmitVertex(); } EndPrimitive(); }\n";
    static const char *const fs =
        "#version 450\nlayout(location = 4) in vec4 gColor;\nlayout(location = 0) out vec4 o;\nvoid main() { o = gColor; }\n";
    static const char *const vs_rt =
        "#version 460\nout float gl_ClipDistance[1];\nout float gl_CullDistance[1];\n"
        "layout(location = 0) in vec3 position;\nlayout(location = 1) out vec4 vColor;\n"
        "void main() { gl_PointSize = 10.0; gl_Position = vec4(position, 1.0); gl_ClipDistance[0] = 1.0; gl_CullDistance[0] = 1.0; vColor = vec4(1.0); }\n";
    static const char *const tcs_rt =
        "#version 460\nlayout(vertices = 3) out;\n"
        "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; float gl_ClipDistance[1]; float gl_CullDistance[1]; } gl_out[3];\n"
        "layout(location = 2) out vec4 tcColor[3];\nlayout(location = 1) in vec4 vColor[];\n"
        "void main() { tcColor[gl_InvocationID] = vColor[gl_InvocationID]; tcColor[gl_InvocationID].x = float(gl_PatchVerticesIn) / 3.0;\n"
        "  if (gl_InvocationID == 0) { gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0; gl_TessLevelInner[0] = 1.0; }\n"
        "  gl_out[gl_InvocationID].gl_ClipDistance[0] = gl_in[gl_InvocationID].gl_ClipDistance[0];\n"
        "  gl_out[gl_InvocationID].gl_CullDistance[0] = gl_in[gl_InvocationID].gl_CullDistance[0];\n"
        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position; }\n";
    static const char *const tes_rt =
        "#version 460\nlayout(triangles, ccw, equal_spacing) in;\nout float gl_ClipDistance[1];\nout float gl_CullDistance[1];\n"
        "layout(location = 3) out vec4 teColor;\nlayout(location = 2) in vec4 tcColor[];\n"
        "void main() { teColor = tcColor[0]; gl_ClipDistance[0] = gl_in[0].gl_ClipDistance[0]; gl_CullDistance[0] = gl_in[0].gl_CullDistance[0];\n"
        "  gl_Position = gl_in[0].gl_Position * gl_TessCoord.x + gl_in[1].gl_Position * gl_TessCoord.y + gl_in[2].gl_Position * gl_TessCoord.z; }\n";
    static const char *const gs_rt =
        "#version 460\nlayout(triangles) in;\nlayout(max_vertices = 3, triangle_strip) out;\n"
        "out float gl_ClipDistance[1];\nout float gl_CullDistance[1];\n"
        "layout(location = 4) out vec4 gColor;\nlayout(location = 3) in vec4 teColor[3];\n"
        "void main() { gColor = teColor[0]; gColor.z = float(gl_PrimitiveIDIn); gl_Layer = 1; gl_ViewportIndex = 1;\n"
        "  for (int i = 0; i < 3; i++) { gl_ClipDistance[0] = gl_in[i].gl_ClipDistance[0]; gl_CullDistance[0] = gl_in[i].gl_CullDistance[0];\n"
        "    gl_Position = gl_in[i].gl_Position; EmitVertex(); } EndPrimitive(); }\n";
    static const char *const fs_rt =
        "#version 460\nlayout(location = 0) out vec4 fColor;\nlayout(location = 4) in vec4 gColor;\nvoid main() { fColor = gColor; }\n";
    const char *const plain[5] = { vs, tcs, tes, gs, fs };
    const char *const round_trip[5] = { vs_rt, tcs_rt, tes_rt, gs_rt, fs_rt };

    layerAndViewport(plain, "GLSL");
    layerAndViewport(round_trip, "SPIR-V round trip");
}

// The pipelines a geometry or tessellation draw builds are remade when the
// blend state or the vertex layout changes under the same program
GPU_TEST(tess_geometry, cached_pipelines_follow_blend_and_layout)
{
    static const char *vs =
        "#version 430 core\nlayout(location = 0) in vec2 p;\nvoid main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 430 core\nlayout(triangles) in;\nlayout(triangle_strip, max_vertices = 3) out;\n"
        "void main() { for (int i = 0; i < 3; i++) { gl_Position = gl_in[i].gl_Position; EmitVertex(); } EndPrimitive(); }\n";
    static const char *tcs =
        "#version 430 core\nlayout(vertices = 3) out;\n"
        "void main() { gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "  gl_TessLevelInner[0] = 1.0; gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0; }\n";
    static const char *tes =
        "#version 430 core\nlayout(triangles) in;\n"
        "void main() { gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position + gl_TessCoord.z * gl_in[2].gl_Position; }\n";
    static const char *fs =
        "#version 430 core\nout vec4 o;\nvoid main() { o = vec4(0.25, 0.0, 0.0, 1.0); }\n";
    // one big triangle, as tight pairs and then spread out with padding
    static const GLfloat tight[6] = { -1, -1, 3, -1, -1, 3 };
    static const GLfloat spread[12] = { -1, -1, -9, -9, 3, -1, -9, -9, -1, 3, -9, -9 };
    MGLTestTarget target;
    GLuint vao, vbo[2];

    CHECK(mgl_target_create(&target, 4, 4, GL_RGBA8, 0));
    mgl_target_bind(&target);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof tight, tight, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof spread, spread, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    for (int tess = 0; tess < 2; tess++)
    {
        const char *srcs[5] = { vs, tess ? tcs : NULL, tess ? tes : NULL, tess ? NULL : gs, fs };
        const GLenum stages[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                                   GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
        GLuint prog = glCreateProgram();
        GLenum mode = tess ? GL_PATCHES : GL_TRIANGLES;
        GLubyte px[4];
        unsigned char *img;

        for (int i = 0; i < 5; i++)
        {
            if (!srcs[i])
                continue;
            GLuint sh = glCreateShader(stages[i]);
            glShaderSource(sh, 1, &srcs[i], NULL);
            glCompileShader(sh);
            glAttachShader(prog, sh);
            glDeleteShader(sh);
        }
        glLinkProgram(prog);
        glUseProgram(prog);

        glClear(GL_COLOR_BUFFER_BIT);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glDrawArrays(mode, 0, 3);

        // the same program again, adding, and reading the spread-out copy
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), NULL);
        glDrawArrays(mode, 0, 3);
        glDisable(GL_BLEND);

        img = mgl_read_rgba8(&target);
        mgl_pixel_at(img, &target, 2, 2, px);
        free(img);
        CHECK_MSG(px[0] > 120 && px[0] < 136, "%s: two draws added up to %u, want about 128",
                  tess ? "tessellation" : "geometry", px[0]);

        glUseProgram(0);
        glDeleteProgram(prog);
    }

    glDeleteBuffers(2, vbo);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&target);
}

// A varying a stage reads and passes on under the next stage's name, matched
// by location, links -- the shape SPIR-V comes back from SPIRV-Cross in
GPU_TEST(tess_geometry, same_location_names_pass_through_every_stage)
{
    static const char *vs =
        "#version 460\nlayout(location = 0) in vec3 position;\nlayout(location = 3) out vec2 texCoord;\n"
        "void main() { gl_Position = vec4(position, 1.0); texCoord = vec2(0.25, 0.75); }\n";
    static const char *tcs =
        "#version 460\nlayout(vertices = 3) out;\nlayout(location = 3) out vec2 texCoordOut[3];\nlayout(location = 3) in vec2 texCoordIn[];\n"
        "void main() { if (gl_InvocationID == 0) { gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0; gl_TessLevelInner[0] = 1.0; }\n"
        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position; texCoordOut[gl_InvocationID] = texCoordIn[gl_InvocationID]; }\n";
    static const char *tes =
        "#version 460\nlayout(triangles, ccw, equal_spacing) in;\nlayout(location = 3) out vec2 texCoordOut;\nlayout(location = 3) in vec2 texCoordIn[];\n"
        "void main() { gl_Position = gl_in[0].gl_Position * gl_TessCoord.x + gl_in[1].gl_Position * gl_TessCoord.y + gl_in[2].gl_Position * gl_TessCoord.z;\n"
        "  texCoordOut = texCoordIn[0]; }\n";
    static const char *gs =
        "#version 460\nlayout(triangles) in;\nlayout(max_vertices = 3, triangle_strip) out;\n"
        "layout(location = 3, stream = 0) out vec2 texCoordOut;\nlayout(location = 3) in vec2 texCoordIn[3];\n"
        "void main() { for (int i = 0; i < 3; i++) { gl_Position = gl_in[i].gl_Position; texCoordOut = texCoordIn[i]; EmitStreamVertex(0); }\n"
        "  EndStreamPrimitive(0); }\n";
    static const char *fs =
        "#version 460\nlayout(location = 3) in vec2 texCoord;\nlayout(location = 0) out vec4 o;\nvoid main() { o = vec4(texCoord, 0.0, 1.0); }\n";
    static const GLfloat tri[9] = { -1, -1, 0,  3, -1, 0,  -1, 3, 0 };
    const char *srcs[5] = { vs, tcs, tes, gs, fs };
    const GLenum stages[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                               GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    GLuint prog = glCreateProgram(), vao, vbo;
    GLint ok = 0;
    MGLTestTarget target;
    GLubyte px[4];
    unsigned char *img;
    char log[1024] = "";

    for (int i = 0; i < 5; i++)
    {
        GLuint sh = glCreateShader(stages[i]);
        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(prog, sizeof log, NULL, log);
    CHECK_MSG(ok, "did not link: %s", log);
    if (!ok || !mgl_target_create(&target, 4, 4, GL_RGBA8, 0))
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glDrawArrays(GL_PATCHES, 0, 3);
    img = mgl_read_rgba8(&target);
    mgl_pixel_at(img, &target, 1, 1, px);
    free(img);
    CHECK_MSG(px[0] > 60 && px[0] < 68 && px[1] > 188 && px[1] < 196, "read %u %u, want 64 191", px[0], px[1]);

    glUseProgram(0);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    mgl_target_destroy(&target);
}
