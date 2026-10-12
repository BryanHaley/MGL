/*
 * test_texture_storage.c
 * MGL
 *
 * Immutable and mutable texture storage: 1D/3D images, DSA texture creation
 * and binding, texture views, texture buffers, and image unit binding.
 */

#include <string.h>
#include <stdint.h>
#include "mgl_test.h"
#include "harness.h"

/* ---------- DSA texture creation ---------- */

GPU_TEST(texture_storage, create_textures_makes_objects)
{
    GLuint t[3] = { 0 };

    glCreateTextures(GL_TEXTURE_2D, 3, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(t[0] != 0 && t[1] != 0 && t[2] != 0);
    CHECK(t[0] != t[1] && t[1] != t[2] && t[0] != t[2]);

    CHECK_EQ_INT(glIsTexture(t[0]), GL_TRUE);
    CHECK_EQ_INT(glIsTexture(t[1]), GL_TRUE);
    CHECK_EQ_INT(glIsTexture(t[2]), GL_TRUE);

    glDeleteTextures(3, t);
    CHECK_EQ_INT(glIsTexture(t[0]), GL_FALSE);
}

GPU_TEST(texture_storage, create_textures_rejects_bad_target)
{
    GLuint t = 0;

    glCreateTextures(0x9999, 1, &t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glCreateTextures(GL_TEXTURE_2D, -1, &t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(texture_storage, create_textures_creates_1D_and_3D)
{
    GLuint t1d = 0, t3d = 0;

    glCreateTextures(GL_TEXTURE_1D, 1, &t1d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(glIsTexture(t1d), GL_TRUE);

    glCreateTextures(GL_TEXTURE_3D, 1, &t3d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(glIsTexture(t3d), GL_TRUE);

    glDeleteTextures(1, &t1d);
    glDeleteTextures(1, &t3d);
}

/* ---------- immutable 1D storage ---------- */

GPU_TEST(texture_storage, tex_storage_1D_round_trips)
{
    GLuint t = 0;
    GLint w = 0, fmt = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_1D, t);

    glTexStorage1D(GL_TEXTURE_1D, 1, GL_RGBA8, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_WIDTH, &w);
    CHECK_EQ_INT(w, 64);

    glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
    CHECK_EQ_INT(fmt, GL_RGBA8);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, tex_storage_1D_validates)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_1D, t);

    // levels must be > 0
    glTexStorage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // width must be > 0
    glTexStorage1D(GL_TEXTURE_1D, 1, GL_RGBA8, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // bad target
    glTexStorage1D(GL_TEXTURE_2D, 1, GL_RGBA8, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, texture_storage_1D_dsa_round_trips)
{
    GLuint t = 0;
    GLint w = 0, fmt = 0;

    glCreateTextures(GL_TEXTURE_1D, 1, &t);

    glTextureStorage1D(t, 1, GL_RGBA8, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureLevelParameteriv(t, 0, GL_TEXTURE_WIDTH, &w);
    CHECK_EQ_INT(w, 64);

    glGetTextureLevelParameteriv(t, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
    CHECK_EQ_INT(fmt, GL_RGBA8);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, texture_storage_1D_dsa_validates)
{
    GLuint t = 0;

    glCreateTextures(GL_TEXTURE_1D, 1, &t);

    glTextureStorage1D(t, 0, GL_RGBA8, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glTextureStorage1D(t, 1, GL_RGBA8, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glTextureStorage1D(999999, 1, GL_RGBA8, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &t);
}

/* ---------- immutable 3D storage ---------- */

GPU_TEST(texture_storage, tex_storage_3D_round_trips)
{
    GLuint t = 0;
    GLint w = 0, h = 0, d = 0, fmt = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);

    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 16, 8, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_WIDTH, &w);
    CHECK_EQ_INT(w, 16);
    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_HEIGHT, &h);
    CHECK_EQ_INT(h, 8);
    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_DEPTH, &d);
    CHECK_EQ_INT(d, 4);
    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
    CHECK_EQ_INT(fmt, GL_RGBA8);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, texture_storage_3D_dsa)
{
    GLuint t = 0;
    GLint d = 0;

    glCreateTextures(GL_TEXTURE_3D, 1, &t);

    glTextureStorage3D(t, 1, GL_RGBA8, 16, 8, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureLevelParameteriv(t, 0, GL_TEXTURE_DEPTH, &d);
    CHECK_EQ_INT(d, 4);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, tex_storage_3D_validates)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);

    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 0, 8, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glTexStorage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 16, 8, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glTexStorage3D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 8, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &t);
}

/* ---------- mutable 1D image via TexImage1D ---------- */

GPU_TEST(texture_storage, tex_image_1D_upload_and_readback)
{
    GLuint t = 0;
    GLint w = 0, fmt = 0;
    unsigned char src[64 * 4];
    unsigned char dst[64 * 4];

    for (int i = 0; i < 64; i++) {
        src[i * 4 + 0] = (unsigned char)i;
        src[i * 4 + 1] = (unsigned char)(255 - i);
        src[i * 4 + 2] = 128;
        src[i * 4 + 3] = 255;
    }

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_1D, t);

    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_WIDTH, &w);
    CHECK_EQ_INT(w, 64);
    glGetTexLevelParameteriv(GL_TEXTURE_1D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
    CHECK_EQ_INT(fmt, GL_RGBA8);

    glGetTexImage(GL_TEXTURE_1D, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(dst[0], 0);
    CHECK_EQ_INT(dst[4], 1);
    CHECK_EQ_INT(dst[1], 255);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, tex_image_1D_validates)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_1D, t);

    // border must be 0
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 64, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // bad target
    glTexImage1D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // negative level
    glTexImage1D(GL_TEXTURE_1D, -1, GL_RGBA8, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &t);
}

/* ---------- mutable 3D image via TexImage3D ---------- */

GPU_TEST(texture_storage, tex_image_3D_upload_and_readback)
{
    GLuint t = 0;
    GLint w = 0, h = 0, d = 0;
    unsigned char src[4 * 4 * 4 * 4];

    memset(src, 0, sizeof src);
    // set one pixel in the middle
    src[2 * 4 * 4 + 2 * 4 + 0] = 42;  // slice 2, row 2, col 0
    src[2 * 4 * 4 + 2 * 4 + 1] = 43;
    src[2 * 4 * 4 + 2 * 4 + 2] = 44;
    src[2 * 4 * 4 + 2 * 4 + 3] = 255;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_WIDTH, &w);
    CHECK_EQ_INT(w, 4);
    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_HEIGHT, &h);
    CHECK_EQ_INT(h, 4);
    glGetTexLevelParameteriv(GL_TEXTURE_3D, 0, GL_TEXTURE_DEPTH, &d);
    CHECK_EQ_INT(d, 4);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, tex_image_3D_validates)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glTexImage3D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &t);
}

