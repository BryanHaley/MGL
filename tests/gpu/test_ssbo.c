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
