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
