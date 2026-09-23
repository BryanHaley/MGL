/*
 * test_image_store.c
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
 * A compute shader writes an image and the value comes back. Metal fixes a
 * texture's usage when it is made, so these also cover a texture that already
 * existed before it was bound as an image.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

static GLuint storeProgram(const char *layout, const char *type, const char *value)
{
    char src[512], log[2048] = "";

    snprintf(src, sizeof src,
             "#version 460 core\n"
             "layout(local_size_x = 1) in;\n"
             "layout(%s, binding = 0) uniform %s img;\n"
             "void main() { imageStore(img, ivec2(0, 0), %s); }\n",
             layout, type, value);

    GLuint prog = mgl_build_compute_program(src, log, sizeof log);

    CHECK_MSG(prog != 0, "%s program did not build: %s", layout, log);
    return prog;
}

static void storeAndRead(GLuint prog, GLenum ifmt, GLenum rfmt, GLenum rtype,
                         bool read_first, void *out, size_t out_size)
{
    GLuint tex = 0;
    unsigned char scratch[64];

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, ifmt, 4, 1);

    /* reading makes the Metal texture before GL asks to write to it */
    if (read_first)
        glGetTexImage(GL_TEXTURE_2D, 0, rfmt, rtype, scratch);

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_WRITE, ifmt);
    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glFinish();

    memset(out, 0, out_size);
    glGetTexImage(GL_TEXTURE_2D, 0, rfmt, rtype, out);

    glUseProgram(0);
    glDeleteTextures(1, &tex);
}

GPU_TEST(image_store, compute_writes_every_kind_of_image)
{
    for (int read_first = 0; read_first < 2; read_first++)
    {
        GLuint prog;
        unsigned char c[16];
        GLuint u[4];
        GLint i[4];
        GLfloat f[4];

        if ((prog = storeProgram("rgba8", "image2D", "vec4(0.2, 0.4, 0.6, 1.0)")))
        {
            storeAndRead(prog, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, read_first, c, sizeof c);
            CHECK_MSG(c[0] == 0x33 && c[1] == 0x66 && c[2] == 0x99 && c[3] == 0xff,
                      "rgba8 (made first=%d): %02x %02x %02x %02x", read_first, c[0], c[1], c[2], c[3]);
            glDeleteProgram(prog);
        }

        if ((prog = storeProgram("r32ui", "uimage2D", "uvec4(1234u)")))
        {
            storeAndRead(prog, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, read_first, u, sizeof u);
            CHECK_MSG(u[0] == 1234u, "r32ui (made first=%d): %u", read_first, u[0]);
            glDeleteProgram(prog);
        }

        if ((prog = storeProgram("r32i", "iimage2D", "ivec4(-5)")))
        {
            storeAndRead(prog, GL_R32I, GL_RED_INTEGER, GL_INT, read_first, i, sizeof i);
            CHECK_MSG(i[0] == -5, "r32i (made first=%d): %d", read_first, i[0]);
            glDeleteProgram(prog);
        }

        if ((prog = storeProgram("r32f", "image2D", "vec4(3.5)")))
        {
            storeAndRead(prog, GL_R32F, GL_RED, GL_FLOAT, read_first, f, sizeof f);
            CHECK_MSG(f[0] == 3.5f, "r32f (made first=%d): %f", read_first, f[0]);
            glDeleteProgram(prog);
        }
    }
}

/* Binding a texture as an image must not disturb what is already in it. */
GPU_TEST(image_store, binding_as_an_image_keeps_contents)
{
    GLuint tex = 0, in[4] = {5, 6, 7, 8}, before[4] = {0}, after[4] = {0};

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 4, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, in);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, before);

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, after);

    for (int k = 0; k < 4; k++)
        CHECK_MSG(before[k] == in[k] && after[k] == in[k],
                  "texel %d: uploaded %u, before binding %u, after %u", k, in[k], before[k], after[k]);

    glDeleteTextures(1, &tex);
}

static const char *QUAD_VS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

/* Metal numbers a shader's textures by declaration order; GL picks them by
   unit. Two samplers on units 5 and 2 read back in the order GL says. */
