/*
 * test_ssbo.c
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Shader storage buffer objects, end to end. The interesting parts are the
 * ones the driver has historically got wrong: indexing an unsized array with
 * a computed index, asking such an array for its .length(), the std430
 * offsets a buffer block's members land on, and atomic updates through a
 * buffer. Expected values come from the GL 4.6 core specification, section
 * 7.6 (buffer variable layout), not from what the driver does.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

/* ---------- helpers ---------- */

/* Runs a compute program over a list of buffers already bound with
   glBindBufferBase, waits for the result, and leaves the program unbound.
   Returns 0 if the program did not build. */
static int run_compute(GLuint prog, GLuint groups_x, GLuint groups_y, GLuint groups_z)
{
    if (!prog)
        return 0;

    glUseProgram(prog);
    glDispatchCompute(groups_x, groups_y, groups_z);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    glFinish();
    glUseProgram(0);

    return 1;
}

/* ---------- 1. indirect indexing into an unsized array ---------- */

/* values[] has no declared size, so the only way to read it is the size the
   bound buffer actually has. Reading values[i * 2] forces the address to be
   computed from a live value rather than folded into a constant. */
static const char *INDIRECT_CS =
"#version 460 core\n"
"layout(local_size_x = 8) in;\n"
"layout(std430, binding = 0) buffer In  { float values[]; };\n"
"layout(std430, binding = 1) buffer Out { float picked[]; };\n"
"void main() {\n"
"    uint i = gl_GlobalInvocationID.x;\n"
"    picked[i] = values[i * 2u];\n"
"}\n";

GPU_TEST(ssbo, indirect_index_into_unsized_array)
{
    static const GLuint count = 16;
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(INDIRECT_CS, log, sizeof log);
    GLuint bufs[2] = { 0, 0 };
    GLfloat in[16], zero[16] = {0}, got[16] = {0};
    GLuint wrong = 0;

    CHECK_MSG(prog != 0, "indirect SSBO kernel did not build: %s", log);
    if (!prog)
        return;

    for (GLuint i = 0; i < count; i++)
        in[i] = (GLfloat)(i + 1);

    glGenBuffers(2, bufs);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof in, in, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof zero, zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[1]);

    /* 8 invocations, each reading an even index: 0, 2, ... 14 */
    CHECK(run_compute(prog, 1, 1, 1));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 8 * sizeof(GLfloat), got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    /* picked[i] should be values[2i] == 2i + 1 */
    for (GLuint i = 0; i < 8; i++)
        if (got[i] != (GLfloat)(2 * i + 1))
            wrong++;

    CHECK_MSG(wrong == 0, "%u of 8 picked values wrong (got %g %g %g %g, want 1 3 5 7)",
              wrong, got[0], got[1], got[2], got[3]);

    glDeleteBuffers(2, bufs);
    glDeleteProgram(prog);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- 2. .length() on an unsized array ---------- */

/* The length of a run-time sized array is the bound buffer's size divided by
   the array's stride. 64 bytes of float is 16 elements. */
static const char *LENGTH_CS =
"#version 460 core\n"
"layout(local_size_x = 1) in;\n"
"layout(std430, binding = 0) buffer Data { float values[]; };\n"
"layout(std430, binding = 1) buffer Cnt  { uint count; };\n"
"void main() { count = values.length(); }\n";

GPU_TEST(ssbo, length_of_unsized_array)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(LENGTH_CS, log, sizeof log);
    GLuint bufs[2] = { 0, 0 };
    GLfloat in[16];
    GLuint count = 0;

    CHECK_MSG(prog != 0, "SSBO .length() kernel did not build: %s", log);
    if (!prog)
        return;

    for (GLuint i = 0; i < 16; i++)
        in[i] = (GLfloat)(i + 1);

    glGenBuffers(2, bufs);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof in, in, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof count, &count, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[1]);

    CHECK(run_compute(prog, 1, 1, 1));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof count, &count);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_UINT(count, 16);

    glDeleteBuffers(2, bufs);
    glDeleteProgram(prog);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- 3. std430 offsets for vec3, mat4 and float ---------- */

