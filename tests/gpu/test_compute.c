/*
 * test_compute.c
 * MGL
 *
 * glDispatchCompute, end to end: the shader has to write memory the
 * application can read back.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

static const char *WRITE_INDEX_CS =
"#version 460 core\n"
"layout(local_size_x = 64) in;\n"
"layout(std430, binding = 0) buffer Out { uint v[]; };\n"
"void main() { uint i = gl_GlobalInvocationID.x; v[i] = i + 1000u; }\n";

static const char *SUM_CS =
"#version 460 core\n"
"layout(local_size_x = 16, local_size_y = 4) in;\n"
"layout(std430, binding = 0) buffer A { uint a[]; };\n"
"layout(std430, binding = 1) buffer B { uint b[]; };\n"
"void main() {\n"
"    uint i = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x\n"
"           + gl_GlobalInvocationID.x;\n"
"    b[i] = a[i] * 2u;\n"
"}\n";

static GLuint compute_program(const char *src)
{
    GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok)
        return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, sh);
    glLinkProgram(prog);
    glDeleteShader(sh);

    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
        return 0;

    return prog;
}

/* the shape that was broken: dispatch counts workgroups, not threads */
GPU_TEST(compute, dispatch_writes_every_element)
{
    GLuint prog = compute_program(WRITE_INDEX_CS);
    CHECK(prog != 0);
    if (!prog) return;

    const GLuint count = 1024;              /* 16 workgroups of 64 */
    GLuint *zero = calloc(count, sizeof(GLuint));
    GLuint ssbo = 0;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(GLuint), zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    glDispatchCompute(count / 64, 1, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();

    GLuint *got = calloc(count, sizeof(GLuint));
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(GLuint), got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    GLuint wrong = 0;
    for (GLuint i = 0; i < count; i++)
        if (got[i] != i + 1000)
            wrong++;

    CHECK_EQ_INT(0, (int)wrong);

    free(zero);
    free(got);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
}

/* small buffers used to keep a CPU copy the GPU never saw */
GPU_TEST(compute, dispatch_writes_a_small_buffer)
{
    GLuint prog = compute_program(WRITE_INDEX_CS);
    CHECK(prog != 0);
    if (!prog) return;

    const GLuint count = 64;                /* 256 bytes, well under a page */
    GLuint zero[64] = {0};
    GLuint got[64] = {0};
    GLuint ssbo = 0;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    for (GLuint i = 0; i < count; i++)
        CHECK_EQ_INT((int)(i + 1000), (int)got[i]);

    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
}

/* two bindings, and a 2D local size */
GPU_TEST(compute, two_storage_buffers)
{
    GLuint prog = compute_program(SUM_CS);
    CHECK(prog != 0);
    if (!prog) return;

    const GLuint count = 256;               /* 4 x 1 groups of 16 x 4 */
    GLuint src[256], dst[256] = {0}, got[256] = {0};
    for (GLuint i = 0; i < count; i++)
        src[i] = i;

    GLuint bufs[2] = {0, 0};
    glGenBuffers(2, bufs);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof src, src, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof dst, dst, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[1]);

    glUseProgram(prog);
    glDispatchCompute(4, 1, 1);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    GLuint wrong = 0;
    for (GLuint i = 0; i < count; i++)
        if (got[i] != i * 2)
            wrong++;

    CHECK_EQ_INT(0, (int)wrong);

    glDeleteBuffers(2, bufs);
    glDeleteProgram(prog);
}

/* GL_COMPUTE_WORK_GROUP_SIZE has to report what the shader declared */
GPU_TEST(compute, reports_local_work_group_size)
{
    GLuint prog = compute_program(SUM_CS);
    CHECK(prog != 0);
    if (!prog) return;

    GLint size[3] = {0, 0, 0};
    glGetProgramiv(prog, GL_COMPUTE_WORK_GROUP_SIZE, size);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_EQ_INT(16, size[0]);
    CHECK_EQ_INT(4, size[1]);
    CHECK_EQ_INT(1, size[2]);

    glDeleteProgram(prog);
}

/* dispatching without a compute stage is an error, not a crash */
GPU_TEST(compute, dispatch_without_compute_program_errors)
{
    glUseProgram(0);
    glDispatchCompute(1, 1, 1);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, glGetError());
}

/* the dispatch limit is the workgroup count, and it has to be a real number */
GPU_TEST(compute, work_group_count_limit_is_enforced)
{
    GLint limit[3] = {0, 0, 0};
    for (GLuint i = 0; i < 3; i++)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &limit[i]);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    /* 4.6 asks for at least 65535 in every dimension */
    for (GLuint i = 0; i < 3; i++)
        CHECK(limit[i] >= 65535);

    glDispatchCompute((GLuint)limit[0] + 1, 1, 1);
    CHECK_EQ_UINT(GL_INVALID_VALUE, glGetError());
}

