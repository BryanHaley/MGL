/*
 * test_buffer_uniform.c
 * MGL
 *
 * Buffer mapping including the DSA forms, and uniform introspection. All of
 * this used to abort on call.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- buffer mapping ---------- */

GPU_TEST(buffer, map_range_write_and_read_back)
{
    GLuint b = 0;
    float src[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float got[4] = { 0 };
    void *p;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof src, src, GL_STATIC_DRAW);

    p = glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof src, GL_MAP_READ_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    if (!p) { glDeleteBuffers(1, &b); SKIP("map returned null"); }

    memcpy(got, p, sizeof got);
    glUnmapBuffer(GL_ARRAY_BUFFER);

    for (int i = 0; i < 4; i++)
        CHECK_NEAR(got[i], src[i], 0.0);

    glDeleteBuffers(1, &b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(buffer, map_rejects_bad_range)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);

    glMapBufferRange(GL_ARRAY_BUFFER, -1, 16, GL_MAP_READ_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glMapBufferRange(GL_ARRAY_BUFFER, 0, 4096, GL_MAP_READ_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
}

GPU_TEST(dsa_buffer, map_named_buffer_range)
{
    GLuint b = 0;
    float src[4] = { 5.0f, 6.0f, 7.0f, 8.0f };
    float got[4] = { 0 };
    void *p;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof src, src, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // no binding needed for the DSA form
    p = glMapNamedBufferRange(b, 0, sizeof src, GL_MAP_READ_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    if (!p) { glDeleteBuffers(1, &b); SKIP("named map returned null"); }

    memcpy(got, p, sizeof got);
    CHECK(glUnmapNamedBuffer(b) == GL_TRUE);

    for (int i = 0; i < 4; i++)
        CHECK_NEAR(got[i], src[i], 0.0);

    glDeleteBuffers(1, &b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(dsa_buffer, get_named_buffer_sub_data)
{
    GLuint b = 0;
    int src[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int got[4] = { 0 };

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof src, src, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGetNamedBufferSubData(b, 4 * sizeof(int), 4 * sizeof(int), got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 4; i++)
        CHECK_EQ_INT(got[i], 4 + i);

    glDeleteBuffers(1, &b);
}

GPU_TEST(dsa_buffer, named_calls_reject_unknown_names)
{
    void *p = NULL;
    int dummy = 0;

    glMapNamedBufferRange(9999, 0, 4, GL_MAP_READ_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetNamedBufferSubData(9999, 0, 4, &dummy);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetNamedBufferPointerv(9999, GL_BUFFER_MAP_POINTER, &p);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(dsa_buffer, get_named_buffer_pointer_is_null_when_unmapped)
{
    GLuint b = 0;
    void *p = (void *)0x1234;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);

    glGetNamedBufferPointerv(b, GL_BUFFER_MAP_POINTER, &p);
    CHECK(p == NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer, invalidate_is_accepted)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);

    glInvalidateBufferData(b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glInvalidateBufferSubData(b, 0, 32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glInvalidateBufferSubData(b, 0, 4096);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
}

/* ---------- uniform introspection ---------- */

static const char *VS_UNI =
"#version 460 core\n"
"layout(location = 0) in vec2 pos;\n"
"layout(location = 0) uniform mat4 u_mvp;\n"
"void main() { gl_Position = u_mvp * vec4(pos, 0.0, 1.0); }\n";

static const char *FS_UNI =
"#version 460 core\n"
"layout(location = 3) uniform vec4 u_tint;\n"
"layout(location = 0) out vec4 frag;\n"
"void main() { frag = u_tint; }\n";

GPU_TEST(uniform_query, indices_and_names_roundtrip)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS_UNI, FS_UNI, log, sizeof log);
    const char *names[2] = { "u_mvp", "u_tint" };
    GLuint idx[2] = { 0xFFFF, 0xFFFF };

    if (!prog) SKIP("shader failed to build");

    glGetUniformIndices(prog, 2, names, idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG(idx[0] != GL_INVALID_INDEX, "u_mvp not found");
    CHECK_MSG(idx[1] != GL_INVALID_INDEX, "u_tint not found");
    CHECK_MSG(idx[0] != idx[1], "both uniforms got index %u", idx[0]);

    for (int i = 0; i < 2; i++)
    {
        char buf[64] = { 0 };
        GLsizei len = 0;

        glGetActiveUniformName(prog, idx[i], sizeof buf, &len, buf);
        CHECK_STR_EQ(buf, names[i]);
        CHECK_EQ_INT(len, (GLsizei)strlen(names[i]));
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, unknown_name_gives_invalid_index)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS_UNI, FS_UNI, log, sizeof log);
    const char *names[1] = { "not_a_uniform" };
    GLuint idx[1] = { 0 };

    if (!prog) SKIP("shader failed to build");

    glGetUniformIndices(prog, 1, names, idx);
    CHECK_EQ_UINT(idx[0], GL_INVALID_INDEX);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, active_uniformsiv_reports_name_length)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS_UNI, FS_UNI, log, sizeof log);
    const char *names[1] = { "u_tint" };
    GLuint idx[1] = { 0 };
    GLint v = 0;

    if (!prog) SKIP("shader failed to build");

    glGetUniformIndices(prog, 1, names, idx);
    if (idx[0] == GL_INVALID_INDEX) { glDeleteProgram(prog); SKIP("u_tint not found"); }

    glGetActiveUniformsiv(prog, 1, idx, GL_UNIFORM_NAME_LENGTH, &v);
    CHECK_EQ_INT(v, (GLint)strlen("u_tint") + 1);

    glGetActiveUniformsiv(prog, 1, idx, GL_UNIFORM_SIZE, &v);
    CHECK_EQ_INT(v, 1);

    // these are in the default block, so the block index is -1
    glGetActiveUniformsiv(prog, 1, idx, GL_UNIFORM_BLOCK_INDEX, &v);
    CHECK_EQ_INT(v, -1);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, out_of_range_index_errors)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS_UNI, FS_UNI, log, sizeof log);
    char buf[32] = { 0 };

    if (!prog) SKIP("shader failed to build");

    glGetActiveUniformName(prog, 9999, sizeof buf, NULL, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, get_uniform_reads_back_what_was_set)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS_UNI, FS_UNI, log, sizeof log);
    GLint loc;
    GLfloat got[4] = { 0 };

    if (!prog) SKIP("shader failed to build");

    glUseProgram(prog);
    loc = glGetUniformLocation(prog, "u_tint");
    if (loc < 0) { glDeleteProgram(prog); SKIP("u_tint has no location"); }

    glUniform4f(loc, 0.25f, 0.5f, 0.75f, 1.0f);
    glGetUniformfv(prog, loc, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_NEAR(got[0], 0.25, 1e-6);
    CHECK_NEAR(got[1], 0.5, 1e-6);
    CHECK_NEAR(got[2], 0.75, 1e-6);
    CHECK_NEAR(got[3], 1.0, 1e-6);

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(uniform_query, block_index_of_unknown_name)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_program(VS_UNI, FS_UNI, log, sizeof log);

    if (!prog) SKIP("shader failed to build");

    CHECK_EQ_UINT(glGetUniformBlockIndex(prog, "no_such_block"), GL_INVALID_INDEX);
    mgl_drain_errors();

    glDeleteProgram(prog);
}

/* ---------- texture level queries ---------- */

GPU_TEST(tex_level, reports_size_and_format)
{
    GLuint tex = 0;
    GLint w = 0, h = 0, fmt = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 40, 24);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT(w, 40);
    CHECK_EQ_INT(h, 24);
    CHECK_EQ_UINT(fmt, GL_RGBA8);

    glDeleteTextures(1, &tex);
}

GPU_TEST(tex_level, float_form_agrees_with_int_form)
{
    GLuint tex = 0;
    GLint wi = 0;
    GLfloat wf = 0.0f;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 32, 32);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &wi);
    glGetTexLevelParameterfv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &wf);

    CHECK_NEAR(wf, (double)wi, 0.0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
}

GPU_TEST(tex_level, bad_level_and_pname_error)
{
    GLuint tex = 0;
    GLint v = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, -1, GL_TEXTURE_WIDTH, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 99, GL_TEXTURE_WIDTH, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &tex);
}

GPU_TEST(tex_param, bad_enum_errors_instead_of_aborting)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);

    // a nonsense value for a known pname
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &tex);
}
