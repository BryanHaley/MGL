/*
 * test_atomics.c
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
 * Two kinds of atomic operation, both counted across many compute invocations
 * running at once. A count that comes back short means the updates raced; one
 * that never comes back means the GPU hung.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

#define INVOCATIONS 1024u

/* A scalar counter and an array after it, sharing one binding. SPIRV-Cross
   only declares a counter that lives in a buffer, so MGL has to move these. */
static const char *COUNTER_CS =
"#version 460 core\n"
"layout(local_size_x = 64) in;\n"
"layout(binding = 0, offset = 0) uniform atomic_uint total;\n"
"layout(binding = 0) uniform atomic_uint per[4];\n"
"void main() {\n"
"    atomicCounterIncrement(total);\n"
"    atomicCounterIncrement(per[gl_GlobalInvocationID.x % 4u]);\n"
"}\n";

/* Every invocation adds one to the same texel. */
static const char *IMAGE_CS =
"#version 310 es\n"
"#extension GL_OES_shader_image_atomic : require\n"
"layout(local_size_x = 64) in;\n"
"layout(r32ui, binding = 0) uniform highp uimage2D img;\n"
"void main() {\n"
"    imageAtomicAdd(img, ivec2(0, 0), 1u);\n"
"}\n";

GPU_TEST(atomics, counters_count_every_invocation)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(COUNTER_CS, log, sizeof log);

    CHECK_MSG(prog != 0, "compute program did not build: %s", log);
    if (!prog)
        return;

    GLuint zero[5] = {0};
    GLuint buf = 0;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buf);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof zero, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, buf);

    glUseProgram(prog);
    glDispatchCompute(INVOCATIONS / 64, 1, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    glFinish();

    GLuint got[5] = {0};

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buf);
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof got, got);

    CHECK_EQ_UINT(INVOCATIONS, got[0]);

    for (int i = 0; i < 4; i++)
        CHECK_MSG(got[1 + i] == INVOCATIONS / 4, "per[%d] counted %u, expected %u",
                  i, got[1 + i], INVOCATIONS / 4);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &buf);
}

GPU_TEST(atomics, image_add_counts_every_invocation)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(IMAGE_CS, log, sizeof log);

    CHECK_MSG(prog != 0, "compute program did not build: %s", log);
    if (!prog)
        return;

    GLuint tex = 0;
    GLuint start[4] = {0};

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 4, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, start);

    /* Read it once before it is ever an image, so the Metal texture already
       exists without write or atomic usage and has to be remade. */
    GLuint before[4] = {99, 99, 99, 99};
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, before);
    CHECK_EQ_UINT(0, before[0]);

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

    glUseProgram(prog);
    glDispatchCompute(INVOCATIONS / 64, 1, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    glFinish();

    GLuint got[4] = {0};

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, got);

    CHECK_EQ_UINT(INVOCATIONS, got[0]);
    CHECK_EQ_UINT(0, got[1]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
}

// The CTS "all targets" shape: every atomic operation on one texel of a 3D
// and an array image. A hang here is the compare-and-swap loop.
static const char *ALL_TARGETS_CS =
"#version 310 es\n"
"#extension GL_OES_shader_image_atomic : require\n"
"layout(local_size_x = 4) in;\n"

"layout(r32i, binding = 2) uniform highp iimage3D vol;\n"
"layout(r32i, binding = 3) uniform highp iimage2DArray arr;\n"
"layout(std430, binding = 0) buffer Out { ivec4 ok[4]; };\n"
"int run(int which, ivec3 c) {\n"
"  int bad = 0;\n"
"  if (which == 1) {\n"
"    imageAtomicExchange(vol, c, 0);\n"
"    if (imageAtomicAdd(vol, c, 2) != 0) bad = 1;\n"
"    if (imageAtomicCompSwap(vol, c, 2, 6) != 2) bad = 2;\n"
"    if (imageAtomicExchange(vol, c, 0) != 6) bad = 3;\n"
"  } else {\n"
"    imageAtomicExchange(arr, c, 0);\n"
"    if (imageAtomicAdd(arr, c, 2) != 0) bad = 1;\n"
"    if (imageAtomicCompSwap(arr, c, 2, 6) != 2) bad = 2;\n"
"    if (imageAtomicExchange(arr, c, 0) != 6) bad = 3;\n"
"  }\n"
"  return bad;\n"
"}\n"
"void main() {\n"
"  int i = int(gl_GlobalInvocationID.x);\n"
"  ivec3 c = ivec3(i, 0, 1);\n"
"  ok[i] = ivec4(0, run(1, c), run(2, c), 7);\n"
"}\n";

GPU_TEST(atomics, image_ops_on_3d_and_array)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(ALL_TARGETS_CS, log, sizeof log);

    CHECK_MSG(prog != 0, "compute program did not build: %s", log);
    if (!prog)
        return;

    GLenum targets[3] = { GL_TEXTURE_CUBE_MAP, GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY };
    GLuint units[3] = { 0, 2, 3 };
    GLuint tex[3];

    glGenTextures(3, tex);

    for (int t = 0; t < 3; t++)
    {
        glBindTexture(targets[t], tex[t]);
        glTexParameteri(targets[t], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(targets[t], GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        if (targets[t] == GL_TEXTURE_CUBE_MAP)
            glTexStorage2D(targets[t], 1, GL_R32I, 4, 4);
        else
            glTexStorage3D(targets[t], 1, GL_R32I, 4, 1, 2);

        glBindImageTexture(units[t], tex[t], 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32I);
    }

    GLint zero[16] = {0};
    GLuint buf = 0;

    glGenBuffers(1, &buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, zero, GL_DYNAMIC_DRAW);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    glFinish();

    GLint got[16] = {0};

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);

    for (int i = 0; i < 4; i++)
        CHECK_MSG(got[i * 4] == 0 && got[i * 4 + 1] == 0 && got[i * 4 + 2] == 0 && got[i * 4 + 3] == 7,
                  "texel %d: cube %d, 3D %d, array %d (0 is right), marker %d",
                  i, got[i * 4], got[i * 4 + 1], got[i * 4 + 2], got[i * 4 + 3]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(3, tex);
    glDeleteBuffers(1, &buf);
}
