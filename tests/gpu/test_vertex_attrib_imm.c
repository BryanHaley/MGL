/*
 * test_vertex_attrib_imm.c
 * MGL
 *
 * Immediate (current-value) vertex attribute setters — glVertexAttrib{1,2,3,4}
 * in their float, short, double, byte, unsigned and normalized forms.
 * Every function is called and read back via glGetVertexAttribfv with
 * GL_CURRENT_VERTEX_ATTRIB so that an empty-body implementation would fail.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- float forms: 1f, 1fv, 2fv, 3f, 3fv, 4fv ---------- */

GPU_TEST(vertex_attrib_imm, float_forms)
{
    GLfloat got[4];

    // glVertexAttrib1f – one component, defaults for the rest
    glVertexAttrib1f(1, 0.25f);
    glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 0.25f, 1e-6f);
    CHECK_NEAR(got[1], 0.0f,  1e-6f);
    CHECK_NEAR(got[2], 0.0f,  1e-6f);
    CHECK_NEAR(got[3], 1.0f,  1e-6f);

    // glVertexAttrib1fv – same from a pointer
    {
        const GLfloat v[] = { -0.5f };
        glVertexAttrib1fv(2, v);
        glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], -0.5f, 1e-6f);
        CHECK_NEAR(got[3],  1.0f, 1e-6f);
    }

    // glVertexAttrib2fv – two components
    {
        const GLfloat v[] = { 1.0f, 2.0f };
        glVertexAttrib2fv(3, v);
        glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 1.0f, 1e-6f);
        CHECK_NEAR(got[1], 2.0f, 1e-6f);
        CHECK_NEAR(got[2], 0.0f, 1e-6f);
        CHECK_NEAR(got[3], 1.0f, 1e-6f);
    }

    // glVertexAttrib3f – three components, w defaults to 1
    glVertexAttrib3f(4, 0.1f, 0.2f, 0.3f);
    glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 0.1f, 1e-6f);
    CHECK_NEAR(got[1], 0.2f, 1e-6f);
    CHECK_NEAR(got[2], 0.3f, 1e-6f);
    CHECK_NEAR(got[3], 1.0f, 1e-6f);

    // glVertexAttrib3fv – three from a pointer
    {
        const GLfloat v[] = { 10.0f, 20.0f, 30.0f };
        glVertexAttrib3fv(5, v);
        glGetVertexAttribfv(5, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 10.0f, 1e-6f);
        CHECK_NEAR(got[2], 30.0f, 1e-6f);
        CHECK_NEAR(got[3],  1.0f, 1e-6f);
    }

    // glVertexAttrib4fv – all four from a pointer
    {
        const GLfloat v[] = { 2.0f, 4.0f, 6.0f, 8.0f };
        glVertexAttrib4fv(6, v);
        glGetVertexAttribfv(6, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 2.0f, 1e-6f);
        CHECK_NEAR(got[1], 4.0f, 1e-6f);
        CHECK_NEAR(got[2], 6.0f, 1e-6f);
        CHECK_NEAR(got[3], 8.0f, 1e-6f);
    }
}

/* ---------- double forms: 1d, 1dv, 2d, 2dv, 3d, 3dv, 4d, 4dv ---------- */

GPU_TEST(vertex_attrib_imm, double_forms)
{
    GLfloat got[4];

    glVertexAttrib1d(1, 0.5);
    glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 0.5f, 1e-6f);
    CHECK_NEAR(got[3], 1.0f, 1e-6f);

    {
        const GLdouble v[] = { -1.5 };
        glVertexAttrib1dv(2, v);
        glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], -1.5f, 1e-6f);
    }

    glVertexAttrib2d(3, 1.0, 2.0);
    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 1.0f, 1e-6f);
    CHECK_NEAR(got[1], 2.0f, 1e-6f);
    CHECK_NEAR(got[3], 1.0f, 1e-6f);

    {
        const GLdouble v[] = { 3.0, 4.0 };
        glVertexAttrib2dv(4, v);
        glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 3.0f, 1e-6f);
        CHECK_NEAR(got[1], 4.0f, 1e-6f);
    }

    glVertexAttrib3d(5, 0.1, 0.2, 0.3);
    glGetVertexAttribfv(5, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 0.1f, 1e-6f);
    CHECK_NEAR(got[2], 0.3f, 1e-6f);
    CHECK_NEAR(got[3], 1.0f, 1e-6f);

    {
        const GLdouble v[] = { 10.0, 20.0, 30.0 };
        glVertexAttrib3dv(6, v);
        glGetVertexAttribfv(6, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 10.0f, 1e-6f);
        CHECK_NEAR(got[2], 30.0f, 1e-6f);
    }

    glVertexAttrib4d(7, 1.0, 2.0, 3.0, 4.0);
    glGetVertexAttribfv(7, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 1.0f, 1e-6f);
    CHECK_NEAR(got[3], 4.0f, 1e-6f);

    {
        const GLdouble v[] = { 5.0, 6.0, 7.0, 8.0 };
        glVertexAttrib4dv(8, v);
        glGetVertexAttribfv(8, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 5.0f, 1e-6f);
        CHECK_NEAR(got[3], 8.0f, 1e-6f);
    }
}

