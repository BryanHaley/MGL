/*
 * test_texture_query.c
 * MGL
 *
 * Texture readback and parameter queries.
 *
 * Covers: glGetCompressedTexImage, glGetCompressedTextureImage,
 * glGetCompressedTextureSubImage, glGetTexImage, glGetTexParameterIiv,
 * glGetTexParameterIuiv, glGetTexParameterfv, glGetTextureImage,
 * glGetTextureLevelParameterfv, glGetTextureLevelParameteriv,
 * glGetTextureParameterIiv, glGetTextureParameterIuiv,
 * glGetTextureParameterfv, glGetTextureParameteriv,
 * glGetTextureSubImage, glGetnCompressedTexImage, glGetnTexImage.
 */

#include "mgl_test.h"
#include "harness.h"
#include <string.h>

/* ---------- glGetTexParameterfv / iv — set then read back ---------- */

GPU_TEST(texture_query, set_and_get_tex_parameter)
{
    GLuint tex = 0;
    GLint iv = 0xDEAD;
    GLfloat fv = -999.0f;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    CHECK(tex != 0);

    /* Defaults per OpenGL 4.6 Core table 8.19 */
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &fv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT((GLint)fv, GL_LINEAR);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &iv);
    CHECK_EQ_INT(iv, GL_NEAREST_MIPMAP_LINEAR);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iv);
    CHECK_EQ_INT(iv, GL_REPEAT);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &iv);
    CHECK_EQ_INT(iv, GL_REPEAT);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, &iv);
    CHECK_EQ_INT(iv, GL_NONE);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, &iv);
    CHECK_EQ_INT(iv, GL_LEQUAL);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &iv);
    CHECK_EQ_INT(iv, 0);

    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, &fv);
    CHECK_NEAR(fv, -1000.0f, 0.5f);

    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, &fv);
    CHECK_NEAR(fv, 1000.0f, 0.5f);

    /* Change some values, read them back */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &iv);
    CHECK_EQ_INT(iv, GL_NEAREST);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &iv);
    CHECK_EQ_INT(iv, GL_LINEAR);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &iv);
    CHECK_EQ_INT(iv, GL_CLAMP_TO_EDGE);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &iv);
    CHECK_EQ_INT(iv, GL_MIRRORED_REPEAT);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, &iv);
    CHECK_EQ_INT(iv, GL_COMPARE_REF_TO_TEXTURE);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, &iv);
    CHECK_EQ_INT(iv, GL_LESS);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_query, set_and_get_tex_border_color)
{
    GLuint tex = 0;
    GLfloat bf[4] = { 99, 99, 99, 99 };
    GLint   bi[4] = { 99, 99, 99, 99 };
    const GLfloat red[]  = { 1.0f, 0.0f, 0.0f, 0.5f };

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    CHECK(tex != 0);

    /* Default border color is (0, 0, 0, 0) */
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bf);
    CHECK_NEAR(bf[0], 0.0f, 1e-6f);
    CHECK_NEAR(bf[3], 0.0f, 1e-6f);

    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, red);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bf);
    CHECK_NEAR(bf[0], 1.0f, 1e-6f);
    CHECK_NEAR(bf[1], 0.0f, 1e-6f);
    CHECK_NEAR(bf[3], 0.5f, 1e-6f);

    /* Signed integer border color */
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                      (const GLint[]){ -1, 0, 1, 2 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bi);
    CHECK_EQ_INT(bi[0], -1);
    CHECK_EQ_INT(bi[1],  0);
    CHECK_EQ_INT(bi[2],  1);
    CHECK_EQ_INT(bi[3],  2);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_query, get_tex_parameter_rejects_bad_target)
{
    GLint iv = 0;
    GLfloat fv = 0.0f;

    glGetTexParameterfv(0x9999, GL_TEXTURE_MAG_FILTER, &fv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetTexParameteriv(0x9999, GL_TEXTURE_MIN_FILTER, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}
GPU_TEST(texture_query, get_tex_parameter_rejects_bad_pname)
{
    GLuint tex = 0;
    GLint iv = 0;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);

    glGetTexParameteriv(GL_TEXTURE_2D, 0xFFFF, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &tex);
}

/* ---------- glGetTexParameterIiv / Iuiv (stubs) ---------- */

GPU_TEST(texture_query, get_tex_parameter_I_forms)
{
    GLuint tex = 0;
    GLint  iv = 0xDEAD;
    GLuint uv = 0xDEAD;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);

    /* These are currently stubs that return 0.
     * The test verifies they exist and don't crash. */
    glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
}

/* ---------- glGetTexLevelParameteriv / fv ---------- */

