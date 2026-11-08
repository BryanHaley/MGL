/*
 * test_layered_rendering.c
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
 * A geometry shader that writes gl_Layer and gl_ViewportIndex routes the
 * primitive to the named layer and viewport. These tests verify that MGL's
 * geometry rewrite carries both built-ins through the generated passthrough
 * vertex shader into Metal's [[render_target_array_index]] and
 * [[viewport_array_index]].
 */

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

static GLuint linkVGF(const char *vs, const char *gs, const char *fs,
                      char *log, int log_size)
{
    GLuint prog = glCreateProgram();
    GLuint v, g, f;
    GLint ok = 0;

    if (log && log_size)
        log[0] = 0;

    v = compileOne(GL_VERTEX_SHADER, vs, log, log_size);
    g = compileOne(GL_GEOMETRY_SHADER, gs, log, log_size);
    f = compileOne(GL_FRAGMENT_SHADER, fs, log, log_size);

    if (!v || !g || !f)
        return 0;

    glAttachShader(prog, v);
    glAttachShader(prog, g);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok && log && log_size)
        glGetProgramInfoLog(prog, log_size, NULL, log);

    return ok ? prog : 0;
}

static int count_green(const MGLTestTarget *t)
{
    unsigned char *px = mgl_read_rgba8(t);
    int n = 0;

    if (!px)
        return -1;

    for (int i = 0; i < t->width * t->height; i++)
        if (px[i * 4 + 1] >= 200 && px[i * 4] < 60 && px[i * 4 + 2] < 60)
            n++;

    free(px);
    return n;
}

/* ---------- gl_Layer routes to a non-zero array layer ---------- */

GPU_TEST(layered_rendering, geometry_shader_gl_layer_writes_to_layer_1)
{
    // full-screen triangle in NDC: covers the entire viewport
    static const GLfloat tri[6] = { -1,-1, 3,-1, -1,3 };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        gl_Position = gl_in[i].gl_Position;\n"
        "        gl_Layer = 1;\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0,1,0,1); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo, tex, fb;
    char log[2048];
    int green_layer0, green_layer1;

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "geometry program did not link: %s", log);
    if (!prog)
        return;

    /* A 2D-array texture with two layers, 64x64 each. */
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 64, 64, 2);

    /* The whole array is attached, so gl_Layer alone picks the layer; with a
       single layer attached GL ignores gl_Layer and the test proves nothing. */
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);

    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* Read back layer 1: all green. */
    if (!mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        glDeleteProgram(prog);
        glDeleteFramebuffers(1, &fb);
        glDeleteTextures(1, &tex);
        return;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
    glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, t.fbo);
    glBlitFramebuffer(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    green_layer1 = count_green(&t);
    CHECK_MSG(green_layer1 == 64 * 64,
              "layer 1 should be 4096 green pixels, counted %d", green_layer1);

    /* Read back layer 0: still the black it was cleared to. */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
    glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, t.fbo);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlitFramebuffer(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    green_layer0 = count_green(&t);
    CHECK_MSG(green_layer0 == 0,
              "layer 0 should be black, counted %d green pixels", green_layer0);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
    mgl_target_destroy(&t);
}

/* ---------- gl_ViewportIndex is carried through ---------- */

