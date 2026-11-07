/*
 * test_transform_feedback.c
 * Copyright (C) The MooGL Project
 *
 * Transform feedback capture. Metal has no feedback stage, so MGL rewrites the
 * vertex shader to copy the recorded varyings into the bound buffers itself.
 * These tests read the buffers back and compare the numbers.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdio.h>
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

// Feedback can name one element of an array, and a block member by the
// block's name rather than its instance's. Only whole plain varyings were
// looked up, so every one of these failed the link.
GPU_TEST(transform_feedback, block_members_and_array_elements_by_name)
{
    static const GLfloat pts[2] = { 0.0f, 0.0f };
    static const char *vs =
        "#version 430\n"
        "layout(location = 0) in vec2 p;\n"
        "out StageData { vec4 v[3]; float f; } vs_out;\n"
        "out float arr[4];\n"
        "void main() {\n"
        "  for (int i = 0; i < 3; i++) vs_out.v[i] = vec4(float(i * 10));\n"
        "  vs_out.f = 5.0;\n"
        "  for (int i = 0; i < 4; i++) arr[i] = float(100 + i);\n"
        "  gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *fs =
        "#version 430\n"
        "in StageData { vec4 v[3]; float f; } fs_in;\n"
        "in float arr[4];\n"
        "out vec4 o;\n"
        "void main() { o = fs_in.v[0] + vec4(fs_in.f + arr[0]); }\n";
    static const char *varyings[] = { "StageData.v[2]", "arr[3]", "StageData.f", "StageData.v[1]" };
    // v[2] as four floats, then arr[3], f, and v[1]
    static const GLfloat want[10] = { 20, 20, 20, 20, 103, 5, 10, 10, 10, 10 };
    MGLTestTarget t;
    GLuint prog, vao, vbo, tf;
    GLfloat got[10];
    char log[2048];

    prog = linkRecording(vs, fs, varyings, 4, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "block member feedback did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
        return;

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
    glDrawArrays(GL_POINTS, 0, 1);
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0, sizeof got);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);

    for (int i = 0; i < 10; i++)
        CHECK_MSG(got[i] == want[i], "word %d captured %g, want %g", i, got[i], want[i]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &tf);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// GL records an instanced draw one instance after another. The capture
// placed each vertex by its vertex id alone, so every instance wrote over
// the first, and the instanced and base-vertex draws set up no capture at all.
GPU_TEST(transform_feedback, instances_record_one_after_another)
{
    static const char *vs =
        "#version 430\n"
        "layout(location = 0) in float v;\n"
        "out float captured;\n"
        "void main() { captured = v + 100.0 * float(gl_InstanceID); gl_Position = vec4(0.0); }\n";
    static const char *fs =
        "#version 430\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "captured" };
    static const GLfloat vals[3] = { 1, 2, 3 };
    static const GLushort idx[3] = { 0, 1, 2 };
    GLuint prog, vao, vbo, ibo, tf;
    GLfloat got[12];
    char log[2048];

    prog = linkRecording(vs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    CHECK_MSG(prog != 0, "recording program did not link: %s", log);

    if (!prog) return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vals, vals, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);

    tf = recordingBuffer(0, 256);
    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);

    // two instances of three, arrays then elements
    glBeginTransformFeedback(GL_POINTS);
    glDrawArraysInstanced(GL_POINTS, 0, 3, 2);
    glDrawElementsInstanced(GL_POINTS, 3, GL_UNSIGNED_SHORT, NULL, 2);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0, sizeof got);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, tf);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);

    for (int d = 0; d < 2; d++)
        for (int i = 0; i < 6; i++)
        {
            GLfloat want = vals[i % 3] + 100.0f * (GLfloat)(i / 3);

            CHECK_MSG(got[d * 6 + i] == want, "%s record %d holds %g, want %g",
                      d ? "elements" : "arrays", i, got[d * 6 + i], want);
        }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &tf);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
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

/* ---------- the object model around the capture ---------- */

