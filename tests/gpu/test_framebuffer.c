/*
 * test_framebuffer.c
 * MGL
 *
 * Renderbuffers and the direct state access framebuffer surface. Almost all of
 * this used to abort on call.
 */

#include "mgl_test.h"
#include "harness.h"

#define FW 64
#define FH 64

/* ---------- renderbuffers ---------- */

GPU_TEST(rbo, storage_and_query_roundtrip)
{
    GLuint rb = 0;
    GLint w = 0, h = 0, fmt = 0;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &w);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &h);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &fmt);

    CHECK_EQ_INT(w, FW);
    CHECK_EQ_INT(h, FH);
    CHECK_EQ_UINT(fmt, GL_RGBA8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteRenderbuffers(1, &rb);
}

GPU_TEST(rbo, query_before_storage_is_legal)
{
    GLuint rb = 0;
    GLint w = -1;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);

    // no storage yet, so the image is 0x0 rather than an error
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &w);
    CHECK_EQ_INT(w, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteRenderbuffers(1, &rb);
}

GPU_TEST(rbo, depth_and_stencil_sizes_report)
{
    GLuint rb = 0;
    GLint d = -1, sten = -1;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, FW, FH);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE, &d);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE, &sten);

    CHECK_MSG(d >= 0, "depth size came back %d", d);
    CHECK_MSG(sten >= 0, "stencil size came back %d", sten);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteRenderbuffers(1, &rb);
}

GPU_TEST(rbo, respecifying_storage_replaces_the_image)
{
    GLuint rb = 0;
    GLint w = 0;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);

    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 48, 48);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &w);
    CHECK_EQ_INT(w, 48);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteRenderbuffers(1, &rb);
}

GPU_TEST(rbo, bad_target_and_size_error)
{
    GLuint rb = 0;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);

    glRenderbufferStorage(GL_TEXTURE_2D, GL_RGBA8, 16, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, -1, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteRenderbuffers(1, &rb);
}

GPU_TEST(rbo, attaches_to_a_framebuffer_and_clears)
{
    GLuint fbo = 0, rb = 0;
    unsigned char px[16 * 16 * 4];

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &rb);
        SKIP("renderbuffer attachment not complete yet");
    }

    glViewport(0, 0, 16, 16);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG(px[1] > 200, "renderbuffer clear gave G = %d, want green", px[1]);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rb);
}

GPU_TEST(rbo, delete_unbinds_and_detaches)
{
    GLuint fbo = 0, rb = 0;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);

    glDeleteRenderbuffers(1, &rb);
    CHECK_MSG(glIsRenderbuffer(rb) == GL_FALSE, "renderbuffer %u survived delete", rb);

    // the framebuffer must still be usable with the attachment gone
    glCheckFramebufferStatus(GL_FRAMEBUFFER);
    mgl_drain_errors();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}

/* ---------- DSA renderbuffers ---------- */

GPU_TEST(dsa_rbo, create_storage_and_query_without_binding)
{
    GLuint rb = 0;
    GLint w = 0, h = 0;

    glCreateRenderbuffers(1, &rb);
    CHECK_MSG(rb != 0, "glCreateRenderbuffers gave 0");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glNamedRenderbufferStorage(rb, GL_RGBA8, 32, 24);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_WIDTH, &w);
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_HEIGHT, &h);

    CHECK_EQ_INT(w, 32);
    CHECK_EQ_INT(h, 24);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteRenderbuffers(1, &rb);
}