/* GL 4.6 section 7.6.2.2: in std430 a vec3 has base alignment 16, so it
   occupies 12 bytes but the following mat4 is aligned to 16 and starts at
   offset 16; the mat4 is 64 bytes (column-major), so the float lands at
   offset 80. The shader reads each member at those offsets and folds them
   into one weighted sum, so a wrong offset shows up as a wrong checksum. */
static const char *LAYOUT_CS =
"#version 460 core\n"
"layout(local_size_x = 1) in;\n"
"layout(std430, binding = 0) buffer B { vec3 v; mat4 m; float f; };\n"
"layout(std430, binding = 1) buffer Sum { float s; };\n"
"void main() {\n"
"    float sum = v.x * 1.0 + v.y * 2.0 + v.z * 3.0;\n"
"    for (int c = 0; c < 4; c++)\n"
"        for (int r = 0; r < 4; r++)\n"
"            sum += m[c][r] * float(4 + c * 4 + r);\n"
"    sum += f * 1000.0;\n"
"    s = sum;\n"
"}\n";

GPU_TEST(ssbo, std430_vec3_mat4_float_offsets)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(LAYOUT_CS, log, sizeof log);
    GLuint bufs[2] = { 0, 0 };
    GLfloat block[32];      /* 128 bytes; the struct needs 84 */
    GLfloat sum = 0.0f;
    GLfloat expected;

    CHECK_MSG(prog != 0, "std430 layout kernel did not build: %s", log);
    if (!prog)
        return;

    memset(block, 0, sizeof block);

    /* Write the members at the offsets the specification requires. The float
       array is indexed in units of one float (4 bytes). */
    block[0] = 1.0f; block[1] = 2.0f; block[2] = 3.0f;              /* v  @ 0  */
    for (int k = 0; k < 16; k++)                                    /* m  @ 16 */
        block[4 + k] = (GLfloat)(k + 1);                            /* 4 floats in */
    block[20] = 100.0f;                                             /* f  @ 80 */

    /* The same weighted sum, read from the specification-defined offsets. */
    expected = block[0] * 1.0f + block[1] * 2.0f + block[2] * 3.0f;
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++)
            expected += block[4 + c * 4 + r] * (GLfloat)(4 + c * 4 + r);
    expected += block[20] * 1000.0f;

    glGenBuffers(2, bufs);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof block, block, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof sum, &sum, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[1]);

    CHECK(run_compute(prog, 1, 1, 1));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof sum, &sum);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_NEAR(sum, expected, 0.5);

    glDeleteBuffers(2, bufs);
    glDeleteProgram(prog);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- 4. atomics on an SSBO ---------- */

/* 64 invocations, one group, each adding one. A count short of 64 means the
   read-modify-write raced. */
static const char *ATOMIC_CS =
"#version 460 core\n"
"layout(local_size_x = 64) in;\n"
"layout(std430, binding = 0) buffer C { uint counter; };\n"
"void main() { atomicAdd(counter, 1u); }\n";

GPU_TEST(ssbo, atomic_add_counts_every_invocation)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(ATOMIC_CS, log, sizeof log);
    GLuint buf = 0;
    GLuint counter = 0;

    CHECK_MSG(prog != 0, "atomic SSBO kernel did not build: %s", log);
    if (!prog)
        return;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof counter, &counter, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);

    CHECK(run_compute(prog, 1, 1, 1));
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof counter, &counter);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_UINT(counter, 64);

    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// glBindBufferBase reports a start and size of 0, a range has to start on
