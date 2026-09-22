/*
 * test_multisample.c
 * Copyright (C) The Moogle Project
 *
 * Multisample rasterisation: storage, the sample count GL reports, resolving
 * through glBlitFramebuffer, and the shader-side sample variables.
 *
 * The thing worth pinning here is that a driver which quietly renders single
 * sampled reports exactly the same GL_SAMPLES. Only the resolved edge tells
 * the two apart.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

static const char *VS =
    "#version 420\n"
    "layout(location = 0) in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

static GLuint buildFor(const char *fs_src)
{
    char log[1024];

    return mgl_build_program(VS, fs_src, log, sizeof log);
}

// A multisampled colour target of `samples` samples, left bound.
static GLuint msTarget(GLsizei samples, GLuint *tex_out)
{
    GLuint tex = 0, fbo = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA8, 64, 64, GL_TRUE);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D_MULTISAMPLE, tex, 0);

    if (tex_out)
        *tex_out = tex;

    return fbo;
}

/* ---------- the sample count GL reports ---------- */

GPU_TEST(multisample, samples_and_sample_buffers)
{
    MGLTestTarget single;
    GLint n = -1, bufs = -1;
    GLuint ms;

    if (!mgl_target_create(&single, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&single);
    glGetIntegerv(GL_SAMPLES, &n);
    glGetIntegerv(GL_SAMPLE_BUFFERS, &bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(n, 0);
    CHECK_EQ_INT(bufs, 0);

    msTarget(4, &ms);
    glGetIntegerv(GL_SAMPLES, &n);
    glGetIntegerv(GL_SAMPLE_BUFFERS, &bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(n, 4);
    CHECK_EQ_INT(bufs, 1);

    mgl_target_destroy(&single);
}

/* ---------- rasterising, resolved through a blit ---------- */

GPU_TEST(multisample, edge_resolves_to_partial_coverage)
{
    static const GLfloat tri[6] = { -1,-1, 1,-1, -1,1 };
    MGLTestTarget resolved;
    GLuint msfbo, ms, prog, vao, vbo;
    unsigned char *px;
    int partial = 0, inside = 0, outside = 0;

    prog = buildFor("#version 420\nout vec4 o;\nvoid main(){ o = vec4(0,1,0,1); }\n");
    CHECK(prog != 0);

    if (!prog || !mgl_target_create(&resolved, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    msfbo = msTarget(4, &ms);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glUseProgram(prog);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, msfbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved.fbo);
    glBlitFramebuffer(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolved.fbo);
    px = mgl_read_rgba8(&resolved);

    if (px)
    {
        unsigned char c[4];

        for (int i = 0; i < 64 * 64; i++)
            if (px[i * 4 + 1] > 20 && px[i * 4 + 1] < 235)
                partial++;

        mgl_pixel_at(px, &resolved, 2, 2, c);
        inside = c[1];
        mgl_pixel_at(px, &resolved, 61, 61, c);
        outside = c[1];
        free(px);
    }

    // the hypotenuse crosses 64 pixels; anything single sampled gives none
    CHECK_MSG(partial > 8, "resolved edge had %d partly covered pixels", partial);
    CHECK_EQ_INT(inside, 255);
    CHECK_EQ_INT(outside, 0);

    glUseProgram(0);
    mgl_target_destroy(&resolved);
}

/* ---------- gl_NumSamples ---------- */

// glslang drops gl_NumSamples when it targets SPIR-V, so MGL rewrites it into
// a uniform and writes the framebuffer's sample count into it at draw time.
GPU_TEST(multisample, gl_num_samples_reports_the_framebuffer)
{
    static const GLfloat quad[12] = { -1,-1, 1,-1, -1,1,  1,-1, 1,1, -1,1 };
    GLuint prog, vao, vbo, msfbo, ms;
    MGLTestTarget single, resolved;
    unsigned char c[4];
    unsigned char *px;

    prog = buildFor("#version 420\nout vec4 o;\n"
                    "void main(){ o = vec4(float(gl_NumSamples) / 8.0, 0, 0, 1); }\n");

    CHECK_MSG(prog != 0, "gl_NumSamples did not build");

    if (!prog)
        return;

    if (!mgl_target_create(&single, 64, 64, GL_RGBA8, 0) ||
        !mgl_target_create(&resolved, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glViewport(0, 0, 64, 64);

    // one sample: GL_SAMPLES reads 0, but gl_NumSamples is 1
    mgl_target_bind(&single);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&single);

    if (px)
    {
        mgl_pixel_at(px, &single, 32, 32, c);
        CHECK_EQ_INT(c[0], 32);
        free(px);
    }

    // four samples
    msfbo = msTarget(4, &ms);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, msfbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved.fbo);
    glBlitFramebuffer(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, resolved.fbo);
    px = mgl_read_rgba8(&resolved);

    if (px)
    {
        mgl_pixel_at(px, &resolved, 32, 32, c);
        CHECK_EQ_INT(c[0], 128);
        free(px);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    mgl_target_destroy(&single);
    mgl_target_destroy(&resolved);
}

/* ---------- gl_SampleMask ---------- */

// Writing zero into gl_SampleMask discards the fragment. If it does nothing,
// the quad below covers the target and the clear colour is gone.
/* gl_SampleMask picks samples, and on a target that has only one it does not
   apply at all: GL skips the multisample fragment operations when there is no
   multisample buffer, so a shader that masks every bit off still draws. This
   test used to assert the opposite, on a single-sampled target, which is what
   Metal does by itself and what the CTS says GL must not do. */
GPU_TEST(multisample, sample_mask_applies_only_when_multisampled)
{
    static const GLfloat quad[12] = { -1,-1, 1,-1, -1,1,  1,-1, 1,1, -1,1 };
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    unsigned char c[4], *px;
    GLint loc;

    prog = buildFor("#version 420\nout vec4 o;uniform int mask;\n"
                    "void main(){ for (int i = 0; i < (gl_NumSamples + 31) / 32; ++i)\n"
                    "  gl_SampleMask[i] = mask & gl_SampleMaskIn[i];\n"
                    "  o = vec4(1,0,0,1); }\n");

    CHECK_MSG(prog != 0, "gl_SampleMask did not build");

    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glUseProgram(prog);
    glViewport(0, 0, 64, 64);
    loc = glGetUniformLocation(prog, "mask");

    /* one sample: the mask is not consulted, so this draws even at zero */
    glUniform1i(loc, 0);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 32, 32, c);
        CHECK_MSG(c[0] == 255,
                  "a single-sampled target ignores gl_SampleMask, centre is %u,%u,%u", c[0], c[1], c[2]);
        free(px);
    }

    glUniform1i(loc, ~0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 32, 32, c);
        CHECK_MSG(c[0] == 255, "mask ~0 should have drawn, centre is %u,%u,%u", c[0], c[1], c[2]);
        free(px);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    mgl_target_destroy(&t);

    /* and where there are several samples it does pick them: that is
       sample_mask_picks_the_samples_it_names, below */
}

/* ---------- the other sample variables at least build ---------- */

GPU_TEST(multisample, sample_variables_build)
{
    static const struct { const char *name, *src; } cases[] = {
        { "gl_SampleID",
          "#version 420\nout vec4 o;void main(){o=vec4(float(gl_SampleID));}\n" },
        { "gl_SamplePosition",
          "#version 420\nout vec4 o;void main(){o=vec4(gl_SamplePosition,0,1);}\n" },
        { "gl_SampleMaskIn",
          "#version 420\nout vec4 o;void main(){o=vec4(float(gl_SampleMaskIn[0]));}\n" },
        { "interpolateAtSample",
          "#version 420\nin vec2 uv;out vec4 o;void main(){o=vec4(interpolateAtSample(uv,0),0,1);}\n" },
        { "interpolateAtOffset",
          "#version 420\nin vec2 uv;out vec4 o;void main(){o=vec4(interpolateAtOffset(uv,vec2(0.1)),0,1);}\n" },
        { "interpolateAtCentroid",
          "#version 420\nin vec2 uv;out vec4 o;void main(){o=vec4(interpolateAtCentroid(uv),0,1);}\n" },
        { "sample in",
          "#version 420\nsample in vec2 uv;out vec4 o;void main(){o=vec4(uv,0,1);}\n" },
        { "sampler2DMS",
          "#version 420\nuniform sampler2DMS s;out vec4 o;void main(){o=texelFetch(s,ivec2(0),0);}\n" },
    };
    static const char *vs_uv =
        "#version 420\nlayout(location=0) in vec2 p;out vec2 uv;\n"
        "void main(){uv=p;gl_Position=vec4(p,0,1);}\n";

    for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++)
    {
        char log[1024] = { 0 };
        GLuint p = mgl_build_program(vs_uv, cases[i].src, log, sizeof log);

        CHECK_MSG(p != 0, "%s did not link: %s", cases[i].name, log);

        if (p)
            glDeleteProgram(p);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- glSampleCoverage keeps its fraction ---------- */

GPU_TEST(multisample, sample_coverage_value_is_a_fraction)
{
    GLfloat v = -1.0f;
    GLboolean invert = GL_FALSE;

    glSampleCoverage(0.25f, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFloatv(GL_SAMPLE_COVERAGE_VALUE, &v);
    glGetBooleanv(GL_SAMPLE_COVERAGE_INVERT, &invert);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG(v > 0.24f && v < 0.26f, "GL_SAMPLE_COVERAGE_VALUE reads %f, set 0.25", v);
    CHECK_EQ_INT(invert, GL_TRUE);

    glSampleCoverage(1.0f, GL_FALSE);
}

/* ---------- what each sample actually holds ---------- */

/* sample_mask_discards only looks at the resolved picture, where "some samples
   were killed" and "all of them were" can look alike. This reads the samples
   one at a time, which is what the CTS does and what tells them apart.

   Returns a bitmask of the samples that came back red, or -1 if the shaders
   would not build. */
#define USE_CLEAR_BUFFER 1

static int redSamplesUnderMask2(GLsizei samples, GLint mask, int use_cts_setup, int clear_with_buffer)
{
    static const GLfloat quad[12] = { -1,-1, 1,-1, -1,1,  1,-1, 1,1, -1,1 };
    GLuint ms_fbo, ms_tex = 0, prog, resolve, vao = 0, vbo = 0;
    MGLTestTarget wide;
    unsigned char *px;
    char log[1024];
    int found = 0;

    prog = buildFor("#version 420\n"
                    "out vec4 o; uniform int mask;\n"
                    "void main() {\n"
                    "  for (int i = 0; i < (gl_NumSamples + 31) / 32; ++i)\n"
                    "    gl_SampleMask[i] = mask & gl_SampleMaskIn[i];\n"
                    "  o = vec4(1, 0, 0, 1);\n"
                    "}\n");

    /* one column per sample, so sample s of pixel x lands at x * samples + s */
    resolve = mgl_build_program(VS,
                    "#version 420\n"
                    "uniform sampler2DMS tex; uniform int samples;\n"
                    "layout(location = 0) out vec4 o;\n"
                    "void main() {\n"
                    "  ivec2 c = ivec2(int(gl_FragCoord.x) / samples, int(gl_FragCoord.y));\n"
                    "  o = texelFetch(tex, c, int(gl_FragCoord.x) % samples);\n"
                    "}\n", log, sizeof log);

    if (!prog || !resolve)
        return -1;

    if (use_cts_setup)
    {
        glGenTextures(1, &ms_tex);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms_tex);
        glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA8, 64, 64, GL_FALSE);
        glGenFramebuffers(1, &ms_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, ms_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D_MULTISAMPLE, ms_tex, 0);
    }
    else
        ms_fbo = msTarget(samples, &ms_tex);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return -1;

    vao = mgl_fullscreen_quad(&vbo);
    (void)quad;

    glViewport(0, 0, 64, 64);
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "mask"), mask);
    if (clear_with_buffer)
    {
        GLfloat green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
        glClearBufferfv(GL_COLOR, 0, green);
    }
    else
    {
        glClearColor(0, 1, 0, 1);      /* green is "this sample was not drawn" */
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDrawArrays(GL_TRIANGLES, 0, 6);

    /* read the samples out side by side */
    if (!mgl_target_create(&wide, 64 * samples, 64, GL_RGBA8, 0))
        return -1;

    mgl_target_bind(&wide);
    glViewport(0, 0, 64 * samples, 64);
    glUseProgram(resolve);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms_tex);
    glUniform1i(glGetUniformLocation(resolve, "tex"), 0);
    glUniform1i(glGetUniformLocation(resolve, "samples"), samples);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&wide);

    if (px)
    {
        for (GLsizei s = 0; s < samples; s++)
        {
            unsigned char c[4];

            mgl_pixel_at(px, &wide, 32 * samples + s, 32, c);

            if (c[0] > 200 && c[1] < 60)
                found |= 1 << s;
        }

        free(px);
    }

    glUseProgram(0);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&wide);
    glDeleteFramebuffers(1, &ms_fbo);
    glDeleteTextures(1, &ms_tex);
    glDeleteProgram(prog);
    glDeleteProgram(resolve);

    return found;
}

