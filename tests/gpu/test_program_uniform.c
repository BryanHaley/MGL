/*
 * test_program_uniform.c
 * MGL
 *
 * glProgramUniform* DSA uniform entry points: all 47 forms.
 */

#include "mgl_test.h"
#include "harness.h"

/* A uniform of every size the setters fit: float, int, uint and double
   scalars and vectors, and float and double matrices. MGL gives each uniform
   its own Metal buffer and a stage has 31, so the doubles live in the
   fragment shader. All are read, or they would have no location. */
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
    "layout(location =12) uniform float u_f1;\n"
    "layout(location =13) uniform vec2  u_f2;\n"
    "layout(location =14) uniform vec3  u_f3;\n"
    "layout(location =15) uniform int   u_i1;\n"
    "layout(location =16) uniform ivec2 u_i2;\n"
    "layout(location =17) uniform ivec3 u_i3;\n"
    "layout(location =18) uniform uint  u_ui1;\n"
    "layout(location =19) uniform uvec2 u_ui2;\n"
    "layout(location =20) uniform uvec3 u_ui3;\n"
    "void main() {\n"
    "    gl_Position = vec4(pos, 0.0, 1.0)\n"
    "      + u_f4 + vec4(u_i4) + vec4(u_ui4)\n"
    "      + u_m4[0] + vec4(u_m3[0], 0) + vec4(u_m2[0], 0, 0)\n"
    "      + vec4(u_m23[0], 0) + vec4(u_m32[0], 0, 0) + u_m24[0]\n"
    "      + vec4(u_m42[0], 0, 0) + u_m34[0] + vec4(u_m43[0], 0)\n"
    "      + vec4(u_f1) + vec4(u_f2, 0, 0) + vec4(u_f3, 0)\n"
    "      + vec4(u_i1) + vec4(u_i2, 0, 0) + vec4(u_i3, 0)\n"
    "      + vec4(u_ui1) + vec4(u_ui2, 0, 0) + vec4(u_ui3, 0);\n"
    "}\n";

static const char *FS_DOUBLES =
    "#version 460 core\n"
    "layout(location =21) uniform double u_d1;\n"
    "layout(location =22) uniform dvec2  u_d2;\n"
    "layout(location =23) uniform dvec3  u_d3;\n"
    "layout(location =24) uniform dvec4  u_d4;\n"
    "layout(location =25) uniform dmat4  u_dm4;\n"
    "layout(location =26) uniform dmat3  u_dm3;\n"
    "layout(location =27) uniform dmat2  u_dm2;\n"
    "layout(location =28) uniform dmat2x3 u_dm23;\n"
    "layout(location =29) uniform dmat3x2 u_dm32;\n"
    "layout(location =30) uniform dmat2x4 u_dm24;\n"
    "layout(location =31) uniform dmat4x2 u_dm42;\n"
    "layout(location =32) uniform dmat3x4 u_dm34;\n"
    "layout(location =33) uniform dmat4x3 u_dm43;\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() {\n"
    "    frag = vec4(float(u_d1)) + vec4(vec2(u_d2), 0, 0) + vec4(vec3(u_d3), 0) + vec4(u_d4)\n"
    "      + vec4(u_dm4[0]) + vec4(vec3(u_dm3[0]), 0) + vec4(vec2(u_dm2[0]), 0, 0)\n"
    "      + vec4(vec3(u_dm23[0]), 0) + vec4(vec2(u_dm32[0]), 0, 0) + vec4(u_dm24[0])\n"
    "      + vec4(vec2(u_dm42[0]), 0, 0) + vec4(u_dm34[0]) + vec4(vec3(u_dm43[0]), 0);\n"
    "}\n";

/* ---------- float scalar / vector forms (1f, 2f, 3f, 4f, 1fv, 2fv, 3fv, 4fv) ---------- */

