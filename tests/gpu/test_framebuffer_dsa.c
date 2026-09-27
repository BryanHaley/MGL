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
                  "%d of %d stencil texels are not the written 5", bad_stencil, FW * FH);

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