static int redSamplesUnderMask(GLsizei samples, GLint mask)
{
    return redSamplesUnderMask2(samples, mask, 0, 0);
}

/* glClearBuffer* records its request on the attachment, not in the state-wide
   mask glClear uses. Nothing consumed that, so a program that only ever called
   glClearBufferfv drew onto whatever the texture happened to hold. It reads as
   a sample-mask bug, because the samples the mask spares are the ones left
   showing the uncleared memory. */
GPU_TEST(multisample, clear_buffer_fv_clears_every_sample)
{
    /* with nothing drawn, every sample has to be the colour asked for */
    CHECK_EQ_INT(redSamplesUnderMask2(4, 0x0, 0, USE_CLEAR_BUFFER), 0x0);

    /* and the mask still picks out samples over a cleared target */
    CHECK_EQ_INT(redSamplesUnderMask2(4, 0x5, 0, USE_CLEAR_BUFFER), 0x5);

    /* the immutable multisample storage the CTS uses behaves the same */
    CHECK_EQ_INT(redSamplesUnderMask2(4, 0x5, 1, USE_CLEAR_BUFFER), 0x5);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(multisample, sample_mask_picks_the_samples_it_names)
{
    int got;

    /* every sample, then none of them */
    got = redSamplesUnderMask(4, 0xf);
    if (got < 0) { CHECK_MSG(0, "sample mask shaders would not build"); return; }
    CHECK_EQ_INT(got, 0xf);

    CHECK_EQ_INT(redSamplesUnderMask(4, 0x0), 0x0);

    /* and the ones in between: the mask names samples, one bit each */
    CHECK_EQ_INT(redSamplesUnderMask(4, 0x1), 0x1);
    CHECK_EQ_INT(redSamplesUnderMask(4, 0x8), 0x8);
    CHECK_EQ_INT(redSamplesUnderMask(4, 0x5), 0x5);

    /* a bit past the sample count names nothing */
    CHECK_EQ_INT(redSamplesUnderMask(2, 0x4), 0x0);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* gl_SamplePosition and the GL_SAMPLE_POSITION query have to agree. The query
   used to answer 0.5, 0.5 for every sample whatever the count was. */
GPU_TEST(multisample, sample_positions_match_the_query)
{
    GLuint ms_fbo, ms_tex = 0, prog, resolve, vao, vbo;
    MGLTestTarget wide;
    unsigned char *px;
    char log[1024];
    const GLsizei samples = 4;

    prog = buildFor("#version 420\n"
                    "out vec4 o;\n"
                    "void main() { o = vec4(gl_SamplePosition, 0, 1); }\n");
    resolve = mgl_build_program(VS,
                    "#version 420\n"
                    "uniform sampler2DMS tex; uniform int samples;\n"
                    "layout(location = 0) out vec4 o;\n"
                    "void main() {\n"
                    "  ivec2 c = ivec2(int(gl_FragCoord.x) / samples, int(gl_FragCoord.y));\n"
                    "  o = texelFetch(tex, c, int(gl_FragCoord.x) % samples);\n"
                    "}\n", log, sizeof log);

    if (!prog || !resolve) { CHECK(0); return; }

    ms_fbo = msTarget(samples, &ms_tex);
    vao = mgl_fullscreen_quad(&vbo);
    glViewport(0, 0, 64, 64);
    glUseProgram(prog);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (!mgl_target_create(&wide, 64 * samples, 64, GL_RGBA8, 0)) { CHECK(0); return; }

    mgl_target_bind(&wide);
    glViewport(0, 0, 64 * samples, 64);
    glUseProgram(resolve);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms_tex);
    glUniform1i(glGetUniformLocation(resolve, "tex"), 0);
    glUniform1i(glGetUniformLocation(resolve, "samples"), samples);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&wide);

    if (px)
    {
        for (GLsizei s = 0; s < samples; s++)
        {
            unsigned char c[4];
            GLfloat q[2] = { -1.0f, -1.0f };

            mgl_pixel_at(px, &wide, 32 * samples + s, 32, c);
            glBindFramebuffer(GL_FRAMEBUFFER, ms_fbo);
            glGetMultisamplefv(GL_SAMPLE_POSITION, (GLuint)s, q);

            /* the shader value came back through an 8-bit target */
            CHECK_MSG(fabsf(c[0] / 255.0f - q[0]) < 0.01f && fabsf(c[1] / 255.0f - q[1]) < 0.01f,
                      "sample %d: shader (%.3f, %.3f) but the query says (%.3f, %.3f)",
                      (int)s, c[0] / 255.0, c[1] / 255.0, q[0], q[1]);
        }
        free(px);
    }

    /* every sample sits somewhere different */
    {
        GLfloat a[2] = {0,0}, b[2] = {0,0};

        glBindFramebuffer(GL_FRAMEBUFFER, ms_fbo);
        glGetMultisamplefv(GL_SAMPLE_POSITION, 0, a);
        glGetMultisamplefv(GL_SAMPLE_POSITION, 1, b);
        CHECK(a[0] != b[0] || a[1] != b[1]);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&wide);
    glDeleteFramebuffers(1, &ms_fbo);
    glDeleteTextures(1, &ms_tex);
    glDeleteProgram(prog);
    glDeleteProgram(resolve);
}

