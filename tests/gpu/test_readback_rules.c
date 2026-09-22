/*
 * test_readback_rules.c
 * Copyright (C) The MooGL Project
 *
 * GL 4.6 section 8.11.4: a readback's client format has to agree with the
 * texture's base internal format about two things Metal cannot reinterpret --
 * whether the values are integers, and whether they are depth or stencil.
 */

#include <string.h>

#include "mgl_test.h"
#include "harness.h"

static GLuint tex_with(GLenum internalformat, GLenum format, GLenum type)
{
    GLubyte px[8 * 8 * 16] = {0};
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalformat, 4, 4, 0, format, type, px);
    mgl_drain_errors();

    return tex;
}

static void expect(GLuint tex, GLenum format, GLenum type, GLenum want, const char *what)
{
    GLubyte out[8 * 8 * 16];

    glBindTexture(GL_TEXTURE_2D, tex);
    mgl_drain_errors();

    glGetTexImage(GL_TEXTURE_2D, 0, format, type, out);

    CHECK_MSG(glGetError() == want, "%s", what);
}

GPU_TEST(readback_rules, integer_and_float_formats_do_not_mix)
{
    GLuint i = tex_with(GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE);
    GLuint f = tex_with(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

    expect(i, GL_RGBA, GL_UNSIGNED_BYTE, GL_INVALID_OPERATION,
           "an integer texture read as a float format must fail");
    expect(f, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, GL_INVALID_OPERATION,
           "a float texture read as an integer format must fail");

    expect(i, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, GL_NO_ERROR,
           "an integer texture read as an integer format must work");
    expect(f, GL_RGBA, GL_UNSIGNED_BYTE, GL_NO_ERROR,
           "a float texture read as a float format must work");

    glDeleteTextures(1, &i);
    glDeleteTextures(1, &f);
}

GPU_TEST(readback_rules, colour_is_not_depth_or_stencil)
{
    GLuint t = tex_with(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

    expect(t, GL_DEPTH_COMPONENT, GL_FLOAT, GL_INVALID_OPERATION,
           "a colour texture read as GL_DEPTH_COMPONENT must fail");
    expect(t, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, GL_INVALID_OPERATION,
           "a colour texture read as GL_STENCIL_INDEX must fail");

    glDeleteTextures(1, &t);
}

/* These already held; they are here so a change to the rule above cannot
   quietly loosen them. */
GPU_TEST(readback_rules, packed_types_keep_their_component_counts)
{
    GLuint rgb  = tex_with(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE);
    GLuint rgba = tex_with(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

    expect(rgb,  GL_RGB,  GL_UNSIGNED_SHORT_5_6_5, GL_NO_ERROR,
           "5_6_5 with GL_RGB is legal");
    expect(rgb,  GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, GL_INVALID_OPERATION,
           "5_6_5 with GL_RGBA is not");
    expect(rgba, GL_RGB,  GL_UNSIGNED_SHORT_4_4_4_4, GL_INVALID_OPERATION,
           "4_4_4_4 with GL_RGB is not");
    expect(rgba, GL_RGB,  GL_UNSIGNED_INT_2_10_10_10_REV, GL_INVALID_OPERATION,
           "2_10_10_10_REV with GL_RGB is not");

    glDeleteTextures(1, &rgb);
    glDeleteTextures(1, &rgba);
}

/* ---------- readback into a pixel pack buffer ---------- */

GPU_TEST(readback_rules, get_tex_image_writes_into_a_pack_buffer)
{
    // With GL_PIXEL_PACK_BUFFER bound, the pointer is an offset into it.
    // glGetTexImage ignored the binding and rejected the NULL pointer.
    GLubyte src[4 * 4 * 4];
    GLuint tex = 0, pbo = 0;

    for (int i = 0; i < 16; i++)
    {
        src[i * 4 + 0] = (GLubyte)(i * 16);
        src[i * 4 + 1] = 0x20;
        src[i * 4 + 2] = 0x30;
        src[i * 4 + 3] = 0xFF;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);

    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, sizeof src, NULL, GL_STATIC_READ);

    mgl_drain_errors();

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    const GLubyte *m = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, sizeof src, GL_MAP_READ_BIT);
    CHECK(m != NULL);

    if (m)
    {
        CHECK_EQ_INT(0x00, m[0]);
        CHECK_EQ_INT(0x20, m[1]);
        CHECK_EQ_INT(0x10, m[4]);
        CHECK_EQ_INT(0xFF, m[7]);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glDeleteBuffers(1, &pbo);
    glDeleteTextures(1, &tex);
}

/* Table 8.3 lists the single-channel selectors, and table 8.5 pairs the packed
   types with the _INTEGER formats as well as the plain ones. */
GPU_TEST(readback_rules, single_channel_selectors_are_legal)
{
    GLuint t = tex_with(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

    expect(t, GL_GREEN, GL_UNSIGNED_BYTE, GL_NO_ERROR, "GL_GREEN must be legal");
    expect(t, GL_BLUE,  GL_UNSIGNED_BYTE, GL_NO_ERROR, "GL_BLUE must be legal");

    glDeleteTextures(1, &t);
}

GPU_TEST(readback_rules, packed_types_pair_with_integer_formats_too)
{
    GLuint rgb = tex_with(GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE);
    GLuint rgba = tex_with(GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE);

    expect(rgb,  GL_RGB_INTEGER,  GL_UNSIGNED_SHORT_5_6_5, GL_NO_ERROR,
           "5_6_5 with GL_RGB_INTEGER is legal");
    expect(rgba, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT_4_4_4_4, GL_NO_ERROR,
           "4_4_4_4 with GL_RGBA_INTEGER is legal");

    glDeleteTextures(1, &rgb);
    glDeleteTextures(1, &rgba);
}

/* GL 4.6 table 8.1: SWAP_BYTES reverses the bytes of each component. MGL
   recorded the flag and ignored it. */
GPU_TEST(readback_rules, pack_swap_bytes_reverses_components)
{
    GLubyte src[4 * 4] = {0};
    GLuint tex = 0;
    GLushort normal[4 * 4 * 4], swapped[4 * 4 * 4];

    for (int i = 0; i < 4; i++)
    {
        src[i * 4 + 0] = 0x12;
        src[i * 4 + 1] = 0x34;
        src[i * 4 + 2] = 0x56;
        src[i * 4 + 3] = 0xFF;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    mgl_drain_errors();

    memset(normal, 0xAB, sizeof normal);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT, normal);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(swapped, 0xAB, sizeof swapped);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_SHORT, swapped);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);

    {
        const GLubyte *a = (const GLubyte *)normal;
        const GLubyte *b = (const GLubyte *)swapped;

        /* every 16-bit component comes back with its two bytes reversed */
        for (int i = 0; i < 8; i++)
        {
            CHECK_EQ_INT(a[i * 2 + 1], b[i * 2 + 0]);
            CHECK_EQ_INT(a[i * 2 + 0], b[i * 2 + 1]);
        }
    }

    glDeleteTextures(1, &tex);
}

/* An integer client format carries raw integers in the packed fields, not
   normalised values. Packing them through the float path made every integer
   readback through a packed type come back as 0. */
GPU_TEST(readback_rules, packed_integer_readback_keeps_raw_values)
{
    GLuint src[4 * 4];
    GLuint got[4 * 4];
    GLuint tex = 0;

    for (int i = 0; i < 4; i++)
    {
        src[i * 4 + 0] = 700;
        src[i * 4 + 1] = 300;
        src[i * 4 + 2] = 1000;
        src[i * 4 + 3] = 2;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2UI, 2, 2, 0,
                 GL_RGBA_INTEGER, GL_UNSIGNED_INT, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_EQ_UINT(700u,  got[0] & 0x3FFu);
    CHECK_EQ_UINT(300u,  (got[0] >> 10) & 0x3FFu);
    CHECK_EQ_UINT(1000u, (got[0] >> 20) & 0x3FFu);
    CHECK_EQ_UINT(2u,    (got[0] >> 30) & 0x3u);

    /* and the same values survive a packed upload */
    {
        GLuint packed[4];
        GLuint back[4 * 4];

        for (int i = 0; i < 4; i++)
            packed[i] = (2u << 30) | (1000u << 20) | (300u << 10) | 700u;

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2UI, 2, 2, 0,
                     GL_RGBA_INTEGER, GL_UNSIGNED_INT_2_10_10_10_REV, packed);
        CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

        memset(back, 0xAB, sizeof back);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, back);

        CHECK_EQ_UINT(700u,  back[0]);
        CHECK_EQ_UINT(300u,  back[1]);
        CHECK_EQ_UINT(1000u, back[2]);
        CHECK_EQ_UINT(2u,    back[3]);
    }

    glDeleteTextures(1, &tex);
}

/* PACK_SKIP_IMAGES counts whole images, so it means nothing to glReadPixels or
   to a 2D texture read. Adding it there walked past what was written. */
GPU_TEST(readback_rules, skip_images_does_not_move_a_two_dimensional_read)
{
    const GLsizei w = 4, h = 4;
    GLubyte src[4 * 4 * 4], got[4 * 4 * 4];
    GLuint tex = 0;
    int bad = 0;

    for (int i = 0; i < w * h * 4; i++)
        src[i] = (GLubyte)(i * 3);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glPixelStorei(GL_PACK_SKIP_IMAGES, 2);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, 8);

    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    for (int i = 0; i < w * h * 4; i++)
        if (got[i] != src[i])
            bad++;

    CHECK_MSG(bad == 0, "%d of %d bytes differ", bad, w * h * 4);

    glPixelStorei(GL_PACK_SKIP_IMAGES, 0);
    glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
    glDeleteTextures(1, &tex);
}
