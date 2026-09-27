/*
 * test_transform_feedback_gs.c
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
 * Transform feedback past the geometry stage, and feedback of 64-bit
 * varyings.
 *
 * When a geometry shader is active, GL 4.6 section 11.1.2.1 records the
 * geometry shader's output variables, one set per emitted vertex -- not the
 * vertex shader's. MGL captures from the vertex stage only, so these tests
 * fail on the frozen build and pass once the geometry compute stage's output
 * buffer feeds the feedback path.
 *
 * GL stores a double varying as its binary64 bits. The capture has to keep
 * all eight bytes and the stride has to count a double as eight, not four.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

/* ---------- helpers ---------- */

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

/* glTransformFeedbackVaryings has to be called before the link. A NULL
 * geometry source builds a vertex/fragment program. */
static GLuint linkRecording(const char *vs, const char *gs, const char *fs,
                            const char *const *varyings, GLsizei count, GLenum mode,
                            char *log, int log_size)
{
    GLuint prog = glCreateProgram();
    GLuint v, g = 0, f;
    GLint ok = 0;

    if (log && log_size)
        log[0] = 0;

    v = compileOne(GL_VERTEX_SHADER, vs, log, log_size);
    f = compileOne(GL_FRAGMENT_SHADER, fs, log, log_size);

    if (gs)
        g = compileOne(GL_GEOMETRY_SHADER, gs, log, log_size);

    if (!v || !f || (gs && !g))
        return 0;

    glAttachShader(prog, v);
    glAttachShader(prog, f);

    if (g)
        glAttachShader(prog, g);

    glTransformFeedbackVaryings(prog, count, varyings, mode);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok && log && log_size)
        glGetProgramInfoLog(prog, log_size, NULL, log);

    return ok ? prog : 0;
}

/* A fresh feedback object with one buffer bound at the given binding point. */
static GLuint recordingBuffer(GLuint xfb, GLuint index, GLsizeiptr bytes)
{
    GLuint b = 0;
    void *zero = calloc(1, (size_t)bytes);

    glGenBuffers(1, &b);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, b);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bytes, zero, GL_DYNAMIC_COPY);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, b);

    free(zero);

    return b;
}

static GLuint pointsVAO(const GLfloat *pts, size_t bytes, GLuint *vbo_out)
{
    GLuint vao, vbo;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, pts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    if (vbo_out)
        *vbo_out = vbo;

    return vao;
}

/* ---------- 1: feedback records the geometry shader's output ---------- */

