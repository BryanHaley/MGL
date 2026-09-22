/*
 * test_clear_texture.c
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
 * glClearTexImage and glClearTexSubImage: a level well down the chain with
 * nothing above it, an empty region, a depth texture, and the errors.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

/* ---------- a level the application defined on its own ---------- */

GPU_TEST(clear_texture, clears_a_level_with_nothing_above_it)
{
    GLuint tex = 0;
    const int w = 32, h = 32;
    unsigned char *fill = malloc(w * h * 4);
    unsigned char *back = malloc(w * h * 4);
    unsigned char clear[4] = { 9, 8, 7, 6 };

    if (!fill || !back)
    {
        CHECK(0);
        free(fill);
        free(back);
        return;
    }

    memset(fill, 0x33, w * h * 4);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);

    // only level 3 exists; levels 0 to 2 are never defined
    glTexImage2D(GL_TEXTURE_2D, 3, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fill);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexImage(GL_TEXTURE_2D, 3, GL_RGBA, GL_UNSIGNED_BYTE, back);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(back[0], 0x33);

    glClearTexImage(tex, 3, GL_RGBA, GL_UNSIGNED_BYTE, clear);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(back, 0, w * h * 4);
    glGetTexImage(GL_TEXTURE_2D, 3, GL_RGBA, GL_UNSIGNED_BYTE, back);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT(back[0], 9);
    CHECK_EQ_INT(back[1], 8);
    CHECK_EQ_INT(back[2], 7);
    CHECK_EQ_INT(back[3], 6);
    CHECK_EQ_INT(back[(w * h - 1) * 4], 9);

    glDeleteTextures(1, &tex);
    free(fill);
    free(back);
}

/* ---------- an empty region does nothing, quietly ---------- */

GPU_TEST(clear_texture, an_empty_region_is_not_an_error)
{
    GLuint tex = 0;
    const int w = 8, h = 8;
    unsigned char fill[8 * 8 * 4];
    unsigned char back[8 * 8 * 4];
    unsigned char clear[4] = { 1, 2, 3, 4 };

    memset(fill, 0x44, sizeof fill);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fill);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClearTexSubImage(tex, 0, 0, 0, 0, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, clear);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClearTexSubImage(tex, 0, 0, 0, 0, 0, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, clear);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(back, 0, sizeof back);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, back);
    CHECK_EQ_INT(back[0], 0x44);

    // a negative one is still an error
    glClearTexSubImage(tex, 0, 0, 0, 0, w, h, -1, GL_RGBA, GL_UNSIGNED_BYTE, clear);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &tex);
}

/* ---------- a depth texture takes pixels from the CPU ---------- */

GPU_TEST(clear_texture, a_depth_texture_takes_and_gives_back_pixels)
{
    GLuint tex = 0;
    const int w = 16, h = 16;
    float fill[16 * 16];
    float back[16 * 16];
    float clear = 0.25f;

    for (int i = 0; i < w * h; i++)
        fill[i] = 0.5f;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, fill);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(back, 0, sizeof back);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, back);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(back[0] > 0.49f && back[0] < 0.51f, "the depth texture read back %f, wanted 0.5", back[0]);

    glClearTexImage(tex, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &clear);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(back, 0, sizeof back);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, back);
    CHECK_MSG(back[0] > 0.24f && back[0] < 0.26f, "the cleared depth texture read back %f, wanted 0.25", back[0]);
    CHECK_MSG(back[w * h - 1] > 0.24f && back[w * h - 1] < 0.26f,
              "the last depth texel read back %f, wanted 0.25", back[w * h - 1]);

    glDeleteTextures(1, &tex);
}

/* ---------- the clear format has to name the same kind of data ---------- */

GPU_TEST(clear_texture, errors)
{
    GLuint tex = 0, depth = 0, integer = 0, buf = 0, tbo = 0;
    unsigned char value[8] = { 0 };

    // a name nobody made
    glClearTexImage(4242, 0, GL_RGBA, GL_UNSIGNED_BYTE, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glClearTexImage(0, 0, GL_RGBA, GL_UNSIGNED_BYTE, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // a level nobody defined
    glClearTexImage(tex, 2, GL_RGBA, GL_UNSIGNED_BYTE, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // colour storage will not take a depth or an integer clear value
    glClearTexImage(tex, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glClearTexImage(tex, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glClearTexImage(tex, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 8, 8, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    mgl_drain_errors();

    // and depth storage will not take a colour one
    glClearTexImage(depth, 0, GL_RGBA, GL_FLOAT, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGenTextures(1, &integer);
    glBindTexture(GL_TEXTURE_2D, integer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 8, 8, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, NULL);
    mgl_drain_errors();

    glClearTexImage(integer, 0, GL_RGBA, GL_UNSIGNED_BYTE, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glClearTexImage(integer, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // a buffer texture holds someone else's buffer
    glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(GL_TEXTURE_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glGenTextures(1, &tbo);
    glBindTexture(GL_TEXTURE_BUFFER, tbo);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, buf);
    mgl_drain_errors();

    glClearTexImage(tbo, 0, GL_RGBA, GL_FLOAT, value);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &tbo);
    glDeleteBuffers(1, &buf);
    glDeleteTextures(1, &integer);
    glDeleteTextures(1, &depth);
    glDeleteTextures(1, &tex);
}
