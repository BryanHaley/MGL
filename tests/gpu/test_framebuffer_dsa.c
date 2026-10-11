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
    static const GLenum win[] = { GL_COLOR };

    glCreateFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FW, FH);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);

    /* glInvalidateNamedFramebufferData with a valid FBO */
    glInvalidateNamedFramebufferData(fbo, 1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glInvalidateNamedFramebufferData with framebuffer 0 (default): the
       window has COLOR, DEPTH and STENCIL, and no attachment points */
    glInvalidateNamedFramebufferData(0, 1, win);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glInvalidateNamedFramebufferData(0, 1, att);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* glInvalidateNamedFramebufferSubData with valid FBO */
    glInvalidateNamedFramebufferSubData(fbo, 1, att, 0, 0, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glInvalidateNamedFramebufferSubData with framebuffer 0 */
    glInvalidateNamedFramebufferSubData(0, 1, win, 0, 0, FW, FH);
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

/* ---------- DSA depth-stencil renderbuffer readback ---------- */

/* glNamedRenderbufferStorage takes the same storage path as the bound
   glRenderbufferStorage, but through the DSA entry point. A depth24_stencil8
   renderbuffer has to come back through both glReadPixels(GL_DEPTH_COMPONENT)
   and glReadPixels(GL_STENCIL_INDEX). */

GPU_TEST(framebuffer_dsa, stencil_renderbuffer_readback)
{
    static const char *vs =
        "#version 460 core\n"
        "void main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));"
        "gl_Position=vec4(p[gl_VertexID],0,1);}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main(){gl_FragDepth=0.5;o=vec4(1,0,0,1);}\n";
    GLuint rb = 0, fb = 0, vao = 0, prog = 0;
    GLubyte stencil[FW * FH];
    GLfloat depth[FW * FH];
    char log[512] = { 0 };
    int bad_stencil = 0, bad_depth = 0;

    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorage(rb, GL_DEPTH24_STENCIL8, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glCreateFramebuffers(1, &fb);
    glNamedFramebufferRenderbuffer(fb, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* a depth-stencil-only framebuffer is complete on its own (GL 4.6 9.4.2) */
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, FW, FH);

    glClearStencil(0);
    glClearDepth(1.0);
    glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "depth-stencil program did not build: %s", log);

    if (prog)
    {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glUseProgram(prog);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_ALWAYS);
        glEnable(GL_STENCIL_TEST);
        /* the triangle covers the whole viewport, so every pixel takes ref 5 */
        glStencilFunc(GL_ALWAYS, 5, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        glDrawArrays(GL_TRIANGLES, 0, 3);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);

        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        memset(stencil, 0xAA, sizeof stencil);
        glReadPixels(0, 0, FW, FH, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencil);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        for (int i = 0; i < FW * FH; i++)
            if (stencil[i] != 5)
                bad_stencil++;

        CHECK_MSG(bad_stencil == 0,
                  "%d of %d stencil texels are not the written 5 (first %u)", bad_stencil, FW * FH, stencil[0]);

        memset(depth, 0, sizeof depth);
        glReadPixels(0, 0, FW, FH, GL_DEPTH_COMPONENT, GL_FLOAT, depth);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        for (int i = 0; i < FW * FH; i++)
            if (fabs((double)depth[i] - 0.5) > 0.01)
                bad_depth++;

        CHECK_MSG(bad_depth == 0,
                  "%d of %d depth texels are not the written 0.5", bad_depth, FW * FH);

        glUseProgram(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(prog);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);
}

/* The CTS storage_multisample case attaches one packed depth-stencil
   renderbuffer to GL_DEPTH_ATTACHMENT and GL_STENCIL_ATTACHMENT as two
   separate calls, then asks the framebuffer what is on each. That layout is
   worth pinning on its own: the two attachment slots end up pointing at the
   same image. */

GPU_TEST(framebuffer_dsa, depth_and_stencil_attached_separately)
{
    GLuint rb = 0, fb = 0;
    GLint v = -1;

    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorage(rb, GL_DEPTH24_STENCIL8, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glCreateFramebuffers(1, &fb);
    glNamedFramebufferRenderbuffer(fb, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb);
    glNamedFramebufferRenderbuffer(fb, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    v = -1;
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_DEPTH_ATTACHMENT,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, GL_RENDERBUFFER);

    v = -1;
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_DEPTH_ATTACHMENT,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &v);
    CHECK_EQ_INT(v, (GLint)rb);

    v = -1;
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_STENCIL_ATTACHMENT,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &v);
    CHECK_EQ_INT(v, (GLint)rb);

    /* the packed format answers for both halves when they are one image */
    v = -1;
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_DEPTH_ATTACHMENT,
                                               GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &v);
    CHECK_EQ_INT(v, 24);

    v = -1;
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_STENCIL_ATTACHMENT,
                                               GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &v);
    CHECK_EQ_INT(v, 8);

    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);
}

