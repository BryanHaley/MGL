/*
 * test_packed_formats.c
 * Copyright (C) The MooGL Project
 *
 * Formats whose components do not land on byte boundaries. Each one is a
 * separate encode and decode in pixel_convert.c, and a missing one shows up
 * as glTexImage2D refusing data GL says is legal.
 */

#include <string.h>

#include "mgl_test.h"
#include "harness.h"

/* GL lets the client data format and type differ from the internal format;
   the driver converts. These three had no converter and were refused. */
static void upload_accepts(GLenum internalformat, const char *name)
{
    GLubyte px[4 * 4 * 4];
    GLuint tex = 0;

    for (int i = 0; i < 16; i++)
    {
        px[i * 4 + 0] = (GLubyte)(i * 16);
        px[i * 4 + 1] = (GLubyte)(255 - i * 16);
        px[i * 4 + 2] = 77;
        px[i * 4 + 3] = 255;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalformat, 4, 4, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);

    CHECK_MSG(glGetError() == GL_NO_ERROR, "%s rejected an RGBA/UNSIGNED_BYTE upload", name);

    glDeleteTextures(1, &tex);
}

GPU_TEST(packed_formats, upload_converts_into_narrow_formats)
{
    upload_accepts(GL_RGB5_A1, "GL_RGB5_A1");
    upload_accepts(GL_RGBA4, "GL_RGBA4");
    upload_accepts(GL_RGB9_E5, "GL_RGB9_E5");
    upload_accepts(GL_RGB10_A2, "GL_RGB10_A2");
    upload_accepts(GL_R11F_G11F_B10F, "GL_R11F_G11F_B10F");
}

/* Round trip through a 4-bit-per-channel format: what comes back has to be
   the input quantised to 4 bits, not zero and not the input unchanged. */
GPU_TEST(packed_formats, rgba4_round_trips_quantised)
{
    GLubyte in[4 * 4 * 4], out[4 * 4 * 4];
    GLuint tex = 0;

    for (int i = 0; i < 16; i++)
    {
        in[i * 4 + 0] = 0xFF;
        in[i * 4 + 1] = 0x00;
        in[i * 4 + 2] = 0x88;
        in[i * 4 + 3] = 0xFF;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, in);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(out, 0xAB, sizeof out);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    /* 4 bits each: 0xFF stays 0xFF, 0x00 stays 0x00, 0x88 lands on 0x88 or 0x99 */
    CHECK_EQ_INT(0xFF, out[0]);
    CHECK_EQ_INT(0x00, out[1]);
    CHECK(out[2] == 0x88 || out[2] == 0x99);
    CHECK_EQ_INT(0xFF, out[3]);

    glDeleteTextures(1, &tex);
}

/* One shared exponent across three channels; the encoder has to pick it from
   the largest of them. */
GPU_TEST(packed_formats, rgb9e5_round_trips)
{
    GLfloat in[4 * 4 * 3], out[4 * 4 * 3];
    GLuint tex = 0;

    for (int i = 0; i < 16; i++)
    {
        in[i * 3 + 0] = 1.0f;
        in[i * 3 + 1] = 0.5f;
        in[i * 3 + 2] = 0.25f;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB9_E5, 4, 4, 0, GL_RGB, GL_FLOAT, in);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(out, 0, sizeof out);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, out);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_NEAR(1.0f,  out[0], 0.01f);
    CHECK_NEAR(0.5f,  out[1], 0.01f);
    CHECK_NEAR(0.25f, out[2], 0.01f);

    glDeleteTextures(1, &tex);
}

GPU_TEST(packed_formats, rgb5_a1_keeps_one_alpha_bit)
{
    GLubyte in[4 * 4 * 4], out[4 * 4 * 4];
    GLuint tex = 0;

    for (int i = 0; i < 16; i++)
    {
        in[i * 4 + 0] = 0xFF;
        in[i * 4 + 1] = 0x00;
        in[i * 4 + 2] = 0xFF;
        in[i * 4 + 3] = (i & 1) ? 0xFF : 0x00;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, in);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(out, 0xAB, sizeof out);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_EQ_INT(0xFF, out[0]);
    CHECK_EQ_INT(0x00, out[1]);
    CHECK_EQ_INT(0xFF, out[2]);
    CHECK_EQ_INT(0x00, out[3]);
    CHECK_EQ_INT(0xFF, out[7]);

    glDeleteTextures(1, &tex);
}

