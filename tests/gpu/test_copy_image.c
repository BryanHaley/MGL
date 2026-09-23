/*
 * test_copy_image.c
 * Copyright (C) The MooGL Project
 *
 * glCopyImageSubData: the copy itself, and the errors the spec lists for it.
 */

#include <stdio.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

/* MAX_LEVEL defaults to 1000, so a one-level texture is mipmap-incomplete
   until the app says otherwise. The CTS relies on that, and so does GL. */
static GLuint make_tex(GLenum internalformat, GLsizei w, GLsizei h, GLubyte fill)
{
    GLubyte *px = malloc((size_t)w * h * 4);
    GLuint tex = 0;

    for (GLsizei i = 0; i < w * h; i++)
    {
        px[i * 4 + 0] = (GLubyte)(fill + i);
        px[i * 4 + 1] = fill;
        px[i * 4 + 2] = (GLubyte)(fill ^ 0x5A);
        px[i * 4 + 3] = 0xFF;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalformat, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    free(px);
    return tex;
}

GPU_TEST(copy_image, copies_pixels_and_leaves_the_source_alone)
{
    /* The Metal texture is created on bind, and glCopyImageSubData used to
       refuse the copy because mtl_data was still NULL at validation time. */
    GLuint src = make_tex(GL_RGBA8, 4, 4, 0x01);
    GLuint dst = make_tex(GL_RGBA8, 4, 4, 0x99);
    GLubyte got[4 * 4 * 4];

    mgl_drain_errors();

    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 0, 0, 0,
                       dst, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(got, 0xAB, sizeof got);
    glBindTexture(GL_TEXTURE_2D, dst);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_INT(0x01, got[0]);
    CHECK_EQ_INT(0x01, got[1]);
    CHECK_EQ_INT(0x02, got[4]);

    memset(got, 0xAB, sizeof got);
    glBindTexture(GL_TEXTURE_2D, src);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_INT(0x01, got[0]);

    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
}

GPU_TEST(copy_image, rejects_a_mipmap_incomplete_texture)
{
    GLuint src = make_tex(GL_RGBA8, 4, 4, 0x01);
    GLuint dst = 0;
    GLubyte px[4 * 4 * 4] = {0};

    /* left at MAX_LEVEL 1000 with only level 0, so incomplete */
    glGenTextures(1, &dst);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

    mgl_drain_errors();

    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 0, 0, 0,
                       dst, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, glGetError());

    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
}

GPU_TEST(copy_image, rejects_mismatched_texel_sizes)
{
    GLuint a = make_tex(GL_RGBA8, 4, 4, 0x01);      /* 4 bytes per texel */
    GLuint b = 0;
    GLushort px[4 * 4 * 4] = {0};

    glGenTextures(1, &b);                            /* 8 bytes per texel */
    glBindTexture(GL_TEXTURE_2D, b);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16UI, 4, 4, 0,
                 GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    mgl_drain_errors();

    glCopyImageSubData(a, GL_TEXTURE_2D, 0, 0, 0, 0,
                       b, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, glGetError());

    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
}