GPU_TEST(image_store, samplers_read_the_unit_they_are_set_to)
{
    static const char *fs =
        "#version 460 core\n"
        "uniform sampler2D first;\n"
        "uniform sampler2D second;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(texture(first, vec2(0.5)).r, texture(second, vec2(0.5)).r, 0.0, 1.0); }\n";
    char log[2048] = "";
    GLuint prog = mgl_build_program(QUAD_VS, fs, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    GLubyte red = 200, dark = 40;
    GLuint tex[2];

    glGenTextures(2, tex);

    for (int t = 0; t < 2; t++)
    {
        glActiveTexture(t ? GL_TEXTURE2 : GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, tex[t]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, t ? &dark : &red);
    }

    glActiveTexture(GL_TEXTURE0);

    MGLTestTarget target;
    GLuint vbo = 0, vao;

    CHECK(mgl_target_create(&target, 4, 4, GL_RGBA8, 0));
    mgl_target_bind(&target);
    vao = mgl_fullscreen_quad(&vbo);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "first"), 5);
    glUniform1i(glGetUniformLocation(prog, "second"), 2);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glFinish();

    unsigned char *px = mgl_read_rgba8(&target);
    unsigned char rgba[4] = {0};

    if (px)
        mgl_pixel_at(px, &target, 2, 2, rgba);

    CHECK_MSG(abs(rgba[0] - 200) <= 2 && abs(rgba[1] - 40) <= 2,
              "first read %u (want 200), second read %u (want 40)", rgba[0], rgba[1]);

    free(px);
    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(2, tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&target);
}

/* A fragment shader writes an image too, not only a compute shader. */
GPU_TEST(image_store, fragment_shader_writes_an_image)
{
    static const char *fs =
        "#version 460 core\n"
        "layout(r32ui, binding = 3) uniform uimage2D img;\n"
        "out vec4 o;\n"
        "void main() {\n"
        "    imageStore(img, ivec2(gl_FragCoord.xy), uvec4(77u));\n"
        "    o = vec4(1.0);\n"
        "}\n";
    char log[2048] = "";
    GLuint prog = mgl_build_program(QUAD_VS, fs, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 4, 4);
    glBindImageTexture(3, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);

    MGLTestTarget target;
    GLuint vbo = 0, vao;

    CHECK(mgl_target_create(&target, 4, 4, GL_RGBA8, 0));
    mgl_target_bind(&target);
    vao = mgl_fullscreen_quad(&vbo);

    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glFinish();

    GLuint got[16] = {0};

    glBindTexture(GL_TEXTURE_2D, tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, got);
    CHECK_MSG(got[0] == 77 && got[15] == 77, "image holds %u and %u, want 77", got[0], got[15]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&target);
}

// GL lets an image unit name a format other than the texture's own; one of
// the same size reads the texels as that format. It was refused outright.
GPU_TEST(image_store, binding_as_another_format_is_not_an_error)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
    glDeleteTextures(1, &tex);
}