GPU_TEST(dsa_rbo, unknown_name_errors)
{
    GLint w = 0;

    glNamedRenderbufferStorage(9999, GL_RGBA8, 16, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetNamedRenderbufferParameteriv(9999, GL_RENDERBUFFER_WIDTH, &w);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(dsa_rbo, multisample_storage_accepted)
{
    GLuint rb = 0;
    GLint samples = -1;

    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorageMultisample(rb, 4, GL_RGBA8, 32, 32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_SAMPLES, &samples);
    CHECK_MSG(samples >= 0, "samples came back %d", samples);

    glDeleteRenderbuffers(1, &rb);
}

/* ---------- DSA framebuffers ---------- */

GPU_TEST(dsa_fbo, create_attach_and_check_without_binding)
{
    GLuint fbo = 0, tex = 0;
    GLenum status;

    glCreateFramebuffers(1, &fbo);
    CHECK_MSG(fbo != 0, "glCreateFramebuffers gave 0");

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);

    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
    CHECK_MSG(status == GL_FRAMEBUFFER_COMPLETE,
              "named framebuffer status 0x%x, want COMPLETE", status);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(dsa_fbo, binding_is_left_alone_by_dsa_calls)
{
    GLuint a = 0, b = 0, tex = 0;
    GLint bound = -1;

    glGenFramebuffers(1, &a);
    glCreateFramebuffers(1, &b);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);

    glBindFramebuffer(GL_FRAMEBUFFER, a);

    // operating on b must not disturb the binding to a
    glNamedFramebufferTexture(b, GL_COLOR_ATTACHMENT0, tex, 0);
    glCheckNamedFramebufferStatus(b, GL_FRAMEBUFFER);

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound);
    CHECK_MSG(bound == (GLint)a, "binding moved to %d, expected %u", bound, a);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &a);
    glDeleteFramebuffers(1, &b);
    glDeleteTextures(1, &tex);
    mgl_drain_errors();
}

GPU_TEST(dsa_fbo, named_renderbuffer_attachment)
{
    GLuint fbo = 0, rb = 0;

    glCreateFramebuffers(1, &fbo);
    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorage(rb, GL_RGBA8, 32, 32);

    glNamedFramebufferRenderbuffer(fbo, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rb);
}

