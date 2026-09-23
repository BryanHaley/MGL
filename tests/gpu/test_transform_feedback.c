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
