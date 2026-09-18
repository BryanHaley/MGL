/*
 * test_framebuffer_dsa.c
 * MGL
 *
 * Named framebuffer objects, typed clears, blits, and the framebuffer
 * texture attachment variants that have no test coverage yet.
 */

#include "mgl_test.h"
#include "harness.h"

#define FW 64
#define FH 64

/* ---------- framebuffer texture attachment variants ---------- */

GPU_TEST(framebuffer_dsa, texture_attachment_variants)
{
    GLuint fbo = 0, tex = 0;
    GLint type = 0, name = 0;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);

    /* glFramebufferTexture — no textarget, uses the texture's own target */
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    CHECK_EQ_INT(type, GL_TEXTURE);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
    CHECK_EQ_INT(name, (GLint)tex);

    /* detach so the next test can attach */
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glFramebufferTexture1D — textarget must be GL_TEXTURE_1D */
    glFramebufferTexture1D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_1D, tex, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* glFramebufferTexture3D — textarget must be GL_TEXTURE_3D */
    glFramebufferTexture3D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, tex, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* glFramebufferTextureLayer — attaches a single layer */
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    CHECK_EQ_INT(type, GL_TEXTURE);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &name);
    CHECK_EQ_INT(name, 0);

    /* glNamedFramebufferTextureLayer — DSA variant */
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glNamedFramebufferTextureLayer(fbo, GL_COLOR_ATTACHMENT0, tex, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
    CHECK_EQ_INT(name, (GLint)tex);

    /* invalid framebuffer name for the DSA variant */
    glNamedFramebufferTextureLayer(9999, GL_COLOR_ATTACHMENT0, tex, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}

/* ---------- glDrawBuffers ---------- */

GPU_TEST(framebuffer_dsa, draw_buffers)
{
    GLuint fbo = 0, tex = 0;
    static const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_NONE };

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex, 0);

    /* set multiple draw buffers */
    glDrawBuffers(2, bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* GL_BACK is a real buffer name, just not one an FBO has (GL 4.6 17.4.1) */
    static const GLenum bad_bufs[] = { GL_BACK };
    glDrawBuffers(1, bad_bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* GL_FRONT is also invalid for an FBO */
    static const GLenum bad_bufs2[] = { GL_FRONT };
    glDrawBuffers(1, bad_bufs2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* negative n */
    glDrawBuffers(-1, bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}

/* ---------- glClearBufferfi ---------- */

GPU_TEST(framebuffer_dsa, clear_bufferfi)
{
    GLuint fbo = 0, rb = 0;

    /* need a depth-stencil attachment */
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, FW, FH);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, rb);

    /* valid call — buffer must be GL_DEPTH_STENCIL */
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.5f, 7);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* buffer not GL_DEPTH_STENCIL */
    glClearBufferfi(GL_COLOR, 0, 0.5f, 7);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glClearBufferfi(GL_DEPTH, 0, 0.5f, 7);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* drawbuffer must be 0 */
    glClearBufferfi(GL_DEPTH_STENCIL, 1, 0.5f, 7);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rb);
}

/* ---------- glClearNamedFramebuffer* family ---------- */

