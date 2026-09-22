/*
 * test_texture_storage.c
 * MGL
 *
 * Immutable and mutable texture storage: 1D/3D images, DSA texture creation
 * and binding, texture views, texture buffers, and image unit binding.
 */

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
    CHECK_EQ_INT(dst[16], 128);    // untouched (offset 4 pixels)
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
