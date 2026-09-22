/*
 * test_program_query.c
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
 * The program interface queries: names the way GL spells them, block arrays
 * one block per element, atomic counter buffers, and locations that agree
 * with the older calls.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

static const char *VS =
    "#version 430 core\n"
    "layout(location = 0) in vec4 position;\n"
    "in vec3 extra[2];\n"
    "void main() { gl_Position = position + vec4(extra[1], 0.0); }\n";

static const char *FS =
    "#version 430 core\n"
    "struct S { float a; vec2 b[2]; };\n"
    "uniform S s;\n"
    "layout(location = 7) uniform sampler2D tex;\n"
    "layout(std140, binding = 3) uniform Block { mat3 m; float f[2]; } blk[2];\n"
    "layout(std430, binding = 4) buffer Store { vec4 head; float tail[]; } store;\n"
    "layout(binding = 2, offset = 4) uniform atomic_uint counter;\n"
    "out vec4 color;\n"
    "void main() {\n"
    "    float t = texture(tex, vec2(0.5)).r + s.a + s.b[1].x;\n"
    "    t += blk[1].f[0] + blk[0].m[0][0] + store.head.x + store.tail[2];\n"
    "    color = vec4(t) + vec4(float(atomicCounterIncrement(counter)));\n"
    "}\n";

static GLuint build(void)
{
    char log[2048] = "";
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    return prog;
}

static GLint prop(GLuint prog, GLenum iface, GLuint index, GLenum p)
{
    GLint v = -1234;

    glGetProgramResourceiv(prog, iface, index, 1, &p, 1, NULL, &v);
    return v;
}

GPU_TEST(program_query, names_follow_gl_rules)
{
    GLuint prog = build();

    if (!prog)
        return;

    // an array input is one resource named with [0]
    GLuint extra = glGetProgramResourceIndex(prog, GL_PROGRAM_INPUT, "extra");

    CHECK(extra != GL_INVALID_INDEX);
    CHECK_EQ_UINT(extra, glGetProgramResourceIndex(prog, GL_PROGRAM_INPUT, "extra[0]"));
    CHECK_EQ_INT(2, prop(prog, GL_PROGRAM_INPUT, extra, GL_ARRAY_SIZE));
    CHECK_EQ_INT(GL_FLOAT_VEC3, prop(prog, GL_PROGRAM_INPUT, extra, GL_TYPE));

    char name[64];
    GLsizei len = 0;

    glGetProgramResourceName(prog, GL_PROGRAM_INPUT, extra, sizeof name, &len, name);
    CHECK_MSG(!strcmp(name, "extra[0]") && len == 8, "input named '%s' (%d)", name, (int)len);

    // struct members are spelled out, arrays inside them too
    CHECK(glGetProgramResourceIndex(prog, GL_UNIFORM, "s.a") != GL_INVALID_INDEX);
    CHECK(glGetProgramResourceIndex(prog, GL_UNIFORM, "s.b[0]") != GL_INVALID_INDEX);

    // one block per element, members named by the block
    GLuint b1 = glGetProgramResourceIndex(prog, GL_UNIFORM_BLOCK, "Block[1]");

    CHECK(b1 != GL_INVALID_INDEX);
    CHECK_EQ_UINT(glGetProgramResourceIndex(prog, GL_UNIFORM_BLOCK, "Block[0]"),
                  glGetProgramResourceIndex(prog, GL_UNIFORM_BLOCK, "Block"));
    CHECK_EQ_INT(4, prop(prog, GL_UNIFORM_BLOCK, b1, GL_BUFFER_BINDING));

    GLuint f = glGetProgramResourceIndex(prog, GL_UNIFORM, "Block.f[0]");

    CHECK(f != GL_INVALID_INDEX);
    CHECK_EQ_INT(-1, prop(prog, GL_UNIFORM, f, GL_LOCATION));
    CHECK_EQ_INT(16, prop(prog, GL_UNIFORM, f, GL_ARRAY_STRIDE));

    GLuint m = glGetProgramResourceIndex(prog, GL_UNIFORM, "Block.m");

    CHECK_EQ_INT(16, prop(prog, GL_UNIFORM, m, GL_MATRIX_STRIDE));
    CHECK_EQ_INT(GL_FLOAT_MAT3, prop(prog, GL_UNIFORM, m, GL_TYPE));

    // a runtime array has no size of its own
    GLuint tail = glGetProgramResourceIndex(prog, GL_BUFFER_VARIABLE, "Store.tail");

    CHECK(tail != GL_INVALID_INDEX);
    CHECK_EQ_INT(0, prop(prog, GL_BUFFER_VARIABLE, tail, GL_ARRAY_SIZE));
    CHECK_EQ_INT(1, prop(prog, GL_BUFFER_VARIABLE, tail, GL_TOP_LEVEL_ARRAY_SIZE));
    CHECK_EQ_INT(1, prop(prog, GL_BUFFER_VARIABLE, tail, GL_REFERENCED_BY_FRAGMENT_SHADER));
    CHECK_EQ_INT(0, prop(prog, GL_BUFFER_VARIABLE, tail, GL_REFERENCED_BY_VERTEX_SHADER));

    glDeleteProgram(prog);
}

GPU_TEST(program_query, atomic_counter_buffers)
{
    GLuint prog = build();

    if (!prog)
        return;

    GLint buffers = -1;

    glGetProgramInterfaceiv(prog, GL_ATOMIC_COUNTER_BUFFER, GL_ACTIVE_RESOURCES, &buffers);
    CHECK_EQ_INT(1, buffers);
    CHECK_EQ_INT(2, prop(prog, GL_ATOMIC_COUNTER_BUFFER, 0, GL_BUFFER_BINDING));
    CHECK_EQ_INT(8, prop(prog, GL_ATOMIC_COUNTER_BUFFER, 0, GL_BUFFER_DATA_SIZE));
    CHECK_EQ_INT(1, prop(prog, GL_ATOMIC_COUNTER_BUFFER, 0, GL_NUM_ACTIVE_VARIABLES));

    GLuint counter = glGetProgramResourceIndex(prog, GL_UNIFORM, "counter");

    CHECK_EQ_INT(0, prop(prog, GL_UNIFORM, counter, GL_ATOMIC_COUNTER_BUFFER_INDEX));
    CHECK_EQ_INT(-1, prop(prog, GL_UNIFORM, counter, GL_LOCATION));

    // no names for these, so asking by name is an error
    glGetProgramResourceIndex(prog, GL_ATOMIC_COUNTER_BUFFER, "counter");
    CHECK_EQ_UINT(GL_INVALID_ENUM, glGetError());

    glDeleteProgram(prog);
}

GPU_TEST(program_query, sampler_location_and_unit)
{
    GLuint prog = build();

    if (!prog)
        return;

    // a declared location holds, and the unit starts at the declared binding
    CHECK_EQ_INT(7, glGetUniformLocation(prog, "tex"));
    CHECK_EQ_INT(7, glGetProgramResourceLocation(prog, GL_UNIFORM, "tex"));

    GLint unit = -1;

    glGetUniformiv(prog, 7, &unit);
    CHECK_EQ_INT(0, unit);

    glUseProgram(prog);
    glUniform1i(7, 5);
    glGetUniformiv(prog, 7, &unit);
    CHECK_EQ_INT(5, unit);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glUseProgram(0);
    glDeleteProgram(prog);
}

GPU_TEST(program_query, subroutine_uniform_keeps_its_declared_location)
{
    static const char *fs =
        "#version 430 core\n"
        "subroutine vec4 pick(float x);\n"
        "subroutine(pick) vec4 red(float x) { return vec4(x, 0, 0, 1); }\n"
        "subroutine(pick) vec4 green(float x) { return vec4(0, x, 0, 1); }\n"
        "layout(location = 3) subroutine uniform pick chosen;\n"
        "subroutine uniform pick other;\n"
        "out vec4 color;\n"
        "void main() { color = chosen(1.0) + other(0.5); }\n";
    char log[2048] = "";
    GLuint prog = mgl_build_program(VS, fs, log, sizeof log);

    CHECK_MSG(prog != 0, "program did not build: %s", log);
    if (!prog)
        return;

    CHECK_EQ_INT(3, glGetSubroutineUniformLocation(prog, GL_FRAGMENT_SHADER, "chosen"));
    CHECK_EQ_INT(0, glGetSubroutineUniformLocation(prog, GL_FRAGMENT_SHADER, "other"));

    GLint locations = 0;

    glGetProgramStageiv(prog, GL_FRAGMENT_SHADER, GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS, &locations);
    CHECK_EQ_INT(4, locations);

    GLuint u = glGetProgramResourceIndex(prog, GL_FRAGMENT_SUBROUTINE_UNIFORM, "chosen");
    GLenum props[2] = { GL_NUM_COMPATIBLE_SUBROUTINES, GL_COMPATIBLE_SUBROUTINES };
    GLint vals[4] = { -1, -1, -1, -1 };
    GLsizei n = 0;

    glGetProgramResourceiv(prog, GL_FRAGMENT_SUBROUTINE_UNIFORM, u, 2, props, 4, &n, vals);
    CHECK_EQ_INT(3, n);
    CHECK_EQ_INT(2, vals[0]);

    glDeleteProgram(prog);
}
