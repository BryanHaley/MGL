/*
 * test_fp64.c
 * Copyright (C) The MooGL Project
 *
 * Metal has no double. SPIRV-Cross carries every one as three floats that add
 * up to it, so these check the bits a single float would have thrown away.
 */

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

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

/* glslang writes "!=" on floats as an unordered compare. Its helper sat with
   the vector operations, so a shader using only scalar doubles called a
   function that was never emitted and failed to compile. */
GPU_TEST(fp64, not_equal_works_with_no_double_vectors_in_sight)
{
    CHECK_NEAR(1.0f, run_scalar("double a = double(gl_GlobalInvocationID.x) + 1.0LF;\n"
                                "r = (a != 1.0LF) ? 0.0 : 1.0;"), 1e-5f);
    CHECK_NEAR(2.0f, run_scalar("double a = double(gl_GlobalInvocationID.x) + 1.0LF;\n"
                                "r = (a != 2.0LF) ? 2.0 : 0.0;"), 1e-5f);
}

/* Comparing arrays of dmat walks down through the array before it reaches a
   column. The emulation took the array index for a column and named a member
   the array does not have, so the shader did not compile. */
GPU_TEST(fp64, arrays_of_matrices_compare_element_by_element)
{
    CHECK_NEAR(1.0f, run_scalar("dmat2 x[2][2]; dmat2 y[2][2];\n"
                                "for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) {\n"
                                "  x[a][b] = dmat2(double(a + b)); y[a][b] = dmat2(double(a + b)); }\n"
                                "r = (x == y) ? 1.0 : 0.0;"), 1e-5f);
    CHECK_NEAR(0.0f, run_scalar("dmat2 x[2][2]; dmat2 y[2][2];\n"
                                "for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) {\n"
                                "  x[a][b] = dmat2(double(a + b)); y[a][b] = dmat2(double(a + b)); }\n"
                                "y[1][1][1][0] = 9.0lf;\n"
                                "r = (x == y) ? 1.0 : 0.0;"), 1e-5f);
}

// Metal has no double vertex format, so doubles arrive as raw bits. GLSL
// gives a vertex input one location even for a dvec3 or dvec4, and a Metal
// attribute holds 16 bytes, so the first two doubles come in at the location
// and the rest at 16 + the location. Doubles used to take an attribute index
// each, which collided with the next attribute's.
GPU_TEST(fp64, double_vertex_attributes_arrive_whole)
{
    static const char *vs =
        "#version 450\n"
        "layout(location = 0) in dvec2 a;\n"
        "layout(location = 1) in dvec3 b;\n"
        "layout(location = 2) in dvec4 c;\n"
        "layout(location = 3) in vec2 p;\n"
        "flat out int ok;\n"
        "void main() {\n"
        "  // one bit per value, so a miss says which\n"
        "  ok = (a.x == 1.0lf / 3.0lf ? 1 : 0) | (a.y == 2.5lf ? 2 : 0) | (b.x == 0.1lf ? 4 : 0) |\n"
        "       (b.y == 1e-10lf + 1.0lf ? 8 : 0) | (b.z == -7.0lf ? 16 : 0) |\n"
        "       (c == dvec4(0.2lf, 0.3lf, 1e-12lf, 4.0lf) ? 32 : 0);\n"
        "  gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *fs =
        "#version 450\n"
        "flat in int ok;\n"
        "out vec4 o;\n"
        "void main() { o = ok == 63 ? vec4(0, 1, 0, 1) : vec4(1, 0, float(ok) / 255.0, 1); }\n";
    static const float tri[] = { -1, -1,  3, -1,  -1, 3 };
    GLdouble a[3][2], b[3][3];
    GLuint prog, vao, bufs[3];
    GLint is_long = 0;
    MGLTestTarget t;
    unsigned char c[4] = { 0 };
    unsigned char *px;
    char log[2048];

    for (int v = 0; v < 3; v++)
    {
        a[v][0] = 1.0 / 3.0; a[v][1] = 2.5;
        b[v][0] = 0.1; b[v][1] = 1e-10 + 1.0; b[v][2] = -7.0;
    }

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "double attribute program did not build: %s", log);

    if (!prog || !mgl_target_create(&t, 8, 8, GL_RGBA8, 0))
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(3, bufs);

    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof a, a, GL_STATIC_DRAW);
    glVertexAttribLPointer(0, 2, GL_DOUBLE, 0, NULL);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, bufs[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof b, b, GL_STATIC_DRAW);
    glVertexAttribLPointer(1, 3, GL_DOUBLE, 0, NULL);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, bufs[2]);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(3);

    // location 2 stays disabled and reads the constant, which only a double
    // holds exactly
    glDisableVertexAttribArray(2);
    glVertexAttribL4d(2, 0.2, 0.3, 1e-12, 4.0);

    glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_LONG, &is_long);
    CHECK_EQ_INT(is_long, GL_TRUE);

    mgl_target_bind(&t);
    glViewport(0, 0, 8, 8);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 4, 4, c);
        CHECK_MSG(c[1] > 200 && c[0] < 50, "values that arrived right, one bit each: %d of 63", c[2]);
        free(px);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(3, bufs);
    mgl_target_destroy(&t);
}