/* GL 4.6 section 8.5: an application may hand uncompressed pixels to a
   compressed internal format and the driver compresses them. MGL refused. */
GPU_TEST(packed_formats, uncompressed_upload_into_a_compressed_format)
{
    static const struct { GLenum f; const char *n; GLsizei bytes; } cases[] = {
        {GL_COMPRESSED_RED_RGTC1,        "GL_COMPRESSED_RED_RGTC1",        32},
        {GL_COMPRESSED_SIGNED_RED_RGTC1, "GL_COMPRESSED_SIGNED_RED_RGTC1", 32},
        {GL_COMPRESSED_RG_RGTC2,         "GL_COMPRESSED_RG_RGTC2",         64},
        {GL_COMPRESSED_SIGNED_RG_RGTC2,  "GL_COMPRESSED_SIGNED_RG_RGTC2",  64},
    };
    GLubyte px[8 * 8 * 4];

    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
        {
            size_t i = ((size_t)y * 8 + x) * 4;
            px[i + 0] = (GLubyte)(x * 36);
            px[i + 1] = (GLubyte)(y * 36);
            px[i + 2] = 0;
            px[i + 3] = 0xFF;
        }

    for (unsigned c = 0; c < sizeof cases / sizeof *cases; c++)
    {
        GLuint tex = 0;
        GLint compressed = -1, size = -1, ifmt = 0;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        mgl_drain_errors();

        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)cases[c].f, 8, 8, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, px);
        CHECK_MSG(glGetError() == GL_NO_ERROR, "%s refused an RGBA upload", cases[c].n);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &compressed);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &ifmt);

        CHECK_MSG(compressed == GL_TRUE, "%s did not report itself compressed", cases[c].n);
        CHECK_MSG(size == cases[c].bytes, "%s: image size %d, expected %d",
                  cases[c].n, size, cases[c].bytes);
        CHECK_MSG(ifmt == (GLint)cases[c].f, "%s: internal format came back 0x%x",
                  cases[c].n, ifmt);

        glDeleteTextures(1, &tex);
    }
}

/* The endpoints of a block are the extremes of the texels in it. */
GPU_TEST(packed_formats, rgtc_blocks_carry_the_right_endpoints)
{
    GLubyte px[8 * 8 * 4];
    GLubyte blocks[64];
    GLuint tex = 0;

    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
        {
            size_t i = ((size_t)y * 8 + x) * 4;
            px[i + 0] = (GLubyte)(x * 36);   /* 0, 36, 72, 108 in the first block */
            px[i + 1] = (GLubyte)(y * 36);
            px[i + 2] = 0;
            px[i + 3] = 0xFF;
        }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RG_RGTC2, 8, 8, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(blocks, 0xAB, sizeof blocks);
    glGetCompressedTexImage(GL_TEXTURE_2D, 0, blocks);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_EQ_INT(108, blocks[0]);   /* red high  */
    CHECK_EQ_INT(0,   blocks[1]);   /* red low   */
    CHECK_EQ_INT(108, blocks[8]);   /* green high */
    CHECK_EQ_INT(0,   blocks[9]);   /* green low  */

    glDeleteTextures(1, &tex);
}

/* A compressed colour format is not an integer format, and it is not depth. */
GPU_TEST(packed_formats, compressed_formats_refuse_integer_and_depth_uploads)
{
    static const struct { GLenum ifmt; GLenum cf; GLenum ct; GLenum want; const char *n; } cases[] = {
        {GL_COMPRESSED_RED_RGTC1, GL_RED,             GL_UNSIGNED_BYTE, GL_NO_ERROR,
         "RGTC1 from GL_RED"},
        {GL_COMPRESSED_RED_RGTC1, GL_RED_INTEGER,     GL_UNSIGNED_BYTE, GL_INVALID_OPERATION,
         "RGTC1 from GL_RED_INTEGER"},
        {GL_COMPRESSED_RG_RGTC2,  GL_RGBA_INTEGER,    GL_UNSIGNED_BYTE, GL_INVALID_OPERATION,
         "RGTC2 from GL_RGBA_INTEGER"},
        {GL_COMPRESSED_RG_RGTC2,  GL_DEPTH_COMPONENT, GL_FLOAT,         GL_INVALID_OPERATION,
         "RGTC2 from GL_DEPTH_COMPONENT"},
        {GL_COMPRESSED_RG_RGTC2,  GL_RG,              GL_FLOAT,         GL_NO_ERROR,
         "RGTC2 from GL_RG"},
    };
    GLubyte px[8 * 8 * 16] = {0};

    for (unsigned c = 0; c < sizeof cases / sizeof *cases; c++)
    {
        GLuint tex = 0;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        mgl_drain_errors();

        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)cases[c].ifmt, 8, 8, 0,
                     cases[c].cf, cases[c].ct, px);

        CHECK_MSG(glGetError() == cases[c].want, "%s", cases[c].n);

        glDeleteTextures(1, &tex);
    }
}

