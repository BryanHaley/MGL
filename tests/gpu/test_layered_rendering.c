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