GPU_TEST(dsa_fbo, parameters_roundtrip)
{
    GLuint fbo = 0;
    GLint v = 0;

    glCreateFramebuffers(1, &fbo);

    glNamedFramebufferParameteri(fbo, GL_FRAMEBUFFER_DEFAULT_WIDTH, 128);
    glNamedFramebufferParameteri(fbo, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 96);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedFramebufferParameteriv(fbo, GL_FRAMEBUFFER_DEFAULT_WIDTH, &v);
    CHECK_EQ_INT(v, 128);

    glGetNamedFramebufferParameteriv(fbo, GL_FRAMEBUFFER_DEFAULT_HEIGHT, &v);
    CHECK_EQ_INT(v, 96);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glNamedFramebufferParameteri(fbo, GL_FRAMEBUFFER_DEFAULT_WIDTH, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteFramebuffers(1, &fbo);
}

GPU_TEST(dsa_fbo, draw_and_read_buffer_selection)
{
    GLuint fbo = 0, tex = 0;
    static const GLenum bufs[] = { GL_COLOR_ATTACHMENT0 };

    glCreateFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

    glNamedFramebufferDrawBuffer(fbo, GL_COLOR_ATTACHMENT0);
    mgl_drain_errors();

    glNamedFramebufferDrawBuffers(fbo, 1, bufs);
    mgl_drain_errors();

    glNamedFramebufferReadBuffer(fbo, GL_COLOR_ATTACHMENT0);
    mgl_drain_errors();

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}

GPU_TEST(dsa_fbo, unknown_name_errors)
{
    GLint v = 0;

    glNamedFramebufferParameteri(9999, GL_FRAMEBUFFER_DEFAULT_WIDTH, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetNamedFramebufferParameteriv(9999, GL_FRAMEBUFFER_DEFAULT_WIDTH, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- invalidate ---------- */

GPU_TEST(fbo_invalidate, accepts_valid_input)
{
    MGLTestTarget t;
    static const GLenum att[] = { GL_COLOR_ATTACHMENT0 };

    if (!mgl_target_create(&t, FW, FH, GL_RGBA8, 0)) SKIP("no target");

    mgl_target_bind(&t);

    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glInvalidateSubFramebuffer(GL_FRAMEBUFFER, 1, att, 0, 0, 8, 8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    mgl_target_destroy(&t);
}

GPU_TEST(fbo_invalidate, rejects_bad_input)
{
    static const GLenum att[] = { GL_COLOR_ATTACHMENT0 };

    glInvalidateFramebuffer(0x9999, 1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glInvalidateFramebuffer(GL_FRAMEBUFFER, -1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glInvalidateSubFramebuffer(GL_FRAMEBUFFER, 1, att, 0, 0, -1, 8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- attachment queries ---------- */

GPU_TEST(fbo_query, attachment_object_type_and_name)
{
    MGLTestTarget t;
    GLint type = 0, name = 0;

    if (!mgl_target_create(&t, FW, FH, GL_RGBA8, 0)) SKIP("no target");

    mgl_target_bind(&t);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);

    CHECK_MSG(type == GL_TEXTURE, "attachment type 0x%x, want GL_TEXTURE", type);
    CHECK_MSG(name == (GLint)t.color, "attachment name %d, want %u", name, t.color);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    mgl_target_destroy(&t);
}

GPU_TEST(fbo_query, default_framebuffer_attachment_is_queryable)
{
    GLint type = -1;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);

    CHECK_MSG(type == GL_FRAMEBUFFER_DEFAULT, "default attachment type 0x%x", type);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(fbo_query, framebuffer_parameters_on_bound_object)
{
    GLuint fbo = 0;
    GLint v = 0;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 64);
    glGetFramebufferParameteriv(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, &v);

    CHECK_EQ_INT(v, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFramebufferParameteriv(GL_FRAMEBUFFER, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}

GPU_TEST(fbo_query, bad_framebuffer_target_errors)
{
    GLint v = 0;

    glGetFramebufferParameteriv(0x9999, GL_FRAMEBUFFER_DEFAULT_WIDTH, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glFramebufferParameteri(0x9999, GL_FRAMEBUFFER_DEFAULT_WIDTH, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(fbo_query, bad_texture_target_errors_not_aborts)
{
    GLuint fbo = 0, tex = 0;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    // GL_TEXTURE_3D is not a legal textarget for FramebufferTexture2D
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, tex, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}

/* Two textures take turns on one framebuffer; each keeps what it was given. */
GPU_TEST(framebuffer, swapped_attachments_keep_their_contents)
{
    GLuint tex[2], fbo = 0;

    glGenTextures(2, tex);

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 8, 8);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[0], 0);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[1], 0);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    GLubyte px[4];

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[0], 0);
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_MSG(px[0] == 255 && px[1] == 0, "first texture reads %u %u %u", px[0], px[1], px[2]);

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[1], 0);
    glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_MSG(px[0] == 0 && px[1] == 255, "second texture reads %u %u %u", px[0], px[1], px[2]);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(2, tex);
}

/* The same with a draw rather than a clear, the way the base vertex tests do it. */
GPU_TEST(framebuffer, swapped_attachments_keep_what_was_drawn)
{
    static const char *vs =
        "#version 430 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 430 core\n"
        "uniform vec4 c;\n"
        "out vec4 o;\n"
        "void main() { o = c; }\n";
    char log[1024] = "";
    GLuint prog = mgl_build_program(vs, fs, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    GLuint tex[2], fbo = 0, vbo = 0;
    GLuint vao = mgl_fullscreen_quad(&vbo);

    glGenTextures(2, tex);

    for (int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 8);
    }

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 8, 8);
    glUseProgram(prog);
    glClearColor(0, 0, 0, 1);

    for (int i = 0; i < 2; i++)
    {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUniform4f(glGetUniformLocation(prog, "c"), i ? 0.0f : 1.0f, i ? 1.0f : 0.0f, 0.0f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    GLubyte px[4];

    for (int i = 0; i < 2; i++)
    {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
        glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        CHECK_MSG(px[i ? 1 : 0] == 255 && px[i ? 0 : 1] == 0,
                  "texture %d reads %u %u %u", i, px[0], px[1], px[2]);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(2, tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

/* ---------- completeness is actually worked out ---------- */

// glCheckFramebufferStatus answered COMPLETE for everything, and read colour
// attachment 0's texture whether or not one was there.

static GLuint tex2D(GLenum internalformat, GLsizei w, GLsizei h)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexStorage2D(GL_TEXTURE_2D, 1, internalformat, w, h);

    return t;
}

GPU_TEST(framebuffer_status, nothing_attached_is_missing_an_attachment)
{
    GLuint fbo = 0;

    glCreateFramebuffers(1, &fbo);
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER),
                  GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);

    // unless the defaults say how big it is
    glNamedFramebufferParameteri(fbo, GL_FRAMEBUFFER_DEFAULT_WIDTH, 16);
    glNamedFramebufferParameteri(fbo, GL_FRAMEBUFFER_DEFAULT_HEIGHT, 16);
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glDeleteFramebuffers(1, &fbo);
}

GPU_TEST(framebuffer_status, a_depth_image_in_a_colour_slot_is_incomplete)
{
    GLuint fbo = 0, depth = tex2D(GL_DEPTH_COMPONENT24, 16, 16);

    glCreateFramebuffers(1, &fbo);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, depth, 0);
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER),
                  GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &depth);
}

GPU_TEST(framebuffer_status, a_layer_past_the_end_is_incomplete)
{
    GLuint fbo = 0, arr = 0;

    glGenTextures(1, &arr);
    glBindTexture(GL_TEXTURE_2D_ARRAY, arr);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 16, 16, 2);

    glCreateFramebuffers(1, &fbo);
    glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, arr, 0, 1);
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, arr, 0, 2);
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER),
                  GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &arr);
}

