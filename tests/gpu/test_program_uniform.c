/*
 * test_program_uniform.c
 * MGL
 *
 * glProgramUniform* DSA uniform entry points: all 47 forms.
 */

#include "mgl_test.h"
#include "harness.h"

/* A vertex shader with one uniform per type family the test exercises. */
static const char *VS_UNI =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 0) uniform vec4  u_f4;\n"
    "layout(location = 1) uniform ivec4 u_i4;\n"
    "layout(location = 2) uniform uvec4 u_ui4;\n"
    "layout(location = 3) uniform mat4  u_m4;\n"
    "layout(location = 4) uniform mat3  u_m3;\n"
    "layout(location = 5) uniform mat2  u_m2;\n"
    "layout(location = 6) uniform mat2x3 u_m23;\n"
    "layout(location = 7) uniform mat3x2 u_m32;\n"
    "layout(location = 8) uniform mat2x4 u_m24;\n"
    "layout(location = 9) uniform mat4x2 u_m42;\n"
    "layout(location =10) uniform mat3x4 u_m34;\n"
    "layout(location =11) uniform mat4x3 u_m43;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char *FS_BLANK =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(0.0); }\n";

/* ---------- float scalar / vector forms (1f, 2f, 3f, 4f, 1fv, 2fv, 3fv, 4fv) ---------- */

