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

// Metal has no isoline tessellator, and a documented gap is better than a
// program that links and draws nothing.
GPU_TEST(tessellation, isolines_are_refused_with_a_reason)
{
    static const char *tcs_iso =
        "#version 410\n"
        "layout(vertices = 4) out;\n"
        "void main() {\n"
        "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
        "    gl_TessLevelOuter[0] = 1.0; gl_TessLevelOuter[1] = 1.0;\n"
        "}\n";
    static const char *tes_iso =
        "#version 410\n"
        "layout(isolines) in;\n"
        "void main() { gl_Position = gl_in[0].gl_Position; }\n";
    char log[1024];
    GLuint prog = linkStages(VS, tcs_iso, tes_iso, FS, log, sizeof log);

    CHECK_MSG(prog == 0, "isolines linked, which Metal cannot honour");
    CHECK_MSG(strstr(log, "isoline") != NULL,
              "the info log should say isolines are the problem, got \"%s\"", log);
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
