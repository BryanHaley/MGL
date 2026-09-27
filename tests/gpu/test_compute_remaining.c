/*
 * test_compute_remaining.c
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
 * Compute shapes near the five compute_shader cases still failing: switching
 * between two compute programs, a double-precision uniform, a subroutine, a
 * texture a kernel writes and a later draw samples, and a buffer texture read
 * through a samplerBuffer. They do not reproduce those five cases; the CTS's
 * several-shaders-on-one-stage case in particular is not covered here.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

/* ---------- helpers ---------- */

/* Creates a storage buffer of `bytes`, zero-filled, and binds it to `binding`. */
static GLuint make_ssbo(GLuint binding, size_t bytes)
{
    GLuint buf = 0;
    void *zero = calloc(1, bytes ? bytes : 1);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buf);

    free(zero);
    return buf;
}

/* ---------- 1: two compute programs on one stage ---------- */

// Dispatching program A, then B, then A again runs the right kernel each time.
GPU_TEST(compute_remaining, two_programs_on_one_stage_switch_cleanly)
{
    static const char *WRITE_ONE_CS =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430, binding = 0) buffer Out { uint marker; };\n"
        "void main() { marker = 1u; }\n";
    static const char *WRITE_TWO_CS =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430, binding = 0) buffer Out { uint marker; };\n"
        "void main() { marker = 2u; }\n";
    char log[1024] = { 0 };
    GLuint prog1, prog2, ssbo;
    GLuint marker = 0;

    prog1 = mgl_build_compute_program(WRITE_ONE_CS, log, sizeof log);
    CHECK_MSG(prog1 != 0, "first compute program did not build: %s", log);
    if (!prog1)
        return;

    prog2 = mgl_build_compute_program(WRITE_TWO_CS, log, sizeof log);
    CHECK_MSG(prog2 != 0, "second compute program did not build: %s", log);
    if (!prog2)
    {
        glDeleteProgram(prog1);
        return;
    }

    ssbo = make_ssbo(0, sizeof marker);

    glUseProgram(prog1);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof marker, &marker);
    CHECK_MSG(marker == 1u, "after the first dispatch the marker is %u, want 1", marker);

    glUseProgram(prog2);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof marker, &marker);
    CHECK_MSG(marker == 2u, "after the second dispatch the marker is %u, want 2", marker);

    glUseProgram(prog1);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof marker, &marker);
    CHECK_MSG(marker == 1u, "after switching back the marker is %u, want 1", marker);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog1);
    glDeleteProgram(prog2);
}

/* ---------- 2: a double-precision uniform ---------- */

// A double uniform reaches a compute shader whole. Metal has no double, so
// MGL carries it as three floats; the product has to keep the bits a single
// float would lose.
GPU_TEST(compute_remaining, double_precision_uniform)
{
    static const char *CS =
        "#version 460 core\n"
        "layout(local_size_x = 4) in;\n"
        "uniform double scale;\n"
        "layout(std430, binding = 0) buffer Data { double v[]; };\n"
        "void main() { uint i = gl_GlobalInvocationID.x; v[i] = v[i] * scale; }\n";
    static const GLdouble inputs[4] = { 1.0, 2.5, 3.25, 4.125 };
    const GLdouble scale = 1.234567890123456;
    GLdouble got[4] = { 0, 0, 0, 0 };
    char log[1024] = { 0 };
    GLuint prog, ssbo;
    GLint loc;

    prog = mgl_build_compute_program(CS, log, sizeof log);
    CHECK_MSG(prog != 0, "double uniform kernel did not build: %s", log);
    if (!prog)
        return;

    ssbo = make_ssbo(0, sizeof inputs);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof inputs, inputs);

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "scale");
    CHECK_MSG(loc >= 0, "the shader has no uniform named scale");
    glUniform1d(loc, scale);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);

    for (int i = 0; i < 4; i++)
    {
        GLdouble want = inputs[i] * scale;

        CHECK_MSG(fabs(got[i] - want) <= 1e-12,
                  "v[%d] * scale is %.17g, want %.17g", i, got[i], want);
    }

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
}

/* ---------- 3: a subroutine in a compute shader ---------- */

