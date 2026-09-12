/*
 * test_texture_copy.c
 * MGL
 *
 * Texture copy, clear, invalidate, mipmap generation and compressed uploads.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- glClearTexImage / glClearTexSubImage ---------- */

GPU_TEST(texture_copy, clear_tex_image_rejects_bad_texture)
{
    // zero is never a valid texture
    glClearTexImage(0, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // non-existent name
    glClearTexImage(9999, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(texture_copy, clear_tex_image_needs_defined_level)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    // level 0 has not been defined by TexImage or TexStorage
    glClearTexImage(t, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_copy, clear_tex_image_zeros_a_level)
{
    GLuint t = 0;
    GLsizei w = 4, h = 4;
    unsigned char *px = NULL;
    unsigned char zero[4] = { 0 };

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Fill with 0xFF first via TexSubImage
    unsigned char fill[4 * 4 * 4];
    memset(fill, 0xFF, sizeof fill);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, fill);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Clear to zero (data == NULL)
    glClearTexImage(t, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Read back and verify
    px = (unsigned char *)malloc(w * h * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(memcmp(px, zero, 4) == 0);

    free(px);
    glDeleteTextures(1, &t);
}

GPU_TEST(texture_copy, clear_tex_image_fills_constant)
{
    GLuint t = 0;
    GLsizei w = 4, h = 4;
    unsigned char *px = NULL;
    unsigned char want[4] = { 64, 128, 192, 255 };

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClearTexImage(t, 0, GL_RGBA, GL_UNSIGNED_BYTE, want);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = (unsigned char *)malloc(w * h * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < w * h; i++)
        CHECK(memcmp(px + i * 4, want, 4) == 0);

    free(px);
    glDeleteTextures(1, &t);
}

GPU_TEST(texture_copy, clear_tex_sub_image_rejects_bad_texture)
{
    glClearTexSubImage(0, 0, 0, 0, 0, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(texture_copy, clear_tex_sub_image_region)
{
    GLuint t = 0;
    GLsizei w = 8, h = 8;
    unsigned char *px = NULL;
    unsigned char want[4] = { 0, 0, 0, 0 };

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, w, h);

    unsigned char fill[8 * 8 * 4];
    memset(fill, 0xFF, sizeof fill);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, fill);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Clear a 4x4 region in the centre
    glClearTexSubImage(t, 0, 2, 2, 0, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = (unsigned char *)malloc(w * h * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Top-left corner should still be 0xFF
    CHECK(memcmp(px, fill, 4) == 0);

    // Pixel at (3,3) should be zeroed
    unsigned char *p = px + (3 * w + 3) * 4;
    CHECK(memcmp(p, want, 4) == 0);

    free(px);
    glDeleteTextures(1, &t);
}

/* ---------- glCompressedTexImage{1D,2D,3D} ---------- */

GPU_TEST(texture_copy, compressed_tex_image_rejects_bad_border)
{
    // border must be 0
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 4, 1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glCompressedTexImage1D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 4, 1, 1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(texture_copy, compressed_tex_image_rejects_generic_format)
{
    // Generic compressed formats are not accepted
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB,
                           4, 4, 0, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(texture_copy, compressed_tex_image_3d_rejects_bad_target)
{
    glCompressedTexImage3D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 4, 1, 0, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(texture_copy, compressed_tex_image_1d_rejects_bad_target)
{
    glCompressedTexImage1D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 0, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- glCompressedTexSubImage{1D,2D,3D} ---------- */

GPU_TEST(texture_copy, compressed_tex_sub_image_rejects_bad_target)
{
    glCompressedTexSubImage2D(0x9999, 0, 0, 0, 4, 4,
                              GL_COMPRESSED_RED_RGTC1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glCompressedTexSubImage1D(0x9999, 0, 0, 4,
                              GL_COMPRESSED_RED_RGTC1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glCompressedTexSubImage3D(0x9999, 0, 0, 0, 0, 4, 4, 1,
                              GL_COMPRESSED_RED_RGTC1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- glCompressedTextureSubImage{1D,2D,3D} (DSA) ---------- */

GPU_TEST(texture_copy, compressed_texture_sub_image_rejects_bad_texture)
{
    glCompressedTextureSubImage2D(9999, 0, 0, 0, 4, 4,
                                  GL_COMPRESSED_RED_RGTC1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glCompressedTextureSubImage1D(9999, 0, 0, 4,
                                  GL_COMPRESSED_RED_RGTC1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glCompressedTextureSubImage3D(9999, 0, 0, 0, 0, 4, 4, 1,
                                  GL_COMPRESSED_RED_RGTC1, 16, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- glCopyTexImage1D ---------- */

GPU_TEST(texture_copy, copy_tex_image_1d_rejects_bad_target)
{
    glCopyTexImage1D(0x9999, 0, GL_RGBA8, 0, 0, 4, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // border must be 0
    glCopyTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 0, 0, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- glCopyTexImage2D ---------- */

GPU_TEST(texture_copy, copy_tex_image_2d_rejects_bad_target)
{
    glCopyTexImage2D(0x9999, 0, GL_RGBA8, 0, 0, 4, 4, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // border must be 0
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(texture_copy, copy_tex_image_2d_copies_from_framebuffer)
{
    MGLTestTarget tt;
    GLuint t = 0;

    if (!mgl_target_create(&tt, 8, 8, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&tt);

    // Clear the framebuffer to a known colour
    glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    // Copy from framebuffer to texture
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 4, 4, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // glGetTexImage returns the whole level, not one texel
    unsigned char px[4 * 4 * 4] = { 0 };
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    // 0.25 -> 64, 0.5 -> 128, 0.75 -> 191, 1.0 -> 255
    CHECK_NEAR((int)px[0], 64, 2);
    CHECK_NEAR((int)px[1], 128, 2);

    glDeleteTextures(1, &t);
    mgl_target_destroy(&tt);
}

/* ---------- glCopyTexSubImage{1D,2D,3D} ---------- */

GPU_TEST(texture_copy, copy_tex_sub_image_1d_rejects_bad_target)
{
    glCopyTexSubImage1D(0x9999, 0, 0, 0, 0, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(texture_copy, copy_tex_sub_image_2d_rejects_bad_target)
{
    glCopyTexSubImage2D(0x9999, 0, 0, 0, 0, 0, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(texture_copy, copy_tex_sub_image_3d_rejects_bad_target)
{
    glCopyTexSubImage3D(0x9999, 0, 0, 0, 0, 0, 0, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(texture_copy, copy_tex_sub_image_2d_copies_region)
{
    MGLTestTarget tt;
    GLuint t = 0;
    unsigned char *px = NULL;

    if (!mgl_target_create(&tt, 8, 8, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&tt);

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);

    // Copy from framebuffer into texture
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = (unsigned char *)malloc(4 * 4 * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Should be green (0, 255, 0, 255) from the clear
    CHECK_NEAR((int)px[0], 0, 2);
    CHECK_NEAR((int)px[1], 255, 2);
    CHECK_NEAR((int)px[2], 0, 2);

    free(px);
    glDeleteTextures(1, &t);
    mgl_target_destroy(&tt);
}

/* ---------- glCopyTextureSubImage{1D,2D,3D} (DSA) ---------- */

GPU_TEST(texture_copy, copy_texture_sub_image_rejects_bad_texture)
{
    glCopyTextureSubImage1D(9999, 0, 0, 0, 0, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glCopyTextureSubImage2D(9999, 0, 0, 0, 0, 0, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glCopyTextureSubImage3D(9999, 0, 0, 0, 0, 0, 0, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(texture_copy, copy_texture_sub_image_2d_copies_region)
{
    MGLTestTarget tt;
    GLuint t = 0;
    unsigned char *px = NULL;

    if (!mgl_target_create(&tt, 8, 8, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&tt);

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);

    glCopyTextureSubImage2D(t, 0, 0, 0, 0, 0, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = (unsigned char *)malloc(4 * 4 * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_NEAR((int)px[0], 255, 2);
    CHECK_NEAR((int)px[1], 0, 2);
    CHECK_NEAR((int)px[2], 0, 2);

    free(px);
    glDeleteTextures(1, &t);
    mgl_target_destroy(&tt);
}

/* ---------- glGenerateTextureMipmap ---------- */

GPU_TEST(texture_copy, generate_texture_mipmap_rejects_bad_texture)
{
    glGenerateTextureMipmap(9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(texture_copy, generate_texture_mipmap_needs_level_zero)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    // No image defined yet
    glGenerateTextureMipmap(t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_copy, generate_texture_mipmap_creates_smaller_levels)
{
    GLuint t = 0;
    GLint w = 0, h = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 4, GL_RGBA8, 16, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Fill level 0
    unsigned char fill[16 * 16 * 4];
    memset(fill, 128, sizeof fill);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, fill);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenerateTextureMipmap(t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Level 1 should now exist (8x8)
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &w);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(w, 8);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_HEIGHT, &h);
    CHECK_EQ_INT(h, 8);

    glDeleteTextures(1, &t);
}

/* ---------- glInvalidateTexImage / glInvalidateTexSubImage ---------- */

GPU_TEST(texture_copy, invalidate_tex_image_rejects_bad_texture)
{
    glInvalidateTexImage(9999, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(texture_copy, invalidate_tex_sub_image_rejects_bad_texture)
{
    glInvalidateTexSubImage(9999, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(texture_copy, invalidate_tex_image_does_not_crash)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Invalidation is a hint; must not crash or error on a valid texture
    glInvalidateTexImage(t, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glInvalidateTexSubImage(t, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}
