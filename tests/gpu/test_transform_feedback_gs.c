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

/* ---------- the geometry stage's output is what the queries count ---------- */

// Two points in, each turned into three points out. GL_PRIMITIVES_GENERATED
// counts the six the geometry stage produced, the same as the feedback count.
// It counted the two that went in.
GPU_TEST(transform_feedback_gs, primitives_generated_counts_geometry_output)
{
    static const GLfloat pts[4] = { 0.0f, 0.0f, 0.5f, 0.5f };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 3) out;\n"
        "out float k;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) { gl_Position = gl_in[0].gl_Position; k = float(i); EmitVertex(); }\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "k" };
    GLuint prog, vao, vbo, xfb, buf, q[2];
    GLuint generated = 0, written = 0;
    MGLTestTarget t;
    char log[2048];

    prog = linkRecording(vs, gs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "did not link: %s", log);
    if (!prog || !mgl_target_create(&t, 4, 4, GL_RGBA8, 0))
        return;

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = pointsVAO(pts, sizeof pts, &vbo);
    glGenTransformFeedbacks(1, &xfb);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    buf = recordingBuffer(xfb, 0, 256);

    glGenQueries(2, q);
    glBeginQuery(GL_PRIMITIVES_GENERATED, q[0]);
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, q[1]);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 2);
    glEndTransformFeedback();
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    glEndQuery(GL_PRIMITIVES_GENERATED);

    glGetQueryObjectuiv(q[0], GL_QUERY_RESULT, &generated);
    glGetQueryObjectuiv(q[1], GL_QUERY_RESULT, &written);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT((GLint)written, 6);
    CHECK_EQ_INT((GLint)generated, 6);

    glUseProgram(0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteQueries(2, q);
    glDeleteBuffers(1, &buf);
    glDeleteTransformFeedbacks(1, &xfb);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// An array output a geometry shader records with xfb_offset. Only its first
// element reached the buffer.
GPU_TEST(transform_feedback_gs, an_array_output_is_recorded_whole)
{
    static const char *VS =
        "#version 460 core\n"
        "void main() { gl_Position = vec4(0, 0, 0, 1); }\n";
    static const char *GS =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 1) out;\n"
        "layout(location = 0, xfb_offset = 0) out vec4 goku[3];\n"
        "layout(std140, binding = 0) uniform Goku { vec4 uni_goku[3]; };\n"
        "void main() { goku = uni_goku; gl_Position = vec4(0, 0, 0, 1); EmitVertex(); }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    const char *src[3] = { VS, GS, FS };
    GLenum types[3] = { GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    float block[12], got[24];
    GLuint prog = glCreateProgram(), ubo, xfb, vao;
    GLint ok = 0;
    MGLTestTarget t;

    for (int i = 0; i < 3; i++)
    {
        GLuint sh = glCreateShader(types[i]);

        glShaderSource(sh, 1, &src[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    if (!ok) return;

    for (int i = 0; i < 12; i++)
        block[i] = (float)(i + 1);

    glGenBuffers(1, &ubo);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof block, block, GL_STATIC_DRAW);
    memset(got, 0, sizeof got);
    glGenBuffers(1, &xfb);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof got, got, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb);

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 2);
    glEndTransformFeedback();
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 24; i++)
        if (got[i] != (float)(i % 12 + 1))
        {
            CHECK_MSG(0, "float %d recorded %g, want %g", i, got[i], (double)(i % 12 + 1));
            break;
        }

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ubo);
    glDeleteBuffers(1, &xfb);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// An evaluation shader's output block, recorded by xfb_offset into the
