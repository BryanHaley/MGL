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

// glDispatchComputeIndirect reads its group counts from a buffer and runs
// every invocation of every group; it used to do nothing at all
GPU_TEST(compute, dispatch_indirect_runs_every_invocation)
{
    static const char *CS =
        "#version 460\n"
        "layout(local_size_x = 4) in;\n"
        "layout(std430, binding = 0) buffer Out { uint v[]; };\n"
        "void main() { v[gl_GlobalInvocationID.x] = gl_GlobalInvocationID.x + 1u; }\n";
    static const GLuint groups[3] = { 3, 1, 1 };
    char log[1024] = { 0 };
    GLuint prog = mgl_build_compute_program(CS, log, sizeof log);
    GLuint out, args;
    GLuint zero[12] = { 0 };
    GLuint *v;

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGenBuffers(1, &out);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, zero, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, out);

    glGenBuffers(1, &args);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, args);
    glBufferData(GL_DISPATCH_INDIRECT_BUFFER, sizeof groups, groups, GL_STATIC_DRAW);

    glUseProgram(prog);
    glDispatchComputeIndirect(0);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    v = (GLuint *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof zero, GL_MAP_READ_BIT);
    CHECK(v != NULL);
    if (v)
    {
        for (int i = 0; i < 12; i++)
            CHECK_MSG(v[i] == (GLuint)i + 1, "invocation %d wrote %u", i, v[i]);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    glUseProgram(0);
    glDeleteBuffers(1, &out);
    glDeleteBuffers(1, &args);
    glDeleteProgram(prog);
}

// the compute limits come in threes, and every getter form answers them
GPU_TEST(compute, indexed_limits_answer_in_every_form)
{
    GLint i = 0;
    GLint64 i64 = 0;
    GLfloat f = 0;
    GLdouble d = 0;
    GLboolean b = GL_FALSE;

    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &i);
    glGetInteger64i_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &i64);
    glGetFloati_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &f);
    glGetDoublei_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &d);
    glGetBooleani_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK(i >= 65535);
    CHECK(i64 >= 65535);
    CHECK(f >= 65535.0f);
    CHECK(d >= 1024.0);
    CHECK(b == GL_TRUE);

    glGetFloati_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 3, &f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

// a uniform declared with a value starts with it; glslang used to drop it
GPU_TEST(compute, uniform_initializers_are_kept)
{
    static const char *CS =
        "#version 460\n"
        "layout(local_size_x = 1) in;\n"
        "struct S { int a; float b[2]; };\n"
        "uniform uint count = 5u;\n"
        "uniform vec3 v = vec3(1.0, 2.0, 3.0);\n"
        "uniform mat2 m = mat2(4.0, 5.0, 6.0, 7.0);\n"
        "uniform S s = S(8, float[2](9.0, 10.0));\n"
        "uniform float plain;\n"
        "layout(std430, binding = 0) buffer Out { float o[]; };\n"
        "void main() {\n"
        "    o[0] = float(count); o[1] = v.x; o[2] = v.y; o[3] = v.z;\n"
        "    o[4] = m[0][0]; o[5] = m[0][1]; o[6] = m[1][0]; o[7] = m[1][1];\n"
        "    o[8] = float(s.a); o[9] = s.b[0]; o[10] = s.b[1]; o[11] = plain;\n"
        "}\n";
    static const float want[12] = { 5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0 };
    char log[1024] = { 0 };
    GLuint prog = mgl_build_compute_program(CS, log, sizeof log);
    GLfloat zero[12];
    GLuint out, got = 0;
    GLfloat *o;

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    // GL can read the starting value back before anything sets it
    glGetUniformuiv(prog, glGetUniformLocation(prog, "count"), &got);
    CHECK_EQ_UINT(got, 5);

    for (int i = 0; i < 12; i++)
        zero[i] = -1.0f;

    glGenBuffers(1, &out);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, zero, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, out);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    o = (GLfloat *)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof zero, GL_MAP_READ_BIT);
    CHECK(o != NULL);
    if (o)
    {
        for (int i = 0; i < 12; i++)
            CHECK_MSG(o[i] == want[i], "o[%d] = %g, want %g", i, o[i], want[i]);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    glUseProgram(0);
    glDeleteBuffers(1, &out);
    glDeleteProgram(prog);
}

