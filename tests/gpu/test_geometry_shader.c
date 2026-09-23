/*
 * test_geometry_shader.c
 * Copyright (C) The MooGL Project
 *
 * Geometry shaders. Metal has no geometry stage, so MGL rewrites the shader
 * into a compute kernel with a generated vertex shader in front of the
 * rasteriser -- which means the tests here have to check that vertices come
 * out, not that the program linked.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdlib.h>
#include <string.h>

static GLuint compileOne(GLenum stage, const char *src, char *log, int log_size)
{
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (!ok && log && log_size)
        glGetShaderInfoLog(sh, log_size, NULL, log);

    return ok ? sh : 0;
}

static GLuint linkVGF(const char *vs, const char *gs, const char *fs,
                      char *log, int log_size)
{
    GLuint prog = glCreateProgram();
    GLuint v, g, f;
    GLint ok = 0;

    if (log && log_size)
        log[0] = 0;

    v = compileOne(GL_VERTEX_SHADER, vs, log, log_size);
    g = compileOne(GL_GEOMETRY_SHADER, gs, log, log_size);
    f = compileOne(GL_FRAGMENT_SHADER, fs, log, log_size);

    if (!v || !g || !f)
        return 0;

    glAttachShader(prog, v);
    glAttachShader(prog, g);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok && log && log_size)
        glGetProgramInfoLog(prog, log_size, NULL, log);

    return ok ? prog : 0;
}

static GLuint pointsVAO(const GLfloat *pts, size_t bytes, GLuint *vbo_out)
{
    GLuint vao, vbo;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, pts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    if (vbo_out)
        *vbo_out = vbo;

    return vao;
}

static int countWhere(const MGLTestTarget *t, int r_min, int r_max, int g_min, int g_max)
{
    unsigned char *px = mgl_read_rgba8(t);
    int n = 0;

    if (px == NULL)
        return -1;

    for (int i = 0; i < t->width * t->height; i++)
        if (px[i * 4] >= r_min && px[i * 4] <= r_max &&
            px[i * 4 + 1] >= g_min && px[i * 4 + 1] <= g_max)
            n++;

    free(px);

    return n;
}

/* ---------- one point in, one quad out ---------- */

// Nothing but a geometry stage turns two vertices into two filled squares,
// so the pixel count is the whole test.
GPU_TEST(geometry_shader, point_becomes_a_quad)
{
    static const GLfloat pts[4] = { 0.0f, 0.0f, 0.5f, 0.5f };
    static const char *vs =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "layout(location = 0) out vec4 vcol;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); vcol = vec4(0,1,0,1); }\n";
    static const char *gs =
        "#version 410\n"
        "layout(points) in;\n"
        "layout(triangle_strip, max_vertices = 4) out;\n"
        "layout(location = 0) in vec4 vcol[];\n"
        "layout(location = 0) out vec4 gcol;\n"
        "void main() {\n"
        "    vec4 c = gl_in[0].gl_Position;\n"
        "    for (int i = 0; i < 4; i++) {\n"
        "        gl_Position = c + vec4(float(i & 1) * 0.5 - 0.25,\n"
        "                               float(i >> 1) * 0.5 - 0.25, 0.0, 0.0);\n"
        "        gcol = vcol[0];\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 410\n"
        "layout(location = 0) in vec4 gcol;\n"
        "out vec4 o;\n"
        "void main() { o = gcol; }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    char log[2048];
    int green;
    unsigned char *px, c[4];

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "geometry program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = pointsVAO(pts, sizeof pts, &vbo);
    glViewport(0, 0, 64, 64);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_POINTS, 0, 2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // each quad is a quarter of the target's width and height
    green = countWhere(&t, 0, 60, 200, 255);
    CHECK_MSG(green > 400 && green < 700,
              "two quads should be about 512 pixels, counted %d", green);

    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 32, 32, c);
        CHECK_MSG(c[1] > 200, "the first quad is missing from the centre");
        mgl_pixel_at(px, &t, 48, 48, c);
        CHECK_MSG(c[1] > 200, "the second quad is missing");
        mgl_pixel_at(px, &t, 2, 60, c);
        CHECK_MSG(c[0] > 200 && c[1] < 60, "the corner should still be the clear colour");
        free(px);
    }

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- a geometry shader that passes triangles straight through ---------- */

