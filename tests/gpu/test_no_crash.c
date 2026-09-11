/*
 * test_no_crash.c
 * MGL
 *
 * Calls the paths that used to abort the process. Each one must set a GL error
 * and return. If any of these regress the whole run dies, which is the point.
 */

#include "mgl_test.h"
#include "harness.h"
#include "MGLContext.h"

/* ---------- bad enums into lookup tables ---------- */

GPU_TEST(nocrash, bad_texture_target)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);

    glBindTexture(0x9999, tex);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glTexParameteri(0x9999, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);

    glDeleteTextures(1, &tex);
}

GPU_TEST(nocrash, bad_pixel_format_and_type)
{
    unsigned char buf[64];

    glReadPixels(0, 0, 1, 1, 0x9999, GL_UNSIGNED_BYTE, buf);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);

    glReadPixels(0, 0, 1, 1, GL_RGBA, 0x9999, buf);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);
}

GPU_TEST(nocrash, bad_element_type_in_draw)
{
    // getTypeSize used to abort on anything but SHORT/INT
    glDrawElements(GL_TRIANGLES, 3, 0x9999, NULL);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);

    glDrawElements(GL_TRIANGLES, 3, GL_FLOAT, NULL);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);
}

GPU_TEST(nocrash, mglget_with_unknown_param)
{
    GLuint v = 0xABCD;

    // MGL's own query, not GL; an unknown param must simply do nothing
    MGLget(NULL, 0x9999, &v);
    CHECK_EQ_UINT(v, 0xABCD);
}

/* ---------- sampler parameter getters ---------- */

