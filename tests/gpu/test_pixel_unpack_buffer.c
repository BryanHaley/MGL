/*
 * test_pixel_unpack_buffer.c
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
 * Texture uploads sourced from GL_PIXEL_UNPACK_BUFFER, where the "pixels"
 * argument is a byte offset into the buffer rather than a pointer. The
 * offset used to be applied twice on the way through glTexImage2D, and it
 * was never checked against the size of the buffer.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

#define TW 4
#define TH 4
#define TEXELS (TW * TH)

// a recognisable ramp, one byte apart per channel
static void fill_ramp(unsigned char *p, int texels, int seed)
{
    for (int i = 0; i < texels; i++) {
        p[i * 4 + 0] = (unsigned char)(seed + i * 4 + 0);
        p[i * 4 + 1] = (unsigned char)(seed + i * 4 + 1);
        p[i * 4 + 2] = (unsigned char)(seed + i * 4 + 2);
        p[i * 4 + 3] = 255;
    }
}

GPU_TEST(pixel_unpack_buffer, an_upload_reads_from_the_bound_buffer)
{
    GLuint pbo = 0, tex = 0;
    unsigned char src[TEXELS * 4], got[TEXELS * 4];

    fill_ramp(src, TEXELS, 10);

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof src, src, GL_STATIC_DRAW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TW, TH, 0, GL_RGBA, GL_UNSIGNED_BYTE, (const void *)0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    memset(got, 0, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG(memcmp(src, got, sizeof src) == 0,
              "texel 0 = %d,%d,%d want %d,%d,%d",
              got[0], got[1], got[2], src[0], src[1], src[2]);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &pbo);
}

GPU_TEST(pixel_unpack_buffer, an_upload_starts_at_the_offset_it_was_given)
{
    GLuint pbo = 0, tex = 0;
    unsigned char src[TEXELS * 4 * 2], got[TEXELS * 4];
    const GLsizei off = TEXELS * 4;   // the image sits in the second half

    memset(src, 0x7f, sizeof src);
    fill_ramp(src + off, TEXELS, 90);

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof src, src, GL_STATIC_DRAW);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TW, TH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 (const void *)(size_t)off);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    memset(got, 0, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);

    CHECK_MSG(memcmp(src + off, got, sizeof got) == 0,
              "texel 0 = %d,%d,%d want %d,%d,%d",
              got[0], got[1], got[2], src[off], src[off + 1], src[off + 2]);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &pbo);
}

GPU_TEST(pixel_unpack_buffer, an_offset_past_the_end_is_an_error_not_a_crash)
{
    GLuint pbo = 0, tex = 0;
    unsigned char src[TEXELS * 4];

    fill_ramp(src, TEXELS, 0);

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof src, src, GL_STATIC_DRAW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // the whole image would start one texel before the end of the buffer
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TW, TH, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 (const void *)(size_t)(sizeof src - 4));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &pbo);
}

GPU_TEST(pixel_unpack_buffer, a_persistently_mapped_buffer_can_still_be_uploaded_from)
{
    GLuint pbo = 0, tex = 0;
    unsigned char src[TEXELS * 4], got[TEXELS * 4];
    void *p;

    fill_ramp(src, TEXELS, 40);

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferStorage(GL_PIXEL_UNPACK_BUFFER, sizeof src, src,
                    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    if (mgl_drain_errors() != GL_NO_ERROR) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glDeleteBuffers(1, &pbo);
        SKIP("persistent storage unavailable");
    }

    p = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, sizeof src,
                         GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    CHECK(p != NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // the mapping stays live: this is what GL_MAP_PERSISTENT_BIT is for
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TW, TH, 0, GL_RGBA, GL_UNSIGNED_BYTE, (const void *)0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    memset(got, 0, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_MSG(memcmp(src, got, sizeof src) == 0,
              "texel 0 = %d,%d,%d want %d,%d,%d",
              got[0], got[1], got[2], src[0], src[1], src[2]);

    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &pbo);
}
