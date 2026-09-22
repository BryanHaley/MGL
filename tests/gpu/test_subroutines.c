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