GPU_TEST(transform_feedback, gen_reserves_a_name_and_bind_makes_the_object)
{
    GLuint ids[2] = { 0, 0 };
    GLint binding = -1;

    glGenTransformFeedbacks(2, ids);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(ids[0] != 0 && ids[1] != 0);

    // glGen hands out a name; the object itself is not there yet
    CHECK_EQ_INT(glIsTransformFeedback(ids[0]), GL_FALSE);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, ids[0]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(glIsTransformFeedback(ids[0]), GL_TRUE);

    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &binding);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT((GLuint)binding, ids[0]);

    // deleting the bound one falls back to the default object, not to nothing
    glDeleteTransformFeedbacks(1, &ids[0]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    binding = -1;
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &binding);
    CHECK_EQ_INT(binding, 0);

    // glCreate makes the object outright
    {
        GLuint made = 0;

        glCreateTransformFeedbacks(1, &made);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(glIsTransformFeedback(made), GL_TRUE);

        binding = -1;
        glGetIntegerv(GL_TRANSFORM_FEEDBACK_BINDING, &binding);
        CHECK_MSG(binding == 0, "glCreateTransformFeedbacks bound %d", binding);

        glDeleteTransformFeedbacks(1, &made);
    }

    glDeleteTransformFeedbacks(1, &ids[1]);
}

/* ---------- a replay draws what the last capture came to ---------- */

GPU_TEST(transform_feedback, draw_replays_the_recorded_vertices)
{
    static const GLfloat pts[8] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f };
    static const char *vs =
        "#version 410 core\n"
        "layout(location = 0) in vec2 p;\n"
        "out vec2 captured;\n"
        "void main() { captured = p; gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 410 core\n"
        "in vec2 captured;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(captured, 0.0, 1.0); }\n";
    static const char *const varyings[1] = { "captured" };

    GLuint prog, vao = 0, vbo = 0, tfb = 0, xfb = 0;
    GLint active = -1;
    char log[2048];

    prog = linkRecording(vs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);

    if (!prog)
    {
        CHECK_MSG(0, "recording program did not link: %s", log);
        return;
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof pts, pts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenTransformFeedbacks(1, &xfb);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    tfb = recordingBuffer(0, (GLsizeiptr)(sizeof(GLfloat) * 2 * 4));

    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);

    glGetIntegerv(GL_TRANSFORM_FEEDBACK_ACTIVE, &active);
    CHECK_EQ_INT(active, GL_TRUE);

    glDrawArrays(GL_POINTS, 0, 4);
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    active = -1;
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_ACTIVE, &active);
    CHECK_EQ_INT(active, GL_FALSE);

    // and now the replay: four vertices were recorded, so four are drawn
    {
        MGLTestTarget t;

        if (mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
        {
            GLuint counter = 0;

            glDisable(GL_RASTERIZER_DISCARD);
            mgl_target_bind(&t);
            glViewport(0, 0, t.width, t.height);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            glGenQueries(1, &counter);
            glBeginQuery(GL_PRIMITIVES_GENERATED, counter);
            glDrawTransformFeedback(GL_POINTS, xfb);
            CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
            glEndQuery(GL_PRIMITIVES_GENERATED);

            {
                GLuint drawn = 0;

                glGetQueryObjectuiv(counter, GL_QUERY_RESULT, &drawn);
                CHECK_MSG(drawn == 4, "the replay drew %u points, expected the 4 that were recorded", drawn);
            }

            glDeleteQueries(1, &counter);
            mgl_target_destroy(&t);
        }
    }

    // an object that never finished a capture has nothing to replay
    {
        GLuint fresh = 0;

        glCreateTransformFeedbacks(1, &fresh);
        mgl_drain_errors();
        glDrawTransformFeedback(GL_POINTS, fresh);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
        glDeleteTransformFeedbacks(1, &fresh);
    }

    glUseProgram(0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteTransformFeedbacks(1, &xfb);
    glDeleteBuffers(1, &tfb);
    glDeleteBuffers(1, &vbo);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}

/* An indexed draw records its vertices in the order the elements list them,
   not in index order, for every instance in turn. */
GPU_TEST(transform_feedback, indexed_draws_record_in_element_order)
{
    static const char *vs =
        "#version 430 core\n"
        "layout(location = 0) in float v;\n"
        "out float got;\n"
        "void main() { got = v + float(gl_InstanceID) * 100.0; gl_Position = vec4(0.0); }\n";
    static const char *fs =
        "#version 430 core\nout vec4 o;\nvoid main() { o = vec4(1.0); }\n";
    const char *names[] = { "got" };
    GLuint prog = glCreateProgram(), sh[2];
    GLint ok = 0;

    sh[0] = glCreateShader(GL_VERTEX_SHADER);
    sh[1] = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(sh[0], 1, &vs, NULL);
    glShaderSource(sh[1], 1, &fs, NULL);
    for (int i = 0; i < 2; i++)
    {
        glCompileShader(sh[i]);
        glAttachShader(prog, sh[i]);
        glDeleteShader(sh[i]);
    }
    glTransformFeedbackVaryings(prog, 1, names, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    if (!ok)
        return;

    static const GLfloat values[8] = { 10, 11, 12, 13, 14, 15, 16, 17 };
    static const GLushort idx[5] = { 6, 2, 2, 7, 4 };
    GLuint vao, vbo, ebo, xfb;
    GLfloat got[10];

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof values, values, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
    glGenBuffers(1, &xfb);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof got, NULL, GL_STATIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb);

    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    glDrawElementsInstanced(GL_POINTS, 5, GL_UNSIGNED_SHORT, NULL, 2);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(got, 0, sizeof got);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);

    for (int k = 0; k < 10; k++)
    {
        GLfloat want = values[idx[k % 5]] + (GLfloat)(k / 5) * 100.0f;

        CHECK_MSG(got[k] == want, "recorded vertex %d is %g, want %g", k, got[k], want);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &xfb);
    glDeleteVertexArrays(1, &vao);
}

