/*
 * test_dsa_rules.c
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
 * Starting values and error codes GL 4.6 spells out, found through the
 * direct state access conformance tests, plus the copies and blits that
 * went to the wrong place.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static int ilog2_for_test(int v)
{
    int n = 0;

    while (v > 1) { v >>= 1; n++; }
    return n;
}

GPU_TEST(dsa_rules, a_new_buffer_starts_static_draw)
{
    GLuint buf;
    GLint usage = 0;

    glCreateBuffers(1, &buf);
    glGetNamedBufferParameteriv(buf, GL_BUFFER_USAGE, &usage);
    CHECK_EQ_INT(GL_STATIC_DRAW, usage);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteBuffers(1, &buf);
}

GPU_TEST(dsa_rules, clearing_a_buffer_with_no_such_format_is_a_value_error)
{
    GLuint buf;
    GLubyte one = 1;

    glCreateBuffers(1, &buf);
    glNamedBufferData(buf, 16, NULL, GL_STATIC_DRAW);

    glClearNamedBufferData(buf, GL_R8, 1, GL_UNSIGNED_BYTE, &one);
    CHECK_EQ_UINT(GL_INVALID_VALUE, mgl_drain_errors());

    glClearNamedBufferData(buf, GL_R8, GL_RED, 1, &one);
    CHECK_EQ_UINT(GL_INVALID_VALUE, mgl_drain_errors());

    glDeleteBuffers(1, &buf);
}

GPU_TEST(dsa_rules, the_mapped_pointer_is_the_one_map_returned)
{
    GLuint buf;
    void *mapped, *asked = NULL;

    glCreateBuffers(1, &buf);
    glNamedBufferData(buf, 64, NULL, GL_STATIC_DRAW);

    mapped = glMapNamedBufferRange(buf, 16, 16, GL_MAP_WRITE_BIT);
    glGetNamedBufferPointerv(buf, GL_BUFFER_MAP_POINTER, &asked);
    CHECK(mapped != NULL);
    CHECK(asked == mapped);
    glUnmapNamedBuffer(buf);

    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    glDeleteBuffers(1, &buf);
}

GPU_TEST(dsa_rules, clear_buffer_fv_takes_no_stencil)
{
    static const GLfloat v[4] = { 0 };
    MGLTestTarget t;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 1);
    mgl_target_bind(&t);

    glClearBufferfv(GL_STENCIL, 0, v);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glClearBufferfv(GL_DEPTH, 1, v);
    CHECK_EQ_UINT(GL_INVALID_VALUE, mgl_drain_errors());

    mgl_target_destroy(&t);
}

GPU_TEST(dsa_rules, read_buffer_follows_the_framebuffer)
{
    GLuint fb;

    glNamedFramebufferReadBuffer(0, GL_COLOR_ATTACHMENT0);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glCreateFramebuffers(1, &fb);
    glNamedFramebufferReadBuffer(fb, GL_BACK);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glNamedFramebufferReadBuffer(fb, GL_COLOR_ATTACHMENT0);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteFramebuffers(1, &fb);
}

GPU_TEST(dsa_rules, an_attachment_past_the_limit_is_an_operation_error)
{
    GLint max = 0;
    GLuint fb, rb, tex;

    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max);
    glCreateFramebuffers(1, &fb);
    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorage(rb, GL_RGBA8, 4, 4);
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA8, 4, 4);

    glNamedFramebufferRenderbuffer(fb, GL_COLOR_ATTACHMENT0 + max, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glNamedFramebufferTexture(fb, GL_COLOR_ATTACHMENT0 + max, tex, 0);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glNamedFramebufferTexture(fb, GL_BACK, tex, 0);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glDeleteTextures(1, &tex);
    glDeleteRenderbuffers(1, &rb);
    glDeleteFramebuffers(1, &fb);
}

GPU_TEST(dsa_rules, attachment_queries_and_invalidation_check_their_names)
{
    static const GLenum bad = GL_BACK;
    GLuint fb, rb;
    GLint v = 0;

    glCreateFramebuffers(1, &fb);
    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorage(rb, GL_RGBA8, 4, 4);
    glNamedFramebufferRenderbuffer(fb, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);

    // a renderbuffer has no texture level
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &v);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glInvalidateNamedFramebufferData(fb, 1, &bad);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glDeleteRenderbuffers(1, &rb);
    glDeleteFramebuffers(1, &fb);
}

GPU_TEST(dsa_rules, renderbuffer_storage_checks_format_and_samples)
{
    GLint max = 0;
    GLuint rb;

    glGetIntegerv(GL_MAX_SAMPLES, &max);
    glCreateRenderbuffers(1, &rb);

    glNamedRenderbufferStorage(rb, GL_COMPRESSED_RED, 4, 4);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glNamedRenderbufferStorageMultisample(rb, max + 1, GL_RGBA8, 4, 4);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glNamedRenderbufferStorage(rb, GL_RGB8, 4, 4);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteRenderbuffers(1, &rb);
}

// something attached without storage makes the framebuffer incomplete, not empty
GPU_TEST(dsa_rules, a_renderbuffer_without_storage_is_an_incomplete_attachment)
{
    GLuint fb, rb;

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    CHECK_EQ_UINT(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER));

    glDeleteRenderbuffers(1, &rb);
    glDeleteFramebuffers(1, &fb);
}

GPU_TEST(dsa_rules, texture_calls_check_the_target_and_names)
{
    GLuint t2d, tbuf, cube;
    GLfloat f = 0;
    GLubyte px[4] = { 0 };

    glCreateTextures(GL_TEXTURE_2D, 1, &t2d);
    glCreateTextures(GL_TEXTURE_BUFFER, 1, &tbuf);
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cube);

    glTextureBuffer(t2d, GL_RGBA8, 0);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glGetTextureParameterfv(tbuf, GL_TEXTURE_MAG_FILTER, &f);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glTextureParameterf(t2d, GL_TEXTURE_BORDER_COLOR, 1.0f);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glGetTextureLevelParameterfv(999321, 0, GL_TEXTURE_WIDTH, &f);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glTextureStorage2D(t2d, 1, GL_RGBA, 4, 4);   // unsized
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glTextureStorage2D(t2d, 1, GL_RGBA8, 4, 4);
    glTextureSubImage2D(t2d, 0, 0, 0, 1, 1, 1, GL_UNSIGNED_BYTE, px);   // no such format
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    // one face is not a cube
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenerateTextureMipmap(cube);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glDeleteTextures(1, &t2d);
    glDeleteTextures(1, &tbuf);
    glDeleteTextures(1, &cube);
}

GPU_TEST(dsa_rules, vertex_arrays_check_limits_and_report_integers)
{
    GLint max_offset = 0, max_stride = 0, integer = 0;
    GLuint vao, buf;
    GLintptr off = 0;
    GLsizei stride;

    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET, &max_offset);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_STRIDE, &max_stride);
    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &buf);

    glVertexArrayAttribFormat(vao, 0, 4, GL_FLOAT, GL_FALSE, (GLuint)max_offset + 1);
    CHECK_EQ_UINT(GL_INVALID_VALUE, mgl_drain_errors());

    glVertexArrayElementBuffer(vao, 999123);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    stride = max_stride + 4;
    glVertexArrayVertexBuffers(vao, 0, 1, &buf, &off, &stride);
    CHECK_EQ_UINT(GL_INVALID_VALUE, mgl_drain_errors());

    glVertexArrayAttribIFormat(vao, 1, 2, GL_INT, 0);
    glGetVertexArrayIndexediv(vao, 1, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer);
    CHECK_EQ_INT(GL_TRUE, integer);

    glVertexArrayAttribFormat(vao, 1, 2, GL_FLOAT, GL_FALSE, 0);
    glGetVertexArrayIndexediv(vao, 1, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &integer);
    CHECK_EQ_INT(GL_FALSE, integer);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(dsa_rules, queries_start_available_and_check_buffer_room)
{
    GLuint q, buf;
    GLuint avail = 0;

    glCreateQueries(GL_SAMPLES_PASSED, 1, &q);
    glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &avail);
    CHECK_EQ_INT(GL_TRUE, (GLint)avail);

    glCreateBuffers(1, &buf);
    glNamedBufferData(buf, 4, NULL, GL_STATIC_DRAW);
    glGetQueryBufferObjectiv(q, buf, GL_QUERY_RESULT, 2);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glDeleteBuffers(1, &buf);
    glDeleteQueries(1, &q);
}

// the copy lands in the layer asked for, not layer 0
GPU_TEST(dsa_rules, copy_tex_sub_image_writes_the_named_layer)
{
    static const GLfloat red[4] = { 1, 0, 0, 1 };
    GLubyte px[2 * 4 * 4 * 4];
    MGLTestTarget t;
    GLuint arr;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glClearBufferfv(GL_COLOR, 0, red);

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &arr);
    glTextureStorage3D(arr, 1, GL_RGBA8, 4, 4, 2);
    glCopyTextureSubImage3D(arr, 0, 0, 0, 1, 0, 0, 4, 4);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    glFinish();

    memset(px, 0x33, sizeof px);
    glGetTextureImage(arr, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof px, px);
    CHECK_EQ_INT(255, px[64]);   // layer 1 red

    glDeleteTextures(1, &arr);
    mgl_target_destroy(&t);
}

// a blit stretches when the rectangles differ, and carries depth when asked
GPU_TEST(dsa_rules, blit_stretches_and_carries_depth)
{
    static const GLfloat red[4] = { 1, 0, 0, 1 }, zero[4] = { 0 };
    MGLTestTarget a, b;
    GLubyte px[4 * 4 * 4];
    GLfloat depth[4 * 4];

    mgl_target_create(&a, 4, 4, GL_RGBA8, 1);
    mgl_target_create(&b, 4, 4, GL_RGBA8, 1);

    mgl_target_bind(&b);
    glClearBufferfv(GL_COLOR, 0, zero);
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);

    mgl_target_bind(&a);
    glClearBufferfv(GL_COLOR, 0, red);
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.25f, 0);

    glBlitNamedFramebuffer(a.fbo, b.fbo, 0, 0, 1, 1, 0, 0, 3, 1,
                           GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glBindFramebuffer(GL_READ_FRAMEBUFFER, b.fbo);
    glReadPixels(0, 0, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_INT(255, px[0]);
    CHECK_EQ_INT(255, px[8]);    // the third, stretched to
    CHECK_EQ_INT(0, px[12]);     // the fourth, left alone

    glReadPixels(0, 0, 4, 1, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
    CHECK_NEAR(depth[2], 0.25f, 0.01f);
    CHECK_NEAR(depth[3], 1.0f, 0.01f);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mgl_target_destroy(&a);
    mgl_target_destroy(&b);
}

// two blits into a depth-stencil renderbuffer: the first must survive the second
GPU_TEST(dsa_rules, depth_blits_into_a_renderbuffer_accumulate)
{
    GLuint fb[2], rb[4];
    GLfloat depth[3 * 2];

    glGenFramebuffers(2, fb);
    glGenRenderbuffers(4, rb);

    for (int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fb[i]);
        glBindRenderbuffer(GL_RENDERBUFFER, rb[2 * i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 3, 2);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb[2 * i]);
        glBindRenderbuffer(GL_RENDERBUFFER, rb[2 * i + 1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 3, 2);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[2 * i + 1]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glClearDepth(0.5);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBlitNamedFramebuffer(fb[0], fb[1], 0, 0, 1, 1, 0, 0, 1, 1, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glClearDepth(0.25);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBlitNamedFramebuffer(fb[0], fb[1], 0, 0, 1, 1, 1, 0, 2, 1, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[1]);
    glReadPixels(0, 0, 3, 2, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());
    CHECK_NEAR(depth[0], 0.5f, 0.01f);
    CHECK_NEAR(depth[1], 0.25f, 0.01f);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(4, rb);
    glDeleteFramebuffers(2, fb);
}

GPU_TEST(dsa_rules, second_round_of_dsa_errors)
{
    GLint max_bindings = 0, max_size = 0;
    GLuint buf, fb, rb, t1d, t2d, ms, rect, cube, vao;
    GLint v = 0;
    GLubyte px[4] = { 0 };

    glGetIntegerv(GL_MAX_VERTEX_ATTRIB_BINDINGS, &max_bindings);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);

    glCreateBuffers(1, &buf);
    glNamedBufferData(buf, 64, NULL, GL_STATIC_DRAW);
    glGetNamedBufferParameteriv(buf, GL_TEXTURE_2D, &v);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    // depth-stencil has no one component type
    glCreateFramebuffers(1, &fb);
    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorage(rb, GL_DEPTH24_STENCIL8, 4, 4);
    glNamedFramebufferRenderbuffer(fb, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_DEPTH_STENCIL_ATTACHMENT,
                                               GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &v);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glNamedFramebufferRenderbuffer(fb, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rb);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glNamedFramebufferTexture(fb, GL_COLOR_ATTACHMENT0, 999777, 0);
    CHECK_EQ_UINT(GL_INVALID_VALUE, mgl_drain_errors());

    // each DSA call takes only its own kinds of texture
    glCreateTextures(GL_TEXTURE_1D, 1, &t1d);
    glCreateTextures(GL_TEXTURE_2D, 1, &t2d);
    glTextureStorage1D(t2d, 1, GL_RGBA8, 4);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());
    glTextureStorage2D(t2d, 1, GL_RGBA8, 4, 4);
    glCopyTextureSubImage1D(t2d, 0, 0, 0, 0, 1);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glTextureSubImage2D(t2d, (GLint)ilog2_for_test(max_size) + 1, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(GL_INVALID_VALUE, mgl_drain_errors());

    glGetTextureLevelParameteriv(t2d, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &v);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    // multisample textures have no sampler state; rectangles do not repeat
    glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &ms);
    glTextureParameteri(ms, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glCreateTextures(GL_TEXTURE_RECTANGLE, 1, &rect);
    glTextureParameteri(rect, GL_TEXTURE_WRAP_S, GL_REPEAT);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());
    glTextureParameteri(rect, GL_TEXTURE_BASE_LEVEL, 1);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    // a cube with one face is not a cube to read back
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    {
        GLubyte out[6 * 4];
        glGetTextureImage(cube, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof out, out);
        CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());
    }

    // BGRA is for bytes and packed words, and the binding count has a limit
    glCreateVertexArrays(1, &vao);
    glVertexArrayAttribFormat(vao, 0, GL_BGRA, GL_FLOAT, GL_TRUE, 0);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());
    {
        GLuint bufs[2] = { buf, buf };
        GLintptr offs[2] = { 0, 0 };
        GLsizei strides[2] = { 16, 16 };
        glVertexArrayVertexBuffers(vao, (GLuint)max_bindings - 1, 2, bufs, offs, strides);
        CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &cube);
    glDeleteTextures(1, &rect);
    glDeleteTextures(1, &ms);
    glDeleteTextures(1, &t2d);
    glDeleteTextures(1, &t1d);
    glDeleteRenderbuffers(1, &rb);
    glDeleteFramebuffers(1, &fb);
    glDeleteBuffers(1, &buf);
}