GPU_TEST(program_uniform, float_vector_roundtrip)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);
    GLint loc[4];
    GLfloat got[4] = {0};

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    // the 1-, 2-, 3- and 4-wide uniforms, so every setter has one it fits
    loc[0] = glGetUniformLocation(p, "u_f1");
    loc[1] = glGetUniformLocation(p, "u_f2");
    loc[2] = glGetUniformLocation(p, "u_f3");
    loc[3] = glGetUniformLocation(p, "u_f4");
    CHECK(loc[0] >= 0 && loc[1] >= 0 && loc[2] >= 0 && loc[3] >= 0);
    if (loc[0] < 0 || loc[1] < 0 || loc[2] < 0 || loc[3] < 0) { glDeleteProgram(p); return; }

    /* 1f */
    glProgramUniform1f(p, loc[0], 1.25f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc[0], got);
    CHECK_NEAR(got[0], 1.25f, 1e-5f);

    /* 2f */
    glProgramUniform2f(p, loc[1], 2.0f, 3.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc[1], got);
    CHECK_NEAR(got[0], 2.0f, 1e-5f);
    CHECK_NEAR(got[1], 3.0f, 1e-5f);

    /* 3f */
    glProgramUniform3f(p, loc[2], 4.0f, 5.0f, 6.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc[2], got);
    CHECK_NEAR(got[0], 4.0f, 1e-5f);
    CHECK_NEAR(got[1], 5.0f, 1e-5f);
    CHECK_NEAR(got[2], 6.0f, 1e-5f);

    /* 4f */
    glProgramUniform4f(p, loc[3], 7.0f, 8.0f, 9.0f, 10.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc[3], got);
    CHECK_NEAR(got[0], 7.0f, 1e-5f);
    CHECK_NEAR(got[1], 8.0f, 1e-5f);
    CHECK_NEAR(got[2], 9.0f, 1e-5f);
    CHECK_NEAR(got[3], 10.0f, 1e-5f);

    /* 1fv */
    glProgramUniform1fv(p, loc[0], 1, (const GLfloat[]){ 11.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc[0], got);
    CHECK_NEAR(got[0], 11.0f, 1e-5f);

    /* 2fv */
    glProgramUniform2fv(p, loc[1], 1, (const GLfloat[]){ 12.0f, 13.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc[1], got);
    CHECK_NEAR(got[0], 12.0f, 1e-5f);
    CHECK_NEAR(got[1], 13.0f, 1e-5f);

    /* 3fv */
    glProgramUniform3fv(p, loc[2], 1, (const GLfloat[]){ 14.0f, 15.0f, 16.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc[2], got);
    CHECK_NEAR(got[0], 14.0f, 1e-5f);
    CHECK_NEAR(got[1], 15.0f, 1e-5f);
    CHECK_NEAR(got[2], 16.0f, 1e-5f);

    /* 4fv */
    glProgramUniform4fv(p, loc[3], 1, (const GLfloat[]){ 17.0f, 18.0f, 19.0f, 20.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, loc[3], got);
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
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);
    GLint loc[4];
    GLint got[4] = {0};

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc[0] = glGetUniformLocation(p, "u_i1");
    loc[1] = glGetUniformLocation(p, "u_i2");
    loc[2] = glGetUniformLocation(p, "u_i3");
    loc[3] = glGetUniformLocation(p, "u_i4");
    CHECK(loc[0] >= 0 && loc[1] >= 0 && loc[2] >= 0 && loc[3] >= 0);
    if (loc[0] < 0 || loc[1] < 0 || loc[2] < 0 || loc[3] < 0) { glDeleteProgram(p); return; }

    /* 1i */
    glProgramUniform1i(p, loc[0], 10);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc[0], got);
    CHECK_EQ_INT(got[0], 10);

    /* 2i */
    glProgramUniform2i(p, loc[1], 20, 30);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc[1], got);
    CHECK_EQ_INT(got[0], 20);
    CHECK_EQ_INT(got[1], 30);

    /* 3i */
    glProgramUniform3i(p, loc[2], 40, 50, 60);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc[2], got);
    CHECK_EQ_INT(got[0], 40);
    CHECK_EQ_INT(got[1], 50);
    CHECK_EQ_INT(got[2], 60);

    /* 4i */
    glProgramUniform4i(p, loc[3], 70, 80, 90, 100);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc[3], got);
    CHECK_EQ_INT(got[0], 70);
    CHECK_EQ_INT(got[1], 80);
    CHECK_EQ_INT(got[2], 90);
    CHECK_EQ_INT(got[3], 100);

    /* 1iv */
    glProgramUniform1iv(p, loc[0], 1, (const GLint[]){ 110 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc[0], got);
    CHECK_EQ_INT(got[0], 110);

    /* 2iv */
    glProgramUniform2iv(p, loc[1], 1, (const GLint[]){ 120, 130 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc[1], got);
    CHECK_EQ_INT(got[0], 120);
    CHECK_EQ_INT(got[1], 130);

    /* 3iv */
    glProgramUniform3iv(p, loc[2], 1, (const GLint[]){ 140, 150, 160 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc[2], got);
    CHECK_EQ_INT(got[0], 140);
    CHECK_EQ_INT(got[1], 150);
    CHECK_EQ_INT(got[2], 160);

    /* 4iv */
    glProgramUniform4iv(p, loc[3], 1, (const GLint[]){ 170, 180, 190, 200 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformiv(p, loc[3], got);
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
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);
    GLint loc[4];
    GLuint got[4] = {0};

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc[0] = glGetUniformLocation(p, "u_ui1");
    loc[1] = glGetUniformLocation(p, "u_ui2");
    loc[2] = glGetUniformLocation(p, "u_ui3");
    loc[3] = glGetUniformLocation(p, "u_ui4");
    CHECK(loc[0] >= 0 && loc[1] >= 0 && loc[2] >= 0 && loc[3] >= 0);
    if (loc[0] < 0 || loc[1] < 0 || loc[2] < 0 || loc[3] < 0) { glDeleteProgram(p); return; }

    /* 1ui */
    glProgramUniform1ui(p, loc[0], 100u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc[0], got);
    CHECK_EQ_UINT(got[0], 100u);

    /* 2ui */
    glProgramUniform2ui(p, loc[1], 200u, 300u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc[1], got);
    CHECK_EQ_UINT(got[0], 200u);
    CHECK_EQ_UINT(got[1], 300u);

    /* 3ui */
    glProgramUniform3ui(p, loc[2], 400u, 500u, 600u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc[2], got);
    CHECK_EQ_UINT(got[0], 400u);
    CHECK_EQ_UINT(got[1], 500u);
    CHECK_EQ_UINT(got[2], 600u);

    /* 4ui */
    glProgramUniform4ui(p, loc[3], 700u, 800u, 900u, 1000u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc[3], got);
    CHECK_EQ_UINT(got[0], 700u);
    CHECK_EQ_UINT(got[1], 800u);
    CHECK_EQ_UINT(got[2], 900u);
    CHECK_EQ_UINT(got[3], 1000u);

    /* 1uiv */
    glProgramUniform1uiv(p, loc[0], 1, (const GLuint[]){ 1100u });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc[0], got);
    CHECK_EQ_UINT(got[0], 1100u);

    /* 2uiv */
    glProgramUniform2uiv(p, loc[1], 1, (const GLuint[]){ 1200u, 1300u });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc[1], got);
    CHECK_EQ_UINT(got[0], 1200u);
    CHECK_EQ_UINT(got[1], 1300u);

    /* 3uiv */
    glProgramUniform3uiv(p, loc[2], 1, (const GLuint[]){ 1400u, 1500u, 1600u });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc[2], got);
    CHECK_EQ_UINT(got[0], 1400u);
    CHECK_EQ_UINT(got[1], 1500u);
    CHECK_EQ_UINT(got[2], 1600u);

    /* 4uiv */
    glProgramUniform4uiv(p, loc[3], 1, (const GLuint[]){ 1700u, 1800u, 1900u, 2000u });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformuiv(p, loc[3], got);
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
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);
    GLint loc[4];
    GLdouble got[4] = {0};

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    loc[0] = glGetUniformLocation(p, "u_d1");
    loc[1] = glGetUniformLocation(p, "u_d2");
    loc[2] = glGetUniformLocation(p, "u_d3");
    loc[3] = glGetUniformLocation(p, "u_d4");
    CHECK(loc[0] >= 0 && loc[1] >= 0 && loc[2] >= 0 && loc[3] >= 0);
    if (loc[0] < 0 || loc[1] < 0 || loc[2] < 0 || loc[3] < 0) { glDeleteProgram(p); return; }

    /* 1d */
    glProgramUniform1d(p, loc[0], 1.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc[0], got);
    CHECK_NEAR(got[0], 1.5, 1e-5);

    /* 2d */
    glProgramUniform2d(p, loc[1], 2.5, 3.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc[1], got);
    CHECK_NEAR(got[0], 2.5, 1e-5);
    CHECK_NEAR(got[1], 3.5, 1e-5);

    /* 3d */
    glProgramUniform3d(p, loc[2], 4.5, 5.5, 6.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc[2], got);
    CHECK_NEAR(got[0], 4.5, 1e-5);
    CHECK_NEAR(got[1], 5.5, 1e-5);
    CHECK_NEAR(got[2], 6.5, 1e-5);

    /* 4d */
    glProgramUniform4d(p, loc[3], 7.5, 8.5, 9.5, 10.5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc[3], got);
    CHECK_NEAR(got[0], 7.5, 1e-5);
    CHECK_NEAR(got[1], 8.5, 1e-5);
    CHECK_NEAR(got[2], 9.5, 1e-5);
    CHECK_NEAR(got[3], 10.5, 1e-5);

    /* 1dv */
    glProgramUniform1dv(p, loc[0], 1, (const GLdouble[]){ 11.5 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc[0], got);
    CHECK_NEAR(got[0], 11.5, 1e-5);

    /* 2dv */
    glProgramUniform2dv(p, loc[1], 1, (const GLdouble[]){ 12.5, 13.5 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc[1], got);
    CHECK_NEAR(got[0], 12.5, 1e-5);
    CHECK_NEAR(got[1], 13.5, 1e-5);

    /* 3dv */
    glProgramUniform3dv(p, loc[2], 1, (const GLdouble[]){ 14.5, 15.5, 16.5 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc[2], got);
    CHECK_NEAR(got[0], 14.5, 1e-5);
    CHECK_NEAR(got[1], 15.5, 1e-5);
    CHECK_NEAR(got[2], 16.5, 1e-5);

    /* 4dv */
    glProgramUniform4dv(p, loc[3], 1, (const GLdouble[]){ 17.5, 18.5, 19.5, 20.5 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, loc[3], got);
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
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);
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
    CHECK(loc >= 0);
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
    CHECK(loc >= 0);
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
    CHECK(loc >= 0);
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
    CHECK(loc >= 0);
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
    CHECK(loc >= 0);
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
    CHECK(loc >= 0);
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
    CHECK(loc >= 0);
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
    CHECK(loc >= 0);
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
    CHECK(loc >= 0);
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
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);
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

    /* dmat2 */
    loc = glGetUniformLocation(p, "u_dm2");
    CHECK(loc >= 0);
    if (loc >= 0)
    {
        glProgramUniformMatrix2dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[3], 4.0, 1e-5);
    }

    /* dmat3 */
    loc = glGetUniformLocation(p, "u_dm3");
    CHECK(loc >= 0);
    if (loc >= 0)
    {
        glProgramUniformMatrix3dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[8], 9.0, 1e-5);
    }

    /* dmat4 */
    loc = glGetUniformLocation(p, "u_dm4");
    CHECK(loc >= 0);
    if (loc >= 0)
    {
        glProgramUniformMatrix4dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[15], 16.0, 1e-5);
    }

    /* dmat2x3 */
    loc = glGetUniformLocation(p, "u_dm23");
    CHECK(loc >= 0);
    if (loc >= 0)
    {
        glProgramUniformMatrix2x3dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[5], 6.0, 1e-5);
    }

    /* dmat3x2 */
    loc = glGetUniformLocation(p, "u_dm32");
    CHECK(loc >= 0);
    if (loc >= 0)
    {
        glProgramUniformMatrix3x2dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[5], 6.0, 1e-5);
    }

    /* dmat2x4 */
    loc = glGetUniformLocation(p, "u_dm24");
    CHECK(loc >= 0);
    if (loc >= 0)
    {
        glProgramUniformMatrix2x4dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[7], 8.0, 1e-5);
    }

    /* dmat4x2 */
    loc = glGetUniformLocation(p, "u_dm42");
    CHECK(loc >= 0);
    if (loc >= 0)
    {
        glProgramUniformMatrix4x2dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[7], 8.0, 1e-5);
    }

    /* dmat3x4 */
    loc = glGetUniformLocation(p, "u_dm34");
    CHECK(loc >= 0);
    if (loc >= 0)
    {
        glProgramUniformMatrix3x4dv(p, loc, 1, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, loc, got);
        CHECK_NEAR(got[0], 1.0, 1e-5);
        CHECK_NEAR(got[11], 12.0, 1e-5);
    }

    /* dmat4x3 */
    loc = glGetUniformLocation(p, "u_dm43");
    CHECK(loc >= 0);
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
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);
    GLint loc;
    GLfloat got[16] = {0};

    /* mat2x3 is 2 columns of 3 rows, so row-major input is
       [ 1  2 ]
       [ 3  4 ]
       [ 5  6 ]
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
    /* After transposition: col0=[1,3,5], col1=[2,4,6] */
    CHECK_NEAR(got[0], 1.0f, 1e-5f);
    CHECK_NEAR(got[1], 3.0f, 1e-5f);
    CHECK_NEAR(got[2], 5.0f, 1e-5f);
    CHECK_NEAR(got[3], 2.0f, 1e-5f);
    CHECK_NEAR(got[4], 4.0f, 1e-5f);
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
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    /* The spec says count < 0 generates GL_INVALID_VALUE. Each setter goes
       to a uniform it fits, so the count is the only thing wrong. */
    glProgramUniform1fv(p, glGetUniformLocation(p, "u_f1"), -1, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform2fv(p, glGetUniformLocation(p, "u_f2"), -1, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1iv(p, glGetUniformLocation(p, "u_i1"), -1, (const GLint[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1uiv(p, glGetUniformLocation(p, "u_ui1"), -1, (const GLuint[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1dv(p, glGetUniformLocation(p, "u_d1"), -1, (const GLdouble[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4fv(p, glGetUniformLocation(p, "u_m4"), -1, GL_FALSE, (const GLfloat[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4dv(p, glGetUniformLocation(p, "u_dm4"), -1, GL_FALSE, (const GLdouble[]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

GPU_TEST(program_uniform, rejects_null_value)
{
    char err[1024] = {0};
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);

    CHECK_MSG(p != 0, "link: %s", err);
    if (!p) return;

    /* The v-forms check value != NULL and generate GL_INVALID_VALUE. */
    glProgramUniform1fv(p, glGetUniformLocation(p, "u_f1"), 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1iv(p, glGetUniformLocation(p, "u_i1"), 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1uiv(p, glGetUniformLocation(p, "u_ui1"), 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniform1dv(p, glGetUniformLocation(p, "u_d1"), 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4fv(p, glGetUniformLocation(p, "u_m4"), 1, GL_FALSE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramUniformMatrix4dv(p, glGetUniformLocation(p, "u_dm4"), 1, GL_FALSE, NULL);
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
    GLuint p = mgl_build_program(VS_UNI, FS_DOUBLES, err, sizeof err);

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

// A sampler's starting unit is its layout(binding), however the number is
// written: hex, octal, or through a #define. The #define form read as 0.
GPU_TEST(program_uniform, sampler_binding_as_hex_octal_and_macro)
{
    static const char *VS =
        "#version 460 core\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 460 core\n"
        "#define UNIT 5\n"
        "layout(binding = 0x3) uniform sampler2D hex;\n"
        "layout(binding = 07) uniform sampler2D oct;\n"
        "layout(binding = UNIT) uniform sampler2D mac;\n"
        "out vec4 o;\n"
        "void main() { o = texture(hex, vec2(0)) + texture(oct, vec2(0)) + texture(mac, vec2(0)); }\n";
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    GLint unit = -1;

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGetUniformiv(prog, glGetUniformLocation(prog, "hex"), &unit);
    CHECK_EQ_INT(unit, 3);
    glGetUniformiv(prog, glGetUniformLocation(prog, "oct"), &unit);
    CHECK_EQ_INT(unit, 7);
    glGetUniformiv(prog, glGetUniformLocation(prog, "mac"), &unit);
    CHECK_EQ_INT(unit, 5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(prog);
}

static int compilesFS(const char *fmt, int n)
{
    char src[512];
    const char *p = src;
    GLuint s = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = 0;

    snprintf(src, sizeof src, fmt, n);
    glShaderSource(s, 1, &p, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    glDeleteShader(s);

    return ok;
}

// A block or image binding past the last binding point does not compile,
// counting every element of an array. Only sampler and counter bindings
// were checked.
GPU_TEST(program_uniform, bindings_past_the_limit_do_not_compile)
{
    static const char *UBO =
        "#version 440\nout vec4 o;\n"
        "layout(binding = %d, std140) uniform B { float x; } b;\n"
        "void main() { o = vec4(b.x); }\n";
    static const char *SSBO =
        "#version 440\nout vec4 o;\n"
        "layout(binding = %d, std430) buffer S { float x; } s[2];\n"
        "void main() { o = vec4(s[0].x + s[1].x); }\n";
    static const char *IMG =
        "#version 440\nout vec4 o;\n"
        "layout(binding = %d, rgba8) readonly uniform image2D img;\n"
        "void main() { o = imageLoad(img, ivec2(0)); }\n";
    GLint ubos = 0, ssbos = 0, images = 0;

    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &ubos);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &ssbos);
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &images);

    CHECK(compilesFS(UBO, ubos - 1));
    CHECK(!compilesFS(UBO, ubos));
    CHECK(compilesFS(SSBO, ssbos - 2));
    CHECK(!compilesFS(SSBO, ssbos - 1));
    CHECK(compilesFS(IMG, images - 1));
    CHECK(!compilesFS(IMG, images));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// A uniform the shader never reads is not active and has no location, even
// with an explicit one; both location queries agree. glGetUniformLocation
// used to hand back the explicit location anyway.
GPU_TEST(program_uniform, an_unused_uniform_has_no_location)
{
    static const char *VS =
        "#version 430 core\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 430 core\n"
        "layout(location = 2) uniform vec3 unused;\n"
        "layout(location = 5) uniform float used;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(used); }\n";
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    CHECK_EQ_INT(glGetUniformLocation(prog, "used"), 5);
    CHECK_EQ_INT(glGetProgramResourceLocation(prog, GL_UNIFORM, "used"), 5);
    CHECK_EQ_INT(glGetUniformLocation(prog, "unused"), -1);
    CHECK_EQ_INT(glGetProgramResourceLocation(prog, GL_UNIFORM, "unused"), -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(prog);
}

// Every location taken explicitly leaves none for an implicit uniform, and the
// link fails. It linked, with the implicit one numbered off the end.
GPU_TEST(program_uniform, running_out_of_locations_fails_the_link)
{
    static const char *VS =
        "#version 430 core\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 430 core\n"
        "layout(location = 0) uniform float all_of_them[1024];\n"
        "uniform float one_more;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(all_of_them[1023] + one_more); }\n";
    char log[2048] = { 0 };
    GLint max = 0;
    GLuint prog;

    glGetIntegerv(GL_MAX_UNIFORM_LOCATIONS, &max);
    CHECK_EQ_INT(max, 1024);

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog == 0, "linked although no location was left");
    if (prog)
        glDeleteProgram(prog);
    mgl_drain_errors();
}

// The members of a struct with layout(location) take the locations after it,
// one per element. They were numbered as if the struct had none.
GPU_TEST(program_uniform, struct_members_follow_the_structs_location)
{
    static const char *VS =
        "#version 430 core\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 430 core\n"
        "struct S { vec4 m0; float m1[2]; mat2 m2; };\n"
        "layout(location = 1) uniform S u0[3];\n"
        "out vec4 o;\n"
        "void main() {\n"
        "    vec4 sum = vec4(0.0);\n"
        "    for (int i = 0; i < 3; i++) sum += u0[i].m0 + vec4(u0[i].m1[0] + u0[i].m1[1]) + vec4(u0[i].m2[0], u0[i].m2[1]);\n"
        "    o = sum;\n"
        "}\n";
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[0].m0"), 1);
    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[0].m1[0]"), 2);
    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[0].m1[1]"), 3);
    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[0].m2"), 4);
    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[1].m0"), 5);
    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[2].m2"), 12);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(prog);
}

// A shader's info log comes back terminated, cut to the buffer, and empty
// when there is none. Nothing was written in those cases, so callers read
// whatever their buffer already held.
GPU_TEST(program_uniform, shader_info_log_is_always_a_string)
{
    static const char *bad = "#version 430 core\nvoid main() { this is not glsl }\n";
    static const char *good = "#version 430 core\nvoid main() { }\n";
    GLuint s = glCreateShader(GL_VERTEX_SHADER);
    char buf[8];
    GLsizei len = -1;

    glShaderSource(s, 1, &good, NULL);
    glCompileShader(s);
    memset(buf, 'x', sizeof buf);
    glGetShaderInfoLog(s, sizeof buf, &len, buf);
    CHECK_EQ_INT(buf[0], 0);
    CHECK_EQ_INT(len, 0);

    glShaderSource(s, 1, &bad, NULL);
    glCompileShader(s);
    memset(buf, 'x', sizeof buf);
    glGetShaderInfoLog(s, sizeof buf, &len, buf);
    CHECK_EQ_INT(len, (GLsizei)sizeof buf - 1);
    CHECK_EQ_INT(buf[sizeof buf - 1], 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteShader(s);
}

// A fragment stage may read as many samplers as GL_MAX_TEXTURE_IMAGE_UNITS
// says, and a program that does links. The limit said 128, past the 16
// samplers Metal binds to one stage, so such a program failed in Metal.
GPU_TEST(program_uniform, fragment_samplers_up_to_the_limit_link)
{
    static const char *VS =
        "#version 430 core\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    char fs[512];
    char log[2048] = { 0 };
    GLint max = 0;
    GLuint prog;

    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max);
    CHECK_EQ_INT(max, 16);

    snprintf(fs, sizeof fs,
             "#version 430 core\n"
             "uniform sampler2D s[%d];\n"
             "out vec4 o;\n"
             "void main() { vec4 c = vec4(0); for (int i = 0; i < %d; i++) c += texture(s[i], vec2(0.5)); o = c; }\n",
             max, max);

    prog = mgl_build_program(VS, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (prog)
        glDeleteProgram(prog);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// Two uniform block arrays may start at the same binding and then read the
// same buffers. SPIRV-Cross refused to translate that.
GPU_TEST(program_uniform, block_arrays_may_share_a_binding)
{
    static const char *CS =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(binding = 0, std140) uniform A { float a; } ua[2];\n"
        "layout(binding = 0, std140) uniform B { float b; } ub[2];\n"
        "layout(std430, binding = 0) buffer Out { vec4 got; };\n"
        "void main() { got = vec4(ua[0].a, ua[1].a, ub[0].b, ub[1].b); }\n";
    static const GLfloat one[4] = { 1, 0, 0, 0 }, two[4] = { 2, 0, 0, 0 };
    GLfloat got[4] = { 0 };
    GLuint prog, ubo[2], out;
    char log[2048] = { 0 };

    prog = mgl_build_compute_program(CS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGenBuffers(2, ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo[0]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof one, one, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo[1]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof two, two, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo[0]);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, ubo[1]);

    glGenBuffers(1, &out);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof got, got, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, out);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(got[0] == 1 && got[1] == 2 && got[2] == 1 && got[3] == 2,
              "read %g %g %g %g, want 1 2 1 2", got[0], got[1], got[2], got[3]);

    glUseProgram(0);
    glDeleteBuffers(2, ubo);
    glDeleteBuffers(1, &out);
    glDeleteProgram(prog);
}

// Every element of an array of arrays has a location, the last subscript
// changing fastest. Only the innermost size was counted, and a name with
// two subscripts was not found at all.
GPU_TEST(program_uniform, array_of_arrays_elements_have_locations)
{
    static const char *VS =
        "#version 430 core\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 430 core\n"
        "layout(location = 2) uniform vec4 u0[2][3];\n"
        "out vec4 o;\n"
        "void main() { vec4 s = vec4(0); for (int i = 0; i < 2; i++) for (int j = 0; j < 3; j++) s += u0[i][j]; o = s; }\n";
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    GLfloat got[4] = { 0 };

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[0][0]"), 2);
    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[0][2]"), 4);
    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[1][0]"), 5);
    CHECK_EQ_INT(glGetUniformLocation(prog, "u0[1][2]"), 7);

    glUseProgram(prog);
    glUniform4f(7, 1, 2, 3, 4);
    glGetUniformfv(prog, 7, got);
    CHECK_MSG(got[0] == 1 && got[3] == 4, "u0[1][2] read %g .. %g", got[0], got[3]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(prog);
}

// A glProgramUniform call that fails on its program name must not leave its
// setter type behind for the driver's own uniform writes at the next draw.
GPU_TEST(program_uniform, a_failed_call_does_not_break_the_next_draw)
{
    static const char *VS =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); gl_PointSize = 1.0; }\n";
    static const char *FS =
        "#version 410\n"
        "out vec4 o;\n"
        "void main() { o = vec4(float(gl_NumSamples)); }\n";
    char log[1024];
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log), vao;
    MGLTestTarget t;

    CHECK_MSG(prog != 0, "did not build: %s", log);
    if (!prog || !mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) return;

    glProgramUniformMatrix4dv(999999, 0, 1, GL_FALSE, (const GLdouble[16]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glDrawArrays(GL_POINTS, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// Bools are stored as one-byte Metal bools; reading them back gives 1 or 0
// in whatever type is asked for, however they were set
GPU_TEST(program_uniform, bool_vectors_read_back)
{
    static const char *vs =
        "#version 460 core\n"
        "uniform bool b1;\nuniform bvec2 b2;\nuniform bvec3 b3;\n"
        "void main() { gl_Position = vec4(b1 && b2.y && b3.z ? 1.0 : 0.0); }\n";
    static const char *fs = "#version 460 core\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    char err[1024] = "";
    GLuint p = mgl_build_program(vs, fs, err, sizeof err);
    GLfloat f[3] = { -1, -1, -1 };
    GLint i[3] = { -1, -1, -1 };

    CHECK_MSG(p != 0, "did not build: %s", err);
    if (!p)
        return;

    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p, "b1"), 7);
    glUniform2f(glGetUniformLocation(p, "b2"), 0.0f, 1.0f);
    glUniform3i(glGetUniformLocation(p, "b3"), 0, 5, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetUniformfv(p, glGetUniformLocation(p, "b1"), f);
    CHECK_MSG(f[0] == 1.0f, "b1 read %g", f[0]);
    glGetUniformfv(p, glGetUniformLocation(p, "b2"), f);
    CHECK_MSG(f[0] == 0.0f && f[1] == 1.0f, "b2 read %g %g", f[0], f[1]);
    glGetUniformiv(p, glGetUniformLocation(p, "b3"), i);
    CHECK_MSG(i[0] == 0 && i[1] == 1 && i[2] == 1, "b3 read %d %d %d", i[0], i[1], i[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}

// A transposed non-square matrix: rows in, columns stored
GPU_TEST(program_uniform, non_square_transpose_is_row_major_in)
{
    static const char *vs =
        "#version 460 core\nuniform mat2x3 m;\nuniform mat3x2 k;\n"
        "void main() { gl_Position = vec4(m[1][2] + k[2][1]); }\n";
    static const char *fs = "#version 460 core\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    static const GLfloat rows[6] = { 1, 2, 3, 4, 5, 6 };
    char err[1024] = "";
    GLuint p = mgl_build_program(vs, fs, err, sizeof err);
    GLfloat a[6] = { 0 };

    CHECK_MSG(p != 0, "did not build: %s", err);
    if (!p)
        return;

    glUseProgram(p);
    /* mat2x3: 3 rows of 2 in, columns 1 3 5 and 2 4 6 out */
    glUniformMatrix2x3fv(glGetUniformLocation(p, "m"), 1, GL_TRUE, rows);
    glGetUniformfv(p, glGetUniformLocation(p, "m"), a);
    CHECK_MSG(a[0] == 1 && a[1] == 3 && a[2] == 5 && a[3] == 2 && a[4] == 4 && a[5] == 6,
              "mat2x3 %g %g %g %g %g %g", a[0], a[1], a[2], a[3], a[4], a[5]);
    /* mat3x2: 2 rows of 3 in, columns 1 4, 2 5, 3 6 out */
    glProgramUniformMatrix3x2fv(p, glGetUniformLocation(p, "k"), 1, GL_TRUE, rows);
    glGetUniformfv(p, glGetUniformLocation(p, "k"), a);
    CHECK_MSG(a[0] == 1 && a[1] == 4 && a[2] == 2 && a[3] == 5 && a[4] == 3 && a[5] == 6,
              "mat3x2 %g %g %g %g %g %g", a[0], a[1], a[2], a[3], a[4], a[5]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}

// glProgramUniform's double forms store doubles, as glUniform's do, and a
// transposed matrix comes out the same either way
GPU_TEST(program_uniform, program_double_forms_keep_doubles)
{
    static const char *vs =
        "#version 460 core\n"
        "uniform dvec3 v;\nuniform dmat2x3 m;\nuniform dmat2x3 n;\n"
        "void main() { gl_Position = vec4(float(v.x + m[1][2] + n[1][2])); }\n";
    static const char *fs = "#version 460 core\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    static const GLdouble rows[6] = { 1, 2, 3, 4, 5, 6 };   /* 3 rows of 2, to transpose */
    char err[1024] = "";
    GLuint p = mgl_build_program(vs, fs, err, sizeof err);
    GLdouble a[6] = { 0 }, b[6] = { 0 };

    CHECK_MSG(p != 0, "did not build: %s", err);
    if (!p)
        return;

    glProgramUniform3d(p, glGetUniformLocation(p, "v"), 1.0 / 3.0, 2.0, 1e-10);
    glGetUniformdv(p, glGetUniformLocation(p, "v"), a);
    CHECK_MSG(a[0] == 1.0 / 3.0 && a[1] == 2.0 && a[2] == 1e-10, "v read %.17g %g %g", a[0], a[1], a[2]);

    glProgramUniformMatrix2x3dv(p, glGetUniformLocation(p, "m"), 1, GL_TRUE, rows);
    glUseProgram(p);
    glUniformMatrix2x3dv(glGetUniformLocation(p, "n"), 1, GL_TRUE, rows);
    glUseProgram(0);
    glGetUniformdv(p, glGetUniformLocation(p, "m"), a);
    glGetUniformdv(p, glGetUniformLocation(p, "n"), b);
    CHECK_MSG(!memcmp(a, b, sizeof a), "m %g %g %g %g %g %g, n %g %g %g %g %g %g",
              a[0], a[1], a[2], a[3], a[4], a[5], b[0], b[1], b[2], b[3], b[4], b[5]);
    /* 2 columns of 3 rows: column major 1 3 5, 2 4 6 */
    CHECK_MSG(a[0] == 1 && a[1] == 3 && a[2] == 5 && a[3] == 2 && a[4] == 4 && a[5] == 6,
              "m %g %g %g %g %g %g, want 1 3 5 2 4 6", a[0], a[1], a[2], a[3], a[4], a[5]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
}

// Only the struct members a shader reads are active uniforms, and the two
// ways of counting active uniforms agree
GPU_TEST(program_uniform, unused_struct_members_are_inactive)
{
    static const char *vs =
        "#version 460 core\n"
        "struct S { vec4 used; vec4 unused; float arr[2]; };\n"
        "uniform S s;\nuniform S t[2];\nuniform vec4 plain;\n"
        "void main() { gl_Position = plain + s.used + vec4(t[1].arr[1]); }\n";
    static const char *fs = "#version 460 core\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    char err[1024] = "";
    GLuint p = mgl_build_program(vs, fs, err, sizeof err);
    GLint active = -1, resources = -2;

    CHECK_MSG(p != 0, "did not build: %s", err);
    if (!p)
        return;

    glGetProgramiv(p, GL_ACTIVE_UNIFORMS, &active);
    glGetProgramInterfaceiv(p, GL_UNIFORM, GL_ACTIVE_RESOURCES, &resources);
    CHECK_EQ_INT(active, resources);
    CHECK_EQ_INT(active, 3);   /* plain, s.used, t[1].arr */
    CHECK(glGetUniformLocation(p, "s.used") >= 0);
    CHECK_EQ_INT(glGetUniformLocation(p, "s.unused"), -1);
    CHECK(glGetUniformLocation(p, "t[1].arr[1]") >= 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
}

// Plain struct uniforms reach a compute shader, member by member
GPU_TEST(program_uniform, struct_uniforms_reach_compute)
{
    static const char *cs =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430, binding = 1) buffer R { vec4 r[3]; };\n"
        "struct S { vec4 m0; float m1[2]; mat2 m2; };\n"
        "layout(location = 1) uniform S u0[2];\n"
        "void main() {\n"
        "    r[0] = u0[0].m0;\n"
        "    r[1] = vec4(u0[0].m1[0], u0[0].m1[1], u0[1].m1[1], u0[1].m2[1][0]);\n"
        "    r[2] = u0[1].m0;\n"
        "}\n";
    char log[1024] = "";
    GLuint p = mgl_build_compute_program(cs, log, sizeof log), buf;
    GLfloat r[12] = { 0 };
    static const GLfloat m0a[4] = { 1, 2, 3, 4 }, m0b[4] = { 11, 12, 13, 14 }, m2[4] = { 17, 18, 19, 20 };

    CHECK_MSG(p != 0, "did not build: %s", log);
    if (!p)
        return;

    glUseProgram(p);
    glUniform4fv(1, 1, m0a);
    glUniform1f(2, 5.0f);
    glUniform1f(3, 6.0f);
    glUniform4fv(5, 1, m0b);
    glUniform1f(7, 16.0f);
    glUniformMatrix2fv(8, 1, GL_FALSE, m2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof r, r, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buf);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof r, r);

    CHECK_MSG(r[0] == 1 && r[1] == 2 && r[2] == 3 && r[3] == 4, "u0[0].m0 %g %g %g %g", r[0], r[1], r[2], r[3]);
    CHECK_MSG(r[4] == 5 && r[5] == 6 && r[6] == 16 && r[7] == 19, "members %g %g %g %g", r[4], r[5], r[6], r[7]);
    CHECK_MSG(r[8] == 11 && r[11] == 14, "u0[1].m0 %g %g %g %g", r[8], r[9], r[10], r[11]);

    glUseProgram(0);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(p);
}

// More plain uniforms than Metal has buffer slots, all read by one stage
GPU_TEST(program_uniform, more_plain_uniforms_than_buffer_slots)
{
    enum { N = 40 };
    static const char *vs =
        "#version 460 core\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    char fs[8192], log[2048] = "";
    size_t at = 0;
    MGLTestTarget t;
    GLuint p, vao;
    unsigned char c[4] = { 0 };

    at += (size_t)snprintf(fs + at, sizeof fs - at, "#version 460 core\nout vec4 o;\n");
    for (int i = 0; i < N; i++)
        at += (size_t)snprintf(fs + at, sizeof fs - at, "uniform float u%d;\n", i);
    at += (size_t)snprintf(fs + at, sizeof fs - at, "uniform dvec2 d;\nuniform bool b;\nuniform mat3 m;\n");
    at += (size_t)snprintf(fs + at, sizeof fs - at, "void main() {\n    float s = 0.0;\n");
    for (int i = 0; i < N; i++)
        at += (size_t)snprintf(fs + at, sizeof fs - at, "    s += u%d;\n", i);
    at += (size_t)snprintf(fs + at, sizeof fs - at,
                           "    bool ok = s == %d.0 && d == dvec2(1.0lf / 3.0lf, 2.0lf) && b && m[2][1] == 8.0;\n"
                           "    o = ok ? vec4(0, 1, 0, 1) : vec4(1, 0, 0, 1);\n}\n", N * (N + 1) / 2);

    p = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(p != 0, "did not build: %s", log);
    if (!p || !mgl_target_create(&t, 4, 4, GL_RGBA8, 0))
        return;

    glUseProgram(p);
    for (int i = 0; i < N; i++)
    {
        char name[16];

        snprintf(name, sizeof name, "u%d", i);
        glUniform1f(glGetUniformLocation(p, name), (GLfloat)(i + 1));
    }
    glUniform2d(glGetUniformLocation(p, "d"), 1.0 / 3.0, 2.0);
    glUniform1i(glGetUniformLocation(p, "b"), 1);
    {
        const GLfloat m[9] = { 0, 1, 2, 3, 4, 5, 6, 8, 7 };

        glUniformMatrix3fv(glGetUniformLocation(p, "m"), 1, GL_FALSE, m);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    unsigned char *px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 1, 1, c);
        free(px);
    }
    CHECK_MSG(c[1] == 255 && c[0] == 0, "drew %d %d %d, want green", c[0], c[1], c[2]);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(p);
    mgl_target_destroy(&t);
}

// A count above 1 is only for an array (GL 4.6 7.6.1); past an array's end
// the extra values are ignored
GPU_TEST(program_uniform, count_above_one_needs_an_array)
{
    static const char *vs =
        "#version 460 core\n"
        "uniform dvec2 d;\nuniform float f;\nuniform float a[2];\n"
        "void main() { gl_Position = vec4(float(d.x) + f + a[1]); }\n";
    static const char *fs = "#version 460 core\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    static const GLdouble dv[4] = { 1, 2, 3, 4 };
    static const GLfloat fv[3] = { 1, 2, 3 };
    char err[1024] = "";
    GLuint p = mgl_build_program(vs, fs, err, sizeof err);

    CHECK_MSG(p != 0, "did not build: %s", err);
    if (!p)
        return;

    glUseProgram(p);
    glUniform2dv(glGetUniformLocation(p, "d"), 2, dv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glUniform1fv(glGetUniformLocation(p, "f"), 2, fv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glUniform1fv(glGetUniformLocation(p, "a"), 3, fv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glUniform2dv(glGetUniformLocation(p, "d"), 1, dv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}

// Reading a uniform as another type converts its value: floats and doubles
// round to the nearest integer, a negative one reads as 0 unsigned, and a
// struct member is read from its struct
GPU_TEST(program_uniform, readback_converts_by_declared_type)
{
    static const char *vs =
        "#version 460 core\n"
        "uniform double d;\nuniform double n;\nuniform int i;\n"
        "uniform struct S { float a; double b; } s;\n"
        "void main() { gl_Position = vec4(float(d + n) + float(i) + s.a + float(s.b)); }\n";
    static const char *fs = "#version 460 core\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    char err[1024] = "";
    GLuint p = mgl_build_program(vs, fs, err, sizeof err);
    GLint iv = 0;
    GLuint uv = 9;
    GLfloat fv = 0;
    GLdouble dv = 0;

    CHECK_MSG(p != 0, "did not build: %s", err);
    if (!p)
        return;

    glUseProgram(p);
    glUniform1d(glGetUniformLocation(p, "d"), 11.75);
    glUniform1d(glGetUniformLocation(p, "n"), -4.25);
    glUniform1i(glGetUniformLocation(p, "i"), -7);
    glUniform1d(glGetUniformLocation(p, "s.b"), 6.5);

    glGetUniformiv(p, glGetUniformLocation(p, "d"), &iv);
    CHECK_EQ_INT(iv, 12);
    glGetUniformuiv(p, glGetUniformLocation(p, "n"), &uv);
    CHECK_EQ_UINT(uv, 0);
    glGetUniformfv(p, glGetUniformLocation(p, "i"), &fv);
    CHECK_MSG(fv == -7.0f, "int read as float gave %g", fv);
    glGetUniformdv(p, glGetUniformLocation(p, "s.b"), &dv);
    CHECK_MSG(dv == 6.5, "struct member read %g", dv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(p);
}
