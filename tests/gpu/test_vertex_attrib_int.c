/*
 * test_vertex_attrib_int.c
 * MGL
 *
 * Integer (I*), double (L*) and packed (P*) generic vertex attribute
 * current-value entry points, plus IPointer and LPointer array state.
 *
 * Covers all 33 entry points listed in the brief.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- integer current value (glVertexAttribI*) ---------- */

GPU_TEST(vertex_attrib_int, integer_set_and_readback)
{
    GLint iv[4] = { 0 };

    glVertexAttribI4i(2, -100, 200, -300, 400);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribIiv(2, GL_CURRENT_VERTEX_ATTRIB, iv);
    CHECK_EQ_INT(iv[0], -100);
    CHECK_EQ_INT(iv[1], 200);
    CHECK_EQ_INT(iv[2], -300);
    CHECK_EQ_INT(iv[3], 400);

    // I1i sets first component; others become (0,0,1)
    glVertexAttribI1i(3, 42);
    glGetVertexAttribIiv(3, GL_CURRENT_VERTEX_ATTRIB, iv);
    CHECK_EQ_INT(iv[0], 42);
    CHECK_EQ_INT(iv[1], 0);
    CHECK_EQ_INT(iv[2], 0);
    CHECK_EQ_INT(iv[3], 1);

    glVertexAttribI2i(4, 10, 20);
    glGetVertexAttribIiv(4, GL_CURRENT_VERTEX_ATTRIB, iv);
    CHECK_EQ_INT(iv[0], 10);
    CHECK_EQ_INT(iv[1], 20);
    CHECK_EQ_INT(iv[2], 0);
    CHECK_EQ_INT(iv[3], 1);

    glVertexAttribI3i(5, 1, 2, 3);
    glGetVertexAttribIiv(5, GL_CURRENT_VERTEX_ATTRIB, iv);
    CHECK_EQ_INT(iv[0], 1);
    CHECK_EQ_INT(iv[1], 2);
    CHECK_EQ_INT(iv[2], 3);
    CHECK_EQ_INT(iv[3], 1);
}