// One point in, three vertices out. Each emitted vertex carries a different
// colour, so the buffer names exactly what the geometry stage produced.
GPU_TEST(transform_feedback_gs, geometry_output_is_captured)
{
    static const GLfloat pt[2] = { 0.0f, 0.0f };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "out vec4 vsColor;\n"
        "void main() { vsColor = vec4(0.5); gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "in vec4 vsColor[];\n"
        "out vec4 outColor;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        gl_Position = gl_in[0].gl_Position + vec4(float(i), 0.0, 0.0, 0.0);\n"
        "        outColor = vec4(float(i), float(i) + 1.0, float(i) + 2.0, 1.0);\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "in vec4 outColor;\n"
        "out vec4 o;\n"
        "void main() { o = outColor; }\n";
    static const char *varyings[] = { "outColor" };
    MGLTestTarget t;
    GLuint prog, vao, vbo, xfb, buf;
    GLfloat got[12];
    char log[2048];

    prog = linkRecording(vs, gs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "geometry recording program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = pointsVAO(pt, sizeof pt, &vbo);
    glViewport(0, 0, 16, 16);

    glGenTransformFeedbacks(1, &xfb);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    buf = recordingBuffer(xfb, 0, 256);

    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDrawArrays(GL_POINTS, 0, 1);
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0, sizeof got);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // the geometry stage emitted three vertices, each a different colour
    for (int i = 0; i < 3; i++)
    {
        CHECK_MSG(got[i * 4 + 0] == (GLfloat)i, "vertex %d colour.x captured %g, want %d",
                  i, got[i * 4 + 0], i);
        CHECK_MSG(got[i * 4 + 1] == (GLfloat)(i + 1), "vertex %d colour.y captured %g, want %d",
                  i, got[i * 4 + 1], i + 1);
        CHECK_MSG(got[i * 4 + 2] == (GLfloat)(i + 2), "vertex %d colour.z captured %g, want %d",
                  i, got[i * 4 + 2], i + 2);
        CHECK_MSG(got[i * 4 + 3] == 1.0f, "vertex %d colour.w captured %g, want 1",
                  i, got[i * 4 + 3]);
    }

    glUseProgram(0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteBuffers(1, &buf);
    glDeleteTransformFeedbacks(1, &xfb);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* ---------- 2: a double varying keeps all eight bytes ---------- */

// The value exercises the low mantissa bits a 24-bit float would drop, so a
// four-byte stride or a four-byte write shows up as a wrong number.
GPU_TEST(transform_feedback_gs, double_varying_round_trips)
{
    static const GLfloat pt[2] = { 0.0f, 0.0f };
    static const GLdouble want = 1.234567890123456;
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "out double dval;\n"
        "void main() { dval = 1.234567890123456lf; gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "dval" };
    MGLTestTarget t;
    GLuint prog, vao, vbo, xfb, buf;
    GLdouble got = 0.0;
    char log[2048];

    prog = linkRecording(vs, NULL, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "double recording program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = pointsVAO(pt, sizeof pt, &vbo);
    glViewport(0, 0, 16, 16);

    glGenTransformFeedbacks(1, &xfb);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    buf = recordingBuffer(xfb, 0, 256);

    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDrawArrays(GL_POINTS, 0, 1);
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, &got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // A four-byte write or a four-byte stride reads back a denormal or a
    // float-truncated value, which is nowhere near; the tolerance only
    // forgives the last few bits of the triple-float emulation.
    CHECK_MSG(fabs(got - want) <= 1e-12, "double varying captured %.17g, wrote %.17g", got, want);

    glUseProgram(0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteBuffers(1, &buf);
    glDeleteTransformFeedbacks(1, &xfb);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* ---------- 3: gl_Layer is capturable ---------- */

// The geometry stage routes its primitive to layer 1 and feedback records the
// layer index. The captured value is the int the shader wrote.
GPU_TEST(transform_feedback_gs, gl_layer_is_captured)
{
    static const GLfloat pt[2] = { 0.0f, 0.0f };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        gl_Position = gl_in[0].gl_Position + vec4(float(i), 0.0, 0.0, 0.0);\n"
        "        gl_Layer = 1;\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0, 1, 0, 1); }\n";
    static const char *varyings[] = { "gl_Layer" };
    MGLTestTarget t;
    GLuint prog, vao, vbo, xfb, buf;
    GLint got[3] = { -1, -1, -1 };
    char log[2048];

    prog = linkRecording(vs, gs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "gl_Layer recording program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = pointsVAO(pt, sizeof pt, &vbo);
    glViewport(0, 0, 16, 16);

    glGenTransformFeedbacks(1, &xfb);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    buf = recordingBuffer(xfb, 0, 256);

    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDrawArrays(GL_POINTS, 0, 1);
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0, sizeof got);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 3; i++)
        CHECK_MSG(got[i] == 1, "emitted vertex %d captured gl_Layer %d, want 1", i, got[i]);

    glUseProgram(0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteBuffers(1, &buf);
    glDeleteTransformFeedbacks(1, &xfb);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* ---------- 4: two geometry outputs pack tightly, interleaved ---------- */

// A vec4 and a float interleave five floats a vertex. GL packs them with no
// padding, so the size follows its colour immediately.
GPU_TEST(transform_feedback_gs, geometry_varyings_interleave_tightly)
{
    static const GLfloat pt[2] = { 0.0f, 0.0f };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "out vec4 outColor;\n"
        "out float outSize;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        gl_Position = gl_in[0].gl_Position + vec4(float(i), 0.0, 0.0, 0.0);\n"
        "        outColor = vec4(float(i) * 10.0, float(i) * 10.0 + 1.0,\n"
        "                        float(i) * 10.0 + 2.0, float(i) * 10.0 + 3.0);\n"
        "        outSize = float(i) + 0.5;\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "in vec4 outColor;\n"
        "in float outSize;\n"
        "out vec4 o;\n"
        "void main() { o = outColor * outSize; }\n";
    static const char *varyings[] = { "outColor", "outSize" };
    MGLTestTarget t;
    GLuint prog, vao, vbo, xfb, buf;
    GLfloat got[15];
    char log[2048];

    prog = linkRecording(vs, gs, fs, varyings, 2, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "interleaved geometry program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = pointsVAO(pt, sizeof pt, &vbo);
    glViewport(0, 0, 16, 16);

    glGenTransformFeedbacks(1, &xfb);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    buf = recordingBuffer(xfb, 0, 256);

    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDrawArrays(GL_POINTS, 0, 1);
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0, sizeof got);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // five floats a vertex: colour, then size, with no gap between them
    for (int i = 0; i < 3; i++)
    {
        const GLfloat *v = &got[i * 5];

        CHECK_MSG(v[0] == (GLfloat)(i * 10), "vertex %d colour.x captured %g, want %d",
                  i, v[0], i * 10);
        CHECK_MSG(v[1] == (GLfloat)(i * 10 + 1), "vertex %d colour.y captured %g, want %d",
                  i, v[1], i * 10 + 1);
        CHECK_MSG(v[2] == (GLfloat)(i * 10 + 2), "vertex %d colour.z captured %g, want %d",
                  i, v[2], i * 10 + 2);
        CHECK_MSG(v[3] == (GLfloat)(i * 10 + 3), "vertex %d colour.w captured %g, want %d",
                  i, v[3], i * 10 + 3);
        CHECK_MSG(v[4] == (GLfloat)i + 0.5f,
                  "vertex %d size captured %g, want %g so the pack is not tight",
                  i, v[4], (GLfloat)i + 0.5f);
    }

    glUseProgram(0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteBuffers(1, &buf);
    glDeleteTransformFeedbacks(1, &xfb);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