/* ---------- sub-image updates ---------- */

GPU_TEST(texture_storage, tex_sub_image_1D_modifies_pixels)
{
    GLuint t = 0;
    unsigned char src[16 * 4];
    unsigned char patch[4 * 4];
    unsigned char dst[16 * 4];

    memset(src, 128, sizeof src);
    memset(patch, 0, sizeof patch);
    patch[0] = 77; patch[1] = 88; patch[2] = 99; patch[3] = 255;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_1D, t);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);

    glTexSubImage1D(GL_TEXTURE_1D, 0, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, patch);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexImage(GL_TEXTURE_1D, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    CHECK_EQ_INT(dst[0], 128);     // untouched
    CHECK_EQ_INT(dst[12], 128);    // untouched (pixel 3)
    CHECK_EQ_INT(dst[4 * 4 + 0], 77);  // patched pixel
    CHECK_EQ_INT(dst[4 * 4 + 3], 255);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, texture_sub_image_1D_dsa)
{
    GLuint t = 0;
    unsigned char src[16 * 4];
    unsigned char dst[16 * 4];

    memset(src, 64, sizeof src);

    glCreateTextures(GL_TEXTURE_1D, 1, &t);
    glTextureStorage1D(t, 1, GL_RGBA8, 16);
    glTextureSubImage1D(t, 0, 0, 16, GL_RGBA, GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // check via target-based readback
    glBindTexture(GL_TEXTURE_1D, t);
    glGetTexImage(GL_TEXTURE_1D, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    CHECK_EQ_INT(dst[0], 64);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, texture_sub_image_2D_dsa)
{
    GLuint t = 0;
    unsigned char src[4 * 4 * 4];
    unsigned char dst[4 * 4 * 4];

    memset(src, 33, sizeof src);

    glCreateTextures(GL_TEXTURE_2D, 1, &t);
    glTextureStorage2D(t, 1, GL_RGBA8, 4, 4);
    glTextureSubImage2D(t, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindTexture(GL_TEXTURE_2D, t);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    CHECK_EQ_INT(dst[0], 33);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, tex_sub_image_3D_and_dsa)
{
    GLuint t = 0;
    unsigned char src[4 * 4 * 4 * 4];
    unsigned char patch[2 * 2 * 2 * 4];

    memset(src, 100, sizeof src);
    memset(patch, 200, sizeof patch);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);

    // target-based sub-image
    glTexSubImage3D(GL_TEXTURE_3D, 0, 1, 1, 1, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, patch);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // DSA sub-image on a different texture
    GLuint t2 = 0;
    glCreateTextures(GL_TEXTURE_3D, 1, &t2);
    glTextureStorage3D(t2, 1, GL_RGBA8, 4, 4, 4);
    glTextureSubImage3D(t2, 0, 0, 0, 0, 4, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
    glDeleteTextures(1, &t2);
}

GPU_TEST(texture_storage, sub_image_validates)
{
    GLuint t = 0;
    unsigned char data[16] = { 0 };

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_1D, t);

    // no image defined yet
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 4, GL_RGBA, GL_UNSIGNED_BYTE, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // offset + width exceeds texture bounds
    glTexSubImage1D(GL_TEXTURE_1D, 0, 14, 4, GL_RGBA, GL_UNSIGNED_BYTE, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &t);
}

/* ---------- image unit binding ---------- */

GPU_TEST(texture_storage, bind_image_texture_round_trip)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);

    glBindImageTexture(0, t, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, bind_image_texture_validates)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);

    // bad access
    glBindImageTexture(0, t, 0, GL_FALSE, 0, 0x9999, GL_RGBA8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // bad unit
    glBindImageTexture(9999, t, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // non-existent texture
    glBindImageTexture(0, 999999, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, bind_image_textures_batch)
{
    GLuint t[2] = { 0 };

    glGenTextures(2, t);
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    glBindImageTextures(0, 2, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindImageTextures(0, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(2, t);
}

/* ---------- batch texture binding ---------- */

GPU_TEST(texture_storage, bind_textures_batch)
{
    GLuint t[2] = { 0 };

    glGenTextures(2, t);
    glBindTexture(GL_TEXTURE_2D, t[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
    glBindTexture(GL_TEXTURE_2D, t[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    glBindTextures(0, 2, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // unbind with NULL
    glBindTextures(0, 2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(2, t);
}

GPU_TEST(texture_storage, bind_texture_unit)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    glBindTextureUnit(0, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // unbind
    glBindTextureUnit(0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}

/* ---------- multisample stubs ---------- */

GPU_TEST(texture_storage, tex_image_2D_multisample_stub)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, t);

    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 16, 16, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, tex_image_3D_multisample_stub)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, t);

    glTexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 4, GL_RGBA8, 16, 16, 4, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, tex_storage_2D_multisample_stub)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, t);

    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 16, 16, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, tex_storage_3D_multisample_stub)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, t);

    glTexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 4, GL_RGBA8, 16, 16, 4, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, texture_storage_2D_multisample_dsa)
{
    GLuint t = 0;

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &t);

    glTextureStorage2DMultisample(t, 4, GL_RGBA8, 16, 16, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, texture_storage_3D_multisample_dsa)
{
    GLuint t = 0;

    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 1, &t);

    glTextureStorage3DMultisample(t, 4, GL_RGBA8, 16, 16, 4, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t);
}

/* ---------- texture buffer stubs ---------- */

GPU_TEST(texture_storage, texture_buffer_stubs)
{
    GLuint tex = 0, buf = 0;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);

    glCreateTextures(GL_TEXTURE_BUFFER, 1, &tex);

    glTextureBuffer(tex, GL_RGBA8, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glTextureBufferRange(tex, GL_RGBA8, buf, 0, 128);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &buf);
    glDeleteTextures(1, &tex);
}

/* ---------- texture view stub ---------- */

GPU_TEST(texture_storage, texture_view_stub)
{
    GLuint src = 0, dst = 0;

    glCreateTextures(GL_TEXTURE_2D, 1, &src);
    glTextureStorage2D(src, 1, GL_RGBA8, 32, 32);

    glGenTextures(1, &dst);

    glTextureView(dst, GL_TEXTURE_2D, src, GL_RGBA8, 0, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
}

// glTextureStorage2D on a cube map allocated one face, so Metal refused the
// texture and every shadow map and sky box in a DSA engine went missing
GPU_TEST(texture_storage, dsa_cube_map_gets_six_faces)
{
    GLuint tex = 0;
    unsigned char face3[4 * 4 * 4], back[6 * 4 * 4 * 4];

    for (int i = 0; i < (int)sizeof(face3); i++)
        face3[i] = (unsigned char)(i * 3);

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA8, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glTextureSubImage3D(tex, 0, 0, 0, 3, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, face3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetTextureImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof(back), back);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(memcmp(back + 3 * sizeof(face3), face3, sizeof(face3)) == 0);

    glDeleteTextures(1, &tex);
}

/* Every layer of a 3D texture has to come back the way it went in. A copy into
   one layer must leave the others alone, which is what glCopyImageSubData's
   functional cases check and what this pins down without them. */
GPU_TEST(texture_storage, tex_image_3D_layers_round_trip)
{
    enum { W = 7, H = 7, D = 4 };
    GLuint t = 0;
    unsigned char src[W * H * D * 4];
    unsigned char dst[W * H * D * 4];

    for (int z = 0; z < D; z++)
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                int i = ((z * H + y) * W + x) * 4;

                src[i + 0] = (unsigned char)(x * 10);
                src[i + 1] = (unsigned char)(y * 10);
                src[i + 2] = (unsigned char)(z * 10 + 1);
                src[i + 3] = 255;
            }

    memset(dst, 0, sizeof dst);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_3D, t);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, W, H, D, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* the corner of every layer, which is where a wrong image stride shows */
    for (int z = 0; z < D; z++)
    {
        int i = ((z * H + (H - 1)) * W + (W - 1)) * 4;

        CHECK_EQ_INT(dst[i + 0], (W - 1) * 10);
        CHECK_EQ_INT(dst[i + 1], (H - 1) * 10);
        CHECK_EQ_INT(dst[i + 2], z * 10 + 1);
    }

    CHECK(memcmp(src, dst, sizeof src) == 0);

    glDeleteTextures(1, &t);
}

