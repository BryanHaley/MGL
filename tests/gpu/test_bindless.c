/*
 * test_bindless.c
 * Copyright (C) The Moogle Project
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
 * GL_ARB_bindless_texture: textures reached through handles stored in blocks,
 * in plain uniforms and made on the spot, plus images written by handle.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

static GLuint solidTexture(const unsigned char rgba[4])
{
    unsigned char texels[4 * 4];
    GLuint tex = 0;

    for (int i = 0; i < 4; i++)
        memcpy(texels + i * 4, rgba, 4);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

static bool hasBindless(void)
{
    GLint n = 0;

    glGetIntegerv(GL_NUM_EXTENSIONS, &n);

    for (GLint i = 0; i < n; i++)
        if (!strcmp((const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i), "GL_ARB_bindless_texture"))
            return true;

    return false;
}

GPU_TEST(bindless, advertised)
{
    CHECK_MSG(hasBindless(), "GL_ARB_bindless_texture is not in the extension list");
}

GPU_TEST(bindless, handles_follow_the_rules)
{
    static const unsigned char red[4] = { 255, 0, 0, 255 };
    GLuint tex = solidTexture(red);
    GLuint smp = 0;

    GLuint64 a = glGetTextureHandleARB(tex);
    GLuint64 b = glGetTextureHandleARB(tex);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(a != 0);
    CHECK(a == b);      // the same texture gives the same handle

    glGenSamplers(1, &smp);
    glSamplerParameteri(smp, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    GLuint64 c = glGetTextureSamplerHandleARB(tex, smp);

    CHECK(c != 0 && c != a);
    CHECK(!glIsTextureHandleResidentARB(a));
    glMakeTextureHandleResidentARB(a);
    CHECK(glIsTextureHandleResidentARB(a));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // resident twice, or a handle nobody made, is an error
    glMakeTextureHandleResidentARB(a);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glMakeTextureHandleResidentARB(0x1234);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // texture zero has no handle
    CHECK(glGetTextureHandleARB(0) == 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glMakeTextureHandleNonResidentARB(a);
    CHECK(!glIsTextureHandleResidentARB(a));

    glMakeImageHandleResidentARB(glGetImageHandleARB(tex, 0, GL_FALSE, 0, GL_RGBA8), GL_BLEND);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteSamplers(1, &smp);
    glDeleteTextures(1, &tex);
}

// Handles reach the shader three ways: in a uniform block, in a storage block
// indexed by a value the GPU picks, and built from a plain uvec2 uniform.
GPU_TEST(bindless, compute_samples_through_handles)
{
    static const unsigned char red[4] = { 255, 0, 0, 255 };
    static const unsigned char green[4] = { 0, 255, 0, 255 };
    static const unsigned char blue[4] = { 0, 0, 255, 255 };
    static const char *src =
        "#version 460 core\n"
        "#extension GL_ARB_bindless_texture : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std140, binding = 0) uniform Ubo { sampler2D fromUbo; };\n"
        "layout(std430, binding = 1) buffer Table { uint pick; sampler2D list[]; };\n"
        "layout(std430, binding = 2) buffer Out { vec4 got[3]; };\n"
        "uniform uvec2 raw;\n"
        "void main() {\n"
        "    got[0] = texture(fromUbo, vec2(0.5));\n"
        "    got[1] = textureLod(list[pick], vec2(0.5), 0.0);\n"
        "    got[2] = texelFetch(sampler2D(raw), ivec2(1, 1), 0);\n"
        "}\n";
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(src, log, sizeof log);

    CHECK_MSG(prog != 0, "did not build: %s", log);

    if (prog == 0)
        return;

    GLuint tex[3] = { solidTexture(red), solidTexture(green), solidTexture(blue) };
    GLuint64 h[3];

    for (int i = 0; i < 3; i++)
    {
        h[i] = glGetTextureHandleARB(tex[i]);
        glMakeTextureHandleResidentARB(h[i]);
    }

    GLuint bufs[3];
    GLuint table[2 + 2 * 3] = { 1, 0 };     // pick = 1, then padding to 8 bytes

    memcpy(table + 2, h, sizeof(h));
    glGenBuffers(3, bufs);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, bufs[0]);
    glBufferData(GL_UNIFORM_BUFFER, 16, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 8, &h[0]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(table), table, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bufs[2]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 3 * 16, NULL, GL_DYNAMIC_READ);

    glUseProgram(prog);
    glUniform2ui(glGetUniformLocation(prog, "raw"), (GLuint)h[2], (GLuint)(h[2] >> 32));
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    float got[12] = { 0 };

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(got), got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    static const float want[12] = { 1, 0, 0, 1,   0, 1, 0, 1,   0, 0, 1, 1 };

    for (int i = 0; i < 12; i++)
        CHECK_MSG(got[i] == want[i], "component %d read %g, not %g", i, got[i], want[i]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(3, bufs);
    glDeleteTextures(3, tex);
}

GPU_TEST(bindless, compute_writes_an_image_by_handle)
{
    static const char *src =
        "#version 460 core\n"
        "#extension GL_ARB_bindless_texture : require\n"
        "layout(local_size_x = 1) in;\n"
        "uniform uvec2 target;\n"
        "void main() { imageStore(layout(rgba8) image2D(target), ivec2(1, 0), vec4(0, 1, 0, 1)); }\n";
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(src, log, sizeof log);

    if (prog == 0)
    {
        // some front ends want the format on the declaration instead
        src =
            "#version 460 core\n"
            "#extension GL_ARB_bindless_texture : require\n"
            "layout(local_size_x = 1) in;\n"
            "uniform uvec2 target;\n"
            "void main() { imageStore(image2D(target), ivec2(1, 0), vec4(0, 1, 0, 1)); }\n";
        prog = mgl_build_compute_program(src, log, sizeof log);
    }

    CHECK_MSG(prog != 0, "did not build: %s", log);

    if (prog == 0)
        return;

    GLuint tex = 0;
    unsigned char texels[2 * 4] = { 0 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, texels);

    GLuint64 h = glGetImageHandleARB(tex, 0, GL_FALSE, 0, GL_RGBA8);

    glMakeImageHandleResidentARB(h, GL_WRITE_ONLY);
    glUseProgram(prog);
    glUniform2ui(glGetUniformLocation(prog, "target"), (GLuint)h, (GLuint)(h >> 32));
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glFinish();

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(texels[0], 0);
    CHECK_EQ_UINT(texels[4], 0);
    CHECK_EQ_UINT(texels[5], 255);
    CHECK_EQ_UINT(texels[7], 255);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
}

// The same lookup from a fragment shader, handle in a uniform block
GPU_TEST(bindless, fragment_samples_through_a_handle)
{
    static const unsigned char blue[4] = { 0, 0, 255, 255 };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0, 1); }\n";
    static const char *fs =
        "#version 460 core\n"
        "#extension GL_ARB_bindless_texture : require\n"
        "layout(std140, binding = 3) uniform Mat { sampler2D albedo; };\n"
        "out vec4 color;\n"
        "void main() { color = texture(albedo, vec2(0.5)); }\n";
    char log[2048] = "";
    GLuint prog = mgl_build_program(vs, fs, log, sizeof log);

    CHECK_MSG(prog != 0, "did not build: %s", log);

    if (prog == 0)
        return;

    MGLTestTarget t;

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0))
    {
        CHECK_MSG(0, "no render target");
        glDeleteProgram(prog);
        return;
    }

    GLuint tex = solidTexture(blue);
    GLuint64 h = glGetTextureHandleARB(tex);
    GLuint ubo, vao, vbo;

    glMakeTextureHandleResidentARB(h);
    glGenBuffers(1, &ubo);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, ubo);
    glBufferData(GL_UNIFORM_BUFFER, 16, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 8, &h);

    vao = mgl_fullscreen_quad(&vbo);
    mgl_target_bind(&t);
    glUseProgram(prog);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    unsigned char *px = mgl_read_rgba8(&t);
    unsigned char rgba[4] = { 0 };

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(px != NULL);

    if (px)
    {
        mgl_pixel_at(px, &t, 4, 4, rgba);
        free(px);
    }

    CHECK_EQ_UINT(rgba[0], 0);
    CHECK_EQ_UINT(rgba[2], 255);

    mgl_target_destroy(&t);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &ubo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
}

// GL_EXT_shader_image_load_formatted: an image read without a format qualifier
// takes its format from the image that is bound
GPU_TEST(bindless, image_loads_without_a_format)
{
    static const char *src =
        "#version 460 core\n"
        "#extension GL_EXT_shader_image_load_formatted : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(binding = 0) uniform image2D img;\n"
        "layout(std430, binding = 1) buffer Out { vec4 got; };\n"
        "void main() { got = imageLoad(img, ivec2(1, 0)); }\n";
    static const unsigned char texels[8] = { 0, 0, 0, 0,   0, 255, 0, 255 };
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(src, log, sizeof log);

    CHECK_MSG(prog != 0, "did not build: %s", log);

    if (prog == 0)
        return;

    GLuint tex = 0, buf = 0;
    float got[4] = { 0 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

    glGenBuffers(1, &buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16, NULL, GL_DYNAMIC_READ);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(got), got);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(got[0] == 0.0f && got[1] == 1.0f && got[3] == 1.0f,
              "read %g %g %g %g", got[0], got[1], got[2], got[3]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &buf);
    glDeleteTextures(1, &tex);
}
