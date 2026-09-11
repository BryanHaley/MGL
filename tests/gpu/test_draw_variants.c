/*
 * test_draw_variants.c
 * MGL
 *
 * Instanced, base-vertex, base-instance, multi-draw and indirect draws.
 */

#include "mgl_test.h"
#include "harness.h"

static const char *VS_TRI =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_RED =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(1.0, 0.0, 0.0, 1.0); }\n";

/* ---------- helpers ---------- */

static GLuint make_vao_with_ibo(void)
{
    GLuint vao, vbo, ibo;
    const float verts[] = { -1, -1, 3, -1, -1, 3 };
    const unsigned short idx[] = { 0, 1, 2 };

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);

    return vao;
}

static void make_indirect_buffer(GLuint *out_buf)
{
    glGenBuffers(1, out_buf);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, *out_buf);
    {
        const GLuint cmd[] = { 3, 1, 0, 0 };
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof cmd, cmd, GL_STATIC_DRAW);
    }
}

/* ---------- base-instance family ----------
 *
 * glDrawArraysInstancedBaseInstance, glDrawElementsInstancedBaseInstance,
 * glDrawElementsInstancedBaseVertexBaseInstance.
 * The spec says count and instancecount must not be negative.
 * count == 0 and instancecount == 0 are valid (they draw nothing).
 * MGL rejects zero with GL_INVALID_VALUE — that is a bug.
 */

/* A bad mode and a bad index type are both GL_INVALID_ENUM in the spec, not
   GL_INVALID_VALUE. These checks used to assert what MGL happened to do. */

