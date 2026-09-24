/*
 * test_texture_channels.c
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
 * What a shader reads from channels the GL format does not have, and what a
 * draw writes into an sRGB image. Metal stores RGB as RGBA and leaves a depth
 * texture's other channels undefined, so GL's answers have to be made.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static const char *VS =
    "#version 460 core\n"
    "void main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));"
    "gl_Position=vec4(p[gl_VertexID],0,1);}\n";

// draws one full-screen triangle into a 4x4 RGBA8 target and reads its corner
static int draw_and_read(const char *fs, GLuint tex, GLenum target, GLubyte out[4])
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS, fs, err, sizeof err);
    MGLTestTarget t;
    GLuint vao = 0;
    unsigned char *px;

    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog)
        return 0;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(target, tex);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    px = mgl_read_rgba8(&t);
    if (px)
        mgl_pixel_at(px, &t, 1, 1, out);
    free(px);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);

    return px != NULL;
}

static void nearest(GLenum target)
{
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
}

// An RGB image drawn into keeps whatever alpha the shader wrote in Metal's
// RGBA storage; GL has no alpha there and reads 1
GPU_TEST(texture_channels, drawn_rgb_image_reads_alpha_as_one)
{
    static const char *FILL =
        "#version 460 core\n"
        "out vec4 o;void main(){o=vec4(0.5,0.25,0.75,0.0);}\n";
    static const char *READ =
        "#version 460 core\n"
        "uniform sampler2D s;out vec4 o;void main(){o=texelFetch(s,ivec2(0),0);}\n";
    char err[1024] = { 0 };
    GLuint tex, fb, vao, prog;
    GLubyte c[4] = { 0 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    nearest(GL_TEXTURE_2D);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, 4, 4);

    prog = mgl_build_program(VS, FILL, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (draw_and_read(READ, tex, GL_TEXTURE_2D, c))
    {
        CHECK_MSG(abs(c[0] - 128) <= 1, "red %d", c[0]);
        CHECK_MSG(abs(c[2] - 191) <= 1, "blue %d", c[2]);
        CHECK_EQ_INT(255, c[3]);
    }

    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// GL reads a depth texture as (d, 0, 0, 1); Metal leaves the last three undefined
GPU_TEST(texture_channels, depth_texture_reads_zero_and_one_elsewhere)
{
    static const char *READ =
        "#version 460 core\n"
        "uniform sampler2D s;out vec4 o;"
        "void main(){vec4 t=texelFetch(s,ivec2(0),0);o=vec4(t.g,t.b,t.a,t.r);}\n";
    GLfloat depth[16];
    GLuint tex;
    GLubyte c[4] = { 0 };

    for (int i = 0; i < 16; i++)
        depth[i] = 0.5f;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 4, 4, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
    nearest(GL_TEXTURE_2D);

    if (draw_and_read(READ, tex, GL_TEXTURE_2D, c))
    {
        CHECK_EQ_INT(0, c[0]);
        CHECK_EQ_INT(0, c[1]);
        CHECK_EQ_INT(255, c[2]);
        CHECK_MSG(abs(c[3] - 128) <= 1, "depth %d", c[3]);
    }

    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    glDeleteTextures(1, &tex);
}

// GL_STENCIL_INDEX samples the stencil half of a depth-stencil texture
GPU_TEST(texture_channels, stencil_mode_reads_the_stencil)
{
    static const char *READ =
        "#version 460 core\n"
        "uniform usampler2D s;out vec4 o;"
        "void main(){uint v=texelFetch(s,ivec2(0),0).r;o=vec4(float(v)/255.0,0,0,1);}\n";
    GLuint packed[16];
    GLuint tex;
    GLubyte c[4] = { 0 };

    for (int i = 0; i < 16; i++)
        packed[i] = (0x800000u << 8) | 0x5Au;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, 4, 4, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);
    nearest(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);

    if (draw_and_read(READ, tex, GL_TEXTURE_2D, c))
        CHECK_EQ_INT(0x5A, c[0]);

    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    glDeleteTextures(1, &tex);
}

static GLubyte draw_half_into_srgb(int encode)
{
    static const char *FILL =
        "#version 460 core\n"
        "out vec4 o;void main(){o=vec4(0.5);}\n";
    char err[1024] = { 0 };
    GLuint tex, fb, vao, prog;
    GLubyte texel[4 * 4 * 4];

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    nearest(GL_TEXTURE_2D);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, 4, 4);

    if (encode)
        glEnable(GL_FRAMEBUFFER_SRGB);

    prog = mgl_build_program(VS, FILL, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    memset(texel, 0, sizeof texel);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);

    return texel[0];
}

// GL only encodes into an sRGB image while GL_FRAMEBUFFER_SRGB is on
GPU_TEST(texture_channels, framebuffer_srgb_decides_the_encoding)
{
    GLubyte raw = draw_half_into_srgb(0);
    GLubyte encoded = draw_half_into_srgb(1);

    CHECK_MSG(abs(raw - 128) <= 1, "raw write stored %d", raw);
    CHECK_MSG(abs(encoded - 188) <= 1, "encoded write stored %d", encoded);
}

// a swizzle applies to sampling only; the texture can still be drawn into
GPU_TEST(texture_channels, swizzled_texture_can_be_drawn_into)
{
    static const char *FILL =
        "#version 460 core\n"
        "out vec4 o;void main(){o=vec4(1.0,0.0,0.0,1.0);}\n";
    static const char *READ =
        "#version 460 core\n"
        "uniform sampler2D s;out vec4 o;void main(){o=texelFetch(s,ivec2(0),0);}\n";
    char err[1024] = { 0 };
    GLuint tex, fb, vao, prog;
    GLubyte raw[4 * 4 * 4], c[4] = { 0 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    nearest(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_GREEN);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    CHECK_EQ_UINT(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    glViewport(0, 0, 4, 4);

    prog = mgl_build_program(VS, FILL, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // the stored texel is what the shader wrote
    glReadPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, raw);
    CHECK_EQ_INT(255, raw[0]);
    CHECK_EQ_INT(0, raw[1]);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // and sampling swaps red and green
    if (draw_and_read(READ, tex, GL_TEXTURE_2D, c))
    {
        CHECK_EQ_INT(0, c[0]);
        CHECK_EQ_INT(255, c[1]);
    }

    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_channels, swizzle_rgba_answers_four_values)
{
    GLint iv[4] = { 0 }, dsa[4] = { 0 };
    GLfloat fv[4] = { 0 };
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);

    glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, iv);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    CHECK_EQ_INT(GL_RED, iv[0]);
    CHECK_EQ_INT(GL_GREEN, iv[1]);
    CHECK_EQ_INT(GL_ONE, iv[2]);
    CHECK_EQ_INT(GL_ALPHA, iv[3]);

    glGetTextureParameteriv(tex, GL_TEXTURE_SWIZZLE_RGBA, dsa);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    CHECK_EQ_INT(GL_ONE, dsa[2]);

    glGetTextureParameterfv(tex, GL_TEXTURE_SWIZZLE_RGBA, fv);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    CHECK_EQ_INT(GL_ALPHA, (GLint)fv[3]);

    glDeleteTextures(1, &tex);
}

// GL_RGB12 claims 12 bits a channel, so it cannot be kept in 8
GPU_TEST(texture_channels, rgb12_keeps_twelve_bits)
{
    GLushort in[3] = { 0x1230, 0x8880, 0xFED0 };
    GLushort out[4 * 3];
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB12, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT, in);

    memset(out, 0, sizeof out);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_SHORT, out);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    for (int i = 0; i < 3; i++)
        CHECK_MSG(abs((int)out[i] - (int)in[i]) <= 16, "channel %d: 0x%04x back as 0x%04x", i, in[i], out[i]);

    glDeleteTextures(1, &tex);
}
