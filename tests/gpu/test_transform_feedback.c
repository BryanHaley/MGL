/*
 * test_transform_feedback.c
 * Copyright (C) The Moogle Project
 *
 * Transform feedback capture. Metal has no feedback stage, so MGL rewrites the
 * vertex shader to copy the recorded varyings into the bound buffers itself.
 * These tests read the buffers back and compare the numbers.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdlib.h>
#include <string.h>

static GLuint compileOne(GLenum stage, const char *src)
{
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    return ok ? sh : 0;
}

// glTransformFeedbackVaryings has to be called before the link, which
// mgl_build_program does not leave room for.
static GLuint linkRecording(const char *vs, const char *fs,
                            const char *const *varyings, GLsizei count, GLenum mode,
                            char *log, int log_size)
{
    GLuint prog = glCreateProgram();
    GLuint v = compileOne(GL_VERTEX_SHADER, vs);
    GLuint f = compileOne(GL_FRAGMENT_SHADER, fs);
    GLint ok = 0;

    if (log && log_size)
        log[0] = 0;

    if (!v || !f)
        return 0;

    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glTransformFeedbackVaryings(prog, count, varyings, mode);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok && log && log_size)
        glGetProgramInfoLog(prog, log_size, NULL, log);

    return ok ? prog : 0;
}

static GLuint recordingBuffer(GLuint index, GLsizeiptr bytes)
{
    GLuint b = 0;
    void *zero = calloc(1, (size_t)bytes);

    glGenBuffers(1, &b);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, b);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bytes, zero, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, index, b);
    free(zero);

    return b;
}

/* ---------- what the shader wrote comes back ---------- */

GPU_TEST(transform_feedback, interleaved_capture_round_trips)
{
    static const GLfloat pts[6] = { -0.5f, -0.25f, 0.5f, 0.25f, 0.75f, -0.75f };
    static const char *vs =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "out vec4 captured;\n"
        "void main() { captured = vec4(p, 7.0, 9.0); gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 410\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "captured" };
    MGLTestTarget t;
    GLuint prog, vao, vbo, tf;
    GLfloat got[12];
    char log[2048];

    prog = linkRecording(vs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "recording program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof pts, pts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    tf = recordingBuffer(0, 256);
    glViewport(0, 0, 16, 16);

    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDrawArrays(GL_POINTS, 0, 3);
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0, sizeof got);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 3; i++)
    {
        CHECK_MSG(got[i * 4 + 0] == pts[i * 2 + 0],
                  "vertex %d x captured %f, wrote %f", i, got[i * 4 + 0], pts[i * 2 + 0]);
        CHECK_MSG(got[i * 4 + 1] == pts[i * 2 + 1],
                  "vertex %d y captured %f, wrote %f", i, got[i * 4 + 1], pts[i * 2 + 1]);
        CHECK_MSG(got[i * 4 + 2] == 7.0f, "vertex %d z captured %f, wrote 7", i, got[i * 4 + 2]);
        CHECK_MSG(got[i * 4 + 3] == 9.0f, "vertex %d w captured %f, wrote 9", i, got[i * 4 + 3]);
    }

    glUseProgram(0);
    glDeleteBuffers(1, &tf);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// Two varyings of different widths have to land tightly packed, the way GL
// lays interleaved capture out -- not padded the way a std430 block would be.
GPU_TEST(transform_feedback, two_varyings_pack_tightly)
{
    static const GLfloat pts[4] = { 0.25f, 0.5f, -0.25f, -0.5f };
    static const char *vs =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "out vec3 three;\n"
        "out float one;\n"
        "void main() { three = vec3(p, 3.0); one = 11.0; gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 410\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "three", "one" };
    MGLTestTarget t;
    GLuint prog, vao, vbo, tf;
    GLfloat got[8];
    char log[2048];

    prog = linkRecording(vs, fs, varyings, 2, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "two varying program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof pts, pts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    tf = recordingBuffer(0, 256);
    glViewport(0, 0, 16, 16);

    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 2);
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0, sizeof got);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);

    // four components a vertex: three, three, three, one
    for (int i = 0; i < 2; i++)
    {
        CHECK_MSG(got[i * 4 + 0] == pts[i * 2 + 0], "vertex %d three.x is %f", i, got[i * 4 + 0]);
        CHECK_MSG(got[i * 4 + 1] == pts[i * 2 + 1], "vertex %d three.y is %f", i, got[i * 4 + 1]);
        CHECK_MSG(got[i * 4 + 2] == 3.0f, "vertex %d three.z is %f", i, got[i * 4 + 2]);
        CHECK_MSG(got[i * 4 + 3] == 11.0f,
                  "vertex %d one is %f, so the pack is not tight", i, got[i * 4 + 3]);
    }

    glUseProgram(0);
    glDeleteBuffers(1, &tf);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- nothing is recorded while it is off ---------- */

GPU_TEST(transform_feedback, paused_records_nothing)
{
    static const GLfloat pts[2] = { 0.25f, 0.5f };
    static const char *vs =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "out vec4 captured;\n"
        "void main() { captured = vec4(5.0); gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 410\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "captured" };
    MGLTestTarget t;
    GLuint prog, vao, vbo, tf;
    GLfloat got[4];
    char log[2048];

    prog = linkRecording(vs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof pts, pts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    tf = recordingBuffer(0, 256);
    glViewport(0, 0, 16, 16);

    // a draw outside any begin must leave the buffer alone
    glDrawArrays(GL_POINTS, 0, 1);

    memset(got, 0xAB, sizeof got);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 4; i++)
        CHECK_MSG(got[i] == 0.0f, "a draw with no feedback active wrote %f at %d", got[i], i);

    // and so must one while it is paused
    glBeginTransformFeedback(GL_POINTS);
    glPauseTransformFeedback();
    glDrawArrays(GL_POINTS, 0, 1);
    glResumeTransformFeedback();
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);

    for (int i = 0; i < 4; i++)
        CHECK_MSG(got[i] == 0.0f, "a paused draw wrote %f at %d", got[i], i);

    glUseProgram(0);
    glDeleteBuffers(1, &tf);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- the varyings belong to the program ---------- */

// GL keeps the recorded varyings on the program object, and they take effect
// at the next link. MGL used to keep them on whichever feedback object was
// bound, which lost them for any program that set them before binding one.
GPU_TEST(transform_feedback, varyings_belong_to_the_program)
{
    static const char *vs =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "out vec4 alpha;\n"
        "out vec4 beta;\n"
        "void main() { alpha = vec4(1); beta = vec4(2); gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 410\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "alpha", "beta" };
    GLuint prog;
    char log[2048];
    char name[64];
    GLsizei len = 0, size = 0;
    GLenum type = 0;

    prog = linkRecording(vs, fs, varyings, 2, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);

    if (!prog)
        return;

    for (GLuint i = 0; i < 2; i++)
    {
        name[0] = 0;
        glGetTransformFeedbackVarying(prog, i, sizeof name, &len, &size, &type, name);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_MSG(!strcmp(name, varyings[i]),
                  "varying %u reads back as \"%s\", recorded \"%s\"", i, name, varyings[i]);
    }

    // one past the end is an error, not a name
    glGetTransformFeedbackVarying(prog, 2, sizeof name, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(prog);
}