GPU_TEST(geometry_shader, triangle_passthrough_covers_the_target)
{
    static const GLfloat quad[12] = { -1,-1, 1,-1, -1,1,  1,-1, 1,1, -1,1 };
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "void main() {\n"
        "    for (int i = 0; i < 3; i++) {\n"
        "        gl_Position = gl_in[i].gl_Position;\n"
        "        EmitVertex();\n"
        "    }\n"
        "    EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0,1,0,1); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    char log[2048];
    int green;

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "passthrough geometry program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = pointsVAO(quad, sizeof quad, &vbo);
    glViewport(0, 0, 64, 64);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    green = countWhere(&t, 0, 60, 200, 255);
    CHECK_MSG(green == 64 * 64, "passthrough covered %d of 4096 pixels", green);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- a shader that emits nothing draws nothing ---------- */

GPU_TEST(geometry_shader, emitting_nothing_leaves_the_target_alone)
{
    static const GLfloat quad[12] = { -1,-1, 1,-1, -1,1,  1,-1, 1,1, -1,1 };
    static const char *vs =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 410\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "void main() { }\n";
    static const char *fs =
        "#version 410\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0,1,0,1); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    char log[2048];
    int green;

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "empty geometry program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = pointsVAO(quad, sizeof quad, &vbo);
    glViewport(0, 0, 64, 64);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // the slots the shader never wrote must not become stray primitives
    green = countWhere(&t, 0, 60, 200, 255);
    CHECK_MSG(green == 0, "a shader that emits nothing drew %d pixels", green);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- the limits a geometry shader is written against ---------- */

GPU_TEST(geometry_shader, limits_meet_the_floor)
{
    static const struct { const char *name; GLenum e; GLint floor_value; } req[] = {
        { "GL_MAX_GEOMETRY_OUTPUT_VERTICES",         0x8DE0, 256 },
        { "GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS", 0x8DE1, 1024 },
        { "GL_MAX_GEOMETRY_INPUT_COMPONENTS",        0x9123, 64 },
        { "GL_MAX_GEOMETRY_OUTPUT_COMPONENTS",       0x9124, 128 },
        { "GL_MAX_GEOMETRY_SHADER_INVOCATIONS",      0x8E5A, 32 },
        { "GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS",     0x8C29, 16 },
        { "GL_MAX_GEOMETRY_UNIFORM_COMPONENTS",      0x8DDF, 1024 },
        { "GL_MAX_VERTEX_STREAMS",                   0x8E71, 1 },
        { "GL_MAX_VARYING_COMPONENTS",               0x8B4B, 60 },
    };

    for (unsigned i = 0; i < sizeof req / sizeof *req; i++)
    {
        GLint v = -1;

        mgl_drain_errors();
        glGetIntegerv(req[i].e, &v);

        CHECK_MSG(mgl_drain_errors() == GL_NO_ERROR, "%s was not answered", req[i].name);
        CHECK_MSG(v >= req[i].floor_value, "%s reports %d, floor is %d",
                  req[i].name, v, req[i].floor_value);
    }
}

// Records what each gl_in[] slot holds, one point per slot, so a test can
// read back exactly which vertices the geometry stage was handed.
static GLuint linkRecorder(const char *layout_in, int n_in)
{
    static const char *vs =
        "#version 430 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 430 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1.0); }\n";
    char gs[512], log[2048] = "";
    const char *names[] = { "got" };
    GLint ok = 0;

    snprintf(gs, sizeof gs,
             "#version 430 core\n"
             "layout(%s) in;\n"
             "layout(points, max_vertices = %d) out;\n"
             "out float got;\n"
             "void main() {\n"
             "    for (int i = 0; i < %d; i++) { got = gl_in[i].gl_Position.x; EmitVertex(); }\n"
             "}\n", layout_in, n_in, n_in);

    GLuint prog = glCreateProgram();
    GLuint v = compileOne(GL_VERTEX_SHADER, vs, log, sizeof log);
    GLuint g = compileOne(GL_GEOMETRY_SHADER, gs, log, sizeof log);
    GLuint f = compileOne(GL_FRAGMENT_SHADER, fs, log, sizeof log);

    CHECK_MSG(v && g && f, "did not compile: %s", log);
    if (!v || !g || !f)
        return 0;

    glAttachShader(prog, v);
    glAttachShader(prog, g);
    glAttachShader(prog, f);
    glTransformFeedbackVaryings(prog, 1, names, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
        glGetProgramInfoLog(prog, sizeof log, NULL, log);
    CHECK_MSG(ok, "did not link: %s", log);

    return ok ? prog : 0;
}

// Vertex n sits at x = n, so what was recorded names the vertices directly.
static void recordDrawN(GLuint prog, GLenum mode, int n_verts, const void *indices, GLenum itype,
                        int n_draw, GLint base, GLsizei instances, GLfloat *out, int n_out)
{
    GLfloat pts[32][2];
    GLuint vao, vbo, ebo = 0, xfb;

    for (int k = 0; k < n_verts; k++)
    {
        pts[k][0] = (GLfloat)k;
        pts[k][1] = 0.0f;
    }

    vao = pointsVAO(&pts[0][0], sizeof(GLfloat) * 2 * (size_t)n_verts, &vbo);

    if (indices)
    {
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, n_draw * (itype == GL_UNSIGNED_SHORT ? 2 : 4), indices, GL_STATIC_DRAW);
    }

    glGenBuffers(1, &xfb);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, xfb);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, sizeof(GLfloat) * (size_t)n_out, NULL, GL_STATIC_READ);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, xfb);

    glUseProgram(prog);
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);

    if (indices)
        glDrawElementsInstancedBaseVertex(mode, n_draw, itype, NULL, instances, base);
    else
        glDrawArraysInstanced(mode, 0, n_draw, instances);

    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetBufferSubData(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(GLfloat) * (size_t)n_out, out);

    glUseProgram(0);
    glDeleteBuffers(1, &xfb);
    if (ebo)
        glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