GPU_TEST(layered_rendering, geometry_shader_gl_viewport_index_reaches_metal)
{
    /* Writing gl_ViewportIndex must not break the draw. Index 0 is all this
     * can check: only one viewport reaches Metal so far, so picking another
     * one is not implemented yet. */
    static const GLfloat tri[6] = { -1,-1, 3,-1, -1,3 };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        gl_Position = gl_in[i].gl_Position;\n"
        "        gl_ViewportIndex = 0;\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0,1,0,1); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    char log[2048];
    int green;

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "geometry program with gl_ViewportIndex did not link: %s", log);
    if (!prog)
        return;

    if (!mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
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
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glViewport(0, 0, 64, 64);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    green = count_green(&t);
    CHECK_MSG(green == 64 * 64,
              "triangle with gl_ViewportIndex=0 covered %d of 4096 pixels", green);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* ---------- default values: no write means layer 0, viewport 0 ---------- */

GPU_TEST(layered_rendering, default_layer_is_zero)
{
    /* A geometry shader that writes neither gl_Layer nor gl_ViewportIndex
     * should draw to layer 0 (the default). */
    static const GLfloat tri[6] = { -1,-1, 3,-1, -1,3 };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        gl_Position = gl_in[i].gl_Position;\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0,1,0,1); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    char log[2048];
    int green;

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "passthrough program did not link: %s", log);
    if (!prog)
        return;

    if (!mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
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
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glViewport(0, 0, 64, 64);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    green = count_green(&t);
    CHECK_MSG(green == 64 * 64,
              "passthrough with no layer write covered %d of 4096 pixels", green);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* ---------- gl_ViewportIndex picks a viewport ---------- */

// Two viewports side by side. The geometry shader sends the triangle through
// viewport 1, so only the right half fills. With one viewport reaching Metal
// it used to land on the left.
GPU_TEST(layered_rendering, gl_viewport_index_selects_the_viewport)
{
    static const GLfloat tri[6] = { -1,-1, 3,-1, -1,3 };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        gl_Position = gl_in[i].gl_Position;\n"
        "        gl_ViewportIndex = 1;\n"
        "        EmitVertex();\n"
        "    }\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0,1,0,1); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    char log[2048];
    unsigned char *px, left[4] = { 0 }, right[4] = { 0 };

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return;

    if (!mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        glDeleteProgram(prog);
        return;
    }

    mgl_target_bind(&t);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewportIndexedf(0, 0, 0, 32, 64);
    glViewportIndexedf(1, 32, 0, 32, 64);

    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, 10, 32, left);
        mgl_pixel_at(px, &t, 50, 32, right);
        free(px);
    }

    CHECK_MSG(left[0] == 255 && left[1] == 0, "left half should keep the clear, got %d,%d,%d", left[0], left[1], left[2]);
    CHECK_MSG(right[1] == 255 && right[0] == 0, "right half should be drawn, got %d,%d,%d", right[0], right[1], right[2]);

    glViewport(0, 0, 64, 64);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// glViewport, glScissor and glDepthRange set every index; the scissor test