// The feedback rules the API checks: the generic binding reads back, a buffer
// cannot be swapped while recording, the overflow query has no stream to
// name, and varyings with nothing to write them do not link.
GPU_TEST(transform_feedback, api_rules_follow_the_spec)
{
    static const char *VS =
        "#version 460 core\n"
        "out float v;\n"
        "void main() { v = 1.0; gl_Position = vec4(0.0); }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "v" };
    GLuint buf, prog, vs, fs, fs_only, vao, q;
    GLint v = -1, ok = 1;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 64, NULL, GL_DYNAMIC_COPY);
    glGetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, (GLint)buf);

    vs = glCreateShader(GL_VERTEX_SHADER);
    fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &VS, NULL);
    glShaderSource(fs, 1, &FS, NULL);
    glCompileShader(vs);
    glCompileShader(fs);

    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glTransformFeedbackVaryings(prog, 1, varyings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);

    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
    glBeginTransformFeedback(GL_POINTS);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glEndTransformFeedback();
    glUseProgram(0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenQueries(1, &q);
    glGetQueryIndexediv(GL_TRANSFORM_FEEDBACK_OVERFLOW, 1, GL_CURRENT_QUERY, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    fs_only = glCreateProgram();
    glAttachShader(fs_only, fs);
    glTransformFeedbackVaryings(fs_only, 1, varyings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(fs_only);
    glGetProgramiv(fs_only, GL_LINK_STATUS, &ok);
    CHECK(!ok);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteQueries(1, &q);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glDeleteProgram(fs_only);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteBuffers(1, &buf);
}

// Doubles copied from a std140 block into a varying the shader records with
// xfb_offset. kind 0 is one dmat2, kind 1 an array of three.
static void doubleCaptureRound(int kind, double *got, int n)
{
    static const char *vs_one =
        "#version 460 core\n"
        "layout(location = 0, xfb_offset = 0) flat out dmat2 goku;\n"
        "layout(std140, binding = 0) uniform Goku { dmat2 uni_goku; };\n"
        "void main() { goku = uni_goku; gl_Position = vec4(0, 0, 0, 1); gl_PointSize = 1.0; }\n";
    static const char *vs_three =
        "#version 460 core\n"
        "layout(location = 0, xfb_offset = 0) flat out dmat2 goku[3];\n"
        "layout(std140, binding = 0) uniform Goku { dmat2 uni_goku[3]; };\n"
        "void main() { goku = uni_goku; gl_Position = vec4(0, 0, 0, 1); gl_PointSize = 1.0; }\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    char log[2048] = "";
    GLuint prog = linkRecording(kind ? vs_three : vs_one, fs, NULL, 0, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    double block[12];
    GLuint ubo, xfb, vao;
    MGLTestTarget t;

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog)
        return;

    // std140 puts each dvec2 column on its own 16 bytes, so the block is
    // already packed; values the emulation cannot round
    for (int i = 0; i < 12; i++)
        block[i] = (i + 1) * 0.25;

    glGenBuffers(1, &ubo);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof block, block, GL_STATIC_DRAW);
    xfb = recordingBuffer(0, (GLsizeiptr)(n * sizeof(double)));

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 2);
    glEndTransformFeedback();
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, (GLsizeiptr)(n * sizeof(double)), got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ubo);
    glDeleteBuffers(1, &xfb);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// An array of double matrices out of a uniform block did not build, and