/* ---------- DSA integer renderbuffer ---------- */
/* ---------- DSA integer renderbuffer ---------- */

/* An integer renderbuffer keeps integer values exactly, so the fragment
   shader's ivec4 output has to reach glReadPixels(GL_RED_INTEGER, GL_INT)
   unchanged. */

GPU_TEST(framebuffer_dsa, integer_renderbuffer_readback)
{
    static const char *vs =
        "#version 460 core\n"
        "void main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));"
        "gl_Position=vec4(p[gl_VertexID],0,1);}\n";
    static const char *fs =
        "#version 460 core\n"
        "out ivec4 o;\n"
        "void main(){o=ivec4(42,43,44,45);}\n";
    GLuint rb = 0, fb = 0, vao = 0, prog = 0;
    GLint px[FW * FH];
    const GLint clear[4] = { 0, 0, 0, 0 };
    char log[512] = { 0 };
    int bad = 0;

    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorage(rb, GL_R32I, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glCreateFramebuffers(1, &fb);
    glNamedFramebufferRenderbuffer(fb, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, FW, FH);

    glClearBufferiv(GL_COLOR, 0, clear);

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "integer program did not build: %s", log);

    if (prog)
    {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glUseProgram(prog);
        glDisable(GL_DITHER);

        glDrawArrays(GL_TRIANGLES, 0, 3);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        memset(px, 0, sizeof px);
        glReadPixels(0, 0, FW, FH, GL_RED_INTEGER, GL_INT, px);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        /* R32I holds only the red channel; 42 is what the shader wrote */
        for (int i = 0; i < FW * FH; i++)
            if (px[i] != 42)
                bad++;

        CHECK_MSG(bad == 0,
                  "%d of %d integer texels are not the written 42", bad, FW * FH);

        glEnable(GL_DITHER);
        glUseProgram(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteProgram(prog);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);
}

/* ---------- DSA multisample renderbuffer storage and query ---------- */

/* This is the shape of the direct_state_access.renderbuffers_storage_multisample
   harness case: create, give storage through the DSA entry point, query every
   renderbuffer parameter, attach it and query the attachment. */

GPU_TEST(framebuffer_dsa, renderbuffer_storage_multisample_dsa)
{
    GLuint rb = 0, fb = 0;
    GLint v = -1;

    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorageMultisample(rb, 4, GL_RGBA8, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    v = -1;
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_SAMPLES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, 4);

    v = -1;
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_WIDTH, &v);
    CHECK_EQ_INT(v, FW);

    v = -1;
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_HEIGHT, &v);
    CHECK_EQ_INT(v, FH);

    v = -1;
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_INTERNAL_FORMAT, &v);
    CHECK_EQ_INT(v, GL_RGBA8);

    glCreateFramebuffers(1, &fb);
    glNamedFramebufferRenderbuffer(fb, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    v = -1;
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, GL_RENDERBUFFER);

    v = -1;
    glGetNamedFramebufferAttachmentParameteriv(fb, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &v);
    CHECK_EQ_INT(v, (GLint)rb);

    /* a multisample renderbuffer is still a complete attachment */
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);

    /* the CTS case also re-specifies the same renderbuffer with zero samples,
       which is the non-multisample path through the same entry point */
    glCreateRenderbuffers(1, &rb);
    glNamedRenderbufferStorageMultisample(rb, 0, GL_RGBA8, FW, FH);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    v = -1;
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_SAMPLES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, 0);

    v = -1;
    glGetNamedRenderbufferParameteriv(rb, GL_RENDERBUFFER_INTERNAL_FORMAT, &v);
    CHECK_EQ_INT(v, GL_RGBA8);

    glDeleteRenderbuffers(1, &rb);
}

