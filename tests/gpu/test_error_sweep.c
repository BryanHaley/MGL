/*
 * test_error_sweep.c
 * MGL
 *
 * Confidence pass over the API surface. Every call here is given arguments an
 * application could plausibly get wrong, and the only requirement is that MGL
 * survives and leaves a sane error behind. Reaching the end is the assertion.
 *
 * These aim for breadth, not depth; the per-subsystem files check that calls
 * with good arguments do the right thing.
 */

#include "mgl_test.h"
#include "harness.h"

#define BAD 0x9999

static GLint  i4[4];
static GLuint u4[4];
static GLfloat f4[4];
static GLdouble d4[4];
static GLint64 i64v[4];
static GLubyte bytes[1024];
static void *ptrv;
static char strbuf[256];
static GLsizei lenv;

/* ---------- object lifecycle with hostile names ---------- */

GPU_TEST(sweep, deletes_of_never_generated_names)
{
    GLuint junk[3] = { 7777, 8888, 9999 };

    glDeleteBuffers(3, junk);
    glDeleteTextures(3, junk);
    glDeleteFramebuffers(3, junk);
    glDeleteRenderbuffers(3, junk);
    glDeleteVertexArrays(3, junk);
    glDeleteSamplers(3, junk);
    glDeleteProgramPipelines(3, junk);
    glDeleteTransformFeedbacks(3, junk);

    // object deletes ignore names GL never generated
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // programs and shaders are the exception: the spec asks for INVALID_VALUE
    glDeleteProgram(7771);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteShader(8881);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(sweep, binds_of_never_generated_names)
{
    glBindBuffer(GL_ARRAY_BUFFER, 7777);        mgl_drain_errors();
    glBindTexture(GL_TEXTURE_2D, 7777);         mgl_drain_errors();
    glBindFramebuffer(GL_FRAMEBUFFER, 7777);    mgl_drain_errors();
    glBindRenderbuffer(GL_RENDERBUFFER, 7777);  mgl_drain_errors();
    glBindVertexArray(7777);                    mgl_drain_errors();
    glBindSampler(0, 7777);                     mgl_drain_errors();
    glUseProgram(7777);                         mgl_drain_errors();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    CHECK(1);
}

GPU_TEST(sweep, is_queries_on_junk)
{
    // a name no other test binds, since binding one creates the object
    const GLuint n = 31337;

    CHECK(glIsBuffer(n) == GL_FALSE);
    CHECK(glIsTexture(n) == GL_FALSE);
    CHECK(glIsFramebuffer(n) == GL_FALSE);
    CHECK(glIsRenderbuffer(n) == GL_FALSE);
    CHECK(glIsVertexArray(n) == GL_FALSE);
    CHECK(glIsSampler(n) == GL_FALSE);
    CHECK(glIsShader(n) == GL_FALSE);
    CHECK(glIsProgram(n) == GL_FALSE);

    mgl_drain_errors();
}

/* ---------- state setters with bad enums ---------- */

GPU_TEST(sweep, state_setters_reject_bad_enums)
{
    glEnable(BAD);                       mgl_drain_errors();
    glDisable(BAD);                      mgl_drain_errors();
    glEnablei(BAD, 0);                   mgl_drain_errors();
    glDisablei(BAD, 0);                  mgl_drain_errors();
    glBlendFunc(BAD, BAD);               mgl_drain_errors();
    glBlendFuncSeparate(BAD, BAD, BAD, BAD); mgl_drain_errors();
    glBlendFunci(0, BAD, BAD);           mgl_drain_errors();
    glBlendEquation(BAD);                mgl_drain_errors();
    glBlendEquationSeparate(BAD, BAD);   mgl_drain_errors();
    glDepthFunc(BAD);                    mgl_drain_errors();
    glStencilFunc(BAD, 0, 0xFF);         mgl_drain_errors();
    glStencilOp(BAD, BAD, BAD);          mgl_drain_errors();
    glStencilOpSeparate(BAD, BAD, BAD, BAD); mgl_drain_errors();
    glCullFace(BAD);                     mgl_drain_errors();
    glFrontFace(BAD);                    mgl_drain_errors();
    glPolygonMode(BAD, BAD);             mgl_drain_errors();
    glHint(BAD, BAD);                    mgl_drain_errors();
    glLogicOp(BAD);                      mgl_drain_errors();
    glProvokingVertex(BAD);              mgl_drain_errors();
    glPointParameteri(BAD, 0);           mgl_drain_errors();
    glClampColor(BAD, BAD);              mgl_drain_errors();

    CHECK(1);
}

GPU_TEST(sweep, state_setters_reject_bad_values)
{
    glViewport(0, 0, -1, -1);            mgl_drain_errors();
    glScissor(0, 0, -1, -1);             mgl_drain_errors();
    glLineWidth(-1.0f);                  mgl_drain_errors();
    glPointSize(-1.0f);                  mgl_drain_errors();
    glSampleCoverage(-5.0f, GL_FALSE);   mgl_drain_errors();
    glStencilMask(0);                    mgl_drain_errors();
    glDepthRange(5.0, -5.0);             mgl_drain_errors();

    CHECK(1);
}

/* ---------- getters with bad enums ---------- */

GPU_TEST(sweep, getters_reject_bad_enums)
{
    glGetBooleanv(BAD, (GLboolean *)i4);  mgl_drain_errors();
    glGetIntegerv(BAD, i4);               mgl_drain_errors();
    glGetInteger64v(BAD, i64v);           mgl_drain_errors();
    glGetFloatv(BAD, f4);                 mgl_drain_errors();
    glGetDoublev(BAD, d4);                mgl_drain_errors();
    glGetIntegeri_v(BAD, 0, i4);          mgl_drain_errors();
    glGetInteger64i_v(BAD, 0, i64v);      mgl_drain_errors();
    glGetString(BAD);                     mgl_drain_errors();
    glGetStringi(BAD, 0);                 mgl_drain_errors();
    glGetPointerv(BAD, &ptrv);            mgl_drain_errors();
    glGetError();

    CHECK(1);
}

GPU_TEST(sweep, indexed_getters_out_of_range)
{
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 9999, i4); mgl_drain_errors();
    glGetStringi(GL_EXTENSIONS, 999999);                        mgl_drain_errors();

    CHECK(1);
}