GPU_TEST(draw_variants, base_instance_rejects_bad_mode)
{
    GLuint vao;
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_TRI, FS_RED, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog) return;

    vao = make_vao_with_ibo();
    glBindVertexArray(vao);
    glUseProgram(prog);

    glDrawArraysInstancedBaseInstance(0x9999, 0, 3, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDrawElementsInstancedBaseInstance(0x9999, 3, GL_UNSIGNED_SHORT, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDrawElementsInstancedBaseVertexBaseInstance(0x9999, 3, GL_UNSIGNED_SHORT, NULL, 1, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(draw_variants, base_instance_rejects_negative_count)
{
    GLuint vao;
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_TRI, FS_RED, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog) return;

    vao = make_vao_with_ibo();
    glBindVertexArray(vao);
    glUseProgram(prog);

    /* negative count */
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, -1, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDrawElementsInstancedBaseInstance(GL_TRIANGLES, -1, GL_UNSIGNED_SHORT, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, -1, GL_UNSIGNED_SHORT, NULL, 1, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* negative instancecount */
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 3, -1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDrawElementsInstancedBaseInstance(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL, -1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL, -1, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* count == 0 is valid per spec — MGL rejects it, so this asserts the bug */
    glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 0, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(draw_variants, base_instance_rejects_bad_type)
{
    GLuint vao;
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_TRI, FS_RED, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog) return;

    vao = make_vao_with_ibo();
    glBindVertexArray(vao);
    glUseProgram(prog);

    glDrawElementsInstancedBaseInstance(GL_TRIANGLES, 3, GL_FLOAT, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, 3, 0x9999, NULL, 1, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- glDrawElementsInstanced and glDrawElementsInstancedBaseVertex ---------- */

GPU_TEST(draw_variants, instanced_rejects_bad_arguments)
{
    GLuint vao;
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_TRI, FS_RED, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog) return;

    vao = make_vao_with_ibo();
    glBindVertexArray(vao);
    glUseProgram(prog);

    /* bad mode */
    glDrawElementsInstanced(0x9999, 3, GL_UNSIGNED_SHORT, NULL, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDrawElementsInstancedBaseVertex(0x9999, 3, GL_UNSIGNED_SHORT, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* negative count */
    glDrawElementsInstanced(GL_TRIANGLES, -1, GL_UNSIGNED_SHORT, NULL, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDrawElementsInstancedBaseVertex(GL_TRIANGLES, -1, GL_UNSIGNED_SHORT, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* negative instancecount */
    glDrawElementsInstanced(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDrawElementsInstancedBaseVertex(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, NULL, -1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* bad type */
    glDrawElementsInstanced(GL_TRIANGLES, 3, 0x9999, NULL, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDrawElementsInstancedBaseVertex(GL_TRIANGLES, 3, 0x9999, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- indirect draws ----------
 *
 * glDrawArraysIndirect, glDrawElementsIndirect,
 * glMultiDrawArraysIndirect, glMultiDrawElementsIndirect.
 * All require a buffer bound to GL_DRAW_INDIRECT_BUFFER.
 */

GPU_TEST(draw_variants, indirect_rejects_no_buffer)
{
    GLuint vao;
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_TRI, FS_RED, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog) return;

    vao = make_vao_with_ibo();
    glBindVertexArray(vao);
    glUseProgram(prog);

    glDrawArraysIndirect(GL_TRIANGLES, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
}

GPU_TEST(draw_variants, indirect_rejects_bad_mode)
{
    GLuint indirect_buf;

    make_indirect_buffer(&indirect_buf);

    glDrawArraysIndirect(0x9999, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDrawElementsIndirect(0x9999, GL_UNSIGNED_SHORT, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glMultiDrawArraysIndirect(0x9999, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glMultiDrawElementsIndirect(0x9999, GL_UNSIGNED_SHORT, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glDeleteBuffers(1, &indirect_buf);
}

GPU_TEST(draw_variants, indirect_rejects_bad_drawcount_and_stride)
{
    GLuint indirect_buf;

    make_indirect_buffer(&indirect_buf);

    /* negative drawcount */
    glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, -1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, NULL, -1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* stride not a multiple of 4 */
    glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, 1, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT, NULL, 1, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* drawcount == 0 is valid per spec — MGL rejects it */
    glMultiDrawArraysIndirect(GL_TRIANGLES, NULL, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glDeleteBuffers(1, &indirect_buf);
}

GPU_TEST(draw_variants, indirect_rejects_bad_element_type)
{
    GLuint indirect_buf;

    make_indirect_buffer(&indirect_buf);

    glDrawElementsIndirect(GL_TRIANGLES, GL_FLOAT, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glMultiDrawElementsIndirect(GL_TRIANGLES, 0x9999, NULL, 1, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glDeleteBuffers(1, &indirect_buf);
}

/* ---------- glDrawRangeElementsBaseVertex ---------- */

GPU_TEST(draw_variants, range_base_vertex_rejects_bad_arguments)
{
    GLuint vao;
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_TRI, FS_RED, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog) return;

    vao = make_vao_with_ibo();
    glBindVertexArray(vao);
    glUseProgram(prog);

    /* bad mode */
    glDrawRangeElementsBaseVertex(0x9999, 0, 2, 3, GL_UNSIGNED_SHORT, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* negative count */
    glDrawRangeElementsBaseVertex(GL_TRIANGLES, 0, 2, -1, GL_UNSIGNED_SHORT, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* end < start per spec */
    glDrawRangeElementsBaseVertex(GL_TRIANGLES, 5, 3, 3, GL_UNSIGNED_SHORT, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* end == start is valid per spec — MGL checks end > start (wrong) */
    glDrawRangeElementsBaseVertex(GL_TRIANGLES, 2, 2, 3, GL_UNSIGNED_SHORT, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* bad type */
    glDrawRangeElementsBaseVertex(GL_TRIANGLES, 0, 2, 3, 0x9999, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
}

/* ---------- multi-draw family ----------
 *
 * glMultiDrawArrays, glMultiDrawElements, glMultiDrawElementsBaseVertex.
 * Spec: GL_INVALID_VALUE if drawcount negative (not zero).
 * MGL rejects drawcount == 0 — bug.
 * MGL also does not check drawcount for glMultiDrawArrays — bug.
 */

GPU_TEST(draw_variants, multi_draw_rejects_bad_mode)
{
    GLint first[] = { 0 };
    GLsizei count[] = { 3 };
    const void *indices[] = { NULL };

    glMultiDrawArrays(0x9999, first, count, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glMultiDrawElements(0x9999, count, GL_UNSIGNED_SHORT, indices, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glMultiDrawElementsBaseVertex(0x9999, count, GL_UNSIGNED_SHORT, indices, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(draw_variants, multi_draw_rejects_negative_drawcount)
{
    GLint first[] = { 0 };
    GLsizei count[] = { 3 };
    const void *indices[] = { NULL };

    glMultiDrawArrays(GL_TRIANGLES, first, count, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glMultiDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glMultiDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices, -1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* drawcount == 0 is valid per spec — MGL rejects it for elements variants */
    glMultiDrawArrays(GL_TRIANGLES, first, count, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMultiDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMultiDrawElementsBaseVertex(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(draw_variants, multi_draw_rejects_bad_type)
{
    GLsizei count[] = { 3 };
    const void *indices[] = { NULL };

    glMultiDrawElements(GL_TRIANGLES, count, 0x9999, indices, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glMultiDrawElementsBaseVertex(GL_TRIANGLES, count, 0x9999, indices, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- transform feedback draw ----------
 *
 * glDrawTransformFeedbackStream, glDrawTransformFeedbackStreamInstanced.
 * These validate arguments then report GL_INVALID_OPERATION because Metal
 * has no capture/replay stage.
 */

GPU_TEST(draw_variants, xfb_draw_validates_arguments)
{
    GLuint t = 0;

    glGenTransformFeedbacks(1, &t);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);

    /* bad mode */
    glDrawTransformFeedbackStream(0x9999, t, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDrawTransformFeedbackStreamInstanced(0x9999, t, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* nonexistent transform feedback */
    glDrawTransformFeedbackStream(GL_TRIANGLES, 999123, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDrawTransformFeedbackStreamInstanced(GL_TRIANGLES, 999123, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* negative instancecount */
    glDrawTransformFeedbackStreamInstanced(GL_TRIANGLES, t, 0, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* stream out of range */
    glDrawTransformFeedbackStream(GL_TRIANGLES, t, 999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDrawTransformFeedbackStreamInstanced(GL_TRIANGLES, t, 999, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* well-formed call — cannot run, reports unsupported */
    glDrawTransformFeedbackStream(GL_TRIANGLES, t, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDrawTransformFeedbackStreamInstanced(GL_TRIANGLES, t, 0, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteTransformFeedbacks(1, &t);
}