/* ---------- DSA compressed texture sub-image ---------- */

/* A compressed format names no client format, so glTextureStorage2D has to
   size the level from the block layout alone. glCompressedTextureSubImage2D
   then has to land in it. */

GPU_TEST(framebuffer_dsa, compressed_texture_sub_image_dsa)
{
    /* 64x64 BC1 is 16x16 blocks of 8 bytes (GL 4.6 table 8.19) */
    enum { BLOCK_W = 4, BLOCK_H = 4, BLOCK_BYTES = 8 };
    enum { BLOCKS = (FW / BLOCK_W) * (FH / BLOCK_H) };
    unsigned char data[BLOCKS * BLOCK_BYTES];
    unsigned char got[BLOCKS * BLOCK_BYTES];
    GLint n = 0, i, supported = 0;
    GLint list[128];
    GLuint tex = 0;

    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &n);

    if (n > 0 && n <= (GLint)(sizeof list / sizeof list[0]))
    {
        glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, list);

        for (i = 0; i < n; i++)
            if ((GLenum)list[i] == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
                supported = 1;
    }

    if (!supported)
        SKIP("GL_COMPRESSED_RGBA_S3TC_DXT1_EXT is not supported by this device");

    for (i = 0; i < (GLint)sizeof data; i++)
        data[i] = (unsigned char)(i * 7);

    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, FW, FH);

    if (mgl_drain_errors() != GL_NO_ERROR)
    {
        glDeleteTextures(1, &tex);
        SKIP("compressed immutable storage is not supported by this device");
    }

    glCompressedTextureSubImage2D(tex, 0, 0, 0, FW, FH,
                                  GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                                  (GLsizei)sizeof data, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* reading the compressed image back proves the bytes landed */
    memset(got, 0xCD, sizeof got);
    glGetCompressedTextureImage(tex, 0, (GLsizei)sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(memcmp(got, data, sizeof data) == 0);

    glDeleteTextures(1, &tex);
}

// A stencil-only renderbuffer reads back what it was cleared to. It read 0.
GPU_TEST(framebuffer_dsa, a_stencil_only_renderbuffer_reads_back)
{
    GLuint rb, fb;
    GLubyte got[16 * 16];

    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, 16, 16);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glClearStencil(7);
    glClear(GL_STENCIL_BUFFER_BIT);
    memset(got, 0xAA, sizeof got);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 16, 16, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(got[0] == 7 && got[16 * 16 - 1] == 7, "read %u and %u, want 7", got[0], got[16 * 16 - 1]);

    // and as the wider types, which the suite reads it as
    GLint gi[16 * 16];
    GLuint gu[16 * 16];
    GLfloat gf[16 * 16];

    memset(gi, 0, sizeof gi);
    glReadPixels(0, 0, 16, 16, GL_STENCIL_INDEX, GL_INT, gi);
    CHECK_MSG(gi[0] == 7, "GL_INT read %d, want 7", gi[0]);
    memset(gu, 0, sizeof gu);
    glReadPixels(0, 0, 16, 16, GL_STENCIL_INDEX, GL_UNSIGNED_INT, gu);
    CHECK_MSG(gu[0] == 7, "GL_UNSIGNED_INT read %u, want 7", gu[0]);
    memset(gf, 0, sizeof gf);
    glReadPixels(0, 0, 16, 16, GL_STENCIL_INDEX, GL_FLOAT, gf);
    CHECK_MSG(gf[0] == 7.0f, "GL_FLOAT read %g, want 7", gf[0]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);
}

