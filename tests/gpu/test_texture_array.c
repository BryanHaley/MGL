/*
 * test_texture_array.c
 * Copyright (C) The MooGL Project
 *
 * Array textures, end to end. Every one of these used to come back as the
 * renderer's emergency gradient: an unbraced else in the array branch of
 * createMTLTextureFromGLTexture returned NULL before Metal ever saw them.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

#define AW 7
#define AH 7

static void fill(GLubyte *px, GLsizei layers, GLubyte base)
{
    for (GLsizei l = 0; l < layers; l++)
        for (int y = 0; y < AH; y++)
            for (int x = 0; x < AW; x++)
            {
                size_t i = (((size_t)l * AH + y) * AW + x) * 4;
                px[i + 0] = (GLubyte)(base + l);
                px[i + 1] = (GLubyte)y;
                px[i + 2] = (GLubyte)x;
                px[i + 3] = 0xFF;
            }
}

static GLuint make_array(GLsizei layers, GLubyte base, GLubyte *px)
{
    GLuint tex = 0;

    fill(px, layers, base);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, AW, AH, layers, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);

    return tex;
}

static int diff_bytes(const GLubyte *a, const GLubyte *b, size_t n)
{
    int bad = 0;

    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i])
            bad++;

    return bad;
}

static void round_trip(GLsizei layers)
{
    size_t bytes = (size_t)AW * AH * layers * 4;
    GLubyte *px = malloc(bytes), *got = malloc(bytes);
    GLuint tex;

    memset(got, 0xAB, bytes);
    tex = make_array(layers, 0x10, px);
    mgl_drain_errors();

    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_MSG(diff_bytes(px, got, bytes) == 0,
              "%d-layer array round trip: %d of %zu bytes differ",
              (int)layers, diff_bytes(px, got, bytes), bytes);

    glDeleteTextures(1, &tex);
    free(px);
    free(got);
}

GPU_TEST(texture_array, one_layer_round_trips)   { round_trip(1); }
GPU_TEST(texture_array, two_layers_round_trip)   { round_trip(2); }
GPU_TEST(texture_array, twelve_layers_round_trip){ round_trip(12); }

/* Metal's sourceSize.depth only moves multiple images for a 3D texture; an
   array needs one blit per layer. */
GPU_TEST(texture_array, copy_image_moves_every_layer)
{
    const GLsizei layers = 12;
    size_t bytes = (size_t)AW * AH * layers * 4;
    GLubyte *a = malloc(bytes), *b = malloc(bytes), *got = malloc(bytes);
    GLuint src = make_array(layers, 0x10, a);
    GLuint dst = make_array(layers, 0x80, b);

    mgl_drain_errors();

    glCopyImageSubData(src, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
                       dst, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, AW, AH, layers);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(got, 0xAB, bytes);
    glBindTexture(GL_TEXTURE_2D_ARRAY, dst);
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_MSG(diff_bytes(a, got, bytes) == 0,
              "destination: %d of %zu bytes differ", diff_bytes(a, got, bytes), bytes);

    memset(got, 0xAB, bytes);
    glBindTexture(GL_TEXTURE_2D_ARRAY, src);
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_MSG(diff_bytes(a, got, bytes) == 0,
              "source was modified: %d of %zu bytes differ", diff_bytes(a, got, bytes), bytes);

    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
    free(a); free(b); free(got);
}

/* A 1D array keeps its layer count in height, which both the Metal descriptor
   and the layer stride used to read as depth. */
GPU_TEST(texture_array, one_dimensional_array_round_trips)
{
    const GLsizei w = 8, layers = 5;
    GLubyte px[8 * 5 * 4], got[8 * 5 * 4];
    GLuint tex = 0;

    for (GLsizei l = 0; l < layers; l++)
        for (GLsizei x = 0; x < w; x++)
        {
            size_t i = ((size_t)l * w + x) * 4;
            px[i + 0] = (GLubyte)(0x40 + l);
            px[i + 1] = (GLubyte)x;
            px[i + 2] = 0x11;
            px[i + 3] = 0xFF;
        }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_1D_ARRAY, tex);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_RGBA8, w, layers, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);

    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_1D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_MSG(diff_bytes(px, got, sizeof px) == 0,
              "1D array round trip: %d of %zu bytes differ",
              diff_bytes(px, got, sizeof px), sizeof px);

    glDeleteTextures(1, &tex);
}

