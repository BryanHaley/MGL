/*
 * test_sparse_texture.c
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
 * GL_ARB_sparse_texture: pages committed and decommitted on a Metal sparse heap.
 */

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

typedef void (*CommitFn)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLboolean);

static CommitFn commitFn(void)
{
    return (CommitFn)dlsym(RTLD_DEFAULT, "glTexPageCommitmentARB");
}

static GLuint *fill(GLuint value, int n)
{
    GLuint *p = malloc(n * sizeof *p);

    for (int i = 0; i < n; i++)
        p[i] = value;
    return p;
}

// count of texels in [x, x+w) x [y, y+h) of a 256-wide level holding value
static int countValue(const GLuint *px, int stride, int x, int y, int w, int h, GLuint value)
{
    int n = 0;

    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            n += px[j * stride + i] == value;
    return n;
}

GPU_TEST(sparse_texture, rgba8_pages_are_128_square_and_two_levels_are_sparse)
{
    GLint n = 0, x = 0, y = 0, z = 0, levels = -1, sparse = -1;
    GLuint tex;

    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, 1, &n);
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_X_ARB, 1, &x);
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_Y_ARB, 1, &y);
    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_VIRTUAL_PAGE_SIZE_Z_ARB, 1, &z);
    CHECK_EQ_INT(n, 1);
    CHECK(x == 128 && y == 128 && z == 1);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 256, 256);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, &sparse);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_NUM_SPARSE_LEVELS_ARB, &levels);
    CHECK_EQ_INT(sparse, GL_TRUE);
    CHECK_EQ_INT(levels, 2);

    // fixed once storage exists
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_FALSE);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &tex);

    // not a multiple of the page
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 200, 128);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_VALUE);
    glDeleteTextures(1, &tex);
}

// A committed page keeps what is written to it; the tail commits as a unit.
GPU_TEST(sparse_texture, committed_pages_and_the_tail_hold_their_texels)
{
    CommitFn commit = commitFn();
    GLuint tex, *green = fill(0xFF00FF00u, 256 * 256), *got = calloc(256 * 256, 4);
    GLuint blue = 0xFFFF0000u;

    CHECK(commit != NULL);
    if (!commit)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 256, 256);

    commit(GL_TEXTURE_2D, 0, 128, 0, 0, 128, 128, 1, GL_TRUE);
    commit(GL_TEXTURE_2D, 2, 0, 0, 0, 64, 64, 1, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, green);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_INT(countValue(got, 256, 128, 0, 128, 128, 0xFF00FF00u), 128 * 128);

    {
        GLuint *b = fill(blue, 64 * 64);

        glTexSubImage2D(GL_TEXTURE_2D, 2, 0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, b);
        memset(got, 0, 64 * 64 * 4);
        glGetTexImage(GL_TEXTURE_2D, 2, GL_RGBA, GL_UNSIGNED_BYTE, got);
        CHECK_EQ_INT(countValue(got, 64, 0, 0, 64, 64, blue), 64 * 64);
        free(b);
    }

    // off the page grid, and past the edge of the level
    commit(GL_TEXTURE_2D, 0, 64, 0, 0, 128, 128, 1, GL_TRUE);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_VALUE);
    commit(GL_TEXTURE_2D, 0, 128, 0, 0, 256, 128, 1, GL_TRUE);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_OPERATION);

    commit(GL_TEXTURE_2D, 0, 128, 0, 0, 128, 128, 1, GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);

    // a texture that is not sparse has nothing to commit
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256);
    commit(GL_TEXTURE_2D, 0, 0, 0, 0, 128, 128, 1, GL_TRUE);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_OPERATION);
    glDeleteTextures(1, &tex);

    free(green);
    free(got);
}

// GL_ARB_sparse_texture2: a shader learns whether it read a committed page,
// and an uncommitted one reads as zero.
GPU_TEST(sparse_texture, a_shader_sees_residency_and_zero_for_missing_pages)
{
    static const char *cs =
        "#version 450 core\n"
        "#extension GL_ARB_sparse_texture2 : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(binding = 0) uniform sampler2D t;\n"
        "layout(std430, binding = 0) buffer O { uvec4 o[]; };\n"
        "void main() {\n"
        "    vec4 a, b, c;\n"
        "    int ca = sparseTexelFetchARB(t, ivec2(130, 5), 0, a);\n"
        "    int cb = sparseTexelFetchARB(t, ivec2(5, 5), 0, b);\n"
        "    int cc = sparseTextureLodARB(t, vec2(0.75, 0.25), 0.0, c);\n"
        "    o[0] = uvec4(sparseTexelsResidentARB(ca), sparseTexelsResidentARB(cb), packUnorm4x8(a), packUnorm4x8(b));\n"
        "    o[1] = uvec4(sparseTexelsResidentARB(cc), packUnorm4x8(c), 0u, 0u);\n"
        "}\n";
    CommitFn commit = commitFn();
    GLuint tex, buf, prog, out[8] = { 0 }, *green = fill(0xFF00FF00u, 256 * 256);
    char log[4096];

    CHECK(commit != NULL);
    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "compute program did not link: %s", log);
    if (!prog || !commit)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256);
    commit(GL_TEXTURE_2D, 0, 128, 0, 0, 128, 128, 1, GL_TRUE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, green);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof out, out, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof out, out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_UINT(out[0], 1);
    CHECK_EQ_UINT(out[1], 0);
    CHECK_EQ_UINT(out[2], 0xFF00FF00u);
    CHECK_EQ_UINT(out[3], 0);
    CHECK_EQ_UINT(out[4], 1);
    CHECK_EQ_UINT(out[5], 0xFF00FF00u);

    glUseProgram(0);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    free(green);
}