// buffer the global qualifier names. The stand-in geometry stage left the
// block out, and the capture table never matched "vegeta.trunk".
GPU_TEST(transform_feedback_gs, an_evaluation_block_is_recorded)
{
    static const char *VS =
        "#version 460 core\n"
        "void main() { gl_Position = vec4(0, 0, 0, 1); }\n";
    static const char *TES =
        "#version 460 core\n"
        "layout(isolines, point_mode) in;\n"
        "layout(xfb_buffer = 1) out;\n"
        "layout(xfb_offset = 0) out Vegeta { vec4 trunk; vec4 bra; } vegeta;\n"
        "void main() { vegeta.trunk = vec4(1, 2, 3, 4); vegeta.bra = vec4(5, 6, 7, 8); gl_Position = vec4(0); }\n";
    static const char *FS =
        "#version 460 core\n"
        "in Vegeta { vec4 trunk; vec4 bra; } vegeta;\n"
        "out vec4 o;\n"
        "void main() { o = vegeta.trunk; }\n";
    const char *src[3] = { VS, TES, FS };
    GLenum types[3] = { GL_VERTEX_SHADER, GL_TESS_EVALUATION_SHADER, GL_FRAGMENT_SHADER };
    float got[16];
    GLuint prog = glCreateProgram(), xfb, vao;
    GLint ok = 0;
    MGLTestTarget t;

    for (int i = 0; i < 3; i++)
    {
        GLuint sh = glCreateShader(types[i]);

        glShaderSource(sh, 1, &src[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    if (!ok) return;

    memset(got, 0, sizeof got);
    glGenBuffers(1, &xfb);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof got, got, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, xfb);

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 1);
    {
        const float one[4] = { 1, 1, 1, 1 };

        glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, one);
    }
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_PATCHES, 0, 1);
    glEndTransformFeedback();
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // isolines at level 1 make two points
    for (int i = 0; i < 16; i++)
        if (got[i] != (float)(i % 8 + 1))
        {
            CHECK_MSG(0, "float %d recorded %g, want %g", i, got[i], (double)(i % 8 + 1));
            break;
        }

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &xfb);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// A dmat3 out of a geometry shader. Each dvec3 column sits on 32 bytes in the
// stage's own buffer, and the copy into the recorded one read them as packed.
GPU_TEST(transform_feedback_gs, a_double_matrix_is_recorded)
{
    static const char *VS =
        "#version 460 core\n"
        "void main() { gl_Position = vec4(0, 0, 0, 1); }\n";
    static const char *GS =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 1) out;\n"
        "layout(xfb_offset = 0) flat out dmat3 m;\n"
        "void main() {\n"
        "  m = dmat3(1, 2, 3, 4, 5, 6, 7, 8, 9);\n"
        "  gl_Position = vec4(0, 0, 0, 1); EmitVertex();\n"
        "}\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    const char *src[3] = { VS, GS, FS };
    GLenum types[3] = { GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    double got[9];
    GLuint prog = glCreateProgram(), xfb, vao;
    GLint ok = 0;
    MGLTestTarget t;

    for (int i = 0; i < 3; i++)
    {
        GLuint sh = glCreateShader(types[i]);

        glShaderSource(sh, 1, &src[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    if (!ok) return;

    memset(got, 0, sizeof got);
    glGenBuffers(1, &xfb);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof got, got, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb);

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 1);
    glEndTransformFeedback();
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 9; i++)
        if (got[i] != (double)(i + 1))
        {
            CHECK_MSG(0, "double %d recorded %g, want %d", i, got[i], i + 1);
            break;
        }

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &xfb);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// A block with no instance name, its members written bare, into a buffer with
// room for one record. The rewrite left the block in the generated compute
// shader as it stood, which did not compile, and a record that did not fit
// was written as far as the buffer went.
GPU_TEST(transform_feedback_gs, an_unnamed_block_is_recorded)
{
    static const char *VS =
        "#version 460 core\n"
        "void main() { gl_Position = vec4(0, 0, 0, 1); }\n";
    static const char *GS =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 3) out;\n"
        "layout(xfb_buffer = 0, xfb_offset = 0) out Goku {\n"
        "    vec4 gohan;\n"
        "    layout(xfb_stride = 48) vec4 goten;\n"
        "    vec4 chichi;\n"
        "};\n"
        "layout(binding = 0) uniform gs_block { vec4 uni_gohan; vec4 uni_goten; vec4 uni_chichi; };\n"
        "void main() {\n"
        "  gohan = uni_gohan; goten = uni_goten; chichi = uni_chichi;\n"
        "  gl_Position = vec4(0, 0, 0, 1); EmitVertex();\n"
        "}\n";
    static const char *FS =
        "#version 460 core\n"
        "in Goku { vec4 gohan; vec4 goten; vec4 chichi; };\n"
        "out vec4 o;\n"
        "void main() { o = gohan + goten + chichi; }\n";
    const char *src[3] = { VS, GS, FS };
    GLenum types[3] = { GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    float got[16];
    GLuint prog = glCreateProgram(), xfb, vao;
    GLint ok = 0;
    MGLTestTarget t;

    for (int i = 0; i < 3; i++)
    {
        GLuint sh = glCreateShader(types[i]);

        glShaderSource(sh, 1, &src[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    if (!ok) return;

    for (int i = 0; i < 16; i++)
        got[i] = -1.0f;

    GLuint ubo;

    {
        float values[12];

        for (int i = 0; i < 12; i++)
            values[i] = (float)(i + 1);

        glGenBuffers(1, &ubo);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof values, values, GL_STATIC_DRAW);
    }

    glGenBuffers(1, &xfb);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof got, got, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb);

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glBeginTransformFeedback(GL_POINTS);
    // room for one record; the second point has none and writes nothing
    glDrawArrays(GL_POINTS, 0, 2);
    glEndTransformFeedback();
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // three vec4s, and the last 16 bytes left as they were
    for (int i = 0; i < 16; i++)
    {
        float want = i < 12 ? (float)(i + 1) : -1.0f;

        if (got[i] != want)
        {
            CHECK_MSG(0, "float %d is %g, want %g", i, got[i], want);
            break;
        }
    }

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &xfb);
    glDeleteBuffers(1, &ubo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