// is enabled per index.
GPU_TEST(layered_rendering, whole_array_setters_and_indexed_scissor_enable)
{
    GLfloat vp[4] = { 0 };
    GLint box[4] = { 0 };
    GLdouble dr[2] = { 0 };

    glViewport(1, 2, 30, 40);
    glScissor(3, 4, 10, 20);
    glDepthRange(0.25, 0.75);

    glGetFloati_v(GL_VIEWPORT, 7, vp);
    glGetIntegeri_v(GL_SCISSOR_BOX, 15, box);
    glGetDoublei_v(GL_DEPTH_RANGE, 3, dr);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK(vp[0] == 1 && vp[1] == 2 && vp[2] == 30 && vp[3] == 40);
    CHECK(box[0] == 3 && box[1] == 4 && box[2] == 10 && box[3] == 20);
    CHECK(dr[0] == 0.25 && dr[1] == 0.75);

    glEnablei(GL_SCISSOR_TEST, 2);
    CHECK(glIsEnabledi(GL_SCISSOR_TEST, 2) == GL_TRUE);
    CHECK(glIsEnabledi(GL_SCISSOR_TEST, 1) == GL_FALSE);
    CHECK(glIsEnabled(GL_SCISSOR_TEST) == GL_FALSE);

    glEnable(GL_SCISSOR_TEST);
    CHECK(glIsEnabledi(GL_SCISSOR_TEST, 9) == GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    CHECK(glIsEnabledi(GL_SCISSOR_TEST, 2) == GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glViewport(0, 0, 64, 64);
    glDepthRange(0.0, 1.0);
}

// A scissored glClear is a draw. It has to reach every layer of a layered
// attachment, an integer attachment, and every draw buffer; it used to write
// float colour into buffer 0 of layer 0 only.
GPU_TEST(layered_rendering, scissored_clear_reaches_every_layer_and_buffer)
{
    static const GLint fill = -1;
    GLuint tex, rgba, fb;
    GLint ints[4 * 4 * 3];
    GLubyte px[4 * 4 * 4 * 3];
    const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R32I, 4, 4, 3);
    glClearTexImage(tex, 0, GL_RED_INTEGER, GL_INT, &fill);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &rgba);
    glBindTexture(GL_TEXTURE_2D_ARRAY, rgba);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 4, 4, 3);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, rgba, 0);
    glDrawBuffers(2, bufs);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    // clear the second buffer red everywhere, then only the scissored corner
    glViewport(0, 0, 4, 4);
    glClearBufferfv(GL_COLOR, 1, (const GLfloat[]){ 1, 0, 0, 1 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 2, 2);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureImage(tex, 0, GL_RED_INTEGER, GL_INT, sizeof ints, ints);
    glGetTextureImage(rgba, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof px, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int layer = 0; layer < 3; layer++)
    {
        const GLint *l = ints + layer * 16;

        // inside the box the clear colour, as an int; outside, untouched
        CHECK_MSG(l[0] == 0, "layer %d inside the box is %d, want 0", layer, l[0]);
        CHECK_MSG(l[15] == -1, "layer %d outside the box is %d, want -1", layer, l[15]);
    }

    // the second draw buffer's first layer: green in the box, red outside
    CHECK_MSG(px[1] == 255 && px[0] == 0, "buffer 1 inside the box: %d,%d,%d", px[0], px[1], px[2]);
    CHECK_MSG(px[15 * 4] == 255 && px[15 * 4 + 1] == 0, "buffer 1 outside the box: %d,%d,%d",
              px[60], px[61], px[62]);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
    glDeleteTextures(1, &rgba);
}

// An attachment made with glFramebufferTexture on an array is layered, and
// says so; one layer of it is not. The query always said no.
GPU_TEST(layered_rendering, attachment_reports_layered)
{
    GLuint tex, fb;
    GLint v = -1;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 4, 4, 2);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_LAYERED, &v);
    CHECK_EQ_INT(v, GL_TRUE);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 1);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_LAYERED, &v);
    CHECK_EQ_INT(v, GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// A name that is not a texture is a bad value, not a bad operation.
GPU_TEST(layered_rendering, an_unknown_texture_name_is_a_bad_value)
{
    GLuint fb;

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 4321, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // the layer call reports it as the wrong object, as the suite expects
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 4321, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glNamedFramebufferTextureLayer(fb, GL_COLOR_ATTACHMENT0, 4321, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
}

GPU_TEST(layered_rendering, each_draw_keeps_its_own_geometry_uniform)
{
    static const char *vs =
        "#version 460 core\n"
        "void main() { gl_Position = vec4(0, 0, 0, 1); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(triangle_strip, max_vertices = 4) out;\n"
        "uniform int layer;\n"
        "void main() {\n"
        "    for (int i = 0; i < 4; i++) {\n"
        "        gl_Layer = layer;\n"
        "        gl_Position = vec4((i & 1) * 2 - 1, (i >> 1) * 2 - 1, 0, 1);\n"
        "        EmitVertex();\n"
        "    }\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "uniform vec4 c;\n"
        "out vec4 o;\n"
        "void main() { o = c; }\n";
    GLuint prog, vao, tex, fb;
    GLubyte px[4 * 4 * 4 * 4];
    char log[2048];

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "geometry program did not link: %s", log);
    if (!prog)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 4, 4, 4);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    glViewport(0, 0, 4, 4);
    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // no readback between draws, so they share one command buffer
    for (int l = 0; l < 4; l++)
    {
        glUniform1i(glGetUniformLocation(prog, "layer"), l);
        glUniform4f(glGetUniformLocation(prog, "c"), l / 4.0f, 0.5f, 0, 1);
        glDrawArrays(GL_POINTS, 0, 1);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

    for (int l = 0; l < 4; l++)
    {
        const GLubyte *p = px + l * 64;

        CHECK_MSG(abs(p[0] - l * 64) <= 1 && abs(p[1] - 128) <= 1,
                  "layer %d is (%d, %d), want (%d, 128)", l, p[0], p[1], l * 64);
    }

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// a framebuffer with no attachments still rasterises at its default size
GPU_TEST(layered_rendering, no_attachment_framebuffer_draws_every_layer)
{
    static const char *vs =
        "#version 440\n"
        "void main() { gl_Position = vec4(0, 0, 0, 1); }\n";
    static const char *gs =
        "#version 440\n"
        "layout(points) in;\n"
        "layout(triangle_strip, max_vertices = 16) out;\n"
        "flat out int layer_id;\n"
        "void main() {\n"
        "    for (int n = 0; n < 4; n++)\n"
        "        for (int i = 0; i < 4; i++) {\n"
        "            gl_Layer = n; layer_id = n;\n"
        "            gl_Position = vec4((i & 1) * 2 - 1, (i >> 1) * 2 - 1, 0, 1);\n"
        "            EmitVertex();\n"
        "        }\n"
        "}\n";
    static const char *fs =
        "#version 440\n"
        "layout(r32i, binding = 0) writeonly uniform iimage2DArray img;\n"
        "flat in int layer_id;\n"
        "void main() { imageStore(img, ivec3(gl_FragCoord.xy, layer_id), ivec4(7)); }\n";
    static GLint got[8 * 8 * 4];
    GLuint prog, vao, tex, fb;
    char log[2048];
    int hits = 0;

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R32I, 8, 8, 4);
    glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R32I);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 8);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 8);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_LAYERS, 4);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glViewport(0, 0, 8, 8);
    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, 1);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RED_INTEGER, GL_INT, got);
    for (int i = 0; i < 8 * 8 * 4; i++)
        hits += got[i] == 7;
    CHECK_MSG(hits == 8 * 8 * 4, "%d of %d texels written", hits, 8 * 8 * 4);

    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

