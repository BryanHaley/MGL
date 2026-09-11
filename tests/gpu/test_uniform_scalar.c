/*
 * test_uniform_scalar.c
 * MGL
 *
 * glUniform{1,2,3,4}{f,i,ui,d}{,v} scalar/vector uniforms,
 * glUniformBlockBinding and glUniformSubroutinesuiv.
 */

#include "mgl_test.h"
#include "harness.h"

static const char *VS_SIMPLE =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 0) uniform vec4 u_f4;\n"
    "layout(location = 1) uniform ivec4 u_i4;\n"
    "layout(location = 2) uniform uvec4 u_u4;\n"
    "layout(location = 3) uniform dvec4 u_d4;\n"
    // one uniform per width, so a 1/2/3-component setter has something it
    // actually matches -- calling glUniform1f on a vec4 is INVALID_OPERATION
    "layout(location = 4) uniform float u_f1;\n"
    "layout(location = 5) uniform vec2  u_f2;\n"
    "layout(location = 6) uniform vec3  u_f3;\n"
    "layout(location = 7) uniform int   u_i1;\n"
    "layout(location = 8) uniform ivec2 u_i2;\n"
    "layout(location = 9) uniform ivec3 u_i3;\n"
    "layout(location = 10) uniform uint  u_u1;\n"
    "layout(location = 11) uniform uvec2 u_u2;\n"
    "layout(location = 12) uniform uvec3 u_u3;\n"
    "layout(location = 13) uniform double u_d1;\n"
    "layout(location = 14) uniform dvec2  u_d2;\n"
    "layout(location = 15) uniform dvec3  u_d3;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0)\n"
    "  + vec4(u_f1) + vec4(u_f2,0,0) + vec4(u_f3,0)\n"
    "  + vec4(u_i1) + vec4(u_i2,0,0) + vec4(u_i3,0)\n"
    "  + vec4(u_u1) + vec4(u_u2,0,0) + vec4(u_u3,0)\n"
    // the double uniforms are declared but never referenced: MSL has no
    // double type, so using one here makes the whole shader fail to compile
    "  + u_f4 + vec4(u_i4) + vec4(u_u4); }\n";

static const char *FS_SIMPLE =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(1.0); }\n";

/* ---------- float scalar and vector ---------- */

