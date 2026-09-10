/*
 * test_tier1.c
 * MGL
 *
 * Constant vertex attributes, DSA program uniforms, indexed viewport/scissor
 * state and the indexed getters.
 */

#include "mgl_test.h"
#include "harness.h"
#include <string.h>

#define W 32
#define H 32

/* A disabled attribute must reach the shader as its constant value. */
static const char *VS_ATTR_COLOUR =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 1) in vec4 col;\n"
    "layout(location = 0) out vec4 v_col;\n"
    "void main() { v_col = col; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_PASS_COLOUR =
    "#version 460 core\n"
    "layout(location = 0) in vec4 v_col;\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = v_col; }\n";

static const char *VS_QUAD =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_UNIFORM_COLOUR =
    "#version 460 core\n"
    "layout(location = 0) uniform vec4 u_col;\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = u_col; }\n";

/* ---------- constant vertex attributes ---------- */

GPU_TEST(attrib_const, disabled_array_feeds_constant_to_shader)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao, vbo;
    unsigned char *px, rgba[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&t);

    prog = mgl_build_program(VS_ATTR_COLOUR, FS_PASS_COLOUR, log, sizeof log);
    CHECK_MSG(prog != 0, "link failed: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glBindVertexArray(vao);
    glUseProgram(prog);

    // attribute 1 has no array; it must read the constant
    glDisableVertexAttribArray(1);
    glVertexAttrib4f(1, 0.0f, 0.0f, 1.0f, 1.0f);

    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, W/2, H/2, rgba);
        CHECK_EQ_INT(rgba[0], 0);
        CHECK_EQ_INT(rgba[1], 0);
        CHECK_MSG(rgba[2] > 200, "blue constant did not reach the shader (got %d)", rgba[2]);
        free(px);
    }

    glUseProgram(0);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(attrib_const, default_is_zero_zero_zero_one)
{
    GLfloat v[4] = { 9, 9, 9, 9 };

    glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(v[0], 0.0f, 0.001f);
    CHECK_NEAR(v[1], 0.0f, 0.001f);
    CHECK_NEAR(v[2], 0.0f, 0.001f);
    CHECK_NEAR(v[3], 1.0f, 0.001f);
}

GPU_TEST(attrib_const, short_forms_pad_with_zero_and_one)
{
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttrib2f(3, 0.25f, 0.5f);
    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, v);

    CHECK_NEAR(v[0], 0.25f, 0.001f);
    CHECK_NEAR(v[1], 0.5f,  0.001f);
    CHECK_NEAR(v[2], 0.0f,  0.001f);
    CHECK_NEAR(v[3], 1.0f,  0.001f);
}

GPU_TEST(attrib_const, normalized_unsigned_maps_to_zero_one)
{
    GLubyte c[4] = { 255, 0, 128, 255 };
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttrib4Nubv(4, c);
    glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, v);

    CHECK_NEAR(v[0], 1.0f, 0.001f);
    CHECK_NEAR(v[1], 0.0f, 0.001f);
    CHECK_NEAR(v[2], 128.0f/255.0f, 0.01f);
    CHECK_NEAR(v[3], 1.0f, 0.001f);
}

GPU_TEST(attrib_const, normalized_signed_clamps_at_minus_one)
{
    GLbyte c[4] = { -128, 127, 0, 0 };
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttrib4Nbv(5, c);
    glGetVertexAttribfv(5, GL_CURRENT_VERTEX_ATTRIB, v);

    // -128/127 is below -1 and must be clamped
    CHECK_NEAR(v[0], -1.0f, 0.001f);
    CHECK_NEAR(v[1],  1.0f, 0.001f);
}

GPU_TEST(attrib_const, non_normalized_byte_keeps_its_value)
{
    GLshort c[4] = { 300, -300, 1, 2 };
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttrib4sv(6, c);
    glGetVertexAttribfv(6, GL_CURRENT_VERTEX_ATTRIB, v);

    CHECK_NEAR(v[0],  300.0f, 0.001f);
    CHECK_NEAR(v[1], -300.0f, 0.001f);
}