// One layer of a 3D texture's second level is a complete attachment. It was
// reported incomplete.
GPU_TEST(framebuffer_dsa, a_layer_of_a_3d_level_attaches)
{
    GLuint tex, fb;

    glCreateTextures(GL_TEXTURE_3D, 1, &tex);
    glTextureStorage3D(tex, 2, GL_RGBA8, 8, 8, 48);
    glCreateFramebuffers(1, &fb);
    glNamedFramebufferTextureLayer(fb, GL_COLOR_ATTACHMENT0, tex, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    {
        GLint d = -1;

        glGetTextureLevelParameteriv(tex, 1, GL_TEXTURE_DEPTH, &d);
        CHECK_EQ_INT(d, 24);
    }
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    glNamedFramebufferTextureLayer(fb, GL_COLOR_ATTACHMENT0, tex, 1, 23);
    CHECK_EQ_UINT(glCheckNamedFramebufferStatus(fb, GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// A cleared depth-stencil attachment reads back packed as GL asks for it.
GPU_TEST(framebuffer_dsa, depth_stencil_reads_back_packed)
{
    GLuint tex, fb;
    GLuint packed[16 * 16];
    struct { GLfloat d; GLuint s; } wide[16 * 16];

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, 16, 16);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glClearDepth(0.5);
    glClearStencil(3);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    memset(packed, 0, sizeof packed);
    glReadPixels(0, 0, 16, 16, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG((packed[0] & 0xFF) == 3, "stencil %u, want 3", packed[0] & 0xFF);
    CHECK_MSG(abs((int)(packed[0] >> 8) - 0x7FFFFF) <= 2, "depth %06x, want about 7fffff", packed[0] >> 8);

    memset(wide, 0, sizeof wide);
    glReadPixels(0, 0, 16, 16, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, wide);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(wide[0].d > 0.499f && wide[0].d < 0.501f, "depth %g, want 0.5", wide[0].d);
    CHECK_MSG((wide[0].s & 0xFF) == 3, "stencil %u, want 3", wide[0].s & 0xFF);

    // and through glGetTexImage
    memset(packed, 0, sizeof packed);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG((packed[0] & 0xFF) == 3, "GetTexImage stencil %u, want 3", packed[0] & 0xFF);
    CHECK_MSG(abs((int)(packed[0] >> 8) - 0x7FFFFF) <= 2, "GetTexImage depth %06x", packed[0] >> 8);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// Depth and stencil drawn with no colour buffer at all, as the packed
// depth-stencil conformance cases do: a depth ramp up the target and a
// stencil incremented once everywhere.
GPU_TEST(framebuffer_dsa, depth_and_stencil_draw_without_colour)
{
    static const char *vs = "#version 460\nlayout(location = 0) in vec4 pos;\nvoid main() { gl_Position = pos; }\n";
    static const char *fs = "#version 460\nout vec4 color;\nvoid main() { color = vec4(1); }\n";
    static const GLfloat span[] = { -1, -1, -1, 1,  1, -1, -1, 1,  -1, 1, 1, 1,  1, 1, 1, 1 };
    GLuint tex, fb, vao, vbo, prog;
    GLuint packed[16 * 16];
    char log[1024];
    int bad_d = 0, bad_s = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, 16, 16);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof span, span, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glViewport(0, 0, 16, 16);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);
    glClearStencil(0);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_ZERO, GL_INCR, GL_INCR);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(packed, 0, sizeof packed);
    glReadPixels(0, 0, 16, 16, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++)
        {
            GLuint v = packed[y * 16 + x];
            float d = (float)(v >> 8) / 16777215.0f, want = (y + 0.5f) / 16.0f;

            if (fabsf(d - want) > 0.05f)
                bad_d++;
            if ((v & 0xFF) != 1)
                bad_s++;
        }

    CHECK_MSG(bad_d == 0, "%d depth values off the ramp; row 0 %g, row 15 %g", bad_d,
              (packed[0] >> 8) / 16777215.0, (packed[15 * 16] >> 8) / 16777215.0);
    CHECK_MSG(bad_s == 0, "%d stencil values are not 1; first %u", bad_s, packed[0] & 0xFF);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// Stencil incremented 256 times in the lower half saturates at 255, and a
// later draw tested EQUAL 255 writes depth only there.
GPU_TEST(framebuffer_dsa, stencil_saturates_and_gates_later_draws)
{
    static const char *vs = "#version 460\nlayout(location = 0) in vec4 pos;\nvoid main() { gl_Position = pos; }\n";
    static const char *fs = "#version 460\nout vec4 color;\nvoid main() { color = vec4(1); }\n";
    static const GLfloat span1[] = { -1, -1, -1, 1,  1, -1, -1, 1,  -1, 1, 1, 1,  1, 1, 1, 1 };
    static const GLfloat span2[] = { -1, -1, 1, 1,  1, -1, 1, 1,  -1, 1, -1, 1,  1, 1, -1, 1 };
    GLuint tex, fb, vao, vbo[2], prog;
    GLuint packed[16 * 16];
    char log[1024];

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, 16, 16);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    prog = mgl_build_program(vs, fs, log, sizeof log);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof span1, span1, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof span2, span2, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glViewport(0, 0, 16, 16);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0);
    glClearStencil(0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 16, 8);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_ZERO, GL_INCR, GL_INCR);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    for (int i = 0; i < 256; i++)
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_SCISSOR_TEST);

    glStencilFunc(GL_EQUAL, 0xFF, 0xFF);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(packed, 0, sizeof packed);
    glReadPixels(0, 0, 16, 16, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);

    GLuint low = packed[2 * 16 + 3], high = packed[13 * 16 + 3];
    CHECK_MSG((low & 0xFF) == 255, "lower half stencil %u, want 255", low & 0xFF);
    CHECK_MSG((high & 0xFF) == 0, "upper half stencil %u, want 0", high & 0xFF);
    CHECK_MSG(fabs((low >> 8) / 16777215.0 - 2.5 / 16.0) < 0.05, "lower half depth %g, want the ramp", (low >> 8) / 16777215.0);
    CHECK_MSG((high >> 8) > 0xFFFF00, "upper half depth %g, want 1", (high >> 8) / 16777215.0);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(2, vbo);
    glDeleteVertexArrays(1, &vao);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
}

