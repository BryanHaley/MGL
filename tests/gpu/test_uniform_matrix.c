/*
 * test_uniform_matrix.c
 * MGL
 *
 * glUniformMatrix* — all fv and dv forms.
 */

#include "mgl_test.h"
#include "harness.h"

/* A vertex shader with every matrix type at known, non-overlapping locations.
   The fragment shader is a pass-through so the program links. */
static const char *VS_MATS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 0) uniform mat4 u_m4;\n"
    "layout(location = 4) uniform mat3 u_m3;\n"
    "layout(location = 7) uniform mat2 u_m2;\n"
    "layout(location = 9) uniform mat2x3 u_m23;\n"
    "layout(location = 11) uniform mat3x2 u_m32;\n"
    "layout(location = 13) uniform mat2x4 u_m24;\n"
    "layout(location = 15) uniform mat4x2 u_m42;\n"
    "layout(location = 17) uniform mat3x4 u_m34;\n"
    "layout(location = 20) uniform mat4x3 u_m43;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_PASSTHRU =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(0.0, 0.0, 0.0, 1.0); }\n";

/* Helper: build the matrix shader program and check it linked. */
static GLuint build_mat_program(void)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_MATS, FS_PASSTHRU, err, sizeof err);
    CHECK_MSG(p != 0, "link failed: %s", err);
    return p;
}

/* ---------- fv forms, square matrices ---------- */