// the target's alignment, and a shader's name is the wrong kind of object.
GPU_TEST(ssbo, binding_queries_and_errors_follow_the_spec)
{
    GLuint buf, sh;
    GLint64 size = -1;
    GLint align = 0;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 1024, NULL, GL_DYNAMIC_DRAW);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE, 0, &size);
    CHECK_EQ_INT((GLint)size, 0);

    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &align);
    if (align > 1)
    {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, buf, align / 2, 16);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    }

    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, buf, align, 16);
    glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE, 0, &size);
    CHECK_EQ_INT((GLint)size, 16);

    sh = glCreateShader(GL_VERTEX_SHADER);
    glShaderStorageBlockBinding(sh, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteShader(sh);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    glDeleteBuffers(1, &buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// offset and align need a std140 or std430 block. One with no layout is
// shared, so the shader must not compile; with std430 it does.
GPU_TEST(ssbo, a_shared_block_cannot_place_members)
{
    static const char *shared_cs =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(binding = 0) buffer B { layout(offset = 16) vec4 b; layout(align = 64) vec4 a; } blk;\n"
        "void main() { blk.a = blk.b; }\n";
    static const char *std430_cs =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(std430, binding = 0) buffer B { layout(offset = 16) vec4 b; layout(align = 64) vec4 a; } blk;\n"
        "void main() { blk.a = blk.b; }\n";
    GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
    GLint ok = 1;

    glShaderSource(sh, 1, &shared_cs, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);

    glShaderSource(sh, 1, &std430_cs, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_TRUE);

    glDeleteShader(sh);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// The generic binding queries name the buffer last bound to the target. They
// read a value set once at startup and always said 0.
GPU_TEST(ssbo, generic_binding_queries_follow_the_binds)
{
    GLuint b[2];
    GLint v = -1;

    glGenBuffers(2, b);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, b[0]);
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &v);
    CHECK_EQ_INT(v, (GLint)b[0]);
    glBindBuffer(GL_UNIFORM_BUFFER, b[1]);
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &v);
    CHECK_EQ_INT(v, (GLint)b[1]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glDeleteBuffers(2, b);
}

// Deleting a buffer clears its indexed bindings whole, so START and SIZE read
// 0 again; the range stayed behind. Unbinding still checks the alignment.
GPU_TEST(ssbo, deleting_a_bound_range_clears_it)
{
    GLuint b;
    GLint64 v = -1;
    GLint align = 0;

    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &align);
    glGenBuffers(1, &b);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, b);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 1024, NULL, GL_STATIC_DRAW);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, b, 256, 128);
    glDeleteBuffers(1, &b);

    glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START, 0, &v);
    CHECK_EQ_INT((int)v, 0);
    glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE, 0, &v);
    CHECK_EQ_INT((int)v, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    if (align > 1)
    {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 0, align - 1, 0);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    }
}

// A range may be bound on a buffer that has no storage yet, as BindBufferBase
// allows; it raised INVALID_VALUE and left the generic binding alone.
GPU_TEST(ssbo, a_range_binds_before_storage)
{
    GLuint b;
    GLint v = -1;

    glGenBuffers(1, &b);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 1, b, 256, 512);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &v);
    CHECK_EQ_INT(v, (GLint)b);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, 0);
    glDeleteBuffers(1, &b);
}