// glslang refuses the subroutine keyword when it targets SPIR-V, so MGL
// rewrites it into a selector before the compiler sees it. The uniform still
// has to choose which function the compute invocation runs.
GPU_TEST(compute_remaining, subroutine_selects_the_function)
{
    static const char *CS =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "subroutine void ComputeOp();\n"
        "subroutine uniform ComputeOp op;\n"
        "layout(std430, binding = 0) buffer Out { int r; };\n"
        "subroutine(ComputeOp) void write42() { r = 42; }\n"
        "subroutine(ComputeOp) void write99() { r = 99; }\n"
        "void main() { op(); }\n";
    char log[2048] = { 0 };
    GLuint prog, ssbo, idx42, idx99;
    GLint loc;
    GLint r = 0;

    prog = mgl_build_compute_program(CS, log, sizeof log);
    CHECK_MSG(prog != 0, "subroutine kernel did not build: %s", log);
    if (!prog)
        return;

    ssbo = make_ssbo(0, sizeof r);

    glUseProgram(prog);

    idx42 = glGetSubroutineIndex(prog, GL_COMPUTE_SHADER, "write42");
    idx99 = glGetSubroutineIndex(prog, GL_COMPUTE_SHADER, "write99");
    loc = glGetSubroutineUniformLocation(prog, GL_COMPUTE_SHADER, "op");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG(idx42 != GL_INVALID_INDEX, "write42 has no subroutine index");
    CHECK_MSG(idx99 != GL_INVALID_INDEX, "write99 has no subroutine index");
    CHECK_MSG(idx42 != idx99, "both subroutines got index %u", idx42);
    CHECK_MSG(loc >= 0, "the subroutine uniform op has no location");

    {
        GLuint idx = idx42;

        glUniformSubroutinesuiv(GL_COMPUTE_SHADER, 1, &idx);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof r, &r);
        CHECK_MSG(r == 42, "write42 was selected but the kernel wrote %d", r);
    }

    {
        GLuint idx = idx99;

        glUniformSubroutinesuiv(GL_COMPUTE_SHADER, 1, &idx);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof r, &r);
        CHECK_MSG(r == 99, "write99 was selected but the kernel wrote %d", r);
    }

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
}

/* ---------- 4: a compute kernel whose texture a draw samples ---------- */

// The kernel writes a texture with imageStore; a later draw samples it. GL
// requires a barrier between the two, or the fragment stage may read the
// texture before the image writes are visible.
GPU_TEST(compute_remaining, compute_to_texture_pipeline_chain)
{
    static const char *CS =
        "#version 460 core\n"
        "layout(local_size_x = 4, local_size_y = 4) in;\n"
        "layout(rgba32f, binding = 0) uniform writeonly image2D img;\n"
        "void main() { imageStore(img, ivec2(gl_GlobalInvocationID.xy), vec4(0.25, 0.5, 0.75, 1.0)); }\n";
    static const char *VS =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 460 core\n"
        "uniform sampler2D src;\n"
        "out vec4 o;\n"
        "void main() { o = texture(src, vec2(0.5)); }\n";
    char log[2048] = { 0 };
    MGLTestTarget t;
    GLuint cs, draw, tex = 0, vao, vbo;
    unsigned char *px;
    unsigned char c[4] = { 0 };

    cs = mgl_build_compute_program(CS, log, sizeof log);
    CHECK_MSG(cs != 0, "image-writing kernel did not build: %s", log);
    if (!cs)
        return;

    draw = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(draw != 0, "sampling program did not build: %s", log);
    if (!draw)
    {
        glDeleteProgram(cs);
        return;
    }

    if (!mgl_target_create(&t, 4, 4, GL_RGBA8, 0))
    {
        CHECK(0);
        glDeleteProgram(cs);
        glDeleteProgram(draw);
        return;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 4, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUseProgram(cs);
    glDispatchCompute(1, 1, 1);

    /* GL 4.6 section 7.12: the image writes must be visible to a later fetch. */
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUseProgram(draw);
    glUniform1i(glGetUniformLocation(draw, "src"), 0);
    vao = mgl_fullscreen_quad(&vbo);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, 2, 2, c);
        free(px);
    }

    /* 0.25, 0.5 and 0.75 in an 8-bit UNORM target are 64, 128 and 191. */
    CHECK_MSG(abs((int)c[0] - 64) <= 2 && abs((int)c[1] - 128) <= 2 && abs((int)c[2] - 191) <= 2,
              "the draw sampled %u %u %u, want 64 128 191", c[0], c[1], c[2]);

    glUseProgram(0);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteTextures(1, &tex);
    glDeleteProgram(cs);
    glDeleteProgram(draw);
    mgl_target_destroy(&t);
}

/* ---------- 5: a resource texture, read with texelFetch ---------- */

// A buffer texture backed by 16 floats, read in a compute shader through a
// samplerBuffer. texelFetch has to hand back exactly the values uploaded.
GPU_TEST(compute_remaining, buffer_texture_resource)
{
    static const char *CS =
        "#version 460 core\n"
        "layout(local_size_x = 16) in;\n"
        "uniform samplerBuffer tb;\n"
        "layout(std430, binding = 0) buffer Out { float v[16]; };\n"
        "void main() { int i = int(gl_GlobalInvocationID.x); v[i] = texelFetch(tb, i).r; }\n";
    GLfloat texels[16];
    GLfloat got[16] = { 0 };
    char log[1024] = { 0 };
    GLuint prog, ssbo, buf = 0, tex = 0;

    for (int i = 0; i < 16; i++)
        texels[i] = (GLfloat)i + 0.5f;

    prog = mgl_build_compute_program(CS, log, sizeof log);
    CHECK_MSG(prog != 0, "buffer texture kernel did not build: %s", log);
    if (!prog)
        return;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, sizeof texels, texels, GL_STATIC_DRAW);

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    ssbo = make_ssbo(0, sizeof got);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "tb"), 0);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);

    for (int i = 0; i < 16; i++)
        CHECK_MSG(got[i] == texels[i], "texel %d read %g, want %g", i, got[i], texels[i]);

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
}
