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

/* Runs a shader that already declares its own buffer, and returns the float
   it writes to `r`. The body may use anything the declarations set up. */
static float run_with_decls(const char *decls, const char *body)
{
    char src[4096];

    snprintf(src, sizeof src,
             "#version 460 core\n"
             "layout(local_size_x = 1) in;\n"
             "layout(std430, binding = 0) buffer Out { float r; };\n"
             "%s\n"
             "void main() { %s }\n", decls, body);

    GLuint prog = compute_program(src);
    if (!prog) return -1000.0f;

    float seed = -1000.0f, got = -1000.0f;
    GLuint ssbo = 0, scratch = 0;
    unsigned char zeros[1024] = { 0 };

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof seed, &seed, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    /* whatever the declarations asked for lives at binding 1 */
    glGenBuffers(1, &scratch);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, scratch);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zeros, zeros, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, scratch);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, &got);

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteBuffers(1, &scratch);
    glDeleteProgram(prog);

    return got;
}

/* A dvec is a struct of three-float doubles, so every operator on it is a call
   MGL generates. These check the generated ones agree with the arithmetic. */
GPU_TEST(fp64, vectors_do_arithmetic)
{
    CHECK_NEAR(10.0f, run_scalar("dvec4 a = dvec4(1.0LF, 2.0LF, 3.0LF, 4.0LF);\n"
                                 "r = float(a.x + a.y + a.z + a.w);"), 1e-5f);
    /* a swizzle has to name components, not letters Metal does not have */
    CHECK_NEAR(5.0f, run_scalar("dvec4 a = dvec4(1.0LF, 2.0LF, 3.0LF, 4.0LF);\n"
                                "dvec2 b = a.zy;\n"
                                "r = float(b.x + b.y);"), 1e-5f);
    CHECK_NEAR(20.0f, run_scalar("dvec4 a = dvec4(1.0LF, 2.0LF, 3.0LF, 4.0LF);\n"
                                 "dvec4 b = a * 2.0LF;\n"
                                 "r = float(b.x + b.y + b.z + b.w);"), 1e-5f);
    /* abs gives (1,2,3): 1 - 2 + 3 - 4 */
    CHECK_NEAR(-2.0f, run_scalar("dvec3 a = dvec3(-1.0LF, 2.0LF, -3.0LF);\n"
                                 "dvec3 b = abs(a);\n"
                                 "r = float(b.x - b.y + b.z - 4.0LF);"), 1e-5f);
}

GPU_TEST(fp64, vector_builtins_answer)
{
    CHECK_NEAR(32.0f, run_scalar("r = float(dot(dvec3(1.0LF,2.0LF,3.0LF), dvec3(4.0LF,5.0LF,6.0LF)));"), 1e-4f);
    CHECK_NEAR(5.0f,  run_scalar("r = float(length(dvec2(3.0LF, 4.0LF)));"), 1e-4f);
    CHECK_NEAR(1.0f,  run_scalar("r = float(length(normalize(dvec3(1.0LF, 2.0LF, 3.0LF))));"), 1e-4f);
    /* cross of x and y is z */
    CHECK_NEAR(1.0f,  run_scalar("dvec3 c = cross(dvec3(1.0LF,0.0LF,0.0LF), dvec3(0.0LF,1.0LF,0.0LF));\n"
                                 "r = float(c.z);"), 1e-5f);
    CHECK_NEAR(2.5f,  run_scalar("r = float(mix(dvec2(2.0LF), dvec2(3.0LF), 0.5LF).x);"), 1e-5f);
    CHECK_NEAR(3.0f,  run_scalar("r = float(clamp(dvec2(9.0LF), dvec2(1.0LF), dvec2(3.0LF)).y);"), 1e-5f);
}

/* A dmat is a struct of columns. The product, the transpose and the inverse are
   all written out by MGL, so a round trip through them has to come back. */