GPU_TEST(uniform_matrix, fv_square)
{
    GLuint p = build_mat_program();
    if (!p) return;

    glUseProgram(p);

    /* mat2 at location 7 — 4 floats */
    {
        GLfloat in[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
        GLfloat out[4] = { 0 };
        glUniformMatrix2fv(7, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 7, out);
        CHECK_NEAR(out[0], 1.0f, 1e-6f);
        CHECK_NEAR(out[3], 4.0f, 1e-6f);
    }

    /* mat3 at location 4 — 9 floats */
    {
        GLfloat in[9] = { 1,2,3, 4,5,6, 7,8,9 };
        GLfloat out[9] = { 0 };
        glUniformMatrix3fv(4, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 4, out);
        CHECK_NEAR(out[0], 1.0f, 1e-6f);
        CHECK_NEAR(out[4], 5.0f, 1e-6f);
        CHECK_NEAR(out[8], 9.0f, 1e-6f);
    }

    /* mat4 at location 0 — 16 floats */
    {
        GLfloat in[16];
        GLfloat out[16] = { 0 };
        for (int i = 0; i < 16; i++) in[i] = (float)i;
        glUniformMatrix4fv(0, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 0, out);
        CHECK_NEAR(out[0],  0.0f, 1e-6f);
        CHECK_NEAR(out[5],  5.0f, 1e-6f);
        CHECK_NEAR(out[15], 15.0f, 1e-6f);
    }

    glDeleteProgram(p);
}

/* ---------- fv forms, non-square matrices ---------- */

GPU_TEST(uniform_matrix, fv_nonsquare)
{
    GLuint p = build_mat_program();
    if (!p) return;

    glUseProgram(p);

    /* mat2x3 at location 9 — 6 floats (2 columns, 3 rows) */
    {
        GLfloat in[6] = { 1,2,3, 4,5,6 };
        GLfloat out[6] = { 0 };
        glUniformMatrix2x3fv(9, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 9, out);
        CHECK_NEAR(out[0], 1.0f, 1e-6f);
        CHECK_NEAR(out[5], 6.0f, 1e-6f);
    }

    /* mat3x2 at location 11 — 6 floats (3 columns, 2 rows) */
    {
        GLfloat in[6] = { 10,20, 30,40, 50,60 };
        GLfloat out[6] = { 0 };
        glUniformMatrix3x2fv(11, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 11, out);
        CHECK_NEAR(out[0], 10.0f, 1e-6f);
        CHECK_NEAR(out[5], 60.0f, 1e-6f);
    }

    /* mat2x4 at location 13 — 8 floats (2 columns, 4 rows) */
    {
        GLfloat in[8] = { 1,2,3,4, 5,6,7,8 };
        GLfloat out[8] = { 0 };
        glUniformMatrix2x4fv(13, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 13, out);
        CHECK_NEAR(out[0], 1.0f, 1e-6f);
        CHECK_NEAR(out[7], 8.0f, 1e-6f);
    }

    /* mat4x2 at location 15 — 8 floats (4 columns, 2 rows) */
    {
        GLfloat in[8] = { 0,1, 2,3, 4,5, 6,7 };
        GLfloat out[8] = { 0 };
        glUniformMatrix4x2fv(15, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 15, out);
        CHECK_NEAR(out[0], 0.0f, 1e-6f);
        CHECK_NEAR(out[7], 7.0f, 1e-6f);
    }

    /* mat3x4 at location 17 — 12 floats (3 columns, 4 rows) */
    {
        GLfloat in[12];
        GLfloat out[12] = { 0 };
        for (int i = 0; i < 12; i++) in[i] = (float)i;
        glUniformMatrix3x4fv(17, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 17, out);
        CHECK_NEAR(out[0],  0.0f, 1e-6f);
        CHECK_NEAR(out[11], 11.0f, 1e-6f);
    }

    /* mat4x3 at location 20 — 12 floats (4 columns, 3 rows) */
    {
        GLfloat in[12];
        GLfloat out[12] = { 0 };
        for (int i = 0; i < 12; i++) in[i] = (float)(i * 2);
        glUniformMatrix4x3fv(20, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformfv(p, 20, out);
        CHECK_NEAR(out[0],  0.0f, 1e-6f);
        CHECK_NEAR(out[11], 22.0f, 1e-6f);
    }

    glDeleteProgram(p);
}

/* ---------- transpose ---------- */

GPU_TEST(uniform_matrix, fv_transpose)
{
    GLuint p = build_mat_program();
    if (!p) return;

    glUseProgram(p);

    /* A 2x2 matrix in column-major order, loaded with transpose=true so the
       implementation transposes it. Read back and verify the transposed layout. */
    GLfloat in[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    GLfloat out[4] = { 0 };

    /* in column-major: col0=[1,2], col1=[3,4]; after transpose: col0=[1,3], col1=[2,4] */
    glUniformMatrix2fv(7, 1, GL_TRUE, in);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, 7, out);
    CHECK_NEAR(out[0], 1.0f, 1e-6f);
    CHECK_NEAR(out[1], 3.0f, 1e-6f);
    CHECK_NEAR(out[2], 2.0f, 1e-6f);
    CHECK_NEAR(out[3], 4.0f, 1e-6f);

    /* Same for mat4 — identity with the (1,1) element moved to (0,0) after transpose */
    GLfloat m4[16];
    GLfloat m4out[16] = { 0 };
    for (int i = 0; i < 16; i++) m4[i] = (float)i;
    /* in column-major: element (col,row) = col*4+row.
       After transpose: element (col,row) becomes (row,col) = row*4+col.
       So index 4 (col=1,row=0) moves to index 1 (col=0,row=1). */
    glUniformMatrix4fv(0, 1, GL_TRUE, m4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformfv(p, 0, m4out);
    CHECK_NEAR(m4out[1], 4.0f, 1e-6f);
    CHECK_NEAR(m4out[4], 1.0f, 1e-6f);
    CHECK_NEAR(m4out[0], 0.0f, 1e-6f);

    glDeleteProgram(p);
}

/* ---------- dv forms, square matrices ---------- */

GPU_TEST(uniform_matrix, dv_square)
{
    GLuint p = build_mat_program();
    if (!p) return;

    glUseProgram(p);

    /* mat2dv — 4 doubles at location 7 */
    {
        GLdouble in[4] = { 1.5, 2.5, 3.5, 4.5 };
        GLdouble out[4] = { 0 };
        glUniformMatrix2dv(7, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 7, out);
        CHECK_NEAR(out[0], 1.5, 1e-12);
        CHECK_NEAR(out[3], 4.5, 1e-12);
    }

    /* mat3dv — 9 doubles at location 4 */
    {
        GLdouble in[9];
        GLdouble out[9] = { 0 };
        for (int i = 0; i < 9; i++) in[i] = (double)(i + 10);
        glUniformMatrix3dv(4, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 4, out);
        CHECK_NEAR(out[0], 10.0, 1e-12);
        CHECK_NEAR(out[8], 18.0, 1e-12);
    }

    /* mat4dv — 16 doubles at location 0 */
    {
        GLdouble in[16];
        GLdouble out[16] = { 0 };
        for (int i = 0; i < 16; i++) in[i] = (double)(i * 3);
        glUniformMatrix4dv(0, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 0, out);
        CHECK_NEAR(out[0],  0.0, 1e-12);
        CHECK_NEAR(out[5],  15.0, 1e-12);
        CHECK_NEAR(out[15], 45.0, 1e-12);
    }

    glDeleteProgram(p);
}

/* ---------- dv forms, non-square matrices ---------- */

GPU_TEST(uniform_matrix, dv_nonsquare)
{
    GLuint p = build_mat_program();
    if (!p) return;

    glUseProgram(p);

    /* mat2x3dv — 6 doubles */
    {
        GLdouble in[6] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };
        GLdouble out[6] = { 0 };
        glUniformMatrix2x3dv(9, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 9, out);
        CHECK_NEAR(out[0], 1.0, 1e-12);
        CHECK_NEAR(out[5], 6.0, 1e-12);
    }

    /* mat3x2dv — 6 doubles */
    {
        GLdouble in[6] = { 10.0, 20.0, 30.0, 40.0, 50.0, 60.0 };
        GLdouble out[6] = { 0 };
        glUniformMatrix3x2dv(11, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 11, out);
        CHECK_NEAR(out[0], 10.0, 1e-12);
        CHECK_NEAR(out[5], 60.0, 1e-12);
    }

    /* mat2x4dv — 8 doubles */
    {
        GLdouble in[8] = { 1,2,3,4,5,6,7,8 };
        GLdouble out[8] = { 0 };
        glUniformMatrix2x4dv(13, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 13, out);
        CHECK_NEAR(out[0], 1.0, 1e-12);
        CHECK_NEAR(out[7], 8.0, 1e-12);
    }

    /* mat4x2dv — 8 doubles */
    {
        GLdouble in[8] = { 0,1,2,3,4,5,6,7 };
        GLdouble out[8] = { 0 };
        glUniformMatrix4x2dv(15, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 15, out);
        CHECK_NEAR(out[0], 0.0, 1e-12);
        CHECK_NEAR(out[7], 7.0, 1e-12);
    }

    /* mat3x4dv — 12 doubles */
    {
        GLdouble in[12];
        GLdouble out[12] = { 0 };
        for (int i = 0; i < 12; i++) in[i] = (double)i;
        glUniformMatrix3x4dv(17, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 17, out);
        CHECK_NEAR(out[0],  0.0, 1e-12);
        CHECK_NEAR(out[11], 11.0, 1e-12);
    }

    /* mat4x3dv — 12 doubles */
    {
        GLdouble in[12];
        GLdouble out[12] = { 0 };
        for (int i = 0; i < 12; i++) in[i] = (double)(i * 2);
        glUniformMatrix4x3dv(20, 1, GL_FALSE, in);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetUniformdv(p, 20, out);
        CHECK_NEAR(out[0],  0.0, 1e-12);
        CHECK_NEAR(out[11], 22.0, 1e-12);
    }

    glDeleteProgram(p);
}

/* ---------- dv transpose ---------- */

GPU_TEST(uniform_matrix, dv_transpose)
{
    GLuint p = build_mat_program();
    if (!p) return;

    glUseProgram(p);

    GLdouble in[4] = { 1.0, 2.0, 3.0, 4.0 };
    GLdouble out[4] = { 0 };

    glUniformMatrix2dv(7, 1, GL_TRUE, in);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetUniformdv(p, 7, out);
    CHECK_NEAR(out[0], 1.0, 1e-12);
    CHECK_NEAR(out[1], 3.0, 1e-12);
    CHECK_NEAR(out[2], 2.0, 1e-12);
    CHECK_NEAR(out[3], 4.0, 1e-12);

    glDeleteProgram(p);
}

/* ---------- error conditions ---------- */

GPU_TEST(uniform_matrix, errors)
{
    GLuint p = build_mat_program();
    GLfloat val[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

    if (!p) return;

    /* count < 0 → GL_INVALID_VALUE */
    glUniformMatrix2fv(7, -1, GL_FALSE, val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* no active program → GL_INVALID_OPERATION */
    glUseProgram(0);
    glUniformMatrix2fv(7, 1, GL_FALSE, val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUseProgram(p);

    /* location = -1 is silently ignored */
    glUniformMatrix4fv(-1, 1, GL_FALSE, val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* bad location (too large) → GL_INVALID_OPERATION */
    glUniformMatrix4fv(9999, 1, GL_FALSE, val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteProgram(p);

    /* same errors on a dv form */
    GLdouble dval[4] = { 1.0, 0.0, 0.0, 1.0 };
    glUniformMatrix2dv(7, -1, GL_FALSE, dval);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUniformMatrix2dv(7, 1, GL_FALSE, dval);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUniformMatrix2dv(9999, 1, GL_FALSE, dval);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}