// Colour and depth-stencil renderbuffers asked for the same sample count end
// up with the same one, so together they make a complete framebuffer at
// every count up to GL_MAX_SAMPLES.
GPU_TEST(framebuffer_dsa, same_requested_samples_match_across_formats)
{
    GLint max = 0;

    glGetIntegerv(GL_MAX_SAMPLES, &max);

    for (int i = 0; i <= max; i++)
    {
        GLuint fb, rb[2];
        GLint sc = -1, sd = -1;

        glGenFramebuffers(1, &fb);
        glGenRenderbuffers(2, rb);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glBindRenderbuffer(GL_RENDERBUFFER, rb[0]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, i, GL_DEPTH24_STENCIL8, 16, 16);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &sd);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[0]);
        glBindRenderbuffer(GL_RENDERBUFFER, rb[1]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, i, GL_RGBA8, 16, 16);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &sc);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb[1]);

        CHECK_MSG(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                  "%d samples asked: colour got %d, depth-stencil %d", i, sc, sd);

        // what is reported is what Metal gives, a power of two, from the start
        CHECK_MSG(sc == sd && (sc & (sc - 1)) == 0 && sc >= i, "%d samples asked: reported %d and %d", i, sc, sd);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fb);
        glDeleteRenderbuffers(2, rb);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// Depth and stencil blitted from one framebuffer to another, single-sampled
