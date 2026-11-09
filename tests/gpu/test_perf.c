/*
 * Copyright (C) The MooGL Project
 *
 * test_perf.c
 * MGL
 *
 * Driver work that common GL patterns should not cause, counted with the
 * MGL_STAT_* counters, plus pixel checks that the faster paths draw the same.
 */

#include "mgl_test.h"
#include "harness.h"
#include "MGLContext.h"
#include <stdlib.h>
#include <string.h>

#define W 32
#define H 32

static GLuint stat(GLenum which)
{
    GLuint v = 0;

    MGLget(NULL, which, &v);
    return v;
}

static const char *VS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_GREEN =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static const char *FS_TEX =
    "#version 460 core\n"
    "uniform sampler2D tex;\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = texture(tex, vec2(0.5)); }\n";

static const char *FS_UBO =
    "#version 460 core\n"
    "layout(std140, binding = 0) uniform Colour { vec4 colour; };\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = colour; }\n";

static GLuint solidTexture(const unsigned char rgba[4])
{
    GLuint tex;
    unsigned char px[16];

    for (int i = 0; i < 4; i++)
        memcpy(px + i * 4, rgba, 4);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex;
}

static void pixel(const MGLTestTarget *t, int x, int y, unsigned char out[4])
{
    unsigned char *px = mgl_read_rgba8(t);

    memset(out, 0, 4);
    if (px)
        mgl_pixel_at(px, t, x, y, out);
    free(px);
}

#define IS(c, r, g, b) ((c)[0] / 128 == (r) && (c)[1] / 128 == (g) && (c)[2] / 128 == (b))

/* ---------- 1. sampler uniforms ---------- */