/* ---------- GL_ARB_shader_viewport_layer_array ---------- */

static GLuint linkStages(const GLenum *stages, const char **srcs, int n, char *log, int log_size)
{
    GLuint prog = glCreateProgram();
    GLint ok = 0;

    log[0] = 0;

    for (int i = 0; i < n; i++)
    {
        GLuint sh = compileOne(stages[i], srcs[i], log, log_size);

        if (!sh)
            return 0;

        glAttachShader(prog, sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok)
        glGetProgramInfoLog(prog, log_size, NULL, log);

    return ok ? prog : 0;
}

static const char *green_fs =
    "#version 460 core\n"
    "out vec4 o;\n"
    "void main() { o = vec4(0,1,0,1); }\n";

GPU_TEST(layered_rendering, a_vertex_shader_picks_the_viewport)
{
    static const GLfloat tri[6] = { -1,-1, 3,-1, -1,3 };
    static const char *vs =
        "#version 450 core\n"
        "#extension GL_ARB_shader_viewport_layer_array : require\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); gl_ViewportIndex = 1; }\n";
    const GLenum stages[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
    const char *srcs[] = { vs, green_fs };
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    char log[2048];
    unsigned char *px, left[4] = { 0 }, right[4] = { 0 };

    prog = linkStages(stages, srcs, 2, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return;

    if (!mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        glDeleteProgram(prog);
        return;
    }

    mgl_target_bind(&t);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewportIndexedf(0, 0, 0, 32, 64);
    glViewportIndexedf(1, 32, 0, 32, 64);

    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, 10, 32, left);
        mgl_pixel_at(px, &t, 50, 32, right);
        free(px);
    }
    CHECK_MSG(left[0] == 255 && left[1] == 0, "left half should stay red, got %d,%d", left[0], left[1]);
    CHECK_MSG(right[1] == 255 && right[0] == 0, "right half should be green, got %d,%d", right[0], right[1]);

    glViewport(0, 0, 64, 64);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// Copies src into dst with the one occurrence of key replaced by val.
static void replaceWord(char *dst, size_t size, const char *src, const char *key, const char *val)
{
    const char *k = strstr(src, key);

    snprintf(dst, size, "%.*s%s%s", (int)(k - src), src, val, k + strlen(key));
}

// The CTS pipelines for the layer: vertex only, then with tessellation, then
// with tessellation and a geometry shader, each adding one to the layer, and
// all drawn as two patches from an indexed quad into a 3D texture's slices.
static int layerPipeline(int with_tess, int with_gs, int layers, int layer_in, int *hit)
{
    static const char *vs =
        "#version 450 core\n"
        "#extension GL_ARB_shader_viewport_layer_array: require\n"
        "in highp vec2 inPosition;\n"
        "in int inLayer;\n"
        "in highp vec4 inColor;\n"
        "out int vsLayer;\n"
        "out highp vec3 vsPosition;\n"
        "out highp vec4 vsColor;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(inPosition, 0.0, 1.0);\n"
        "    gl_Layer = (inLayer + 1) % LAYERS;\n"
        "    vsLayer = inLayer;\n"
        "    vsPosition = vec3(inPosition, 0.0);\n"
        "    vsColor = inColor;\n"
        "}\n";
    static const char *tcs =
        "#version 450 core\n"
        "layout(vertices = 3) out;\n"
        "in highp vec3 vsPosition[];\n"
        "in highp vec4 vsColor[];\n"
        "in int vsLayer[];\n"
        "out highp vec3 tcsPosition[];\n"
        "out highp vec4 tcsColor[];\n"
        "out int tcsLayer[];\n"
        "void main()\n"
        "{\n"
        "    tcsPosition[gl_InvocationID] = vsPosition[gl_InvocationID];\n"
        "    tcsColor[gl_InvocationID] = vsColor[gl_InvocationID];\n"
        "    tcsLayer[gl_InvocationID] = vsLayer[gl_InvocationID];\n"
        "    gl_TessLevelInner[0] = 3;\n"
        "    gl_TessLevelOuter[0] = 3;\n"
        "    gl_TessLevelOuter[1] = 3;\n"
        "    gl_TessLevelOuter[2] = 3;\n"
        "}\n";
    static const char *tes =
        "#version 450 core\n"
        "#extension GL_ARB_shader_viewport_layer_array: require\n"
        "layout(triangles, equal_spacing, cw) in;\n"
        "in highp vec3 tcsPosition[];\n"
        "in highp vec4 tcsColor[];\n"
        "in int tcsLayer[];\n"
        "out highp vec4 tesColor;\n"
        "out highp int tesLayer;\n"
        "void main()\n"
        "{\n"
        "    vec3 p0 = gl_TessCoord.x * tcsPosition[0];\n"
        "    vec3 p1 = gl_TessCoord.y * tcsPosition[1];\n"
        "    vec3 p2 = gl_TessCoord.z * tcsPosition[2];\n"
        "    tesColor = tcsColor[0];\n"
        "    tesLayer = tcsLayer[0];\n"
        "    gl_Layer = (tcsLayer[0] + 2) % LAYERS;\n"
        "    gl_Position = vec4(normalize(p0 + p1 + p2), 1.0);\n"
        "}\n";
    static const char *gs =
        "#version 450 core\n"
        "#extension GL_ARB_shader_viewport_layer_array: require\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "in highp vec4 tesColor[];\n"
        "in int tesLayer[];\n"
        "out highp vec4 gsColor;\n"
        "void main()\n"
        "{\n"
        "    for (int i = 0; i<3; i++)\n"
        "    {\n"
        "        gl_Position = gl_in[i].gl_Position;\n"
        "        gl_Layer = (tesLayer[i] + 3) % LAYERS;\n"
        "        gsColor = tesColor[i];\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const GLfloat pos[8] = { -1,-1, -1,1, 1,-1, 1,1 };
    static const GLfloat col[16] = { 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1 };
    static const GLushort idx[6] = { 0, 1, 2, 2, 1, 3 };
    char vsb[2048], tcsb[2048], tesb[2048], gsb[2048], fsb[512], log[2048], defs[64];
    const char *srcs[5];
    GLenum stages[5];
    GLint lay[4] = { layer_in, layer_in, layer_in, layer_in };
    GLuint prog, vao, vbo[4], tex, fb, readfb;
    int n = 0, want, ok;

    snprintf(defs, sizeof defs, "%d", layers);
    replaceWord(vsb, sizeof vsb, vs, "LAYERS", defs);
    replaceWord(tesb, sizeof tesb, tes, "LAYERS", defs);
    replaceWord(gsb, sizeof gsb, gs, "LAYERS", defs);
    snprintf(tcsb, sizeof tcsb, "%s", tcs);
    {
        const char *name = with_gs ? "gsColor" : with_tess ? "tesColor" : "vsColor";
        snprintf(fsb, sizeof fsb,
                 "#version 450 core\nin highp vec4 %s;\nout vec4 finalOutColor;\n"
                 "void main() { finalOutColor = %s; }\n", name, name);
    }

    stages[n] = GL_VERTEX_SHADER; srcs[n++] = vsb;
    if (with_tess)
    {
        stages[n] = GL_TESS_CONTROL_SHADER; srcs[n++] = tcsb;
        stages[n] = GL_TESS_EVALUATION_SHADER; srcs[n++] = tesb;
    }
    if (with_gs)
    {
        stages[n] = GL_GEOMETRY_SHADER; srcs[n++] = gsb;
    }
    stages[n] = GL_FRAGMENT_SHADER; srcs[n++] = fsb;

    prog = linkStages(stages, srcs, n, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog)
        return 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 128, 128, layers, 0, GL_RGBA, GL_FLOAT, 0);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    glViewport(0, 0, 128, 128);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(4, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof pos, pos, GL_STATIC_DRAW);
    glVertexAttribPointer(glGetAttribLocation(prog, "inPosition"), 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(glGetAttribLocation(prog, "inPosition"));
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof lay, lay, GL_STATIC_DRAW);
    glVertexAttribIPointer(glGetAttribLocation(prog, "inLayer"), 1, GL_INT, 0, 0);
    glEnableVertexAttribArray(glGetAttribLocation(prog, "inLayer"));
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof col, col, GL_STATIC_DRAW);
    glVertexAttribPointer(glGetAttribLocation(prog, "inColor"), 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(glGetAttribLocation(prog, "inColor"));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
    glDrawElements(with_tess ? GL_PATCHES : GL_TRIANGLE_STRIP, 6, GL_UNSIGNED_SHORT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    want = (layer_in + (with_gs ? 3 : with_tess ? 2 : 1)) % layers;
    *hit = -1;
    ok = 1;
    glGenFramebuffers(1, &readfb);
    for (int l = 0; l < layers; l++)
    {
        GLfloat rgba[4] = { -1, -1, -1, -1 };

        glBindFramebuffer(GL_READ_FRAMEBUFFER, readfb);
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, l);
        glReadPixels(64, 64, 1, 1, GL_RGBA, GL_FLOAT, rgba);
        if (rgba[0] > 0.9f)
            *hit = l;
        if ((rgba[0] > 0.9f) != (l == want))
            ok = 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(4, vbo);
    glDeleteFramebuffers(1, &fb);
    glDeleteFramebuffers(1, &readfb);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
    return ok;
}

GPU_TEST(layered_rendering, each_stage_picks_a_3d_texture_slice)
{
    int hit;

    for (int stage = 0; stage < 3; stage++)
        for (int layer = 0; layer < 4; layer++)
            CHECK_MSG(layerPipeline(stage > 0, stage > 1, 4, layer, &hit),
                      "stage %d, layer in %d: slice %d drawn", stage, layer, hit);

    CHECK_MSG(layerPipeline(1, 0, 1, 0, &hit), "one slice through tessellation: slice %d drawn", hit);
}