GPU_TEST(program_uniform, float_vector_roundtrip)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;
    GLfloat got[4] = {0};

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_f4");
    CHECK(loc >= 0);
    if (loc < 0) { glDeleteProgram(p); return; }

    /* 1f */
    glProgramUniform1f(p, loc, 1.25f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    CHECK_NEAR(got[0], 1.25f, 1e-5f);
    CHECK_NEAR(got[1], 0.0f, 1e-5f);
    CHECK_NEAR(got[2], 0.0f, 1e-5f);
    CHECK_NEAR(got[3], 0.0f, 1e-5f);

    /* 2f */
    glProgramUniform2f(p, loc, 2.0f, 3.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    CHECK_NEAR(got[0], 2.0f, 1e-5f);
    CHECK_NEAR(got[1], 3.0f, 1e-5f);

    /* 3f */
    glProgramUniform3f(p, loc, 4.0f, 5.0f, 6.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    CHECK_NEAR(got[0], 4.0f, 1e-5f);
    CHECK_NEAR(got[1], 5.0f, 1e-5f);
    CHECK_NEAR(got[2], 6.0f, 1e-5f);

    /* 4f */
    glProgramUniform4f(p, loc, 7.0f, 8.0f, 9.0f, 10.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    CHECK_NEAR(got[0], 7.0f, 1e-5f);
    CHECK_NEAR(got[1], 8.0f, 1e-5f);
    CHECK_NEAR(got[2], 9.0f, 1e-5f);
    CHECK_NEAR(got[3], 10.0f, 1e-5f);

    /* 1fv */
    glProgramUniform1fv(p, loc, 1, (const GLfloat[]){ 11.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    CHECK_NEAR(got[0], 11.0f, 1e-5f);

    /* 2fv */
    glProgramUniform2fv(p, loc, 1, (const GLfloat[]){ 12.0f, 13.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    CHECK_NEAR(got[0], 12.0f, 1e-5f);
    CHECK_NEAR(got[1], 13.0f, 1e-5f);

    /* 3fv */
    glProgramUniform3fv(p, loc, 1, (const GLfloat[]){ 14.0f, 15.0f, 16.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    CHECK_NEAR(got[0], 14.0f, 1e-5f);
    CHECK_NEAR(got[1], 15.0f, 1e-5f);
    CHECK_NEAR(got[2], 16.0f, 1e-5f);

    /* 4fv */
    glProgramUniform4fv(p, loc, 1, (const GLfloat[]){ 17.0f, 18.0f, 19.0f, 20.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    CHECK_NEAR(got[0], 17.0f, 1e-5f);
    CHECK_NEAR(got[1], 18.0f, 1e-5f);
    CHECK_NEAR(got[2], 19.0f, 1e-5f);
    CHECK_NEAR(got[3], 20.0f, 1e-5f);

    glDeleteProgram(p);
}

/* ---------- integer scalar / vector forms (1i, 2i, 3i, 4i, 1iv, 2iv, 3iv, 4iv) ---------- */

GPU_TEST(program_uniform, int_vector_roundtrip)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;
    GLint got[4] = {0};

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_i4");
    CHECK(loc >= 0);
    if (loc < 0) { glDeleteProgram(p); return; }

    /* 1i */
    glProgramUniform1i(p, loc, 10);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc, got);
    CHECK_EQ_INT(got[0], 10);
    CHECK_EQ_INT(got[1], 0);
    CHECK_EQ_INT(got[2], 0);
    CHECK_EQ_INT(got[3], 0);

    /* 2i */
    glProgramUniform2i(p, loc, 20, 30);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc, got);
    CHECK_EQ_INT(got[0], 20);
    CHECK_EQ_INT(got[1], 30);

    /* 3i */
    glProgramUniform3i(p, loc, 40, 50, 60);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc, got);
    CHECK_EQ_INT(got[0], 40);
    CHECK_EQ_INT(got[1], 50);
    CHECK_EQ_INT(got[2], 60);

    /* 4i */
    glProgramUniform4i(p, loc, 70, 80, 90, 100);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc, got);
    CHECK_EQ_INT(got[0], 70);
    CHECK_EQ_INT(got[1], 80);
    CHECK_EQ_INT(got[2], 90);
    CHECK_EQ_INT(got[3], 100);

    /* 1iv */
    glProgramUniform1iv(p, loc, 1, (const GLint[]){ 110 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc, got);
    CHECK_EQ_INT(got[0], 110);

    /* 2iv */
    glProgramUniform2iv(p, loc, 1, (const GLint[]){ 120, 130 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc, got);
    CHECK_EQ_INT(got[0], 120);
    CHECK_EQ_INT(got[1], 130);

    /* 3iv */
    glProgramUniform3iv(p, loc, 1, (const GLint[]){ 140, 150, 160 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc, got);
    CHECK_EQ_INT(got[0], 140);
    CHECK_EQ_INT(got[1], 150);
    CHECK_EQ_INT(got[2], 160);

    /* 4iv */
    glProgramUniform4iv(p, loc, 1, (const GLint[]){ 170, 180, 190, 200 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc, got);
    CHECK_EQ_INT(got[0], 170);
    CHECK_EQ_INT(got[1], 180);
    CHECK_EQ_INT(got[2], 190);
    CHECK_EQ_INT(got[3], 200);

    glDeleteProgram(p);
}

/* ---------- unsigned integer forms (1ui, 2ui, 3ui, 4ui, 1uiv, 2uiv, 3uiv, 4uiv) ---------- */

GPU_TEST(program_uniform, uint_vector_roundtrip)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;
    GLuint got[4] = {0};

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_ui4");
    CHECK(loc >= 0);
    if (loc < 0) { glDeleteProgram(p); return; }

    /* 1ui */
    glProgramUniform1ui(p, loc, 100u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc, got);
    CHECK_EQ_UINT(got[0], 100u);
    CHECK_EQ_UINT(got[1], 0u);

    /* 2ui */
    glProgramUniform2ui(p, loc, 200u, 300u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc, got);
    CHECK_EQ_UINT(got[0], 200u);
    CHECK_EQ_UINT(got[1], 300u);

    /* 3ui */
    glProgramUniform3ui(p, loc, 400u, 500u, 600u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc, got);
    CHECK_EQ_UINT(got[0], 400u);
    CHECK_EQ_UINT(got[1], 500u);
    CHECK_EQ_UINT(got[2], 600u);

    /* 4ui */
    glProgramUniform4ui(p, loc, 700u, 800u, 900u, 1000u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc, got);
    CHECK_EQ_UINT(got[0], 700u);
    CHECK_EQ_UINT(got[1], 800u);
    CHECK_EQ_UINT(got[2], 900u);
    CHECK_EQ_UINT(got[3], 1000u);

    /* 1uiv */
    glProgramUniform1uiv(p, loc, 1, (const GLuint[]){ 1100u });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc, got);
    CHECK_EQ_UINT(got[0], 1100u);

    /* 2uiv */
    glProgramUniform2uiv(p, loc, 1, (const GLuint[]){ 1200u, 1300u });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc, got);
    CHECK_EQ_UINT(got[0], 1200u);
    CHECK_EQ_UINT(got[1], 1300u);

    /* 3uiv */
    glProgramUniform3uiv(p, loc, 1, (const GLuint[]){ 1400u, 1500u, 1600u });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc, got);
    CHECK_EQ_UINT(got[0], 1400u);
    CHECK_EQ_UINT(got[1], 1500u);
    CHECK_EQ_UINT(got[2], 1600u);

    /* 4uiv */
    glProgramUniform4uiv(p, loc, 1, (const GLuint[]){ 1700u, 1800u, 1900u, 2000u });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc, got);
    CHECK_EQ_UINT(got[0], 1700u);
    CHECK_EQ_UINT(got[1], 1800u);
    CHECK_EQ_UINT(got[2], 1900u);
    CHECK_EQ_UINT(got[3], 2000u);

    glDeleteProgram(p);
}

/* ---------- double forms (1d, 2d, 3d, 4d, 1dv, 2dv, 3dv, 4dv) ---------- */

GPU_TEST(program_uniform, double_vector_roundtrip)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;
    GLdouble got[4] = {0};

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_f4");
    CHECK(loc >= 0);
    if (loc < 0) { glDeleteProgram(p); return; }

    /* 1d */
    glProgramUniform1d(p, loc, 1.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc, got);
    CHECK_NEAR(got[0], 1.5, 1e-5);
    CHECK_NEAR(got[1], 0.0, 1e-5);

    /* 2d */
    glProgramUniform2d(p, loc, 2.5, 3.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc, got);
    CHECK_NEAR(got[0], 2.5, 1e-5);
    CHECK_NEAR(got[1], 3.5, 1e-5);

    /* 3d */
    glProgramUniform3d(p, loc, 4.5, 5.5, 6.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc, got);
    CHECK_NEAR(got[0], 4.5, 1e-5);
    CHECK_NEAR(got[1], 5.5, 1e-5);
    CHECK_NEAR(got[2], 6.5, 1e-5);

    /* 4d */
    glProgramUniform4d(p, loc, 7.5, 8.5, 9.5, 10.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc, got);
    CHECK_NEAR(got[0], 7.5, 1e-5);
    CHECK_NEAR(got[1], 8.5, 1e-5);
    CHECK_NEAR(got[2], 9.5, 1e-5);
    CHECK_NEAR(got[3], 10.5, 1e-5);

    /* 1dv */
    glProgramUniform1dv(p, loc, 1, (const GLdouble[]){ 11.5 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc, got);
    CHECK_NEAR(got[0], 11.5, 1e-5);

    /* 2dv */
    glProgramUniform2dv(p, loc, 1, (const GLdouble[]){ 12.5, 13.5 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc, got);
    CHECK_NEAR(got[0], 12.5, 1e-5);
    CHECK_NEAR(got[1], 13.5, 1e-5);

    /* 3dv */
    glProgramUniform3dv(p, loc, 1, (const GLdouble[]){ 14.5, 15.5, 16.5 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc, got);
    CHECK_NEAR(got[0], 14.5, 1e-5);
    CHECK_NEAR(got[1], 15.5, 1e-5);
    CHECK_NEAR(got[2], 16.5, 1e-5);

    /* 4dv */
    glProgramUniform4dv(p, loc, 1, (const GLdouble[]){ 17.5, 18.5, 19.5, 20.5 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc, got);
    CHECK_NEAR(got[0], 17.5, 1e-5);
    CHECK_NEAR(got[1], 18.5, 1e-5);
    CHECK_NEAR(got[2], 19.5, 1e-5);
    CHECK_NEAR(got[3], 20.5, 1e-5);

    glDeleteProgram(p);
}

/* ---------- matrix float forms (all 9) ---------- */

GPU_TEST(program_uniform, matrix_float_roundtrip)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;
    GLfloat got[16] = {0};
    const GLfloat vals[16] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f
    };

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    /* mat2 (2x2 = 4 floats) */
    loc = glGetUniformLocation(p, "u_m2");
    if (loc >= 0)
    {
        glProgramUniformMatrix2fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[1], 2.0f, 1e-5f);
        CHECK_NEAR(got[2], 3.0f, 1e-5f);
        CHECK_NEAR(got[3], 4.0f, 1e-5f);
    }

    /* mat3 (3x3 = 9 floats) */
    loc = glGetUniformLocation(p, "u_m3");
    if (loc >= 0)
    {
        glProgramUniformMatrix3fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[8], 9.0f, 1e-5f);
    }

    /* mat4 (4x4 = 16 floats) */
    loc = glGetUniformLocation(p, "u_m4");
    if (loc >= 0)
    {
        glProgramUniformMatrix4fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[3], 4.0f, 1e-5f);
        CHECK_NEAR(got[15], 16.0f, 1e-5f);
    }

    /* mat2x3 (2 cols, 3 rows = 6 floats) */
    loc = glGetUniformLocation(p, "u_m23");
    if (loc >= 0)
    {
        glProgramUniformMatrix2x3fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[5], 6.0f, 1e-5f);
    }

    /* mat3x2 (3 cols, 2 rows = 6 floats) */
    loc = glGetUniformLocation(p, "u_m32");
    if (loc >= 0)
    {
        glProgramUniformMatrix3x2fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[5], 6.0f, 1e-5f);
    }

    /* mat2x4 (2 cols, 4 rows = 8 floats) */
    loc = glGetUniformLocation(p, "u_m24");
    if (loc >= 0)
    {
        glProgramUniformMatrix2x4fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[7], 8.0f, 1e-5f);
    }

    /* mat4x2 (4 cols, 2 rows = 8 floats) */
    loc = glGetUniformLocation(p, "u_m42");
    if (loc >= 0)
    {
        glProgramUniformMatrix4x2fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[7], 8.0f, 1e-5f);
    }

    /* mat3x4 (3 cols, 4 rows = 12 floats) */
    loc = glGetUniformLocation(p, "u_m34");
    if (loc >= 0)
    {
        glProgramUniformMatrix3x4fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[11], 12.0f, 1e-5f);
    }

    /* mat4x3 (4 cols, 3 rows = 12 floats) */
    loc = glGetUniformLocation(p, "u_m43");
    if (loc >= 0)
    {
        glProgramUniformMatrix4x3fv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, loc, got);
        CHECK_NEAR(got[0], 1.0f, 1e-5f);
        CHECK_NEAR(got[11], 12.0f, 1e-5f);
    }

    glDeleteProgram(p);
}