// and resolved from a multisampled source, arrive intact.
static GLuint dsStencilAfterBlit(int src_samples, GLuint *depth_out)
{
    GLuint fb[2], rb[2];
    GLuint packed[16 * 16];

    glGenFramebuffers(2, fb);
    glGenRenderbuffers(2, rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb[0]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, src_samples, GL_DEPTH24_STENCIL8, 16, 16);
    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[0]);
    glBindRenderbuffer(GL_RENDERBUFFER, rb[1]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 0, GL_DEPTH24_STENCIL8, 16, 16);
    glBindFramebuffer(GL_FRAMEBUFFER, fb[1]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[1]);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glClearDepth(0.25);
    glClearStencil(1);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb[1]);
    glDrawBuffer(GL_NONE);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[1]);
    glReadBuffer(GL_NONE);
    memset(packed, 0, sizeof packed);
    glReadPixels(0, 0, 16, 16, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(2, fb);
    glDeleteRenderbuffers(2, rb);

    *depth_out = packed[5 * 16 + 5] >> 8;
    return packed[5 * 16 + 5] & 0xFF;
}

GPU_TEST(framebuffer_dsa, depth_and_stencil_survive_a_blit)
{
    GLuint d = 0;
    GLuint s = dsStencilAfterBlit(0, &d);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(s == 1, "single-sampled: stencil %u, want 1", s);
    CHECK_MSG(abs((int)d - 0x3FFFFF) < 4, "single-sampled: depth %06x, want 3fffff", d);

    s = dsStencilAfterBlit(4, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(s == 1, "resolved: stencil %u, want 1", s);
    CHECK_MSG(abs((int)d - 0x3FFFFF) < 4, "resolved: depth %06x, want 3fffff", d);
}

// A single-sampled depth-stencil image blitted into a multisampled one fills
// every sample, so resolving it back gives the same values.
GPU_TEST(framebuffer_dsa, depth_and_stencil_blit_into_multisample)
{
    GLuint fb[3], rb[3];
    GLuint packed[16 * 16];

    glGenFramebuffers(3, fb);
    glGenRenderbuffers(3, rb);
    for (int k = 0; k < 3; k++)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, rb[k]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, k == 1 ? 4 : 0, GL_DEPTH24_STENCIL8, 16, 16);
        glBindFramebuffer(GL_FRAMEBUFFER, fb[k]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[k]);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glClearDepth(0.25);
    glClearStencil(1);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb[1]);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb[1]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb[2]);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[2]);
    memset(packed, 0, sizeof packed);
    glReadPixels(0, 0, 16, 16, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);
    CHECK_MSG((packed[37] & 0xFF) == 1, "stencil %u, want 1", packed[37] & 0xFF);
    CHECK_MSG(abs((int)(packed[37] >> 8) - 0x3FFFFF) < 4, "depth %06x, want 3fffff", packed[37] >> 8);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(3, fb);
    glDeleteRenderbuffers(3, rb);
}