// recorded wrong.
GPU_TEST(transform_feedback, double_matrices_from_a_block_are_recorded)
{
    double got[12];

    for (int kind = 0; kind < 2; kind++)
    {
        int n = kind ? 12 : 4;

        memset(got, 0, sizeof got);
        doubleCaptureRound(kind, got, n);

        for (int i = 0; i < n; i++)
            if (got[i] != (i + 1) * 0.25)
            {
                CHECK_MSG(0, "%s: double %d recorded %g, want %g", kind ? "dmat2[3]" : "dmat2", i, got[i],
                          (i + 1) * 0.25);
                break;
            }
    }
}

// What the program interface reports for each recorded varying and buffer:
// offset, type, buffer index and the buffer's stride. b0_v0, b0_v1 and b0_v3
// sit at 0, 1 and 3 floats in buffer 0, b1_v1 at 1 float in buffer 1, and
// both buffers are four floats long.
static void checkFourFloatLayout(GLuint prog, const char *what)
{
    static const char *names[4] = { "b0_v0", "b0_v1", "b0_v3", "b1_v1" };
    static const GLint want_buf[4] = { 0, 0, 0, 1 }, want_off[4] = { 0, 4, 12, 4 };
    static const GLenum props[3] = { GL_OFFSET, GL_TYPE, GL_TRANSFORM_FEEDBACK_BUFFER_INDEX };

    for (int i = 0; i < 4; i++)
    {
        GLuint index = glGetProgramResourceIndex(prog, GL_TRANSFORM_FEEDBACK_VARYING, names[i]);
        GLint v[3] = { -1, -1, -1 }, stride = -1;

        glGetProgramResourceiv(prog, GL_TRANSFORM_FEEDBACK_VARYING, index, 3, props, 3, NULL, v);
        glGetProgramResourceiv(prog, GL_TRANSFORM_FEEDBACK_BUFFER, (GLuint)v[2], 1,
                               (GLenum[]){ GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE }, 1, NULL, &stride);

        CHECK_MSG(index != GL_INVALID_INDEX && v[0] == want_off[i] && v[1] == GL_FLOAT && v[2] == want_buf[i] &&
                  stride == 16, "%s: %s is at %d, type 0x%x, buffer %d of stride %d", what, names[i], v[0], v[1],
                  v[2], stride);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

static GLuint separableProgram(GLenum stage, const char *src, const char *const *varyings, GLsizei count)
{
    GLuint prog = glCreateProgram(), sh = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glAttachShader(prog, sh);
    glProgramParameteri(prog, GL_PROGRAM_SEPARABLE, GL_TRUE);

    if (varyings)
        glTransformFeedbackVaryings(prog, count, varyings, GL_INTERLEAVED_ATTRIBS);

    glLinkProgram(prog);
    glDeleteShader(sh);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    return ok ? prog : 0;
}

// The layout either way: varyings and gl_SkipComponents through the API, or
// xfb qualifiers in a geometry or evaluation shader of its own.
GPU_TEST(transform_feedback, interface_reports_the_recorded_layout)
{
    static const char *vs =
        "#version 460 core\n"
        "out float b0_v1; out float b1_v1; out float b0_v3; out float b0_v0;\n"
        "void main() { b0_v1 = 0.0; b1_v1 = 1.0; b0_v3 = 0.0; b0_v0 = 1.0; gl_Position = vec4(0); }\n";
    static const char *const api[] = {
        "b0_v0", "b0_v1", "gl_SkipComponents1", "b0_v3", "gl_NextBuffer",
        "gl_SkipComponents1", "b1_v1", "gl_SkipComponents2",
    };
    static const char *decl =
        "layout (xfb_buffer = 1, xfb_stride = 16) out;\n"
        "layout (xfb_buffer = 0, xfb_offset = 4) out float b0_v1;\n"
        "layout (xfb_buffer = 1, xfb_offset = 4) out float b1_v1;\n"
        "layout (xfb_buffer = 0, xfb_offset = 12) out float b0_v3;\n"
        "layout (xfb_buffer = 0, xfb_offset = 0) out float b0_v0;\n";
    char gs[1024], tes[1024];
    GLuint prog;

    prog = separableProgram(GL_VERTEX_SHADER, vs, api, 8);
    CHECK(prog != 0);
    if (prog)
        checkFourFloatLayout(prog, "the API");
    glDeleteProgram(prog);

    snprintf(gs, sizeof gs,
             "#version 460 core\nlayout(points) in;\nlayout(points, max_vertices = 1) out;\n%s"
             "out gl_PerVertex { vec4 gl_Position; };\n"
             "void main() { b0_v1 = 0.0; b1_v1 = 1.0; b0_v3 = 0.0; b0_v0 = 1.0; gl_Position = vec4(0); EmitVertex(); }\n",
             decl);
    prog = separableProgram(GL_GEOMETRY_SHADER, gs, NULL, 0);
    CHECK(prog != 0);
    if (prog)
        checkFourFloatLayout(prog, "a geometry shader");
    glDeleteProgram(prog);

    // a dmat2 takes 32 bytes, so the varying after it starts there
    {
        static const char *dvs =
            "#version 460 core\n"
            "out dmat2 a; out float b;\n"
            "void main() { a = dmat2(1.0); b = 1.0; gl_Position = vec4(0); }\n";
        static const char *const both[] = { "a", "b" };
        GLint off = -1;

        prog = separableProgram(GL_VERTEX_SHADER, dvs, both, 2);
        CHECK(prog != 0);
        if (prog)
        {
            glGetProgramResourceiv(prog, GL_TRANSFORM_FEEDBACK_VARYING, 1, 1, (GLenum[]){ GL_OFFSET }, 1, NULL, &off);
            CHECK_EQ_INT(off, 32);
        }
        glDeleteProgram(prog);
    }

    snprintf(tes, sizeof tes,
             "#version 460 core\nlayout(isolines, point_mode) in;\n%s"
             "void main() { b0_v1 = 0.0; b1_v1 = 1.0; b0_v3 = 0.0; b0_v0 = 1.0; }\n",
             decl);
    prog = separableProgram(GL_TESS_EVALUATION_SHADER, tes, NULL, 0);
    CHECK(prog != 0);
    if (prog)
        checkFourFloatLayout(prog, "an evaluation shader");
    glDeleteProgram(prog);
}

// xfb_stride on an array of output blocks in a geometry or evaluation shader
// is the buffer's stride, whatever the members add up to.
GPU_TEST(transform_feedback, a_block_stride_survives_later_stages)
{
    static const char *block =
        "layout (xfb_offset = 0, xfb_stride = 128) out Goku { vec4 gohan; vec4 goten; vec4 chichi; } goku[1];\n";
    static const char *heads[2] = {
        "#version 460 core\nlayout(points) in;\nlayout(points, max_vertices = 1) out;\n",
        "#version 460 core\nlayout(isolines, point_mode) in;\n",
    };
    static const GLenum stages[2] = { GL_GEOMETRY_SHADER, GL_TESS_EVALUATION_SHADER };
    static const char *tails[2] = { "EmitVertex(); ", "" };

    for (int i = 0; i < 2; i++)
    {
        char src[1024];
        GLint stride = -1;

        snprintf(src, sizeof src,
                 "%s%sout gl_PerVertex { vec4 gl_Position; };\n"
                 "void main() { goku[0].gohan = vec4(1); goku[0].goten = vec4(2); goku[0].chichi = vec4(3); "
                 "gl_Position = vec4(0); %s}\n", heads[i], block, tails[i]);

        GLuint prog = separableProgram(stages[i], src, NULL, 0);

        CHECK(prog != 0);
        if (prog)
            glGetProgramResourceiv(prog, GL_TRANSFORM_FEEDBACK_BUFFER, 0, 1,
                                   (GLenum[]){ GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE }, 1, NULL, &stride);
        CHECK_MSG(stride == 128, "stage 0x%x: stride %d", stages[i], stride);
        glDeleteProgram(prog);
    }

    // with the stages before it, the evaluation shader's outputs pass through
    // a generated geometry stage
    {
        static const char *vs = "#version 460 core\nvoid main() { gl_Position = vec4(0); }\n";
        static const char *tcs =
            "#version 460 core\nlayout(vertices = 1) out;\n"
            "void main() { gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; }\n";
        char tes[1024];
        const char *srcs[3] = { vs, tcs, tes };
        GLenum types[3] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER };
        GLuint prog = glCreateProgram();
        GLint ok = 0, stride = -1;

        snprintf(tes, sizeof tes,
                 "%s%svoid main() { goku[0].gohan = vec4(1); goku[0].goten = vec4(2); goku[0].chichi = vec4(3); "
                 "gl_Position = vec4(0); }\n", heads[1], block);

        for (int i = 0; i < 3; i++)
        {
            GLuint sh = glCreateShader(types[i]);

            glShaderSource(sh, 1, &srcs[i], NULL);
            glCompileShader(sh);
            glAttachShader(prog, sh);
            glDeleteShader(sh);
        }

        glProgramParameteri(prog, GL_PROGRAM_SEPARABLE, GL_TRUE);
        glLinkProgram(prog);
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        CHECK(ok);
        glGetProgramResourceiv(prog, GL_TRANSFORM_FEEDBACK_BUFFER, 0, 1,
                               (GLenum[]){ GL_TRANSFORM_FEEDBACK_BUFFER_STRIDE }, 1, NULL, &stride);
        CHECK_MSG(stride == 128, "vertex to evaluation: stride %d", stride);
        glDeleteProgram(prog);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// Indexed strips, loops and fans are recorded as separate primitives in the
// order GL assembles them. A loop and a fan used to record only the vertices
// once each, in index order.
GPU_TEST(transform_feedback, indexed_strips_loops_and_fans_are_recorded_whole)
{
    static const char *vs =
        "#version 460 core\n"
        "out float id;\n"
        "void main() { id = float(gl_VertexID); gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "id" };
    static const struct {
        GLenum mode, record;
        GLuint idx[4];
        int n;
        float want[8];
    } cases[] = {
        { GL_LINE_STRIP,     GL_LINES,     {0, 1, 3, 2}, 6, {0, 1, 1, 3, 3, 2} },
        { GL_LINE_LOOP,      GL_LINES,     {0, 1, 3, 2}, 8, {0, 1, 1, 3, 3, 2, 2, 0} },
        { GL_TRIANGLE_STRIP, GL_TRIANGLES, {0, 1, 2, 3}, 6, {0, 1, 2, 2, 1, 3} },
        { GL_TRIANGLE_FAN,   GL_TRIANGLES, {2, 0, 1, 3}, 6, {2, 0, 1, 2, 1, 3} },
    };
    char log[1024] = "";
    GLuint prog = linkRecording(vs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    GLuint vao, buf;

    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);
    buf = recordingBuffer(0, 64);

    for (int c = 0; c < 4; c++)
    {
        float got[8] = {0};

        glBeginTransformFeedback(cases[c].record);
        glDrawElements(cases[c].mode, 4, GL_UNSIGNED_INT, cases[c].idx);
        glEndTransformFeedback();
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
        glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, cases[c].n * sizeof(float), got);

        for (int i = 0; i < cases[c].n; i++)
            CHECK_MSG(got[i] == cases[c].want[i], "mode 0x%x vertex %d recorded %g, want %g",
                      cases[c].mode, i, got[i], cases[c].want[i]);
    }

    glDisable(GL_RASTERIZER_DISCARD);
    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

// With the raster off a program that has a fragment shader draws nothing, and
// what it records still arrives. It used to draw regardless.
GPU_TEST(transform_feedback, rasterizing_off_draws_nothing_but_still_records)
{
    static const char *vs =
        "#version 460 core\n"
        "out float id;\n"
        "void main() {\n"
        "    id = float(gl_VertexID) + 1.0;\n"
        "    vec2 p[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));\n"
        "    gl_Position = vec4(p[gl_VertexID], 0.0, 1.0);\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "id" };
    char log[1024] = "";
    GLuint prog = linkRecording(vs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    MGLTestTarget t;
    GLuint vao, buf;
    GLubyte px[4] = {0};
    float got[3] = {0};

    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog || !mgl_target_create(&t, 4, 4, GL_RGBA8, 0))
        return;

    // what was linked counts, not what is still attached
    {
        GLuint sh[4];
        GLsizei n = 0;

        glGetAttachedShaders(prog, 4, &n, sh);

        for (GLsizei i = 0; i < n; i++)
            glDetachShader(prog, sh[i]);
    }

    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    buf = recordingBuffer(0, 64);

    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_TRIANGLES);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_MSG(px[0] == 0 && px[3] == 0, "the raster was off, yet the target reads %u %u %u %u",
              px[0], px[1], px[2], px[3]);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_MSG(got[0] == 1 && got[1] == 2 && got[2] == 3, "recorded %g %g %g, want 1 2 3", got[0], got[1], got[2]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);
}

// The indexed feedback buffer bindings belong to the feedback object. Each of
// two objects resumed later records on into its own buffer, not into
// whichever was bound last.
GPU_TEST(transform_feedback, each_object_keeps_its_own_buffers)
{
    static const char *vs =
        "#version 460 core\n"
        "uniform float tag;\n"
        "out float id;\n"
        "void main() { id = tag; gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "id" };
    char log[1024] = "";
    GLuint prog = linkRecording(vs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    GLuint xfb[2], bo[2], vao;

    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return;

    glGenTransformFeedbacks(2, xfb);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);

    for (int j = 0; j < 2; j++)
    {
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb[j]);
        bo[j] = recordingBuffer(0, 64);
        glUniform1f(glGetUniformLocation(prog, "tag"), (float)(j + 1));
        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, 1);
        glPauseTransformFeedback();
    }

    for (int j = 0; j < 2; j++)
    {
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb[j]);
        glUniform1f(glGetUniformLocation(prog, "tag"), (float)(j + 1));
        glResumeTransformFeedback();
        glDrawArrays(GL_POINTS, 0, 1);
        glEndTransformFeedback();
    }

    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int j = 0; j < 2; j++)
    {
        float got[2] = {0};
        GLint bound = -1;

        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb[j]);
        glGetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &bound);
        CHECK_EQ_INT(bound, (GLint)bo[j]);

        glBindBuffer(GL_ARRAY_BUFFER, bo[j]);
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof got, got);
        CHECK_MSG(got[0] == j + 1 && got[1] == j + 1, "object %d recorded %g %g, want %d twice",
                  j, got[0], got[1], j + 1);
    }

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(2, bo);
    glDeleteTransformFeedbacks(2, xfb);
    glDeleteVertexArrays(1, &vao);
}