/* limits that used to read back as zero because nothing ever wrote them */
GPU_TEST(compute, device_limits_are_real)
{
    GLint v = -1;

    glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &v);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    CHECK(v >= 32768);                     /* the 4.3 minimum */

    v = -1;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &v);
    CHECK(v >= 1024);

    v = -1;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &v);
    CHECK(v >= 8);

    v = -1;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &v);
    CHECK(v >= 8);

    v = -1;
    glGetIntegerv(GL_MAX_SAMPLES, &v);
    CHECK(v >= 4);

    GLint size[3] = {0, 0, 0};
    for (GLuint i = 0; i < 3; i++)
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &size[i]);
    CHECK(size[0] >= 1024);
    CHECK(size[1] >= 1024);
    CHECK(size[2] >= 64);
}

/* a uniform and a storage buffer in one kernel: MSL puts them in an order the
   binding code has to respect, and it used to bind by loop index instead */
GPU_TEST(compute, uniform_and_storage_buffer_together)
{
    static const char *CS =
        "#version 460 core\n"
        "layout(local_size_x = 64) in;\n"
        "layout(std430, binding = 0) buffer Out { uint v[]; };\n"
        "uniform uint base;\n"
        "void main() { uint i = gl_GlobalInvocationID.x; v[i] = i + base; }\n";

    char log[2048] = { 0 };
    GLuint prog = mgl_build_compute_program(CS, log, sizeof log);
    CHECK(prog != 0);
    if (!prog) return;

    const GLuint count = 256;
    GLuint zero[256] = {0}, got[256] = {0};
    GLuint ssbo = 0;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);

    GLint loc = glGetUniformLocation(prog, "base");
    CHECK(loc >= 0);
    glUniform1ui(loc, 500u);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDispatchCompute(count / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    GLuint wrong = 0;
    for (GLuint i = 0; i < count; i++)
        if (got[i] != i + 500)
            wrong++;

    CHECK_EQ_INT(0, (int)wrong);

    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
}