// Metal has atomics on cube textures only from MSL 4.0. Before that the
// shader holds a storage cube as a 2D array of its faces -- which is how GL
// numbers the texels anyway -- and the unit is bound as that view. imageSize
// still answers as a cube.
GPU_TEST(image_store, atomics_on_cube_and_cube_array_images)
{
    static const char *cs =
        "#version 450\n"
        "layout(local_size_x = 1) in;\n"
        "layout(r32ui, binding = 0) uniform uimageCube c;\n"
        "layout(r32ui, binding = 1) uniform uimageCubeArray ca;\n"
        "layout(std430, binding = 0) buffer Out { ivec2 cs; ivec3 cas; };\n"
        "void main() {\n"
        "  imageAtomicAdd(c, ivec3(1, 2, 4), 5u);\n"
        "  imageAtomicMax(ca, ivec3(3, 0, 6 * 1 + 2), 9u);\n"
        "  imageStore(c, ivec3(0, 0, 1), uvec4(7u));\n"
        "  cs = imageSize(c);\n"
        "  cas = imageSize(ca);\n"
        "}\n";
    static const GLuint zero = 0;
    GLuint prog, cube = 0, arr = 0, ssbo = 0;
    GLuint face[16], all[4 * 4 * 12];
    GLint sizes[8] = { 0 };
    char log[2048];

    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "cube atomics kernel did not build: %s", log);

    if (!prog) return;

    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_R32UI, 4, 4);
    glClearTexImage(cube, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    glGenTextures(1, &arr);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, arr);
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_R32UI, 4, 4, 12);
    glClearTexImage(arr, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof sizes, sizes, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glBindImageTexture(0, cube, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    glBindImageTexture(1, arr, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // face 4 is +Z; two dispatches of +5
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, face);
    CHECK_MSG(face[2 * 4 + 1] == 10, "+Z texel (1,2) holds %u, want 10", face[2 * 4 + 1]);

    glGetTexImage(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, face);
    CHECK_MSG(face[0] == 7, "-X texel (0,0) holds %u, want 7", face[0]);

    // layer 1, face 2 is layer-face 8
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, arr);
    glGetTexImage(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, all);
    CHECK_MSG(all[8 * 16 + 3] == 9, "layer-face 8 texel (3,0) holds %u, want 9", all[8 * 16 + 3]);
    CHECK_MSG(all[2 * 16 + 3] == 0, "layer-face 2 was written too: %u", all[2 * 16 + 3]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof sizes, sizes);
    CHECK_MSG(sizes[0] == 4 && sizes[1] == 4, "cube size %d x %d, want 4 x 4", sizes[0], sizes[1]);
    // cas is an ivec3 at offset 16 in std430
    CHECK_MSG(sizes[4] == 4 && sizes[5] == 4 && sizes[6] == 2,
              "cube array size %d x %d x %d, want 4 x 4 x 2", sizes[4], sizes[5], sizes[6]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(1, &cube);
    glDeleteTextures(1, &arr);
    glDeleteBuffers(1, &ssbo);
}

// A texture bound to an image unit as another format of its size reads its
// texels as that format. Metal needs the texture made for such views, so
// the first cast remakes it and carries its contents across.
GPU_TEST(image_store, a_cast_reads_the_same_bytes_as_another_format)
{
    static const char *cs =
        "#version 450\n"
        "layout(local_size_x = 1) in;\n"
        "layout(r32ui, binding = 0) uniform readonly uimage2D img;\n"
        "layout(std430, binding = 0) buffer Out { uint got; };\n"
        "void main() { got = imageLoad(img, ivec2(1, 0)).x; }\n";
    static const GLubyte texels[2 * 4] = { 0, 0, 0, 0,  0x11, 0x22, 0x33, 0x44 };
    GLuint prog, tex = 0, ssbo = 0, got = 0;
    char log[1024];

    prog = mgl_build_compute_program(cs, log, sizeof log);
    CHECK_MSG(prog != 0, "cast kernel did not build: %s", log);

    if (!prog) return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, texels);

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof got, &got, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, &got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(got == 0x44332211u, "read 0x%08x, want 0x44332211", got);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &ssbo);
}

static GLuint filledImage(GLuint unit, GLuint rgba)
{
    GLuint tex = 0, texels[16];

    for (int k = 0; k < 16; k++)
        texels[k] = rgba;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    glBindImageTexture(unit, tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    return tex;
}

/* An image with no binding sits on unit 0 beside ones whose layout names
   their units, and each reads the texture on its own unit. */
GPU_TEST(image_store, declared_bindings_pick_units)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "writeonly uniform image2D result;\n"
        "layout(binding = 1, rgba8) uniform image2D a;\n"
        "layout(binding = 2, rgba8) uniform image2D b;\n"
        "layout(binding = 4, rgba8) uniform image2D c;\n"
        "void main() {\n"
        "    vec4 x = imageLoad(a, ivec2(0)), y = imageLoad(b, ivec2(0)), z = imageLoad(c, ivec2(0));\n"
        "    imageStore(result, ivec2(0, 0), x);\n"
        "    imageStore(result, ivec2(1, 0), y);\n"
        "    imageStore(result, ivec2(2, 0), z);\n"
        "}\n", log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    GLuint out = filledImage(0, 0);
    GLuint ta = filledImage(1, 0x000000ffu), tb = filledImage(2, 0x0000ff00u), tc = filledImage(4, 0x00ff0000u);
    GLuint got[16] = {0};

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "result"), 0);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glBindTexture(GL_TEXTURE_2D, out);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);

    CHECK_MSG(got[0] == 0x000000ffu && got[1] == 0x0000ff00u && got[2] == 0x00ff0000u,
              "read %08x %08x %08x", got[0], got[1], got[2]);

    glUseProgram(0);
    glDeleteProgram(prog);
    GLuint all[4] = {out, ta, tb, tc};
    glDeleteTextures(4, all);
}

