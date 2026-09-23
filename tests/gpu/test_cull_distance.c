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

/* ---------- cull distance written by a tessellation stage ---------- */

// A tessellation evaluation stage that writes a cull distance is drawn through
// a geometry stage MGL generates. That stage passed gl_CullDistance on under
// the rewrite's internal name, as an ordinary varying nothing wrote, so the
// program never linked.
static GLuint linkTess(const char *vs, const char *tcs, const char *tes, const char *fs,
                       char *log, int log_size)
{
    GLuint p = glCreateProgram();
    GLuint s[4];
    GLint ok = 0;

    s[0] = compileStage(GL_VERTEX_SHADER, vs, log, log_size);
    s[1] = compileStage(GL_TESS_CONTROL_SHADER, tcs, log, log_size);
    s[2] = compileStage(GL_TESS_EVALUATION_SHADER, tes, log, log_size);
    s[3] = compileStage(GL_FRAGMENT_SHADER, fs, log, log_size);

    for (int i = 0; i < 4; i++)
    {
        if (!s[i])
            return 0;

        glAttachShader(p, s[i]);
    }

    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    if (!ok && log && log_size)
        glGetProgramInfoLog(p, log_size, NULL, log);

    return ok ? p : 0;
}

GPU_TEST(cull_distance, a_tessellation_stage_can_cull_a_whole_patch)
{
    static const char *vs =
        "#version 450\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *tcs =
        "#version 450\n"
        "layout(vertices = 3) out;\n"
        "void main() {\n"
        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "  gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0;\n"
        "  gl_TessLevelInner[0] = 1.0;\n"
        "}\n";
    static const char *tes =
        "#version 450\n"
        "layout(triangles) in;\n"
        "uniform float dist;\n"
        "uniform float slope;\n"
        "out float gl_CullDistance[1];\n"
        "void main() {\n"
        "  gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position\n"
        "              + gl_TessCoord.z * gl_in[2].gl_Position;\n"
        "  gl_CullDistance[0] = dist + slope * gl_TessCoord.x;\n"
        "}\n";
    static const GLfloat tri[6] = { -1,-1, 3,-1, -1,3 };
    MGLTestTarget t;
    GLuint prog, vao = 0, vbo = 0;
    int kept, culled, mixed;
    char log[2048];

    prog = linkTess(vs, tcs, tes, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "tessellation cull program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    mgl_target_bind(&t);
    glViewport(0, 0, 32, 32);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    glUniform1f(glGetUniformLocation(prog, "dist"), 1.0f);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 3);
    kept = greenPixels(&t);

    glUniform1f(glGetUniformLocation(prog, "dist"), -1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 3);
    culled = greenPixels(&t);

    // one corner in front and two behind: culling keeps the whole triangle,
    // where clipping would have cut it
    glUniform1f(glGetUniformLocation(prog, "dist"), -0.5f);
    glUniform1f(glGetUniformLocation(prog, "slope"), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 3);
    mixed = greenPixels(&t);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(kept == 32 * 32, "a positive distance drew %d of 1024 pixels", kept);
    CHECK_MSG(culled == 0, "a negative distance still drew %d pixels", culled);
    CHECK_MSG(mixed == 32 * 32, "a triangle with one corner in front drew %d of 1024 pixels", mixed);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// A fragment shader may read gl_CullDistance back. Metal has no such output,
// so MGL keeps the builtin in a local in the vertex stage; each element now
// also goes out as a plain varying for the fragment side to read.
GPU_TEST(cull_distance, a_fragment_shader_reads_the_distances_back)
{
    static const char *vs =
        "#version 450\n"
        "layout(location = 0) in vec2 p;\n"
        "out float gl_CullDistance[2];\n"
        "void main() { gl_CullDistance[0] = 0.25; gl_CullDistance[1] = 0.75; gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 450\n"
        "in float gl_CullDistance[2];\n"
        "out vec4 o;\n"
        "void main() { o = vec4(gl_CullDistance[0], gl_CullDistance[1], 0.0, 1.0); }\n";
    static const float tri[] = { -1, -1,  3, -1,  -1, 3 };
    GLuint prog, vao, vbo;
    MGLTestTarget t;
    unsigned char c[4] = { 0 };
    unsigned char *px;
    char log[2048];

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not build: %s", log);

    if (!prog || !mgl_target_create(&t, 8, 8, GL_RGBA8, 0))
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    mgl_target_bind(&t);
    glViewport(0, 0, 8, 8);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, 4, 4, c);
        CHECK_MSG(abs(c[0] - 64) <= 2 && abs(c[1] - 191) <= 2, "read back %d,%d, want 64,191", c[0], c[1]);
        free(px);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

// Clip and cull distances written by the vertex stage travel through the
// control and evaluation stages, which pass them on the way any output goes.
GPU_TEST(cull_distance, distances_pass_through_tessellation)
{
    static const char *vs =
        "#version 450\n"
        "layout(location = 0) in vec2 p;\n"
        "uniform float clip;\n"
        "uniform float cull;\n"
        "out float gl_ClipDistance[1];\n"
        "out float gl_CullDistance[1];\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); gl_ClipDistance[0] = clip; gl_CullDistance[0] = cull; }\n";
    static const char *tcs =
        "#version 450\n"
        "layout(vertices = 3) out;\n"
        "void main() {\n"
        "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "  gl_out[gl_InvocationID].gl_ClipDistance[0] = gl_in[gl_InvocationID].gl_ClipDistance[0];\n"
        "  gl_out[gl_InvocationID].gl_CullDistance[0] = gl_in[gl_InvocationID].gl_CullDistance[0];\n"
        "  gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0;\n"
        "  gl_TessLevelInner[0] = 1.0;\n"
        "}\n";
    static const char *tes =
        "#version 450\n"
        "layout(triangles) in;\n"
        "out float gl_ClipDistance[1];\n"
        "out float gl_CullDistance[1];\n"
        "void main() {\n"
        "  gl_Position = vec4(mat3(gl_in[0].gl_Position.xyz, gl_in[1].gl_Position.xyz, gl_in[2].gl_Position.xyz) * gl_TessCoord, 1.0);\n"
        "  gl_ClipDistance[0] = dot(vec3(gl_in[0].gl_ClipDistance[0], gl_in[1].gl_ClipDistance[0], gl_in[2].gl_ClipDistance[0]), gl_TessCoord);\n"
        "  gl_CullDistance[0] = dot(vec3(gl_in[0].gl_CullDistance[0], gl_in[1].gl_CullDistance[0], gl_in[2].gl_CullDistance[0]), gl_TessCoord);\n"
        "}\n";
    static const GLfloat tri[6] = { -1,-1, 3,-1, -1,3 };
    const float cases[4][2] = { { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
    MGLTestTarget t;
    GLuint prog, vao = 0, vbo = 0;
    char log[2048];

    prog = linkTess(vs, tcs, tes, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    mgl_target_bind(&t);
    glViewport(0, 0, 32, 32);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glEnable(GL_CLIP_DISTANCE0);
    glClearColor(0, 0, 0, 1);

    for (int c = 0; c < 4; c++)
    {
        glUniform1f(glGetUniformLocation(prog, "clip"), cases[c][0]);
        glUniform1f(glGetUniformLocation(prog, "cull"), cases[c][1]);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_PATCHES, 0, 3);

        int want = cases[c][0] > 0 && cases[c][1] > 0 ? 32 * 32 : 0;
        int got = greenPixels(&t);

        CHECK_MSG(got == want, "clip %g cull %g drew %d pixels, want %d", cases[c][0], cases[c][1], got, want);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDisable(GL_CLIP_DISTANCE0);
    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}
