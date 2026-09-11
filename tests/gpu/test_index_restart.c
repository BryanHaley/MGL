/*
 * test_index_restart.c
 * MGL
 *
 * Index types and primitive restart, verified by reading pixels back.
 * Metal has no 8-bit index type and only knows the fixed restart index,
 * so GL_UNSIGNED_BYTE and glPrimitiveRestartIndex both go through a
 * conversion pass; these tests are what that pass has to satisfy.
 */

#include "mgl_test.h"
#include "harness.h"

#define W 64
#define H 32

static const char *VS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(0.0, 1.0, 0.0, 1.0); }\n";

/* two quads, left and right, with a gap in the middle of the target */
static const float QUADS[] = {
    -0.9f, -0.8f,   -0.1f, -0.8f,   -0.1f, 0.8f,   -0.9f, 0.8f,   /* 0..3 left  */
     0.1f, -0.8f,    0.9f, -0.8f,    0.9f, 0.8f,    0.1f, 0.8f,   /* 4..7 right */
};

typedef struct {
    MGLTestTarget t;
    GLuint prog, vao, vbo, ebo;
} Scene;

static int scene_begin(Scene *s, const void *indices, size_t bytes)
{
    char log[2048] = { 0 };

    memset(s, 0, sizeof *s);

    if (!mgl_target_create(&s->t, W, H, GL_RGBA8, 0))
        return 0;

    s->prog = mgl_build_program(VS, FS, log, sizeof log);
    if (!s->prog) { mgl_target_destroy(&s->t); return 0; }

    glGenVertexArrays(1, &s->vao);
    glBindVertexArray(s->vao);

    glGenBuffers(1, &s->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof QUADS, QUADS, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glGenBuffers(1, &s->ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bytes, indices, GL_STATIC_DRAW);

    mgl_target_bind(&s->t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(s->prog);

    return 1;
}

/* left quad drawn, gap empty, right quad drawn */
static void scene_check(Scene *s, int want_left, int want_gap, int want_right)
{
    unsigned char *px = mgl_read_rgba8(&s->t);
    unsigned char l[4], g[4], r[4];

    if (!px) { mgl_test_fail(__FILE__, __LINE__, "readback failed"); return; }

    mgl_pixel_at(px, &s->t, W / 4, H / 2, l);
    mgl_pixel_at(px, &s->t, W / 2, H / 2, g);
    mgl_pixel_at(px, &s->t, 3 * W / 4, H / 2, r);

    CHECK_MSG((l[1] > 200) == want_left,  "left quad pixel = %d, want %s", l[1], want_left ? "green" : "black");
    CHECK_MSG((g[1] > 200) == want_gap,   "gap pixel = %d, want %s", g[1], want_gap ? "green" : "black");
    CHECK_MSG((r[1] > 200) == want_right, "right quad pixel = %d, want %s", r[1], want_right ? "green" : "black");

    free(px);
}

static void scene_end(Scene *s)
{
    glDisable(GL_PRIMITIVE_RESTART);
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDeleteProgram(s->prog);
    glDeleteVertexArrays(1, &s->vao);
    glDeleteBuffers(1, &s->vbo);
    glDeleteBuffers(1, &s->ebo);
    mgl_target_destroy(&s->t);
}

/* ---------- GL_UNSIGNED_BYTE indices ---------- */

GPU_TEST(index_restart, ubyte_indices_draw_triangles)
{
    static const GLubyte idx[] = { 0, 1, 2,  0, 2, 3,   4, 5, 6,  4, 6, 7 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_BYTE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, ubyte_indices_honour_byte_offset)
{
    /* the first three bytes are junk the draw must skip */
    static const GLubyte idx[] = { 7, 7, 7,   4, 5, 6,  4, 6, 7 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, (const void *)3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 0, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, ubyte_indices_with_fixed_restart_strip)
{
    /* two strips split by 0xFF; without the split the middle would fill */
    static const GLubyte idx[] = { 0, 1, 3, 2,  0xFF,  4, 5, 7, 6 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(GL_TRIANGLE_STRIP, 9, GL_UNSIGNED_BYTE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, ubyte_indices_instanced_base_vertex)
{
    /* indices name the left quad; base vertex 4 moves the draw to the right */
    static const GLubyte idx[] = { 0, 1, 2,  0, 2, 3 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glDrawElementsInstancedBaseVertex(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0, 1, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 0, 0, 1);
    scene_end(&s);
}

/* ---------- restart on strips (Metal only knows the fixed index) ---------- */

GPU_TEST(index_restart, strip_restart_fixed_index_ushort)
{
    static const GLushort idx[] = { 0, 1, 3, 2,  0xFFFF,  4, 5, 7, 6 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(GL_TRIANGLE_STRIP, 9, GL_UNSIGNED_SHORT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, strip_restart_custom_index_ushort)
{
    /* an application-chosen restart value, not Metal's */
    static const GLushort idx[] = { 0, 1, 3, 2,  1234,  4, 5, 7, 6 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(1234);
    glDrawElements(GL_TRIANGLE_STRIP, 9, GL_UNSIGNED_SHORT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, strip_restart_custom_index_uint)
{
    static const GLuint idx[] = { 0, 1, 3, 2,  70000,  4, 5, 7, 6 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(70000);
    glDrawElements(GL_TRIANGLE_STRIP, 9, GL_UNSIGNED_INT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, strip_without_restart_bridges_the_gap)
{
    /* the same strip with restart off is one strip and fills the middle */
    static const GLushort idx[] = { 0, 1, 3, 2,  4, 5, 7, 6 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glDrawElements(GL_TRIANGLE_STRIP, 8, GL_UNSIGNED_SHORT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 1, 1);
    scene_end(&s);
}

/* ---------- restart on fans and loops (expanded on the CPU) ---------- */

GPU_TEST(index_restart, fan_restart_custom_index)
{
    static const GLushort idx[] = { 0, 1, 2, 3,  99,  4, 5, 6, 7 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(99);
    glDrawElements(GL_TRIANGLE_FAN, 9, GL_UNSIGNED_SHORT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, fan_restart_fixed_index_ubyte)
{
    static const GLubyte idx[] = { 0, 1, 2, 3,  0xFF,  4, 5, 6, 7 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDrawElements(GL_TRIANGLE_FAN, 9, GL_UNSIGNED_BYTE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, fan_consecutive_restarts_and_short_runs)
{
    /* two sentinels in a row and a two-index run must not derail the fan */
    static const GLushort idx[] = { 0, 1, 2, 3,  99, 99,  4, 5,  99,  4, 5, 6, 7 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(99);
    glDrawElements(GL_TRIANGLE_FAN, 13, GL_UNSIGNED_SHORT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 1, 0, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, fan_without_restart_bridges_the_gap)
{
    static const GLushort idx[] = { 0, 1, 2, 3,  4, 5, 6, 7 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glDrawElements(GL_TRIANGLE_FAN, 8, GL_UNSIGNED_SHORT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* fan from vertex 0 sweeps across the gap to reach the right quad */
    scene_check(&s, 1, 1, 1);
    scene_end(&s);
}

GPU_TEST(index_restart, fan_too_short_draws_nothing_without_error)
{
    static const GLushort idx[] = { 0, 1 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glDrawElements(GL_TRIANGLE_FAN, 2, GL_UNSIGNED_SHORT, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 0, 0, 0);
    scene_end(&s);
}

GPU_TEST(index_restart, restart_does_not_apply_to_draw_arrays)
{
    /* vertex 5 exists; restart index 5 must not cut a DrawArrays fan */
    static const GLushort idx[] = { 0 };
    Scene s;

    if (!scene_begin(&s, idx, sizeof idx)) SKIP("no target or pipeline");

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(5);
    glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    scene_check(&s, 0, 0, 1);
    scene_end(&s);
}