// One double built-in on a dvec2 uniform, read back as a double
static double run_uniform(const char *expr, double a, double b)
{
    char src[1024];
    GLuint prog, ssbo = 0;
    double got = -12345.0;

    snprintf(src, sizeof src,
             "#version 460 core\nlayout(local_size_x = 1) in;\n"
             "layout(std430, binding = 0) buffer Out { double r; };\n"
             "uniform dvec2 u;\nvoid main() { r = %s; }\n", expr);
    prog = compute_program(src);
    if (!prog)
        return got;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof got, &got, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glUseProgram(prog);
    glUniform2d(glGetUniformLocation(prog, "u"), a, b);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, &got);

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);

    return got;
}

// Values that came back wrong in the conformance suite, which picks fresh
// random arguments on every run -- so the failures looked like a flake.
GPU_TEST(fp64, builtins_hold_for_the_values_that_failed)
{
    static const struct { const char *expr; double a, b; } cases[] = {
        { "normalize(u).x", 219.5, 219.625 },
        { "normalize(u).y", -324.5, -324.375 },
        { "normalize(u).x", 37.5, 37.625 },
        { "sqrt(u).x", 89.5, 89.625 },
        { "inversesqrt(u).y", 428.5, 428.625 },
        { "mod(u.x, u.y)", -15.5, -11.5 },
        { "length(u)", -372.5, -372.375 },
    };

    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        double a = cases[i].a, b = cases[i].b, len = sqrt(a * a + b * b), want;

        switch (i)
        {
            case 0: case 2: want = a / len; break;
            case 1: want = b / len; break;
            case 3: want = sqrt(a); break;
            case 4: want = 1.0 / sqrt(b); break;
            case 5: want = a - b * floor(a / b); break;
            default: want = len; break;
        }

        double got = run_uniform(cases[i].expr, a, b);

        CHECK_MSG(fabs(got - want) <= 1e-9 * (fabs(want) + 1.0), "%s of (%g, %g): got %.12g, want %.12g",
                  cases[i].expr, a, b, got, want);
    }
}

// The same values through a vertex stage and transform feedback, the way the
// conformance suite runs them
static double run_vertex(const char *expr, double a, double b)
{
    static const char *FS = "#version 400 core\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    const char *varying = "result_0";
    char vs[512];
    GLuint vao, xfb, prog = glCreateProgram(), s[2];
    double got[2] = { -12345.0, -12345.0 };

    snprintf(vs, sizeof vs, "#version 400 core\nuniform dvec2 u;\nflat out dvec2 result_0;\n"
             "void main() { result_0 = dvec2(%s); }\n", expr);

    const char *src[2] = { vs, FS };
    GLenum kinds[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };

    for (int i = 0; i < 2; i++)
    {
        s[i] = glCreateShader(kinds[i]);
        glShaderSource(s[i], 1, &src[i], NULL);
        glCompileShader(s[i]);
        glAttachShader(prog, s[i]);
    }
    glTransformFeedbackVaryings(prog, 1, &varying, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &xfb);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof got, got, GL_DYNAMIC_COPY);
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb, 0, sizeof got);
    glUseProgram(prog);
    glUniform2d(glGetUniformLocation(prog, "u"), a, b);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, 1);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);

    double *m = (double *)glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, GL_READ_ONLY);
    if (m)
        memcpy(got, m, sizeof got);
    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);

    glUseProgram(0);
    for (int i = 0; i < 2; i++)
        glDeleteShader(s[i]);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &xfb);
    glDeleteVertexArrays(1, &vao);

    return got[0];
}

GPU_TEST(fp64, vertex_builtins_hold_for_the_values_that_failed)
{
    double a = 219.5, b = 219.625, len = sqrt(a * a + b * b);
    double got;

    got = run_vertex("normalize(u).x", a, b);
    CHECK_MSG(fabs(got - a / len) <= 1e-9, "normalize: got %.12g, want %.12g", got, a / len);
    got = run_vertex("length(u)", a, b);
    CHECK_MSG(fabs(got - len) <= 1e-9 * len, "length: got %.12g, want %.12g", got, len);
    got = run_vertex("sqrt(u).x", 89.5, 89.625);
    CHECK_MSG(fabs(got - sqrt(89.5)) <= 1e-9 * 10, "sqrt: got %.12g, want %.12g", got, sqrt(89.5));
    got = run_vertex("mod(u.x, u.y)", -15.5, -11.5);
    CHECK_MSG(fabs(got + 4.0) <= 1e-9, "mod: got %.12g, want -4", got);
}

