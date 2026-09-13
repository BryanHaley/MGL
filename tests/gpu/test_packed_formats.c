/*
 * test_packed_formats.c
 * Copyright (C) The Moogle Project
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
