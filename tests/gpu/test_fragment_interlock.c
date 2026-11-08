/*
 * test_fragment_interlock.c
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
 * GL_ARB_fragment_shader_interlock: overlapping fragments run their critical
 * sections one at a time, in primitive order.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

#define LAYERS 64

// Each of 64 stacked quads adds one to every pixel of an image with a plain
// load and store. Unordered, overlapping fragments lose updates; inside the
// critical section every pixel ends at exactly 64. The second image records
// the order: each fragment checks it is the next quad in line.
GPU_TEST(fragment_interlock, overlapping_fragments_take_turns_in_order)
{
    static const char *vs =
        "#version 450 core\n"
        "layout(location = 0) in vec2 p;\n"
        "flat out int quad;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); quad = gl_InstanceID; }\n";
    static const char *fs =
        "#version 450 core\n"
        "#extension GL_ARB_fragment_shader_interlock : require\n"
        "layout(pixel_interlock_ordered) in;\n"
        "layout(r32ui, binding = 0) uniform coherent uimage2D count;\n"
        "layout(r32ui, binding = 1) uniform coherent uimage2D order;\n"
        "flat in int quad;\n"
        "out vec4 o;\n"
        "void main() {\n"
        "    ivec2 p = ivec2(gl_FragCoord.xy);\n"
        "    beginInvocationInterlockARB();\n"
        "    uint n = imageLoad(count, p).x;\n"
        "    imageStore(count, p, uvec4(n + 1u));\n"
        "    if (n != uint(quad)) imageStore(order, p, uvec4(1u));\n"
        "    endInvocationInterlockARB();\n"
        "    o = vec4(0.0);\n"
        "}\n";
    GLuint img[2], prog, vao, vbo;
    GLuint *count, *order;
    MGLTestTarget t;
    char log[2048];
    int wrong = 0, misordered = 0;

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
        return;

    glGenTextures(2, img);
    for (int i = 0; i < 2; i++)
    {
        GLuint zero[64 * 64] = { 0 };

        glBindTexture(GL_TEXTURE_2D, img[i]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 64, 64);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_RED_INTEGER, GL_UNSIGNED_INT, zero);
        glBindImageTexture(i, img[i], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    }

    mgl_target_bind(&t);
    glViewport(0, 0, 64, 64);
    glUseProgram(prog);
    vao = mgl_fullscreen_quad(&vbo);
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, LAYERS);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    count = malloc(64 * 64 * 4);
    order = malloc(64 * 64 * 4);
    glBindTexture(GL_TEXTURE_2D, img[0]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, count);
    glBindTexture(GL_TEXTURE_2D, img[1]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, order);

    for (int i = 0; i < 64 * 64; i++)
    {
        wrong += count[i] != LAYERS;
        misordered += order[i] != 0;
    }

    CHECK_MSG(wrong == 0, "%d of 4096 pixels lost updates (first holds %u)", wrong, count[0]);
    CHECK_MSG(misordered == 0, "%d of 4096 pixels ran out of order", misordered);

    free(count);
    free(order);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteTextures(2, img);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
