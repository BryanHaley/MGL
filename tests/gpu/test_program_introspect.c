/*
 * test_program_introspect.c
 * MGL
 *
 * Attribute/frag-data binding, program introspection, pipeline stages.
 */

#include "mgl_test.h"
#include "harness.h"
#include <string.h>

static const char *VS_POS =
    "#version 460 core\n"
    "layout(location = 2) in vec4 mgl_pos;\n"
    "void main() { gl_Position = mgl_pos; }\n";

static const char *FS_BLUE =
    "#version 460 core\n"
    "layout(location = 1) out vec4 mgl_frag;\n"
    "void main() { mgl_frag = vec4(0.0, 0.0, 1.0, 1.0); }\n";

static const char *VS_EMPTY =
    "#version 460 core\n"
    "void main() { gl_Position = vec4(0.0); }\n";

static const char *FS_EMPTY =
    "#version 460 core\n"
    "void main() { }\n";

/* ---------- attribute binding and query ---------- */

GPU_TEST(prog_introspect, attrib_binding_and_query)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_POS, FS_BLUE, err, sizeof err);
    GLint loc = -2;
    GLint size = -2;
    GLenum type = 0;
    char name[64] = { 0 };
    GLsizei len = 0;

    CHECK_MSG(p != 0, "link failed: %s", err);
    if (!p) return;

    /* glGetAttribLocation — implemented, searches SPIR-V inputs */
    loc = glGetAttribLocation(p, "mgl_pos");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(loc, 2);

    /* unknown name returns -1 with no error */
    loc = glGetAttribLocation(p, "nope");
    CHECK_EQ_INT(loc, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glBindAttribLocation — spec: INVALID_VALUE if index >= MAX_VERTEX_ATTRIBS */
    GLint max_attribs = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attribs);
    glBindAttribLocation(p, (GLuint)max_attribs, "mgl_pos");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* spec: INVALID_OPERATION if name starts with "gl_" */
    glBindAttribLocation(p, 0, "gl_Vertex");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* spec: INVALID_VALUE for a non-program name */
    glBindAttribLocation(999123, 0, "mgl_pos");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* spec: valid call should succeed (MGL sets INVALID_OPERATION — bug) */
    glBindAttribLocation(p, 0, "mgl_pos");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glGetActiveAttrib — spec: returns name, size and type */
    glGetActiveAttrib(p, 0, sizeof name, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_STR_EQ(name, "mgl_pos");
    CHECK_EQ_INT(size, 1);
    CHECK_EQ_UINT(type, GL_FLOAT_VEC4);

    /* index past the end */
    glGetActiveAttrib(p, 999, sizeof name, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* invalid program */
    glGetActiveAttrib(999123, 0, sizeof name, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

/* ---------- fragment data location binding ---------- */

GPU_TEST(prog_introspect, frag_data_binding)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_EMPTY, FS_BLUE, err, sizeof err);
    GLint loc;

    CHECK_MSG(p != 0, "link failed: %s", err);
    if (!p) return;

    /* glGetFragDataLocation returns the output's layout location */
    loc = glGetFragDataLocation(p, "mgl_frag");
    CHECK_EQ_INT(loc, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT(glGetFragDataLocation(p, "nope"), -1);

    /* glBindFragDataLocation — delegates to Indexed with index 0 */
    glBindFragDataLocation(p, 0, "mgl_frag");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* glBindFragDataLocationIndexed */
    glBindFragDataLocationIndexed(p, 0, 0, "mgl_frag");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* spec: index > 1 is INVALID_VALUE (dual-source blending index) */
    glBindFragDataLocationIndexed(p, 0, 2, "mgl_frag");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* spec: colorNumber >= MAX_COLOR_ATTACHMENTS is INVALID_VALUE */
    GLint max_ca = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_ca);
    glBindFragDataLocationIndexed(p, (GLuint)max_ca, 0, "mgl_frag");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* spec: name starting with "gl_" is INVALID_OPERATION */
    glBindFragDataLocationIndexed(p, 0, 0, "gl_FragColor");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* invalid program */
    glBindFragDataLocation(999123, 0, "mgl_frag");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* NULL name */
    glBindFragDataLocation(p, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

/* ---------- glCreateShaderProgramv ---------- */

GPU_TEST(prog_introspect, create_shader_programv)
{
    const char *src = VS_EMPTY;
    GLuint p;
    GLint status = 0;

    p = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &src);
    CHECK_MSG(p != 0, "CreateShaderProgramv returned 0");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetProgramiv(p, GL_LINK_STATUS, &status);
    CHECK_EQ_INT(status, GL_TRUE);

    /* the internal shader was deleted, so GetAttachedShaders returns 0 */
    {
        GLuint shaders[4] = { 0xAA, 0xBB, 0xCC, 0xDD };
        GLsizei count = -1;

        glGetAttachedShaders(p, 4, &count, shaders);
        CHECK_EQ_INT(count, 0);
    }

    glDeleteProgram(p);

    /* invalid type — the spec says GL_INVALID_ENUM */
    p = glCreateShaderProgramv(0x9999, 1, &src);
    CHECK_EQ_UINT(p, 0u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- program pipelines ---------- */

GPU_TEST(prog_introspect, program_pipelines)
{
    const char *src = VS_EMPTY;
    GLuint pp[2] = { 0, 0 };
    GLuint p;
    GLint v = -1;

    /* glCreateProgramPipelines */
    glCreateProgramPipelines(2, pp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(pp[0] != 0 && pp[1] != 0);
    CHECK(pp[0] != pp[1]);

    glBindProgramPipeline(pp[0]);

    /* create a separable single-stage program */
    p = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &src);
    CHECK(p != 0);

    /* glUseProgramStages */
    glUseProgramStages(pp[0], GL_VERTEX_SHADER_BIT, p);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetProgramPipelineiv(pp[0], GL_VERTEX_SHADER, &v);
    CHECK_EQ_INT(v, (GLint)p);

    /* unknown pipeline -> INVALID_OPERATION */
    glUseProgramStages(999123, GL_VERTEX_SHADER_BIT, p);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    /* valid pipeline, unknown program -> INVALID_VALUE */
    glUseProgramStages(pp[0], GL_VERTEX_SHADER_BIT, 999123);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* glGetProgramPipelineInfoLog — returns empty string */
    {
        char log[32] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        GLsizei len = 99;

        glGetProgramPipelineInfoLog(pp[0], sizeof log, &len, log);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(len, 0);
        CHECK_STR_EQ(log, "");

        /* null pipeline name — spec says INVALID_OPERATION */
        glGetProgramPipelineInfoLog(999123, sizeof log, &len, log);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    }

    glBindProgramPipeline(0);
    glDeleteProgramPipelines(2, pp);
    glDeleteProgram(p);
}

/* ---------- get active subroutine name ---------- */

GPU_TEST(prog_introspect, active_subroutine_name)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_EMPTY, FS_EMPTY, err, sizeof err);
    char name[64] = "UNTOUCHED";
    GLsizei len = 99;

    CHECK_MSG(p != 0, "link failed: %s", err);
    if (!p) return;

    /* MGL has no subroutines, so any index should return INVALID_VALUE */
    glGetActiveSubroutineName(p, GL_VERTEX_SHADER, 0, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* invalid program */
    glGetActiveSubroutineName(999123, GL_VERTEX_SHADER, 0, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

/* ---------- get active atomic counter buffer, shader storage block ---------- */

GPU_TEST(prog_introspect, storage_and_atomic)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_EMPTY, FS_EMPTY, err, sizeof err);
    GLint params[4] = { 0xAA, 0xBB, 0xCC, 0xDD };

    CHECK_MSG(p != 0, "link failed: %s", err);
    if (!p) return;

    /* glGetActiveAtomicCounterBufferiv — no atomic counters, so INVALID_VALUE */
    glGetActiveAtomicCounterBufferiv(p, 0, GL_ATOMIC_COUNTER_BUFFER_BINDING, params);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* invalid program */
    glGetActiveAtomicCounterBufferiv(999123, 0, GL_ATOMIC_COUNTER_BUFFER_BINDING, params);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* glShaderStorageBlockBinding — no storage buffers, so index 0 is out of range */
    glShaderStorageBlockBinding(p, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* binding >= MAX_BINDABLE_BUFFERS (16) */
    glShaderStorageBlockBinding(p, 0, 999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* invalid program */
    glShaderStorageBlockBinding(999123, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

/* ---------- program resource location ---------- */

GPU_TEST(prog_introspect, program_resource_location)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_POS, FS_BLUE, err, sizeof err);
    GLint loc;

    CHECK_MSG(p != 0, "link failed: %s", err);
    if (!p) return;

    /* glGetProgramResourceLocation — spec: INVALID_VALUE if program not valid */
    loc = glGetProgramResourceLocation(p, GL_PROGRAM_INPUT, "mgl_pos");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(loc, 2);

    /* unknown name returns -1 */
    loc = glGetProgramResourceLocation(p, GL_PROGRAM_INPUT, "nope");
    CHECK_EQ_INT(loc, -1);

    /* unknown interface -> INVALID_ENUM */
    loc = glGetProgramResourceLocation(p, 0x9999, "mgl_pos");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    /* invalid program -> INVALID_VALUE */
    loc = glGetProgramResourceLocation(999123, GL_PROGRAM_INPUT, "mgl_pos");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    /* glGetProgramResourceLocationIndex — spec: returns location index */
    loc = glGetProgramResourceLocationIndex(p, GL_PROGRAM_OUTPUT, "mgl_frag");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(loc, 1);

    /* unknown name */
    loc = glGetProgramResourceLocationIndex(p, GL_PROGRAM_OUTPUT, "nope");
    CHECK_EQ_INT(loc, -1);

    /* invalid program */
    loc = glGetProgramResourceLocationIndex(999123, GL_PROGRAM_OUTPUT, "mgl_frag");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}