// The conformance suite's own loop: one program, a new feedback buffer and a
// new value before every draw, the arguments it uses. About one draw in 600
// came back wrong, a different one each run: adding two emulated doubles read
// one element past a three-float array, and the compiler took that as licence
// to drop the bounds check, so the merge sometimes used stack garbage.
GPU_TEST(fp64, one_program_many_draws_with_new_values)
{
    static const char *VS = "#version 400 core\nuniform dvec2 u;\nflat out dvec2 result_0;\n"
                            "void main() { result_0 = normalize(u); }\n";
    static const char *FS = "#version 400 core\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    const char *varying = "result_0";
    GLuint vao, xfb, prog = glCreateProgram(), s[2];
    const char *src[2] = { VS, FS };
    GLenum kinds[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
    int wrong = 0;

    for (int i = 0; i < 2; i++)
    {
        s[i] = glCreateShader(kinds[i]);
        glShaderSource(s[i], 1, &src[i], NULL);
        glCompileShader(s[i]);
        glAttachShader(prog, s[i]);
    }
    glTransformFeedbackVaryings(prog, 1, &varying, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &xfb);
    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);

    for (int v = 0; v < 4096; v++)
    {
        double a = -511.5 + (v % 1024), b = a + 0.125;
        double zero[2] = { 0, 0 }, got[2] = { 0, 0 }, len = sqrt(a * a + b * b);

        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
        glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof zero, zero, GL_DYNAMIC_COPY);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb, 0, sizeof zero);
        glBindVertexArray(vao);
        glUniform2dv(glGetUniformLocation(prog, "u"), 1, (const double[]){ a, b });

        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, 1);
        glEndTransformFeedback();

        glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
        double *m = (double *)glMapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, GL_READ_ONLY);
        if (m)
            memcpy(got, m, sizeof got);
        glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);

        if (fabs(got[0] - a / len) > 1e-9 || fabs(got[1] - b / len) > 1e-9)
        {
            if (wrong++ == 0)
                fprintf(stderr, "draw %d (%g, %g): got (%.9g, %.9g) want (%.9g, %.9g)\n",
                        v, a, b, got[0], got[1], a / len, b / len);
        }
    }

    CHECK_MSG(wrong == 0, "%d of 4096 draws wrong", wrong);

    glDisable(GL_RASTERIZER_DISCARD);
    glUseProgram(0);
    for (int i = 0; i < 2; i++)
        glDeleteShader(s[i]);
    glDeleteProgram(prog);
    glDeleteBuffers(1, &xfb);
    glDeleteVertexArrays(1, &vao);
}

// Long chains of emulated arithmetic, thousands of dispatches, each checked
// as a double. Apple's compiler sometimes made the helpers real calls with a
// thread array passed by reference, and then some dispatches -- different ones
// in each process -- lost the low parts or never stored a result.
static int chain_errors(const char *expr, double (*want)(double))
{
    char src[512];
    GLuint prog, ssbo;
    static double out[8192];
    int wrong = 0;

    snprintf(src, sizeof src, "#version 460 core\nlayout(local_size_x = 1) in;\n"
             "layout(std430, binding = 0) buffer Out { double r[]; };\n"
             "uniform double u;\nuniform int slot;\nvoid main() { r[slot] = %s; }\n", expr);
    prog = compute_program(src);
    if (!prog)
        return -1;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof out, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glUseProgram(prog);

    for (int v = 0; v < 8192; v++)
    {
        glUniform1d(glGetUniformLocation(prog, "u"), (double)(v % 1024) + 0.5);
        glUniform1i(glGetUniformLocation(prog, "slot"), v);
        glDispatchCompute(1, 1, 1);
    }
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof out, out);

    for (int v = 0; v < 8192; v++)
    {
        double w = want((double)(v % 1024) + 0.5);

        if (fabs(out[v] - w) > 1e-9 * fabs(w))
            wrong++;
    }

    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);

    return wrong;
}

static double want_pow6(double x) { return x * x * x * x * x * x; }
static double want_inv(double x) { return 1.0 / x; }
static double want_sqrt(double x) { return sqrt(x); }

GPU_TEST(fp64, long_chains_stay_exact_over_many_dispatches)
{
    CHECK_EQ_INT(chain_errors("u * u * u * u * u * u", want_pow6), 0);
    CHECK_EQ_INT(chain_errors("1.0LF / u", want_inv), 0);
    CHECK_EQ_INT(chain_errors("sqrt(u)", want_sqrt), 0);
}