/* ---------- short forms: 1s, 1sv, 2s, 2sv, 3s, 3sv, 4s, 4sv ---------- */

GPU_TEST(vertex_attrib_imm, short_forms)
{
    GLfloat got[4];

    glVertexAttrib1s(1, 100);
    glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 100.0f, 1e-6f);
    CHECK_NEAR(got[3],   1.0f, 1e-6f);

    {
        const GLshort v[] = { -200 };
        glVertexAttrib1sv(2, v);
        glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], -200.0f, 1e-6f);
    }

    glVertexAttrib2s(3, 10, 20);
    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 10.0f, 1e-6f);
    CHECK_NEAR(got[1], 20.0f, 1e-6f);
    CHECK_NEAR(got[3],  1.0f, 1e-6f);

    {
        const GLshort v[] = { 30, 40 };
        glVertexAttrib2sv(4, v);
        glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 30.0f, 1e-6f);
        CHECK_NEAR(got[1], 40.0f, 1e-6f);
    }

    glVertexAttrib3s(5, 1, 2, 3);
    glGetVertexAttribfv(5, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 1.0f, 1e-6f);
    CHECK_NEAR(got[2], 3.0f, 1e-6f);
    CHECK_NEAR(got[3], 1.0f, 1e-6f);

    {
        const GLshort v[] = { 4, 5, 6 };
        glVertexAttrib3sv(6, v);
        glGetVertexAttribfv(6, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 4.0f, 1e-6f);
        CHECK_NEAR(got[2], 6.0f, 1e-6f);
    }

    glVertexAttrib4s(7, 1000, 2000, 3000, 4000);
    glGetVertexAttribfv(7, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0], 1000.0f, 1e-6f);
    CHECK_NEAR(got[3], 4000.0f, 1e-6f);

    {
        const GLshort v[] = { 5000, 6000, 7000, 8000 };
        glVertexAttrib4sv(8, v);
        glGetVertexAttribfv(8, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 5000.0f, 1e-6f);
        CHECK_NEAR(got[3], 8000.0f, 1e-6f);
    }
}

/* ---------- 4-component integral forms: 4bv, 4ubv, 4iv, 4uiv, 4usv ---------- */

GPU_TEST(vertex_attrib_imm, integral_4component)
{
    GLfloat got[4];

    {
        const GLbyte v[] = { -10, 20, -30, 40 };
        glVertexAttrib4bv(1, v);
        glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], -10.0f, 1e-6f);
        CHECK_NEAR(got[1],  20.0f, 1e-6f);
        CHECK_NEAR(got[2], -30.0f, 1e-6f);
        CHECK_NEAR(got[3],  40.0f, 1e-6f);
    }

    {
        const GLubyte v[] = { 0, 128, 200, 255 };
        glVertexAttrib4ubv(2, v);
        glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0],   0.0f, 1e-6f);
        CHECK_NEAR(got[1], 128.0f, 1e-6f);
        CHECK_NEAR(got[2], 200.0f, 1e-6f);
        CHECK_NEAR(got[3], 255.0f, 1e-6f);
    }

    {
        const GLint v[] = { -1000000, 2000000, -3000000, 4000000 };
        glVertexAttrib4iv(3, v);
        glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], -1000000.0f, 1e-3f);
        CHECK_NEAR(got[1],  2000000.0f, 1e-3f);
        CHECK_NEAR(got[2], -3000000.0f, 1e-3f);
        CHECK_NEAR(got[3],  4000000.0f, 1e-3f);
    }

    {
        const GLuint v[] = { 1000000, 2000000, 3000000, 4000000 };
        glVertexAttrib4uiv(4, v);
        glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 1000000.0f, 1e-3f);
        CHECK_NEAR(got[3], 4000000.0f, 1e-3f);
    }

    {
        const GLushort v[] = { 0, 32768, 49152, 65535 };
        glVertexAttrib4usv(5, v);
        glGetVertexAttribfv(5, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0],     0.0f, 1e-6f);
        CHECK_NEAR(got[1], 32768.0f, 1e-6f);
        CHECK_NEAR(got[2], 49152.0f, 1e-6f);
        CHECK_NEAR(got[3], 65535.0f, 1e-6f);
    }
}

