/*
 * test_vertex_binding_stride.c
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
 * glBindVertexBuffer with a stride of zero. Metal has no such stride, so the
 * binding has to become a constant step over one element instead of being
 * passed through -- a zero reaches Metal as "this buffer has no stride" and
 * it rejects the whole draw.
 */

#include <stdlib.h>
#include "mgl_test.h"
#include "harness.h"

#define W 32
#define H 32

static const char *VS_TINTED =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in vec4 tint;\n"
    "out vec4 v_tint;\n"
    "void main() { v_tint = tint; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_TINTED =
    "#version 460 core\n"
    "in vec4 v_tint;\n"
    "out vec4 frag;\n"
    "void main() { frag = v_tint; }\n";

GPU_TEST(vertex_binding_stride, a_zero_stride_binding_repeats_one_element)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao = 0, pos_vbo = 0, tint_vbo = 0;
    unsigned char *px, c[4];

    static const float quad[] = {
        -1.0f, -1.0f,   1.0f, -1.0f,  -1.0f, 1.0f,
        -1.0f,  1.0f,   1.0f, -1.0f,   1.0f, 1.0f,
    };
    // green first, red second: a stride of zero must never reach the red one
    static const float tints[] = {
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
    };

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_TINTED, FS_TINTED, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader pipeline unavailable"); }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &pos_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pos_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);

    glGenBuffers(1, &tint_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, tint_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tints, tints, GL_STATIC_DRAW);

    glBindVertexBuffer(0, pos_vbo, 0, 2 * sizeof(float));
    glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glEnableVertexAttribArray(0);

    // the stride under test
    glBindVertexBuffer(1, tint_vbo, 0, 0);
    glVertexAttribFormat(1, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(1, 1);
    glEnableVertexAttribArray(1);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLint got = -1;
        glGetIntegeri_v(GL_VERTEX_BINDING_STRIDE, 1, &got);
        CHECK_EQ_INT(got, 0);
    }

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px) {
        mgl_pixel_at(px, &t, W / 2, H / 2, c);
        CHECK_MSG(c[1] > 200 && c[0] < 60,
                  "centre = %d,%d,%d - want the first element, green", c[0], c[1], c[2]);

        // a stride that stepped would tint one corner red
        mgl_pixel_at(px, &t, W - 2, 1, c);
        CHECK_MSG(c[1] > 200 && c[0] < 60,
                  "corner = %d,%d,%d - want the first element, green", c[0], c[1], c[2]);
        free(px);
    } else {
        CHECK_MSG(0, "readback failed");
    }

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &pos_vbo);
    glDeleteBuffers(1, &tint_vbo);
    mgl_target_destroy(&t);
}