// more plain uniforms than there are vertex attributes; the binding loop used
// to give up after 16 and the dispatch never ran
GPU_TEST(compute, many_plain_uniforms_all_arrive)
{
    char src[4096];
    char log[1024] = { 0 };
    int len;
    GLuint prog, out;
    GLint result = 123;

    len = snprintf(src, sizeof src,
                   "#version 460\n"
                   "layout(local_size_x = 1) in;\n"
                   "layout(std430, binding = 0) buffer Out { int r; };\n");
    for (int i = 0; i < 24; i++)
        len += snprintf(src + len, sizeof src - (size_t)len, "uniform int u%d;\n", i);
    len += snprintf(src + len, sizeof src - (size_t)len, "void main() { int s = 0;\n");
    for (int i = 0; i < 24; i++)
        len += snprintf(src + len, sizeof src - (size_t)len, "    s += u%d;\n", i);
    snprintf(src + len, sizeof src - (size_t)len, "    r = s; }\n");

    prog = mgl_build_compute_program(src, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    for (int i = 0; i < 24; i++)
    {
        char name[8];

        snprintf(name, sizeof name, "u%d", i);
        glProgramUniform1i(prog, glGetUniformLocation(prog, name), i + 1);
    }

    glGenBuffers(1, &out);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof result, &result, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, out);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof result, &result);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // 1 + 2 + ... + 24
    CHECK_EQ_INT(result, 300);

    glUseProgram(0);
    glDeleteBuffers(1, &out);
    glDeleteProgram(prog);
}

// the work group size belongs to a linked program with a compute stage
GPU_TEST(compute, work_group_size_needs_a_linked_compute_program)
{
    static const char *CS =
        "#version 460\n"
        "layout(local_size_x = 2, local_size_y = 3) in;\n"
        "void main() {}\n";
    char log[1024] = { 0 };
    GLuint prog = mgl_build_compute_program(CS, log, sizeof log);
    GLuint empty = glCreateProgram();
    GLint size[3] = { 0, 0, 0 };

    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGetProgramiv(prog, GL_COMPUTE_WORK_GROUP_SIZE, size);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(size[0] == 2 && size[1] == 3 && size[2] == 1);

    glGetProgramiv(empty, GL_COMPUTE_WORK_GROUP_SIZE, size);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteProgram(empty);
    glDeleteProgram(prog);
}