static void recordDraw(GLuint prog, GLenum mode, int n_verts, const void *indices, GLenum itype,
                       int n_draw, GLint base, GLfloat *out, int n_out)
{
    recordDrawN(prog, mode, n_verts, indices, itype, n_draw, base, 1, out, n_out);
}

// Indices may name any vertex, not just the first `count`, and the base vertex
// moves all of them.
GPU_TEST(geometry_shader, indexed_adjacency_reads_the_named_vertices)
{
    GLuint prog = linkRecorder("lines_adjacency", 4);

    if (!prog)
        return;

    static const GLuint idx32[8] = { 9, 3, 7, 1, 11, 0, 2, 5 };
    static const GLushort idx16[8] = { 9, 3, 7, 1, 11, 0, 2, 5 };
    GLfloat got[8];

    for (int wide = 0; wide < 2; wide++)
    {
        memset(got, 0, sizeof got);
        recordDraw(prog, GL_LINES_ADJACENCY, 13, wide ? (const void *)idx32 : (const void *)idx16,
                   wide ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 8, 1, got, 8);

        for (int k = 0; k < 8; k++)
            CHECK_MSG(got[k] == (GLfloat)(idx32[k] + 1), "%s index %d recorded vertex %g, want %u",
                      wide ? "32 bit" : "16 bit", k, got[k], idx32[k] + 1);
    }

    glDeleteProgram(prog);
}

// GL 4.6 table 10.1: a strip with adjacency hands each triangle its six
// vertices in an order that depends on its place in the strip.
GPU_TEST(geometry_shader, triangle_strip_adjacency_follows_the_table)
{
    GLuint prog = linkRecorder("triangles_adjacency", 6);

    if (!prog)
        return;

    // first, odd middle, even middle, odd last
    static const int want[24] = { 0, 1, 2, 6, 4, 3,    4, 0, 2, 5, 6, 8,
                                  4, 2, 6, 10, 8, 7,   8, 4, 6, 9, 10, 11 };
    GLfloat got[24];

    memset(got, 0, sizeof got);
    recordDraw(prog, GL_TRIANGLE_STRIP_ADJACENCY, 12, NULL, 0, 12, 0, got, 24);

    for (int k = 0; k < 24; k++)
        CHECK_MSG(got[k] == (GLfloat)want[k], "triangle %d slot %d recorded vertex %g, want %d",
                  k / 6, k % 6, got[k], want[k]);

    glDeleteProgram(prog);
}