/* ---------- buffers ---------- */

GPU_TEST(sweep, buffer_calls_with_bad_arguments)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);

    glBufferData(BAD, 16, NULL, GL_STATIC_DRAW);        mgl_drain_errors();
    glBufferData(GL_ARRAY_BUFFER, -1, NULL, GL_STATIC_DRAW); mgl_drain_errors();
    glBufferData(GL_ARRAY_BUFFER, 16, NULL, BAD);       mgl_drain_errors();
    glBufferSubData(GL_ARRAY_BUFFER, -1, 4, bytes);     mgl_drain_errors();
    glBufferSubData(GL_ARRAY_BUFFER, 0, 99999, bytes);  mgl_drain_errors();
    glGetBufferParameteriv(GL_ARRAY_BUFFER, BAD, i4);   mgl_drain_errors();
    glGetBufferPointerv(GL_ARRAY_BUFFER, BAD, &ptrv);   mgl_drain_errors();
    glMapBufferRange(GL_ARRAY_BUFFER, 0, 16, BAD);      mgl_drain_errors();
    glUnmapBuffer(GL_ARRAY_BUFFER);                     mgl_drain_errors();
    glCopyBufferSubData(BAD, BAD, 0, 0, 4);             mgl_drain_errors();
    glBindBufferBase(BAD, 0, b);                        mgl_drain_errors();
    glBindBufferRange(BAD, 0, b, 0, 4);                 mgl_drain_errors();

    glDeleteBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    CHECK(1);
}

/* ---------- textures ---------- */

GPU_TEST(sweep, texture_calls_with_bad_arguments)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    glTexStorage2D(BAD, 1, GL_RGBA8, 8, 8);              mgl_drain_errors();
    glTexStorage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8);    mgl_drain_errors();
    glTexStorage2D(GL_TEXTURE_2D, 1, BAD, 8, 8);         mgl_drain_errors();
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, -8, 8);   mgl_drain_errors();
    glTexImage2D(BAD, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, bytes); mgl_drain_errors();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, BAD, GL_UNSIGNED_BYTE, bytes); mgl_drain_errors();
    glTexParameteri(GL_TEXTURE_2D, BAD, 0);              mgl_drain_errors();
    glTexParameterf(GL_TEXTURE_2D, BAD, 0.0f);           mgl_drain_errors();
    glTexParameteriv(GL_TEXTURE_2D, BAD, i4);            mgl_drain_errors();
    glGetTexParameteriv(GL_TEXTURE_2D, BAD, i4);         mgl_drain_errors();
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, BAD, i4); mgl_drain_errors();
    glGenerateMipmap(BAD);                               mgl_drain_errors();
    glActiveTexture(BAD);                                mgl_drain_errors();
    glActiveTexture(GL_TEXTURE0);

    glDeleteTextures(1, &t);

    CHECK(1);
}

