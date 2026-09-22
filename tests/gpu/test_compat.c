/*
 * test_compat.c
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
 * The compatibility profile leftovers MGL keeps for old games: the alpha
 * test, and GL_CLAMP as a wrap mode.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"
#include "mgl_compat.h"

#ifdef MGL_COMPAT_PROFILE

/* The core headers dropped these, so the test spells them out. */
#define MGL_GL_ALPHA_TEST      0x0BC0
#define MGL_GL_ALPHA_TEST_FUNC 0x0BC1
#define MGL_GL_ALPHA_TEST_REF  0x0BC2
#define MGL_GL_CLAMP           0x2900

extern void glAlphaFunc(GLenum func, GLfloat ref);

static const char *QUAD_VS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

/* Red, with the alpha the test wants to try. */
static const char *ALPHA_FS =
    "#version 460 core\n"
    "uniform float a;\n"
    "out vec4 o;\n"
    "void main() { o = vec4(1.0, 0.0, 0.0, a); }\n";

/* Draws the quad over a blue clear and reports the centre pixel's red.
   Red means the fragment survived, blue means the test discarded it. */
static int drawAndReadRed(GLuint prog, float alpha)
{
    MGLTestTarget target;
    GLuint vbo = 0, vao;
    unsigned char rgba[4] = {0};
    unsigned char *px;

    if (!mgl_target_create(&target, 8, 8, GL_RGBA8, 0))
        return -1;

    mgl_target_bind(&target);
    vao = mgl_fullscreen_quad(&vbo);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "a"), alpha);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glFinish();

    px = mgl_read_rgba8(&target);

    if (px)
        mgl_pixel_at(px, &target, 4, 4, rgba);

    free(px);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&target);

    return px ? rgba[0] : -1;
}

/* ---------- the alpha test ---------- */