/* A rectangle texture is a 2D texture with unnormalised coordinates. Metal has
   no separate type for it, and the target switch had no case, so every one of
   them fell through to the renderer's emergency gradient. */
GPU_TEST(texture_array, rectangle_round_trips)
{
    const GLsizei w = 7, h = 5;
    GLubyte px[7 * 5 * 4], got[7 * 5 * 4];
    GLuint tex = 0;

    for (GLsizei y = 0; y < h; y++)
        for (GLsizei x = 0; x < w; x++)
        {
            size_t i = ((size_t)y * w + x) * 4;
            px[i + 0] = (GLubyte)(0x30 + y * w + x);
            px[i + 1] = (GLubyte)y;
            px[i + 2] = (GLubyte)x;
            px[i + 3] = 0xFF;
        }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_MSG(diff_bytes(px, got, sizeof px) == 0,
              "rectangle round trip: %d of %zu bytes differ",
              diff_bytes(px, got, sizeof px), sizeof px);

    glDeleteTextures(1, &tex);
}

/* An application may define only the first few levels of a chain and set
   MAX_LEVEL to match. Demanding the whole chain threw the texture away. */
GPU_TEST(texture_array, partial_mip_chain_round_trips)
{
    GLubyte l0[8 * 8 * 4], l1[4 * 4 * 4], got0[8 * 8 * 4], got1[4 * 4 * 4];
    GLuint tex = 0;

    for (int i = 0; i < 8 * 8; i++)
    {
        l0[i * 4 + 0] = 0x11; l0[i * 4 + 1] = (GLubyte)i;
        l0[i * 4 + 2] = 0x00; l0[i * 4 + 3] = 0xFF;
    }
    for (int i = 0; i < 4 * 4; i++)
    {
        l1[i * 4 + 0] = 0x22; l1[i * 4 + 1] = (GLubyte)i;
        l1[i * 4 + 2] = 0x01; l1[i * 4 + 3] = 0xFF;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    mgl_drain_errors();

    /* two of the four levels an 8x8 chain could hold */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, l0);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, l1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(got0, 0xAB, sizeof got0);
    memset(got1, 0xAB, sizeof got1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got0);
    glGetTexImage(GL_TEXTURE_2D, 1, GL_RGBA, GL_UNSIGNED_BYTE, got1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_MSG(diff_bytes(l0, got0, sizeof l0) == 0,
              "level 0: %d of %zu bytes differ", diff_bytes(l0, got0, sizeof l0), sizeof l0);
    CHECK_MSG(diff_bytes(l1, got1, sizeof l1) == 0,
              "level 1: %d of %zu bytes differ", diff_bytes(l1, got1, sizeof l1), sizeof l1);

    glDeleteTextures(1, &tex);
}

/* A copy into one level must not touch another. */
GPU_TEST(texture_array, copy_image_stays_on_its_level)
{
    GLubyte a0[8 * 8 * 4], a1[4 * 4 * 4], b0[8 * 8 * 4], b1[4 * 4 * 4], got[8 * 8 * 4];
    GLuint src = 0, dst = 0;

    memset(a0, 0x11, sizeof a0);
    memset(a1, 0x22, sizeof a1);
    memset(b0, 0x88, sizeof b0);
    memset(b1, 0x99, sizeof b1);

    glGenTextures(1, &src);
    glBindTexture(GL_TEXTURE_2D, src);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, a0);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, a1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    glGenTextures(1, &dst);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, b0);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, b1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    mgl_drain_errors();

    glCopyImageSubData(src, GL_TEXTURE_2D, 1, 0, 0, 0,
                       dst, GL_TEXTURE_2D, 1, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glBindTexture(GL_TEXTURE_2D, dst);

    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 1, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_INT(0x22, got[0]);

    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_INT(0x88, got[0]);

    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
}