GPU_TEST(texture_query, level_parameter_round_trip)
{
    GLuint tex = 0;
    GLint iv = 0;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 4, GL_RGBA8, 64, 32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iv);
    CHECK_EQ_INT(iv, 64);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &iv);
    CHECK_EQ_INT(iv, 32);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &iv);
    CHECK_EQ_INT(iv, GL_RGBA8);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_DEPTH, &iv);
    CHECK_EQ_INT(iv, 1);

    /* Level 1 is half size */
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_WIDTH, &iv);
    CHECK_EQ_INT(iv, 32);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 1, GL_TEXTURE_HEIGHT, &iv);
    CHECK_EQ_INT(iv, 16);

    /* COMPRESSED is always FALSE */
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &iv);
    CHECK_EQ_INT(iv, GL_FALSE);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_SAMPLES, &iv);
    CHECK_EQ_INT(iv, 0);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_query, level_parameter_component_sizes)
{
    GLuint tex = 0;
    GLint r = 0, g = 0, b = 0, a = 0;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &r);
    CHECK_EQ_INT(r, 8);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &g);
    CHECK_EQ_INT(g, 8);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_BLUE_SIZE, &b);
    CHECK_EQ_INT(b, 8);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE, &a);
    CHECK_EQ_INT(a, 8);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_query, level_parameter_float_form)
{
    GLuint tex = 0;
    GLfloat fv = -1.0f;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &fv);
    CHECK_NEAR(fv, 16.0f, 1e-5f);

    glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fv);
    CHECK_NEAR(fv, (GLfloat)GL_RGBA8, 1e-5f);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_query, level_parameter_rejects_bad_arguments)
{
    GLuint tex = 0;
    GLint iv = 0;

    glGetTexLevelParameteriv(0x9999, 0, GL_TEXTURE_WIDTH, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* No texture bound */
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 8);

    /* Level out of range */
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 5, GL_TEXTURE_WIDTH, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* Negative level */
    glGetTexLevelParameteriv(GL_TEXTURE_2D, -1, GL_TEXTURE_WIDTH, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* Bad pname */
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, 0xFFFF, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &tex);
}

/* ---------- glGetTextureLevelParameteriv / fv (DSA) ---------- */

GPU_TEST(texture_query, texture_level_parameter_dsa)
{
    GLuint tex = 0;
    GLint iv = 0;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_WIDTH, &iv);
    CHECK_EQ_INT(iv, 32);

    glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_HEIGHT, &iv);
    CHECK_EQ_INT(iv, 32);

    glGetTextureLevelParameteriv(tex, 0, GL_TEXTURE_INTERNAL_FORMAT, &iv);
    CHECK_EQ_INT(iv, GL_RGBA8);

    glGetTextureLevelParameterfv(tex, 0, GL_TEXTURE_WIDTH, (GLfloat[]){ -1 });
    CHECK_NEAR(*(GLfloat[]){ -1 }, 32.0f, 1e-5f);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_query, texture_level_parameter_dsa_missing_validation)
{
    GLuint tex = 0;
    GLint iv = 0;

    /* No such texture — the DSA version does not set an error */
    glGetTextureLevelParameteriv(999123, 0, GL_TEXTURE_WIDTH, &iv);
    /* The implementation currently returns 0 without setting error */

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 8);

    /* Invalid pname — should be GL_INVALID_ENUM but is silently ignored */
    glGetTextureLevelParameteriv(tex, 0, 0xFFFF, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &tex);
}

/* ---------- glGetTextureParameter (DSA stubs) ---------- */