/* ---------- matrix double forms (all 9) ---------- */

GPU_TEST(program_uniform, matrix_double_roundtrip)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;
    GLdouble got[16] = {0};
    const GLdouble vals[16] = {
        1.0, 2.0, 3.0, 4.0,
        5.0, 6.0, 7.0, 8.0,
        9.0, 10.0, 11.0, 12.0,
        13.0, 14.0, 15.0, 16.0
    };

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    /* mat2 */
    loc = glGetUniformLocation(p, "u_m2");
    if (loc >= 0)
    {
        glProgramUniformMatrix2dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[3], 4.0, 1e-5);
    }

    /* mat3 */
    loc = glGetUniformLocation(p, "u_m3");
    if (loc >= 0)
    {
        glProgramUniformMatrix3dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[8], 9.0, 1e-5);
    }

    /* mat4 */
    loc = glGetUniformLocation(p, "u_m4");
    if (loc >= 0)
    {
        glProgramUniformMatrix4dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[15], 16.0, 1e-5);
    }

    /* mat2x3 */
    loc = glGetUniformLocation(p, "u_m23");
    if (loc >= 0)
    {
        glProgramUniformMatrix2x3dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[5], 6.0, 1e-5);
    }

    /* mat3x2 */
    loc = glGetUniformLocation(p, "u_m32");
    if (loc >= 0)
    {
        glProgramUniformMatrix3x2dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[5], 6.0, 1e-5);
    }

    /* mat2x4 */
    loc = glGetUniformLocation(p, "u_m24");
    if (loc >= 0)
    {
        glProgramUniformMatrix2x4dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[7], 8.0, 1e-5);
    }

    /* mat4x2 */
    loc = glGetUniformLocation(p, "u_m42");
    if (loc >= 0)
    {
        glProgramUniformMatrix4x2dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[7], 8.0, 1e-5);
    }

    /* mat3x4 */
    loc = glGetUniformLocation(p, "u_m34");
    if (loc >= 0)
    {
        glProgramUniformMatrix3x4dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[11], 12.0, 1e-5);
    }

    /* mat4x3 */
    loc = glGetUniformLocation(p, "u_m43");
    if (loc >= 0)
    {
        glProgramUniformMatrix4x3dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[11], 12.0, 1e-5);
    }

    glDeleteProgram(p);
}