GPU_TEST(framebuffer_dsa, named_clear)
{
    GLuint fbo = 0, tex = 0;
    const GLfloat clear_f[] = { 0.25f, 0.5f, 0.75f, 1.0f };
    const GLint   clear_i[] = { 1, 2, 3, 4 };
    const GLuint  clear_u[] = { 5, 6, 7, 8 };

    glCreateFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

    /* glClearNamedFramebufferfv */
    glClearNamedFramebufferfv(fbo, GL_COLOR, 0, clear_f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glClearNamedFramebufferiv */
    glClearNamedFramebufferiv(fbo, GL_COLOR, 0, clear_i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glClearNamedFramebufferuiv */
    glClearNamedFramebufferuiv(fbo, GL_COLOR, 0, clear_u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glClearNamedFramebufferfi — needs depth-stencil */
    glClearNamedFramebufferfi(fbo, GL_DEPTH_STENCIL, 0, 0.5f, 7);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* invalid framebuffer name */
    glClearNamedFramebufferfv(9999, GL_COLOR, 0, clear_f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glClearNamedFramebufferiv(9999, GL_COLOR, 0, clear_i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glClearNamedFramebufferuiv(9999, GL_COLOR, 0, clear_u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glClearNamedFramebufferfi(9999, GL_DEPTH_STENCIL, 0, 0.5f, 7);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* framebuffer 0 (default) is valid */
    glClearNamedFramebufferfv(0, GL_COLOR, 0, clear_f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}

/* ---------- glInvalidateNamedFramebufferData / SubData ---------- */

GPU_TEST(framebuffer_dsa, named_invalidate)
{
    GLuint fbo = 0, tex = 0;
    static const GLenum att[] = { GL_COLOR_ATTACHMENT0 };

    glCreateFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

    /* glInvalidateNamedFramebufferData with a valid FBO */
    glInvalidateNamedFramebufferData(fbo, 1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glInvalidateNamedFramebufferData with framebuffer 0 (default) */
    glInvalidateNamedFramebufferData(0, 1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glInvalidateNamedFramebufferSubData with valid FBO */
    glInvalidateNamedFramebufferSubData(fbo, 1, att, 0, 0, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glInvalidateNamedFramebufferSubData with framebuffer 0 */
    glInvalidateNamedFramebufferSubData(0, 1, att, 0, 0, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* invalid framebuffer name */
    glInvalidateNamedFramebufferData(9999, 1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glInvalidateNamedFramebufferSubData(9999, 1, att, 0, 0, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* negative numAttachments */
    glInvalidateNamedFramebufferData(fbo, -1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glInvalidateNamedFramebufferSubData(fbo, -1, att, 0, 0, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* negative width or height */
    glInvalidateNamedFramebufferSubData(fbo, 1, att, 0, 0, -1, FW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glInvalidateNamedFramebufferSubData(fbo, 1, att, 0, 0, FW, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* NULL attachments with non-zero count */
    glInvalidateNamedFramebufferData(fbo, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glInvalidateNamedFramebufferSubData(fbo, 1, NULL, 0, 0, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}

/* ---------- glBlitNamedFramebuffer ---------- */

GPU_TEST(framebuffer_dsa, named_blit)
{
    GLuint read_fbo = 0, draw_fbo = 0, tex = 0;
    unsigned char src_px[FW * FH * 4];
    unsigned char dst_px[FW * FH * 4];


    /* two FBOs: read from one, draw to the other */
    glCreateFramebuffers(1, &read_fbo);
    glCreateFramebuffers(1, &draw_fbo);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_RGBA8, FW, FH);
    glNamedFramebufferTexture(read_fbo, GL_COLOR_ATTACHMENT0, tex, 0);
    glNamedFramebufferTexture(draw_fbo, GL_COLOR_ATTACHMENT0, tex, 1);

    /* check both are complete */
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(read_fbo, GL_FRAMEBUFFER),
                  GL_FRAMEBUFFER_COMPLETE);
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(draw_fbo, GL_FRAMEBUFFER),
                  GL_FRAMEBUFFER_COMPLETE);

    /* draw a red quad into read_fbo */
    glBindFramebuffer(GL_FRAMEBUFFER, read_fbo);
    glViewport(0, 0, FW, FH);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, FW, FH, GL_RGBA, GL_UNSIGNED_BYTE, src_px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(src_px[0] > 200, "source clear gave R = %d, want red", src_px[0]);

    /* blit from read_fbo to draw_fbo */
    glBlitNamedFramebuffer(read_fbo, draw_fbo,
                           0, 0, FW, FH, 0, 0, FW, FH,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* read back from draw_fbo level 1 */
    glBindFramebuffer(GL_FRAMEBUFFER, draw_fbo);
    glReadPixels(0, 0, FW, FH, GL_RGBA, GL_UNSIGNED_BYTE, dst_px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(dst_px[0] > 200, "blit destination gave R = %d, want red", dst_px[0]);

    /* invalid read framebuffer name */
    glBlitNamedFramebuffer(9999, draw_fbo,
                           0, 0, FW, FH, 0, 0, FW, FH,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* invalid draw framebuffer name */
    glBlitNamedFramebuffer(read_fbo, 9999,
                           0, 0, FW, FH, 0, 0, FW, FH,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* invalid filter */
    glBlitNamedFramebuffer(read_fbo, draw_fbo,
                           0, 0, FW, FH, 0, 0, FW, FH,
                           GL_COLOR_BUFFER_BIT, GL_LINEAR_MIPMAP_LINEAR);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* invalid mask */
    glBlitNamedFramebuffer(read_fbo, draw_fbo,
                           0, 0, FW, FH, 0, 0, FW, FH,
                           0xDEAD, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &read_fbo);
    glDeleteFramebuffers(1, &draw_fbo);
    glDeleteTextures(1, &tex);
}

/* ---------- glGetNamedFramebufferAttachmentParameteriv ---------- */

GPU_TEST(framebuffer_dsa, named_attachment_query)
{
    GLuint fbo = 0, tex = 0;
    GLint type = 0, name = 0, level = 0;

    glCreateFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

    /* query through the DSA entry point */
    glGetNamedFramebufferAttachmentParameteriv(fbo, GL_COLOR_ATTACHMENT0,
                                                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(type, GL_TEXTURE);

    glGetNamedFramebufferAttachmentParameteriv(fbo, GL_COLOR_ATTACHMENT0,
                                                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
    CHECK_EQ_INT(name, (GLint)tex);

    glGetNamedFramebufferAttachmentParameteriv(fbo, GL_COLOR_ATTACHMENT0,
                                                GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level);
    CHECK_EQ_INT(level, 0);

    /* query the default framebuffer (0) — must accept it */
    glGetNamedFramebufferAttachmentParameteriv(0, GL_BACK_LEFT,
                                                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(type, GL_FRAMEBUFFER_DEFAULT);

    /* invalid framebuffer name */
    glGetNamedFramebufferAttachmentParameteriv(9999, GL_COLOR_ATTACHMENT0,
                                                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* invalid attachment for a named FBO */
    glGetNamedFramebufferAttachmentParameteriv(fbo, GL_BACK_LEFT,
                                                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
}

/* ---------- glRenderbufferStorageMultisample ---------- */

GPU_TEST(framebuffer_dsa, renderbuffer_multisample)
{
    GLuint rb = 0;
    GLint samples = -1;

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);

    /* valid call */
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, 32, 32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &samples);
    CHECK_MSG(samples >= 0, "samples came back %d", samples);

    /* invalid target */
    glRenderbufferStorageMultisample(GL_TEXTURE_2D, 4, GL_RGBA8, 32, 32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* negative width */
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, -1, 32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* negative height */
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, 32, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteRenderbuffers(1, &rb);
}