// A draw tested against a multisampled stencil buffer, then the colour
// resolved to a single-sampled target.
GPU_TEST(framebuffer_dsa, stencil_tested_draw_into_multisample_resolves)
{
    static const char *vs = "#version 460\nlayout(location = 0) in vec4 pos;\nvoid main() { gl_Position = pos; }\n";
    static const char *fs = "#version 460\nout vec4 color;\nvoid main() { color = vec4(1, 0, 0, 1); }\n";
    static const GLfloat quad[] = { -1, -1, 0, 1,  1, -1, 0, 1,  -1, 1, 0, 1,  1, 1, 0, 1 };
    GLuint fb[2], rb[3], vao, vbo, prog;
    GLubyte px[16 * 16 * 4];
    char log[1024];

    glGenFramebuffers(2, fb);
    glGenRenderbuffers(3, rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb[0]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, 16, 16);
    glBindRenderbuffer(GL_RENDERBUFFER, rb[1]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, 16, 16);
    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[0]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb[1]);
    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    glBindRenderbuffer(GL_RENDERBUFFER, rb[2]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 0, GL_RGBA8, 16, 16);
    glBindFramebuffer(GL_FRAMEBUFFER, fb[1]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb[2]);

    prog = mgl_build_program(vs, fs, log, sizeof log);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glViewport(0, 0, 16, 16);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glClearColor(0, 0, 1, 1);
    glClearStencil(1);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_STENCIL_TEST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb[1]);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[1]);
    memset(px, 0, sizeof px);
    glReadPixels(0, 0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_MSG(px[4 * 37] == 255 && px[4 * 37 + 2] == 0, "got %u %u %u, want red", px[4 * 37], px[4 * 37 + 1], px[4 * 37 + 2]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(2, fb);
    glDeleteRenderbuffers(3, rb);
}

// A draw into a multisampled depth-stencil framebuffer with no colour image
// writes the stencil, and a blit resolves it.
GPU_TEST(framebuffer_dsa, draw_into_multisample_depth_only)
{
    static const char *vs = "#version 460\nlayout(location = 0) in vec4 pos;\nvoid main() { gl_Position = pos; }\n";
    static const char *fs = "#version 460\nout vec4 color;\nvoid main() { color = vec4(1); }\n";
    static const GLfloat quad[] = { -1, -1, -1, 1,  1, -1, -1, 1,  -1, 1, 1, 1,  1, 1, 1, 1 };
    GLuint fb[2], rb[2], vao, vbo, prog;
    GLuint packed[16 * 16];
    char log[1024];

    glGenFramebuffers(2, fb);
    glGenRenderbuffers(2, rb);
    for (int k = 0; k < 2; k++)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, rb[k]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, k == 0 ? 4 : 0, GL_DEPTH24_STENCIL8, 16, 16);
        glBindFramebuffer(GL_FRAMEBUFFER, fb[k]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[k]);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    prog = mgl_build_program(vs, fs, log, sizeof log);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glViewport(0, 0, 16, 16);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glClearStencil(0);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb[1]);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fb[1]);
    memset(packed, 0, sizeof packed);
    glReadPixels(0, 0, 16, 16, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG((packed[37] & 0xFF) == 1, "stencil %u, want 1", packed[37] & 0xFF);
    // a ramp from 0 at the bottom row to 1 at the top
    for (int y = 0; y < 16; y += 5)
    {
        double d = (packed[y * 16 + 4] >> 8) / 16777215.0, want = (y + 0.5) / 16.0;

        CHECK_MSG(fabs(d - want) < 0.04, "row %d depth %g, want %g", y, d, want);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(2, fb);
    glDeleteRenderbuffers(2, rb);
}

// A drawn depth ramp blitted into a multisampled image and resolved back keeps
// its values. Metal copied across sample counts, which it cannot do.
GPU_TEST(framebuffer_dsa, a_depth_ramp_round_trips_through_multisample)
{
    static const char *vs = "#version 460\nlayout(location = 0) in vec4 pos;\nvoid main() { gl_Position = pos; }\n";
    static const char *fs = "#version 460\nout vec4 color;\nvoid main() { color = vec4(1); }\n";
    static const GLfloat ramp[] = { -1, -1, -1, 1,  1, -1, -1, 1,  -1, 1, 1, 1,  1, 1, 1, 1 };
    GLuint fb[3], rb[3], vao, vbo, prog;
    GLuint packed[16 * 16];
    char log[1024];

    glGenFramebuffers(3, fb);
    glGenRenderbuffers(3, rb);
    for (int k = 0; k < 3; k++)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, rb[k]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, k == 1 ? 4 : 0, GL_DEPTH24_STENCIL8, 16, 16);
        glBindFramebuffer(GL_FRAMEBUFFER, fb[k]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb[k]);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    prog = mgl_build_program(vs, fs, log, sizeof log);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof ramp, ramp, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glViewport(0, 0, 16, 16);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[0]);
    glClearDepth(1.0);
    glClearStencil(0);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 6, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb[1]);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb[1]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb[2]);
    glBlitFramebuffer(0, 0, 16, 16, 0, 0, 16, 16, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_FRAMEBUFFER, fb[2]);
    memset(packed, 0, sizeof packed);
    glReadPixels(0, 0, 16, 16, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);
    for (int y = 0; y < 16; y += 5)
    {
        double d = (packed[y * 16 + 4] >> 8) / 16777215.0, want = (y + 0.5) / 16.0;

        CHECK_MSG(fabs(d - want) < 0.04, "row %d depth %g, want %g", y, d, want);
        CHECK_MSG((packed[y * 16 + 4] & 0xFF) == 6, "row %d stencil %u, want 6", y, packed[y * 16 + 4] & 0xFF);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(3, fb);
    glDeleteRenderbuffers(3, rb);
}

