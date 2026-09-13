/*
 * test_readback_rules.c
 * Copyright (C) The Moogle Project
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