// Every instance runs the vertex stage again, and the geometry stage sees
// that instance's vertices, not the first instance's.
GPU_TEST(geometry_shader, instances_each_reach_the_geometry_stage)
{
    static const char *vs =
        "#version 430 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p.x + float(gl_InstanceID) * 100.0, 0.0, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 430 core\n"
        "layout(lines) in;\n"
        "layout(points, max_vertices = 2) out;\n"
        "out float got;\n"
        "void main() {\n"
        "    got = gl_in[0].gl_Position.x; EmitVertex();\n"
        "    got = gl_in[1].gl_Position.x; EmitVertex();\n"
        "}\n";
    static const char *fs =
        "#version 430 core\nout vec4 o;\nvoid main() { o = vec4(1.0); }\n";
    const char *names[] = { "got" };
    char log[2048] = "";
    GLint ok = 0;
    GLuint prog = glCreateProgram();
    GLuint v = compileOne(GL_VERTEX_SHADER, vs, log, sizeof log);
    GLuint g = compileOne(GL_GEOMETRY_SHADER, gs, log, sizeof log);
    GLuint f = compileOne(GL_FRAGMENT_SHADER, fs, log, sizeof log);

    CHECK_MSG(v && g && f, "did not compile: %s", log);
    if (!v || !g || !f)
        return;

    glAttachShader(prog, v);
    glAttachShader(prog, g);
    glAttachShader(prog, f);
    glTransformFeedbackVaryings(prog, 1, names, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    if (!ok)
        return;

    static const GLuint idx[4] = { 3, 1, 2, 0 };
    GLfloat got[12];

    // arrays: 3 instances of two lines over vertices 0..3
    memset(got, 0, sizeof got);
    recordDrawN(prog, GL_LINES, 4, NULL, 0, 4, 0, 3, got, 12);

    for (int k = 0; k < 12; k++)
        CHECK_MSG(got[k] == (GLfloat)((k / 4) * 100 + k % 4), "arrays: output %d is %g", k, got[k]);

    // elements: the same, through an index list
    memset(got, 0, 8 * sizeof(GLfloat));
    recordDrawN(prog, GL_LINES, 4, idx, GL_UNSIGNED_INT, 4, 0, 2, got, 8);

    for (int k = 0; k < 8; k++)
        CHECK_MSG(got[k] == (GLfloat)((k / 4) * 100 + (int)idx[k % 4]), "elements: output %d is %g", k, got[k]);

    glDeleteProgram(prog);
}

// A loop closes back to its first vertex, a fan turns about its first, and a
// strip swaps the first two vertices of every other triangle.
GPU_TEST(geometry_shader, loops_fans_and_strips_hand_over_their_vertices_in_order)
{
    GLuint lines = linkRecorder("lines", 2);
    GLuint tris = linkRecorder("triangles", 3);
    GLfloat got[12];

    if (!lines || !tris)
        return;

    static const int loop[8] = { 0, 1,  1, 2,  2, 3,  3, 0 };
    memset(got, 0, sizeof got);
    recordDraw(lines, GL_LINE_LOOP, 4, NULL, 0, 4, 0, got, 8);
    for (int k = 0; k < 8; k++)
        CHECK_MSG(got[k] == (GLfloat)loop[k], "loop output %d is %g, want %d", k, got[k], loop[k]);

    static const int fan[9] = { 0, 1, 2,  0, 2, 3,  0, 3, 4 };
    memset(got, 0, sizeof got);
    recordDraw(tris, GL_TRIANGLE_FAN, 5, NULL, 0, 5, 0, got, 9);
    for (int k = 0; k < 9; k++)
        CHECK_MSG(got[k] == (GLfloat)fan[k], "fan output %d is %g, want %d", k, got[k], fan[k]);

    static const int strip[9] = { 0, 1, 2,  2, 1, 3,  2, 3, 4 };
    memset(got, 0, sizeof got);
    recordDraw(tris, GL_TRIANGLE_STRIP, 5, NULL, 0, 5, 0, got, 9);
    for (int k = 0; k < 9; k++)
        CHECK_MSG(got[k] == (GLfloat)strip[k], "strip output %d is %g, want %d", k, got[k], strip[k]);

    glDeleteProgram(lines);
    glDeleteProgram(tris);
}