/* layout(binding = 1) on an image array gives element i unit 1 + i. */
GPU_TEST(image_store, image_array_elements_take_consecutive_units)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "writeonly uniform image2D result;\n"
        "layout(binding = 1, rgba8) uniform image2D a[3];\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++)\n"
        "        imageStore(result, ivec2(i, 0), imageLoad(a[i], ivec2(0)));\n"
        "}\n", log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    GLuint colors[3] = {0x000000ffu, 0x0000ff00u, 0x00ff0000u};
    GLuint tex[4] = {filledImage(0, 0), filledImage(1, colors[0]), filledImage(2, colors[1]), filledImage(3, colors[2])};
    GLuint got[16] = {0};
    GLint unit = -1;

    for (int i = 0; i < 3; i++)
    {
        char name[16];

        snprintf(name, sizeof name, "a[%d]", i);
        glGetUniformiv(prog, glGetUniformLocation(prog, name), &unit);
        CHECK_MSG(unit == 1 + i, "%s reports unit %d", name, unit);
    }

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);

    for (int i = 0; i < 3; i++)
        CHECK_MSG(got[i] == colors[i], "element %d read %08x", i, got[i]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(4, tex);
}

/* Atomics on RGBA8 textures bound as R32I and R32UI images, from a fragment
   shader, land in the texture's own bytes. */
GPU_TEST(image_store, fragment_atomics_through_a_cast)
{
    static const char *vs =
        "#version 420 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 420 core\n"
        "layout(location = 0) out vec4 o_color;\n"
        "layout(r32i) coherent uniform iimage2D g_image0;\n"
        "layout(r32ui) coherent uniform uimage2D g_image1;\n"
        "void main() {\n"
        "  o_color = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "  ivec2 coord = ivec2(gl_FragCoord);\n"
        "  if (imageAtomicAdd(g_image0, coord, 2) != 0) o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "  if (imageAtomicAdd(g_image0, coord, -1) != 2) o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "  if (imageAtomicAdd(g_image1, coord, 1) != 0) o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "  if (imageAtomicAdd(g_image1, coord, 2) != 1) o_color = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    char log[2048] = "";
    GLuint prog = mgl_build_program(vs, fs, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    enum { W = 16, H = 16 };
    GLubyte zeros[W * H * 4] = {0}, got[W * H * 4];
    GLuint tex[2], vbo = 0, vao = mgl_fullscreen_quad(&vbo);
    MGLTestTarget t;

    CHECK(mgl_target_create(&t, W, H, GL_RGBA8, 0));
    mgl_target_bind(&t);
    glViewport(0, 0, W, H);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "g_image0"), 0);
    glUniform1i(glGetUniformLocation(prog, "g_image1"), 1);

    glGenTextures(2, tex);
    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, zeros);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindImageTexture(0, tex[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32I);
    glBindImageTexture(1, tex[1], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    const GLubyte want[2] = { 1, 3 };

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
        memset(got, 0xee, sizeof got);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);

        for (int k = 0; k < W * H; k++)
            if (got[k * 4] != want[i] || got[k * 4 + 1] || got[k * 4 + 2] || got[k * 4 + 3])
            {
                CHECK_MSG(0, "image %d texel %d is %u %u %u %u, want %u 0 0 0", i, k,
                          got[k * 4], got[k * 4 + 1], got[k * 4 + 2], got[k * 4 + 3], want[i]);
                break;
            }
    }

    unsigned char *px = mgl_read_rgba8(&t), c[4];

    mgl_pixel_at(px, &t, 3, 3, c);
    CHECK_MSG(c[0] == 0 && c[1] == 255, "fragment wrote %u %u %u", c[0], c[1], c[2]);
    free(px);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteTextures(2, tex);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);
}