// A compute shader's image stores land in the committed pages.
GPU_TEST(sparse_texture, image_stores_fill_committed_pages)
{
    static const char *cs =
        "#version 450 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(rgba8, binding = 0) writeonly uniform image2D img;\n"
        "void main() { imageStore(img, ivec2(gl_WorkGroupID.xy), vec4(1.0)); }\n";
    CommitFn commit = commitFn();
    GLuint tex, prog, *got = calloc(256 * 256, 4);
    char log[4096];

    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "compute program did not link: %s", log);
    if (!prog || !commit)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256);
    commit(GL_TEXTURE_2D, 0, 0, 0, 0, 128, 256, 1, GL_TRUE);

    glUseProgram(prog);
    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
    glDispatchCompute(256, 256, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT(countValue(got, 256, 0, 0, 128, 256, 0xFFFFFFFFu), 128 * 256);
    CHECK_EQ_INT(countValue(got, 256, 128, 0, 128, 256, 0), 128 * 256);

    // and a shader reads them back
    {
        static const char *rs =
            "#version 450 core\n"
            "layout(local_size_x = 1) in;\n"
            "layout(binding = 0) uniform sampler2D t;\n"
            "layout(std430, binding = 0) buffer O { uint o[]; };\n"
            "void main() { o[0] = packUnorm4x8(texelFetch(t, ivec2(5, 5), 0)); }\n";
        GLuint rp = mgl_build_compute_program(rs, log, sizeof log), buf, v = 7;

        glGenBuffers(1, &buf);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 4, &v, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
        glUseProgram(rp);
        glBindTexture(GL_TEXTURE_2D, tex);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 4, &v);
        CHECK_EQ_UINT(v, 0xFFFFFFFFu);
        glDeleteBuffers(1, &buf);
        glDeleteProgram(rp);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    free(got);
}

// sparseImageLoadARB through a read-only image unit reports which texels are
// resident, as a texture lookup does.
GPU_TEST(sparse_texture, image_loads_report_residency)
{
    static const char *cs =
        "#version 450 core\n"
        "#extension GL_ARB_sparse_texture2 : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(rgba8, binding = 0) uniform image2D img;\n"
        "layout(std430, binding = 0) buffer O { uvec4 o[]; };\n"
        "void main() {\n"
        "    vec4 a, b;\n"
        "    int ca = sparseImageLoadARB(img, ivec2(130, 5), a);\n"
        "    int cb = sparseImageLoadARB(img, ivec2(5, 5), b);\n"
        "    o[0] = uvec4(sparseTexelsResidentARB(ca), sparseTexelsResidentARB(cb), packUnorm4x8(a), packUnorm4x8(b));\n"
        "}\n";
    CommitFn commit = commitFn();
    GLuint tex, buf, prog, out[4] = { 0 }, *green = fill(0xFF00FF00u, 256 * 256);
    char log[4096];

    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "compute program did not link: %s", log);
    if (!prog || !commit)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256);
    commit(GL_TEXTURE_2D, 0, 128, 0, 0, 128, 128, 1, GL_TRUE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, green);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof out, out, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof out, out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_UINT(out[0], 1);
    CHECK_EQ_UINT(out[1], 0);
    CHECK_EQ_UINT(out[2], 0xFF00FF00u);
    CHECK_EQ_UINT(out[3], 0);

    glUseProgram(0);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    free(green);
}

