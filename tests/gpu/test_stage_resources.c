/*
 * test_stage_resources.c
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
 * Images, storage buffers and atomic counters outside the vertex and
 * fragment stages, and the size of a rectangle image.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static GLuint compileOne(GLenum type, const char *src, char *log, int log_size)
{
    GLuint s = glCreateShader(type);
    GLint ok = 0;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        glGetShaderInfoLog(s, log_size, NULL, log);
        glDeleteShader(s);
        return 0;
    }

    return s;
}

static GLuint linkVGF(const char *vs, const char *gs, const char *fs, char *log, int log_size)
{
    GLuint prog, v, g, f;
    GLint ok = 0;

    log[0] = 0;
    v = compileOne(GL_VERTEX_SHADER, vs, log, log_size);
    g = compileOne(GL_GEOMETRY_SHADER, gs, log, log_size);
    f = compileOne(GL_FRAGMENT_SHADER, fs, log, log_size);

    if (!v || !g || !f)
        return 0;

    prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, g);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(g);
    glDeleteShader(f);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok)
    {
        glGetProgramInfoLog(prog, log_size, NULL, log);
        glDeleteProgram(prog);
        return 0;
    }

    return prog;
}

static const char *kPointVS =
    "#version 460 core\n"
    "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";

static const char *kPlainFS =
    "#version 460 core\n"
    "out vec4 o;\n"
    "void main() { o = vec4(1.0); }\n";

// imageSize and textureSize on a rectangle give width and height. Both came
// back as the width twice.
GPU_TEST(stage_resources, rectangle_size_has_a_height)
{
    static const char *CS =
        "#version 460 core\n"
        "layout(local_size_x = 1) in;\n"
        "layout(rgba8, binding = 0) uniform readonly image2DRect img;\n"
        "layout(binding = 1) uniform sampler2DRect smp;\n"
        "layout(std430, binding = 0) buffer Out { ivec4 got; };\n"
        "void main() { got = ivec4(imageSize(img), textureSize(smp)); }\n";
    GLint got[4] = { 0 };
    GLuint prog, tex, ssbo;
    char log[2048] = { 0 };

    prog = mgl_build_compute_program(CS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
    glTexStorage2D(GL_TEXTURE_RECTANGLE, 1, GL_RGBA8, 8, 4);
    glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, tex);
    glActiveTexture(GL_TEXTURE0);

    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof got, got, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);

    glUseProgram(prog);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_MSG(got[0] == 8 && got[1] == 4, "imageSize is %d x %d, want 8 x 4", got[0], got[1]);
    CHECK_MSG(got[2] == 8 && got[3] == 4, "textureSize is %d x %d, want 8 x 4", got[2], got[3]);

    glUseProgram(0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    glActiveTexture(GL_TEXTURE0);
    glDeleteBuffers(1, &ssbo);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
}

// A geometry shader may count into two atomic counter buffers, and a storage
// buffer's .length() there is the bound size. Only one counter buffer was
// allowed, and the stage was never given the sizes .length() reads.
GPU_TEST(stage_resources, geometry_stage_counters_and_buffer_length)
{
    static const char *GS =
        "#version 460 core\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 1) out;\n"
        "layout(binding = 0, offset = 0) uniform atomic_uint a;\n"
        "layout(binding = 1, offset = 0) uniform atomic_uint b;\n"
        "layout(std430, binding = 2) buffer Lens { uint n[]; };\n"
        "void main() {\n"
        "    atomicCounterIncrement(a);\n"
        "    atomicCounterIncrement(b);\n"
        "    atomicCounterIncrement(b);\n"
        "    n[0] = uint(n.length());\n"
        "    gl_Position = gl_in[0].gl_Position;\n"
        "    EmitVertex();\n"
        "}\n";
    GLint limit = 0;
    GLuint prog, vao, counters[2], lens, zero = 0, got = 0, len = 0;
    char log[2048] = { 0 };
    MGLTestTarget t;

    glGetIntegerv(GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS, &limit);
    CHECK(limit >= 2);

    prog = linkVGF(kPointVS, GS, kPlainFS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGenBuffers(2, counters);
    for (int i = 0; i < 2; i++)
    {
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counters[i]);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof zero, &zero, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, (GLuint)i, counters[i]);
    }

    glGenBuffers(1, &lens);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, lens);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 5 * sizeof(GLuint), NULL, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, lens);

    mgl_target_create(&t, 4, 4, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glDrawArrays(GL_POINTS, 0, 3);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counters[0]);
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof got, &got);
    CHECK_EQ_INT((GLint)got, 3);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counters[1]);
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof got, &got);
    CHECK_EQ_INT((GLint)got, 6);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, lens);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof len, &len);
    CHECK_EQ_INT((GLint)len, 5);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(2, counters);
    glDeleteBuffers(1, &lens);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
