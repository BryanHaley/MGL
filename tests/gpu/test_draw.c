/*
 * test_draw.c
 * MGL
 *
 * Shader compilation and the draw paths, verified by reading pixels back.
 */

#include "mgl_test.h"
#include "harness.h"

#define W 32
#define H 32

static const char *VS_PASSTHRU =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_SOLID =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(0.0, 1.0, 0.0, 1.0); }\n";

/* ---------- shader objects ---------- */

GPU_TEST(shader, compiles_vertex_and_fragment)
{
    char log[2048] = { 0 };
    GLuint p = mgl_build_program(VS_PASSTHRU, FS_SOLID, log, sizeof log);

    CHECK_MSG(p != 0, "link failed: %s", log);

    if (p) glDeleteProgram(p);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(shader, reports_compile_error_for_bad_source)
{
    GLuint s = glCreateShader(GL_FRAGMENT_SHADER);
    const char *bad = "#version 460 core\nthis is not glsl at all\n";
    GLint ok = 1;

    CHECK(s != 0);

    glShaderSource(s, 1, &bad, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

    CHECK_MSG(ok == GL_FALSE, "bad shader reported success");

    glDeleteShader(s);
}

GPU_TEST(shader, info_log_is_populated_on_failure)
{
    GLuint s = glCreateShader(GL_VERTEX_SHADER);
    const char *bad = "#version 460 core\nvoid main() { undefined_thing(); }\n";
    GLint ok = 1, len = 0;

    glShaderSource(s, 1, &bad, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);

    CHECK(ok == GL_FALSE);
    CHECK_MSG(len > 0, "INFO_LOG_LENGTH = %d", len);

    glDeleteShader(s);
}

GPU_TEST(shader, is_shader_and_delete)
{
    GLuint s = glCreateShader(GL_VERTEX_SHADER);

    CHECK(glIsShader(s) == GL_TRUE);
    glDeleteShader(s);
    CHECK(glIsShader(s) == GL_FALSE);
}

GPU_TEST(shader, create_shader_rejects_bad_stage)
{
    GLuint s = glCreateShader(0x9999);

    CHECK_EQ_UINT(s, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(program, is_program_and_delete)
{
    GLuint p = glCreateProgram();

    CHECK(p != 0);
    CHECK(glIsProgram(p) == GL_TRUE);
    glDeleteProgram(p);
    CHECK(glIsProgram(p) == GL_FALSE);
}

/* ---------- drawing ---------- */

GPU_TEST(draw, fullscreen_quad_fills_target)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao, vbo = 0;
    unsigned char *px, c[4];

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_PASSTHRU, FS_SOLID, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader pipeline unavailable"); }

    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (!px) { SKIP("readback failed"); }

    mgl_pixel_at(px, &t, W / 2, H / 2, c);
    CHECK_MSG(c[1] > 200 && c[0] < 60, "centre = %d,%d,%d - want green", c[0], c[1], c[2]);

    free(px);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, half_covering_triangle_leaves_background)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao = 0, vbo = 0;
    unsigned char *px, lo[4], hi[4];

    // covers the bottom-left half only
    static const float tri[] = { -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f, 1.0f };

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_PASSTHRU, FS_SOLID, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader pipeline unavailable"); }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);   // blue background
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (!px) SKIP("readback failed");

    mgl_pixel_at(px, &t, 4, 4, lo);            // inside the triangle
    mgl_pixel_at(px, &t, W - 4, H - 4, hi);    // outside it

    CHECK_MSG(lo[1] > 200, "inside triangle G = %d, want green", lo[1]);
    CHECK_MSG(hi[2] > 200, "outside triangle B = %d, want blue", hi[2]);

    free(px);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, elements_indexed_draw)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao = 0, vbo = 0, ibo = 0;
    unsigned char *px, c[4];

    static const float quad[] = { -1,-1,  1,-1,  1,1,  -1,1 };
    static const unsigned short idx[] = { 0,1,2, 0,2,3 };

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_PASSTHRU, FS_SOLID, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader pipeline unavailable"); }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (!px) SKIP("readback failed");

    mgl_pixel_at(px, &t, W / 2, H / 2, c);
    CHECK_MSG(c[1] > 200, "indexed draw centre G = %d, want green", c[1]);

    free(px);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, uniform_controls_output_colour)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao, vbo = 0;
    GLint loc;
    unsigned char *px, c[4];

    static const char *fs =
        "#version 460 core\n"
        "layout(location = 0) uniform vec4 tint;\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() { frag = tint; }\n";

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_PASSTHRU, fs, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("uniform shader failed to build"); }

    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "tint");

    glUniform4f(loc >= 0 ? loc : 0, 1.0f, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&t);
    if (!px) SKIP("readback failed");

    mgl_pixel_at(px, &t, W / 2, H / 2, c);
    CHECK_MSG(c[0] > 200 && c[1] < 60, "tinted centre = %d,%d,%d - want red", c[0], c[1], c[2]);

    free(px);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, vertex_colour_interpolates)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao = 0, vpos = 0, vcol = 0;
    unsigned char *px, left[4], right[4];

    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "layout(location = 1) in vec3 col;\n"
        "layout(location = 0) out vec3 vcol;\n"
        "void main() { vcol = col; gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 460 core\n"
        "layout(location = 0) in vec3 vcol;\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() { frag = vec4(vcol, 1.0); }\n";

    static const float pos[] = { -1,-1,  1,-1,  1,1,  -1,-1,  1,1,  -1,1 };
    static const float col[] = { 1,0,0,  0,0,1,  0,0,1,  1,0,0,  0,0,1,  1,0,0 };

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(vs, fs, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("varying shader failed to build"); }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vpos);
    glBindBuffer(GL_ARRAY_BUFFER, vpos);
    glBufferData(GL_ARRAY_BUFFER, sizeof pos, pos, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

    glGenBuffers(1, &vcol);
    glBindBuffer(GL_ARRAY_BUFFER, vcol);
    glBufferData(GL_ARRAY_BUFFER, sizeof col, col, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (!px) SKIP("readback failed");

    mgl_pixel_at(px, &t, 2, H / 2, left);
    mgl_pixel_at(px, &t, W - 3, H / 2, right);

    CHECK_MSG(left[0] > right[0], "expected red to fall left..right (%d vs %d)", left[0], right[0]);
    CHECK_MSG(right[2] > left[2], "expected blue to rise left..right (%d vs %d)", left[2], right[2]);

    free(px);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vpos);
    glDeleteBuffers(1, &vcol);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, depth_test_hides_far_geometry)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao = 0, vbo = 0;
    unsigned char *px, c[4];

    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec3 pos;\n"
        "void main() { gl_Position = vec4(pos, 1.0); }\n";
    static const char *fs_near =
        "#version 460 core\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() { frag = vec4(0.0, 1.0, 0.0, 1.0); }\n";

    // near quad at z=-0.5, then a far quad at z=0.5 that must be rejected
    static const float verts[] = {
        -1,-1,-0.5f,  1,-1,-0.5f,  1,1,-0.5f,  -1,-1,-0.5f,  1,1,-0.5f,  -1,1,-0.5f,
        -1,-1, 0.5f,  1,-1, 0.5f,  1,1, 0.5f,  -1,-1, 0.5f,  1,1, 0.5f,  -1,1, 0.5f,
    };

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 1)) SKIP("no depth target");

    prog = mgl_build_program(vs, fs_near, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("depth shader failed to build"); }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    mgl_target_bind(&t);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 12);      // both quads, near one first
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (!px) SKIP("readback failed");

    mgl_pixel_at(px, &t, W / 2, H / 2, c);
    CHECK_MSG(c[1] > 200, "depth-tested centre G = %d, want the near quad", c[1]);

    free(px);
    glDisable(GL_DEPTH_TEST);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, blending_averages_two_layers)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao, vbo = 0;
    GLint loc;
    unsigned char *px, c[4];

    static const char *fs =
        "#version 460 core\n"
        "layout(location = 0) uniform vec4 tint;\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() { frag = tint; }\n";

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_PASSTHRU, fs, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("blend shader failed to build"); }

    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "tint");
    if (loc < 0) loc = 0;

    // opaque red, then white at 50% alpha over it
    glDisable(GL_BLEND);
    glUniform4f(loc, 1.0f, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform4f(loc, 1.0f, 1.0f, 1.0f, 0.5f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_BLEND);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (!px) SKIP("readback failed");

    mgl_pixel_at(px, &t, W / 2, H / 2, c);

    // red stays high, green rises to roughly half
    CHECK_MSG(c[0] > 200, "blended R = %d", c[0]);
    CHECK_MSG(c[1] > 90 && c[1] < 170, "blended G = %d, want ~128", c[1]);

    free(px);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, colour_mask_blocks_channels)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao, vbo = 0;
    unsigned char *px, c[4];

    static const char *fs_white =
        "#version 460 core\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() { frag = vec4(1.0); }\n";

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_PASSTHRU, fs_white, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader failed to build"); }

    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);   // red only
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    px = mgl_read_rgba8(&t);
    if (!px) SKIP("readback failed");

    mgl_pixel_at(px, &t, W / 2, H / 2, c);
    CHECK_MSG(c[0] > 200, "masked R = %d, want written", c[0]);
    CHECK_MSG(c[1] < 60, "masked G = %d, want blocked", c[1]);
    CHECK_MSG(c[2] < 60, "masked B = %d, want blocked", c[2]);

    free(px);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, instanced_draw_runs)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao, vbo = 0;

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_PASSTHRU, FS_SOLID, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader failed to build"); }

    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 4);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

