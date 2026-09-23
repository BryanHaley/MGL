/*
 * test_plain_uniforms.c
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
 * Uniforms declared outside any block. Each one lives in a Metal buffer of
 * its own, which the shapes below got wrong on the way through SPIRV-Cross.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "mgl_test.h"
#include "harness.h"

static const char *VS =
    "#version 430 core\n"
    "layout(location = 0) in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

// draws a full target and hands back the centre pixel
static bool drawCentre(GLuint prog, unsigned char c[4])
{
    MGLTestTarget t;
    GLuint vao, vbo;
    unsigned char *px;

    if (!mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
        return false;

    mgl_target_bind(&t);
    glViewport(0, 0, 16, 16);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&t);

    if (px)
        mgl_pixel_at(px, &t, 8, 8, c);

    free(px);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);

    return px != NULL;
}

// A subroutine's index can be picked with layout(index = N). The layout was
// left in front of the plain function the rewrite made, so the shader did
// not compile, and the functions were numbered by position regardless. The
// functions also read a plain uniform from inside the dispatcher, which
// Metal refused: the dispatcher took it as a thread value.
GPU_TEST(plain_uniforms, a_subroutine_is_picked_by_its_declared_index)
{
    static const char *fs =
        "#version 430 core\n"
        "uniform float zero;\n"
        "subroutine vec4 pick_t(float p);\n"
        "layout(index = 4) subroutine(pick_t) vec4 red(float p) { return vec4(zero) + vec4(1, 0, 0, 1); }\n"
        "layout(index = 1) subroutine(pick_t) vec4 green(float p) { return vec4(zero) + vec4(0, 1, 0, 1); }\n"
        "subroutine uniform pick_t pick;\n"
        "out vec4 o;\n"
        "void main() { o = pick(0.0); }\n";
    GLuint prog, index;
    unsigned char c[4] = { 0 };
    char log[2048];

    prog = mgl_build_program(VS, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "subroutine program did not build: %s", log);

    if (!prog) return;

    CHECK_EQ_UINT(glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "red"), 4);
    CHECK_EQ_UINT(glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "green"), 1);

    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "zero"), 0.0f);

    index = 1;
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &index);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(drawCentre(prog, c));
    CHECK_MSG(c[1] > 200 && c[0] < 50, "index 1 drew %d,%d,%d, want green", c[0], c[1], c[2]);

    index = 4;
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &index);
    CHECK(drawCentre(prog, c));
    CHECK_MSG(c[0] > 200 && c[1] < 50, "index 4 drew %d,%d,%d, want red", c[0], c[1], c[2]);

    // 0 names no function in this shader
    index = 0;
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &index);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUseProgram(0);
    glDeleteProgram(prog);
}

// An array of structs outside a block was split into one Metal buffer per
// element, the way a block array is, and every member access through it
// failed to compile. Only the first element's members had locations, too.
GPU_TEST(plain_uniforms, an_array_of_structs_reads_each_element)
{
    static const char *fs =
        "#version 430 core\n"
        "struct S { float a; vec2 b; };\n"
        "uniform S sa[3];\n"
        "uniform int which;\n"
        "out vec4 o;\n"
        "vec4 look(int i) { return vec4(sa[i].a, sa[i].b, 1.0); }\n"
        "void main() { o = look(which); }\n";
    GLuint prog;
    unsigned char c[4] = { 0 };
    char log[2048];

    prog = mgl_build_program(VS, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "struct array program did not build: %s", log);

    if (!prog) return;

    glUseProgram(prog);

    for (int e = 0; e < 3; e++)
    {
        char name[32];

        snprintf(name, sizeof name, "sa[%d].a", e);
        CHECK_MSG(glGetUniformLocation(prog, name) >= 0, "%s has no location", name);
        glUniform1f(glGetUniformLocation(prog, name), 0.25f * (float)e);

        snprintf(name, sizeof name, "sa[%d].b", e);
        CHECK_MSG(glGetUniformLocation(prog, name) >= 0, "%s has no location", name);
        glUniform2f(glGetUniformLocation(prog, name), 0.5f, 0.125f * (float)(e + 1));
    }

    glUniform1i(glGetUniformLocation(prog, "which"), 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(drawCentre(prog, c));

    // sa[2] is 0.5, (0.5, 0.375)
    CHECK_MSG(abs(c[0] - 128) <= 2 && abs(c[1] - 128) <= 2 && abs(c[2] - 96) <= 2,
              "sa[2] drew %d,%d,%d, want 128,128,96", c[0], c[1], c[2]);

    glUseProgram(0);
    glDeleteProgram(prog);
}

// Implicit locations were handed out after the highest explicit one, so an
// explicit location at the top of the range pushed the rest past the end
// and the draw found nothing bound for them.
GPU_TEST(plain_uniforms, an_explicit_location_at_the_top_leaves_room_below)
{
    static const char *fs =
        "#version 430 core\n"
        "layout(location = 1023) uniform float top;\n"
        "uniform float a;\n"
        "uniform vec2 b[2];\n"
        "out vec4 o;\n"
        "void main() { o = vec4(top, a, b[1].x, 1.0); }\n";
    GLint max_loc = 0, la, lb;
    GLuint prog;
    unsigned char c[4] = { 0 };
    char log[2048];

    glGetIntegerv(GL_MAX_UNIFORM_LOCATIONS, &max_loc);
    if (max_loc != 1024) SKIP("the shader is written for 1024 locations");

    prog = mgl_build_program(VS, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not build: %s", log);

    if (!prog) return;

    la = glGetUniformLocation(prog, "a");
    lb = glGetUniformLocation(prog, "b");
    CHECK_MSG(la >= 0 && la < 1023 && lb >= 0 && lb + 1 < 1023 && (la < lb || la > lb + 1),
              "a at %d and b at %d do not fit below 1023 apart", la, lb);

    glUseProgram(prog);
    glUniform1f(1023, 1.0f);
    glUniform1f(la, 0.5f);
    glUniform2f(lb + 1, 0.25f, 0.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(drawCentre(prog, c));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(c[0] > 250 && abs(c[1] - 128) <= 2 && abs(c[2] - 64) <= 2,
              "drew %d,%d,%d, want 255,128,64", c[0], c[1], c[2]);

    glUseProgram(0);
    glDeleteProgram(prog);
}
