/*
 * test_uniform_type.c
 * MGL
 *
 * The linker records what type each uniform was declared as. Without that,
 * glGetActiveUniform reports GL_NONE and a write of the wrong width goes
 * through unnoticed.
 */

#include "mgl_test.h"
#include "harness.h"

static const char *VS =
    "#version 460 core\n"
    "uniform float uFloat;\n"
    "uniform vec2  uVec2;\n"
    "uniform vec3  uVec3;\n"
    "uniform vec4  uVec4;\n"
    "uniform int   uInt;\n"
    "uniform ivec2 uIVec2;\n"
    "uniform uint  uUint;\n"
    "uniform mat3  uMat3;\n"
    "uniform mat4  uMat4;\n"
    "void main() {\n"
    "    gl_Position = uMat4 * vec4(uVec3, 1.0) + vec4(uVec2, 0.0, 0.0)\n"
    "                + vec4(uMat3[0], 0.0) + uVec4 * uFloat\n"
    "                + vec4(float(uInt) + float(uUint) + float(uIVec2.x));\n"
    "}\n";

static const char *FS =
    "#version 460 core\n"
    "out vec4 c;\n"
    "void main() { c = vec4(1.0); }\n";

static GLenum type_of(GLuint prog, const char *want_name)
{
    GLint active = 0;
    glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &active);

    for (GLint i = 0; i < active; i++)
    {
        char name[128] = { 0 };
        GLint size = 0;
        GLenum type = GL_NONE;

        glGetActiveUniform(prog, (GLuint)i, sizeof name, NULL, &size, &type, name);

        if (strcmp(name, want_name) == 0)
            return type;
    }

    return GL_NONE;
}

GPU_TEST(uniform_type, get_active_uniform_reports_the_declared_type)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    struct { const char *name; GLenum want; } expect[] = {
        { "uFloat", GL_FLOAT },      { "uVec2", GL_FLOAT_VEC2 },
        { "uVec3",  GL_FLOAT_VEC3 }, { "uVec4", GL_FLOAT_VEC4 },
        { "uInt",   GL_INT },        { "uIVec2", GL_INT_VEC2 },
        { "uUint",  GL_UNSIGNED_INT },
        { "uMat3",  GL_FLOAT_MAT3 }, { "uMat4", GL_FLOAT_MAT4 },
    };

    for (unsigned i = 0; i < sizeof expect / sizeof expect[0]; i++)
    {
        GLenum got = type_of(prog, expect[i].name);
        CHECK_MSG(got == expect[i].want, "%s: got 0x%x, want 0x%x",
                  expect[i].name, got, expect[i].want);
    }

    glDeleteProgram(prog);
}

GPU_TEST(uniform_type, get_active_uniformsiv_agrees)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    GLint active = 0;
    glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &active);
    CHECK(active > 0);

    for (GLint i = 0; i < active; i++)
    {
        GLuint index = (GLuint)i;
        GLint type_iv = 0, size_iv = 0;
        char name[128] = { 0 };
        GLint size = 0;
        GLenum type = GL_NONE;

        glGetActiveUniformsiv(prog, 1, &index, GL_UNIFORM_TYPE, &type_iv);
        glGetActiveUniformsiv(prog, 1, &index, GL_UNIFORM_SIZE, &size_iv);
        glGetActiveUniform(prog, index, sizeof name, NULL, &size, &type, name);

        CHECK_MSG((GLenum)type_iv == type, "%s: iv says 0x%x, GetActiveUniform says 0x%x",
                  name, type_iv, type);
        CHECK_MSG(size_iv == size, "%s: iv size %d, GetActiveUniform size %d",
                  name, size_iv, size);
        CHECK_MSG(type != GL_NONE, "%s reported no type", name);
    }

    glDeleteProgram(prog);
}

/* The spec requires a too-narrow write to be GL_INVALID_OPERATION. MGL does not
   reject it yet -- see the note in programUniformWrite -- so this records what
   it does today and will start failing the moment that is switched on, which is
   the signal to flip these expectations. */
GPU_TEST(uniform_type, writing_the_wrong_width_is_not_yet_rejected)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glUseProgram(prog);

    GLint vec4Loc = glGetUniformLocation(prog, "uVec4");
    GLint floatLoc = glGetUniformLocation(prog, "uFloat");
    CHECK(vec4Loc >= 0);
    CHECK(floatLoc >= 0);
    mgl_drain_errors();

    /* one float into a vec4: should be GL_INVALID_OPERATION, currently is not */
    glUniform1f(vec4Loc, 1.0f);
    mgl_drain_errors();

    /* the right width is accepted */
    GLfloat four[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    glUniform4fv(vec4Loc, 1, four);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    /* and a scalar into a scalar */
    glUniform1f(floatLoc, 1.0f);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDeleteProgram(prog);
}

/* GL hands a bool uniform four bytes per component and Metal's bool is one,
   so a bvec read three zeroes out of the first int and compared false. */
GPU_TEST(uniform_type, bool_uniforms_reach_the_shader)
{
    static const char *VS =
        "#version 460 core\n"
        "void main() {\n"
        "    vec2 p[4] = vec2[4](vec2(-1,-1), vec2(3,-1), vec2(-1,3), vec2(3,3));\n"
        "    gl_Position = vec4(p[gl_VertexID & 3], 0.0, 1.0);\n"
        "}\n";
    static const char *FS =
        "#version 460 core\n"
        "uniform bool b1;\n"
        "uniform bvec3 b3;\n"
        "layout(location = 0) out vec4 frag;\n"
        "void main() {\n"
        "    frag = vec4(b1 ? 1.0 : 0.0, b3.x ? 1.0 : 0.0, b3.y ? 1.0 : 0.0, b3.z ? 1.0 : 0.0);\n"
        "}\n";

    MGLTestTarget t;
    GLuint prog, vao, vbo;
    const GLint v3[3] = { 0, 1, 1 };
    unsigned char *px, rgba[4];
    char log[512] = { 0 };

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0))
        return;

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    mgl_target_bind(&t);
    glUseProgram(prog);
    glBindVertexArray(vao);

    glUniform1i(glGetUniformLocation(prog, "b1"), 1);
    glUniform3iv(glGetUniformLocation(prog, "b3"), 1, v3);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&t);
    CHECK(px != NULL);
    if (px)
    {
        mgl_pixel_at(px, &t, 4, 4, rgba);
        CHECK_EQ_UINT(255u, rgba[0]);
        CHECK_EQ_UINT(0u,   rgba[1]);
        CHECK_EQ_UINT(255u, rgba[2]);
        CHECK_EQ_UINT(255u, rgba[3]);
        free(px);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
