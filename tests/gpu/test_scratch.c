/*
 * test_scratch.c
 * MGL
 *
 * Primitive expansion needs a temporary index buffer per draw. It used to grow
 * one buffer and reuse it, which is fine for a single draw in flight and wrong
 * the moment there are several: the next draw resizes the buffer the GPU is
 * still reading from.
 */

#include <stdlib.h>

#include "mgl_test.h"
#include "harness.h"

#define W 64
#define H 64

static const char *VS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS =
    "#version 460 core\n"
    "out vec4 c;\n"
    "void main() { c = vec4(0.0, 1.0, 0.0, 1.0); }\n";

/* a fan covering the target, with `segments` edge vertices */
static GLuint make_fan(GLuint *out_vbo, int segments)
{
    GLfloat *v = malloc((size_t)(segments + 2) * 2 * sizeof(GLfloat));
    GLuint vao = 0, vbo = 0;

    v[0] = 0.0f; v[1] = 0.0f;
    for (int i = 0; i <= segments; i++)
    {
        float t = 6.2831853f * (float)i / (float)segments;
        v[2 + i * 2 + 0] = 0.9f * cosf(t);
        v[2 + i * 2 + 1] = 0.9f * sinf(t);
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(segments + 2) * 2 * sizeof(GLfloat), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);

    free(v);
    *out_vbo = vbo;

    return vao;
}

/* many expanded draws of different sizes in one frame, all before a flush */
GPU_TEST(scratch, many_expanded_draws_in_one_frame)
{
    MGLTestTarget t;
    char log[2048] = { 0 };

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader pipeline unavailable"); }

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);

    /* sizes chosen to cross the allocator's buckets both ways */
    const int sizes[] = { 3, 64, 2000, 8, 5000, 16, 300, 4 };
    GLuint vaos[8] = { 0 }, vbos[8] = { 0 };

    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++)
    {
        vaos[i] = make_fan(&vbos[i], sizes[i]);
        glBindVertexArray(vaos[i]);
        glDrawArrays(GL_TRIANGLE_FAN, 0, sizes[i] + 2);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    unsigned char *px = mgl_read_rgba8(&t);
    if (px)
    {
        unsigned char c[4];
        mgl_pixel_at(px, &t, W / 2, H / 2, c);
        CHECK_MSG(c[1] > 200 && c[0] < 60, "centre = %d,%d,%d after 8 fans", c[0], c[1], c[2]);
        free(px);
    }

    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++)
    {
        glDeleteVertexArrays(1, &vaos[i]);
        glDeleteBuffers(1, &vbos[i]);
    }

    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* the same draw repeated across many frames must not leak allocations */
GPU_TEST(scratch, repeated_frames_do_not_grow_without_bound)
{
    MGLTestTarget t;
    char log[2048] = { 0 };

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader pipeline unavailable"); }

    GLuint vbo = 0;
    GLuint vao = make_fan(&vbo, 512);

    for (int frame = 0; frame < 32; frame++)
    {
        mgl_target_bind(&t);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 514);
        glFinish();
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    unsigned char *px = mgl_read_rgba8(&t);
    if (px)
    {
        unsigned char c[4];
        mgl_pixel_at(px, &t, W / 2, H / 2, c);
        CHECK_MSG(c[1] > 200, "centre = %d,%d,%d after 32 frames", c[0], c[1], c[2]);
        free(px);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
