/*
 * test_subroutines.c
 * Copyright (C) The MooGL Project
 *
 * GLSL subroutines. glslang refuses the keyword outright when it is targeting
 * SPIR-V, so MGL rewrites them into a plain int selector and a switch before
 * the compiler ever sees them. What matters here is that the uniform still
 * chooses which function runs.
 */

#include "mgl_test.h"
#include "harness.h"
#include <stdlib.h>
#include <string.h>

static const char *VS =
    "#version 400\n"
    "layout(location = 0) in vec2 p;\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";

static const char *FS_PICK =
    "#version 400\n"
    "subroutine vec4 pick();\n"
    "subroutine uniform pick which;\n"
    "subroutine(pick) vec4 red()   { return vec4(1, 0, 0, 1); }\n"
    "subroutine(pick) vec4 green() { return vec4(0, 1, 0, 1); }\n"
    "out vec4 o;\n"
    "void main() { o = which(); }\n";

static unsigned char centreGreen(const MGLTestTarget *t)
{
    unsigned char *px = mgl_read_rgba8(t);
    unsigned char c[4] = { 0, 0, 0, 0 };

    if (px == NULL)
        return 0;

    mgl_pixel_at(px, t, t->width / 2, t->height / 2, c);
    free(px);

    return c[1];
}

/* ---------- the uniform picks which function runs ---------- */

GPU_TEST(subroutines, uniform_selects_the_function)
{
    MGLTestTarget t;
    GLuint prog, vao, vbo;
    GLuint red_index, green_index, idx;
    GLint loc;
    char log[2048];

    prog = mgl_build_program(VS, FS_PICK, log, sizeof log);
    CHECK_MSG(prog != 0, "subroutine program did not link: %s", log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = mgl_fullscreen_quad(&vbo);
    glViewport(0, 0, 32, 32);

    red_index = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "red");
    green_index = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "green");
    loc = glGetSubroutineUniformLocation(prog, GL_FRAGMENT_SHADER, "which");

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(red_index != GL_INVALID_INDEX, "red has no subroutine index");
    CHECK_MSG(green_index != GL_INVALID_INDEX, "green has no subroutine index");
    CHECK_MSG(red_index != green_index, "both subroutines got index %u", red_index);
    CHECK_EQ_INT(loc, 0);

    glClearColor(0, 0, 1, 1);

    idx = red_index;
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_MSG(centreGreen(&t) < 60, "red was selected but the draw came out green");

    idx = green_index;
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &idx);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_MSG(centreGreen(&t) > 200, "green was selected but the draw did not come out green");

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);
}

/* ---------- what the program reports about its subroutines ---------- */