GPU_TEST(nocrash, sampler_integer_getters)
{
    GLuint s = 0;
    GLint iv[4] = { 0 };
    GLuint uv[4] = { 0 };

    glGenSamplers(1, &s);
    glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glGetSamplerParameterIiv(s, GL_TEXTURE_MIN_FILTER, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(iv[0], GL_NEAREST);

    glGetSamplerParameterIuiv(s, GL_TEXTURE_MIN_FILTER, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(uv[0], GL_NEAREST);

    glGetSamplerParameterIiv(9999, GL_TEXTURE_MIN_FILTER, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteSamplers(1, &s);
}

/* ---------- vertex array queries ---------- */

GPU_TEST(vao_query, element_buffer_binding)
{
    GLuint vao = 0, ibo = 0;
    GLint v = -1;
    static const unsigned short idx[] = { 0, 1, 2 };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);

    glGetVertexArrayiv(vao, GL_ELEMENT_ARRAY_BUFFER_BINDING, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, (GLint)ibo);

    glGetVertexArrayiv(vao, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &ibo);
}

GPU_TEST(vao_query, indexed_attribute_state)
{
    GLuint vao = 0, vbo = 0;
    GLint v = -1;
    static const float verts[] = { 0, 0, 1, 0, 1, 1 };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glGetVertexArrayIndexediv(vao, 0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &v);
    CHECK_EQ_INT(v, GL_TRUE);

    glGetVertexArrayIndexediv(vao, 0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &v);
    CHECK_EQ_INT(v, 2);

    glGetVertexArrayIndexediv(vao, 0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &v);
    CHECK_EQ_UINT(v, GL_FLOAT);

    glGetVertexArrayIndexediv(vao, 0, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &v);
    CHECK_EQ_INT(v, GL_FALSE);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexArrayIndexediv(vao, 9999, GL_VERTEX_ATTRIB_ARRAY_SIZE, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetVertexArrayIndexediv(vao, 0, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

GPU_TEST(vao_query, indexed64_takes_only_binding_offset)
{
    GLuint vao = 0;
    GLint64 v = -1;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGetVertexArrayIndexed64iv(vao, 0, GL_VERTEX_BINDING_OFFSET, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexArrayIndexed64iv(vao, 0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- compute ---------- */

GPU_TEST(nocrash, dispatch_compute_indirect_validates)
{
    GLuint b = 0;

    // no indirect buffer bound
    glDispatchComputeIndirect(0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGenBuffers(1, &b);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, b);
    glBufferData(GL_DISPATCH_INDIRECT_BUFFER, 64, NULL, GL_STATIC_DRAW);

    glDispatchComputeIndirect(-4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDispatchComputeIndirect(3);          // not 4 byte aligned
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDispatchComputeIndirect(1024);       // past the end
    CHECK(mgl_drain_errors() != GL_NO_ERROR);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

/* ---------- a broad sweep of nonsense arguments ---------- */

GPU_TEST(nocrash, sweep_of_bad_enums)
{
    GLint iv = 0;
    GLfloat fv = 0.0f;
    GLuint tmp = 0;

    glEnable(0x9999);          mgl_drain_errors();
    glDisable(0x9999);         mgl_drain_errors();
    glIsEnabled(0x9999);       mgl_drain_errors();
    glGetIntegerv(0x9999, &iv);   mgl_drain_errors();
    glGetFloatv(0x9999, &fv);     mgl_drain_errors();
    glBlendFunc(0x9999, 0x9999);  mgl_drain_errors();
    glBlendEquation(0x9999);      mgl_drain_errors();
    glDepthFunc(0x9999);          mgl_drain_errors();
    glCullFace(0x9999);           mgl_drain_errors();
    glFrontFace(0x9999);          mgl_drain_errors();
    glHint(0x9999, 0x9999);       mgl_drain_errors();
    glBindBuffer(0x9999, 0);      mgl_drain_errors();
    glBindFramebuffer(0x9999, 0); mgl_drain_errors();
    glBindRenderbuffer(0x9999, 0); mgl_drain_errors();
    glReadBuffer(0x9999);         mgl_drain_errors();
    glDrawBuffer(0x9999);         mgl_drain_errors();
    glGenBuffers(1, &tmp);
    glDeleteBuffers(1, &tmp);

    // getting here at all is the assertion
    CHECK(1);
}

GPU_TEST(nocrash, sweep_of_unbound_objects)
{
    GLint iv = 0;

    // every one of these operates with nothing bound
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); mgl_drain_errors();
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iv); mgl_drain_errors();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);          mgl_drain_errors();
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &iv); mgl_drain_errors();
    glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, GL_MAP_READ_BIT);         mgl_drain_errors();
    glUnmapBuffer(GL_ARRAY_BUFFER);                                    mgl_drain_errors();
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 16); mgl_drain_errors();

    CHECK(1);
}

/* ---------- paths that used to assert inside the Metal layer ---------- */

GPU_TEST(nocrash, indexed_draw_without_an_element_buffer)
{
    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    mgl_drain_errors();

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glDrawElementsInstanced(GL_TRIANGLES, 3, GL_UNSIGNED_INT, NULL, 1);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glDeleteVertexArrays(1, &vao);
}

/* the spec says a bad index type is GL_INVALID_ENUM, not GL_INVALID_VALUE */
GPU_TEST(nocrash, bad_index_type_is_invalid_enum)
{
    GLuint vao = 0, ebo = 0;
    GLuint idx[3] = { 0, 1, 2 };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
    mgl_drain_errors();

    glDrawElements(GL_TRIANGLES, 3, GL_FLOAT, NULL);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glDrawElements(GL_TRIANGLES, 3, 0x9999, NULL);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    /* GL_UNSIGNED_BYTE is a legal enum even where Metal cannot draw it, so it
       must not come back as GL_INVALID_ENUM */
    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, NULL);
    CHECK(mgl_drain_errors() != GL_INVALID_ENUM);

    glDeleteBuffers(1, &ebo);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(nocrash, indirect_draw_without_an_indirect_buffer)
{
    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    mgl_drain_errors();

    glDrawArraysIndirect(GL_TRIANGLES, NULL);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, mgl_drain_errors());

    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, NULL);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(nocrash, framebuffer_texture_1d_and_3d_reject_wrong_targets)
{
    GLuint fbo = 0, tex = 0;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &tex);
    mgl_drain_errors();

    glFramebufferTexture1D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0, 0);
    CHECK_EQ_UINT(GL_INVALID_ENUM, mgl_drain_errors());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}

GPU_TEST(nocrash, gen_objects_with_a_null_array)
{
    glGenFramebuffers(1, NULL);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);

    glGenRenderbuffers(1, NULL);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);
}