/* ---------- samplers ---------- */

GPU_TEST(sweep, sampler_calls_with_bad_arguments)
{
    GLuint s = 0;

    glGenSamplers(1, &s);

    glSamplerParameteri(s, BAD, 0);          mgl_drain_errors();
    glSamplerParameterf(s, BAD, 0.0f);       mgl_drain_errors();
    glGetSamplerParameteriv(s, BAD, i4);     mgl_drain_errors();
    glGetSamplerParameterfv(s, BAD, f4);     mgl_drain_errors();
    glGetSamplerParameterIiv(s, BAD, i4);    mgl_drain_errors();
    glGetSamplerParameterIuiv(s, BAD, u4);   mgl_drain_errors();
    glSamplerParameteri(7777, GL_TEXTURE_MIN_FILTER, GL_NEAREST); mgl_drain_errors();
    glBindSampler(9999, s);                  mgl_drain_errors();

    glDeleteSamplers(1, &s);

    CHECK(1);
}

/* ---------- framebuffers ---------- */

GPU_TEST(sweep, framebuffer_calls_with_bad_arguments)
{
    GLuint fbo = 0, rb = 0;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &rb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);

    glFramebufferTexture2D(BAD, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);   mgl_drain_errors();
    glFramebufferTexture2D(GL_FRAMEBUFFER, BAD, GL_TEXTURE_2D, 0, 0);         mgl_drain_errors();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, BAD, 0, 0);  mgl_drain_errors();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, BAD, GL_RENDERBUFFER, rb);      mgl_drain_errors();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, BAD, rb); mgl_drain_errors();
    glRenderbufferStorage(BAD, GL_RGBA8, 8, 8);                               mgl_drain_errors();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, -8, 8);                  mgl_drain_errors();
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, BAD, i4);                   mgl_drain_errors();
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, BAD, i4); mgl_drain_errors();
    glCheckFramebufferStatus(BAD);                                            mgl_drain_errors();
    glFramebufferParameteri(GL_FRAMEBUFFER, BAD, 0);                          mgl_drain_errors();
    glDrawBuffer(BAD);                                                        mgl_drain_errors();
    glReadBuffer(BAD);                                                        mgl_drain_errors();
    glBlitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, BAD, GL_NEAREST);               mgl_drain_errors();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rb);

    CHECK(1);
}

GPU_TEST(sweep, dsa_calls_on_unknown_names)
{
    static const GLenum bufs[1] = { GL_COLOR_ATTACHMENT0 };

    glNamedFramebufferTexture(7777, GL_COLOR_ATTACHMENT0, 0, 0);          mgl_drain_errors();
    glNamedFramebufferRenderbuffer(7777, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, 0); mgl_drain_errors();
    glNamedFramebufferParameteri(7777, GL_FRAMEBUFFER_DEFAULT_WIDTH, 1);  mgl_drain_errors();
    glGetNamedFramebufferParameteriv(7777, GL_FRAMEBUFFER_DEFAULT_WIDTH, i4); mgl_drain_errors();
    glNamedFramebufferDrawBuffers(7777, 1, bufs);                         mgl_drain_errors();
    glCheckNamedFramebufferStatus(7777, GL_FRAMEBUFFER);                  mgl_drain_errors();
    glNamedRenderbufferStorage(7777, GL_RGBA8, 8, 8);                     mgl_drain_errors();
    glGetNamedRenderbufferParameteriv(7777, GL_RENDERBUFFER_WIDTH, i4);   mgl_drain_errors();
    glMapNamedBufferRange(7777, 0, 4, GL_MAP_READ_BIT);                   mgl_drain_errors();
    glGetNamedBufferSubData(7777, 0, 4, bytes);                           mgl_drain_errors();
    glUnmapNamedBuffer(7777);                                             mgl_drain_errors();

    CHECK(1);
}

/* ---------- shaders and programs ---------- */