/* A copy into one layer of a 3D texture leaves every other layer as it was. */
GPU_TEST(texture_storage, copy_into_one_3D_layer_spares_the_rest)
{
    enum { W = 7, H = 7, D = 4 };
    GLuint src_tex = 0, dst_tex = 0;
    unsigned char fill[W * H * D * 4];
    unsigned char one[4] = { 9, 9, 9, 255 };
    unsigned char dst[W * H * D * 4];

    for (size_t i = 0; i < sizeof fill; i += 4)
    {
        fill[i + 0] = (unsigned char)(i / 4);
        fill[i + 1] = 7;
        fill[i + 2] = (unsigned char)(i / (4 * W * H) + 1);
        fill[i + 3] = 255;
    }

    glGenTextures(1, &dst_tex);
    glBindTexture(GL_TEXTURE_3D, dst_tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, W, H, D, 0, GL_RGBA, GL_UNSIGNED_BYTE, fill);
    /* one level only, so the texture counts as complete to copy from */
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);

    glGenTextures(1, &src_tex);
    glBindTexture(GL_TEXTURE_3D, src_tex);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, one);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glCopyImageSubData(src_tex, GL_TEXTURE_3D, 0, 0, 0, 0,
                       dst_tex, GL_TEXTURE_3D, 0, 6, 6, 0, 1, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(dst, 0, sizeof dst);
    glBindTexture(GL_TEXTURE_3D, dst_tex);
    glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* the one texel that was copied */
    {
        int i = ((0 * H + 6) * W + 6) * 4;
        CHECK_EQ_INT(dst[i + 0], 9);
    }

    /* and every layer above it, untouched */
    for (int z = 1; z < D; z++)
    {
        int i = ((z * H + 6) * W + 6) * 4;

        CHECK_EQ_INT(dst[i + 1], 7);
        CHECK_EQ_INT(dst[i + 2], z + 1);
    }

    glDeleteTextures(1, &src_tex);
    glDeleteTextures(1, &dst_tex);
}

