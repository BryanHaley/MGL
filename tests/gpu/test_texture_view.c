/*
 * test_texture_view.c
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * glTextureView: a second texture over part of another's storage. It used
 * to create nothing at all.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static GLint texParam(GLuint tex, GLenum target, GLenum pname)
{
    GLint v = -1;

    glBindTexture(target, tex);
    glGetTexParameteriv(target, pname, &v);
    glBindTexture(target, 0);

    return v;
}

// The view's levels and layers, reported relative to the original, and the
// errors the specification lists
GPU_TEST(texture_view, queries_and_errors)
{
    GLuint orig, view, vov, mut, names[3];

    glGenTextures(1, &orig);
    glBindTexture(GL_TEXTURE_2D_ARRAY, orig);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 6, GL_RGBA8, 64, 32, 16);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    CHECK_EQ_INT(texParam(orig, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_IMMUTABLE_LEVELS), 6);
    CHECK_EQ_INT(texParam(orig, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_NUM_LEVELS), 6);
    CHECK_EQ_INT(texParam(orig, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_NUM_LAYERS), 16);

    glGenTextures(3, names);
    view = names[0];
    vov = names[1];

    glTextureView(view, GL_TEXTURE_2D_ARRAY, orig, GL_R32UI, 2, 3, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(texParam(view, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_MIN_LEVEL), 2);
    CHECK_EQ_INT(texParam(view, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_NUM_LEVELS), 3);
    CHECK_EQ_INT(texParam(view, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_MIN_LAYER), 1);
    CHECK_EQ_INT(texParam(view, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_NUM_LAYERS), 2);
    CHECK_EQ_INT(texParam(view, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_IMMUTABLE_LEVELS), 6);

    // the same arguments again run past the first view's end and are cut short
    glTextureView(vov, GL_TEXTURE_2D_ARRAY, view, GL_RGBA8, 2, 3, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(texParam(vov, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_MIN_LEVEL), 4);
    CHECK_EQ_INT(texParam(vov, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_NUM_LEVELS), 1);
    CHECK_EQ_INT(texParam(vov, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_MIN_LAYER), 2);
    CHECK_EQ_INT(texParam(vov, GL_TEXTURE_2D_ARRAY, GL_TEXTURE_VIEW_NUM_LAYERS), 1);

    glTextureView(0, GL_TEXTURE_2D, orig, GL_RGBA8, 0, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glTextureView(0x7FFFFFF0u, GL_TEXTURE_2D, orig, GL_RGBA8, 0, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glTextureView(view, GL_TEXTURE_2D, orig, GL_RGBA8, 0, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glTextureView(names[2], GL_TEXTURE_3D, orig, GL_RGBA8, 0, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glTextureView(names[2], GL_TEXTURE_2D, orig, GL_RGBA16F, 0, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glTextureView(names[2], GL_TEXTURE_2D, orig, GL_RGBA8, 6, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glTextureView(names[2], GL_TEXTURE_2D, orig, GL_RGBA8, 0, 1, 0, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    // the faces of a cube have to be square
    glTextureView(names[2], GL_TEXTURE_CUBE_MAP, orig, GL_RGBA8, 0, 1, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGenTextures(1, &mut);
    glBindTexture(GL_TEXTURE_2D, mut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    CHECK_EQ_INT(texParam(mut, GL_TEXTURE_2D, GL_TEXTURE_VIEW_NUM_LEVELS), 0);
    glTextureView(names[2], GL_TEXTURE_2D, mut, GL_RGBA8, 0, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(3, names);
    glDeleteTextures(1, &mut);
    glDeleteTextures(1, &orig);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// What is written through one is read through the other, in each one's own
// format, and the view sees only its own level
GPU_TEST(texture_view, view_and_original_share_their_texels)
{
    static const GLubyte level1[4 * 2 * 2] = {
        1, 2, 3, 4,   5, 6, 7, 8,   9, 10, 11, 12,   13, 14, 15, 16,
    };
    const GLuint word = 0x44332211u;
    GLuint orig, view, got[4] = { 0 };
    GLubyte back[16] = { 0 };

    glGenTextures(1, &orig);
    glBindTexture(GL_TEXTURE_2D, orig);
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_RGBA8, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, level1);

    glGenTextures(1, &view);
    glTextureView(view, GL_TEXTURE_2D, orig, GL_R32UI, 1, 1, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureImage(view, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(got[0] == 0x04030201u && got[3] == 0x100F0E0Du,
              "view read 0x%08x .. 0x%08x", got[0], got[3]);

    glTextureSubImage2D(view, 0, 1, 1, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &word);
    glGetTextureImage(orig, 1, GL_RGBA, GL_UNSIGNED_BYTE, sizeof back, back);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(back[12] == 0x11 && back[15] == 0x44 && back[0] == 1,
              "original read %d,%d .. %d", back[12], back[15], back[0]);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &view);
    glDeleteTextures(1, &orig);
}

// A 2D view of one face of a cube map, and a render into a view landing in
// the original
GPU_TEST(texture_view, cube_face_and_render_target)
{
    static const GLubyte red[4] = { 255, 0, 0, 255 };
    GLuint cube, face, fb;
    GLubyte px[4 * 4 * 4] = { 0 };

    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, 4, 4);
    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, 2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, red);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glGenTextures(1, &face);
    glTextureView(face, GL_TEXTURE_2D, cube, GL_RGBA8, 0, 1, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTextureImage(face, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof px, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(px[(2 * 4 + 2) * 4] == 255, "face view missed the original's texel");

    // clear the face through the view; the original's face changes
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, face, 0);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    glViewport(0, 0, 4, 4);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    memset(px, 0, sizeof px);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(px[1] == 255 && px[0] == 0, "cube face after the clear: %d,%d,%d", px[0], px[1], px[2]);

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &face);
    glDeleteTextures(1, &cube);
}

// The storage lives as long as any view of it, even with the original and a
// view in the middle deleted
GPU_TEST(texture_view, storage_outlives_the_original)
{
    static const GLubyte color[4] = { 123, 34, 56, 78 };
    GLuint a, b, c;
    GLubyte px[4] = { 0 };

    glGenTextures(1, &a);
    glBindTexture(GL_TEXTURE_2D, a);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &b);
    glTextureView(b, GL_TEXTURE_2D, a, GL_RGBA8, 0, 1, 0, 1);
    glGenTextures(1, &c);
    glTextureView(c, GL_TEXTURE_2D, b, GL_RGBA8, 0, 1, 0, 1);
    glTextureSubImage2D(a, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &a);
    glDeleteTextures(1, &b);

    glGetTextureImage(c, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof px, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(px[0] == 123 && px[3] == 78, "the last view read %d,%d,%d,%d", px[0], px[1], px[2], px[3]);

    glDeleteTextures(1, &c);
}