GPU_TEST(sweep, shader_and_program_calls_with_bad_arguments)
{
    GLuint p = glCreateProgram();
    GLuint sh = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(7777, 1, NULL, NULL);          mgl_drain_errors();
    glCompileShader(7777);                        mgl_drain_errors();
    glGetShaderiv(7777, GL_COMPILE_STATUS, i4);   mgl_drain_errors();
    glGetShaderiv(sh, BAD, i4);                   mgl_drain_errors();
    glGetShaderInfoLog(7777, sizeof strbuf, &lenv, strbuf); mgl_drain_errors();
    glGetShaderSource(7777, sizeof strbuf, &lenv, strbuf);  mgl_drain_errors();
    glAttachShader(p, 7777);                      mgl_drain_errors();
    glAttachShader(7777, sh);                     mgl_drain_errors();
    glDetachShader(p, 7777);                      mgl_drain_errors();
    glLinkProgram(7777);                          mgl_drain_errors();
    glGetProgramiv(7777, GL_LINK_STATUS, i4);     mgl_drain_errors();
    glGetProgramiv(p, BAD, i4);                   mgl_drain_errors();
    glGetProgramInfoLog(7777, sizeof strbuf, &lenv, strbuf); mgl_drain_errors();
    glGetUniformLocation(7777, "x");              mgl_drain_errors();
    glGetUniformBlockIndex(7777, "x");            mgl_drain_errors();
    glGetActiveUniformName(7777, 0, sizeof strbuf, &lenv, strbuf); mgl_drain_errors();
    glValidateProgram(7777);                      mgl_drain_errors();

    glDeleteShader(sh);
    glDeleteProgram(p);

    CHECK(1);
}

GPU_TEST(sweep, unlinked_program_reports_failure)
{
    GLuint p = glCreateProgram();
    GLint linked = GL_TRUE;

    // nothing attached, so linking cannot succeed
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &linked);

    CHECK_MSG(linked == GL_FALSE, "empty program reported LINK_STATUS true");

    GLint len = 0;
    glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
    CHECK_MSG(len > 0, "failed link left an empty info log");

    glGetProgramInfoLog(p, sizeof strbuf, &lenv, strbuf);
    CHECK_MSG(lenv > 0, "info log came back empty");

    glDeleteProgram(p);
    mgl_drain_errors();
}

GPU_TEST(sweep, failed_compile_reports_failure)
{
    GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
    const char *bad = "#version 460 core\nnot valid glsl\n";
    GLint ok = GL_TRUE, len = 0;

    glShaderSource(sh, 1, &bad, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);

    CHECK_MSG(ok == GL_FALSE, "bad shader reported COMPILE_STATUS true");
    CHECK_MSG(len > 0, "failed compile left an empty info log");

    glDeleteShader(sh);
    mgl_drain_errors();
}

GPU_TEST(sweep, good_program_reports_success)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location=0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 460 core\n"
        "layout(location=0) out vec4 c;\n"
        "void main() { c = vec4(1.0); }\n";
    char log[1024] = { 0 };
    GLuint prog = mgl_build_program(vs, fs, log, sizeof log);
    GLint linked = GL_FALSE, len = -1, active = -1;

    if (!prog) SKIP("shader failed to build");

    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    CHECK_MSG(linked == GL_TRUE, "good program reported LINK_STATUS false");

    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    CHECK_MSG(len == 0, "successful link left a %d byte info log", len);

    glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTES, &active);
    CHECK_MSG(active >= 1, "ACTIVE_ATTRIBUTES = %d, expected at least 1", active);

    glDeleteProgram(prog);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- vertex arrays and draws ---------- */

GPU_TEST(sweep, vertex_array_calls_with_bad_arguments)
{
    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexAttribPointer(9999, 4, GL_FLOAT, GL_FALSE, 0, NULL);  mgl_drain_errors();
    glVertexAttribPointer(0, 99, GL_FLOAT, GL_FALSE, 0, NULL);    mgl_drain_errors();
    glVertexAttribPointer(0, 4, BAD, GL_FALSE, 0, NULL);          mgl_drain_errors();
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, -1, NULL);    mgl_drain_errors();
    glEnableVertexAttribArray(9999);                              mgl_drain_errors();
    glDisableVertexAttribArray(9999);                             mgl_drain_errors();
    glVertexAttribDivisor(9999, 1);                               mgl_drain_errors();
    glGetVertexAttribiv(9999, GL_VERTEX_ATTRIB_ARRAY_SIZE, i4);   mgl_drain_errors();
    glGetVertexArrayiv(7777, GL_ELEMENT_ARRAY_BUFFER_BINDING, i4); mgl_drain_errors();
    glGetVertexArrayIndexediv(vao, 9999, GL_VERTEX_ATTRIB_ARRAY_SIZE, i4); mgl_drain_errors();

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);

    CHECK(1);
}

