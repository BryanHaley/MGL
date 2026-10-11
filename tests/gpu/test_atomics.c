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

// Two counters on two bindings, counted up and down across many work groups:
// every invocation gets its own number from each.
GPU_TEST(atomics, counters_on_two_bindings_hand_out_unique_numbers)
{
    static const char *CS =
        "#version 460\n"
        "layout(local_size_x = 4, local_size_y = 3, local_size_z = 2) in;\n"
        "layout(std430, binding = 0) buffer Output { uint inc_data[576]; uint dec_data[576]; };\n"
        "layout(binding = 0, offset = 0) uniform atomic_uint g_inc_counter;\n"
        "layout(binding = 1, offset = 0) uniform atomic_uint g_dec_counter;\n"
        "void main() {\n"
        "  const uint index = atomicCounterIncrement(g_inc_counter);\n"
        "  inc_data[index] = index;\n"
        "  dec_data[index] = atomicCounterDecrement(g_dec_counter);\n"
        "}\n";
    char log[1024] = { 0 };
    GLuint prog = mgl_build_compute_program(CS, log, sizeof log);
    GLuint out, counters[2], start[2] = { 0, 576 }, value[2] = { 0, 0 };
    GLuint *data;
    int wrong = 0;

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGenBuffers(1, &out);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * 576 * 2, NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(2, counters);
    for (int i = 0; i < 2; i++)
    {
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, (GLuint)i, counters[i]);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &start[i], GL_STREAM_DRAW);
    }

    glUseProgram(prog);
    glDispatchCompute(2, 3, 4);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    data = (GLuint *)malloc(sizeof(GLuint) * 576 * 2);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, out);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * 576 * 2, data);

    for (GLuint i = 0; i < 576 && data; i++)
        if (data[i] != i && wrong++ == 0)
            CHECK_MSG(0, "inc_data[%u] = %u", i, data[i]);

    for (int i = 0; i < 2; i++)
    {
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counters[i]);
        glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &value[i]);
    }
    CHECK_EQ_UINT(value[0], 576);
    CHECK_EQ_UINT(value[1], 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    free(data);
    glUseProgram(0);
    glDeleteBuffers(1, &out);
    glDeleteBuffers(2, counters);
    glDeleteProgram(prog);
}

// Counters are active uniforms of their own, each tied to its buffer, and the
// program counts the buffers.
GPU_TEST(atomics, counters_are_listed_as_uniforms)
{
    static const char *vs = "#version 420 core\nvoid main() { gl_Position = vec4(0.0); }\n";
    static const char *fs =
        "#version 420 core\n"
        "layout(location = 0) out vec4 o;\n"
        "layout(binding = 0, offset = 4) uniform atomic_uint a;\n"
        "layout(binding = 0) uniform atomic_uint b[2];\n"
        "void main() { o = vec4(float(atomicCounterIncrement(a) + atomicCounterIncrement(b[1]))); }\n";
    char log[1024] = "";
    GLuint prog = mgl_build_program(vs, fs, log, sizeof log);
    GLint n = -1, buffers = -1;

    CHECK_MSG(prog != 0, "program did not build: %s", log);

    glGetProgramiv(prog, GL_ACTIVE_ATOMIC_COUNTER_BUFFERS, &buffers);
    CHECK_EQ_INT(buffers, 1);
    glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &n);
    CHECK_EQ_INT(n, 2);

    const GLchar *names[2] = { "a", "b[0]" };
    GLuint idx[2] = { GL_INVALID_INDEX, GL_INVALID_INDEX };

    glGetUniformIndices(prog, 2, names, idx);
    CHECK(idx[0] != GL_INVALID_INDEX && idx[1] != GL_INVALID_INDEX);

    GLint type[2] = {0}, offset[2] = {-1, -1}, stride[2] = {-1, -1}, buffer[2] = {-1, -1};

    glGetActiveUniformsiv(prog, 2, idx, GL_UNIFORM_TYPE, type);
    glGetActiveUniformsiv(prog, 2, idx, GL_UNIFORM_OFFSET, offset);
    glGetActiveUniformsiv(prog, 2, idx, GL_UNIFORM_ARRAY_STRIDE, stride);
    glGetActiveUniformsiv(prog, 2, idx, GL_UNIFORM_ATOMIC_COUNTER_BUFFER_INDEX, buffer);
    CHECK_EQ_INT(type[0], GL_UNSIGNED_INT_ATOMIC_COUNTER);
    CHECK_EQ_INT(offset[0], 4);
    CHECK_EQ_INT(offset[1], 8);
    CHECK_EQ_INT(stride[1], 4);
    CHECK_EQ_INT(buffer[0], 0);
    CHECK_EQ_INT(buffer[1], 0);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    glDeleteProgram(prog);
}

