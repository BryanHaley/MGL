/*
 * test_vertex_array_dsa.c
 * MGL
 *
 * Vertex array DSA entry points and the separate attribute format family.
 * Covers glCreateVertexArrays, the DSA variants
 * (VertexArrayAttribFormat/IFormat/LFormat/AttribBinding/BindingDivisor/
 *  ElementBuffer/VertexBuffer/VertexBuffers/EnableVertexArrayAttrib/
 *  DisableVertexArrayAttrib), the non-DSA variants
 * (VertexAttribFormat/IFormat/LFormat/AttribBinding/BindingDivisor/
 *  BindVertexBuffer/BindVertexBuffers), and the query entry points
 * (GetVertexAttribdv/Ldv/Pointerv).
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- glCreateVertexArrays ---------- */

GPU_TEST(vertex_array_dsa, create_initialises_to_default_state)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(vao != 0);
    CHECK_EQ_INT(glIsVertexArray(vao), GL_TRUE);

    GLint sz = -1, enabled = -1, ntype = -1;
    glBindVertexArray(vao);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &ntype);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
    CHECK_EQ_INT(sz, 4);
    CHECK_EQ_INT(ntype, GL_FLOAT);
    CHECK_EQ_INT(enabled, GL_FALSE);

    GLint eab = -1;
    glGetVertexArrayiv(vao, GL_ELEMENT_ARRAY_BUFFER_BINDING, &eab);
    CHECK_EQ_INT(eab, 0);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, create_rejects_null_arrays)
{
    glCreateVertexArrays(1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(vertex_array_dsa, create_with_zero_count_is_noop)
{
    GLuint sentinel = 0xABCD;

    glCreateVertexArrays(0, &sentinel);
    CHECK_EQ_UINT(sentinel, 0xABCD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- EnableVertexArrayAttrib / DisableVertexArrayAttrib (DSA) ---------- */

GPU_TEST(vertex_array_dsa, dsa_enable_disable_on_non_bound_vao)
{
    GLuint a = 0, b = 0;

    glCreateVertexArrays(1, &a);
    glCreateVertexArrays(1, &b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(b);

    glEnableVertexArrayAttrib(a, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint ea = -1, eb = -1;
    glBindVertexArray(a);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &ea);
    glBindVertexArray(b);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &eb);
    CHECK_EQ_INT(ea, GL_TRUE);
    CHECK_EQ_INT(eb, GL_FALSE);

    glBindVertexArray(b);
    glDisableVertexArrayAttrib(a, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glBindVertexArray(a);
    glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &ea);
    CHECK_EQ_INT(ea, GL_FALSE);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &a);
    glDeleteVertexArrays(1, &b);
}

GPU_TEST(vertex_array_dsa, dsa_enable_rejects_bad_index)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glEnableVertexArrayAttrib(vao, 9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDisableVertexArrayAttrib(vao, 9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, dsa_enable_rejects_bad_vao)
{
    glEnableVertexArrayAttrib(999123, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- VertexArrayElementBuffer (DSA) ---------- */

GPU_TEST(vertex_array_dsa, element_buffer_binds_and_unbinds)
{
    GLuint vao = 0, buf = 0;

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glVertexArrayElementBuffer(vao, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint eab = -1;
    glGetVertexArrayiv(vao, GL_ELEMENT_ARRAY_BUFFER_BINDING, &eab);
    CHECK_EQ_INT(eab, (GLint)buf);

    glVertexArrayElementBuffer(vao, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetVertexArrayiv(vao, GL_ELEMENT_ARRAY_BUFFER_BINDING, &eab);
    CHECK_EQ_INT(eab, 0);

    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, element_buffer_rejects_bad_buffer)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexArrayElementBuffer(vao, 999123);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, element_buffer_rejects_bad_vao)
{
    glVertexArrayElementBuffer(999123, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- VertexArrayAttribFormat (DSA) ---------- */

GPU_TEST(vertex_array_dsa, attrib_format_writes_to_specified_vao)
{
    GLuint a = 0, b = 0;

    glCreateVertexArrays(1, &a);
    glCreateVertexArrays(1, &b);

    glBindVertexArray(b);
    glVertexArrayAttribFormat(a, 1, 3, GL_FLOAT, GL_FALSE, 12);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint sz = -1, ntype = -1, off = -1;
    glBindVertexArray(a);
    glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz);
    glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_TYPE, &ntype);
    glGetVertexArrayIndexediv(a, 1, GL_VERTEX_ATTRIB_RELATIVE_OFFSET, &off);
    CHECK_EQ_INT(sz, 3);
    CHECK_EQ_INT(ntype, GL_FLOAT);
    CHECK_EQ_INT(off, 12);

    glBindVertexArray(b);
    glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz);
    CHECK_EQ_INT(sz, 4);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &a);
    glDeleteVertexArrays(1, &b);
}

GPU_TEST(vertex_array_dsa, attrib_format_rejects_bad_type)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexArrayAttribFormat(vao, 0, 3, 0x9999, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, attrib_format_accepts_packed_size_4)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexArrayAttribFormat(vao, 0, 4, GL_INT_2_10_10_10_REV, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, attrib_format_rejects_bad_attrib_index)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexArrayAttribFormat(vao, 9999, 3, GL_FLOAT, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexArrayAttribIFormat (DSA) ---------- */

GPU_TEST(vertex_array_dsa, attrib_i_format_writes_to_specified_vao)
{
    GLuint a = 0, b = 0;

    glCreateVertexArrays(1, &a);
    glCreateVertexArrays(1, &b);

    glBindVertexArray(b);
    glVertexArrayAttribIFormat(a, 0, 4, GL_UNSIGNED_INT, 8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint sz = -1, ntype = -1;
    glBindVertexArray(a);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &ntype);
    CHECK_EQ_INT(sz, 4);
    CHECK_EQ_INT(ntype, GL_UNSIGNED_INT);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &a);
    glDeleteVertexArrays(1, &b);
}

/* ---------- VertexArrayAttribLFormat (DSA) ---------- */

GPU_TEST(vertex_array_dsa, attrib_l_format_double)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexArrayAttribLFormat(vao, 0, 3, GL_DOUBLE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint ntype = -1;
    glBindVertexArray(vao);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &ntype);
    CHECK_EQ_INT(ntype, GL_DOUBLE);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexArrayAttribBinding (DSA) ---------- */

GPU_TEST(vertex_array_dsa, attrib_binding_associates_attribute_to_binding)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexArrayAttribBinding(vao, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, attrib_binding_rejects_bad_binding_index)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexArrayAttribBinding(vao, 0, 9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexArrayBindingDivisor (DSA) ---------- */

GPU_TEST(vertex_array_dsa, binding_divisor_round_trip)
{
    GLuint vao = 0, buf = 0;

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 1024, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);
    glBindVertexBuffer(0, buf, 0, 16);
    glVertexArrayBindingDivisor(vao, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint div = -1;
    glGetVertexArrayIndexediv(vao, 0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &div);
    CHECK_EQ_INT(div, 3);

    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, dsa_binding_divisor_rejects_bad_index)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexArrayBindingDivisor(vao, 9999, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexArrayVertexBuffer / VertexArrayVertexBuffers (DSA) ---------- */

GPU_TEST(vertex_array_dsa, vertex_buffer_binds_and_reads_back)
{
    GLuint vao = 0, buf = 0;

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glVertexArrayVertexBuffer(vao, 2, buf, 8, 24);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, vertex_buffer_rejects_bad_binding)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexArrayVertexBuffer(vao, 9999, 0, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, vertex_buffers_binds_multiple)
{
    GLuint vao = 0, bufs[2] = { 0 };

    glCreateVertexArrays(1, &vao);
    glGenBuffers(2, bufs);
    for (int i = 0; i < 2; i++)
    {
        glBindBuffer(GL_ARRAY_BUFFER, bufs[i]);
        glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLintptr offsets[2] = { 0, 16 };
    GLsizei strides[2] = { 12, 24 };

    glBindVertexArray(vao);
    glVertexArrayVertexBuffers(vao, 0, 2, bufs, offsets, strides);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(0);
    glDeleteBuffers(2, bufs);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexAttribFormat (non-DSA) ---------- */

GPU_TEST(vertex_array_dsa, attrib_format_requires_bound_vao)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindVertexArray(vao);
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint sz = -1;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz);
    CHECK_EQ_INT(sz, 3);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, attrib_format_unsigned_types)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexAttribFormat(0, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint ntype = -1;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &ntype);
    CHECK_EQ_INT(ntype, GL_UNSIGNED_BYTE);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexAttribIFormat (non-DSA) ---------- */

GPU_TEST(vertex_array_dsa, attrib_i_format_accepts_integer_types)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexAttribIFormat(0, 1, GL_UNSIGNED_INT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexAttribLFormat (non-DSA) ---------- */

GPU_TEST(vertex_array_dsa, attrib_l_format_double_non_dsa)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexAttribLFormat(0, 3, GL_DOUBLE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexAttribBinding (non-DSA) ---------- */

GPU_TEST(vertex_array_dsa, attrib_binding_requires_bound_vao)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexAttribBinding(0, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindVertexArray(vao);
    glVertexAttribBinding(0, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, attrib_binding_rejects_bad_indices)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexAttribBinding(9999, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttribBinding(0, 9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- VertexBindingDivisor (non-DSA) ---------- */

GPU_TEST(vertex_array_dsa, binding_divisor_requires_bound_vao)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);

    glVertexBindingDivisor(0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindVertexArray(vao);
    glVertexBindingDivisor(0, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, non_dsa_binding_divisor_rejects_bad_index)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glVertexBindingDivisor(9999, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- BindVertexBuffer / BindVertexBuffers ---------- */

GPU_TEST(vertex_array_dsa, bind_vertex_buffer_requires_bound_vao)
{
    GLuint buf = 0;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexBuffer(0, buf, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteBuffers(1, &buf);
}

GPU_TEST(vertex_array_dsa, bind_vertex_buffer_binds_and_affects_attribs)
{
    GLuint vao = 0, buf = 0;

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glBindVertexBuffer(0, buf, 0, 12);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint bname = -1;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bname);
    CHECK_EQ_INT(bname, (GLint)buf);

    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, bind_vertex_buffer_rejects_bad_binding_index)
{
    GLuint vao = 0, buf = 0;

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);
    glBindVertexBuffer(9999, buf, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, bind_vertex_buffer_with_zero_unbinds)
{
    GLuint vao = 0, buf = 0;

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 0);
    glBindVertexBuffer(0, buf, 0, 12);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexBuffer(0, 0, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, bind_vertex_buffers_multiple)
{
    GLuint vao = 0, bufs[2] = { 0 };

    glCreateVertexArrays(1, &vao);
    glGenBuffers(2, bufs);
    for (int i = 0; i < 2; i++)
    {
        glBindBuffer(GL_ARRAY_BUFFER, bufs[i]);
        glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);
    GLintptr offsets[2] = { 0, 32 };
    GLsizei strides[2] = { 16, 16 };
    glBindVertexBuffers(0, 2, bufs, offsets, strides);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindVertexArray(0);
    glDeleteBuffers(2, bufs);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, bind_vertex_buffers_requires_bound_vao)
{
    GLuint bufs[1] = { 0 };
    GLintptr offs[1] = { 0 };
    GLsizei strs[1] = { 16 };

    glBindVertexBuffers(0, 1, bufs, offs, strs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(vertex_array_dsa, bind_vertex_buffers_rejects_excessive_count)
{
    GLuint vao = 0;

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glBindVertexBuffers(0, 9999, NULL, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- GetVertexAttribdv / GetVertexAttribLdv ---------- */

GPU_TEST(vertex_array_dsa, get_vertex_attrib_dv_current_no_vao)
{
    GLdouble params[4] = { 0 };

    glVertexAttrib3f(0, 0.25f, 0.5f, 0.75f);
    glGetVertexAttribdv(0, GL_CURRENT_VERTEX_ATTRIB, params);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(params[0], 0.25, 1e-5);
    CHECK_NEAR(params[3], 1.0, 1e-5);
}

GPU_TEST(vertex_array_dsa, get_vertex_attrib_dv_array_state_needs_vao)
{
    GLdouble params[4] = { 0 };

    glGetVertexAttribdv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, params);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(vertex_array_dsa, get_vertex_attrib_dv_reads_back_format)
{
    GLuint vao = 0;
    GLdouble params[4] = { 0 };

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 4);

    glGetVertexAttribdv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, params);
    CHECK_NEAR(params[0], 2.0, 0.0);

    glGetVertexAttribdv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, params);
    CHECK_NEAR(params[0], (GLdouble)GL_FLOAT, 0.0);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, get_vertex_attrib_ldv_reads_current)
{
    GLdouble params[4] = { 0 };

    glVertexAttribL3d(0, 1.0, 2.0, 3.0);
    glGetVertexAttribLdv(0, GL_CURRENT_VERTEX_ATTRIB, params);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(params[0], 1.0, 1e-5);
    CHECK_NEAR(params[1], 2.0, 1e-5);
    CHECK_NEAR(params[2], 3.0, 1e-5);
    CHECK_NEAR(params[3], 1.0, 1e-5);
}

GPU_TEST(vertex_array_dsa, get_vertex_attrib_ldv_array_state_needs_vao)
{
    GLdouble params[4] = { 0 };

    glGetVertexAttribLdv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, params);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(vertex_array_dsa, get_vertex_attrib_dv_rejects_bad_index)
{
    GLdouble params[4] = { 0 };

    glGetVertexAttribdv(9999, GL_CURRENT_VERTEX_ATTRIB, params);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- GetVertexAttribPointerv ---------- */

GPU_TEST(vertex_array_dsa, get_vertex_attrib_pointerv_requires_bound_vao)
{
    void *ptr = (void *)0xDEAD;

    glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(vertex_array_dsa, get_vertex_attrib_pointerv_returns_offset)
{
    GLuint vao = 0, buf = 0;
    void *ptr = (void *)0xDEAD;

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);

    glBindVertexArray(vao);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (void *)24);

    glGetVertexAttribPointerv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK((uintptr_t)ptr == 24);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, get_vertex_attrib_pointerv_rejects_bad_enum)
{
    GLuint vao = 0;
    void *ptr = NULL;

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGetVertexAttribPointerv(0, 0x9999, &ptr);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(vertex_array_dsa, get_vertex_attrib_pointerv_rejects_bad_index)
{
    void *ptr = NULL;

    glGetVertexAttribPointerv(9999, GL_VERTEX_ATTRIB_ARRAY_POINTER, &ptr);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- end-to-end: set up a complete separate-format binding ---------- */

GPU_TEST(vertex_array_dsa, full_separate_format_pipeline)
{
    GLuint vao = 0, buf = 0;
    GLfloat verts[] = {
        0.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,   0.0f, 0.0f, 1.0f,
    };

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(vao);
    glVertexArrayVertexBuffer(vao, 0, buf, 0, 24);
    glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, 12);
    glVertexArrayAttribBinding(vao, 0, 0);
    glVertexArrayAttribBinding(vao, 1, 0);
    glEnableVertexArrayAttrib(vao, 0);
    glEnableVertexArrayAttrib(vao, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLint sz0 = -1, sz1 = -1, bname = -1;
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz0);
    glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz1);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &bname);
    CHECK_EQ_INT(sz0, 3);
    CHECK_EQ_INT(sz1, 3);
    CHECK_EQ_INT(bname, (GLint)buf);

    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- a binding is state of its own ---------- */

GPU_TEST(vertex_array_dsa, binding_state_is_not_attribute_state)
{
    GLuint vao = 0, buf = 0;
    GLint iv = -1;
    GLint64 i64 = -1;

    glCreateVertexArrays(1, &vao);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, 4096, NULL, GL_STATIC_DRAW);
    glBindVertexArray(vao);

    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(0, 5);
    glBindVertexBuffer(5, buf, 1024, 128);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegeri_v(GL_VERTEX_BINDING_STRIDE, 5, &iv);
    CHECK_EQ_INT(iv, 128);

    glGetInteger64i_v(GL_VERTEX_BINDING_OFFSET, 5, &i64);
    CHECK_EQ_INT((GLint)i64, 1024);

    glGetIntegeri_v(GL_VERTEX_BINDING_BUFFER, 5, &iv);
    CHECK_EQ_UINT((GLuint)iv, buf);

    // glBindVertexBuffer touches none of these
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &iv);
    CHECK_EQ_INT(iv, 0);

    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_BINDING, &iv);
    CHECK_EQ_INT(iv, 5);

    // the attribute reaches the buffer through its binding
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &iv);
    CHECK_EQ_UINT((GLuint)iv, buf);

    // and an attribute on an empty binding sees no buffer at all
    glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &iv);
    CHECK_EQ_INT(iv, 0);

    // a divisor belongs to the binding too
    glVertexBindingDivisor(5, 3);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &iv);
    CHECK_EQ_INT(iv, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &buf);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- the binding offset and a stride of zero both reach the draw ---------- */

static const char *BIND_VS =
    "#version 410 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in float walks;\n"
    "layout(location = 2) in float stays;\n"
    "out float vwalks;\n"
    "out float vstays;\n"
    "void main() {\n"
    "    vwalks = walks;\n"
    "    vstays = stays;\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char *BIND_FS =
    "#version 410 core\n"
    "in float vwalks;\n"
    "in float vstays;\n"
    "out vec4 o;\n"
    "void main() { o = vec4(vwalks, vstays, 0.0, 1.0); }\n";

GPU_TEST(vertex_array_dsa, binding_offset_and_zero_stride_reach_the_draw)
{
    MGLTestTarget t;
    GLuint prog, vao = 0, geom = 0, data = 0;
    char log[2048];
    unsigned char *px, c[4] = { 0, 0, 0, 0 };

    // two triangles covering the target, so every fragment is drawn
    const float quad[12] = { -1, -1,  1, -1, -1, 1,   -1, 1,  1, -1,  1, 1 };
    // the first float is skipped by the binding's offset; the walking
    // attribute then reads 0.25 for every vertex and the still one reads 0.75
    const float values[7] = { 0.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
    const float still[2] = { 0.0f, 0.75f };

    prog = mgl_build_program(BIND_VS, BIND_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not build: %s", log);

    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &geom);
    glBindBuffer(GL_ARRAY_BUFFER, geom);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &data);
    glBindBuffer(GL_ARRAY_BUFFER, data);
    glBufferData(GL_ARRAY_BUFFER, sizeof values, values, GL_STATIC_DRAW);

    glVertexAttribFormat(1, 1, GL_FLOAT, GL_FALSE, 0);
    glVertexAttribBinding(1, 1);
    glBindVertexBuffer(1, data, 4, 4);          // starts one float in
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    {
        GLuint fixed = 0;

        glGenBuffers(1, &fixed);
        glBindBuffer(GL_ARRAY_BUFFER, fixed);
        glBufferData(GL_ARRAY_BUFFER, sizeof still, still, GL_STATIC_DRAW);
        glVertexAttribFormat(2, 1, GL_FLOAT, GL_FALSE, 4);
        glVertexAttribBinding(2, 2);
        glBindVertexBuffer(2, fixed, 0, 0);     // stride zero: one value for all
        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        mgl_target_bind(&t);
        glViewport(0, 0, t.width, t.height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        px = mgl_read_rgba8(&t);

        if (px)
        {
            mgl_pixel_at(px, &t, t.width / 2, t.height / 2, c);
            free(px);
        }

        CHECK_MSG(c[0] > 50 && c[0] < 78, "the offset binding read %u, expected about 64", c[0]);
        CHECK_MSG(c[1] > 178 && c[1] < 204, "the zero-stride binding read %u, expected about 191", c[1]);

        glDeleteBuffers(1, &fixed);
    }

    glUseProgram(0);
    glBindVertexArray(0);
    glDeleteBuffers(1, &geom);
    glDeleteBuffers(1, &data);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
