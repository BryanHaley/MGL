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
GPU_TEST(multisample, sample_mask_discards)
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

    glUniform1i(loc, 0);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 32, 32, c);
        CHECK_MSG(c[2] == 255 && c[0] == 0,
                  "mask 0 should have discarded everything, centre is %u,%u,%u", c[0], c[1], c[2]);
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
