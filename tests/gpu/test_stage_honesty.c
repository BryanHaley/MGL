/*
 * test_stage_honesty.c
 * MGL
 *
 * A program that reports GL_LINK_STATUS true must either draw or have a
 * usable info log.  Geometry and tessellation shaders are accepted by glslang
 * but SPIRV-Cross emits broken MSL, and the Metal compiler error is swallowed
 * — the worst failure mode in the codebase.
 */

#include "mgl_test.h"
#include "harness.h"
#include <string.h>

/* ----- shader sources ----- */

static const char *VS_PASSTHROUGH =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_WHITE =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(1.0); }\n";

static const char *GS_PASSTHROUGH =
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

static const char *TCS_PASSTHROUGH =
    "#version 460 core\n"
    "layout(vertices = 3) out;\n"
    "void main() {\n"
    "    gl_TessLevelOuter[0] = 1.0;\n"
    "    gl_TessLevelOuter[1] = 1.0;\n"
    "    gl_TessLevelOuter[2] = 1.0;\n"
    "    gl_TessLevelInner[0] = 1.0;\n"
    "    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
    "}\n";

static const char *TES_PASSTHROUGH =
    "#version 460 core\n"
    "layout(triangles) in;\n"
    "void main() {\n"
    "    gl_Position = gl_TessCoord.x * gl_in[0].gl_Position +\n"
    "                  gl_TessCoord.y * gl_in[1].gl_Position +\n"
    "                  gl_TessCoord.z * gl_in[2].gl_Position;\n"
    "}\n";

/* glslang accepts this; SPIRV-Cross cannot lower atomic counters to valid
 * MSL for a fragment shader and emits something the Metal compiler rejects. */
static const char *FS_ATOMIC =
    "#version 460 core\n"
    "layout(binding = 0, offset = 0) uniform atomic_uint counter;\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() {\n"
    "    uint c = atomicCounterIncrement(counter);\n"
    "    frag = vec4(0.0, float(c & 1u), 0.0, 1.0);\n"
    "}\n";

/* ----- helpers ----- */

static GLuint compile(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);

    return s;
}

static int compiled(GLuint s)
{
    GLint ok = 0;

    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

    return ok == GL_TRUE;
}

/* Build a program from n shaders.  On return the shaders are detached and
 * deleted.  Returns 0 on link failure; if log is non-NULL and link fails the
 * info log is copied there. */
static GLuint build_program(GLuint *stages, int n, char *log, int log_size)
{
    GLuint p = glCreateProgram();
    GLint ok = 0;

    for (int i = 0; i < n; i++)
        glAttachShader(p, stages[i]);

    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    for (int i = 0; i < n; i++)
    {
        glDetachShader(p, stages[i]);
        glDeleteShader(stages[i]);
    }

    if (!ok)
    {
        if (log && log_size > 0)
            glGetProgramInfoLog(p, log_size, NULL, log);

        glDeleteProgram(p);

        return 0;
    }

    return p;
}

static int centre_is_white(const MGLTestTarget *t)
{
    unsigned char *px = mgl_read_rgba8(t);
    unsigned char c[4];

    if (!px) return 0;

    mgl_pixel_at(px, t, t->width / 2, t->height / 2, c);
    free(px);

    /* expect white: the fragment shader writes (1,1,1,1) */
    return c[0] >= 250 && c[1] >= 250 && c[2] >= 250;
}

/* ----- geometry shader: link and draw, or honest failure ----- */