// glTexSubImage changes only the texels it names. It used to make the Metal
// texture again from the CPU copy, which wiped whatever had been drawn into it.
GPU_TEST(texture_storage, sub_image_keeps_what_was_drawn)
{
    static const GLubyte red[4] = { 255, 0, 0, 255 };
    GLuint tex, fb;
    GLubyte px[4 * 4 * 4] = { 0 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, 4, 4);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, red);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG(px[(1 * 4 + 1) * 4] == 255 && px[(1 * 4 + 1) * 4 + 1] == 0, "the new texel is %d,%d",
              px[20], px[21]);
    CHECK_MSG(px[1] == 255 && px[0] == 0, "a drawn texel is now %d,%d,%d", px[0], px[1], px[2]);

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// A mip chain given an unsized format at every level is one format. Level 0
// stored GL_RGB as GL_RGB8, so every later level looked different and was
// refused.
GPU_TEST(texture_storage, unsized_levels_build_one_chain)
{
    GLuint tex;
    GLubyte px[16 * 16 * 3] = { 0 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    for (GLint l = 0, s = 16; s > 0; l++, s >>= 1)
        glTexImage2D(GL_TEXTURE_2D, l, GL_RGB, s, s, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
}

// One texel gathered or sampled by a compute shader, written to a buffer
static int gatherProbe(const char *decl, const char *expr, GLuint tex, GLenum target, float out[4])
{
    char src[1024], log[2048] = { 0 };
    GLuint prog, ssbo;

    snprintf(src, sizeof src,
             "#version 460 core\nlayout(local_size_x = 1) in;\n"
             "layout(std430, binding = 0) buffer Out { vec4 r; };\n%s\n"
             "void main() { r = %s; }\n", decl, expr);
    prog = mgl_build_compute_program(src, log, sizeof log);
    if (!prog)
    {
        fprintf(stderr, "%s\n", log);
        return 0;
    }

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(target, tex);
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "t"), 0);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 16, out);
    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
    return 1;
}