// GL_BUFFER_ACCESS follows the mapping: read-write when unmapped, write-only
// for a range mapped for writing.
GPU_TEST(atomics, buffer_access_follows_the_mapping)
{
    GLuint buf;
    GLint access = 0, flags = -1;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buf);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, 64, NULL, GL_DYNAMIC_DRAW);
    glGetBufferParameteriv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_ACCESS, &access);
    CHECK_EQ_INT(access, GL_READ_WRITE);

    glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, 16, GL_MAP_WRITE_BIT);
    glGetBufferParameteriv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_ACCESS, &access);
    CHECK_EQ_INT(access, GL_WRITE_ONLY);
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

    glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_READ_ONLY);
    glGetBufferParameteriv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_ACCESS_FLAGS, &flags);
    CHECK_EQ_INT(flags, GL_MAP_READ_BIT);
    glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

    glGetBufferParameteriv(GL_ATOMIC_COUNTER_BUFFER, GL_BUFFER_ACCESS, &access);
    CHECK_EQ_INT(access, GL_READ_WRITE);

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    glDeleteBuffers(1, &buf);
}

// A counter placed past the largest buffer GL allows does not compile.
GPU_TEST(atomics, an_offset_past_the_buffer_limit_does_not_compile)
{
    GLint max = 0, ok = 1;
    char src[512];

    glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE, &max);
    snprintf(src, sizeof src,
             "#version 420 core\n"
             "layout(location = 0) out uvec4 o;\n"
             "layout(binding = 0, offset = %d) uniform atomic_uint c;\n"
             "void main() { o = uvec4(atomicCounterIncrement(c)); }\n", max);

    const char *p = src;
    GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(sh, 1, &p, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);
    glDeleteShader(sh);
}

// An evaluation stage with no control stage, recorded by feedback, counts
// once per vertex. Metal can run a shared vertex twice.
GPU_TEST(atomics, an_evaluation_stage_counts_each_vertex_once)
{
    static const char *vs = "#version 420 core\nvoid main() { gl_Position = vec4(0.0); }\n";
    static const char *tes =
        "#version 420 core\n"
        "layout(triangles, equal_spacing, ccw) in;\n"
        "out uint got;\n"
        "layout(binding = 0) uniform atomic_uint c;\n"
        "void main() { got = atomicCounterIncrement(c); }\n";
    static const char *varyings[] = { "got" };
    GLuint prog = glCreateProgram(), v, e, counter, xfb, vao;
    GLuint start = 100, seen[3] = {0}, final = 0;
    GLint ok = 0;

    v = glCreateShader(GL_VERTEX_SHADER);
    e = glCreateShader(GL_TESS_EVALUATION_SHADER);
    glShaderSource(v, 1, &vs, NULL);
    glShaderSource(e, 1, &tes, NULL);
    glCompileShader(v);
    glCompileShader(e);
    glAttachShader(prog, v);
    glAttachShader(prog, e);
    glTransformFeedbackVaryings(prog, 1, varyings, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);

    glGenBuffers(1, &counter);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counter);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, 4, &start, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, counter);

    glGenBuffers(1, &xfb);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 64, NULL, GL_STREAM_COPY);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glBeginTransformFeedback(GL_TRIANGLES);
    glDrawArrays(GL_PATCHES, 0, 3);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);

    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof seen, seen);
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, 4, &final);
    CHECK_EQ_UINT(final, 103);
    CHECK_MSG(seen[0] + seen[1] + seen[2] == 100 + 101 + 102,
              "recorded %u %u %u, want 100 101 102 in some order", seen[0], seen[1], seen[2]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteShader(v);
    glDeleteShader(e);
    glDeleteBuffers(1, &counter);
    glDeleteBuffers(1, &xfb);
    glDeleteVertexArrays(1, &vao);
}

