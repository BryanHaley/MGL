/*
 * test_occlusion_query.c
 * Copyright (C) The Moogle Project
 *
 * Occlusion queries and the counters that go with them. Metal counts fragments
 * into a buffer rather than into a query object, so a query that survives more
 * than one render encoder has to add its slots up.
 *
 * A query that always answers zero passes every error check there is, so every
 * test here checks the number.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdlib.h>
#include <string.h>

static const char *VS =
    "#version 410\n"
    "layout(location = 0) in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

static const char *FS =
    "#version 410\n"
    "out vec4 o;\n"
    "void main() { o = vec4(0, 1, 0, 1); }\n";

// A quad covering the left half of the viewport, so the count is known.
static GLuint halfQuad(GLuint *vbo_out)
{
    static const GLfloat v[12] = { -1,-1, 0,-1, -1,1,  0,-1, 0,1, -1,1 };
    GLuint vao, vbo;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    if (vbo_out)
        *vbo_out = vbo;

    return vao;
}

GPU_TEST(occlusion_query, samples_passed_counts_fragments)
{
    MGLTestTarget t;
    GLuint prog, vao, vbo, q;
    GLuint result = 0xdeadbeef;
    char log[1024];

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = halfQuad(&vbo);
    glViewport(0, 0, 32, 32);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEndQuery(GL_SAMPLES_PASSED);

    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &result);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // sixteen columns of thirty-two rows
    CHECK_MSG(result == 16 * 32, "the half quad covers 512 fragments, counted %u", result);

    glUseProgram(0);
    glDeleteQueries(1, &q);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// Two draws inside one query add up, even though Metal's counter belongs to
// whichever encoder was open at the time.
GPU_TEST(occlusion_query, two_draws_add_up)
{
    MGLTestTarget t;
    GLuint prog, vao, vbo, q;
    GLuint result = 0;
    char log[1024];

    prog = mgl_build_program(VS, FS, log, sizeof log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = halfQuad(&vbo);
    glViewport(0, 0, 32, 32);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEndQuery(GL_SAMPLES_PASSED);

    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &result);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(result == 2 * 16 * 32, "two half quads make 1024 fragments, counted %u", result);

    glUseProgram(0);
    glDeleteQueries(1, &q);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(occlusion_query, any_samples_passed_is_a_boolean)
{
    MGLTestTarget t;
    GLuint prog, vao, vbo, q_drew, q_empty;
    GLuint drew = 0xdeadbeef, empty = 0xdeadbeef;
    char log[1024];

    prog = mgl_build_program(VS, FS, log, sizeof log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = halfQuad(&vbo);
    glViewport(0, 0, 32, 32);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenQueries(1, &q_drew);
    glGenQueries(1, &q_empty);

    glBeginQuery(GL_ANY_SAMPLES_PASSED, q_drew);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEndQuery(GL_ANY_SAMPLES_PASSED);

    glBeginQuery(GL_ANY_SAMPLES_PASSED, q_empty);
    glEndQuery(GL_ANY_SAMPLES_PASSED);

    glGetQueryObjectuiv(q_drew, GL_QUERY_RESULT, &drew);
    glGetQueryObjectuiv(q_empty, GL_QUERY_RESULT, &empty);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // the spec says this one is GL_TRUE or GL_FALSE, not a count
    CHECK_EQ_UINT(drew, GL_TRUE);
    CHECK_EQ_UINT(empty, GL_FALSE);

    glUseProgram(0);
    glDeleteQueries(1, &q_drew);
    glDeleteQueries(1, &q_empty);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(occlusion_query, primitives_generated_counts_primitives)
{
    MGLTestTarget t;
    GLuint prog, vao, vbo, q;
    GLuint result = 0;
    char log[1024];

    prog = mgl_build_program(VS, FS, log, sizeof log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = halfQuad(&vbo);
    glViewport(0, 0, 32, 32);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenQueries(1, &q);
    glBeginQuery(GL_PRIMITIVES_GENERATED, q);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEndQuery(GL_PRIMITIVES_GENERATED);

    glGetQueryObjectuiv(q, GL_QUERY_RESULT, &result);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(result == 2, "six vertices make two triangles, counted %u", result);

    glUseProgram(0);
    glDeleteQueries(1, &q);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}
