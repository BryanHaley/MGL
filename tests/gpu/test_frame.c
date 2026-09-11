/*
 * test_frame.c
 * MGL
 *
 * Whole-frame tests: one pass writes what the next pass reads.
 *
 * The rest of the suite sets one object up, calls one function and reads the
 * answer back, which is why it passed clean while a real application rendered
 * black. Everything here needs at least two passes to fail, so it covers the
 * class of bug that shape of test cannot reach.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdlib.h>
#include <string.h>

static const char *kVS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "out vec2 uv;\n"
    "void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0.0, 1.0); }\n";

// pass 1: a flat colour, so the expected value is exact
static const char *kFlatFS =
    "#version 460 core\n"
    "out vec4 frag;\n"
    "uniform vec4 tint;\n"
    "void main() { frag = tint; }\n";

// pass 2: read pass 1 and permute the channels, so sampling the wrong thing
// cannot accidentally produce the right answer
static const char *kSwizzleFS =
    "#version 460 core\n"
    "in vec2 uv;\n"
    "layout(binding = 0) uniform sampler2D src;\n"
    "out vec4 frag;\n"
    "void main() { vec4 c = texture(src, uv); frag = vec4(c.b, c.r, c.g, 1.0); }\n";

// depth-only writer for the depth-across-passes test
static const char *kDepthFS =
    "#version 460 core\n"
    "out vec4 frag;\n"
    "void main() { frag = vec4(1.0, 0.0, 0.0, 1.0); }\n";

typedef struct {
    GLuint prog_flat, prog_swizzle, vao, vbo;
} FrameRig;

static int rig_build(FrameRig *r)
{
    char log[512] = { 0 };

    r->prog_flat = mgl_build_program(kVS, kFlatFS, log, sizeof(log));
    if (!r->prog_flat)
        return 0;

    r->prog_swizzle = mgl_build_program(kVS, kSwizzleFS, log, sizeof(log));
    if (!r->prog_swizzle)
        return 0;

    r->vao = mgl_fullscreen_quad(&r->vbo);
    return r->vao != 0;
}

static void rig_destroy(FrameRig *r)
{
    if (r->prog_flat)    glDeleteProgram(r->prog_flat);
    if (r->prog_swizzle) glDeleteProgram(r->prog_swizzle);
    if (r->vao)          glDeleteVertexArrays(1, &r->vao);
    if (r->vbo)          glDeleteBuffers(1, &r->vbo);
}

static void draw_flat(const FrameRig *r, float rr, float gg, float bb)
{
    glUseProgram(r->prog_flat);
    glUniform4f(glGetUniformLocation(r->prog_flat, "tint"), rr, gg, bb, 1.0f);
    glBindVertexArray(r->vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* ---------- two passes, the second reading the first ---------- */