// The shader storage block spellings from the CTS's basic-syntax case: each
// one reads three positions out of a buffer to cover the target
GPU_TEST(ssbo, every_basic_syntax_draws)
{
    static const char *VS[] = {
        "#version 430 core\nlayout(std430) buffer Buffer {\n  vec4 position[3];\n} g_input_buffer;\nvoid main() {\n  gl_Position = g_input_buffer.position[gl_VertexID];\n}\n",
        "#version 430 core\ncoherent buffer Buffer {\n  buffer vec4 position0;\n  coherent vec4 position1;\n  restrict readonly vec4 position2;\n} g_input_buffer;\nvoid main() {\n  if (gl_VertexID == 0) gl_Position = g_input_buffer.position0;\n  if (gl_VertexID == 1) gl_Position = g_input_buffer.position1;\n  if (gl_VertexID == 2) gl_Position = g_input_buffer.position2;\n}\n",
        "#version 430 core\nlayout(std140, binding = 0) readonly buffer Buffer {\n  readonly vec4 position[];\n};\nvoid main() {\n  gl_Position = position[gl_VertexID];\n}\n",
        "#version 430 core\nlayout(std430, column_major, std140, std430, row_major, packed, shared) buffer;\nlayout(std430) buffer;\ncoherent restrict volatile buffer Buffer {\n  restrict coherent vec4 position[];\n} g_buffer;\nvoid main() {\n  gl_Position = g_buffer.position[gl_VertexID];\n}\n",
        "#version 430 core\nbuffer Buffer {\n  vec4 position[3];\n} g_buffer[1];\nvoid main() {\n  gl_Position = g_buffer[0].position[gl_VertexID];\n}\n",
        "#version 430 core\nlayout(shared) coherent buffer Buffer {\n  restrict volatile vec4 position0;\n  buffer readonly vec4 position1;\n  vec4 position2;\n} g_buffer[1];\nvoid main() {\n  if (gl_VertexID == 0) gl_Position = g_buffer[0].position0;\n  else if (gl_VertexID == 1) gl_Position = g_buffer[0].position1;\n  else if (gl_VertexID == 2) gl_Position = g_buffer[0].position2;\n}\n",
        "#version 430 core\nlayout(packed) coherent buffer Buffer {\n  vec4 position01[];\n  vec4 position2;\n} g_buffer;\nvoid main() {\n  if (gl_VertexID == 0) gl_Position = g_buffer.position01[0];\n  else if (gl_VertexID == 1) gl_Position = g_buffer.position01[1];\n  else if (gl_VertexID == 2) gl_Position = g_buffer.position2;\n}\n",
        "#version 430 core\nlayout(std430) coherent buffer Buffer {\n  coherent vec4 position01[];\n  vec4 position2[];\n} g_buffer;\nvoid main() {\n  switch (gl_VertexID) {\n    case 0: gl_Position = g_buffer.position01[0]; break;\n    case 1: gl_Position = g_buffer.position01[1]; break;\n    case 2: gl_Position = g_buffer.position2[gl_VertexID - 2]; break;\n  }\n}\n",
    };
    static const char *FS =
        "#version 430 core\n"
        "layout(location = 0) out vec4 o_color;\n"
        "void main() { o_color = vec4(0.0, 1.0, 0.0, 1.0); }\n";
    static const float data[12] = { -1, -1, 0, 1,  3, -1, 0, 1,  -1, 3, 0, 1 };
    MGLTestTarget t;
    GLuint buf, vao;
    char log[1024];

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0)) { CHECK(0); return; }

    glGenBuffers(1, &buf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof data, data, GL_STATIC_DRAW);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    for (int i = 0; i < (int)(sizeof VS / sizeof VS[0]); i++)
    {
        GLuint p = mgl_build_program(VS[i], FS, log, sizeof log);
        unsigned char *px, c[4] = { 0 };

        CHECK_MSG(p != 0, "variant %d did not build: %s", i, log);
        if (!p) continue;

        glUseProgram(p);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        px = mgl_read_rgba8(&t);
        mgl_pixel_at(px, &t, 4, 4, c);
        free(px);
        CHECK_MSG(c[1] == 255, "variant %d drew %d %d %d", i, c[0], c[1], c[2]);
        glUseProgram(0);
        glDeleteProgram(p);
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &buf);
    mgl_target_destroy(&t);
}

GPU_TEST(ssbo, block_size_limits_read_as_positive_ints)
{
    GLint ubo = 0, ssbo = 0, bindings = 0;
    GLuint buf = 0;

    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &ubo);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &ssbo);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &bindings);
    CHECK_MSG(ubo >= 16384, "GL_MAX_UNIFORM_BLOCK_SIZE %d", ubo);
    CHECK_MSG(ssbo >= 134217728, "GL_MAX_SHADER_STORAGE_BLOCK_SIZE %d", ssbo);
    if (ubo <= 0 || bindings <= 0) return;

    // what CTS multi_bind allocates per binding
    glGenBuffers(1, &buf);
    glBindBuffer(GL_UNIFORM_BUFFER, buf);
    glBufferData(GL_UNIFORM_BUFFER, ubo / bindings, NULL, GL_STATIC_DRAW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDeleteBuffers(1, &buf);
}

// Matrices in a coherent buffer, as in the CTS advanced-matrix case
static const char *COHERENT_MAT_CS =
"#version 460 core\n"
"layout(local_size_x = 1) in;\n"
"layout(std430, binding = 0) coherent buffer B { mat3x4 t[2]; mat4x3 c; vec4 r; };\n"
"void main() {\n"
"    c[1] = vec3(7.0, 8.0, 9.0);\n"
"    r = vec4(transpose(t[1]) * vec4(1.0, 0.0, 0.0, 0.0), c[1].y);\n"
"}\n";