// Pages come back to the pool when a texture is deleted: more than the pool
// holds, made and dropped one after another, still commits and holds data.
GPU_TEST(sparse_texture, deleted_textures_give_their_pages_back)
{
    CommitFn commit = commitFn();
    GLuint *green = fill(0xFF00FF00u, 1024 * 1024), *got = calloc(1024 * 1024, 4);
    int good = 0;

    if (!commit)
        return;

    // 4 MB a texture, 96 of them
    for (int n = 0; n < 96; n++)
    {
        GLuint tex;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1024, 1024);
        commit(GL_TEXTURE_2D, 0, 0, 0, 0, 1024, 1024, 1, GL_TRUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, green);

        if (n == 95)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            // read back through the GPU, not the CPU copy
            GLuint fb;
            glGenFramebuffers(1, &fb);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            glReadPixels(0, 0, 1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, got);
            good = countValue(got, 1024, 0, 0, 1024, 1024, 0xFF00FF00u);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fb);
        }

        glDeleteTextures(1, &tex);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(good, 1024 * 1024);
    free(green);
    free(got);
}

// Zero gradients on a shadow sampler read level 0, or the clamp level; offset gathers compile.
GPU_TEST(sparse_texture, shadow_lookups_with_zero_gradients_and_offset_gathers)
{
    static const char *cs =
        "#version 450 core\n"
        "#extension GL_ARB_sparse_texture_clamp : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(binding = 0) uniform sampler2DShadow t;\n"
        "layout(std430, binding = 0) buffer O { uvec4 o[2]; };\n"
        "void main() {\n"
        "    const ivec2 offs[4] = ivec2[](ivec2(0), ivec2(1, 0), ivec2(0, 1), ivec2(1));\n"
        "    vec3 p = vec3(0.5, 0.5, 0.5);\n"
        "    vec4 g;\n"
        "    int gc = sparseTextureGatherOffsetsARB(t, p.xy, 0.5, offs, g);\n"
        "    float b;\n"
        "    float a = textureGrad(t, p, vec2(0), vec2(0));\n"
        "    int code = sparseTextureGradClampARB(t, p, vec2(0), vec2(0), 1.0, b);\n"
        "    float c = textureGradClampARB(t, p, vec2(0), vec2(0), 1.0);\n"
        "    o[0] = uvec4(uint(a), uint(b), uint(c), uint(sparseTexelsResidentARB(code)));\n"
        "    o[1] = uvec4(uint(dot(g, vec4(1))), uint(sparseTexelsResidentARB(gc)), 0u, 0u);\n"
        "}\n";
    static const float one[16] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    static const float zero[4] = { 0 };
    GLuint tex, buf, prog, out[8] = { 9, 9, 9, 9, 9, 9, 9, 9 };
    char log[4096];

    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "compute program did not link: %s", log);
    if (!prog)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_DEPTH_COMPONENT32F, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_DEPTH_COMPONENT, GL_FLOAT, one);
    glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_DEPTH_COMPONENT, GL_FLOAT, zero);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof out, out, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof out, out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_UINT(out[0], 1);
    CHECK_EQ_UINT(out[1], 0);
    CHECK_EQ_UINT(out[2], 0);
    CHECK_EQ_UINT(out[3], 1);
    CHECK_EQ_UINT(out[4], 4);
    CHECK_EQ_UINT(out[5], 1);

    glUseProgram(0);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
}

// An uncommitted texel stays zero inside one shader: an atomic or a store to
// it goes nowhere and a load of it after reads 0, while a committed texel
// keeps what was done to it.
GPU_TEST(sparse_texture, uncommitted_texels_ignore_atomics_and_stores)
{
    static const char *cs =
        "#version 450 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(r32ui, binding = 0) uniform uimage2D img;\n"
        "layout(std430, binding = 0) buffer O { uvec4 o; };\n"
        "void main() {\n"
        "    uint a = imageAtomicAdd(img, ivec2(5, 5), 7u);\n"
        "    uint b = imageAtomicAdd(img, ivec2(130, 5), 7u);\n"
        "    imageStore(img, ivec2(131, 5), uvec4(9u));\n"
        "    o = uvec4(imageLoad(img, ivec2(5, 5)).x, imageLoad(img, ivec2(130, 5)).x,\n"
        "              imageLoad(img, ivec2(131, 5)).x, a + b * 1000u);\n"
        "}\n";
    CommitFn commit = commitFn();
    GLuint tex, buf, prog, out[4] = { 9, 9, 9, 9 }, *three = fill(3u, 256 * 256);
    char log[4096];

    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "compute program did not link: %s", log);
    if (!prog || !commit)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 256, 256);
    commit(GL_TEXTURE_2D, 0, 0, 0, 0, 128, 256, 1, GL_TRUE);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 128, 256, GL_RED_INTEGER, GL_UNSIGNED_INT, three);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof out, out, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof out, out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_UINT(out[0], 10);
    CHECK_EQ_UINT(out[1], 0);
    CHECK_EQ_UINT(out[2], 0);
    CHECK_EQ_UINT(out[3], 3);

    glUseProgram(0);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    free(three);
}
