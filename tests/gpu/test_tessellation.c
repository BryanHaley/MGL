/*
 * test_tessellation.c
 * Copyright (C) The Moogle Project
 *
 * Tessellation: what links, what the program reports about the domain, and
 * what the tessellator actually produces.
 *
 * The subdivision check is the one that matters. A driver that links the
 * program and then draws the patch as a single triangle answers every query
 * correctly and still is not tessellating, so the gate is that raising the
 * tessellation level changes the picture.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdlib.h>
#include <string.h>

#ifndef GL_PATCHES
#define GL_PATCHES 0x000E
#endif

static const char *VS =
    "#version 410\n"
    "layout(location = 0) in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

static const char *TCS =
    "#version 410\n"
    "layout(vertices = 3) out;\n"
    "uniform float lvl;\n"
    "void main() {\n"
    "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
    "    gl_TessLevelOuter[0] = lvl;\n"
    "    gl_TessLevelOuter[1] = lvl;\n"
    "    gl_TessLevelOuter[2] = lvl;\n"
    "    gl_TessLevelInner[0] = lvl;\n"
    "}\n";

// A warp that is not linear in the tessellation coordinate, so one triangle
// and sixteen do not cover the same pixels.
static const char *TES =
    "#version 410\n"
    "layout(triangles, equal_spacing, ccw) in;\n"
    "void main() {\n"
    "    vec4 q = gl_TessCoord.x * gl_in[0].gl_Position\n"
    "           + gl_TessCoord.y * gl_in[1].gl_Position\n"
    "           + gl_TessCoord.z * gl_in[2].gl_Position;\n"
    "    float s = 0.30 * sin(9.4248 * gl_TessCoord.x) * sin(9.4248 * gl_TessCoord.y);\n"
    "    gl_Position = vec4(q.xy * 0.7 + vec2(s, s), 0.0, 1.0);\n"
    "}\n";

static const char *FS =
    "#version 410\n"
    "out vec4 o;\n"
    "void main() { o = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static GLuint compileOne(GLenum stage, const char *src)
{
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    return ok ? sh : 0;
}

// Links whichever of the four stages are given; 0 means the link failed.
static GLuint linkStages(const char *vs, const char *tcs, const char *tes, const char *fs,
                         char *log, int log_size)
{
    struct { GLenum stage; const char *src; } all[] = {
        { GL_VERTEX_SHADER, vs },
        { GL_TESS_CONTROL_SHADER, tcs },
        { GL_TESS_EVALUATION_SHADER, tes },
        { GL_FRAGMENT_SHADER, fs },
    };
    GLuint prog = glCreateProgram();
    GLint ok = 0;

    if (log && log_size)
        log[0] = 0;

    for (unsigned i = 0; i < sizeof all / sizeof *all; i++)
    {
        GLuint sh;

        if (all[i].src == NULL)
            continue;

        sh = compileOne(all[i].stage, all[i].src);

        if (sh == 0)
        {
            if (log && log_size)
                snprintf(log, log_size, "stage 0x%x did not compile", all[i].stage);

            return 0;
        }

        glAttachShader(prog, sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok && log && log_size)
        glGetProgramInfoLog(prog, log_size, NULL, log);

    return ok ? prog : 0;
}

static int greenPixels(const MGLTestTarget *t)
{
    unsigned char *px = mgl_read_rgba8(t);
    int n = 0;

    if (px == NULL)
        return -1;

    for (int i = 0; i < t->width * t->height; i++)
        if (px[i * 4 + 1] > 200 && px[i * 4] < 60)
            n++;

    free(px);

    return n;
}

/* ---------- the tessellator really subdivides ---------- */

