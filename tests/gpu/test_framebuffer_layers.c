/*
 * test_framebuffer_layers.c
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
 * A framebuffer attachment names one level and one layer of its texture. The
 * render pass and the readback both used to ignore them, so every draw went
 * to level 0, layer 0 -- and reading back the same wrong image hid it.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static const char *VS =
    "#version 460 core\n"
    "void main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));"
    "gl_Position=vec4(p[gl_VertexID],0,1);}\n";

static const char *RED =
    "#version 460 core\n"
    "out vec4 o;void main(){o=vec4(1,0,0,1);}\n";

// one full-screen triangle in red into whatever framebuffer is bound
static void draw_red(GLsizei w, GLsizei h)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS, RED, err, sizeof err), vao;

    CHECK_MSG(prog != 0, "link: %s", err);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glViewport(0, 0, w, h);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
}

static GLuint fbo_for(void)
{
    GLuint fb;

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    return fb;
}

GPU_TEST(framebuffer_layers, draws_into_the_attached_array_layer)
{
    GLubyte px[3 * 4 * 4 * 4];
    GLuint tex, fb;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 4, 4, 3);

    fb = fbo_for();
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 2);
    CHECK_EQ_UINT(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glClearColor(0, 0, 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 2);
    draw_red(4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    memset(px, 0x77, sizeof px);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

    CHECK_EQ_INT(0, px[0]);                  /* layer 0 untouched */
    CHECK_EQ_INT(255, px[2 * 64 + 0]);       /* layer 2 red */
    CHECK_EQ_INT(0, px[2 * 64 + 1]);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

GPU_TEST(framebuffer_layers, draws_into_the_attached_mip_level)
{
    GLubyte zero[8 * 8 * 4] = { 0 }, l0[8 * 8 * 4], l1[4 * 4 * 4];
    GLuint tex, fb;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    fb = fbo_for();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 1);
    CHECK_EQ_UINT(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    draw_red(4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindTexture(GL_TEXTURE_2D, tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, l0);
    glGetTexImage(GL_TEXTURE_2D, 1, GL_RGBA, GL_UNSIGNED_BYTE, l1);

    CHECK_EQ_INT(0, l0[0]);
    CHECK_EQ_INT(255, l1[0]);
    CHECK_EQ_INT(255, l1[4 * 4 * 4 - 1]);   /* the whole of level 1 */
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

GPU_TEST(framebuffer_layers, draws_into_the_attached_cube_face)
{
    GLubyte zero[4 * 4 * 4] = { 0 }, px[4 * 4 * 4], other[4 * 4 * 4];
    GLuint tex, fb;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    for (int f = 0; f < 6; f++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero);

    fb = fbo_for();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, tex, 0);
    CHECK_EQ_UINT(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    draw_red(4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, GL_UNSIGNED_BYTE, other);

    CHECK_EQ_INT(255, px[0]);
    CHECK_EQ_INT(0, other[0]);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

GPU_TEST(framebuffer_layers, draws_into_the_attached_3d_slice)
{
    GLubyte zero[4 * 4 * 4 * 4] = { 0 }, px[4 * 4 * 4 * 4];
    GLuint tex, fb;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero);

    fb = fbo_for();
    glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, tex, 0, 2);
    CHECK_EQ_UINT(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    draw_red(4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindTexture(GL_TEXTURE_3D, tex);
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

    CHECK_EQ_INT(0, px[0]);
    CHECK_EQ_INT(255, px[2 * 64]);
    CHECK_EQ_INT(0, px[3 * 64]);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

GPU_TEST(framebuffer_layers, reads_back_the_attached_layer)
{
    GLubyte layers[2][4 * 4 * 4], out[4 * 4 * 4];
    GLuint tex, fb;

    memset(layers[0], 0x11, sizeof layers[0]);
    memset(layers[1], 0xC8, sizeof layers[1]);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 4, 4, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, layers);

    fb = fbo_for();
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    memset(out, 0, sizeof out);
    glReadPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, out);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    CHECK_EQ_INT(0xC8, out[0]);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// the one that found it: a multisample array drawn a layer at a time
GPU_TEST(framebuffer_layers, draws_into_a_multisample_array_layer)
{
    static const char *READ =
        "#version 460 core\n"
        "uniform sampler2DMSArray s;out vec4 o;"
        "void main(){o=texelFetch(s,ivec3(0,0,1),0);}\n";
    char err[1024] = { 0 };
    MGLTestTarget t;
    GLuint tex, fb, prog, vao;
    unsigned char *px;
    GLubyte c[4] = { 0 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, tex);
    glTexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 1, GL_RGBA8, 4, 4, 2, GL_FALSE);

    fb = fbo_for();
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1);
    CHECK_EQ_UINT(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
    draw_red(4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    prog = mgl_build_program(VS, READ, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, tex);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, 1, 1, c);
        CHECK_EQ_INT(255, c[0]);
        free(px);
    }
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

static GLuint rgba_tex(void)
{
    GLubyte zero[4 * 4 * 4] = { 0 };
    GLuint t;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    return t;
}

static GLubyte red_of(GLuint t)
{
    GLubyte px[4 * 4 * 4];

    glBindTexture(GL_TEXTURE_2D, t);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    return px[0];
}

// output 0 goes to whatever glDrawBuffer names, not to attachment 0
GPU_TEST(framebuffer_layers, draw_buffer_picks_the_attachment)
{
    GLuint a = rgba_tex(), b = rgba_tex(), fb = fbo_for();

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, a, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, b, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    draw_red(4, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    CHECK_EQ_INT(0, red_of(a));
    CHECK_EQ_INT(255, red_of(b));
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
}

// a GL_NONE slot writes nowhere, and a later slot still gets its own output
GPU_TEST(framebuffer_layers, draw_buffers_leave_a_none_slot_alone)
{
    static const char *TWO =
        "#version 460 core\n"
        "layout(location=0) out vec4 o0;layout(location=1) out vec4 o1;"
        "void main(){o0=vec4(1,0,0,1);o1=vec4(0.5,0,0,1);}\n";
    static const GLenum bufs[2] = { GL_NONE, GL_COLOR_ATTACHMENT0 };
    char err[1024] = { 0 };
    GLuint a = rgba_tex(), fb = fbo_for(), prog, vao;

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, a, 0);
    glDrawBuffers(2, bufs);

    prog = mgl_build_program(VS, TWO, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glViewport(0, 0, 4, 4);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // attachment 0 sits in slot 1, so it gets output 1
    CHECK_MSG(abs(red_of(a) - 128) <= 1, "attachment 0 holds %d", red_of(a));
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &a);
}

// glClearBuffer's drawbuffer is a slot in glDrawBuffers' list
GPU_TEST(framebuffer_layers, clear_buffer_clears_the_draw_buffer_slot)
{
    static const GLfloat white[4] = { 1, 1, 1, 1 };
    GLuint a = rgba_tex(), b = rgba_tex(), fb = fbo_for();

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, a, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, b, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT1);
    glClearBufferfv(GL_COLOR, 0, white);
    glFinish();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    CHECK_EQ_INT(0, red_of(a));
    CHECK_EQ_INT(255, red_of(b));
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
}

// Deleting a renderbuffer takes it off the framebuffers bound now and no
// others; the conformance tests delete one by mistake and expect exactly this
GPU_TEST(framebuffer_layers, a_deleted_renderbuffer_stays_on_an_unbound_framebuffer)
{
    GLuint keeper, other, depth;
    GLint type = 0;

    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 4, 4);

    glGenFramebuffers(1, &keeper);
    glBindFramebuffer(GL_FRAMEBUFFER, keeper);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);

    glGenFramebuffers(1, &other);
    glBindFramebuffer(GL_FRAMEBUFFER, other);
    glDeleteRenderbuffers(1, &depth);

    glBindFramebuffer(GL_FRAMEBUFFER, keeper);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    CHECK_EQ_INT(GL_RENDERBUFFER, type);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    // and once that framebuffer lets go, it is gone
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    CHECK_EQ_INT(GL_NONE, type);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &keeper);
    glDeleteFramebuffers(1, &other);
}

