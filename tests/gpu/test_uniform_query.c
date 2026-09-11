/*
 * test_uniform_query.c
 * MGL
 *
 * Uniform introspection: glGetActiveUniform, glGetUniform*iv,
 * glGetnUniform*iv, uniform block queries and the subroutine uniform family.
 */

#include "mgl_test.h"
#include "harness.h"
#include <string.h>

static const char *VS_INT =
    "#version 460 core\n"
    "layout(location = 0) uniform int u_count;\n"
    "layout(location = 1) uniform uint u_mask;\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *VS_BLOCK =
    "#version 460 core\n"
    "layout(std140, binding = 0) uniform TestBlock {\n"
    "    vec4 color;\n"
    "    float intensity;\n"
    "};\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 0) out vec4 v_col;\n"
    "void main() { v_col = color * intensity; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_FLAT =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(1.0); }\n";

/* ---------- glGetActiveUniform ---------- */

GPU_TEST(uniform_query, get_active_uniform_reports_name_type_size)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    char name[64] = { 0 };
    GLsizei len = 0;
    GLint size = 0;
    GLenum type = 0;

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    // Per GL 4.6 § 7.3.1: glGetActiveUniform writes name, length, size and
    // type for the uniform at the given index. MGL's implementation is a stub
    // that only sets GL_INVALID_OPERATION instead.
    glGetActiveUniform(prog, 0, sizeof name, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(size, 1);
    CHECK_MSG(len > 0, "uniform name length must be > 0");

    // Out of range index
    glGetActiveUniform(prog, 9999, sizeof name, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Negative bufSize
    glGetActiveUniform(prog, 0, -1, &len, &size, &type, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(prog);
}

/* ---------- glGetUniformiv / glGetUniformuiv ---------- */

GPU_TEST(uniform_query, get_uniformiv_reads_back_integers)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint loc_count, loc_mask;
    GLint iv[4] = { -1, -1, -1, -1 };
    GLuint uv[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    glUseProgram(prog);
    loc_count = glGetUniformLocation(prog, "u_count");
    loc_mask = glGetUniformLocation(prog, "u_mask");

    if (loc_count < 0 || loc_mask < 0)
    {
        glDeleteProgram(prog);
        SKIP("uniforms have no locations");
    }

    // Set and read back a signed int uniform
    glUniform1i(loc_count, 42);
    glGetUniformiv(prog, loc_count, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(iv[0], 42);

    // Set and read back an unsigned int uniform
    glUniform1ui(loc_mask, 0xDEADu);
    glGetUniformuiv(prog, loc_mask, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(uv[0], 0xDEADu);

    // The int readback on a uint location still copies the stored bits
    memset(iv, 0, sizeof iv);
    glGetUniformiv(prog, loc_mask, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT((GLuint)iv[0], 0xDEADu);

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, get_uniformiv_rejects_bad_args)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint iv[4] = { 0 };

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    // NULL params
    glGetUniformiv(prog, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetUniformuiv(prog, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Negative location
    glGetUniformiv(prog, -1, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // Non-existent program
    glGetUniformiv(999123, 0, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(prog);
}

/* ---------- glGetnUniformiv / glGetnUniformuiv ---------- */

GPU_TEST(uniform_query, getn_uniform_checks_buffer_size)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint loc;
    GLint iv[4] = { 0 };

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "u_count");
    if (loc < 0) { glDeleteProgram(prog); SKIP("u_count has no location"); }

    glUniform1i(loc, 7);

    // Enough space: one GLint fits
    glGetnUniformiv(prog, loc, sizeof iv, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(iv[0], 7);

    // Not enough: 2 bytes < 4 bytes for one int
    glGetnUniformiv(prog, loc, 2, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // Negative bufSize
    glGetnUniformiv(prog, loc, -1, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // NULL params
    glGetnUniformiv(prog, loc, sizeof iv, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, getn_uniformuiv_rejects_short_buffer)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint loc;
    GLuint uv[4] = { 0 };

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "u_mask");
    if (loc < 0) { glDeleteProgram(prog); SKIP("u_mask has no location"); }

    glUniform1ui(loc, 0xFFu);

    // Enough space
    glGetnUniformuiv(prog, loc, sizeof uv, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(uv[0], 0xFFu);

    // Not enough: 1 byte < 4 bytes for one uint
    glGetnUniformuiv(prog, loc, 1, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUseProgram(0);
    glDeleteProgram(prog);
}

/* ---------- uniform block queries ---------- */

GPU_TEST(uniform_query, active_uniform_block_name_and_properties)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_BLOCK, FS_FLAT, err, sizeof err);
    GLint nblocks = 0;
    GLint val = -1;
    char name[64] = { 0 };
    GLsizei len = 0;

    if (!prog) { SKIP("UBO shader did not compile"); return; }

    glGetProgramiv(prog, GL_ACTIVE_UNIFORM_BLOCKS, &nblocks);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    if (nblocks == 0) { glDeleteProgram(prog); SKIP("no uniform blocks reflected"); return; }

    // Query the block name
    glGetActiveUniformBlockName(prog, 0, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(len > 0, "block name length must be > 0");

    // Query the binding point (set in the shader to 0)
    glGetActiveUniformBlockiv(prog, 0, GL_UNIFORM_BLOCK_BINDING, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(val, 0);

    // Query the name length
    glGetActiveUniformBlockiv(prog, 0, GL_UNIFORM_BLOCK_NAME_LENGTH, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(val > 0, "block name length reported as %d", val);

    // Out of range index
    glGetActiveUniformBlockiv(prog, 9999, GL_UNIFORM_BLOCK_BINDING, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetActiveUniformBlockName(prog, 9999, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Bad pname
    glGetActiveUniformBlockiv(prog, 0, 0x9999, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, active_uniform_block_errors)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLint val = -1;
    char name[64] = { 0 };
    GLsizei len = 0;

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    // No uniform blocks in this program, so any index is out of range
    glGetActiveUniformBlockiv(prog, 0, GL_UNIFORM_BLOCK_BINDING, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetActiveUniformBlockName(prog, 0, sizeof name, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Invalid program
    glGetActiveUniformBlockiv(999123, 0, GL_UNIFORM_BLOCK_BINDING, &val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // NULL params
    glGetActiveUniformBlockiv(prog, 0, GL_UNIFORM_BLOCK_BINDING, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Negative bufSize
    glGetActiveUniformBlockName(prog, 0, -1, &len, name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(prog);
}

/* ---------- subroutine uniform family ---------- */

GPU_TEST(uniform_query, subroutine_uniforms_return_no_active)
{
    char err[1024] = { 0 };
    GLuint prog = mgl_build_program(VS_INT, FS_FLAT, err, sizeof err);
    GLuint uv = 0;

    CHECK_MSG(prog != 0, "link failed: %s", err);
    if (!prog) return;

    glUseProgram(prog);

    // MGL captures no subroutines, so any index is out of range.
    // Per GL 4.6 § 7.3.1.1: GL_INVALID_VALUE when index >= count.
    glGetActiveSubroutineUniformiv(prog, GL_VERTEX_SHADER, 0,
                                   GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS,
                                   &(GLint){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetActiveSubroutineUniformName(prog, GL_VERTEX_SHADER, 0, 64, NULL,
                                     (char[64]){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // glGetUniformSubroutineuiv: any location is out of range
    glGetUniformSubroutineuiv(GL_VERTEX_SHADER, 0, &uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUseProgram(0);

    // No program bound → GL_INVALID_OPERATION
    glGetUniformSubroutineuiv(GL_VERTEX_SHADER, 0, &uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // NULL params → GL_INVALID_VALUE (checked before the program check)
    glGetUniformSubroutineuiv(GL_VERTEX_SHADER, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // Bad shader type for the subroutine queries
    glGetActiveSubroutineUniformiv(prog, 0x9999, 0,
                                   GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS,
                                   &(GLint){0});
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(prog);
}