GPU_TEST(tessellation, level_changes_the_geometry)
{
    static const GLfloat tri[6] = { -1,-1, 1,-1, -1,1 };
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    GLint lvl;
    int flat, fine;
    char log[1024];

    prog = linkStages(VS, TCS, TES, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "tessellation program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    lvl = glGetUniformLocation(prog, "lvl");
    CHECK_MSG(lvl >= 0, "the control shader's uniform has no location");

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glViewport(0, 0, 64, 64);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glClearColor(1, 0, 0, 1);

    glUniform1f(lvl, 4.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 3);
    flat = greenPixels(&t);

    glUniform1f(lvl, 16.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 3);
    fine = greenPixels(&t);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(flat > 0, "level 4 drew nothing");
    CHECK_MSG(fine > 0, "level 16 drew nothing");
    CHECK_MSG(flat != fine, "level 4 covered %d and level 16 covered %d", flat, fine);

    glUseProgram(0);
    mgl_target_destroy(&t);
}

/* ---------- what the program says about the domain ---------- */

GPU_TEST(tessellation, program_reports_its_domain)
{
    GLuint prog;
    GLint v = -1;
    char log[1024];

    prog = linkStages(VS, TCS, TES, FS, log, sizeof log);

    if (!prog)
    {
        CHECK_MSG(0, "tessellation program did not link: %s", log);
        return;
    }

    glGetProgramiv(prog, GL_TESS_CONTROL_OUTPUT_VERTICES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, 3);

    glGetProgramiv(prog, GL_TESS_GEN_MODE, &v);
    CHECK_EQ_INT(v, GL_TRIANGLES);

    glGetProgramiv(prog, GL_TESS_GEN_SPACING, &v);
    CHECK_EQ_INT(v, GL_EQUAL);

    glGetProgramiv(prog, GL_TESS_GEN_VERTEX_ORDER, &v);
    CHECK_EQ_INT(v, GL_CCW);

    glGetProgramiv(prog, GL_TESS_GEN_POINT_MODE, &v);
    CHECK_EQ_INT(v, GL_FALSE);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDeleteProgram(prog);
}

/* ---------- the linking rules ---------- */

GPU_TEST(tessellation, control_without_evaluation_does_not_link)
{
    char log[1024];
    GLuint prog = linkStages(VS, TCS, NULL, FS, log, sizeof log);

    // GL 4.6 section 7.3 names this one specifically
    CHECK_MSG(prog == 0, "a control shader with no evaluation shader linked anyway");
    CHECK_MSG(log[0] != 0, "the failed link left no info log");
}

// An evaluation shader on its own is legal; the control stage is then fixed
// function and the levels come from glPatchParameterfv.
GPU_TEST(tessellation, evaluation_without_control_links_and_draws)
{
    static const GLfloat tri[6] = { -1,-1, 1,-1, -1,1 };
    static const GLfloat inner[2] = { 1.0f, 1.0f };
    static const GLfloat outer[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const char *tes_passthrough =
        "#version 410\n"
        "layout(triangles, equal_spacing, ccw) in;\n"
        "void main() {\n"
        "    gl_Position = gl_TessCoord.x * gl_in[0].gl_Position\n"
        "                + gl_TessCoord.y * gl_in[1].gl_Position\n"
        "                + gl_TessCoord.z * gl_in[2].gl_Position;\n"
        "}\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    char log[1024];
    int drawn;

    glPatchParameteri(GL_PATCH_VERTICES, 3);
    prog = linkStages(VS, NULL, tes_passthrough, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "evaluation shader alone did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glViewport(0, 0, 64, 64);
    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, inner);
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 3);

    drawn = greenPixels(&t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(drawn > 1000, "the fixed function control stage drew %d pixels", drawn);

    glUseProgram(0);
    mgl_target_destroy(&t);
}

// Metal has no isoline tessellator. MGL runs isolines as quads and hands the
// segments to a generated geometry stage, so they draw as lines.
GPU_TEST(tessellation, isolines_draw_as_lines)
{
    static const char *tcs_iso =
        "#version 410\n"
        "layout(vertices = 4) out;\n"
        "void main() {\n"
        "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "    gl_TessLevelOuter[0] = 4.0; gl_TessLevelOuter[1] = 8.0;\n"
        "}\n";
    static const char *tes_iso =
        "#version 410\n"
        "layout(isolines) in;\n"
        "void main() {\n"
        "    vec2 p = mix(vec2(-0.9), vec2(0.9), gl_TessCoord.xy);\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const GLfloat quad[8] = { -1,-1, 1,-1, 1,1, -1,1 };
    MGLTestTarget t;
    GLuint vao, vbo;
    char log[1024];
    GLuint prog = linkStages(VS, tcs_iso, tes_iso, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "isolines did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
        return;

    mgl_target_bind(&t);
    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glViewport(0, 0, 64, 64);
    glPatchParameteri(GL_PATCH_VERTICES, 4);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 4);

    int green = greenPixels(&t);

    // four one-pixel lines across most of the target, not a filled quad
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(green > 4 * 40 && green < 64 * 64 / 4, "isolines covered %d pixels", green);

    glUseProgram(0);
    mgl_target_destroy(&t);
}

/* ---------- the patch state queries ---------- */

GPU_TEST(tessellation, patch_parameters_read_back)
{
    static const GLfloat inner[2] = { 2.5f, 3.5f };
    static const GLfloat outer[4] = { 1.5f, 2.5f, 3.5f, 4.5f };
    GLint v = -1;
    GLfloat got_inner[2] = { 0 }, got_outer[4] = { 0 };

    glPatchParameteri(GL_PATCH_VERTICES, 4);
    glGetIntegerv(GL_PATCH_VERTICES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, 4);

    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, inner);
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
    glGetFloatv(GL_PATCH_DEFAULT_INNER_LEVEL, got_inner);
    glGetFloatv(GL_PATCH_DEFAULT_OUTER_LEVEL, got_outer);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 2; i++)
        CHECK_MSG(got_inner[i] == inner[i], "inner level %d reads %f, set %f",
                  i, got_inner[i], inner[i]);

    for (int i = 0; i < 4; i++)
        CHECK_MSG(got_outer[i] == outer[i], "outer level %d reads %f, set %f",
                  i, got_outer[i], outer[i]);

    glPatchParameteri(GL_PATCH_VERTICES, 3);
}