GPU_TEST(ssbo, coherent_matrices_index_and_transpose)
{
    char log[2048] = "";
    GLuint prog = mgl_build_compute_program(COHERENT_MAT_CS, log, sizeof log);
    GLuint buf = 0;
    GLfloat data[44] = { 0 };   /* t @ 0 (24 floats), c @ 24 (16), r @ 40 */

    CHECK_MSG(prog != 0, "coherent matrix kernel did not build: %s", log);
    if (!prog)
        return;

    for (int k = 0; k < 12; k++)
        data[12 + k] = (GLfloat)(k + 1);     /* t[1], three columns of four */

    glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof data, data, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);

    CHECK(run_compute(prog, 1, 1, 1));
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof data, data);

    /* the first row of t[1] is element 0 of each column */
    CHECK_NEAR(data[40], 1.0, 0.001);
    CHECK_NEAR(data[41], 5.0, 0.001);
    CHECK_NEAR(data[42], 9.0, 0.001);
    CHECK_NEAR(data[43], 8.0, 0.001);
    CHECK_NEAR(data[28], 7.0, 0.001);

    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// one quad per draw, each read from a different buffer or range
GPU_TEST(ssbo, switching_buffers_between_draws)
{
    static const char *VS =
        "#version 430 core\n"
        "struct V { vec2 p; vec3 c; };\n"
        "layout(binding = 0, std430) buffer In { V v[]; };\n"
        "out vec3 col;\n"
        "void main() { gl_Position = vec4(v[gl_VertexID].p, 0, 1); col = v[gl_VertexID].c; }\n";
    static const char *FS =
        "#version 430 core\n"
        "in vec3 col;\n"
        "layout(location = 0) out vec4 o;\n"
        "void main() { o = vec4(col, 1); }\n";
    static const float off[4][2] = { { -0.5f, -0.5f }, { 0.5f, -0.5f }, { -0.5f, 0.5f }, { 0.5f, 0.5f } };
    static const float col[4][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 0 } };
    MGLTestTarget t;
    GLuint bufs[5], vao;
    char log[1024];

    if (!mgl_target_create(&t, 16, 16, GL_RGBA8, 0)) { CHECK(0); return; }

    GLuint p = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(p != 0, "did not build: %s", log);
    if (!p) { mgl_target_destroy(&t); return; }

    glGenBuffers(5, bufs);
    for (int q = 0; q < 4; q++)
    {
        float d[32] = { 0 };

        for (int k = 0; k < 4; k++)
        {
            d[k * 8 + 0] = off[q][0] + ((k & 1) ? 0.4f : -0.4f);
            d[k * 8 + 1] = off[q][1] + ((k & 2) ? 0.4f : -0.4f);
            memcpy(&d[k * 8 + 4], col[q], sizeof col[q]);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[q]);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof d, d, GL_STATIC_DRAW);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[4]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 4 * 32 * sizeof(float), NULL, GL_STATIC_DRAW);
    for (int q = 0; q < 4; q++)
    {
        glBindBuffer(GL_COPY_READ_BUFFER, bufs[q]);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_SHADER_STORAGE_BUFFER, 0, q * 32 * sizeof(float), 32 * sizeof(float));
    }

    {
        float all[128] = { 0 };

        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof all, all);
        for (int q = 0; q < 4; q++)
            CHECK_MSG(all[q * 32 + 4] == col[q][0] && all[q * 32 + 5] == col[q][1],
                      "copy %d landed as %g %g", q, all[q * 32 + 4], all[q * 32 + 5]);
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(p);

    for (int pass = 0; pass < 2; pass++)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int q = 0; q < 4; q++)
        {
            if (pass == 0)
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[q]);
            else
                glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, bufs[4], q * 32 * sizeof(float), 32 * sizeof(float));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        unsigned char *px = mgl_read_rgba8(&t);

        for (int q = 0; q < 4; q++)
        {
            unsigned char c[4] = { 0 };
            int x = off[q][0] < 0 ? 4 : 12, y = off[q][1] < 0 ? 4 : 12;

            mgl_pixel_at(px, &t, x, y, c);
            CHECK_MSG(c[0] == col[q][0] * 255 && c[1] == col[q][1] * 255 && c[2] == col[q][2] * 255,
                      "pass %d quad %d drew %d %d %d", pass, q, c[0], c[1], c[2]);
        }

        free(px);
    }

    glUseProgram(0);
    glDeleteProgram(p);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(5, bufs);
    mgl_target_destroy(&t);
}