static GLint fragmentCompiles(const char *decl)
{
    char src[1024];
    const char *p = src;
    GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = 0;

    snprintf(src, sizeof src,
             "#version 460\n%s\nlayout(location = 0) out uvec4 o;\n"
             "void main() { o = uvec4(atomicCounter(a)); }\n", decl);
    glShaderSource(sh, 1, &p, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    glDeleteShader(sh);

    return ok;
}

// The atomic counter layout examples of GLSL 4.60 section 4.4.6.3.
GPU_TEST(atomics, counter_layouts_follow_the_rules)
{
    // a counter needs a binding, even with a default offset for one
    CHECK_EQ_INT(fragmentCompiles("layout(binding = 2, offset = 4) uniform atomic_uint;\n"
                                  "layout(offset = 8) uniform atomic_uint a;"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("layout(offset = 4) uniform atomic_uint a;"), GL_FALSE);

    // two in one place, and one between places
    CHECK_EQ_INT(fragmentCompiles("layout(binding = 1, offset = 0) uniform atomic_uint a;\n"
                                  "layout(binding = 1, offset = 0) uniform atomic_uint b;"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("layout(binding = 1, offset = 2) uniform atomic_uint a;"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("layout(binding = 1, offset = 0) uniform atomic_uint b[2];\n"
                                  "layout(binding = 1, offset = 4) uniform atomic_uint a;"), GL_FALSE);

    // and what is fine
    CHECK_EQ_INT(fragmentCompiles("layout(binding = 1, offset = 0) uniform atomic_uint a;\n"
                                  "layout(binding = 2, offset = 0) uniform atomic_uint b;"), GL_TRUE);
    CHECK_EQ_INT(fragmentCompiles("layout(binding = 2, offset = 4) uniform atomic_uint;\n"
                                  "layout(binding = 2) uniform atomic_uint a;\n"
                                  "layout(binding = 2) uniform atomic_uint b;"), GL_TRUE);
}

// An array of arrays of counters counts every element against the stage limit
GPU_TEST(atomics, an_array_of_arrays_counts_every_counter)
{
    static const char *VS =
        "#version 430 core\n"
        "void main() { gl_Position = vec4(0.0); }\n";
    GLint max = 0;
    char fs[512], log[1024];

    glGetIntegerv(GL_MAX_FRAGMENT_ATOMIC_COUNTERS, &max);
    CHECK(max >= 2);

    for (int over = 0; over < 2; over++)
    {
        snprintf(fs, sizeof fs,
                 "#version 430 core\n"
                 "layout(location = 0) out uvec4 o;\n"
                 "layout(binding = 0) uniform atomic_uint a[2][%d];\n"
                 "void main() {\n"
                 "  for (int i = 0; i < 2; i++) for (int j = 0; j < %d; j++) o.x += atomicCounterIncrement(a[i][j]);\n"
                 "}\n", max / 2 + over, max / 2 + over);

        GLuint p = mgl_build_program(VS, fs, log, sizeof log);

        CHECK_MSG(over ? p == 0 : p != 0, "%d counters %s, limit %d", 2 * (max / 2 + over),
                  p ? "built" : "did not build", max);
        if (p) glDeleteProgram(p);
    }
    mgl_drain_errors();
}