// glUniform1i on a sampler threw away the program's Metal functions, so every
// switch back to the program remade them.
GPU_TEST(perf, setting_a_sampler_uniform_keeps_the_pipeline)
{
    static const unsigned char red[4] = { 255, 0, 0, 255 };
    MGLTestTarget t;
    char log[1024];
    GLuint a, b, vao, vbo, tex;
    GLint loc;

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    a = mgl_build_program(VS, FS_TEX, log, sizeof log);
    b = mgl_build_program(VS, FS_GREEN, log, sizeof log);
    CHECK(a && b);
    vao = mgl_fullscreen_quad(&vbo);
    tex = solidTexture(red);
    loc = glGetUniformLocation(a, "tex");

    mgl_target_bind(&t);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLuint built = 0, rebuilt = 0;

    for (int frame = 0; frame < 4; frame++)
    {
        if (frame == 1)
        {
            built = stat(MGL_STAT_PIPELINES_BUILT);
            rebuilt = stat(MGL_STAT_PROGRAM_REBUILDS);
        }

        glUseProgram(a);
        glUniform1i(loc, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glUseProgram(b);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    CHECK_EQ_UINT(stat(MGL_STAT_PIPELINES_BUILT) - built, 0);
    CHECK_EQ_UINT(stat(MGL_STAT_PROGRAM_REBUILDS) - rebuilt, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(a);
    glDeleteProgram(b);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// Changing only a sampler's unit still changes what the next draw samples.
GPU_TEST(perf, a_sampler_unit_change_reaches_the_next_draw)
{
    static const unsigned char red[4] = { 255, 0, 0, 255 }, blue[4] = { 0, 0, 255, 255 };
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo, tex[2];
    unsigned char c[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    p = mgl_build_program(VS, FS_TEX, log, sizeof log);
    CHECK(p);
    vao = mgl_fullscreen_quad(&vbo);
    tex[0] = solidTexture(red);
    tex[1] = solidTexture(blue);

    mgl_target_bind(&t);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glUseProgram(p);

    glUniform1i(glGetUniformLocation(p, "tex"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    pixel(&t, W / 2, H / 2, c);
    CHECK_MSG(IS(c, 1, 0, 0), "unit 0 gave %d,%d,%d, want red", c[0], c[1], c[2]);

    glUniform1i(glGetUniformLocation(p, "tex"), 1);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    pixel(&t, W / 2, H / 2, c);
    CHECK_MSG(IS(c, 0, 0, 1), "unit 1 gave %d,%d,%d, want blue", c[0], c[1], c[2]);

    glDeleteProgram(p);
    glDeleteTextures(2, tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- 2. buffer updates ---------- */

// A partial update of a large buffer the GPU is still reading waited for the
// GPU to finish. The draw before the update must still see the old value.
GPU_TEST(perf, a_partial_buffer_update_after_a_draw_does_not_wait)
{
    static const float red[4] = { 1, 0, 0, 1 }, green[4] = { 0, 1, 0, 1 };
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo, ubo;
    unsigned char c[4];
    const GLsizeiptr big = 1 << 20;

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    p = mgl_build_program(VS, FS_UBO, log, sizeof log);
    CHECK(p);
    vao = mgl_fullscreen_quad(&vbo);

    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, big, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof red, red);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, ubo, 0, 16);

    mgl_target_bind(&t);
    glBindVertexArray(vao);
    glUseProgram(p);

    GLuint waits = stat(MGL_STAT_GPU_WAITS);

    glViewport(0, 0, W / 2, H);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof green, green);
    glViewport(W / 2, 0, W / 2, H);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // a buffer the GPU only read can be read back without waiting
    float back[4];
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof back, back);

    CHECK_EQ_UINT(stat(MGL_STAT_GPU_WAITS) - waits, 0);

    pixel(&t, W / 4, H / 2, c);
    CHECK_MSG(IS(c, 1, 0, 0), "first draw gave %d,%d,%d, want red", c[0], c[1], c[2]);
    pixel(&t, 3 * W / 4, H / 2, c);
    CHECK_MSG(IS(c, 0, 1, 0), "second draw gave %d,%d,%d, want green", c[0], c[1], c[2]);

    // the updated value reads back
    float now[4] = { 0 };
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glGetBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof now, now);
    CHECK(now[1] == 1.0f && now[0] == 0.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
    glDeleteBuffers(1, &ubo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- 3. deleting objects ---------- */

// Deleting a GL object committed the command buffer, ending the render pass.
GPU_TEST(perf, deleting_objects_keeps_the_render_pass)
{
    static const unsigned char red[4] = { 255, 0, 0, 255 };
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo, tex, spare;
    unsigned char c[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    p = mgl_build_program(VS, FS_TEX, log, sizeof log);
    CHECK(p);
    vao = mgl_fullscreen_quad(&vbo);
    tex = solidTexture(red);

    mgl_target_bind(&t);
    glBindVertexArray(vao);
    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p, "tex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glViewport(0, 0, W / 2, H);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    GLuint passes = stat(MGL_STAT_RENDER_PASSES);

    // the texture the draw above used, and a buffer with storage behind it
    glDeleteTextures(1, &tex);
    glGenBuffers(1, &spare);
    glBindBuffer(GL_ARRAY_BUFFER, spare);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glDeleteBuffers(1, &spare);

    tex = solidTexture(red);
    glViewport(W / 2, 0, W / 2, H);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    CHECK_EQ_UINT(stat(MGL_STAT_RENDER_PASSES) - passes, 0);

    pixel(&t, W / 4, H / 2, c);
    CHECK_MSG(IS(c, 1, 0, 0), "first draw gave %d,%d,%d, want red", c[0], c[1], c[2]);
    pixel(&t, 3 * W / 4, H / 2, c);
    CHECK_MSG(IS(c, 1, 0, 0), "second draw gave %d,%d,%d, want red", c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- 4. passes per frame ---------- */

// glClear ran its own render pass before the draw's pass.
GPU_TEST(perf, clears_join_the_next_draws_pass)
{
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo;
    unsigned char c[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 1)) SKIP("no target");
    p = mgl_build_program(VS, FS_GREEN, log, sizeof log);
    CHECK(p);
    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glBindVertexArray(vao);
    glUseProgram(p);
    glEnable(GL_DEPTH_TEST);

    for (int frame = 0; frame < 2; frame++)
    {
        GLuint passes = stat(MGL_STAT_RENDER_PASSES);

        glViewport(0, 0, W, H);
        glClearColor(0, 0, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glClear(GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, W / 2, H);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        CHECK_EQ_UINT(stat(MGL_STAT_RENDER_PASSES) - passes, 1);
    }

    pixel(&t, W / 4, H / 2, c);
    CHECK_MSG(IS(c, 0, 1, 0), "drawn half %d,%d,%d, want green", c[0], c[1], c[2]);
    pixel(&t, 3 * W / 4, H / 2, c);
    CHECK_MSG(IS(c, 0, 0, 1), "cleared half %d,%d,%d, want blue", c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDisable(GL_DEPTH_TEST);
    glDeleteProgram(p);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// A clear waiting for the next pass still happens when nothing is drawn.
GPU_TEST(perf, a_clear_with_no_draw_after_it_still_happens)
{
    MGLTestTarget t;
    GLuint vao;
    unsigned char c[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    mgl_target_bind(&t);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint passes = stat(MGL_STAT_RENDER_PASSES);

    glFlush();
    CHECK_EQ_UINT(stat(MGL_STAT_RENDER_PASSES) - passes, 1);

    pixel(&t, W / 2, H / 2, c);
    CHECK_MSG(IS(c, 0, 0, 1), "centre %d,%d,%d, want blue", c[0], c[1], c[2]);

    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);
}

/* ---------- 5. redundant state ---------- */

// Setting state to what it already is, or changing state that is not part of
// the Metal pipeline, rebuilt the pipeline descriptor and its cache key.
GPU_TEST(perf, state_outside_the_pipeline_skips_the_pipeline_lookup)
{
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo;

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 1)) SKIP("no target");
    p = mgl_build_program(VS, FS_GREEN, log, sizeof log);
    CHECK(p);
    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glBindVertexArray(vao);
    glUseProgram(p);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    GLuint lookups = stat(MGL_STAT_PIPELINE_LOOKUPS);

    // the same values again
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // new values, none of them in the pipeline
    glViewport(1, 1, W - 2, H - 2);
    glScissor(0, 0, W / 2, H / 2);
    glEnable(GL_SCISSOR_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    CHECK_EQ_UINT(stat(MGL_STAT_PIPELINE_LOOKUPS) - lookups, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glCullFace(GL_BACK);
    glDeleteProgram(p);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// A coherent persistent mapping, once written with glBufferSubData, stayed
// dirty forever, so every draw rebound the stage's whole buffer list.
GPU_TEST(perf, a_coherent_mapped_buffer_is_not_rebound_every_draw)
{
    static const float green[4] = { 0, 1, 0, 1 };
    const GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo, ubo;
    unsigned char c[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    p = mgl_build_program(VS, FS_UBO, log, sizeof log);
    CHECK(p);
    vao = mgl_fullscreen_quad(&vbo);

    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferStorage(GL_UNIFORM_BUFFER, 4096, NULL, flags | GL_DYNAMIC_STORAGE_BIT);
    float *map = glMapBufferRange(GL_UNIFORM_BUFFER, 0, 4096, flags);
    CHECK(map != NULL);
    if (map) memcpy(map, green, sizeof green);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

    mgl_target_bind(&t);
    glBindVertexArray(vao);
    glUseProgram(p);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof green, green);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    GLuint rebinds = stat(MGL_STAT_BUFFER_REBINDS);

    for (int i = 0; i < 4; i++)
        glDrawArrays(GL_TRIANGLES, 0, 6);

    CHECK_EQ_UINT(stat(MGL_STAT_BUFFER_REBINDS) - rebinds, 0);

    pixel(&t, W / 2, H / 2, c);
    CHECK_MSG(IS(c, 0, 1, 0), "centre %d,%d,%d, want green", c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
    glDeleteBuffers(1, &ubo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- 6. primitive restart ---------- */

// With primitive restart on, every GL_TRIANGLES element draw was expanded on
// the CPU, though only a list that holds the restart index needs it.
GPU_TEST(perf, a_triangle_list_without_restart_indices_is_not_expanded)
{
    static const GLushort plain[6] = { 0, 1, 2, 0, 2, 3 };
    static const GLushort restart[7] = { 0, 1, 2, 0xFFFF, 0, 2, 3 };
    static const float corners[8] = { -1, -1, 1, -1, 1, 1, -1, 1 };
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo, ebo[2];
    unsigned char c[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    p = mgl_build_program(VS, FS_GREEN, log, sizeof log);
    CHECK(p);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof corners, corners, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glGenBuffers(2, ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof restart, restart, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof plain, plain, GL_STATIC_DRAW);

    mgl_target_bind(&t);
    glUseProgram(p);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint expanded = stat(MGL_STAT_CPU_EXPANSIONS);

    for (int i = 0; i < 3; i++)
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    CHECK_EQ_UINT(stat(MGL_STAT_CPU_EXPANSIONS) - expanded, 0);
    pixel(&t, W / 4, 3 * H / 4, c);
    CHECK_MSG(IS(c, 0, 1, 0), "plain list %d,%d,%d, want green", c[0], c[1], c[2]);

    // a list with the restart index in it still draws both triangles
    glClear(GL_COLOR_BUFFER_BIT);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[1]);
    glDrawElements(GL_TRIANGLES, 7, GL_UNSIGNED_SHORT, 0);
    pixel(&t, W / 4, 3 * H / 4, c);
    CHECK_MSG(IS(c, 0, 1, 0), "upper-left %d,%d,%d, want green", c[0], c[1], c[2]);
    pixel(&t, 3 * W / 4, H / 4, c);
    CHECK_MSG(IS(c, 0, 1, 0), "lower-right %d,%d,%d, want green", c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    glDeleteProgram(p);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(2, ebo);
    mgl_target_destroy(&t);
}

/* ---------- vertex attribute conversion ---------- */

// An attribute Metal cannot read, like GL_INT read as float, was converted
// from the whole buffer on every draw. It is now converted once per change.
GPU_TEST(perf, a_converted_attribute_is_converted_once_per_change)
{
    static const char *VS_INT =
        "#version 460 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos * 0.25, 0.0, 1.0); }\n";
    static const GLint full[12] = { -4, -4, 4, -4, 4, 4, -4, -4, 4, 4, -4, 4 };
    static const GLint half[12] = { -2, -2, 2, -2, 2, 2, -2, -2, 2, 2, -2, 2 };
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo;
    unsigned char c[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    p = mgl_build_program(VS_INT, FS_GREEN, log, sizeof log);
    CHECK(p);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof full, full, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_INT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    mgl_target_bind(&t);
    glUseProgram(p);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint conversions = stat(MGL_STAT_ATTRIB_CONVERSIONS);

    for (int i = 0; i < 4; i++)
        glDrawArrays(GL_TRIANGLES, 0, 6);

    CHECK_EQ_UINT(stat(MGL_STAT_ATTRIB_CONVERSIONS) - conversions, 1);
    pixel(&t, 1, 1, c);
    CHECK_MSG(IS(c, 0, 1, 0), "corner %d,%d,%d, want green", c[0], c[1], c[2]);

    // new contents are converted again
    glClear(GL_COLOR_BUFFER_BIT);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof half, half);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    pixel(&t, 1, 1, c);
    CHECK_MSG(IS(c, 0, 0, 0), "corner %d,%d,%d, want black after shrinking", c[0], c[1], c[2]);
    pixel(&t, W / 2, H / 2, c);
    CHECK_MSG(IS(c, 0, 1, 0), "centre %d,%d,%d, want green", c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// Normalised GL_INT_2_10_10_10_REV, how SuperTuxKart sends its normals, was
// converted on the CPU though Metal reads it as it is.
GPU_TEST(perf, normalised_signed_10_10_10_2_needs_no_conversion)
{
    static const char *VS_PACKED =
        "#version 460 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "layout(location = 1) in vec4 n;\n"
        "out vec4 colour;\n"
        "void main() { colour = n * 0.5 + 0.5; gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *FS_COLOUR =
        "#version 460 core\n"
        "in vec4 colour;\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() { frag = vec4(colour.xyz, 1.0); }\n";
    static const float corners[12] = { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };
    // x = 1, y = -1, z = 0, w = 1
    const GLuint packed = (511u) | ((GLuint)(-511 & 0x3FF) << 10) | (0u << 20) | (1u << 30);
    GLuint normals[6];
    MGLTestTarget t;
    char log[1024];
    GLuint p, vao, vbo[2];
    unsigned char c[4];

    for (int i = 0; i < 6; i++)
        normals[i] = packed;

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");
    p = mgl_build_program(VS_PACKED, FS_COLOUR, log, sizeof log);
    CHECK(p);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(2, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof corners, corners, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof normals, normals, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 4, GL_INT_2_10_10_10_REV, GL_TRUE, 0, 0);
    glEnableVertexAttribArray(1);

    mgl_target_bind(&t);
    glUseProgram(p);

    GLuint conversions = stat(MGL_STAT_ATTRIB_CONVERSIONS);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(stat(MGL_STAT_ATTRIB_CONVERSIONS) - conversions, 0);

    pixel(&t, W / 2, H / 2, c);
    CHECK_MSG(c[0] > 250 && c[1] < 5 && c[2] > 120 && c[2] < 135, "colour %d,%d,%d, want 255,0,128",
              c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(2, vbo);
    mgl_target_destroy(&t);
}