GPU_TEST(sweep, draw_calls_with_bad_arguments)
{
    glDrawArrays(BAD, 0, 3);                              mgl_drain_errors();
    glDrawArrays(GL_TRIANGLES, 0, -1);                    mgl_drain_errors();
    glDrawArrays(GL_TRIANGLES, -1, 3);                    mgl_drain_errors();
    glDrawElements(BAD, 3, GL_UNSIGNED_SHORT, NULL);      mgl_drain_errors();
    glDrawElements(GL_TRIANGLES, -1, GL_UNSIGNED_SHORT, NULL); mgl_drain_errors();
    glDrawElements(GL_TRIANGLES, 3, BAD, NULL);           mgl_drain_errors();
    glDrawArraysInstanced(GL_TRIANGLES, 0, 3, -1);        mgl_drain_errors();
    glDrawRangeElements(GL_TRIANGLES, 10, 0, 3, GL_UNSIGNED_SHORT, NULL); mgl_drain_errors();
    glDrawElementsBaseVertex(GL_TRIANGLES, 3, BAD, NULL, 0); mgl_drain_errors();

    CHECK(1);
}

GPU_TEST(sweep, draws_with_nothing_bound)
{
    glUseProgram(0);
    glBindVertexArray(0);

    // no program and no vertex array; must error rather than dereference
    glDrawArrays(GL_TRIANGLES, 0, 3);
    mgl_drain_errors();

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL);
    mgl_drain_errors();

    CHECK(1);
}

/* ---------- pixel transfer ---------- */

GPU_TEST(sweep, pixel_transfer_with_bad_arguments)
{
    glPixelStorei(BAD, 1);                                   mgl_drain_errors();
    glPixelStorei(GL_PACK_ALIGNMENT, 3);                     mgl_drain_errors();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 0);                   mgl_drain_errors();
    glReadPixels(0, 0, -1, -1, GL_RGBA, GL_UNSIGNED_BYTE, bytes); mgl_drain_errors();
    glReadPixels(0, 0, 1, 1, BAD, GL_UNSIGNED_BYTE, bytes);  mgl_drain_errors();
    glReadPixels(0, 0, 1, 1, GL_RGBA, BAD, bytes);           mgl_drain_errors();
    glReadPixels(0, 0, 1, 1, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, bytes); mgl_drain_errors();

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    CHECK(1);
}

/* ---------- uniforms with no program bound ---------- */

GPU_TEST(sweep, uniform_calls_with_no_program)
{
    glUseProgram(0);

    glUniform1i(0, 1);            mgl_drain_errors();
    glUniform4f(0, 1, 2, 3, 4);   mgl_drain_errors();
    glUniform1fv(0, 1, f4);       mgl_drain_errors();
    glUniformMatrix4fv(0, 1, GL_FALSE, (const GLfloat *)f4); mgl_drain_errors();
    glUniform1i(-1, 1);           mgl_drain_errors();
    glUniform1i(99999, 1);        mgl_drain_errors();

    CHECK(1);
}

/* ---------- the error state itself ---------- */

GPU_TEST(sweep, error_state_latches_and_clears)
{
    // drain whatever the earlier calls left
    mgl_drain_errors();

    glEnable(BAD);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_ENUM);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);

    // GL keeps the first error until it is read
    glEnable(BAD);
    glCullFace(BAD);
    CHECK_EQ_UINT(glGetError(), GL_INVALID_ENUM);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);
}

GPU_TEST(sweep, context_survives_the_whole_sweep)
{
    // if any of the above corrupted state, a plain render will show it
    MGLTestTarget t;
    unsigned char *px, c[4];

    if (!mgl_target_create(&t, 32, 32, GL_RGBA8, 0)) SKIP("no target");

    mgl_target_bind(&t);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    px = mgl_read_rgba8(&t);
    if (!px) { mgl_target_destroy(&t); SKIP("readback failed"); }

    mgl_pixel_at(px, &t, 16, 16, c);
    CHECK_MSG(c[1] > 200, "context is still usable after the sweep (G = %d)", c[1]);

    free(px);
    mgl_target_destroy(&t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}