// A gather that names its component on a rectangle texture. Metal wants an
// offset in front of the component, and rectangles were not given one, so
// the shader did not build.
GPU_TEST(texture_storage, rectangle_gathers_a_chosen_component)
{
    GLuint tex;
    GLfloat px[2 * 2 * 4] = { 0.1f, 0.2f, 0.3f, 0.4f,  0.1f, 0.2f, 0.3f, 0.4f,
                              0.1f, 0.2f, 0.3f, 0.4f,  0.1f, 0.2f, 0.3f, 0.4f };
    float r[4] = { -1, -1, -1, -1 };

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA32F, 2, 2, 0, GL_RGBA, GL_FLOAT, px);
    CHECK(gatherProbe("uniform sampler2DRect t;", "textureGather(t, vec2(1.0), 1)", tex, GL_TEXTURE_RECTANGLE, r));
    CHECK_NEAR(r[0], 0.2f, 1e-5f);
    CHECK_NEAR(r[3], 0.2f, 1e-5f);
    CHECK(gatherProbe("uniform sampler2DRect t;", "textureGatherOffset(t, vec2(1.0), ivec2(0), 2)", tex,
                      GL_TEXTURE_RECTANGLE, r));
    CHECK_NEAR(r[0], 0.3f, 1e-5f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDeleteTextures(1, &tex);
}

// A mip level defined at the wrong size is legal and leaves the texture
// incomplete, which samples as (0, 0, 0, 1). The level was refused, so the
// texture stayed complete and sampled its data.
GPU_TEST(texture_storage, a_wrong_size_level_makes_the_texture_incomplete)
{
    GLuint tex;
    static GLfloat px[4 * 4 * 4];
    float r[4] = { -1, -1, -1, -1 };

    for (int i = 0; i < 4 * 4 * 4; i++)
        px[i] = 0.5f;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGBA, GL_FLOAT, px);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 2, 2, 0, GL_RGBA, GL_FLOAT, px);
    glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, px);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK(gatherProbe("uniform sampler2D t;", "textureLod(t, vec2(0.5), 0.0)", tex, GL_TEXTURE_2D, r));
    CHECK_MSG(r[0] == 0.0f && r[3] == 1.0f, "sampled %g %g %g %g, want 0 0 0 1", r[0], r[1], r[2], r[3]);

    // the right size makes it complete again
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, 2, 2, 0, GL_RGBA, GL_FLOAT, px);
    CHECK(gatherProbe("uniform sampler2D t;", "textureLod(t, vec2(0.5), 0.0)", tex, GL_TEXTURE_2D, r));
    CHECK_NEAR(r[0], 0.5f, 1e-5f);
    glDeleteTextures(1, &tex);
}