// Drawing from what was just recorded, recording again into the other buffer:
// each draw reads the last one's output as its vertices. With nothing read
// back between draws, the reads used to run ahead of the writes.
GPU_TEST(transform_feedback, a_feedback_loop_sees_each_capture)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec4 position;\n"
        "void main() { gl_Position = position * 2.0; }\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    static const char *varyings[] = { "gl_Position" };
    static const float init[4] = { 1, 2, 3, 4 };
    char log[1024] = "";
    GLuint prog = linkRecording(vs, fs, varyings, 1, GL_INTERLEAVED_ATTRIBS, log, sizeof log);
    GLuint xfb, bo[2], vao[2];
    float got[4] = {0};
    int src = 0;

    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return;

    glUseProgram(prog);
    glGenTransformFeedbacks(1, &xfb);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, xfb);
    glGenBuffers(2, bo);
    glBindBuffer(GL_ARRAY_BUFFER, bo[0]);
    glBufferData(GL_ARRAY_BUFFER, 16, init, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, bo[1]);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 16, NULL, GL_DYNAMIC_COPY);
    glGenVertexArrays(2, vao);

    for (int i = 0; i < 2; i++)
    {
        glBindVertexArray(vao[i]);
        glBindBuffer(GL_ARRAY_BUFFER, bo[i]);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(0);
    }

    glBindVertexArray(vao[0]);
    glEnable(GL_RASTERIZER_DISCARD);

    for (int d = 0; d < 3; d++)
    {
        if (d)
        {
            glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, bo[src]);
            src = 1 - src;
            glBindVertexArray(vao[src]);
        }

        glBeginTransformFeedback(GL_POINTS);

        if (d == 0)
            glDrawArrays(GL_POINTS, 0, 1);
        else
            glDrawTransformFeedback(GL_POINTS, xfb);

        glEndTransformFeedback();
    }

    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);
    CHECK_MSG(got[0] == 8 && got[1] == 16 && got[2] == 24 && got[3] == 32,
              "after three passes the buffer holds %g %g %g %g, want 8 16 24 32", got[0], got[1], got[2], got[3]);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(2, bo);
    glDeleteVertexArrays(2, vao);
    glDeleteTransformFeedbacks(1, &xfb);
}