GPU_TEST(uniform_scalar, float_scalar_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLfloat f[4] = { -9, -9, -9, -9 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);

    glUniform1f(glGetUniformLocation(p, "u_f1"), 0.25f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, glGetUniformLocation(p, "u_f1"), f);
    CHECK_NEAR(f[0], 0.25f, 1e-6f);

    glUniform2f(glGetUniformLocation(p, "u_f2"), 0.5f, 0.75f);
    glGetUniformfv(p, glGetUniformLocation(p, "u_f2"), f);
    CHECK_NEAR(f[0], 0.5f,  1e-6f);
    CHECK_NEAR(f[1], 0.75f, 1e-6f);

    glUniform3f(glGetUniformLocation(p, "u_f3"), 1.0f, 2.0f, 3.0f);
    glGetUniformfv(p, glGetUniformLocation(p, "u_f3"), f);
    CHECK_NEAR(f[0], 1.0f, 1e-6f);
    CHECK_NEAR(f[2], 3.0f, 1e-6f);

    glUniform4f(glGetUniformLocation(p, "u_f4"), 1.0f, 2.0f, 3.0f, 4.0f);
    glGetUniformfv(p, glGetUniformLocation(p, "u_f4"), f);
    CHECK_NEAR(f[3], 4.0f, 1e-6f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}

GPU_TEST(uniform_scalar, float_vector_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLint loc;
    const GLfloat vals[4] = { 0.1f, 0.2f, 0.3f, 0.4f };
    GLfloat f[4] = { 0 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);
    loc = glGetUniformLocation(p, "u_f4");

    glUniform1fv(loc, 1, vals);
    glGetUniformfv(p, loc, f);
    CHECK_NEAR(f[0], 0.1f, 1e-6f);
    CHECK_NEAR(f[1], 0.0f, 1e-6f);

    glUniform2fv(loc, 1, vals);
    glGetUniformfv(p, loc, f);
    CHECK_NEAR(f[0], 0.1f, 1e-6f);
    CHECK_NEAR(f[1], 0.2f, 1e-6f);

    glUniform3fv(loc, 1, vals);
    glGetUniformfv(p, loc, f);
    CHECK_NEAR(f[0], 0.1f, 1e-6f);
    CHECK_NEAR(f[1], 0.2f, 1e-6f);
    CHECK_NEAR(f[2], 0.3f, 1e-6f);

    glUseProgram(0);
    glDeleteProgram(p);
}

/* ---------- integer scalar and vector ---------- */

GPU_TEST(uniform_scalar, int_scalar_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLint iv[4] = { -9, -9, -9, -9 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);

    glUniform1i(glGetUniformLocation(p, "u_i1"), 42);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, glGetUniformLocation(p, "u_i1"), iv);
    CHECK_EQ_INT(iv[0], 42);

    glUniform2i(glGetUniformLocation(p, "u_i2"), 10, 20);
    glGetUniformiv(p, glGetUniformLocation(p, "u_i2"), iv);
    CHECK_EQ_INT(iv[0], 10);
    CHECK_EQ_INT(iv[1], 20);

    glUniform3i(glGetUniformLocation(p, "u_i3"), 1, 2, 3);
    glGetUniformiv(p, glGetUniformLocation(p, "u_i3"), iv);
    CHECK_EQ_INT(iv[0], 1);
    CHECK_EQ_INT(iv[2], 3);

    glUniform4i(glGetUniformLocation(p, "u_i4"), 5, 6, 7, 8);
    glGetUniformiv(p, glGetUniformLocation(p, "u_i4"), iv);
    CHECK_EQ_INT(iv[3], 8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}

GPU_TEST(uniform_scalar, int_vector_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLint loc;
    const GLint vals[4] = { 100, 200, 300, 400 };
    GLint iv[4] = { 0 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);
    loc = glGetUniformLocation(p, "u_i4");

    glUniform1iv(loc, 1, vals);
    glGetUniformiv(p, loc, iv);
    CHECK_EQ_INT(iv[0], 100);
    CHECK_EQ_INT(iv[1], 0);

    glUniform2iv(loc, 1, vals);
    glGetUniformiv(p, loc, iv);
    CHECK_EQ_INT(iv[0], 100);
    CHECK_EQ_INT(iv[1], 200);

    glUniform3iv(loc, 1, vals);
    glGetUniformiv(p, loc, iv);
    CHECK_EQ_INT(iv[0], 100);
    CHECK_EQ_INT(iv[1], 200);
    CHECK_EQ_INT(iv[2], 300);

    glUniform4iv(loc, 1, vals);
    glGetUniformiv(p, loc, iv);
    CHECK_EQ_INT(iv[0], 100);
    CHECK_EQ_INT(iv[1], 200);
    CHECK_EQ_INT(iv[2], 300);
    CHECK_EQ_INT(iv[3], 400);

    glUseProgram(0);
    glDeleteProgram(p);
}

/* ---------- unsigned integer scalar and vector ---------- */

GPU_TEST(uniform_scalar, uint_scalar_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLuint uv[4] = { 99, 99, 99, 99 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);

    glUniform1ui(glGetUniformLocation(p, "u_u1"), 7u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, glGetUniformLocation(p, "u_u1"), uv);
    CHECK_EQ_UINT(uv[0], 7u);

    glUniform2ui(glGetUniformLocation(p, "u_u2"), 11u, 22u);
    glGetUniformuiv(p, glGetUniformLocation(p, "u_u2"), uv);
    CHECK_EQ_UINT(uv[0], 11u);
    CHECK_EQ_UINT(uv[1], 22u);

    glUniform3ui(glGetUniformLocation(p, "u_u3"), 1u, 2u, 3u);
    glGetUniformuiv(p, glGetUniformLocation(p, "u_u3"), uv);
    CHECK_EQ_UINT(uv[0], 1u);
    CHECK_EQ_UINT(uv[2], 3u);

    glUniform4ui(glGetUniformLocation(p, "u_u4"), 4u, 5u, 6u, 7u);
    glGetUniformuiv(p, glGetUniformLocation(p, "u_u4"), uv);
    CHECK_EQ_UINT(uv[3], 7u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}

GPU_TEST(uniform_scalar, uint_vector_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLint loc;
    const GLuint vals[4] = { 10u, 20u, 30u, 40u };
    GLuint uv[4] = { 0 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);
    loc = glGetUniformLocation(p, "u_u4");

    glUniform1uiv(loc, 1, vals);
    glGetUniformuiv(p, loc, uv);
    CHECK_EQ_UINT(uv[0], 10u);
    CHECK_EQ_UINT(uv[1], 0u);

    glUniform2uiv(loc, 1, vals);
    glGetUniformuiv(p, loc, uv);
    CHECK_EQ_UINT(uv[0], 10u);
    CHECK_EQ_UINT(uv[1], 20u);

    glUniform3uiv(loc, 1, vals);
    glGetUniformuiv(p, loc, uv);
    CHECK_EQ_UINT(uv[0], 10u);
    CHECK_EQ_UINT(uv[1], 20u);
    CHECK_EQ_UINT(uv[2], 30u);

    glUniform4uiv(loc, 1, vals);
    glGetUniformuiv(p, loc, uv);
    CHECK_EQ_UINT(uv[0], 10u);
    CHECK_EQ_UINT(uv[1], 20u);
    CHECK_EQ_UINT(uv[2], 30u);
    CHECK_EQ_UINT(uv[3], 40u);

    glUseProgram(0);
    glDeleteProgram(p);
}

/* ---------- double scalar and vector ---------- */

GPU_TEST(uniform_scalar, double_scalar_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLdouble d[4] = { -9, -9, -9, -9 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);

    glUniform1d(glGetUniformLocation(p, "u_d1"), 1.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, glGetUniformLocation(p, "u_d1"), d);
    CHECK_NEAR(d[0], 1.5, 1e-12);

    glUniform2d(glGetUniformLocation(p, "u_d2"), 2.5, 3.5);
    glGetUniformdv(p, glGetUniformLocation(p, "u_d2"), d);
    CHECK_NEAR(d[0], 2.5, 1e-12);
    CHECK_NEAR(d[1], 3.5, 1e-12);

    glUniform3d(glGetUniformLocation(p, "u_d3"), 4.0, 5.0, 6.0);
    glGetUniformdv(p, glGetUniformLocation(p, "u_d3"), d);
    CHECK_NEAR(d[0], 4.0, 1e-12);
    CHECK_NEAR(d[2], 6.0, 1e-12);

    // this one caught mglUniform4d writing a literal 2 in place of w
    glUniform4d(glGetUniformLocation(p, "u_d4"), 7.0, 8.0, 9.0, 10.0);
    glGetUniformdv(p, glGetUniformLocation(p, "u_d4"), d);
    CHECK_NEAR(d[3], 10.0, 1e-12);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}

// The spec rejects a setter whose component count differs from the declared
// uniform, rather than writing part of it.
GPU_TEST(uniform_scalar, size_mismatch_is_rejected)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLint loc;

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);
    loc = glGetUniformLocation(p, "u_f4");
    (void)mgl_drain_errors();

    glUniform1f(loc, 0.25f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2f(loc, 0.5f, 0.75f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3f(loc, 1.0f, 2.0f, 3.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUseProgram(0);
    glDeleteProgram(p);
}

GPU_TEST(uniform_scalar, double_vector_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLint loc;
    const GLdouble vals[4] = { 1.5, 2.5, 3.5, 4.5 };
    GLdouble d[4] = { 0 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);
    loc = glGetUniformLocation(p, "u_d4");

    glUniform1dv(loc, 1, vals);
    glGetUniformdv(p, loc, d);
    CHECK_NEAR(d[0], 1.5, 1e-10);
    CHECK_NEAR(d[1], 0.0, 1e-10);

    glUniform2dv(loc, 1, vals);
    glGetUniformdv(p, loc, d);
    CHECK_NEAR(d[0], 1.5, 1e-10);
    CHECK_NEAR(d[1], 2.5, 1e-10);

    glUniform3dv(loc, 1, vals);
    glGetUniformdv(p, loc, d);
    CHECK_NEAR(d[0], 1.5, 1e-10);
    CHECK_NEAR(d[1], 2.5, 1e-10);
    CHECK_NEAR(d[2], 3.5, 1e-10);

    glUniform4dv(loc, 1, vals);
    glGetUniformdv(p, loc, d);
    CHECK_NEAR(d[0], 1.5, 1e-10);
    CHECK_NEAR(d[1], 2.5, 1e-10);
    CHECK_NEAR(d[2], 3.5, 1e-10);
    CHECK_NEAR(d[3], 4.5, 1e-10);

    glUseProgram(0);
    glDeleteProgram(p);
}

/* ---------- error cases ---------- */

GPU_TEST(uniform_scalar, no_program_errors)
{
    GLfloat f = 1.0f;
    GLint i = 1;
    GLuint ui = 1u;
    GLdouble d = 1.0;

    glUseProgram(0);

    // every entry point must set GL_INVALID_OPERATION with no current program
    glUniform1f(0, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform1d(0, 1.0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform1ui(0, 1u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform1iv(0, 1, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform1uiv(0, 1, &ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform1dv(0, 1, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2f(0, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2fv(0, 1, &f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2i(0, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2iv(0, 1, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2ui(0, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2uiv(0, 1, &ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2d(0, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform2dv(0, 1, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3f(0, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3fv(0, 1, &f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3i(0, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3iv(0, 1, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3ui(0, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3uiv(0, 1, &ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3d(0, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform3dv(0, 1, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform4i(0, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform4iv(0, 1, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform4ui(0, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform4uiv(0, 1, &ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform4d(0, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniform4dv(0, 1, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(uniform_scalar, negative_count_errors)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLint loc;
    GLfloat f[4] = { 1, 2, 3, 4 };
    GLint i[4] = { 1, 2, 3, 4 };
    GLuint ui[4] = { 1, 2, 3, 4 };
    GLdouble d[4] = { 1, 2, 3, 4 };

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);
    loc = glGetUniformLocation(p, "u_f4");

    glUniform1fv(loc, -1, f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform2fv(loc, -1, f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform3fv(loc, -1, f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform4iv(loc, -1, i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform1uiv(loc, -1, ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform2uiv(loc, -1, ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform3uiv(loc, -1, ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform4uiv(loc, -1, ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform1dv(loc, -1, d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform2dv(loc, -1, d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform3dv(loc, -1, d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniform4dv(loc, -1, d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUseProgram(0);
    glDeleteProgram(p);
}

GPU_TEST(uniform_scalar, location_minus_one_is_silently_ignored)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLfloat f = 1.0f;
    GLint i = 1;
    GLuint ui = 1u;
    GLdouble d = 1.0;

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);

    // location -1 must be silently ignored, no error
    glUniform1f(-1, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform1d(-1, 1.0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform1ui(-1, 1u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform1iv(-1, 1, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform1uiv(-1, 1, &ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform1dv(-1, 1, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform2f(-1, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform2fv(-1, 1, &f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform2i(-1, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform2iv(-1, 1, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform2ui(-1, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform2uiv(-1, 1, &ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform2d(-1, 1, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform2dv(-1, 1, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform3f(-1, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform3fv(-1, 1, &f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform3i(-1, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform3iv(-1, 1, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform3ui(-1, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform3uiv(-1, 1, &ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform3d(-1, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform3dv(-1, 1, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform4i(-1, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform4iv(-1, 1, &i);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform4ui(-1, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform4uiv(-1, 1, &ui);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform4d(-1, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform4dv(-1, 1, &d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}

/* ---------- glUniformBlockBinding ---------- */

GPU_TEST(uniform_scalar, uniform_block_binding_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLuint idx;

    CHECK(p != 0);
    if (!p) return;

    // no uniform blocks in our program, so any index should be invalid
    idx = glGetUniformBlockIndex(p, "nonexistent");
    CHECK_EQ_UINT(idx, GL_INVALID_INDEX);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // a bad program name
    glUniformBlockBinding(999123, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // an out-of-range block index (no blocks exist, index 0 is out of range)
    glUniformBlockBinding(p, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // a binding >= GL_MAX_UNIFORM_BUFFER_BINDINGS
    GLint max_bind = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_bind);
    glUniformBlockBinding(p, 0, (GLuint)max_bind);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

/* ---------- glUniformSubroutinesuiv ---------- */

GPU_TEST(uniform_scalar, subroutine_uniforms_not_supported)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_SIMPLE, FS_SIMPLE, err, sizeof err);
    GLint count = -1;

    CHECK(p != 0);
    if (!p) return;

    glUseProgram(p);

    // no active subroutines
    glGetProgramStageiv(p, GL_VERTEX_SHADER, GL_ACTIVE_SUBROUTINE_UNIFORMS, &count);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(count, 0);

    // count must match the active subroutine uniform count (0)
    const GLuint idx = 0;
    glUniformSubroutinesuiv(GL_VERTEX_SHADER, 0, &idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // non-zero count is an error when there are no subroutine uniforms
    glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // negative count
    glUniformSubroutinesuiv(GL_VERTEX_SHADER, -1, &idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // no current program
    glUseProgram(0);
    glUniformSubroutinesuiv(GL_VERTEX_SHADER, 0, &idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUseProgram(p);
    glDeleteProgram(p);
}