// glGetTextureSubImage counts a 1D array's layers as rows, and reads every
// slice its depth asks for
GPU_TEST(texture_storage, sub_image_reads_layers_and_slices)
{
    GLuint t1 = 0, t2 = 0;
    GLubyte src[3 * 4 * 4], got[2 * 2 * 4 * 2];

    for (int i = 0; i < (int)sizeof src; i++)
        src[i] = (GLubyte)i;

    glCreateTextures(GL_TEXTURE_1D_ARRAY, 1, &t1);
    glTextureStorage2D(t1, 1, GL_RGBA8, 4, 3);
    glTextureSubImage2D(t1, 0, 0, 0, 4, 3, GL_RGBA, GL_UNSIGNED_BYTE, src);

    memset(got, 0xEE, sizeof got);
    glGetTextureSubImage(t1, 0, 1, 1, 0, 2, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    // layer 1 texel 1 starts at byte (1 * 4 + 1) * 4, layer 2 texel 1 at (2 * 4 + 1) * 4
    CHECK_MSG(got[0] == 20 && got[8] == 36, "1D array: first bytes %d and %d, want 20 and 36", got[0], got[8]);

    // a 2D array, two layers of a 2 x 2 corner
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &t2);
    glTextureStorage3D(t2, 1, GL_RGBA8, 2, 2, 3);
    glTextureSubImage3D(t2, 0, 0, 0, 0, 2, 2, 3, GL_RGBA, GL_UNSIGNED_BYTE, src);

    memset(got, 0xEE, sizeof got);
    glGetTextureSubImage(t2, 0, 0, 0, 1, 2, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(got[0] == 16 && got[16] == 32, "2D array: layer starts %d and %d, want 16 and 32", got[0], got[16]);

    glDeleteTextures(1, &t1);
    glDeleteTextures(1, &t2);
}

// A compressed level keeps each layer as rows of blocks, so a sub-image read
// or write of layer 1 starts right after layer 0's blocks
GPU_TEST(texture_storage, compressed_layers_line_up)
{
    GLuint t = 0;
    GLubyte all[2 * 2 * 2 * 16], got[2 * 2 * 16], mine[2 * 2 * 16];

    for (int i = 0; i < (int)sizeof all; i++)
        all[i] = (GLubyte)(i * 7 + 1);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_ARRAY, t);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 8, 8, 2, 0, sizeof all, all);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetCompressedTextureSubImage(t, 0, 0, 0, 1, 8, 8, 1, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(memcmp(got, all + sizeof got, sizeof got) == 0, "layer 1 read back as layer %s",
              memcmp(got, all, sizeof got) == 0 ? "0" : "garbage");

    for (int i = 0; i < (int)sizeof mine; i++)
        mine[i] = (GLubyte)(255 - i);

    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 8, 8, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                              sizeof mine, mine);
    glGetCompressedTextureSubImage(t, 0, 0, 0, 1, 8, 8, 1, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(memcmp(got, mine, sizeof got) == 0, "layer 1 written and read back differ");

    glGetCompressedTextureSubImage(t, 0, 0, 0, 0, 8, 8, 1, sizeof got, got);
    CHECK_MSG(memcmp(got, all, sizeof got) == 0, "writing layer 1 changed layer 0");

    glDeleteTextures(1, &t);
}

// Immutable compressed storage filled whole with glCompressedTexSubImage
GPU_TEST(texture_storage, compressed_storage_takes_a_full_sub_image)
{
    static GLubyte blocks[16 * 16 * 16];
    GLuint t2 = 0, ta = 0;

    glGenTextures(1, &t2);
    glBindTexture(GL_TEXTURE_2D, t2);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA8_ETC2_EAC, 64, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_COMPRESSED_RGBA8_ETC2_EAC, sizeof blocks, blocks);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenTextures(1, &ta);
    glBindTexture(GL_TEXTURE_2D_ARRAY, ta);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_COMPRESSED_RGBA8_ETC2_EAC, 32, 32, 4);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 32, 32, 4, GL_COMPRESSED_RGBA8_ETC2_EAC,
                              sizeof blocks, blocks);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &t2);
    glDeleteTextures(1, &ta);
}