/* ---------- the buffer between the control and evaluation stages ---------- */

// Metal lays that buffer out from each stage's own declarations. A point size
// the control stage writes and the evaluation stage never reads used to shift
// every vertex after the first patch.
GPU_TEST(tessellation, ignored_control_output_keeps_the_layout)
{
    static const char *vs =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "out vec4 color;\n"
        "void main() { gl_PointSize = 0.1; color = vec4(0, 1, 0, 1); gl_Position = vec4(p, 0, 1); }\n";
    static const char *tcs =
        "#version 410\n"
        "layout(vertices = 3) out;\n"
        "in vec4 color[];\n"
        "out vec4 tcColor[];\n"
        "void main() {\n"
        "    tcColor[gl_InvocationID] = color[gl_InvocationID];\n"
        "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "    gl_out[gl_InvocationID].gl_PointSize = gl_in[gl_InvocationID].gl_PointSize;\n"
        "    gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0; gl_TessLevelOuter[2] = 1.0;\n"
        "    gl_TessLevelInner[0] = 1.0;\n"
        "}\n";
    static const char *tes =
        "#version 410\n"
        "layout(triangles) in;\n"
        "in vec4 tcColor[];\n"
        "out vec4 c;\n"
        "void main() {\n"
        "    gl_Position = gl_TessCoord.x * gl_in[0].gl_Position + gl_TessCoord.y * gl_in[1].gl_Position\n"
        "                + gl_TessCoord.z * gl_in[2].gl_Position;\n"
        "    c = tcColor[0];\n"
        "}\n";
    static const char *fs = "#version 410\nin vec4 c;\nout vec4 o;\nvoid main() { o = c; }\n";
    static const GLfloat two[12] = { -1,-1, 1,-1, -1,1,  1,1, -1,1, 1,-1 };
    MGLTestTarget t;
    GLuint vao, vbo;
    char log[1024];
    GLuint prog = linkStages(vs, tcs, tes, fs, log, sizeof log);

    CHECK_MSG(prog != 0, "did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
        return;

    mgl_target_bind(&t);
    glUseProgram(prog);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof two, two, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glViewport(0, 0, 32, 32);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_PATCHES, 0, 6);

    // both triangles, so the whole target
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(greenPixels(&t), 32 * 32);

    glUseProgram(0);
    mgl_target_destroy(&t);
}

/* ---------- built-ins in the tessellation stages ---------- */