/* ---------- matrix transpose ---------- */

GPU_TEST(program_uniform, matrix_transpose)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;
    GLfloat got[16] = {0};

    /* Row-major input for a 2x3 matrix:
       [ 1  2  3 ]
       [ 4  5  6 ]
       stored as [1,2,3,4,5,6] */
    const GLfloat row_major[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_m23");
    if (loc < 0) { glDeleteProgram(p); return; }

    /* transpose=GL_TRUE means input is row-major. MGL transposes it to
       column-major for storage, then reads back column-major. */
    glProgramUniformMatrix2x3fv(p, loc, 1, GL_TRUE, row_major);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc, got);
    /* After transposition: col0=[1,4], col1=[2,5], col2=[3,6] */
    CHECK_NEAR(got[0], 1.0f, 1e-5f);
    CHECK_NEAR(got[1], 4.0f, 1e-5f);
    CHECK_NEAR(got[2], 2.0f, 1e-5f);
    CHECK_NEAR(got[3], 5.0f, 1e-5f);
    CHECK_NEAR(got[4], 3.0f, 1e-5f);
    CHECK_NEAR(got[5], 6.0f, 1e-5f);

    /* Same with 4x4 and GL_TRUE. */
    {
        const GLfloat rm4[16] = {
            1.0f,  2.0f,  3.0f,  4.0f,
            5.0f,  6.0f,  7.0f,  8.0f,
            9.0f,  10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f, 16.0f
        };
        loc = glGetUniformLocation(p, "u_m4");
        if (loc >= 0)
        {
            glProgramUniformMatrix4fv(p, loc, 1, GL_TRUE, rm4);
            CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
            glGetUniformfv(p, loc, got);
            /* Transpose of rm4: column-major = [1,5,9,13, 2,6,10,14, ...] */
            CHECK_NEAR(got[0], 1.0f, 1e-5f);
            CHECK_NEAR(got[1], 5.0f, 1e-5f);
            CHECK_NEAR(got[4], 2.0f, 1e-5f);
            CHECK_NEAR(got[15], 16.0f, 1e-5f);
        }
    }

    glDeleteProgram(p);
}