// A cube map array is layer-faces in a row: z counts faces, six to a layer.
// Each one written on its own has to read back in its own place.
GPU_TEST(texture_storage, cube_array_layer_faces_round_trip)
{
    enum { W = 4, LF = 12 };
    GLubyte src[W * W * 4 * LF], got[W * W * 4 * LF];
    GLuint t = 0, pbo = 0;

    for (int z = 0; z < LF; z++)
        for (int i = 0; i < W * W * 4; i++)
            src[z * W * W * 4 + i] = (GLubyte)(z * 16 + (i & 15));

    for (int use_pbo = 0; use_pbo < 2; use_pbo++)
    {
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, t);
        glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_RGBA8, W, W, LF);

        if (use_pbo)
        {
            glGenBuffers(1, &pbo);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, sizeof src, src, GL_STATIC_DRAW);
        }

        // first half as one call, the rest one layer-face at a time
        glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, W, W, 6, GL_RGBA, GL_UNSIGNED_BYTE,
                        use_pbo ? (const void *)0 : src);
        for (int z = 6; z < LF; z++)
            glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, z, W, W, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                            use_pbo ? (const void *)(uintptr_t)(z * W * W * 4) : src + z * W * W * 4);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        if (use_pbo)
        {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glDeleteBuffers(1, &pbo);
        }

        memset(got, 0, sizeof got);
        glGetTexImage(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        for (int z = 0; z < LF; z++)
            CHECK_MSG(got[z * W * W * 4] == src[z * W * W * 4] &&
                      got[z * W * W * 4 + W * W * 4 - 1] == src[z * W * W * 4 + W * W * 4 - 1],
                      "%s: layer-face %d reads %d, want %d", use_pbo ? "pbo" : "client",
                      z, got[z * W * W * 4], src[z * W * W * 4]);

        glDeleteTextures(1, &t);
    }
}

// Each layer-face of a cube map array reads back through a framebuffer,
// before and after mipmaps are made from it
GPU_TEST(texture_storage, cube_array_layer_faces_read_through_a_framebuffer)
{
    enum { W = 8, LF = 12 };
    static GLubyte src[W * W * 4 * LF];
    GLubyte got[W * W * 4];
    GLuint t = 0, fbo = 0;

    for (int z = 0; z < LF; z++)
        for (int i = 0; i < W * W * 4; i++)
            src[z * W * W * 4 + i] = (GLubyte)(z * 16 + (i & 7));

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, t);
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 4, GL_RGBA8, W, W, LF);
    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, W, W, LF, GL_RGBA, GL_UNSIGNED_BYTE, src);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);

    for (int pass = 0; pass < 2; pass++)
    {
        if (pass)
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP_ARRAY);

        for (int z = 0; z < LF; z++)
        {
            glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, t, 0, z);
            memset(got, 0, sizeof got);
            glReadPixels(0, 0, W, W, GL_RGBA, GL_UNSIGNED_BYTE, got);
            CHECK_MSG(memcmp(got, src + z * W * W * 4, sizeof got) == 0,
                      "%s mipmaps: layer-face %d reads %d, want %d", pass ? "after" : "before",
                      z, got[0], src[z * W * W * 4]);
        }
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &t);
}

GPU_TEST(texture_storage, array_layers_survive_generated_mipmaps)
{
    enum { W = 8, LF = 12 };
    static GLubyte src[W * W * 4 * LF];
    GLubyte got[W * W * 4];
    GLuint t = 0, fbo = 0;

    for (int z = 0; z < LF; z++)
        for (int i = 0; i < W * W * 4; i++)
            src[z * W * W * 4 + i] = (GLubyte)(z * 16 + (i & 7));

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D_ARRAY, t);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 4, GL_RGBA8, W, W, LF);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, W, W, LF, GL_RGBA, GL_UNSIGNED_BYTE, src);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);

    for (int pass = 0; pass < 2; pass++)
    {
        if (pass)
        {
            static GLubyte all[W * W * 4 * LF];
            glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
            glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, all);
            CHECK_MSG(memcmp(all, src, sizeof all) == 0, "GetTexImage after mipmaps differs (first %d %d)", all[0], all[W * W * 4]);
        }

        for (int z = 0; z < LF; z++)
        {
            glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, t, 0, z);
            memset(got, 0, sizeof got);
            glReadPixels(0, 0, W, W, GL_RGBA, GL_UNSIGNED_BYTE, got);
            CHECK_MSG(memcmp(got, src + z * W * W * 4, sizeof got) == 0,
                      "%s mipmaps: layer-face %d reads %d, want %d", pass ? "after" : "before",
                      z, got[0], src[z * W * W * 4]);
        }
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &t);
}