// Point mode that reads the control stage's point size but never sets its own
// used to lose every point. Two draws in a row also used to hang.
GPU_TEST(tessellation, point_mode_keeps_points_that_read_point_size)
{
    static const char *vs = "#version 440\nvoid main() { gl_Position = vec4(0, 0, 0, 1); }\n";
    static const char *tcs =
        "#version 440\n"
        "layout(vertices = 3) out;\n"
        "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_out[];\n"
        "void main() {\n"
        "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "    gl_out[gl_InvocationID].gl_PointSize = 2.0 + float(gl_InvocationID);\n"
        "    gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0;\n"
        "    gl_TessLevelOuter[2] = 1.0; gl_TessLevelInner[0] = 1.0;\n"
        "}\n";
    static const char *tes =
        "#version 440\n"
        "layout(triangles, point_mode) in;\n"
        "in gl_PerVertex { vec4 gl_Position; float gl_PointSize; } gl_in[];\n"
        "out gl_PerVertex { vec4 gl_Position; float gl_PointSize; };\n"
        "out float te_size;\n"
        "void main() {\n"
        "    gl_Position = vec4(gl_TessCoord, 1.0);\n"
        "    te_size = gl_in[0].gl_PointSize + gl_in[2].gl_PointSize;\n"
        "}\n";
    static const char *fs = "#version 440\nvoid main() {}\n";
    static const char *names[1] = { "te_size" };
    GLuint prog = glCreateProgram();
    GLuint vao, buf;
    GLint ok = 0;

    glAttachShader(prog, compileOne(GL_VERTEX_SHADER, vs));
    glAttachShader(prog, compileOne(GL_TESS_CONTROL_SHADER, tcs));
    glAttachShader(prog, compileOne(GL_TESS_EVALUATION_SHADER, tes));
    glAttachShader(prog, compileOne(GL_FRAGMENT_SHADER, fs));
    glTransformFeedbackVaryings(prog, 1, names, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK_MSG(ok, "did not link");

    if (!ok)
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 1024, NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);

    // a triangle at level one is its three corners; two draws make six points
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_PATCHES, 0, 3);
    glDrawArrays(GL_PATCHES, 0, 3);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);

    const GLfloat *d = (const GLfloat *)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 1024, GL_MAP_READ_BIT);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(d != NULL);

    if (d)
    {
        for (int i = 0; i < 6; i++)
            CHECK_MSG(d[i] == 6.0f, "point %d captured %g, not 6", i, d[i]);

        glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(tessellation, ids_read_back_through_feedback)
{
    static const char *vs = "#version 440\nvoid main() { gl_Position = vec4(0, 0, 0, 1); }\n";
    static const char *tcs =
        "#version 440\n"
        "layout(vertices = 4) out;\n"
        "out int tc_invocation[];\n"
        "out int tc_prim[];\n"
        "void main() {\n"
        "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "    tc_invocation[gl_InvocationID] = gl_InvocationID;\n"
        "    tc_prim[gl_InvocationID] = gl_PrimitiveID;\n"
        "    gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0;\n"
        "    gl_TessLevelOuter[2] = 1.0; gl_TessLevelOuter[3] = 1.0;\n"
        "    gl_TessLevelInner[0] = 1.0; gl_TessLevelInner[1] = 1.0;\n"
        "}\n";
    static const char *tes =
        "#version 440\n"
        "layout(isolines, point_mode) in;\n"
        "in int tc_invocation[];\n"
        "in int tc_prim[];\n"
        "flat out int last_invocation;\n"
        "flat out int patch_vertices;\n"
        "flat out int prim_tc;\n"
        "flat out int prim_te;\n"
        "void main() {\n"
        "    gl_Position = vec4(0, 0, 0, 1);\n"
        "    last_invocation = tc_invocation[gl_PatchVerticesIn - 1];\n"
        "    patch_vertices = gl_PatchVerticesIn;\n"
        "    prim_tc = tc_prim[0];\n"
        "    prim_te = gl_PrimitiveID;\n"
        "}\n";
    static const char *fs = "#version 440\nvoid main() {}\n";
    static const char *names[4] = { "last_invocation", "patch_vertices", "prim_tc", "prim_te" };
    GLuint prog = glCreateProgram();
    GLuint vao, buf;
    GLint ok = 0;

    glAttachShader(prog, compileOne(GL_VERTEX_SHADER, vs));
    glAttachShader(prog, compileOne(GL_TESS_CONTROL_SHADER, tcs));
    glAttachShader(prog, compileOne(GL_TESS_EVALUATION_SHADER, tes));
    glAttachShader(prog, compileOne(GL_FRAGMENT_SHADER, fs));
    glTransformFeedbackVaryings(prog, 4, names, GL_INTERLEAVED_ATTRIBS);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK_MSG(ok, "did not link");

    if (!ok)
        return;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glPatchParameteri(GL_PATCH_VERTICES, 4);
    glGenBuffers(1, &buf);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, buf);
    glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, 4096, NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buf);

    // two patches, drawn twice over: eight patches, two points each
    glEnable(GL_RASTERIZER_DISCARD);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArraysInstanced(GL_PATCHES, 0, 8, 2);
    glEndTransformFeedback();
    glDisable(GL_RASTERIZER_DISCARD);

    const GLint *d = (const GLint *)glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 4096, GL_MAP_READ_BIT);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(d != NULL);

    if (d)
    {
        // the last vertex of the last patch of the second instance
        const GLint *v = d + (4 * 2 - 1) * 4;

        CHECK_EQ_INT(d[0], 3);      // gl_InvocationID of the last control invocation
        CHECK_EQ_INT(d[1], 4);      // gl_PatchVerticesIn
        CHECK_EQ_INT(d[2], 0);      // gl_PrimitiveID in the control stage
        CHECK_EQ_INT(d[3], 0);      // and in the evaluation stage
        CHECK_EQ_INT(v[2], 1);      // starts over for the second instance
        CHECK_EQ_INT(v[3], 1);
        glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
    }

    glUseProgram(0);
    glDeleteProgram(prog);
}