GPU_TEST(compat, alpha_test_discards_below_the_reference)
{
    char log[2048] = "";
    GLuint prog = mgl_build_program(QUAD_VS, ALPHA_FS, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    glEnable(MGL_GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // 0.25 is not greater than 0.5, so the blue clear survives
    CHECK_EQ_INT(drawAndReadRed(prog, 0.25f), 0);

    // 0.75 is, so the red draw lands
    CHECK_EQ_INT(drawAndReadRed(prog, 0.75f), 255);

    glDisable(MGL_GL_ALPHA_TEST);
    glDeleteProgram(prog);
}

GPU_TEST(compat, alpha_test_does_nothing_until_it_is_enabled)
{
    char log[2048] = "";
    GLuint prog = mgl_build_program(QUAD_VS, ALPHA_FS, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    // the same compare that discarded above, with the test switched off
    glAlphaFunc(GL_GREATER, 0.5f);
    CHECK_EQ_INT(drawAndReadRed(prog, 0.25f), 255);

    glDeleteProgram(prog);
}

GPU_TEST(compat, alpha_test_honours_every_compare)
{
    struct { GLenum func; int passes; } cases[] = {
        { GL_NEVER,    0 },
        { GL_LESS,     1 },  // 0.25 <  0.5
        { GL_EQUAL,    0 },
        { GL_LEQUAL,   1 },
        { GL_GREATER,  0 },
        { GL_NOTEQUAL, 1 },
        { GL_GEQUAL,   0 },
        { GL_ALWAYS,   1 },
    };
    char log[2048] = "";
    GLuint prog = mgl_build_program(QUAD_VS, ALPHA_FS, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    glEnable(MGL_GL_ALPHA_TEST);

    for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++)
    {
        glAlphaFunc(cases[i].func, 0.5f);

        int red = drawAndReadRed(prog, 0.25f);

        CHECK_MSG(red == (cases[i].passes ? 255 : 0),
                  "compare 0x%x with alpha 0.25 ref 0.5 read red %d, wanted %d",
                  cases[i].func, red, cases[i].passes ? 255 : 0);
    }

    glDisable(MGL_GL_ALPHA_TEST);
    glDeleteProgram(prog);
}

GPU_TEST(compat, alpha_test_state_reads_back)
{
    GLint func = 0;
    GLfloat ref = -1.0f;

    CHECK_EQ_INT(glIsEnabled(MGL_GL_ALPHA_TEST), GL_FALSE);

    glEnable(MGL_GL_ALPHA_TEST);
    CHECK_EQ_INT(glIsEnabled(MGL_GL_ALPHA_TEST), GL_TRUE);

    glAlphaFunc(GL_GEQUAL, 0.25f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegerv(MGL_GL_ALPHA_TEST_FUNC, &func);
    glGetFloatv(MGL_GL_ALPHA_TEST_REF, &ref);
    CHECK_EQ_UINT((GLenum)func, GL_GEQUAL);
    CHECK_MSG(ref > 0.24f && ref < 0.26f, "reference read back %f", ref);

    // GL clamps the reference into [0, 1] as it stores it
    glAlphaFunc(GL_GEQUAL, 4.0f);
    glGetFloatv(MGL_GL_ALPHA_TEST_REF, &ref);
    CHECK_MSG(ref > 0.99f && ref < 1.01f, "reference 4.0 stored as %f", ref);

    glDisable(MGL_GL_ALPHA_TEST);
    CHECK_EQ_INT(glIsEnabled(MGL_GL_ALPHA_TEST), GL_FALSE);
}

GPU_TEST(compat, alpha_func_rejects_a_compare_it_does_not_know)
{
    mgl_drain_errors();

    glAlphaFunc(GL_TRIANGLES, 0.5f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- GL_CLAMP, the wrap mode that came before the two ---------- */

GPU_TEST(compat, legacy_clamp_is_stored_as_clamp_to_border)
{
    GLuint tex = 0, samp = 0;
    GLint got = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, MGL_GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, MGL_GL_CLAMP);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &got);
    CHECK_EQ_UINT((GLenum)got, GL_CLAMP_TO_BORDER);

    got = 0;
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &got);
    CHECK_EQ_UINT((GLenum)got, GL_CLAMP_TO_BORDER);

    glGenSamplers(1, &samp);
    glSamplerParameteri(samp, GL_TEXTURE_WRAP_S, MGL_GL_CLAMP);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    got = 0;
    glGetSamplerParameteriv(samp, GL_TEXTURE_WRAP_S, &got);
    CHECK_EQ_UINT((GLenum)got, GL_CLAMP_TO_BORDER);

    glDeleteSamplers(1, &samp);
    glDeleteTextures(1, &tex);
}

GPU_TEST(compat, legacy_clamp_samples_the_border_colour)
{
    static const char *fs =
        "#version 460 core\n"
        "uniform sampler2D s;\n"
        "out vec4 o;\n"
        // well outside [0, 1], so the wrap mode decides what comes back
        "void main() { o = texture(s, vec2(3.0, 3.0)); }\n";
    char log[2048] = "";
    GLuint prog = mgl_build_program(QUAD_VS, fs, log, sizeof log);
    GLuint tex = 0, vbo = 0, vao;
    // Metal keeps three border colours, so the test asks for one of them
    GLfloat border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    unsigned char red[4] = { 255, 0, 0, 255 };
    unsigned char rgba[4] = {0};
    unsigned char *px;
    MGLTestTarget target;

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    CHECK(mgl_target_create(&target, 8, 8, GL_RGBA8, 0));
    mgl_target_bind(&target);
    vao = mgl_fullscreen_quad(&vbo);

    // after the target, whose own colour texture lands on a unit as it is made
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, red);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, MGL_GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, MGL_GL_CLAMP);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "s"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glFinish();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&target);

    if (px)
        mgl_pixel_at(px, &target, 4, 4, rgba);

    // the white border, not the red texel
    CHECK_MSG(rgba[0] > 200 && rgba[1] > 200 && rgba[2] > 200,
              "sampled outside the texture and read %u,%u,%u,%u", rgba[0], rgba[1], rgba[2], rgba[3]);

    free(px);
    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&target);
}

#endif /* MGL_COMPAT_PROFILE */