static const char *FRAG_DEPTH_GREEN =
    "#version 460 core\n"
    "out vec4 o;void main(){o=vec4(0,1,0,1);gl_FragDepth=0.3;}\n";

// a shader writing gl_FragDepth into a target with no depth buffer: the depth
// goes nowhere and the colour still lands, test on or off
GPU_TEST(framebuffer_layers, frag_depth_into_a_colour_only_target_still_draws)
{
    char err[1024] = { 0 };
    GLuint tex = rgba_tex(), fb = fbo_for(), prog, vao;

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    CHECK_EQ_UINT(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    prog = mgl_build_program(VS, FRAG_DEPTH_GREEN, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glViewport(0, 0, 4, 4);

    for (int test = 0; test < 2; test++)
    {
        GLubyte px[4 * 4 * 4];

        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        if (test)
        {
            // no depth buffer means the test always passes
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
        }

        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisable(GL_DEPTH_TEST);

        glReadPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, px);
        CHECK_MSG(px[1] == 255, "depth test %s: green %d", test ? "on" : "off", px[1]);
    }

    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// one pass drawing with and without gl_FragDepth: every draw lands
GPU_TEST(framebuffer_layers, frag_depth_and_plain_draws_share_a_pass)
{
    char err[1024] = { 0 };
    GLuint tex = rgba_tex(), fb = fbo_for(), plain, depth, vao;
    GLubyte px[4 * 4 * 4];

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, 4, 4);

    plain = mgl_build_program(VS, RED, err, sizeof err);
    CHECK_MSG(plain != 0, "link: %s", err);
    depth = mgl_build_program(VS, FRAG_DEPTH_GREEN, err, sizeof err);
    CHECK_MSG(depth != 0, "link: %s", err);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // left half plain, right half with gl_FragDepth, then plain again over
    // the left: each needs its pipeline to match the pass it lands in
    glEnable(GL_SCISSOR_TEST);
    glUseProgram(plain);
    glScissor(0, 0, 2, 4);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glUseProgram(depth);
    glScissor(2, 0, 2, 4);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glUseProgram(plain);
    glScissor(0, 0, 1, 4);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_SCISSOR_TEST);

    glReadPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_MSG(px[0] == 255 && px[1] == 0, "left %d %d", px[0], px[1]);
    CHECK_MSG(px[3 * 4 + 1] == 255, "right green %d", px[3 * 4 + 1]);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteProgram(plain);
    glDeleteProgram(depth);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}
