/*
 * test_copy_image.c
 * Copyright (C) The MooGL Project
 *
 * glCopyImageSubData: the copy itself, and the errors the spec lists for it.
 */

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