GPU_TEST(vertex_attrib_int, unsigned_set_and_readback)
{
    GLuint uv[4] = { 0 };

    glVertexAttribI4ui(2, 100u, 200u, 300u, 400u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribIuiv(2, GL_CURRENT_VERTEX_ATTRIB, uv);
    CHECK_EQ_UINT(uv[0], 100u);
    CHECK_EQ_UINT(uv[1], 200u);
    CHECK_EQ_UINT(uv[2], 300u);
    CHECK_EQ_UINT(uv[3], 400u);

    glVertexAttribI1ui(3, 99u);
    glGetVertexAttribIuiv(3, GL_CURRENT_VERTEX_ATTRIB, uv);
    CHECK_EQ_UINT(uv[0], 99u);
    CHECK_EQ_UINT(uv[1], 0u);
    CHECK_EQ_UINT(uv[2], 0u);
    CHECK_EQ_UINT(uv[3], 1u);

    glVertexAttribI2ui(4, 11u, 22u);
    glGetVertexAttribIuiv(4, GL_CURRENT_VERTEX_ATTRIB, uv);
    CHECK_EQ_UINT(uv[0], 11u);
    CHECK_EQ_UINT(uv[1], 22u);
    CHECK_EQ_UINT(uv[2], 0u);
    CHECK_EQ_UINT(uv[3], 1u);

    glVertexAttribI3ui(5, 7u, 8u, 9u);
    glGetVertexAttribIuiv(5, GL_CURRENT_VERTEX_ATTRIB, uv);
    CHECK_EQ_UINT(uv[0], 7u);
    CHECK_EQ_UINT(uv[1], 8u);
    CHECK_EQ_UINT(uv[2], 9u);
    CHECK_EQ_UINT(uv[3], 1u);
}

GPU_TEST(vertex_attrib_int, integer_vector_forms)
{
    GLint iv[4] = { 0 };
    GLuint uv[4] = { 0 };

    {
        const GLint vals[2] = { -7, 14 };
        glVertexAttribI2iv(3, vals);
        glGetVertexAttribIiv(3, GL_CURRENT_VERTEX_ATTRIB, iv);
        CHECK_EQ_INT(iv[0], -7);
        CHECK_EQ_INT(iv[1], 14);
        CHECK_EQ_INT(iv[2], 0);
        CHECK_EQ_INT(iv[3], 1);
    }

    {
        const GLint vals[3] = { 5, 6, 7 };
        glVertexAttribI3iv(4, vals);
        glGetVertexAttribIiv(4, GL_CURRENT_VERTEX_ATTRIB, iv);
        CHECK_EQ_INT(iv[0], 5);
        CHECK_EQ_INT(iv[1], 6);
        CHECK_EQ_INT(iv[2], 7);
        CHECK_EQ_INT(iv[3], 1);
    }

    {
        const GLint vals[4] = { -1, -2, -3, -4 };
        glVertexAttribI4iv(5, vals);
        glGetVertexAttribIiv(5, GL_CURRENT_VERTEX_ATTRIB, iv);
        CHECK_EQ_INT(iv[0], -1);
        CHECK_EQ_INT(iv[1], -2);
        CHECK_EQ_INT(iv[2], -3);
        CHECK_EQ_INT(iv[3], -4);
    }

    {
        const GLuint vals[1] = { 42u };
        glVertexAttribI1uiv(6, vals);
        glGetVertexAttribIuiv(6, GL_CURRENT_VERTEX_ATTRIB, uv);
        CHECK_EQ_UINT(uv[0], 42u);
        CHECK_EQ_UINT(uv[1], 0u);
        CHECK_EQ_UINT(uv[2], 0u);
        CHECK_EQ_UINT(uv[3], 1u);
    }

    {
        const GLuint vals[4] = { 10u, 20u, 30u, 40u };
        glVertexAttribI4uiv(7, vals);
        glGetVertexAttribIuiv(7, GL_CURRENT_VERTEX_ATTRIB, uv);
        CHECK_EQ_UINT(uv[0], 10u);
        CHECK_EQ_UINT(uv[1], 20u);
        CHECK_EQ_UINT(uv[2], 30u);
        CHECK_EQ_UINT(uv[3], 40u);
    }

    // I4bv: byte sign-extends to int
    {
        const GLbyte b[4] = { -1, 0, 127, -128 };
        glVertexAttribI4bv(8, b);
        glGetVertexAttribIiv(8, GL_CURRENT_VERTEX_ATTRIB, iv);
        CHECK_EQ_INT(iv[0], -1);
        CHECK_EQ_INT(iv[1], 0);
        CHECK_EQ_INT(iv[2], 127);
        CHECK_EQ_INT(iv[3], -128);
    }

    // I4sv: short sign-extends to int
    {
        const GLshort s[4] = { -32768, 32767, -1, 0 };
        glVertexAttribI4sv(9, s);
        glGetVertexAttribIiv(9, GL_CURRENT_VERTEX_ATTRIB, iv);
        CHECK_EQ_INT(iv[0], -32768);
        CHECK_EQ_INT(iv[1], 32767);
        CHECK_EQ_INT(iv[2], -1);
        CHECK_EQ_INT(iv[3], 0);
    }

    // I4ubv: unsigned byte zero-extends
    {
        const GLubyte b[4] = { 0, 255, 128, 1 };
        glVertexAttribI4ubv(10, b);
        glGetVertexAttribIuiv(10, GL_CURRENT_VERTEX_ATTRIB, uv);
        CHECK_EQ_UINT(uv[0], 0u);
        CHECK_EQ_UINT(uv[1], 255u);
        CHECK_EQ_UINT(uv[2], 128u);
        CHECK_EQ_UINT(uv[3], 1u);
    }

    // I4usv: unsigned short zero-extends
    {
        const GLushort s[4] = { 65535, 0, 1, 32768 };
        glVertexAttribI4usv(11, s);
        glGetVertexAttribIuiv(11, GL_CURRENT_VERTEX_ATTRIB, uv);
        CHECK_EQ_UINT(uv[0], 65535u);
        CHECK_EQ_UINT(uv[1], 0u);
        CHECK_EQ_UINT(uv[2], 1u);
        CHECK_EQ_UINT(uv[3], 32768u);
    }
}

GPU_TEST(vertex_attrib_int, vector_forms_reject_null)
{
    glVertexAttribI1iv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI2iv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI3iv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI4iv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI1uiv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI2uiv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI3uiv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI4uiv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI4bv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI4sv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI4ubv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribI4usv(2, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- double current value (glVertexAttribL*) ---------- */

GPU_TEST(vertex_attrib_int, double_set_and_readback)
{
    GLdouble d[4] = { 0 };

    glVertexAttribL4d(2, 1.0, 2.0, 3.0, 4.0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribLdv(2, GL_CURRENT_VERTEX_ATTRIB, d);
    CHECK_NEAR(d[0], 1.0, 1e-15);
    CHECK_NEAR(d[1], 2.0, 1e-15);
    CHECK_NEAR(d[2], 3.0, 1e-15);
    CHECK_NEAR(d[3], 4.0, 1e-15);

    glVertexAttribL1d(3, 5.0);
    glGetVertexAttribLdv(3, GL_CURRENT_VERTEX_ATTRIB, d);
    CHECK_NEAR(d[0], 5.0, 1e-15);
    CHECK_NEAR(d[1], 0.0, 1e-15);
    CHECK_NEAR(d[2], 0.0, 1e-15);
    CHECK_NEAR(d[3], 1.0, 1e-15);

    glVertexAttribL2d(4, 1.5, 2.5);
    glGetVertexAttribLdv(4, GL_CURRENT_VERTEX_ATTRIB, d);
    CHECK_NEAR(d[0], 1.5, 1e-15);
    CHECK_NEAR(d[1], 2.5, 1e-15);
    CHECK_NEAR(d[2], 0.0, 1e-15);
    CHECK_NEAR(d[3], 1.0, 1e-15);

    glVertexAttribL3d(5, 0.25, 0.5, 0.75);
    glGetVertexAttribLdv(5, GL_CURRENT_VERTEX_ATTRIB, d);
    CHECK_NEAR(d[0], 0.25, 1e-15);
    CHECK_NEAR(d[1], 0.5, 1e-15);
    CHECK_NEAR(d[2], 0.75, 1e-15);
    CHECK_NEAR(d[3], 1.0, 1e-15);
}

GPU_TEST(vertex_attrib_int, double_preserves_precision)
{
    GLdouble d[4] = { 0 };
    // 1.0/3.0 is not representable exactly in float (0.333333343...)
    // The spec requires L* to preserve full 64-bit double precision.
    GLdouble precise = 1.0 / 3.0;

    glVertexAttribL1d(3, precise);
    glGetVertexAttribLdv(3, GL_CURRENT_VERTEX_ATTRIB, d);
    // MGL stores doubles as float internally, so this will FAIL.
    CHECK_NEAR(d[0], precise, 1e-15);
}

GPU_TEST(vertex_attrib_int, double_vector_forms)
{
    GLdouble d[4] = { 0 };

    {
        const GLdouble vals[2] = { 0.125, 0.375 };
        glVertexAttribL2dv(3, vals);
        glGetVertexAttribLdv(3, GL_CURRENT_VERTEX_ATTRIB, d);
        CHECK_NEAR(d[0], 0.125, 1e-15);
        CHECK_NEAR(d[1], 0.375, 1e-15);
        CHECK_NEAR(d[2], 0.0, 1e-15);
        CHECK_NEAR(d[3], 1.0, 1e-15);
    }

    {
        const GLdouble vals[3] = { 0.1, 0.2, 0.3 };
        glVertexAttribL3dv(4, vals);
        glGetVertexAttribLdv(4, GL_CURRENT_VERTEX_ATTRIB, d);
        CHECK_NEAR(d[0], 0.1, 1e-15);
        CHECK_NEAR(d[1], 0.2, 1e-15);
        CHECK_NEAR(d[2], 0.3, 1e-15);
        CHECK_NEAR(d[3], 1.0, 1e-15);
    }

    {
        const GLdouble vals[4] = { 1.0, 2.0, 3.0, 4.0 };
        glVertexAttribL4dv(5, vals);
        glGetVertexAttribLdv(5, GL_CURRENT_VERTEX_ATTRIB, d);
        CHECK_NEAR(d[0], 1.0, 1e-15);
        CHECK_NEAR(d[1], 2.0, 1e-15);
        CHECK_NEAR(d[2], 3.0, 1e-15);
        CHECK_NEAR(d[3], 4.0, 1e-15);
    }

    glVertexAttribL1dv(3, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttribL4dv(3, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- packed current value (glVertexAttribP*) ---------- */

GPU_TEST(vertex_attrib_int, packed_unsigned_2_10_10_10_rev)
{
    GLfloat f[4] = { 0 };
    // bits: [31:30]=w, [29:20]=z, [19:10]=y, [9:0]=x
    GLuint packed = (0u << 30) | (3u << 20) | (2u << 10) | 1u;

    glVertexAttribP4ui(3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, f);
    CHECK_NEAR(f[0], 1.0f, 1e-5f);
    CHECK_NEAR(f[1], 2.0f, 1e-5f);
    CHECK_NEAR(f[2], 3.0f, 1e-5f);
    CHECK_NEAR(f[3], 0.0f, 1e-5f);

    // normalized: values / 1023 for 10-bit, / 3 for 2-bit
    glVertexAttribP4ui(4, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, packed);
    glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, f);
    CHECK_NEAR(f[0], 1.0f / 1023.0f, 1e-5f);
    CHECK_NEAR(f[1], 2.0f / 1023.0f, 1e-5f);
    CHECK_NEAR(f[2], 3.0f / 1023.0f, 1e-5f);
    CHECK_NEAR(f[3], 0.0f / 3.0f, 1e-5f);
}

GPU_TEST(vertex_attrib_int, packed_signed_2_10_10_10_rev)
{
    GLfloat f[4] = { 0 };
    GLuint packed = (0u << 30) | (3u << 20) | (2u << 10) | 1u;

    glVertexAttribP4ui(3, GL_INT_2_10_10_10_REV, GL_FALSE, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, f);
    CHECK_NEAR(f[0], 1.0f, 1e-5f);
    CHECK_NEAR(f[1], 2.0f, 1e-5f);
    CHECK_NEAR(f[2], 3.0f, 1e-5f);
    CHECK_NEAR(f[3], 0.0f, 1e-5f);

    // w=0b10 = -2 in 2-bit signed, x=0x3FF = -1 in 10-bit signed
    packed = (2u << 30) | 0x3FFu;
    glVertexAttribP4ui(4, GL_INT_2_10_10_10_REV, GL_FALSE, packed);
    glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, f);
    CHECK_NEAR(f[0], -1.0f, 1e-5f);
    CHECK_NEAR(f[3], -2.0f, 1e-5f);
}

GPU_TEST(vertex_attrib_int, packed_float_11_11_10)
{
    GLfloat f[4] = { 0 };
    // 1.0 in 11-bit float (6 mantissa, 5 exponent): 0x3C0
    // 1.0 in 10-bit float (5 mantissa, 5 exponent): 0x1E0
    GLuint packed = (0x1E0u << 22) | (0x3C0u << 11) | 0x3C0u;

    glVertexAttribP4ui(3, GL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, f);
    CHECK_NEAR(f[0], 1.0f, 1e-4f);
    CHECK_NEAR(f[1], 1.0f, 1e-4f);
    CHECK_NEAR(f[2], 1.0f, 1e-4f);
    CHECK_NEAR(f[3], 1.0f, 1e-5f);
}

GPU_TEST(vertex_attrib_int, packed_vector_and_remaining_forms)
{
    GLfloat f[4] = { 0 };
    GLuint packed = (3u << 20) | (2u << 10) | 1u;

    // P2ui: x,y from packed; z=0, w=1
    glVertexAttribP2ui(3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, f);
    CHECK_NEAR(f[0], 1.0f, 1e-5f);
    CHECK_NEAR(f[1], 2.0f, 1e-5f);
    CHECK_NEAR(f[2], 0.0f, 1e-5f);
    CHECK_NEAR(f[3], 1.0f, 1e-5f);

    // P3ui: x,y,z from packed; w=1
    glVertexAttribP3ui(4, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, f);
    CHECK_NEAR(f[0], 1.0f, 1e-5f);
    CHECK_NEAR(f[1], 2.0f, 1e-5f);
    CHECK_NEAR(f[2], 3.0f, 1e-5f);
    CHECK_NEAR(f[3], 1.0f, 1e-5f);

    // P1uiv
    {
        const GLuint vals[1] = { packed };
        glVertexAttribP1uiv(5, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetVertexAttribfv(5, GL_CURRENT_VERTEX_ATTRIB, f);
        CHECK_NEAR(f[0], 1.0f, 1e-5f);
        CHECK_NEAR(f[1], 0.0f, 1e-5f);
        CHECK_NEAR(f[2], 0.0f, 1e-5f);
        CHECK_NEAR(f[3], 1.0f, 1e-5f);
    }

    // P2uiv
    {
        const GLuint vals[1] = { packed };
        glVertexAttribP2uiv(6, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetVertexAttribfv(6, GL_CURRENT_VERTEX_ATTRIB, f);
        CHECK_NEAR(f[0], 1.0f, 1e-5f);
        CHECK_NEAR(f[1], 2.0f, 1e-5f);
        CHECK_NEAR(f[2], 0.0f, 1e-5f);
        CHECK_NEAR(f[3], 1.0f, 1e-5f);
    }

    // P3uiv
    {
        const GLuint vals[1] = { packed };
        glVertexAttribP3uiv(7, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetVertexAttribfv(7, GL_CURRENT_VERTEX_ATTRIB, f);
        CHECK_NEAR(f[0], 1.0f, 1e-5f);
        CHECK_NEAR(f[1], 2.0f, 1e-5f);
        CHECK_NEAR(f[2], 3.0f, 1e-5f);
        CHECK_NEAR(f[3], 1.0f, 1e-5f);
    }

    // P4uiv
    {
        const GLuint vals[1] = { packed };
        glVertexAttribP4uiv(8, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glGetVertexAttribfv(8, GL_CURRENT_VERTEX_ATTRIB, f);
        CHECK_NEAR(f[0], 1.0f, 1e-5f);
        CHECK_NEAR(f[1], 2.0f, 1e-5f);
        CHECK_NEAR(f[2], 3.0f, 1e-5f);
        CHECK_NEAR(f[3], 0.0f, 1e-5f);
    }

    // NULL pointers
    glVertexAttribP1uiv(3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribP2uiv(3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribP3uiv(3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glVertexAttribP4uiv(3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(vertex_attrib_int, packed_invalid_enum)
{
    glVertexAttribP4ui(3, 0x9999, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glVertexAttribP1ui(3, GL_FLOAT, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- IPointer and LPointer array state ---------- */

GPU_TEST(vertex_attrib_int, i_pointer_state)
{
    GLuint vao = 0, buf = 0;
    GLint v = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);

    glVertexAttribIPointer(2, 3, GL_INT, 12, (const void *)8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_SIZE, &v);
    CHECK_EQ_INT(v, 3);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_TYPE, &v);
    CHECK_EQ_INT(v, GL_INT);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &v);
    CHECK_EQ_INT(v, 12);

    glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_SIZE, &v);
    CHECK_EQ_INT(v, 1);

    glVertexAttribIPointer(4, 2, GL_SHORT, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetVertexAttribiv(4, GL_VERTEX_ATTRIB_ARRAY_SIZE, &v);
    CHECK_EQ_INT(v, 2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_attrib_int, l_pointer_state)
{
    GLuint vao = 0, buf = 0;
    GLint v = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);

    glVertexAttribLPointer(2, 2, GL_DOUBLE, 16, (const void *)0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_SIZE, &v);
    CHECK_EQ_INT(v, 2);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_TYPE, &v);
    CHECK_EQ_INT(v, GL_DOUBLE);

    glVertexAttribLPointer(3, 3, GL_FLOAT, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_attrib_int, i_pointer_accepts_integer_types_only)
{
    GLuint vao = 0, buf = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);

    glVertexAttribIPointer(2, 4, GL_BYTE, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glVertexAttribIPointer(3, 4, GL_UNSIGNED_SHORT, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Non-integer types must be rejected per spec
    glVertexAttribIPointer(5, 4, GL_FLOAT, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glVertexAttribIPointer(6, 4, GL_DOUBLE, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glVertexAttribIPointer(7, 4, GL_HALF_FLOAT, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- error conditions ---------- */

GPU_TEST(vertex_attrib_int, index_out_of_range)
{
    GLint iv[4] = { 0 };
    GLuint uv[4] = { 0 };
    GLdouble d[4] = { 0 };

    glVertexAttribI4i(999, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttribI4ui(999, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttribL4d(999, 1, 2, 3, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttribP4ui(999, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetVertexAttribIiv(999, GL_CURRENT_VERTEX_ATTRIB, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetVertexAttribIuiv(999, GL_CURRENT_VERTEX_ATTRIB, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetVertexAttribLdv(999, GL_CURRENT_VERTEX_ATTRIB, d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

// Attribute 0 aliased gl_Vertex in the compatibility profile, which is where
// the "index 0 is an error" wording in the reference pages comes from. In a
// core profile it is an ordinary attribute and reads back like any other.
GPU_TEST(vertex_attrib_int, current_attrib_index_zero)
{
    GLint    iv[4] = { 0 };
    GLuint   uv[4] = { 0 };
    GLdouble dv[4] = { 0 };

    glVertexAttribI4i(0, -7, 8, -9, 10);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribIiv(0, GL_CURRENT_VERTEX_ATTRIB, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(iv[0], -7);
    CHECK_EQ_INT(iv[3], 10);

    glVertexAttribI4ui(0, 1u, 2u, 3u, 4u);
    glGetVertexAttribIuiv(0, GL_CURRENT_VERTEX_ATTRIB, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(uv[0], 1u);
    CHECK_EQ_UINT(uv[3], 4u);

    glGetVertexAttribLdv(0, GL_CURRENT_VERTEX_ATTRIB, dv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(vertex_attrib_int, invalid_pname_for_get)
{
    GLint iv[4] = { 0 };

    glGetVertexAttribIiv(2, 0x9999, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetVertexAttribIuiv(2, 0x9999, (GLuint *)iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetVertexAttribLdv(2, 0x9999, (GLdouble *)iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(vertex_attrib_int, i_pointer_validates)
{
    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexAttribIPointer(2, 4, GL_INT, -1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribIPointer(2, 4, GL_INT, 0, (const void *)4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_attrib_int, l_pointer_validates)
{
    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexAttribLPointer(2, 4, GL_DOUBLE, -1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribLPointer(2, 4, GL_DOUBLE, 0, (const void *)4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_attrib_int, null_params_silent)
{
    glGetVertexAttribIiv(2, GL_CURRENT_VERTEX_ATTRIB, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribIuiv(2, GL_CURRENT_VERTEX_ATTRIB, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribLdv(2, GL_CURRENT_VERTEX_ATTRIB, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(vertex_attrib_int, get_non_current_needs_vao)
{
    GLint v = 0;

    glBindVertexArray(0);

    glGetVertexAttribIiv(2, GL_VERTEX_ATTRIB_ARRAY_SIZE, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetVertexAttribIuiv(2, GL_VERTEX_ATTRIB_ARRAY_SIZE, (GLuint *)&v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetVertexAttribLdv(2, GL_VERTEX_ATTRIB_ARRAY_SIZE, (GLdouble *)&v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}