// A new framebuffer object reads from and draws to colour attachment 0, and
// the queries say so; they reported the window's buffers.
GPU_TEST(framebuffer_dsa, a_new_framebuffer_reports_its_own_buffers)
{
    GLuint fb;
    GLint rb = -1, db = -1;

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glGetIntegerv(GL_READ_BUFFER, &rb);
    glGetIntegerv(GL_DRAW_BUFFER0, &db);
    CHECK_EQ_INT(rb, GL_COLOR_ATTACHMENT0);
    CHECK_EQ_INT(db, GL_COLOR_ATTACHMENT0);

    // what was read can be set back
    glReadBuffer((GLenum)rb);
    glDrawBuffers(1, (const GLenum *)&db);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
}

// glCopyTexImage2D into a depth or depth-stencil format reads the framebuffer's
// depth and stencil, converting where the formats differ. It read the colour
// read buffer instead, which a depth-only framebuffer does not have.
GPU_TEST(framebuffer_dsa, copy_tex_image_takes_depth_and_stencil)
{
    GLuint src, fb, d16, ds;
    GLushort got16[8 * 8];
    GLuint packed[8 * 8];

    glGenTextures(1, &src);
    glBindTexture(GL_TEXTURE_2D, src);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, 8, 8);
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, src, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glClearDepth(0.5);
    glClearStencil(9);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glGenTextures(1, &d16);
    glBindTexture(GL_TEXTURE_2D, d16);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 0, 0, 8, 8, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    memset(got16, 0, sizeof got16);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, got16);
    CHECK_MSG(abs((int)got16[9] - 0x7FFF) < 4, "16-bit depth %04x, want 7fff", got16[9]);

    glGenTextures(1, &ds);
    glBindTexture(GL_TEXTURE_2D, ds);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, 0, 0, 8, 8, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    memset(packed, 0, sizeof packed);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, packed);
    CHECK_MSG((packed[9] & 0xFF) == 9, "stencil %u, want 9", packed[9] & 0xFF);
    CHECK_MSG(abs((int)(packed[9] >> 8) - 0x7FFFFF) < 4, "depth %06x, want 7fffff", packed[9] >> 8);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &src);
    glDeleteTextures(1, &d16);
    glDeleteTextures(1, &ds);
}

// A packed depth-stencil image attached as only one half: the missing half's
// test always passes, so even NEVER lets the draw through.
GPU_TEST(framebuffer_dsa, a_missing_half_always_passes)
{
    static const char *vs = "#version 460\nlayout(location = 0) in vec4 pos;\nvoid main() { gl_Position = pos; }\n";
    static const char *fs = "#version 460\nout vec4 color;\nvoid main() { color = vec4(0, 1, 0, 1); }\n";
    static const GLfloat quad[] = { -1, -1, 0, 1,  1, -1, 0, 1,  -1, 1, 0, 1,  1, 1, 0, 1 };
    GLuint vao, vbo, prog;
    char log[1024];

    prog = mgl_build_program(vs, fs, log, sizeof log);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glViewport(0, 0, 16, 16);

    for (int i = 0; i < 2; i++)
    {
        GLuint fb, rb[2];
        GLubyte px[4] = { 0 };

        glGenFramebuffers(1, &fb);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glGenRenderbuffers(2, rb);
        glBindRenderbuffer(GL_RENDERBUFFER, rb[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);
        glBindRenderbuffer(GL_RENDERBUFFER, rb[1]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 16, 16);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb[0]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, i ? GL_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb[1]);

        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        if (i)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_NEVER);
        }
        else
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_NEVER, 0, 0xFF);
        }
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDepthFunc(GL_LESS);

        glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        CHECK_MSG(px[1] == 255 && px[0] == 0, "%s only: got %u %u %u, want green", i ? "stencil" : "depth",
                  px[0], px[1], px[2]);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fb);
        glDeleteRenderbuffers(2, rb);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}