GPU_TEST(stage_honesty, geometry_shader_reports_or_draws)
{
    MGLTestTarget t;
    GLuint st[3];
    GLuint prog, vao, vbo;
    char log[4096] = { 0 };
    int linked, draws;

    if (!mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }
    mgl_target_bind(&t);

    st[0] = compile(GL_VERTEX_SHADER, VS_PASSTHROUGH);
    st[1] = compile(GL_GEOMETRY_SHADER, GS_PASSTHROUGH);
    st[2] = compile(GL_FRAGMENT_SHADER, FS_WHITE);

    CHECK_MSG(compiled(st[0]), "vertex shader did not compile");
    CHECK_MSG(compiled(st[1]), "geometry shader did not compile");
    CHECK_MSG(compiled(st[2]), "fragment shader did not compile");

    if (!compiled(st[0]) || !compiled(st[1]) || !compiled(st[2]))
    {
        for (int i = 0; i < 3; i++)
            if (st[i]) glDeleteShader(st[i]);
        mgl_target_destroy(&t);
        return;
    }

    prog = build_program(st, 3, log, sizeof log);
    linked = (prog != 0);

    draws = 0;
    if (linked)
    {
        vao = mgl_fullscreen_quad(&vbo);
        glUseProgram(prog);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        draws = centre_is_white(&t);

        glUseProgram(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteProgram(prog);
    }

    mgl_target_destroy(&t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG((linked && draws) || (!linked && log[0] != '\0'),
              "geometry program: linked=%d drew=%d log=\"%s\"",
              linked, draws, log);
}

/* ----- tessellation shader: link and draw, or honest failure ----- */

GPU_TEST(stage_honesty, tessellation_reports_or_draws)
{
    MGLTestTarget t;
    GLuint st[4];
    GLuint prog, vao, vbo;
    char log[4096] = { 0 };
    int linked, draws;

    if (!mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
    {
        CHECK(0);
        return;
    }
    mgl_target_bind(&t);

    st[0] = compile(GL_VERTEX_SHADER, VS_PASSTHROUGH);
    st[1] = compile(GL_TESS_CONTROL_SHADER, TCS_PASSTHROUGH);
    st[2] = compile(GL_TESS_EVALUATION_SHADER, TES_PASSTHROUGH);
    st[3] = compile(GL_FRAGMENT_SHADER, FS_WHITE);

    CHECK_MSG(compiled(st[0]), "vertex shader did not compile");
    CHECK_MSG(compiled(st[1]), "tess control shader did not compile");
    CHECK_MSG(compiled(st[2]), "tess evaluation shader did not compile");
    CHECK_MSG(compiled(st[3]), "fragment shader did not compile");

    if (!compiled(st[0]) || !compiled(st[1]) ||
        !compiled(st[2]) || !compiled(st[3]))
    {
        for (int i = 0; i < 4; i++)
            if (st[i]) glDeleteShader(st[i]);
        mgl_target_destroy(&t);
        return;
    }

    prog = build_program(st, 4, log, sizeof log);
    linked = (prog != 0);

    draws = 0;
    if (linked)
    {
        vao = mgl_fullscreen_quad(&vbo);
        glUseProgram(prog);
        glPatchParameteri(GL_PATCH_VERTICES, 3);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_PATCHES, 0, 3);
        draws = centre_is_white(&t);

        glUseProgram(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteProgram(prog);
    }

    mgl_target_destroy(&t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG((linked && draws) || (!linked && log[0] != '\0'),
              "tessellation program: linked=%d drew=%d log=\"%s\"",
              linked, draws, log);
}

/* ----- shader glslang accepts but SPIRV-Cross cannot lower ----- */

GPU_TEST(stage_honesty, malformed_shader_is_not_silent_success)
{
    GLuint st[2];
    GLuint prog;
    char log[4096] = { 0 };
    int linked;

    st[0] = compile(GL_VERTEX_SHADER, VS_PASSTHROUGH);
    st[1] = compile(GL_FRAGMENT_SHADER, FS_ATOMIC);

    CHECK_MSG(compiled(st[0]), "vertex shader did not compile");
    CHECK_MSG(compiled(st[1]), "atomic fragment shader did not compile");

    if (!compiled(st[0]) || !compiled(st[1]))
    {
        for (int i = 0; i < 2; i++)
            if (st[i]) glDeleteShader(st[i]);
        return;
    }

    prog = build_program(st, 2, log, sizeof log);
    linked = (prog != 0);

    if (linked)
        glDeleteProgram(prog);

    /* The spec (§7.3) says a program that links must be usable.  A shader
     * glslang accepts but SPIRV-Cross cannot lower to valid MSL produces a
     * program that either links and draws, or reports link failure.  MGL must
     * not report success for a program whose Metal library will not compile. */
    CHECK_MSG(!linked || log[0] != '\0',
              "atomic-counter program linked=%d log=\"%s\"",
              linked, log);
}

/* ----- info log is actionable when link fails ----- */

GPU_TEST(stage_honesty, info_log_describes_failure)
{
    GLuint vs, gs, fs;
    GLuint prog;
    GLint ok = -1, log_len = 0;
    char log[4096] = { 0 };

    vs = compile(GL_VERTEX_SHADER, VS_PASSTHROUGH);
    gs = compile(GL_GEOMETRY_SHADER, GS_PASSTHROUGH);
    fs = compile(GL_FRAGMENT_SHADER, FS_WHITE);

    if (!compiled(vs) || !compiled(gs) || !compiled(fs))
    {
        if (vs) glDeleteShader(vs);
        if (gs) glDeleteShader(gs);
        if (fs) glDeleteShader(fs);
        return;
    }

    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, gs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok)
    {
        /* Per GL 4.6 §7.3.1, the info log on link failure must describe the
         * reason.  An empty string is not actionable. */
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);
        CHECK_MSG(log_len > 1,
                  "link failed but GL_INFO_LOG_LENGTH is %d (expected > 1)", log_len);

        glGetProgramInfoLog(prog, sizeof log, NULL, log);
        CHECK_MSG(log[0] != '\0',
                  "link failed but info log is empty");
    }

    glDetachShader(prog, vs);
    glDetachShader(prog, gs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(gs);
    glDeleteShader(fs);
    glDeleteProgram(prog);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}
