/*
 * test_conditional_render.c
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
 * Between glBeginConditionalRender and glEndConditionalRender, draws, clears
 * and compute dispatches only happen when the query saw samples pass. MGL
 * used to record the query and then run everything regardless.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static const char *VS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 p;\n"
    "void main(){gl_Position=vec4(p,0,1);}\n";

static const char *GREEN =
    "#version 460 core\n"
    "out vec4 o;void main(){o=vec4(0,1,0,1);}\n";

static const char *COUNT_CS =
    "#version 460\n"
    "layout(local_size_x = 1) in;\n"
    "layout(std430, binding = 0) buffer Out { uint n; };\n"
    "void main() { atomicAdd(n, 1u); }\n";

// one query that saw a sample pass and one that saw nothing
static void make_queries(GLuint prog, GLuint vao, GLuint q[2])
{
    glGenQueries(2, q);

    glUseProgram(prog);
    glBindVertexArray(vao);

    glBeginQuery(GL_ANY_SAMPLES_PASSED, q[0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEndQuery(GL_ANY_SAMPLES_PASSED);

    glBeginQuery(GL_ANY_SAMPLES_PASSED, q[1]);
    glEndQuery(GL_ANY_SAMPLES_PASSED);
}

static void pixel(const MGLTestTarget *t, unsigned char rgba[4])
{
    unsigned char *px = mgl_read_rgba8(t);

    memset(rgba, 0, 4);

    if (px)
    {
        mgl_pixel_at(px, t, 1, 1, rgba);
        free(px);
    }
}

GPU_TEST(conditional_render, a_failed_query_throws_draws_and_clears_away)
{
    char err[1024] = { 0 };
    MGLTestTarget t;
    GLuint prog, vao, vbo, q[2];
    unsigned char rgba[4];

    prog = mgl_build_program(VS, GREEN, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog) return;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);
    vao = mgl_fullscreen_quad(&vbo);

    make_queries(prog, vao, q);

    // nothing passed: the clear to red and the green draw are both dropped
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBeginConditionalRender(q[1], GL_QUERY_WAIT);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEndConditionalRender();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    pixel(&t, rgba);
    CHECK_MSG(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 255,
              "skipped: got %d,%d,%d", rgba[0], rgba[1], rgba[2]);

    // the inverted mode runs exactly when nothing passed
    glBeginConditionalRender(q[1], GL_QUERY_WAIT_INVERTED);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glEndConditionalRender();

    pixel(&t, rgba);
    CHECK_MSG(rgba[0] == 0 && rgba[1] == 255 && rgba[2] == 0,
              "inverted: got %d,%d,%d", rgba[0], rgba[1], rgba[2]);

    // samples passed: the clear happens
    glBeginConditionalRender(q[0], GL_QUERY_WAIT);
    glClear(GL_COLOR_BUFFER_BIT);
    glEndConditionalRender();

    pixel(&t, rgba);
    CHECK_MSG(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0,
              "ran: got %d,%d,%d", rgba[0], rgba[1], rgba[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteQueries(2, q);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(conditional_render, a_failed_query_throws_dispatches_away)
{
    static const GLuint groups[3] = { 3, 1, 1 };
    char err[1024] = { 0 };
    MGLTestTarget t;
    GLuint prog, cs, vao, vbo, out, args, q[2];
    GLuint zero = 0, n = 0;

    prog = mgl_build_program(VS, GREEN, err, sizeof err);
    cs = mgl_build_compute_program(COUNT_CS, err, sizeof err);
    CHECK_MSG(prog != 0 && cs != 0, "link: %s", err);
    if (!prog || !cs) return;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);
    vao = mgl_fullscreen_quad(&vbo);

    make_queries(prog, vao, q);

    glGenBuffers(1, &out);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, &zero, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, out);

    glGenBuffers(1, &args);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, args);
    glBufferData(GL_DISPATCH_INDIRECT_BUFFER, sizeof groups, groups, GL_STATIC_DRAW);

    glUseProgram(cs);

    // only the one group under the passing query counts
    glBeginConditionalRender(q[1], GL_QUERY_WAIT);
    glDispatchCompute(2, 1, 1);
    glDispatchComputeIndirect(0);
    glEndConditionalRender();

    glBeginConditionalRender(q[0], GL_QUERY_WAIT);
    glDispatchCompute(1, 1, 1);
    glEndConditionalRender();

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof n, &n);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(n, 1);

    glUseProgram(0);
    glDeleteQueries(2, q);
    glDeleteBuffers(1, &out);
    glDeleteBuffers(1, &args);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glDeleteProgram(cs);
    mgl_target_destroy(&t);
}
