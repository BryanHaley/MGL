/*
 * test_cull_distance.c
 * Copyright (C) The MooGL Project
 *
 * gl_CullDistance throws a primitive away only when every one of its vertices
 * says to. Metal has no such thing, so MGL records the distances in a pass of
 * its own and picks the survivors in compute. The difference that matters is
 * the triangle with one negative corner: clipping would cut it, culling keeps
 * all of it.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdlib.h>
#include <string.h>

static const char *VS =
    "#version 450\n"
    "layout(location = 0) in vec2 p;\n"
    "layout(location = 1) in float d;\n"
    "out float gl_CullDistance[1];\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "    gl_CullDistance[0] = d;\n"
    "}\n";

static const char *FS =
    "#version 450\n"
    "out vec4 o;\n"
    "void main() { o = vec4(0, 1, 0, 1); }\n";

/* A triangle that covers the whole target, with a cull distance per corner. */
static GLuint triangleWithDistances(GLuint *vbo, float d0, float d1, float d2)
{
    GLfloat v[] = {
        -1.0f, -1.0f, d0,
         3.0f, -1.0f, d1,
        -1.0f,  3.0f, d2,
    };
    GLuint vao = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 12, (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 12, (void *)8);
    glEnableVertexAttribArray(1);

    return vao;
}

static int greenPixels(const MGLTestTarget *t)
{
    unsigned char *px = mgl_read_rgba8(t);
    int n = 0;

    if (px == NULL)
        return -1;

    for (int y = 0; y < t->height; y++)
        for (int x = 0; x < t->width; x++)
        {
            unsigned char c[4];

            mgl_pixel_at(px, t, x, y, c);

            if (c[1] > 200 && c[0] < 60)
                n++;
        }

    free(px);

    return n;
}

static int drawWithDistances(GLuint prog, float d0, float d1, float d2)
{
    MGLTestTarget t;
    GLuint vao, vbo = 0;
    int green;

    if (!mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
        return -1;

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = triangleWithDistances(&vbo, d0, d1, d2);
    glViewport(0, 0, 32, 32);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    green = greenPixels(&t);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);

    return green;
}

GPU_TEST(cull_distance, every_corner_negative_drops_the_triangle)
{
    char log[2048];
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "cull distance program did not link: %s", log);

    if (!prog)
        return;

    CHECK_EQ_INT(drawWithDistances(prog, -1.0f, -1.0f, -1.0f), 0);

    glDeleteProgram(prog);
}

GPU_TEST(cull_distance, one_negative_corner_keeps_the_whole_triangle)
{
    char log[2048];
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    int all_positive, one_negative;

    CHECK_MSG(prog != 0, "cull distance program did not link: %s", log);

    if (!prog)
        return;

    all_positive = drawWithDistances(prog, 1.0f, 1.0f, 1.0f);
    one_negative = drawWithDistances(prog, -1.0f, 1.0f, 1.0f);

    CHECK_MSG(all_positive > 400, "a triangle with no negative distance drew %d pixels", all_positive);
    CHECK_MSG(one_negative == all_positive,
              "one negative corner cut the triangle down to %d of %d pixels -- that is clipping, not culling",
              one_negative, all_positive);

    glDeleteProgram(prog);
}

GPU_TEST(cull_distance, the_limits_are_answered)
{
    GLint v = -1;

    glGetIntegerv(GL_MAX_CULL_DISTANCES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(v >= 8, "GL_MAX_CULL_DISTANCES is %d, the 4.6 floor is 8", v);

    v = -1;
    glGetIntegerv(GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(v >= 8, "GL_MAX_COMBINED_CLIP_AND_CULL_DISTANCES is %d, the 4.6 floor is 8", v);
}

/* ---------------------------------------------------------------------------
 * Through a geometry stage.
 *
 * The geometry shader is rewritten into a compute pass, so gl_CullDistance has
 * to travel through the generated structs rather than through GL's own
 * plumbing, and the primitive is dropped inside the emitter.
 */

static GLuint compileStage(GLenum stage, const char *src, char *log, int log_size)
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

    v = compileStage(GL_VERTEX_SHADER, vs, log, log_size);
    g = compileStage(GL_GEOMETRY_SHADER, gs, log, log_size);
    f = compileStage(GL_FRAGMENT_SHADER, fs, log, log_size);

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

/* passes the vertex stage's cull distance straight through */
static const char *GS_PASS_CULL =
    "#version 450\n"
    "layout(triangles) in;\n"
    "layout(triangle_strip, max_vertices = 3) out;\n"
    "out float gl_CullDistance[1];\n"
    "void main()\n"
    "{\n"
    "    for (int i = 0; i < 3; i++)\n"
    "    {\n"
    "        gl_Position = gl_in[i].gl_Position;\n"
    "        gl_CullDistance[0] = gl_in[i].gl_CullDistance[0];\n"
    "        EmitVertex();\n"
    "    }\n"
    "    EndPrimitive();\n"
    "}\n";

/* ignores what came in and culls on its own terms */
static const char *GS_OWN_CULL =
    "#version 450\n"
    "layout(triangles) in;\n"
    "layout(triangle_strip, max_vertices = 3) out;\n"
    "out float gl_CullDistance[1];\n"
    "uniform float bias;\n"
    "void main()\n"
    "{\n"
    "    for (int i = 0; i < 3; i++)\n"
    "    {\n"
    "        gl_Position = gl_in[i].gl_Position;\n"
    "        gl_CullDistance[0] = bias;\n"
    "        EmitVertex();\n"
    "    }\n"
    "    EndPrimitive();\n"
    "}\n";

GPU_TEST(cull_distance, a_geometry_stage_passes_the_distance_through)
{
    char log[2048];
    GLuint prog = linkVGF(VS, GS_PASS_CULL, FS, log, sizeof log);
    int all_positive, one_negative, all_negative;

    CHECK_MSG(prog != 0, "the geometry cull program did not build: %s", log);

    if (!prog)
        return;

    all_positive = drawWithDistances(prog, 1.0f, 1.0f, 1.0f);
    one_negative = drawWithDistances(prog, -1.0f, 1.0f, 1.0f);
    all_negative = drawWithDistances(prog, -1.0f, -1.0f, -1.0f);

    CHECK_MSG(all_positive > 400, "the geometry stage drew %d pixels with nothing culled", all_positive);
    CHECK_MSG(one_negative == all_positive,
              "one negative corner cut the triangle to %d of %d pixels -- that is clipping, not culling",
              one_negative, all_positive);
    CHECK_EQ_INT(all_negative, 0);

    glDeleteProgram(prog);
}

GPU_TEST(cull_distance, a_geometry_stage_can_cull_on_its_own)
{
    char log[2048];
    GLuint prog = linkVGF(VS, GS_OWN_CULL, FS, log, sizeof log);
    GLint bias;

    CHECK_MSG(prog != 0, "the geometry cull program did not build: %s", log);

    if (!prog)
        return;

    glUseProgram(prog);
    bias = glGetUniformLocation(prog, "bias");
    CHECK_MSG(bias >= 0, "the geometry shader's own uniform has no location");

    /* the vertex stage says keep, the geometry stage says drop */
    glUniform1f(bias, -1.0f);
    glUseProgram(0);
    CHECK_EQ_INT(drawWithDistances(prog, 1.0f, 1.0f, 1.0f), 0);

    glUseProgram(prog);
    glUniform1f(bias, 1.0f);
    glUseProgram(0);
    CHECK_MSG(drawWithDistances(prog, -1.0f, -1.0f, -1.0f) > 400,
              "the geometry stage kept the primitive and it still did not draw");

    glDeleteProgram(prog);
}