GPU_TEST(framebuffer_status, a_layered_and_a_flat_attachment_do_not_mix)
{
    GLuint fbo = 0, arr = 0, flat = tex2D(GL_RGBA8, 16, 16);

    glGenTextures(1, &arr);
    glBindTexture(GL_TEXTURE_2D_ARRAY, arr);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 16, 16, 2);

    glCreateFramebuffers(1, &fbo);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, arr, 0);   // every layer
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT1, flat, 0);
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER),
                  GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &arr);
    glDeleteTextures(1, &flat);
}

GPU_TEST(framebuffer_status, clearing_an_incomplete_framebuffer_is_an_error_not_a_crash)
{
    GLuint fbo = 0;
    static const GLfloat red[4] = { 1, 0, 0, 1 };

    glCreateFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glClear(GL_COLOR_BUFFER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_FRAMEBUFFER_OPERATION);

    glClearBufferfv(GL_COLOR, 0, red);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_FRAMEBUFFER_OPERATION);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}

// Metal pairs a packed depth-stencil image only with itself, so depth from
// one image and stencil from another went on to a pipeline Metal's own
// validation aborts the process over. GL lets the driver answer UNSUPPORTED,
// and a draw into it has to be refused.
GPU_TEST(framebuffer_status, depth_and_stencil_from_different_images_are_unsupported)
{
    GLuint fbo = 0, rb[3] = { 0 }, vao, vbo, prog;
    char log[512];

    prog = mgl_build_program("#version 430 core\nlayout(location = 0) in vec2 p;\n"
                             "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n",
                             "#version 430 core\nout vec4 o;\nvoid main() { o = vec4(1.0); }\n",
                             log, sizeof log);

    glGenRenderbuffers(3, rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb[0]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
    glBindRenderbuffer(GL_RENDERBUFFER, rb[1]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 16, 16);
    glBindRenderbuffer(GL_RENDERBUFFER, rb[2]);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 16, 16);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb[0]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[1]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb[2]);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_UNSUPPORTED);

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glViewport(0, 0, 16, 16);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_FRAMEBUFFER_OPERATION);

    // the same packed image in both slots is fine
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[1]);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(3, rb);
}

// A renderbuffer deleted while attached to a framebuffer that is not bound
// was freed and left behind in that framebuffer, and the next status check
// read through the dangling pointer.
GPU_TEST(framebuffer_status, deleting_an_attached_renderbuffer_leaves_no_dangling_image)
{
    GLuint fbo = 0, rb = 0;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &rb);

    // allocations that would reuse the freed memory
    for (int i = 0; i < 64; i++)
    {
        GLuint t = 0;

        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
        glDeleteTextures(1, &t);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE);
    mgl_drain_errors();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
}
