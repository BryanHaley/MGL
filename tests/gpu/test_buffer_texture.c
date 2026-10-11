/*
 * test_buffer_texture.c
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
 * Buffer textures read in a shader. glTexBuffer stored the binding and the
 * renderer never made a Metal texture for it, so every samplerBuffer read
 * nothing -- and no test here had ever sampled one.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static const char *CS =
    "#version 460 core\n"
    "layout(local_size_x = 1) in;\n"
    "uniform samplerBuffer tb;\n"
    "layout(std430, binding = 0) buffer Out { vec4 got[2]; };\n"
    "void main() { got[0] = texelFetch(tb, 1); got[1] = texelFetch(tb, 2); }\n";

// runs the shader against whatever is bound to GL_TEXTURE_BUFFER on unit 0
static int fetchTwo(GLuint prog, GLfloat out[8])
{
    GLuint ssbo = 0;

    memset(out, 0, 8 * sizeof(GLfloat));
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 8 * sizeof(GLfloat), out, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "tb"), 0);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 8 * sizeof(GLfloat), out);

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);

    return mgl_drain_errors() == GL_NO_ERROR;
}

GPU_TEST(buffer_texture, a_shader_reads_the_buffer_through_it)
{
    static const GLfloat texels[16] = {
        0, 0, 0, 0,   1, 2, 3, 4,   5, 6, 7, 8,   0, 0, 0, 0,
    };
    GLuint prog, buf = 0, tex = 0;
    GLfloat got[8];
    char log[1024];

    prog = mgl_build_compute_program(CS, log, sizeof log);
    CHECK_MSG(prog != 0, "buffer texture shader did not build: %s", log);

    if (!prog) return;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, sizeof texels, texels, GL_STATIC_DRAW);

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK(fetchTwo(prog, got));
    CHECK_MSG(got[0] == 1 && got[3] == 4 && got[4] == 5 && got[7] == 8,
              "read %g %g %g %g / %g %g %g %g, want 1 2 3 4 / 5 6 7 8",
              got[0], got[1], got[2], got[3], got[4], got[5], got[6], got[7]);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
}

GPU_TEST(buffer_texture, a_range_starts_at_its_offset)
{
    GLfloat texels[64];
    GLuint prog, buf = 0, tex = 0;
    GLint align = 0;
    GLfloat got[8];
    char log[1024];

    glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &align);
    if (align <= 0 || align > 128) SKIP("alignment does not fit the test's buffer");

    prog = mgl_build_compute_program(CS, log, sizeof log);
    if (!prog) SKIP("shader pipeline unavailable");

    // texel n holds n in every channel, so the texel read names its offset
    for (int i = 0; i < 64; i++)
        texels[i] = (GLfloat)(i / 4);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, sizeof texels, texels, GL_STATIC_DRAW);

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA32F, buf, align, (GLsizeiptr)sizeof texels - align);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK(fetchTwo(prog, got));

    GLfloat first = (GLfloat)(align / 16 + 1);
    CHECK_MSG(got[0] == first && got[4] == first + 1,
              "read %g and %g, want %g and %g", got[0], got[4], first, first + 1);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
}

// glBufferData gives the buffer new storage; a view kept from before would
// read memory the buffer no longer owns
GPU_TEST(buffer_texture, new_storage_under_it_is_what_is_read)
{
    static const GLfloat before[16] = { 0,0,0,0,  1,1,1,1,  2,2,2,2,  0,0,0,0 };
    static const GLfloat after[16]  = { 0,0,0,0,  7,7,7,7,  9,9,9,9,  0,0,0,0 };
    GLuint prog, buf = 0, tex = 0;
    GLfloat got[8];
    char log[1024];

    prog = mgl_build_compute_program(CS, log, sizeof log);
    if (!prog) SKIP("shader pipeline unavailable");

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, sizeof before, before, GL_STATIC_DRAW);

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, buf);

    CHECK(fetchTwo(prog, got));
    CHECK_MSG(got[0] == 1 && got[4] == 2, "first read %g %g, want 1 2", got[0], got[4]);

    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, sizeof after, after, GL_STATIC_DRAW);

    CHECK(fetchTwo(prog, got));
    CHECK_MSG(got[0] == 7 && got[4] == 9, "after new storage read %g %g, want 7 9", got[0], got[4]);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
}

// An imageBuffer bind was refused outright (a buffer texture has no mip levels
// in MGL's bookkeeping), and even bound there was no Metal texture behind it.
// Written through the image, the bytes land in the buffer itself.
GPU_TEST(buffer_texture, an_image_buffer_writes_into_the_buffer)
{
    static const char *cs =
        "#version 460 core\n"
        "layout(local_size_x = 4) in;\n"
        "layout(r32f, binding = 0) uniform writeonly imageBuffer img;\n"
        "void main() { imageStore(img, int(gl_GlobalInvocationID.x), vec4(float(gl_GlobalInvocationID.x) + 0.5)); }\n";
    GLfloat zero[4] = { 0, 0, 0, 0 }, got[4];
    GLuint prog, buf = 0, tex = 0;
    char log[1024];

    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "imageBuffer shader did not build: %s", log);

    if (!prog) return;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, sizeof zero, zero, GL_DYNAMIC_DRAW);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, buf);

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glUseProgram(0);

    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glGetBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(got[0] == 0.5f && got[3] == 3.5f,
              "buffer holds %g %g %g %g, want 0.5 1.5 2.5 3.5", got[0], got[1], got[2], got[3]);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
}

// A buffer texture's level 0 describes the range it views. Asking about it
// raised INVALID_VALUE, as if the texture had no levels.
GPU_TEST(buffer_texture, level_queries_describe_the_range)
{
    GLuint tex, buf;
    GLint v = -1;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, 1024, NULL, GL_STATIC_DRAW);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_R32F, buf, 256, 512);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &v);
    CHECK_EQ_INT(v, (GLint)buf);
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_OFFSET, &v);
    CHECK_EQ_INT(v, 256);
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_SIZE, &v);
    CHECK_EQ_INT(v, 512);
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &v);
    CHECK_EQ_INT(v, 128);
    glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_INTERNAL_FORMAT, &v);
    CHECK_EQ_INT(v, GL_R32F);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 1, GL_TEXTURE_WIDTH, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
}

// The limits and state a few extensions add answer instead of raising
// INVALID_ENUM.
GPU_TEST(buffer_texture, extension_state_answers)
{
    GLfloat f = -1.0f;
    GLint n = -1;
    GLuint tex;

    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &f);
    CHECK_MSG(f >= 2.0f, "max anisotropy %f", f);
    glGetFloatv(GL_POLYGON_OFFSET_CLAMP, &f);
    CHECK_MSG(f == 0.0f, "offset clamp %f", f);
    glGetIntegerv(GL_NUM_SPIR_V_EXTENSIONS, &n);
    CHECK_MSG(n >= 0, "spir-v extensions %d", n);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 0.5f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glDeleteTextures(1, &tex);
}

// There are no SPIR-V extensions, so any index is out of range rather than
// the name being unknown.
GPU_TEST(buffer_texture, spirv_extension_strings_are_out_of_range)
{
    CHECK(glGetStringi(GL_SPIR_V_EXTENSIONS, 0) == NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

// glTexBuffer views the whole buffer, so new storage under it changes the
// size it reports. It kept the size from the moment it was attached.
GPU_TEST(buffer_texture, whole_buffer_size_follows_new_storage)
{
    GLuint tex, buf;
    GLint v = -1;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, 512, NULL, GL_STATIC_DRAW);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R8, buf);
    glBufferData(GL_TEXTURE_BUFFER, 1040, NULL, GL_STATIC_DRAW);

    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_SIZE, &v);
    CHECK_EQ_INT(v, 1040);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
}

// a buffer longer than the limit still attaches; the texture is cut to the limit
GPU_TEST(buffer_texture, a_buffer_past_the_limit_is_clamped)
{
    GLuint tex, buf;
    GLint max = 0, w = -1, sz = -1;

    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &max);
    CHECK(max > 0);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, (GLsizeiptr)max + 10, NULL, GL_STATIC_DRAW);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R8I, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_BUFFER_SIZE, &sz);
    CHECK_EQ_INT(w, max);
    CHECK_EQ_INT(sz, max + 10);

    glTexBufferRange(GL_TEXTURE_BUFFER, GL_R8I, buf, 0, (GLsizeiptr)max + 10);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetTexLevelParameteriv(GL_TEXTURE_BUFFER, 0, GL_TEXTURE_WIDTH, &w);
    CHECK_EQ_INT(w, max);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
}

// Metal has no three-channel texel buffer, so these read through a wider copy
GPU_TEST(buffer_texture, an_rgb32_buffer_reads_three_channels_and_one)
{
    static const char *SZ =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "uniform samplerBuffer tb;\n"
        "layout(std430, binding = 0) buffer Out { vec4 got[2]; };\n"
        "void main() { got[0] = texelFetch(tb, 1); got[1] = vec4(textureSize(tb), texelFetch(tb, 2).xyz); }\n";
    static const GLfloat texels[12] = { 0, 0, 0,  1, 2, 3,  5, 6, 7,  9, 9, 9 };
    GLuint prog, buf = 0, tex = 0;
    GLfloat got[8];
    char log[1024];

    prog = mgl_build_compute_program(SZ, log, sizeof log);
    CHECK_MSG(prog != 0, "buffer texture shader did not build: %s", log);

    if (!prog) return;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, sizeof texels, texels, GL_STATIC_DRAW);

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, tex);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK(fetchTwo(prog, got));
    CHECK_MSG(got[0] == 1 && got[1] == 2 && got[2] == 3 && got[3] == 1,
              "read %g %g %g %g, want 1 2 3 1", got[0], got[1], got[2], got[3]);
    CHECK_MSG(got[4] == 4 && got[5] == 5 && got[7] == 7,
              "size %g, texel 2 %g %g %g; want 4, 5 6 7", got[4], got[5], got[6], got[7]);

    // new data in the buffer is what the next read sees
    GLfloat nine[3] = { 8, 8, 8 };
    glBufferSubData(GL_TEXTURE_BUFFER, 12, sizeof nine, nine);
    CHECK(fetchTwo(prog, got));
    CHECK_MSG(got[0] == 8 && got[2] == 8, "after update read %g %g, want 8 8", got[0], got[2]);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
}