GPU_TEST(fp64, matrices_multiply_and_invert)
{
    /* identity times anything is that thing */
    CHECK_NEAR(7.0f, run_scalar("dmat3 i = dmat3(1.0LF);\n"
                                "dmat3 m = dmat3(7.0LF, 0.0LF, 0.0LF,\n"
                                "                0.0LF, 1.0LF, 0.0LF,\n"
                                "                0.0LF, 0.0LF, 1.0LF);\n"
                                "dmat3 p = i * m;\n"
                                "r = float(p[0][0]);"), 1e-4f);
    CHECK_NEAR(6.0f, run_scalar("dmat2 m = dmat2(2.0LF, 0.0LF, 0.0LF, 3.0LF);\n"
                                "r = float(determinant(m));"), 1e-4f);
    /* m times its inverse is the identity */
    CHECK_NEAR(1.0f, run_scalar("dmat3 m = dmat3(2.0LF, 1.0LF, 0.0LF,\n"
                                "                0.0LF, 3.0LF, 0.0LF,\n"
                                "                1.0LF, 0.0LF, 4.0LF);\n"
                                "dmat3 p = m * inverse(m);\n"
                                "r = float(p[0][0] + p[1][1] + p[2][2] - 2.0LF);"), 1e-3f);
    CHECK_NEAR(3.0f, run_scalar("dmat2x3 m = dmat2x3(1.0LF, 2.0LF, 3.0LF, 4.0LF, 5.0LF, 6.0LF);\n"
                                "dmat3x2 t = transpose(m);\n"
                                "r = float(t[2][0]);"), 1e-4f);
    /* a matrix times a vector is its columns scaled and summed: the columns
       are (1,2) and (3,4), so the result is (1*2+3*3, 2*2+4*3) = (11, 16) */
    CHECK_NEAR(27.0f, run_scalar("dmat2 m = dmat2(1.0LF, 2.0LF, 3.0LF, 4.0LF);\n"
                                 "dvec2 v = m * dvec2(2.0LF, 3.0LF);\n"
                                 "r = float(v.x + v.y);"), 1e-4f);
}

/* In a buffer a double keeps its binary64 bits, so these check the slots line up
   with what GL laid out -- including a dvec3 column, which is padded. */
GPU_TEST(fp64, doubles_round_trip_through_a_block)
{
    float r = run_with_decls(
        "layout(std430, binding = 1) buffer Data { double s; dvec2 v2; dvec3 v3; dvec4 v4; dmat3 m3; };",
        "s = 1.0LF; v2 = dvec2(2.0LF, 3.0LF); v3 = dvec3(4.0LF, 5.0LF, 6.0LF);\n"
        "v4 = dvec4(7.0LF, 8.0LF, 9.0LF, 10.0LF);\n"
        "m3 = dmat3(11.0LF, 0.0LF, 0.0LF, 0.0LF, 12.0LF, 0.0LF, 0.0LF, 0.0LF, 13.0LF);\n"
        "r = float(s + v2.x + v2.y + v3.x + v3.y + v3.z\n"
        "          + v4.x + v4.y + v4.z + v4.w + m3[0][0] + m3[1][1] + m3[2][2]);");

    /* 1+2+3+4+5+6+7+8+9+10+11+12+13 */
    CHECK_NEAR(91.0f, r, 1e-3f);
}

/* A double that comes in as a plain uniform, rather than inside a block, is a
   run of slots the shader has to decode for itself. */
GPU_TEST(fp64, plain_uniform_doubles_arrive_intact)
{
    const char *src =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430, binding = 0) buffer Out { float r; };\n"
        "uniform double us;\n"
        "uniform dvec4 uv;\n"
        "uniform dmat2 um;\n"
        "void main() { r = float(us + uv.x + uv.y + uv.z + uv.w + um[0][0] + um[1][1]); }\n";

    GLuint prog = compute_program(src);
    CHECK(prog != 0);
    if (!prog) return;

    float seed = -1000.0f, got = -1000.0f;
    GLuint ssbo = 0;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof seed, &seed, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    {
        const double m[4] = { 6.0, 0.0, 0.0, 7.0 };

        glUniform1d(glGetUniformLocation(prog, "us"), 1.0);
        glUniform4d(glGetUniformLocation(prog, "uv"), 2.0, 3.0, 4.0, 5.0);
        glUniformMatrix2dv(glGetUniformLocation(prog, "um"), 1, GL_FALSE, m);
    }
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, &got);

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);

    /* 1+2+3+4+5+6+7 */
    CHECK_NEAR(28.0f, got, 1e-3f);
}