GPU_TEST(subroutines, program_reports_what_it_has)
{
    GLuint prog;
    GLint v = -1;
    char log[2048];
    char name[64];
    GLsizei len = 0;

    prog = mgl_build_program(VS, FS_PICK, log, sizeof log);

    if (!prog)
    {
        CHECK_MSG(0, "subroutine program did not link: %s", log);
        return;
    }

    glGetProgramStageiv(prog, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, 2);

    glGetProgramStageiv(prog, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINE_UNIFORMS, &v);
    CHECK_EQ_INT(v, 1);

    glGetProgramStageiv(prog, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS, &v);
    CHECK_EQ_INT(v, 1);

    glGetProgramStageiv(prog, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINE_MAX_LENGTH, &v);
    CHECK_MSG(v >= 6, "the longest subroutine name is \"green\", reported %d", v);

    glGetActiveSubroutineUniformName(prog, GL_FRAGMENT_SHADER, 0, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(!strcmp(name, "which"), "subroutine uniform 0 is named \"%s\"", name);

    glGetActiveSubroutineUniformiv(prog, GL_FRAGMENT_SHADER, 0, GL_NUM_COMPATIBLE_SUBROUTINES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, 2);

    // every index the queries hand out has to name one of the two functions
    for (GLuint i = 0; i < 2; i++)
    {
        name[0] = 0;
        glGetActiveSubroutineName(prog, GL_FRAGMENT_SHADER, i, sizeof name, &len, name);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_MSG(!strcmp(name, "red") || !strcmp(name, "green"),
                  "subroutine %u is named \"%s\"", i, name);
    }

    glDeleteProgram(prog);
}

/* ---------- the error rules ---------- */

GPU_TEST(subroutines, errors)
{
    GLuint prog;
    GLuint idx[2] = { 0, 0 };
    char log[2048];

    prog = mgl_build_program(VS, FS_PICK, log, sizeof log);

    if (!prog)
    {
        CHECK_MSG(0, "subroutine program did not link: %s", log);
        return;
    }

    glUseProgram(prog);

    // a name that is not a subroutine
    CHECK_EQ_UINT(glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "nosuch"), GL_INVALID_INDEX);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT(glGetSubroutineUniformLocation(prog, GL_FRAGMENT_SHADER, "nosuch"), -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // a stage that is not a shader type
    glGetSubroutineIndex(prog, GL_ARRAY_BUFFER, "red");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // the count must match the stage's subroutine uniform locations, which is 1
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // and the value must be an index this stage knows
    idx[0] = 99;
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUseProgram(0);
    glDeleteProgram(prog);
}

/* ---------- the extension has to be advertised to be found ---------- */

GPU_TEST(subroutines, extension_and_limits)
{
    GLint n = 0, found = 0, v = -1;

    glGetIntegerv(GL_NUM_EXTENSIONS, &n);

    for (GLint i = 0; i < n; i++)
    {
        const GLubyte *s = glGetStringi(GL_EXTENSIONS, (GLuint)i);

        if (s && !strcmp((const char *)s, "GL_ARB_shader_subroutine"))
            found = 1;
    }

    CHECK_MSG(found, "GL_ARB_shader_subroutine is not in the extension string");

    glGetIntegerv(GL_MAX_SUBROUTINES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(v >= 256, "GL_MAX_SUBROUTINES reports %d, floor is 256", v);

    glGetIntegerv(GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(v >= 1024, "GL_MAX_SUBROUTINE_UNIFORM_LOCATIONS reports %d, floor is 1024", v);
}

/* ---------- a subroutine in the vertex stage ---------- */

static const char *VS_PICK =
    "#version 400\n"
    "layout(location = 0) in vec2 p;\n"
    "subroutine vec4 place();\n"
    "subroutine uniform place where;\n"
    "subroutine(place) vec4 here()  { return vec4(0, 0, 0, 1); }\n"
    "subroutine(place) vec4 there() { return vec4(1, 1, 0, 1); }\n"
    "void main() { gl_Position = vec4(p, 0.0, 1.0) + where() * 0.0; }\n";

static const char *FS_PLAIN =
    "#version 400\n"
    "out vec4 o;\n"
    "void main() { o = vec4(0, 1, 0, 1); }\n";

GPU_TEST(subroutines, the_vertex_stage_lists_its_own)
{
    GLuint prog;
    GLint v = -1;
    GLuint idx;
    char log[2048];
    char name[64];
    GLsizei len = -1;

    prog = mgl_build_program(VS_PICK, FS_PLAIN, log, sizeof log);

    if (!prog)
    {
        CHECK_MSG(0, "vertex subroutine program did not link: %s", log);
        return;
    }

    glGetProgramStageiv(prog, GL_VERTEX_SHADER, GL_ACTIVE_SUBROUTINES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, 2);

    glGetProgramInterfaceiv(prog, GL_VERTEX_SUBROUTINE, GL_ACTIVE_RESOURCES, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, 2);

    glGetProgramInterfaceiv(prog, GL_VERTEX_SUBROUTINE_UNIFORM, GL_ACTIVE_RESOURCES, &v);
    CHECK_EQ_INT(v, 1);

    glGetProgramInterfaceiv(prog, GL_VERTEX_SUBROUTINE_UNIFORM, GL_MAX_NUM_COMPATIBLE_SUBROUTINES, &v);
    CHECK_EQ_INT(v, 2);

    idx = glGetProgramResourceIndex(prog, GL_VERTEX_SUBROUTINE_UNIFORM, "where");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(idx != GL_INVALID_INDEX, "the vertex stage does not list \"where\"");

    if (idx != GL_INVALID_INDEX)
    {
        GLenum prop = GL_COMPATIBLE_SUBROUTINES;
        GLint compat[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

        name[0] = 0;
        glGetProgramResourceName(prog, GL_VERTEX_SUBROUTINE_UNIFORM, idx, sizeof name, &len, name);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_MSG(!strcmp(name, "where"), "uniform %u is named \"%s\"", idx, name);
        CHECK_EQ_INT(len, 5);

        // the test that used to take the process down read this length back
        // and indexed with it, so it has to be written even on the error path
        len = -1;
        glGetProgramResourceiv(prog, GL_VERTEX_SUBROUTINE_UNIFORM, idx, 1, &prop,
                               8, &len, compat);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(len, 2);
    }

    idx = glGetProgramResourceIndex(prog, GL_VERTEX_SUBROUTINE, "there");
    CHECK_MSG(idx != GL_INVALID_INDEX, "the vertex stage does not list \"there\"");

    glDeleteProgram(prog);
}

/* ---------- every stage keeps its own subroutines ---------- */

static const char *TCS_PICK =
    "#version 400\n"
    "layout(vertices = 3) out;\n"
    "subroutine vec4 place();\n"
    "subroutine uniform place where;\n"
    "subroutine(place) vec4 here() { return vec4(1); }\n"
    "void main() {\n"
    "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position + where() * 0.0;\n"
    "    gl_TessLevelInner[0] = 1.0;\n"
    "    gl_TessLevelInner[1] = 1.0;\n"
    "    gl_TessLevelOuter[0] = 1.0;\n"
    "    gl_TessLevelOuter[1] = 1.0;\n"
    "    gl_TessLevelOuter[2] = 1.0;\n"
    "}\n";

static const char *TES_PICK =
    "#version 400\n"
    "layout(triangles, equal_spacing) in;\n"
    "subroutine vec4 place();\n"
    "subroutine uniform place where;\n"
    "subroutine(place) vec4 here() { return vec4(1); }\n"
    "void main() { gl_Position = gl_in[0].gl_Position + where() * 0.0; }\n";

static const char *GS_PICK =
    "#version 400\n"
    "layout(triangles) in;\n"
    "layout(triangle_strip, max_vertices = 3) out;\n"
    "subroutine vec4 place();\n"
    "subroutine uniform place where;\n"
    "subroutine(place) vec4 here() { return vec4(1); }\n"
    "void main() {\n"
    "    for (int i = 0; i < 3; ++i) {\n"
    "        gl_Position = gl_in[i].gl_Position + where() * 0.0;\n"
    "        EmitVertex();\n"
    "    }\n"
    "    EndPrimitive();\n"
    "}\n";

static const char *FS_PICK_ONE =
    "#version 400\n"
    "out vec4 o;\n"
    "subroutine vec4 place();\n"
    "subroutine uniform place where;\n"
    "subroutine(place) vec4 here() { return vec4(0, 1, 0, 1); }\n"
    "void main() { o = where(); }\n";

GPU_TEST(subroutines, all_five_stages_keep_theirs)
{
    const char *srcs[5] = { VS_PICK, TCS_PICK, TES_PICK, GS_PICK, FS_PICK_ONE };
    const GLenum stages[5] = { GL_VERTEX_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER,
                               GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    const GLenum ifaces[5] = { GL_VERTEX_SUBROUTINE, GL_TESS_CONTROL_SUBROUTINE, GL_TESS_EVALUATION_SUBROUTINE,
                               GL_GEOMETRY_SUBROUTINE, GL_FRAGMENT_SUBROUTINE };
    const GLenum uniform_ifaces[5] = { GL_VERTEX_SUBROUTINE_UNIFORM, GL_TESS_CONTROL_SUBROUTINE_UNIFORM,
                                       GL_TESS_EVALUATION_SUBROUTINE_UNIFORM, GL_GEOMETRY_SUBROUTINE_UNIFORM,
                                       GL_FRAGMENT_SUBROUTINE_UNIFORM };
    // the vertex stage declares two, everything else declares one
    const GLint want[5] = { 2, 1, 1, 1, 1 };
    GLuint prog = glCreateProgram();
    GLint linked = 0;

    for (int i = 0; i < 5; i++)
    {
        GLuint sh = glCreateShader(stages[i]);

        glShaderSource(sh, 1, &srcs[i], NULL);
        glCompileShader(sh);
        glAttachShader(prog, sh);
        glDeleteShader(sh);
    }

    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        char log[2048] = "";

        glGetProgramInfoLog(prog, sizeof log, NULL, log);
        CHECK_MSG(0, "five stage subroutine program did not link: %s", log);
        glDeleteProgram(prog);
        return;
    }

    for (int i = 0; i < 5; i++)
    {
        GLint v = -1;

        glGetProgramInterfaceiv(prog, ifaces[i], GL_ACTIVE_RESOURCES, &v);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_MSG(v == want[i], "stage %d lists %d subroutines, expected %d", i, v, want[i]);

        v = -1;
        glGetProgramInterfaceiv(prog, uniform_ifaces[i], GL_ACTIVE_RESOURCES, &v);
        CHECK_MSG(v == 1, "stage %d lists %d subroutine uniforms, expected 1", i, v);

        CHECK_MSG(glGetProgramResourceIndex(prog, uniform_ifaces[i], "where") != GL_INVALID_INDEX,
                  "stage %d does not list \"where\"", i);
    }

    glDeleteProgram(prog);
}

// A subroutine's program resource index is its subroutine index, including
// one layout(index) sets. The resource queries used the storage position.
GPU_TEST(subroutines, resource_index_is_the_subroutine_index)
{
    static const char *VS =
        "#version 460 core\n"
        "subroutine float st(float x);\n"
        "layout(index = 5) subroutine(st) float twice(float x) { return x * 2.0; }\n"
        "layout(index = 2) subroutine(st) float half_(float x) { return x * 0.5; }\n"
        "subroutine uniform st routine;\n"
        "void main() { gl_Position = vec4(routine(1.0)); }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    char log[2048] = { 0 }, name[32] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    CHECK_EQ_UINT(glGetSubroutineIndex(prog, GL_VERTEX_SHADER, "twice"), 5);
    CHECK_EQ_UINT(glGetProgramResourceIndex(prog, GL_VERTEX_SUBROUTINE, "twice"), 5);
    CHECK_EQ_UINT(glGetProgramResourceIndex(prog, GL_VERTEX_SUBROUTINE, "half_"), 2);

    glGetProgramResourceName(prog, GL_VERTEX_SUBROUTINE, 5, sizeof name, NULL, name);
    CHECK_MSG(!strcmp(name, "twice"), "resource 5 is named \"%s\"", name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(prog);
}

// A function of another subroutine type cannot go in a uniform, a stage the
// program lacks has no subroutine state, and an unlinked program has no
// locations. All three were let through.
GPU_TEST(subroutines, misuse_is_an_invalid_operation)
{
    static const char *VS =
        "#version 460 core\n"
        "subroutine float st(float x);\n"
        "subroutine float other(float x);\n"
        "subroutine(st) float twice(float x) { return x * 2.0; }\n"
        "subroutine(other) float half_(float x) { return x * 0.5; }\n"
        "subroutine uniform st routine;\n"
        "subroutine uniform other routine2;\n"
        "void main() { gl_Position = vec4(routine(1.0) + routine2(1.0)); }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log), idx[2], v = 0, bare;
    GLint l0, l1;

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glUseProgram(prog);
    l0 = glGetSubroutineUniformLocation(prog, GL_VERTEX_SHADER, "routine");
    l1 = glGetSubroutineUniformLocation(prog, GL_VERTEX_SHADER, "routine2");
    CHECK_MSG(l0 >= 0 && l1 >= 0 && l0 != l1, "locations %d and %d", l0, l1);
    if (l0 < 0 || l1 < 0 || l0 > 1 || l1 > 1) return;

    idx[l0] = glGetSubroutineIndex(prog, GL_VERTEX_SHADER, "half_");
    idx[l1] = glGetSubroutineIndex(prog, GL_VERTEX_SHADER, "twice");
    glUniformSubroutinesuiv(GL_VERTEX_SHADER, 2, idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    idx[l0] = glGetSubroutineIndex(prog, GL_VERTEX_SHADER, "twice");
    idx[l1] = glGetSubroutineIndex(prog, GL_VERTEX_SHADER, "half_");
    glUniformSubroutinesuiv(GL_VERTEX_SHADER, 2, idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetUniformSubroutineuiv(GL_GEOMETRY_SHADER, 0, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    bare = glCreateProgram();
    glGetSubroutineUniformLocation(bare, GL_VERTEX_SHADER, "routine");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUseProgram(0);
    glDeleteProgram(bare);
    glDeleteProgram(prog);
}

// An array of arrays of subroutine uniforms is laid out row by row, and
// glGetSubroutineUniformLocation agrees with the resource query. It read
// only the first subscript.
GPU_TEST(subroutines, arrays_of_arrays_take_a_location_each)
{
    static const char *VS =
        "#version 460 core\n"
        "subroutine float st(float x);\n"
        "subroutine(st) float twice(float x) { return x * 2.0; }\n"
        "subroutine uniform st u1[2][3];\n"
        "void main() { gl_Position = vec4(u1[1][2](1.0)); }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(1); }\n";
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    GLint base, loc;

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    base = glGetSubroutineUniformLocation(prog, GL_VERTEX_SHADER, "u1[0][0]");
    loc = glGetSubroutineUniformLocation(prog, GL_VERTEX_SHADER, "u1[0][1]");
    CHECK_EQ_INT(loc, base + 1);
    loc = glGetSubroutineUniformLocation(prog, GL_VERTEX_SHADER, "u1[1][2]");
    CHECK_EQ_INT(loc, base + 5);
    CHECK_EQ_INT(glGetProgramResourceLocation(prog, GL_VERTEX_SUBROUTINE_UNIFORM, "u1[1][2]"), loc);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(prog);
}

// A vertex and a fragment stage may each have a subroutine uniform of the same
// name, and each keeps its own selection. Both became one plain uniform, so
// setting one stage set the other.
GPU_TEST(subroutines, each_stage_keeps_its_own_selection)
{
    static const char *VS2 =
        "#version 400\n"
        "layout(location = 0) in vec2 p;\n"
        "subroutine float level();\n"
        "subroutine uniform level which;\n"
        "subroutine(level) float low()  { return 0.0; }\n"
        "subroutine(level) float high() { return 1.0; }\n"
        "out float v;\n"
        "void main() { v = which(); gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *FS2 =
        "#version 400\n"
        "subroutine float level();\n"
        "subroutine uniform level which;\n"
        "subroutine(level) float low()  { return 0.0; }\n"
        "subroutine(level) float high() { return 1.0; }\n"
        "in float v;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(v, which(), 0.0, 1.0); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo, idx;
    char log[2048];
    unsigned char c[4] = { 0, 0, 0, 0 };

    prog = mgl_build_program(VS2, FS2, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);

    if (!prog || !mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = mgl_fullscreen_quad(&vbo);
    glViewport(0, 0, 32, 32);

    // the vertex stage picks high, the fragment stage low
    idx = glGetSubroutineIndex(prog, GL_VERTEX_SHADER, "high");
    glUniformSubroutinesuiv(GL_VERTEX_SHADER, 1, &idx);
    idx = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "low");
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    unsigned char *px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, 16, 16, c);
        free(px);
    }
    CHECK_MSG(c[0] > 250 && c[1] < 5, "got %u %u %u, want 255 0 0", c[0], c[1], c[2]);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// The geometry stage's selection reaches it, in a program of its own and
// through a pipeline. In a pipeline it was dropped.
static GLuint stageProgram(GLenum type, const char *src)
{
    return glCreateShaderProgramv(type, 1, &src);
}

GPU_TEST(subroutines, geometry_stage_selection_reaches_a_pipeline)
{
    static const char *VSP =
        "#version 410\n"
        "layout(location = 0) in vec2 p;\n"
        "out gl_PerVertex { vec4 gl_Position; };\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *GSP =
        "#version 410\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "in gl_PerVertex { vec4 gl_Position; } gl_in[];\n"
        "out gl_PerVertex { vec4 gl_Position; };\n"
        "subroutine float level();\n"
        "subroutine uniform level which;\n"
        "subroutine(level) float low()  { return 0.0; }\n"
        "subroutine(level) float high() { return 1.0; }\n"
        "out float g;\n"
        "void main() {\n"
        "  for (int i = 0; i < 3; i++) { g = which(); gl_Position = gl_in[i].gl_Position; EmitVertex(); }\n"
        "  EndPrimitive();\n"
        "}\n";
    static const char *FSP =
        "#version 410\n"
        "in float g;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(g, 0.0, 0.0, 1.0); }\n";
    MGLTestTarget t;
    GLuint vs, gs, fs, pipe, vao, vbo, idx;
    unsigned char c[4] = { 0, 0, 0, 0 };

    vs = stageProgram(GL_VERTEX_SHADER, VSP);
    gs = stageProgram(GL_GEOMETRY_SHADER, GSP);
    fs = stageProgram(GL_FRAGMENT_SHADER, FSP);

    if (!mgl_target_create(&t, 32, 32, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }

    glGenProgramPipelines(1, &pipe);
    glUseProgramStages(pipe, GL_VERTEX_SHADER_BIT, vs);
    glUseProgramStages(pipe, GL_GEOMETRY_SHADER_BIT, gs);
    glUseProgramStages(pipe, GL_FRAGMENT_SHADER_BIT, fs);
    glUseProgram(0);
    glBindProgramPipeline(pipe);

    mgl_target_bind(&t);
    vao = mgl_fullscreen_quad(&vbo);
    glViewport(0, 0, 32, 32);

    idx = glGetSubroutineIndex(gs, GL_GEOMETRY_SHADER, "high");
    glUniformSubroutinesuiv(GL_GEOMETRY_SHADER, 1, &idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    unsigned char *px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, 16, 16, c);
        free(px);
    }
    CHECK_MSG(c[0] > 250, "got %u %u %u, want 255 0 0", c[0], c[1], c[2]);

    glBindProgramPipeline(0);
    glDeleteProgramPipelines(1, &pipe);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(vs);
    glDeleteProgram(gs);
    glDeleteProgram(fs);
    mgl_target_destroy(&t);
}

// Linking, and making a program current, leave every subroutine uniform on a
// function of its own type. Every location read 0, which is not one for the
// second type.
GPU_TEST(subroutines, linking_picks_a_compatible_function)
{
    static const char *FS3 =
        "#version 400\n"
        "subroutine vec4 st1(vec4 a);\n"
        "subroutine vec4 st2(vec4 a);\n"
        "subroutine(st1) vec4 a1(vec4 a) { return a; }\n"
        "subroutine(st1) vec4 b1(vec4 a) { return a * 2.0; }\n"
        "subroutine(st2) vec4 a2(vec4 a) { return a + 1.0; }\n"
        "subroutine(st2) vec4 b2(vec4 a) { return a - 1.0; }\n"
        "subroutine uniform st1 one[2];\n"
        "subroutine uniform st2 two[2];\n"
        "out vec4 o;\n"
        "void main() { o = one[0](vec4(0)) + one[1](vec4(0)) + two[0](vec4(0)) + two[1](vec4(0)); }\n";
    char log[2048];
    GLuint prog = mgl_build_program(VS, FS3, log, sizeof log);
    GLuint v[4] = { 99, 99, 99, 99 };

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glUseProgram(prog);
    for (int l = 0; l < 4; l++)
        glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, l, &v[l]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLuint a1 = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "a1"), b1 = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "b1");
    GLuint a2 = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "a2"), b2 = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "b2");
    GLint l1 = glGetSubroutineUniformLocation(prog, GL_FRAGMENT_SHADER, "one");
    GLint l2 = glGetSubroutineUniformLocation(prog, GL_FRAGMENT_SHADER, "two");

    for (int e = 0; e < 2; e++)
    {
        CHECK_MSG(v[l1 + e] == a1 || v[l1 + e] == b1, "one[%d] holds %u", e, v[l1 + e]);
        CHECK_MSG(v[l2 + e] == a2 || v[l2 + e] == b2, "two[%d] holds %u", e, v[l2 + e]);
    }

    // a choice made, then the program made current again: back to a default
    GLuint first[4];
    for (int l = 0; l < 4; l++)
        first[l] = v[l];
    GLuint pick[4];
    pick[l1] = first[l1] == a1 ? b1 : a1; pick[l1 + 1] = pick[l1];
    pick[l2] = first[l2] == a2 ? b2 : a2; pick[l2 + 1] = pick[l2];
    glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 4, pick);
    glUseProgram(prog);
    for (int l = 0; l < 4; l++)
    {
        glGetUniformSubroutineuiv(GL_FRAGMENT_SHADER, l, &v[l]);
        CHECK_MSG(v[l] == first[l], "location %d holds %u after glUseProgram, want %u", l, v[l], first[l]);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(subroutines, geometry_stage_starts_on_a_compatible_function)
{
    static const char *VS4 =
        "#version 400\nlayout(location = 0) in vec2 p;\nvoid main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *GS4 =
        "#version 400\n"
        "layout(points) in;\nlayout(points, max_vertices = 1) out;\n"
        "subroutine vec4 st1(vec4 a);\nsubroutine vec4 st2(vec4 a);\n"
        "subroutine(st1) vec4 a1(vec4 a) { return a; }\n"
        "subroutine(st2) vec4 a2(vec4 a) { return a + 1.0; }\n"
        "subroutine uniform st1 one;\nsubroutine uniform st2 two;\n"
        "void main() { gl_Position = one(gl_in[0].gl_Position) + two(vec4(0)); EmitVertex(); }\n";
    static const char *FS4 = "#version 400\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    const char *src[3] = { VS4, GS4, FS4 };
    GLenum kinds[3] = { GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER };
    GLuint prog = glCreateProgram(), s[3];
    GLint ok = 0;

    for (int i = 0; i < 3; i++)
    {
        s[i] = glCreateShader(kinds[i]);
        glShaderSource(s[i], 1, &src[i], NULL);
        glCompileShader(s[i]);
        glAttachShader(prog, s[i]);
    }
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok);
    glUseProgram(prog);

    GLint l2 = glGetSubroutineUniformLocation(prog, GL_GEOMETRY_SHADER, "two");
    GLuint v = 99, a2 = glGetSubroutineIndex(prog, GL_GEOMETRY_SHADER, "a2");

    glGetUniformSubroutineuiv(GL_GEOMETRY_SHADER, l2, &v);
    CHECK_MSG(v == a2, "two holds %u, a2 is %u", v, a2);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    for (int i = 0; i < 3; i++)
        glDeleteShader(s[i]);
    glDeleteProgram(prog);
}