// every float, int and uint shape of plain uniform reaches a compute shader
// intact, set once through glProgramUniform* and again, after a relink, with
// glUniform*. A set bit in the result names the uniform that came out wrong.
GPU_TEST(compute, every_uniform_shape_arrives)
{
    static const char *CS =
        "#version 460\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430, binding = 0) buffer Out { uint bad; };\n"
        "uniform float g_0; uniform vec2 g_1; uniform vec3 g_2; uniform vec4 g_3;\n"
        "uniform mat2 g_4; uniform mat2x3 g_5; uniform mat2x4 g_6; uniform mat3x2 g_7;\n"
        "uniform mat3 g_8; uniform mat3x4 g_9; uniform mat4x2 g_10; uniform mat4x3 g_11;\n"
        "uniform mat4 g_12; uniform int g_13; uniform ivec2 g_14; uniform ivec3 g_15;\n"
        "uniform ivec4 g_16; uniform uint g_17; uniform uvec2 g_18; uniform uvec3 g_19;\n"
        "uniform uvec4 g_20;\n"
        "void main() {\n"
        "  uint b = 0u;\n"
        "  if (g_0 != 1.0) b |= 1u << 0;\n"
        "  if (g_1 != vec2(2, 3)) b |= 1u << 1;\n"
        "  if (g_2 != vec3(4, 5, 6)) b |= 1u << 2;\n"
        "  if (g_3 != vec4(7, 8, 9, 10)) b |= 1u << 3;\n"
        "  if (g_4 != mat2(11, 12, 13, 14)) b |= 1u << 4;\n"
        "  if (g_5 != mat2x3(15, 16, 17, 18, 19, 20)) b |= 1u << 5;\n"
        "  if (g_6 != mat2x4(21, 22, 23, 24, 25, 26, 27, 28)) b |= 1u << 6;\n"
        "  if (g_7 != mat3x2(29, 30, 31, 32, 33, 34)) b |= 1u << 7;\n"
        "  if (g_8 != mat3(35, 36, 37, 38, 39, 40, 41, 42, 43)) b |= 1u << 8;\n"
        "  if (g_9 != mat3x4(44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55)) b |= 1u << 9;\n"
        "  if (g_10 != mat4x2(56, 57, 58, 59, 60, 61, 62, 63)) b |= 1u << 10;\n"
        "  if (g_11 != mat4x3(63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74)) b |= 1u << 11;\n"
        "  if (g_12 != mat4(75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90)) b |= 1u << 12;\n"
        "  if (g_13 != 91) b |= 1u << 13;\n"
        "  if (g_14 != ivec2(92, 93)) b |= 1u << 14;\n"
        "  if (g_15 != ivec3(94, 95, 96)) b |= 1u << 15;\n"
        "  if (g_16 != ivec4(97, 98, 99, 100)) b |= 1u << 16;\n"
        "  if (g_17 != 101u) b |= 1u << 17;\n"
        "  if (g_18 != uvec2(102, 103)) b |= 1u << 18;\n"
        "  if (g_19 != uvec3(104, 105, 106)) b |= 1u << 19;\n"
        "  if (g_20 != uvec4(107, 108, 109, 110)) b |= 1u << 20;\n"
        "  bad = b;\n"
        "}\n";
    static const GLfloat m[] = {
        11, 12, 13, 14,  15, 16, 17, 18, 19, 20,  21, 22, 23, 24, 25, 26, 27, 28,
        29, 30, 31, 32, 33, 34,  35, 36, 37, 38, 39, 40, 41, 42, 43,
        44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,  56, 57, 58, 59, 60, 61, 62, 63,
        63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
        75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90 };
    GLuint prog = glCreateProgram();
    GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
    GLuint out, bad = 0xFFFFFFFFu;
    GLint ok = 0;

    // the shader stays attached so the program can be linked again
    glShaderSource(sh, 1, &CS, NULL);
    glCompileShader(sh);
    glAttachShader(prog, sh);
    glDeleteShader(sh);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK(ok == GL_TRUE);
    if (!ok) { glDeleteProgram(prog); return; }

#define LOC(n) glGetUniformLocation(prog, n)
    glGenBuffers(1, &out);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof bad, &bad, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, out);

    glProgramUniform1f(prog, LOC("g_0"), 1);
    glProgramUniform2f(prog, LOC("g_1"), 2, 3);
    glProgramUniform3f(prog, LOC("g_2"), 4, 5, 6);
    glProgramUniform4f(prog, LOC("g_3"), 7, 8, 9, 10);
    glProgramUniformMatrix2fv(prog, LOC("g_4"), 1, GL_FALSE, m);
    glProgramUniformMatrix2x3fv(prog, LOC("g_5"), 1, GL_FALSE, m + 4);
    glProgramUniformMatrix2x4fv(prog, LOC("g_6"), 1, GL_FALSE, m + 10);
    glProgramUniformMatrix3x2fv(prog, LOC("g_7"), 1, GL_FALSE, m + 18);
    glProgramUniformMatrix3fv(prog, LOC("g_8"), 1, GL_FALSE, m + 24);
    glProgramUniformMatrix3x4fv(prog, LOC("g_9"), 1, GL_FALSE, m + 33);
    glProgramUniformMatrix4x2fv(prog, LOC("g_10"), 1, GL_FALSE, m + 45);
    glProgramUniformMatrix4x3fv(prog, LOC("g_11"), 1, GL_FALSE, m + 53);
    glProgramUniformMatrix4fv(prog, LOC("g_12"), 1, GL_FALSE, m + 65);
    glProgramUniform1i(prog, LOC("g_13"), 91);
    glProgramUniform2i(prog, LOC("g_14"), 92, 93);
    glProgramUniform3i(prog, LOC("g_15"), 94, 95, 96);
    glProgramUniform4i(prog, LOC("g_16"), 97, 98, 99, 100);
    glProgramUniform1ui(prog, LOC("g_17"), 101);
    glProgramUniform2ui(prog, LOC("g_18"), 102, 103);
    glProgramUniform3ui(prog, LOC("g_19"), 104, 105, 106);
    glProgramUniform4ui(prog, LOC("g_20"), 107, 108, 109, 110);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof bad, &bad);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(bad == 0, "ProgramUniform: wrong uniforms 0x%x", bad);

    // a relink starts every uniform at zero again
    glLinkProgram(prog);
    bad = 0xFFFFFFFFu;
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof bad, &bad);

    glUniform1f(LOC("g_0"), 1);
    glUniform2f(LOC("g_1"), 2, 3);
    glUniform3f(LOC("g_2"), 4, 5, 6);
    glUniform4f(LOC("g_3"), 7, 8, 9, 10);
    glUniformMatrix2fv(LOC("g_4"), 1, GL_FALSE, m);
    glUniformMatrix2x3fv(LOC("g_5"), 1, GL_FALSE, m + 4);
    glUniformMatrix2x4fv(LOC("g_6"), 1, GL_FALSE, m + 10);
    glUniformMatrix3x2fv(LOC("g_7"), 1, GL_FALSE, m + 18);
    glUniformMatrix3fv(LOC("g_8"), 1, GL_FALSE, m + 24);
    glUniformMatrix3x4fv(LOC("g_9"), 1, GL_FALSE, m + 33);
    glUniformMatrix4x2fv(LOC("g_10"), 1, GL_FALSE, m + 45);
    glUniformMatrix4x3fv(LOC("g_11"), 1, GL_FALSE, m + 53);
    glUniformMatrix4fv(LOC("g_12"), 1, GL_FALSE, m + 65);
    glUniform1i(LOC("g_13"), 91);
    glUniform2i(LOC("g_14"), 92, 93);
    glUniform3i(LOC("g_15"), 94, 95, 96);
    glUniform4i(LOC("g_16"), 97, 98, 99, 100);
    glUniform1ui(LOC("g_17"), 101);
    glUniform2ui(LOC("g_18"), 102, 103);
    glUniform3ui(LOC("g_19"), 104, 105, 106);
    glUniform4ui(LOC("g_20"), 107, 108, 109, 110);
#undef LOC

    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof bad, &bad);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(bad == 0, "Uniform after relink: wrong uniforms 0x%x", bad);

    glUseProgram(0);
    glDeleteBuffers(1, &out);
    glDeleteProgram(prog);
}