// An array of doubles read as floats, sharing its binding with a float
// attribute, and doubles read as doubles from fewer components than the
// shader declares. Metal has neither format; the first used to fail the draw.
GPU_TEST(transform_feedback, double_arrays_feed_float_and_double_inputs)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec4 f;\n"
        "layout(location = 1) in vec4 g;\n"
        "layout(location = 2) in dvec4 d;\n"
        "out vec4 of;\n"
        "out vec4 og;\n"
        "out vec4 od;\n"
        "void main() { of = f; og = g; od = vec4(d); }\n";
    static const char *varyings[] = { "of", "og", "od" };
    // per vertex: 4 doubles, 4 floats, then 2 more doubles
    struct { GLdouble d[4]; GLfloat f[4]; GLdouble e[2]; } data[2] = {
        { { 1, 2, 3, 4 }, { 5, 6, 7, 8 }, { 20, 21 } },
        { { 9, 10, 11, 12 }, { 13, 14, 15, 16 }, { 22, 23 } },
    };
    char log[1024] = "";
    GLuint prog = glCreateProgram(), v = compileOne(GL_VERTEX_SHADER, vs);
    GLuint vao, vbo, buf;
    GLint ok = 0;
    float got[24] = {0};

    glAttachShader(prog, v);
    glTransformFeedbackVaryings(prog, 3, varyings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(prog, sizeof log, NULL, log);
    CHECK_MSG(ok, "program did not link: %s", log);
    if (!ok)
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_STATIC_DRAW);
    glVertexAttribFormat(0, 4, GL_DOUBLE, GL_FALSE, 0);
    glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE, 32);
    glVertexAttribLFormat(2, 2, GL_DOUBLE, 48);

    for (GLuint a = 0; a < 3; a++)
    {
        glVertexAttribBinding(a, 0);
        glEnableVertexAttribArray(a);
    }

    glBindVertexBuffer(0, vbo, 0, sizeof data[0]);

    glUseProgram(prog);
    buf = recordingBuffer(0, sizeof got);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 2);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);

    static const float want[24] = { 1, 2, 3, 4,  5, 6, 7, 8,  20, 21, 0, 1,
                                    9, 10, 11, 12,  13, 14, 15, 16,  22, 23, 0, 1 };

    for (int i = 0; i < 24; i++)
        CHECK_MSG(got[i] == want[i], "value %d recorded %g, want %g", i, got[i], want[i]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteShader(v);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

// Formats Metal has no vertex format for, read as floats: 32-bit integers,
// normalised or not, the packed ten-bit format unnormalised, and bytes in
// BGRA order. The draw used to fail or read the raw bits.
GPU_TEST(transform_feedback, vertex_formats_metal_lacks_are_converted)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec4 a;\n"
        "layout(location = 1) in vec4 b;\n"
        "layout(location = 2) in vec4 c;\n"
        "layout(location = 3) in vec4 d;\n"
        "out vec4 oa;\n"
        "out vec4 ob;\n"
        "out vec4 oc;\n"
        "out vec4 od;\n"
        "void main() { oa = a; ob = b; oc = c; od = d; }\n";
    static const char *varyings[] = { "oa", "ob", "oc", "od" };
    struct {
        GLint i[4];
        GLuint u[4];
        GLuint packed;
        GLubyte bgra[4];
    } data = {
        { -3, 5, 7, -9 },
        { 0, 0xffffffffu, 0, 0xffffffffu },
        1u | (2u << 10) | (3u << 20) | (1u << 30),
        { 0, 0, 255, 255 },
    };
    char log[1024] = "";
    GLuint prog = glCreateProgram(), v = compileOne(GL_VERTEX_SHADER, vs);
    GLuint vao, vbo, buf;
    GLint ok = 0;
    float got[16] = {0};

    glAttachShader(prog, v);
    glTransformFeedbackVaryings(prog, 4, varyings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glGetProgramInfoLog(prog, sizeof log, NULL, log);
    CHECK_MSG(ok, "program did not link: %s", log);
    if (!ok)
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof data, &data, GL_STATIC_DRAW);
    glVertexAttribFormat(0, 4, GL_INT, GL_FALSE, 0);
    glVertexAttribFormat(1, 4, GL_UNSIGNED_INT, GL_TRUE, 16);
    glVertexAttribFormat(2, 4, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 32);
    glVertexAttribFormat(3, GL_BGRA, GL_UNSIGNED_BYTE, GL_TRUE, 36);

    for (GLuint a = 0; a < 4; a++)
    {
        glVertexAttribBinding(a, 0);
        glEnableVertexAttribArray(a);
    }

    glBindVertexBuffer(0, vbo, 0, sizeof data);

    glUseProgram(prog);
    buf = recordingBuffer(0, sizeof got);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 1);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof got, got);

    static const float want[16] = { -3, 5, 7, -9,  0, 1, 0, 1,  1, 2, 3, 1,  1, 0, 0, 1 };

    for (int i = 0; i < 16; i++)
        CHECK_MSG(got[i] == want[i], "value %d recorded %g, want %g", i, got[i], want[i]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteShader(v);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}
