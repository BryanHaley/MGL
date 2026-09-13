/*
 * test_fp64.c
 * Copyright (C) The Moogle Project
 *
 * Metal has no double. SPIRV-Cross carries every one as three floats that add
 * up to it, so these check the bits a single float would have thrown away.
 */

#include <string.h>

#include "mgl_test.h"
#include "harness.h"

static GLuint compute_program(const char *src)
{
    GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        char log[1024] = {0};
        glGetShaderInfoLog(sh, sizeof log, NULL, log);
        CHECK_MSG(0, "compute shader would not compile: %s", log);
        glDeleteShader(sh);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, sh);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(sh);

    if (!ok)
    {
        glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

/* Runs a one-invocation shader whose body writes a float to `r` and returns it. */
static float run_scalar(const char *body)
{
    char src[1024];
    const char *p = src;

    snprintf(src, sizeof src,
             "#version 460 core\n"
             "layout(local_size_x = 1) in;\n"
             "layout(std430, binding = 0) buffer Out { float r; };\n"
             "void main() { %s }\n", body);

    GLuint prog = compute_program(p);
    if (!prog) return -1000.0f;

    float seed = -1000.0f, got = -1000.0f;
    GLuint ssbo = 0;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof seed, &seed, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, &got);

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);

    return got;
}

GPU_TEST(fp64, extension_is_advertised)
{
    GLint n = 0;
    int found = 0;

    glGetIntegerv(GL_NUM_EXTENSIONS, &n);

    for (GLint i = 0; i < n; i++)
    {
        const char *s = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);

        if (s && !strcmp(s, "GL_ARB_gpu_shader_fp64"))
            found = 1;
    }

    CHECK(found);
}

GPU_TEST(fp64, simple_arithmetic)
{
    CHECK_NEAR(3.0f, run_scalar("r = float(1.0LF + 2.0LF);"), 1e-5f);
    CHECK_NEAR(0.5f, run_scalar("r = float(1.0LF / 2.0LF);"), 1e-5f);
}

/* 1e-10 added to 1.0 disappears in a 24-bit float and survives in a double. */
GPU_TEST(fp64, keeps_bits_a_float_cannot)
{
    float r = run_scalar("double a = 1.0LF;\n"
                         "double b = a + 1.0e-10LF;\n"
                         "r = float((b - a) * 1.0e10LF);");

    CHECK_NEAR(1.0f, r, 1e-3f);
}

/* One third is not representable in either, but a double gets 29 more bits of
   it than a float does. */
GPU_TEST(fp64, division_is_more_exact_than_float)
{
    float r = run_scalar("double t = 1.0LF / 3.0LF;\n"
                         "r = float((t * 3.0LF - 1.0LF) * 1.0e9LF);");

    CHECK_NEAR(0.0f, r, 1e-3f);
}