/* interpolateAtOffset(v, gl_SamplePosition - 0.5) has to land on the sample,
   so it must equal what a sample-qualified varying already holds. SPIRV-Cross
   shifts the offset by half a pixel because Metal measures from the corner and
   GL from the centre, and upstream takes off another 1/16 for Intel, which on
   this hardware misses the sample by exactly that much. */
GPU_TEST(multisample, interpolate_at_offset_lands_on_the_sample)
{
    static const GLfloat quad[16] = {
        -1,-1, 0,0,   1,-1, 1,0,   -1,1, 0,1,   1,1, 1,1
    };
    GLuint ms_fbo, ms_tex = 0, prog, resolve, vao, vbo;
    MGLTestTarget wide;
    unsigned char *px;
    char log[1024];
    const GLsizei samples = 4;

    /* green says the two agree at this sample */
    prog = mgl_build_program(
        "#version 420\n"
        "layout(location = 0) in vec2 p;\n"
        "layout(location = 1) in vec2 uv;\n"
        "out vec4 v_base;\n"
        "sample out vec4 v_sample;\n"
        "void main() { v_base = vec4(uv, 0, 1); v_sample = vec4(uv, 0, 1);\n"
        "              gl_Position = vec4(p, 0, 1); }\n",
        "#version 420\n"
        "in vec4 v_base;\n"
        "sample in vec4 v_sample;\n"
        "layout(location = 0) out vec4 o;\n"
        "void main() {\n"
        "  vec4 at = interpolateAtOffset(v_base, gl_SamplePosition - 0.5);\n"
        "  bool same = all(lessThan(abs(at.xy - v_sample.xy), vec2(0.002)));\n"
        "  o = same ? vec4(0,1,0,1) : vec4(1,0,0,1);\n"
        "}\n", log, sizeof log);

    resolve = mgl_build_program(VS,
        "#version 420\n"
        "uniform sampler2DMS tex; uniform int samples;\n"
        "layout(location = 0) out vec4 o;\n"
        "void main() {\n"
        "  ivec2 c = ivec2(int(gl_FragCoord.x) / samples, int(gl_FragCoord.y));\n"
        "  o = texelFetch(tex, c, int(gl_FragCoord.x) % samples);\n"
        "}\n", log, sizeof log);

    CHECK_MSG(prog != 0 && resolve != 0, "interpolateAtOffset shaders would not build: %s", log);

    if (!prog || !resolve)
        return;

    ms_fbo = msTarget(samples, &ms_tex);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          (const void *)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glViewport(0, 0, 64, 64);
    glUseProgram(prog);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (!mgl_target_create(&wide, 64 * samples, 64, GL_RGBA8, 0)) { CHECK(0); return; }

    mgl_target_bind(&wide);
    glViewport(0, 0, 64 * samples, 64);
    glUseProgram(resolve);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms_tex);
    glUniform1i(glGetUniformLocation(resolve, "tex"), 0);
    glUniform1i(glGetUniformLocation(resolve, "samples"), samples);
    /* the same four-vertex strip: this VAO holds four, not six */
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    px = mgl_read_rgba8(&wide);

    if (px)
    {
        for (GLsizei s = 0; s < samples; s++)
        {
            unsigned char c[4];

            mgl_pixel_at(px, &wide, 32 * samples + s, 32, c);
            CHECK_MSG(c[1] > 200 && c[0] < 60,
                      "sample %d: interpolateAtOffset missed it, got %u,%u,%u",
                      (int)s, c[0], c[1], c[2]);
        }
        free(px);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&wide);
    glDeleteFramebuffers(1, &ms_fbo);
    glDeleteTextures(1, &ms_tex);
    glDeleteProgram(prog);
    glDeleteProgram(resolve);
}