/* Metal names the components of a packed format from the low bits up, so
   MTLPixelFormatABGR4Unorm holds red at the top -- the same place GL's
   4_4_4_4 puts it. Reading the name left to right stored every channel
   backwards. */
GPU_TEST(packed_formats, rgba4_keeps_channel_order)
{
    GLushort src = (GLushort)((0xAu << 12) | (0x5u << 8) | (0x3u << 4) | 0xFu);
    GLushort got = 0;
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, &src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, &got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    CHECK_EQ_UINT((unsigned)src, (unsigned)got);

    {
        GLubyte rgba[4] = { 0, 0, 0, 0 };

        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        CHECK_EQ_UINT(0xAAu, rgba[0]);
        CHECK_EQ_UINT(0x55u, rgba[1]);
        CHECK_EQ_UINT(0x33u, rgba[2]);
        CHECK_EQ_UINT(0xFFu, rgba[3]);
    }

    glDeleteTextures(1, &tex);
}

GPU_TEST(packed_formats, rgb5_a1_keeps_channel_order)
{
    GLushort src = (GLushort)((0x15u << 11) | (0x0Au << 6) | (0x05u << 1) | 1u);
    GLushort got = 0;
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, &src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, &got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    CHECK_EQ_UINT((unsigned)src, (unsigned)got);

    glDeleteTextures(1, &tex);
}

/* GL_RGB10 was stored in an 8-bit texture while GL_TEXTURE_RED_SIZE claimed
   ten, so anything reading it back at more than 8 bits saw the quantisation. */
GPU_TEST(packed_formats, rgb10_really_has_ten_bits)
{
    const GLushort src[3] = { 0xAAAA, 0x5555, 0x3333 };
    GLushort got[3] = { 0, 0, 0 };
    GLint red_bits = 0;
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &red_bits);
    CHECK_EQ_UINT(10u, (unsigned)red_bits);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_SHORT, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    /* ten bits gets within a thousandth; eight bits would not */
    for (int i = 0; i < 3; i++)
    {
        int d = (int)got[i] - (int)src[i];

        if (d < 0) d = -d;
        CHECK_MSG(d <= 64, "channel %d: %u for %u", i, got[i], src[i]);
    }

    glDeleteTextures(1, &tex);
}

/* Table 8.5 pairs the 3_3_2 and 5_6_5 families with GL_RGB *and*
   GL_RGB_INTEGER. MGL only accepted GL_RGB, so every integer format the CTS
   fed a packed type came back as INVALID_OPERATION. */
GPU_TEST(packed_formats, rgb_integer_takes_the_packed_rgb_types)
{
    static const GLenum types[4] = {
        GL_UNSIGNED_BYTE_3_3_2, GL_UNSIGNED_BYTE_2_3_3_REV,
        GL_UNSIGNED_SHORT_5_6_5, GL_UNSIGNED_SHORT_5_6_5_REV
    };
    static const GLenum ifmts[3] = { GL_RGBA8UI, GL_RGBA16UI, GL_RGBA32UI };
    GLubyte data[8 * 8 * 4];
    GLuint tex = 0;

    memset(data, 0x24, sizeof data);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    mgl_drain_errors();

    for (int f = 0; f < 3; f++)
        for (int t = 0; t < 4; t++)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, (GLint)ifmts[f], 8, 8, 0,
                         GL_RGB_INTEGER, types[t], data);
            CHECK_MSG(mgl_drain_errors() == GL_NO_ERROR,
                      "internalformat 0x%x with type 0x%x", ifmts[f], types[t]);
        }

    /* and a non-integer format with an integer client format is still wrong */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0,
                 GL_RGB_INTEGER, GL_UNSIGNED_BYTE_3_3_2, data);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glDeleteTextures(1, &tex);
}