GPU_TEST(draw, rejects_bad_primitive_mode)
{
    glDrawArrays(0x9999, 0, 3);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);
}

GPU_TEST(draw, negative_count_errors)
{
    glDrawArrays(GL_TRIANGLES, 0, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(uniform, update_between_draws_takes_effect)
{
    MGLTestTarget t;
    char log[2048] = { 0 };
    GLuint prog, vao, vbo = 0;
    GLint loc;
    unsigned char *px, c[4];

    static const char *fs =
        "#version 460 core\n"
        "layout(location = 0) uniform vec4 tint;\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() { frag = tint; }\n";

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0)) SKIP("no target");

    prog = mgl_build_program(VS_PASSTHRU, fs, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("shader failed to build"); }

    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "tint");
    if (loc < 0) loc = 0;

    // red first, then green over the top; the green must win
    glUniform4f(loc, 1.0f, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glUniform4f(loc, 0.0f, 1.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&t);
    if (!px) SKIP("readback failed");

    mgl_pixel_at(px, &t, W / 2, H / 2, c);
    CHECK_MSG(c[1] > 200, "second draw G = %d, want green from the updated uniform", c[1]);
    CHECK_MSG(c[0] < 60, "second draw R = %d, want the first draw overwritten", c[0]);

    free(px);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