GPU_TEST(attrib_const, integer_forms_round_trip)
{
    GLint iv[4] = { 0 };
    GLuint uv[4] = { 0 };

    glVertexAttribI4i(7, -5, 6, -7, 8);
    glGetVertexAttribIiv(7, GL_CURRENT_VERTEX_ATTRIB, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(iv[0], -5);
    CHECK_EQ_INT(iv[1],  6);
    CHECK_EQ_INT(iv[2], -7);
    CHECK_EQ_INT(iv[3],  8);

    glVertexAttribI4ui(8, 1, 2, 3, 4);
    glGetVertexAttribIuiv(8, GL_CURRENT_VERTEX_ATTRIB, uv);
    CHECK_EQ_UINT(uv[0], 1u);
    CHECK_EQ_UINT(uv[3], 4u);
}

GPU_TEST(attrib_const, rejects_out_of_range_index)
{
    glVertexAttrib4f(9999, 0, 0, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glVertexAttribI4i(9999, 0, 0, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- DSA program uniforms ---------- */

GPU_TEST(program_uniform, sets_without_binding_the_program)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao, vbo;
    unsigned char *px, rgba[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&t);

    prog = mgl_build_program(VS_QUAD, FS_UNIFORM_COLOUR, log, sizeof log);
    CHECK_MSG(prog != 0, "link failed: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glBindVertexArray(vao);

    // deliberately not current when the uniform is set
    glUseProgram(0);
    glProgramUniform4f(prog, glGetUniformLocation(prog, "u_col"), 0.0f, 1.0f, 0.0f, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(prog);
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, W/2, H/2, rgba);
        CHECK_MSG(rgba[1] > 200, "green uniform did not reach the shader (got %d)", rgba[1]);
        CHECK(rgba[0] < 60);
        free(px);
    }

    glUseProgram(0);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(program_uniform, two_programs_do_not_share_a_location)
{
    char log[2048] = { 0 };
    GLuint a = mgl_build_program(VS_QUAD, FS_UNIFORM_COLOUR, log, sizeof log);
    GLuint b = mgl_build_program(VS_QUAD, FS_UNIFORM_COLOUR, log, sizeof log);
    GLfloat va[4] = { 0 }, vb[4] = { 0 };
    GLint la, lb;

    CHECK(a != 0 && b != 0);
    if (!a || !b) return;

    la = glGetUniformLocation(a, "u_col");
    lb = glGetUniformLocation(b, "u_col");

    glProgramUniform4f(a, la, 1.0f, 0.0f, 0.0f, 1.0f);
    glProgramUniform4f(b, lb, 0.0f, 0.0f, 1.0f, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetUniformfv(a, la, va);
    glGetUniformfv(b, lb, vb);

    // both programs use the same location; the values must stay apart
    CHECK_NEAR(va[0], 1.0f, 0.01f);
    CHECK_NEAR(va[2], 0.0f, 0.01f);
    CHECK_NEAR(vb[0], 0.0f, 0.01f);
    CHECK_NEAR(vb[2], 1.0f, 0.01f);

    glDeleteProgram(a);
    glDeleteProgram(b);
}

GPU_TEST(program_uniform, minus_one_location_is_silently_ignored)
{
    char log[2048] = { 0 };
    GLuint p = mgl_build_program(VS_QUAD, FS_UNIFORM_COLOUR, log, sizeof log);

    CHECK(p != 0);
    if (!p) return;

    glProgramUniform1f(p, -1, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
}

GPU_TEST(program_uniform, rejects_bad_program_name)
{
    glProgramUniform1f(999999, 0, 1.0f);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);
}

GPU_TEST(program_uniform, rejects_negative_count)
{
    char log[2048] = { 0 };
    GLuint p = mgl_build_program(VS_QUAD, FS_UNIFORM_COLOUR, log, sizeof log);
    GLfloat v[4] = { 0, 0, 0, 1 };

    CHECK(p != 0);
    if (!p) return;

    glProgramUniform4fv(p, 0, -1, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

/* ---------- indexed viewport / scissor / depth range ---------- */

GPU_TEST(viewport_array, index_zero_matches_plain_viewport)
{
    GLfloat v[4] = { 0 };
    GLint iv[4] = { 0 };

    glViewport(2, 3, 20, 21);
    glGetFloati_v(GL_VIEWPORT, 0, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(v[0], 2.0f,  0.01f);
    CHECK_NEAR(v[3], 21.0f, 0.01f);

    glViewportIndexedf(0, 5, 6, 30, 31);
    glGetIntegerv(GL_VIEWPORT, iv);
    CHECK_EQ_INT(iv[0], 5);
    CHECK_EQ_INT(iv[2], 30);
}

GPU_TEST(viewport_array, arrayv_writes_a_range)
{
    GLfloat in[8] = { 1, 2, 3, 4,  5, 6, 7, 8 };
    GLfloat out[4] = { 0 };

    glViewportArrayv(0, 2, in);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFloati_v(GL_VIEWPORT, 1, out);
    CHECK_NEAR(out[0], 5.0f, 0.01f);
    CHECK_NEAR(out[3], 8.0f, 0.01f);
}

GPU_TEST(viewport_array, scissor_index_zero_matches_plain_scissor)
{
    GLint v[4] = { 0 };

    glScissor(4, 5, 10, 11);
    glGetIntegeri_v(GL_SCISSOR_BOX, 0, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v[0], 4);
    CHECK_EQ_INT(v[2], 10);

    glScissorIndexed(0, 7, 8, 12, 13);
    glGetIntegerv(GL_SCISSOR_BOX, v);
    CHECK_EQ_INT(v[0], 7);
    CHECK_EQ_INT(v[3], 13);
}

GPU_TEST(viewport_array, depth_range_indexed_round_trips)
{
    GLdouble d[2] = { 0 };

    glDepthRangeIndexed(1, 0.25, 0.75);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetDoublei_v(GL_DEPTH_RANGE, 1, d);
    CHECK_NEAR((float)d[0], 0.25f, 0.001f);
    CHECK_NEAR((float)d[1], 0.75f, 0.001f);
}

GPU_TEST(viewport_array, depth_range_clamps_to_zero_one)
{
    GLdouble d[2] = { 0 };

    glDepthRangeIndexed(2, -3.0, 4.0);
    glGetDoublei_v(GL_DEPTH_RANGE, 2, d);

    CHECK_NEAR((float)d[0], 0.0f, 0.001f);
    CHECK_NEAR((float)d[1], 1.0f, 0.001f);
}

GPU_TEST(viewport_array, rejects_bad_index_and_count)
{
    GLfloat v[4] = { 0, 0, 1, 1 };

    glViewportIndexedf(9999, 0, 0, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glViewportArrayv(0, -1, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glViewportArrayv(15, 4, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glScissorIndexed(0, 0, 0, -1, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- indexed getters ---------- */

GPU_TEST(indexed_get, boolean_reports_nonzero_as_true)
{
    GLboolean b[4] = { 0 };

    glViewportIndexedf(3, 0, 0, 16, 0);
    glGetBooleani_v(GL_VIEWPORT, 3, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT(b[0], GL_FALSE);   // x is 0
    CHECK_EQ_INT(b[2], GL_TRUE);    // width is 16
    CHECK_EQ_INT(b[3], GL_FALSE);   // height is 0
}

GPU_TEST(indexed_get, current_vertex_attrib_is_readable_by_index)
{
    GLfloat v[4] = { 0 };

    glVertexAttrib4f(5, 0.5f, 0.25f, 0.125f, 1.0f);
    glGetFloati_v(GL_CURRENT_VERTEX_ATTRIB, 5, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(v[0], 0.5f,   0.001f);
    CHECK_NEAR(v[2], 0.125f, 0.001f);
}

GPU_TEST(indexed_get, rejects_bad_target_and_index)
{
    GLfloat v[4] = { 0 };
    GLboolean b[4] = { 0 };

    glGetFloati_v(0x9999, 0, v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetBooleani_v(GL_VIEWPORT, 9999, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(indexed_get, null_pointer_is_ignored)
{
    glGetFloati_v(GL_VIEWPORT, 0, NULL);
    glGetBooleani_v(GL_VIEWPORT, 0, NULL);
    glGetDoublei_v(GL_DEPTH_RANGE, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- packed constant attributes ---------- */

GPU_TEST(attrib_const, packed_unsigned_2101010_normalizes)
{
    // x=1023, y=0, z=511, w=3 packed into 2/10/10/10 rev
    GLuint packed = (1023u) | (0u << 10) | (511u << 20) | (3u << 30);
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttribP4ui(2, GL_UNSIGNED_INT_2_10_10_10_REV, GL_TRUE, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribfv(2, GL_CURRENT_VERTEX_ATTRIB, v);
    CHECK_NEAR(v[0], 1.0f, 0.001f);
    CHECK_NEAR(v[1], 0.0f, 0.001f);
    CHECK_NEAR(v[2], 511.0f/1023.0f, 0.01f);
    CHECK_NEAR(v[3], 1.0f, 0.001f);
}

GPU_TEST(attrib_const, packed_unsigned_2101010_unnormalized)
{
    GLuint packed = (700u) | (0u << 10) | (0u << 20) | (2u << 30);
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttribP4ui(3, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, packed);
    glGetVertexAttribfv(3, GL_CURRENT_VERTEX_ATTRIB, v);

    CHECK_NEAR(v[0], 700.0f, 0.001f);
    CHECK_NEAR(v[3], 2.0f, 0.001f);
}

GPU_TEST(attrib_const, packed_signed_2101010_sign_extends)
{
    // 0x3FF in a 10 bit signed field is -1; 0x3 in the 2 bit field is -1
    GLuint packed = (0x3FFu) | (0x001u << 10) | (0x200u << 20) | (0x3u << 30);
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttribP4ui(4, GL_INT_2_10_10_10_REV, GL_FALSE, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribfv(4, GL_CURRENT_VERTEX_ATTRIB, v);
    CHECK_NEAR(v[0], -1.0f,   0.001f);   // 0x3FF -> -1
    CHECK_NEAR(v[1],  1.0f,   0.001f);   // 0x001 ->  1
    CHECK_NEAR(v[2], -512.0f, 0.001f);   // 0x200 -> -512, the most negative
    CHECK_NEAR(v[3], -1.0f,   0.001f);   // 0x3   -> -1
}

GPU_TEST(attrib_const, packed_signed_normalized_clamps_at_minus_one)
{
    // -512/511 is below -1 and must clamp
    GLuint packed = (0x200u) | (0x200u << 10) | (0x200u << 20) | (0x2u << 30);
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttribP4ui(5, GL_INT_2_10_10_10_REV, GL_TRUE, packed);
    glGetVertexAttribfv(5, GL_CURRENT_VERTEX_ATTRIB, v);

    CHECK_NEAR(v[0], -1.0f, 0.001f);
    CHECK_NEAR(v[3], -1.0f, 0.001f);     // 2 bit -2 clamps too
}

GPU_TEST(attrib_const, packed_float_11_11_10_decodes)
{
    // 1.0 as an 11 bit float is exponent bias 15, mantissa 0 -> 15 << 6
    GLuint one11 = 15u << 6;
    GLuint one10 = 15u << 5;
    GLuint packed = one11 | (one11 << 11) | (one10 << 22);
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttribP4ui(6, GL_UNSIGNED_INT_10F_11F_11F_REV, GL_FALSE, packed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetVertexAttribfv(6, GL_CURRENT_VERTEX_ATTRIB, v);
    CHECK_NEAR(v[0], 1.0f, 0.001f);
    CHECK_NEAR(v[1], 1.0f, 0.001f);
    CHECK_NEAR(v[2], 1.0f, 0.001f);
    CHECK_NEAR(v[3], 1.0f, 0.001f);      // no alpha in this format, defaults to 1
}

GPU_TEST(attrib_const, packed_rejects_bad_type)
{
    glVertexAttribP4ui(2, GL_FLOAT, GL_FALSE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glVertexAttribP4uiv(2, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(attrib_const, packed_partial_forms_pad)
{
    GLuint packed = (511u) | (511u << 10) | (511u << 20) | (1u << 30);
    GLfloat v[4] = { 9, 9, 9, 9 };

    glVertexAttribP2ui(7, GL_UNSIGNED_INT_2_10_10_10_REV, GL_FALSE, packed);
    glGetVertexAttribfv(7, GL_CURRENT_VERTEX_ATTRIB, v);

    CHECK_NEAR(v[0], 511.0f, 0.001f);
    CHECK_NEAR(v[1], 511.0f, 0.001f);
    CHECK_NEAR(v[2], 0.0f,   0.001f);    // z not written, defaults to 0
    CHECK_NEAR(v[3], 1.0f,   0.001f);    // w not written, defaults to 1
}

GPU_TEST(attrib_const, integer_type_survives_a_float_readback)
{
    GLfloat f[4] = { 0 };

    glVertexAttribI4i(9, -3, 4, -5, 6);
    glGetVertexAttribfv(9, GL_CURRENT_VERTEX_ATTRIB, f);

    CHECK_NEAR(f[0], -3.0f, 0.001f);
    CHECK_NEAR(f[2], -5.0f, 0.001f);
}