GPU_TEST(copy_image, rejects_a_region_past_the_edge)
{
    GLuint a = make_tex(GL_RGBA8, 4, 4, 0x01);
    GLuint b = make_tex(GL_RGBA8, 4, 4, 0x02);

    mgl_drain_errors();

    glCopyImageSubData(a, GL_TEXTURE_2D, 0, 2, 0, 0,
                       b, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(GL_INVALID_VALUE, glGetError());

    glCopyImageSubData(a, GL_TEXTURE_2D, 0, 0, 0, 0,
                       b, GL_TEXTURE_2D, 0, 0, 3, 0, 4, 4, 1);
    CHECK_EQ_UINT(GL_INVALID_VALUE, glGetError());

    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
}

GPU_TEST(copy_image, rejects_a_level_that_does_not_exist)
{
    GLuint a = make_tex(GL_RGBA8, 4, 4, 0x01);
    GLuint b = make_tex(GL_RGBA8, 4, 4, 0x02);

    mgl_drain_errors();

    glCopyImageSubData(a, GL_TEXTURE_2D, 1, 0, 0, 0,
                       b, GL_TEXTURE_2D, 0, 0, 0, 0, 2, 2, 1);
    CHECK_EQ_UINT(GL_INVALID_VALUE, glGetError());

    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
}

GPU_TEST(copy_image, accepts_a_renderbuffer_as_either_end)
{
    /* Renderbuffers live in their own table; findTexture never saw them. */
    GLuint rb = 0, tex = make_tex(GL_RGBA8, 4, 4, 0x01);

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 4, 4);

    mgl_drain_errors();

    glCopyImageSubData(tex, GL_TEXTURE_2D, 0, 0, 0, 0,
                       rb, GL_RENDERBUFFER, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glCopyImageSubData(rb, GL_RENDERBUFFER, 0, 0, 0, 0,
                       tex, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDeleteRenderbuffers(1, &rb);
    glDeleteTextures(1, &tex);
}

/* ---------------------------------------------------------------------------
 * Two different internal formats of the same size class.
 *
 * GL allows it; Metal's texture-to-texture blit wants one pixel format, so the
 * bytes take a detour through a buffer. The point of the test is that the bits
 * arrive unchanged -- the copy reinterprets, it does not convert.
 */

static GLuint packed_tex(GLenum internalformat, GLenum format, GLenum type,
                         GLsizei w, GLsizei h, const GLuint *px)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalformat, w, h, 0, format, type, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    return tex;
}

GPU_TEST(copy_image, copies_between_two_formats_of_one_size_class)
{
    /* four texels of RGBA8UI, which is 32 bits like R32UI, and both read back
       exactly -- so any difference is the copy's, not the readback's */
    static const GLuint src_px[4] = { 0x11223344u, 0x55667788u, 0x0BADF00Du, 0xFEEDFACEu };
    static const GLuint dst_px[4] = { 0u, 0u, 0u, 0u };
    GLuint src = packed_tex(GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, 2, 2, src_px);
    GLuint dst = packed_tex(GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, 2, 2, dst_px);
    GLuint back[4] = { 0u, 0u, 0u, 0u };

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 0, 0, 0,
                       dst, GL_TEXTURE_2D, 0, 0, 0, 0, 2, 2, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindTexture(GL_TEXTURE_2D, dst);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, back);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 4; i++)
        CHECK_MSG(back[i] == src_px[i],
                  "texel %d came back 0x%08X, the source bits are 0x%08X",
                  i, back[i], src_px[i]);

    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
}

/* ---------- the target has to be what the object is ---------- */

GPU_TEST(copy_image, rejects_a_target_that_is_not_the_objects_own)
{
    GLuint a = make_tex(GL_RGBA8, 4, 4, 0x01);
    GLuint b = make_tex(GL_RGBA8, 4, 4, 0x02);

    mgl_drain_errors();

    // both are 2D textures; calling one a 3D texture is an enum error
    glCopyImageSubData(a, GL_TEXTURE_3D, 0, 0, 0, 0,
                       b, GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1);
    CHECK_EQ_UINT(GL_INVALID_ENUM, glGetError());

    glCopyImageSubData(a, GL_TEXTURE_2D, 0, 0, 0, 0,
                       b, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 1, 1, 1);
    CHECK_EQ_UINT(GL_INVALID_ENUM, glGetError());

    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);
}

/* ---------- a 1D array's layers are reached through z ---------- */

// MGL keeps a 1D array's layer count where a height would go, so y was
// allowed to walk the layers. An image in a 1D array is one texel tall.
GPU_TEST(copy_image, a_1d_array_is_one_texel_tall)
{
    GLuint t[2];
    GLubyte px[8 * 4 * 4];

    memset(px, 0x40, sizeof px);
    glGenTextures(2, t);

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_1D_ARRAY, t[i]);
        glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_RGBA8, 8, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
    }

    mgl_drain_errors();

    // all four layers, the way GL says: through z and depth
    glCopyImageSubData(t[0], GL_TEXTURE_1D_ARRAY, 0, 0, 0, 0,
                       t[1], GL_TEXTURE_1D_ARRAY, 0, 0, 0, 0, 8, 1, 4);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    // two rows tall is past the edge of a 1D image
    glCopyImageSubData(t[0], GL_TEXTURE_1D_ARRAY, 0, 0, 0, 0,
                       t[1], GL_TEXTURE_1D_ARRAY, 0, 0, 0, 0, 8, 2, 1);
    CHECK_EQ_UINT(GL_INVALID_VALUE, glGetError());

    glDeleteTextures(2, t);
}