GPU_TEST(frame, second_pass_reads_what_the_first_wrote)
{
    MGLTestTarget a, b;
    FrameRig rig = { 0 };
    unsigned char *px = NULL, rgba[4];

    if (!mgl_target_create(&a, 32, 32, GL_RGBA8, 0))
        SKIP("could not create the first target");
    if (!mgl_target_create(&b, 32, 32, GL_RGBA8, 0)) {
        mgl_target_destroy(&a);
        SKIP("could not create the second target");
    }
    if (!rig_build(&rig)) {
        rig_destroy(&rig); mgl_target_destroy(&a); mgl_target_destroy(&b);
        SKIP("frame programs did not build");
    }

    // pass 1 -> target a, a known non-grey colour
    mgl_target_bind(&a);
    glViewport(0, 0, a.width, a.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_flat(&rig, 0.25f, 0.50f, 0.75f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // pass 2 -> target b, sampling a and rotating rgb -> brg
    mgl_target_bind(&b);
    glViewport(0, 0, b.width, b.height);
    glClearColor(1.0f, 0.0f, 1.0f, 1.0f);   // magenta, so "untouched" is obvious
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(rig.prog_swizzle);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, a.color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindVertexArray(rig.vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&b);
    if (px) {
        mgl_pixel_at(px, &b, 16, 16, rgba);
        // 0.25,0.50,0.75 rotated to b,r,g is 0.75,0.25,0.50
        CHECK_NEAR((float)rgba[0], 191.0f, 2.0f);
        CHECK_NEAR((float)rgba[1],  64.0f, 2.0f);
        CHECK_NEAR((float)rgba[2], 128.0f, 2.0f);
        free(px);
    } else {
        CHECK(0);
    }

    rig_destroy(&rig);
    mgl_target_destroy(&a);
    mgl_target_destroy(&b);
}

/* ---------- the regression this file was written for ---------- */

GPU_TEST(frame, texture_contents_survive_a_parameter_change)
{
    MGLTestTarget a, b;
    FrameRig rig = { 0 };
    unsigned char *px = NULL, rgba[4];

    if (!mgl_target_create(&a, 32, 32, GL_RGBA8, 0))
        SKIP("could not create the first target");
    if (!mgl_target_create(&b, 32, 32, GL_RGBA8, 0)) {
        mgl_target_destroy(&a);
        SKIP("could not create the second target");
    }
    if (!rig_build(&rig)) {
        rig_destroy(&rig); mgl_target_destroy(&a); mgl_target_destroy(&b);
        SKIP("frame programs did not build");
    }

    // write to the texture on the GPU
    mgl_target_bind(&a);
    glViewport(0, 0, a.width, a.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_flat(&rig, 1.0f, 0.0f, 0.0f);      // solid red
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glFinish();

    // Touch only sampler state. This used to drop the Metal texture and
    // rebuild it from the CPU-side copy, which never had the rendered pixels,
    // so everything drawn above was silently lost.
    glBindTexture(GL_TEXTURE_2D, a.color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // now read it back by sampling
    mgl_target_bind(&b);
    glViewport(0, 0, b.width, b.height);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(rig.prog_swizzle);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, a.color);
    glBindVertexArray(rig.vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&b);
    if (px) {
        mgl_pixel_at(px, &b, 16, 16, rgba);
        // red in, rotated b,r,g out, so green should carry it
        CHECK_NEAR((float)rgba[0],   0.0f, 2.0f);
        CHECK_NEAR((float)rgba[1], 255.0f, 2.0f);
        CHECK_NEAR((float)rgba[2],   0.0f, 2.0f);
        free(px);
    } else {
        CHECK(0);
    }

    rig_destroy(&rig);
    mgl_target_destroy(&a);
    mgl_target_destroy(&b);
}

GPU_TEST(frame, copied_pixels_survive_a_parameter_change)
{
    // Modelled on misc_core.copy_image_round_trips_pixels, which passes, with
    // one thing added: a four-value texture parameter set between the GPU-side
    // copy and the read. Only the multi-value setters mark the texture dirty,
    // and a dirty texture used to be rebuilt from the CPU-side copy -- which
    // never held the copied pixels.
    MGLTestTarget t;
    GLuint src = 0, dst = 0, p = 0;
    unsigned char *px = NULL;
    char err[1024] = { 0 };
    const GLfloat border[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "out vec2 uv;\n"
        "void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0, 1); }\n";
    const char *fs =
        "#version 460 core\n"
        "uniform sampler2D u_tex;\n"
        "in vec2 uv;\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() { frag = texture(u_tex, uv); }\n";

    if (!mgl_target_create(&t, 4, 4, GL_RGBA8, 0))
        SKIP("could not create a render target");
    mgl_target_bind(&t);

    glGenTextures(1, &src);
    glBindTexture(GL_TEXTURE_2D, src);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    (const unsigned char[]){ 0x80, 0x40, 0x20, 0xFF });

    glGenTextures(1, &dst);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, 0);

    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 1, 1, 0,
                       dst, GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    p = mgl_build_program(vs, fs, err, sizeof err);
    if (!p) {
        glDeleteTextures(1, &src); glDeleteTextures(1, &dst);
        mgl_target_destroy(&t);
        SKIP("sampler program did not build");
    }

    glUseProgram(p);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // the four-value setter, which is the one that marks the texture dirty.
    // Border colour is pure sampler state -- unlike swizzle it needs no new
    // Metal texture -- so the pixels must survive it.
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUniform1i(glGetUniformLocation(p, "u_tex"), 0);

    mgl_fullscreen_quad(NULL);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    px = mgl_read_rgba8(&t);
    if (px) {
        unsigned char rgba[4];
        mgl_pixel_at(px, &t, 0, 0, rgba);
        CHECK_EQ_UINT(rgba[0], 0x80u);
        CHECK_EQ_UINT(rgba[1], 0x40u);
        CHECK_EQ_UINT(rgba[2], 0x20u);
        free(px);
    } else {
        CHECK(0);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDeleteProgram(p);
    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
    mgl_target_destroy(&t);
}

/* ---------- state that has to survive from one pass into the next ---------- */

GPU_TEST(frame, depth_test_works_across_two_draws)
{
    MGLTestTarget t;
    FrameRig rig = { 0 };
    GLuint prog_depth = 0;
    char log[512] = { 0 };
    unsigned char *px = NULL, rgba[4];

    if (!mgl_target_create(&t, 32, 32, GL_RGBA8, 1))
        SKIP("could not create a depth target");

    if (!rig_build(&rig)) {
        rig_destroy(&rig); mgl_target_destroy(&t);
        SKIP("frame programs did not build");
    }

    prog_depth = mgl_build_program(kVS, kDepthFS, log, sizeof(log));
    if (!prog_depth) {
        rig_destroy(&rig); mgl_target_destroy(&t);
        SKIP("depth program did not build");
    }

    mgl_target_bind(&t);
    glViewport(0, 0, t.width, t.height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // near quad, green, writes depth 0
    draw_flat(&rig, 0.0f, 1.0f, 0.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // the same depth again must not pass GL_LESS, so red never lands
    glUseProgram(prog_depth);
    glBindVertexArray(rig.vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px) {
        mgl_pixel_at(px, &t, 16, 16, rgba);
        CHECK_NEAR((float)rgba[0],   0.0f, 2.0f);   // still green
        CHECK_NEAR((float)rgba[1], 255.0f, 2.0f);
        free(px);
    } else {
        CHECK(0);
    }

    glDisable(GL_DEPTH_TEST);
    glDeleteProgram(prog_depth);
    rig_destroy(&rig);
    mgl_target_destroy(&t);
}

/* ---------- three passes, the shape a deferred renderer actually uses ---------- */

GPU_TEST(frame, three_pass_chain_carries_its_values)
{
    MGLTestTarget a, b, c;
    FrameRig rig = { 0 };
    unsigned char *px = NULL, rgba[4];
    int i;
    MGLTestTarget *chain[3];

    if (!mgl_target_create(&a, 16, 16, GL_RGBA8, 0))
        SKIP("could not create target a");
    if (!mgl_target_create(&b, 16, 16, GL_RGBA8, 0)) {
        mgl_target_destroy(&a); SKIP("could not create target b");
    }
    if (!mgl_target_create(&c, 16, 16, GL_RGBA8, 0)) {
        mgl_target_destroy(&a); mgl_target_destroy(&b);
        SKIP("could not create target c");
    }
    if (!rig_build(&rig)) {
        rig_destroy(&rig);
        mgl_target_destroy(&a); mgl_target_destroy(&b); mgl_target_destroy(&c);
        SKIP("frame programs did not build");
    }

    chain[0] = &a; chain[1] = &b; chain[2] = &c;

    // seed
    mgl_target_bind(&a);
    glViewport(0, 0, a.width, a.height);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_flat(&rig, 1.0f, 0.0f, 0.0f);      // red

    // two more passes, each rotating rgb -> brg, so red -> green -> blue
    for (i = 1; i < 3; i++) {
        mgl_target_bind(chain[i]);
        glViewport(0, 0, chain[i]->width, chain[i]->height);
        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(rig.prog_swizzle);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, chain[i - 1]->color);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindVertexArray(rig.vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    }

    px = mgl_read_rgba8(&c);
    if (px) {
        mgl_pixel_at(px, &c, 8, 8, rgba);
        // red, rotated twice, is blue
        CHECK_NEAR((float)rgba[0],   0.0f, 2.0f);
        CHECK_NEAR((float)rgba[1],   0.0f, 2.0f);
        CHECK_NEAR((float)rgba[2], 255.0f, 2.0f);
        free(px);
    } else {
        CHECK(0);
    }

    rig_destroy(&rig);
    mgl_target_destroy(&a);
    mgl_target_destroy(&b);
    mgl_target_destroy(&c);
}