/* ---------- normalized forms: 4Niv, 4Nsv, 4Nub, 4Nuiv, 4Nusv ---------- */

GPU_TEST(vertex_attrib_imm, normalized_forms)
{
    GLfloat got[4];

    // glVertexAttrib4Nsv – signed short mapped to [-1,1]
    {
        const GLshort v[] = { 0, 16384, -16384, 32767 };
        glVertexAttrib4Nsv(1, v);
        glGetVertexAttribfv(1, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0],  0.0f,                         1e-5f);
        CHECK_NEAR(got[1],  16384.0f / 32767.0f,          1e-5f);
        CHECK_NEAR(got[2], -16384.0f / 32767.0f,          1e-5f);
        CHECK_NEAR(got[3],  1.0f,                         1e-5f);
    }

    // glVertexAttrib4Niv – signed int mapped to [-1,1]
    {
        const GLint v[] = { 0, 1073741824, -1073741824, 2147483647 };
        glVertexAttrib4Niv(2, v);
        glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0],  0.0f,                             1e-5f);
        CHECK_NEAR(got[1],  1073741824.0f / 2147483647.0f,    1e-5f);
        CHECK_NEAR(got[2], -1073741824.0f / 2147483647.0f,    1e-5f);
        CHECK_NEAR(got[3],  1.0f,                             1e-5f);
    }

    // glVertexAttrib4Nub – 4 GLubyte scalars, stored as [0,1]
    glVertexAttrib4Nub(3, 0, 128, 255, 64);
    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(got[0],   0.0f / 255.0f, 1e-5f);
    CHECK_NEAR(got[1], 128.0f / 255.0f, 1e-5f);
    CHECK_NEAR(got[2], 255.0f / 255.0f, 1e-5f);
    CHECK_NEAR(got[3],  64.0f / 255.0f, 1e-5f);

    // glVertexAttrib4Nusv – unsigned short mapped to [0,1]
    {
        const GLushort v[] = { 0, 16384, 32768, 65535 };
        glVertexAttrib4Nusv(4, v);
        glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0],    0.0f / 65535.0f, 1e-5f);
        CHECK_NEAR(got[1], 16384.0f / 65535.0f, 1e-5f);
        CHECK_NEAR(got[2], 32768.0f / 65535.0f, 1e-5f);
        CHECK_NEAR(got[3],    1.0f,            1e-5f);
    }

    // glVertexAttrib4Nuiv – unsigned int mapped to [0,1]
    {
        const GLuint v[] = { 0, 1073741824, 2147483648u, 4294967295u };
        glVertexAttrib4Nuiv(5, v);
        glGetVertexAttribfv(5, GL_CURRENT_VERTEX_ATTRIB, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0],         0.0f / 4294967295.0f, 1e-5f);
        CHECK_NEAR(got[1], 1073741824.0f / 4294967295.0f, 1e-5f);
        CHECK_NEAR(got[2], 2147483648.0f / 4294967295.0f, 1e-5f);
        CHECK_NEAR(got[3],         1.0f,                 1e-5f);
    }
}

/* ---------- error: index out of range ---------- */

GPU_TEST(vertex_attrib_imm, error_bad_index)
{
    // A large index guaranteed to be >= GL_MAX_VERTEX_ATTRIBS
    const GLuint bad = 9999;

    glVertexAttrib1f(bad, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib1d(bad, 1.0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib1s(bad, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib2fv(bad, (const GLfloat[]){ 1, 2 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib3f(bad, 1, 2, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4fv(bad, (const GLfloat[]){ 1, 2, 3, 4 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4Nub(bad, 0, 128, 255, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // glGetVertexAttribfv with bad index
    {
        GLfloat tmp[4];
        glGetVertexAttribfv(bad, GL_CURRENT_VERTEX_ATTRIB, tmp);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    }
}

/* ---------- error: NULL pointer on *v forms ---------- */

GPU_TEST(vertex_attrib_imm, error_null_pointer)
{
    glVertexAttrib1fv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib2fv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib3fv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4fv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib1dv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib2dv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib3dv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4dv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib1sv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib2sv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib3sv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4sv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4bv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4ubv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4iv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4uiv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4usv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4Nsv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4Niv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4Nusv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttrib4Nuiv(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- error: index 0 has no current state ---------- */

GPU_TEST(vertex_attrib_imm, error_index0_no_current)
{
    GLfloat tmp[4];

    glGetVertexAttribfv(0, GL_CURRENT_VERTEX_ATTRIB, tmp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}