/* ---------- error conditions ---------- */

GPU_TEST(program_uniform, rejects_bad_program)
{
    /* GL 4.6 section 2.3.1: a name that is neither a program nor a shader
       is GL_INVALID_VALUE, which is also what the CTS checks */
    glProgramUniform1f(999999, 0, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1i(999999, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1ui(999999, 0, 1u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1d(999999, 0, 1.0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4fv(999999, 0, 1, GL_FALSE, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4dv(999999, 0, 1, GL_FALSE, (const GLdouble[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(program_uniform, rejects_negative_count)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_f4");
    if (loc < 0) { glDeleteProgram(p); return; }

    /* The spec says count < 0 generates GL_INVALID_VALUE. */
    glProgramUniform1fv(p, loc, -1, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform2fv(p, loc, -1, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1iv(p, loc, -1, (const GLint[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1uiv(p, loc, -1, (const GLuint[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1dv(p, loc, -1, (const GLdouble[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4fv(p, loc, -1, GL_FALSE, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4dv(p, loc, -1, GL_FALSE, (const GLdouble[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

GPU_TEST(program_uniform, rejects_null_value)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);
    GLint loc;

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_f4");
    if (loc < 0) { glDeleteProgram(p); return; }

    /* The v-forms check value != NULL and generate GL_INVALID_VALUE. */
    glProgramUniform1fv(p, loc, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1iv(p, loc, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1uiv(p, loc, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1dv(p, loc, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4fv(p, loc, 1, GL_FALSE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4dv(p, loc, 1, GL_FALSE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

GPU_TEST(program_uniform, rejects_unlinked_program)
{
    GLuint p = glCreateProgram();

    CHECK(p != 0);

    /* An unlinked program generates GL_INVALID_OPERATION because
       programForUniform checks pptr->linked_glsl_program. */
    glProgramUniform1f(p, 0, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glProgramUniformMatrix4fv(p, 0, 1, GL_FALSE, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteProgram(p);
}

GPU_TEST(program_uniform, location_minus_one_is_silent)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_BLANK, err, sizeof err);

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    /* The spec says location == -1 is silently ignored. */
    glProgramUniform1f(p, -1, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glProgramUniform4f(p, -1, 1.0f, 2.0f, 3.0f, 4.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glProgramUniform1i(p, -1, 42);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glProgramUniform1ui(p, -1, 42u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glProgramUniform1d(p, -1, 1.0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glProgramUniform1fv(p, -1, 1, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glProgramUniformMatrix4fv(p, -1, 1, GL_FALSE, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glProgramUniformMatrix4dv(p, -1, 1, GL_FALSE, (const GLdouble[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
}