/* a run of dispatches shares one compute encoder; results must still be right */
GPU_TEST(compute, repeated_dispatches)
{
    char log[2048] = { 0 };
    GLuint prog = mgl_build_compute_program(WRITE_INDEX_CS, log, sizeof log);
    CHECK(prog != 0);
    if (!prog) return;

    const GLuint count = 1024;
    GLuint *zero = calloc(count, sizeof(GLuint));
    GLuint *got = calloc(count, sizeof(GLuint));
    GLuint ssbo = 0;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(GLuint), zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    for (int k = 0; k < 5; k++)
    {
        glDispatchCompute(count / 64, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    glFinish();

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(GLuint), got);

    GLuint wrong = 0;
    for (GLuint i = 0; i < count; i++)
        if (got[i] != i + 1000)
            wrong++;

    CHECK_EQ_INT(0, (int)wrong);

    free(zero);
    free(got);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
}

/* compute, then a draw, then compute again -- the encoder has to switch kinds
   twice and both results have to survive */
GPU_TEST(compute, interleaved_with_drawing)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 c;\n"
        "void main() { c = vec4(0.0, 1.0, 0.0, 1.0); }\n";

    MGLTestTarget t;
    char log[2048] = { 0 };

    if (!mgl_target_create(&t, 64, 64, GL_RGBA8, 0)) SKIP("no target");

    GLuint cs = mgl_build_compute_program(WRITE_INDEX_CS, log, sizeof log);
    GLuint gs = mgl_build_program(VS, FS, log, sizeof log);
    if (!cs || !gs) { mgl_target_destroy(&t); SKIP("shader pipeline unavailable"); }

    const GLuint count = 256;
    GLuint zero[256] = {0}, got[256] = {0};
    GLuint ssbo = 0, vbo = 0;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    GLuint vao = mgl_fullscreen_quad(&vbo);

    /* compute */
    glUseProgram(cs);
    glDispatchCompute(count / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    /* draw */
    mgl_target_bind(&t);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gs);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* compute again */
    glUseProgram(cs);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
    glDispatchCompute(count / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();

    unsigned char *px = mgl_read_rgba8(&t);
    if (px)
    {
        unsigned char c[4];
        mgl_pixel_at(px, &t, 32, 32, c);
        CHECK_MSG(c[1] > 200 && c[0] < 60, "draw between dispatches gave %d,%d,%d", c[0], c[1], c[2]);
        free(px);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);

    GLuint wrong = 0;
    for (GLuint i = 0; i < count; i++)
        if (got[i] != i + 1000)
            wrong++;

    CHECK_EQ_INT(0, (int)wrong);

    glDeleteBuffers(1, &ssbo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(cs);
    glDeleteProgram(gs);
    mgl_target_destroy(&t);
}

/* glGetBufferSubData is a blocking read: it has to show what the GPU already
   wrote, without the application asking for glFinish first. MGL used to copy
   its stale CPU side and hand back the seed values. */
GPU_TEST(compute, readback_without_finish_sees_gpu_writes)
{
    GLuint prog = compute_program(WRITE_INDEX_CS);
    CHECK(prog != 0);
    if (!prog) return;

    const GLuint count = 256;
    GLuint *seed = calloc(count, sizeof(GLuint));
    GLuint ssbo = 0;

    for (GLuint i = 0; i < count; i++)
        seed[i] = 0xDEADBEEFu;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(GLuint), seed, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    glDispatchCompute(count / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    /* deliberately no glFinish */

    GLuint *got = calloc(count, sizeof(GLuint));
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(GLuint), got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    GLuint wrong = 0;
    for (GLuint i = 0; i < count; i++)
        if (got[i] != i + 1000)
            wrong++;

    CHECK_EQ_INT(0, (int)wrong);

    free(seed);
    free(got);
    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
}

/* Same rule for a mapped read. */
GPU_TEST(compute, map_read_without_finish_sees_gpu_writes)
{
    GLuint prog = compute_program(WRITE_INDEX_CS);
    CHECK(prog != 0);
    if (!prog) return;

    const GLuint count = 256;
    GLuint *seed = calloc(count, sizeof(GLuint));
    GLuint ssbo = 0;

    for (GLuint i = 0; i < count; i++)
        seed[i] = 0xDEADBEEFu;

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, count * sizeof(GLuint), seed, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    glDispatchCompute(count / 64, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    const GLuint *m = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0,
                                       count * sizeof(GLuint), GL_MAP_READ_BIT);
    CHECK(m != NULL);

    if (m)
    {
        GLuint wrong = 0;

        for (GLuint i = 0; i < count; i++)
            if (m[i] != i + 1000)
                wrong++;

        CHECK_EQ_INT(0, (int)wrong);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    free(seed);
    glUseProgram(0);
    glDeleteBuffers(1, &ssbo);
    glDeleteProgram(prog);
}

// A std140 struct whose Metal size already equals its array stride was still
// wrapped in a padding helper, which then held a zero-length array -- and
// Metal will not compile one of those.
GPU_TEST(compute, a_std140_struct_that_fills_its_stride)
{
    static const char *cs =
        "#version 430 core\n"
        "layout(local_size_x = 1) in;\n"
        "struct S4 { float f[1]; int i[2]; uint ui[3]; bool b[4]; ivec3 iv[5]; bvec2 bv[6]; vec4 v[7]; uvec2 uv[8]; };\n"
        "struct S6 { S4 s4[3]; };\n"
        "layout(std140, binding = 0) buffer Out4 { S4 data[]; } g4;\n"
        "layout(std140, binding = 1) buffer Out6 { S6 data[]; } g6;\n"
        "layout(std430, binding = 2) buffer Len { int len[2]; };\n"
        "void main() {\n"
        "  len[0] = g4.data.length();\n"
        "  len[1] = g6.data.length();\n"
        "  g4.data[1].uv[7] = uvec2(7u, 8u);\n"
        "  g6.data[0].s4[2].v[6] = vec4(9.0);\n"
        "}\n";
    GLuint prog = compute_program(cs);
    GLuint bufs[3];
    GLuint *zero = calloc(1728, 1);
    GLuint u[2];
    GLfloat f[4];
    GLint len[2] = { -1, -1 };

    CHECK_MSG(prog != 0, "std140 struct kernel did not build");

    if (!prog) { free(zero); return; }

    // S4 is 576 bytes in std140, and S6 three of those
    glGenBuffers(3, bufs);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 2 * 576, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[0]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 1728, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[1]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[2]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof len, len, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bufs[2]);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof len, len);
    CHECK_MSG(len[0] == 2 && len[1] == 1, "lengths %d and %d, want 2 and 1", len[0], len[1]);

    // data[1].uv[7] sits 448 + 7 * 16 bytes into the second element
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 576 + 448 + 7 * 16, sizeof u, u);
    CHECK_MSG(u[0] == 7 && u[1] == 8, "uv[7] holds %u %u, want 7 8", u[0], u[1]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 2 * 576 + 336 + 6 * 16, sizeof f, f);
    CHECK_MSG(f[0] == 9.0f && f[3] == 9.0f, "s4[2].v[6] holds %g .. %g, want 9", f[0], f[3]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    free(zero);
    glUseProgram(0);
    glDeleteBuffers(3, bufs);
    glDeleteProgram(prog);
}
