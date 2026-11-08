/*
 * test_subgroups.c
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
 * GL_KHR_shader_subgroup on Apple GPUs, whose SIMD groups are 32 wide.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

// Runs one 64-invocation workgroup that writes four uints per invocation and
// returns them.
static GLuint *runCompute(const char *cs)
{
    static GLuint out[64 * 4];
    GLuint prog, buf;
    char log[4096];

    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "compute program did not link: %s", log);
    if (!prog)
        return NULL;

    memset(out, 0xEE, sizeof out);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof out, out, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof out, out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
    return out;
}

GPU_TEST(subgroups, queries_report_a_32_wide_group)
{
    GLint size = 0, stages = 0, features = 0, quad = -1;

    glGetIntegerv(GL_SUBGROUP_SIZE_KHR, &size);
    glGetIntegerv(GL_SUBGROUP_SUPPORTED_STAGES_KHR, &stages);
    glGetIntegerv(GL_SUBGROUP_SUPPORTED_FEATURES_KHR, &features);
    glGetBooleanv(GL_SUBGROUP_QUAD_ALL_STAGES_KHR, (GLboolean *)&quad);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT(size, 32);
    CHECK(stages & GL_COMPUTE_SHADER_BIT);
    CHECK(features & GL_SUBGROUP_FEATURE_BASIC_BIT_KHR);
}

GPU_TEST(subgroups, compute_sees_ids_sums_and_ballots)
{
    static const char *cs =
        "#version 450 core\n"
        "#extension GL_KHR_shader_subgroup_basic : require\n"
        "#extension GL_KHR_shader_subgroup_arithmetic : require\n"
        "#extension GL_KHR_shader_subgroup_ballot : require\n"
        "#extension GL_KHR_shader_subgroup_shuffle : require\n"
        "layout(local_size_x = 64) in;\n"
        "layout(std430, binding = 0) buffer Out { uvec4 o[]; };\n"
        "void main() {\n"
        "    uint i = gl_LocalInvocationIndex;\n"
        "    o[i] = uvec4(gl_SubgroupSize * 1000u + gl_SubgroupID * 100u + gl_SubgroupInvocationID,\n"
        "                 subgroupAdd(i),\n"
        "                 subgroupBallotBitCount(subgroupBallot((i & 1u) == 0u)),\n"
        "                 subgroupShuffleXor(i, 1u));\n"
        "}\n";
    GLuint *out = runCompute(cs);
    int bad = 0;

    if (!out)
        return;

    for (GLuint i = 0; i < 64; i++)
    {
        GLuint group = i / 32, base = group * 32;
        GLuint sum = 32 * base + 31 * 32 / 2;

        if (out[i * 4] != 32000 + group * 100 + i % 32 || out[i * 4 + 1] != sum ||
            out[i * 4 + 2] != 16 || out[i * 4 + 3] != (i ^ 1))
        {
            if (bad++ < 4)
                CHECK_MSG(0, "invocation %u: %u %u %u %u", i, out[i * 4], out[i * 4 + 1], out[i * 4 + 2], out[i * 4 + 3]);
        }
    }
    CHECK_EQ_INT(bad, 0);
}

// Inside a branch only the active invocations take part. The same call made
// before the branch must not be reused for it.
GPU_TEST(subgroups, a_reduction_in_a_branch_sees_only_active_invocations)
{
    static const char *cs =
        "#version 450 core\n"
        "#extension GL_KHR_shader_subgroup_arithmetic : require\n"
        "layout(local_size_x = 64) in;\n"
        "layout(std430, binding = 0) buffer Out { uvec4 o[]; };\n"
        "void main() {\n"
        "    uint i = gl_LocalInvocationIndex;\n"
        "    uint v = 31u - (i % 32u);\n"
        "    float f = float(v);\n"
        "    uvec4 r = uvec4(subgroupMax(v), uint(subgroupMax(f)), 0u, 0u);\n"
        "    if ((i & 1u) == 1u) {\n"
        "        r.z = subgroupMax(v);\n"
        "        r.w = uint(subgroupMax(f));\n"
        "    }\n"
        "    o[i] = r;\n"
        "}\n";
    GLuint *out = runCompute(cs);
    int bad = 0;

    if (!out)
        return;

    for (GLuint i = 0; i < 64; i++)
    {
        GLuint odd = i & 1, want = odd ? 30 : 0;

        if (out[i * 4] != 31 || out[i * 4 + 1] != 31 || out[i * 4 + 2] != want || out[i * 4 + 3] != want)
            if (bad++ < 4)
                CHECK_MSG(0, "invocation %u: %u %u %u %u", i, out[i * 4], out[i * 4 + 1], out[i * 4 + 2], out[i * 4 + 3]);
    }
    CHECK_EQ_INT(bad, 0);
}

// A fragment shader sees the same 32-wide groups: the count of active
// invocations agrees between a sum, a ballot and the invocation ids.
GPU_TEST(subgroups, fragment_shaders_count_their_group)
{
    static const char *vs =
        "#version 450 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 450 core\n"
        "#extension GL_KHR_shader_subgroup_arithmetic : require\n"
        "#extension GL_KHR_shader_subgroup_ballot : require\n"
        "#extension GL_KHR_shader_subgroup_clustered : require\n"
        "out vec4 o;\n"
        "void main() {\n"
        "    uint n = subgroupAdd(1u);\n"
        "    uint id = gl_SubgroupInvocationID;\n"
        "    bool clustered = float(subgroupClusteredAdd(id, 4u)) == subgroupClusteredAdd(float(id), 4u) &&\n"
        "                     float(subgroupClusteredMax(id, 8u)) == subgroupClusteredMax(float(id), 8u);\n"
        "    uvec4 b = subgroupBallot(true);\n"
        "    bool ok = n == subgroupBallotBitCount(b) && gl_SubgroupSize == 32u &&\n"
        "              subgroupBallotBitExtract(b, gl_SubgroupInvocationID) &&\n"
        "              subgroupMax(gl_SubgroupInvocationID) < 32u && clustered;\n"
        "    o = ok ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    unsigned char *px;
    char log[4096];
    int green = 0;

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
        return;

    mgl_target_bind(&t);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    vao = mgl_fullscreen_quad(&vbo);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        for (int i = 0; i < 64 * 64; i++)
            green += px[i * 4 + 1] == 255 && px[i * 4] == 0;
        free(px);
    }
    CHECK_EQ_INT(green, 64 * 64);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// Scans Metal has no function for, a shuffle whose mask differs per lane, and
// a sum of doubles, which MGL keeps as three floats.
GPU_TEST(subgroups, scans_lane_shuffles_and_doubles)
{
    static const char *cs =
        "#version 450 core\n"
        "#extension GL_KHR_shader_subgroup_arithmetic : require\n"
        "#extension GL_KHR_shader_subgroup_shuffle : require\n"
        "layout(local_size_x = 32) in;\n"
        "layout(std430, binding = 0) buffer Out { uvec4 o[]; };\n"
        "void main() {\n"
        "    uint i = gl_SubgroupInvocationID;\n"
        "    uint v = (i * 7u) % 32u;\n"
        "    double d = double(i) + 0.25lf;\n"
        "    o[i] = uvec4(subgroupInclusiveMax(v), subgroupExclusiveOr(1u << (i % 8u)),\n"
        "                 subgroupShuffleXor(i, i % 4u), uint(subgroupAdd(d) * 4.0lf));\n"
        "}\n";
    GLuint *out = runCompute(cs);
    int bad = 0;

    if (!out)
        return;

    for (GLuint i = 0; i < 32; i++)
    {
        GLuint mx = 0, orv = 0;

        for (GLuint j = 0; j <= i; j++)
            mx = (j * 7 % 32) > mx ? (j * 7 % 32) : mx;
        for (GLuint j = 0; j < i; j++)
            orv |= 1u << (j % 8);

        // the sum of i + 0.25 over 32 lanes, times four
        if (out[i * 4] != mx || out[i * 4 + 1] != orv || out[i * 4 + 2] != (i ^ (i % 4)) ||
            out[i * 4 + 3] != (496 * 4 + 32))
            if (bad++ < 4)
                CHECK_MSG(0, "lane %u: %u %u %u %u", i, out[i * 4], out[i * 4 + 1], out[i * 4 + 2], out[i * 4 + 3]);
    }
    CHECK_EQ_INT(bad, 0);
}

// A vertex function has no lane or group size inputs in Metal; MGL works the
// lane out from a ballot. Every vertex checks they agree.
GPU_TEST(subgroups, vertex_shaders_know_their_lane)
{
    static const char *vs =
        "#version 450 core\n"
        "#extension GL_KHR_shader_subgroup_ballot : require\n"
        "layout(location = 0) in vec2 p;\n"
        "flat out uint ok;\n"
        "void main() {\n"
        "    uvec4 b = subgroupBallot(true);\n"
        "    ok = uint(gl_SubgroupSize == 32u && subgroupBallotBitExtract(b, gl_SubgroupInvocationID));\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *fs =
        "#version 450 core\n"
        "flat in uint ok;\n"
        "out vec4 o;\n"
        "void main() { o = ok == 1u ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    unsigned char *px;
    char log[4096];
    int green = 0;

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not link: %s", log);
    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
        return;

    mgl_target_bind(&t);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    vao = mgl_fullscreen_quad(&vbo);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        for (int i = 0; i < 64 * 64; i++)
            green += px[i * 4 + 1] == 255 && px[i * 4] == 0;
        free(px);
    }
    CHECK_EQ_INT(green, 64 * 64);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