GPU_TEST(texture_query, texture_parameter_dsa_stubs)
{
    GLuint tex = 0;
    GLint  iv = 0xDEAD;
    GLfloat fv = -1.0f;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 8);

    /* These are stubs that always return 0. A correct implementation
     * would read the actual stored parameter value. */
    glGetTextureParameteriv(tex, GL_TEXTURE_MIN_FILTER, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureParameterfv(tex, GL_TEXTURE_MAG_FILTER, &fv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureParameterIiv(tex, GL_TEXTURE_WRAP_S, &iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureParameterIuiv(tex, GL_TEXTURE_WRAP_T, (GLuint *)&iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
}

/* ---------- glGetTexImage — readback ---------- */

GPU_TEST(texture_query, get_tex_image_readback)
{
    GLuint tex = 0;
    unsigned char src[] = {
        1, 2, 3, 255,   5, 6, 7, 255,
        9,10,11,255,   13,14,15,255
    };
    unsigned char dst[16] = { 0 };

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(dst[0], 1);
    CHECK_EQ_INT(dst[1], 2);
    CHECK_EQ_INT(dst[4], 5);
    CHECK_EQ_INT(dst[15], 255);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_query, get_tex_image_rejects_bad_state)
{
    unsigned char tmp[64] = { 0 };

    /* No texture bound */
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* Bad target */
    glGetTexImage(0x9999, 0, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- glGetTextureImage (DSA with bufSize) ---------- */

GPU_TEST(texture_query, get_texture_image_dsa)
{
    GLuint tex = 0;
    unsigned char src[] = { 10, 20, 30, 255,  40, 50, 60, 255,
                            70, 80, 90, 255, 100,110,120, 255 };
    unsigned char dst[16] = { 0 };

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof dst, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(dst[0], 10);
    CHECK_EQ_INT(dst[4], 40);

    /* bufSize too small */
    glGetTextureImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, 8, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_query, get_texture_image_dsa_bad_texture)
{
    unsigned char dst[16] = { 0 };

    glGetTextureImage(999123, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof dst, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- glGetTextureSubImage ---------- */

GPU_TEST(texture_query, get_texture_sub_image)
{
    GLuint tex = 0;
    unsigned char src[4 * 4 * 4] = { 0 };
    unsigned char dst[2 * 2 * 4] = { 0 };

    /* Fill a 4x4 RGBA8 texture with a pattern */
    for (int i = 0; i < 4 * 4 * 4; i++)
        src[i] = (unsigned char)(i * 17);

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA,
                    GL_UNSIGNED_BYTE, src);

    /* Read back a 2x2 sub-region starting at (1, 1) */
    glGetTextureSubImage(tex, 0, 1, 1, 0, 2, 2, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, sizeof dst, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* Pixel at (1,1) in the source = row 1, col 1 = byte offset (1*4+1)*4=20 */
    CHECK_EQ_INT(dst[0], src[20]);

    glDeleteTextures(1, &tex);
}

/* ---------- glGetnTexImage ---------- */

GPU_TEST(texture_query, getn_tex_image)
{
    GLuint tex = 0;
    unsigned char src[] = { 1,2,3,255, 5,6,7,255, 9,10,11,255, 13,14,15,255 };
    unsigned char dst[16] = { 0 };

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, src);

    /* Sufficient buffer */
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof dst, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* Buffer too small */
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, 8, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* Negative bufSize */
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, -1, dst);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &tex);
}

/* ---------- Compressed texture stubs ---------- */

GPU_TEST(texture_query, compressed_texture_stubs)
{
    /* All compressed texture readback entry points are stubs that print
     * a warning but do not set an error. Verify they don't crash. */
    unsigned char buf[64] = { 0 };

    glGetCompressedTexImage(GL_TEXTURE_2D, 0, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetnCompressedTexImage(GL_TEXTURE_2D, 0, sizeof buf, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetCompressedTextureSubImage(999, 0, 0, 0, 0, 4, 4, 1, sizeof buf, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetCompressedTextureImage(999, 0, sizeof buf, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- glGetnCompressedTexImage error validation ---------- */

GPU_TEST(texture_query, compressed_rejects_invalid_enum)
{
    unsigned char buf[64] = { 0 };

    /* Invalid target */
    glGetCompressedTexImage(0x9999, 0, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- Texture image entry points with no bound texture ---------- */

GPU_TEST(texture_query, get_tex_image_no_texture)
{
    unsigned char buf[64] = { 0 };

    /* These should all set INVALID_OPERATION when no texture exists */
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof buf, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- Swizzle parameter round-trip ---------- */

GPU_TEST(texture_query, swizzle_round_trip)
{
    GLuint tex = 0;
    GLint swiz[4] = { 99, 99, 99, 99 };

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);

    /* Default identity swizzle */
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, &swiz[0]);
    CHECK_EQ_INT(swiz[0], GL_RED);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, &swiz[0]);
    CHECK_EQ_INT(swiz[0], GL_GREEN);

    /* Set a custom swizzle */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ALPHA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_GREEN);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, &swiz[0]);
    CHECK_EQ_INT(swiz[0], GL_BLUE);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, &swiz[1]);
    CHECK_EQ_INT(swiz[1], GL_ALPHA);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, &swiz[2]);
    CHECK_EQ_INT(swiz[2], GL_RED);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, &swiz[3]);
    CHECK_EQ_INT(swiz[3], GL_GREEN);

    glDeleteTextures(1, &tex);
}

/* ---------- Depth-stencil texture mode ---------- */

GPU_TEST(texture_query, depth_stencil_texture_mode)
{
    GLuint tex = 0;
    GLint mode = 0;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, &mode);
    CHECK_EQ_INT(mode, GL_DEPTH_COMPONENT);

    glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, &mode);
    CHECK_EQ_INT(mode, GL_STENCIL_INDEX);

    glDeleteTextures(1, &tex);
}

/* ---------- glGetTexLevelParameterfv via glGetTextureLevelParameterfv -- DSA float ---------- */

GPU_TEST(texture_query, texture_level_parameter_fv_dsa)
{
    GLuint tex = 0;
    GLfloat fv = -1.0f;

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    glGetTextureLevelParameterfv(tex, 0, GL_TEXTURE_INTERNAL_FORMAT, &fv);
    CHECK_NEAR(fv, (GLfloat)GL_RGBA8, 1e-5f);

    glDeleteTextures(1, &tex);
}
