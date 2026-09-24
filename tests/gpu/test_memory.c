/*
 * test_memory.c
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
 * Memory an application gives back stays given back. A plain C program never
 * drains an autorelease pool, so anything Metal handed back autoreleased used
 * to live until the process died -- the conformance run reached 38 GB.
 */

#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mgl_test.h"
#include "harness.h"
#include "MGLContext.h"

static size_t footprint(void)
{
    task_vm_info_data_t info;
    mach_msg_type_number_t n = TASK_VM_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &n) != KERN_SUCCESS)
        return 0;

    return (size_t)info.phys_footprint;
}

static void churn(int rounds)
{
    enum { N = 64 };
    static GLubyte texels[N * N * N * 4];

    memset(texels, 0x5a, sizeof texels);

    for (int r = 0; r < rounds; r++)
    {
        GLuint tex[2];
        GLubyte back[16];

        glGenTextures(2, tex);

        for (int i = 0; i < 2; i++)
        {
            glBindTexture(GL_TEXTURE_3D, tex[i]);
            glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, N, N, N);
        }

        glBindTexture(GL_TEXTURE_3D, tex[0]);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, N, N, N, GL_RGBA, GL_UNSIGNED_BYTE, texels);
        glCopyImageSubData(tex[0], GL_TEXTURE_3D, 0, 0, 0, 0, tex[1], GL_TEXTURE_3D, 0, 0, 0, 0, N, N, N);

        // reading back waits for the copy, so every round really runs
        glBindTexture(GL_TEXTURE_3D, tex[1]);
        glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
        memcpy(back, texels, sizeof back);
        CHECK_MSG(back[0] == 0x5a, "round %d copied %02x", r, back[0]);

        glBindTexture(GL_TEXTURE_3D, 0);
        glDeleteTextures(2, tex);
    }

    glFinish();
}

// Each round makes and throws away 2 MB of textures. Leaking them, forty
// rounds grow the process by 80 MB and more; given back, it stays flat.
GPU_TEST(memory, deleted_textures_are_given_back)
{
    churn(10);   // settle the caches and pools first

    size_t before = footprint();

    churn(40);

    size_t after = footprint();
    long grew = (long)(after > before ? after - before : 0) / (1024 * 1024);

    CHECK_MSG(before != 0, "could not read the process footprint");
    CHECK_MSG(grew < 24, "forty rounds grew the process by %ld MB", grew);
}

// A compile that fails used to keep glslang's whole memory pool for the
// shader. Conformance compiles thousands of broken shaders in one case and
// reached 11 GB on it.
GPU_TEST(memory, failed_compiles_are_given_back)
{
    static const char *bad =
        "#version 460 core\n"
        "float gl_reserved;\n"
        "void main() { gl_Position = vec4(gl_reserved); }\n";
    GLuint sh = glCreateShader(GL_VERTEX_SHADER);
    GLint ok = 1;

    glShaderSource(sh, 1, &bad, NULL);

    for (int i = 0; i < 20; i++)
        glCompileShader(sh);

    size_t before = footprint();

    for (int i = 0; i < 400; i++)
        glCompileShader(sh);

    size_t after = footprint();
    long grew = (long)(after > before ? after - before : 0) / (1024 * 1024);

    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    CHECK_MSG(!ok, "a shader using a reserved name compiled");
    CHECK_MSG(grew < 16, "400 failed compiles grew the process by %ld MB", grew);

    glDeleteShader(sh);
}

// Metal hands GPU memory back a second or two after the work that used it
// finishes, so this waits for the footprint to come down before reading it.
static size_t settledFootprint(void)
{
    size_t low = footprint();

    for (int i = 0; i < 10; i++)
    {
        usleep(200 * 1000);

        size_t now = footprint();

        if (now < low)
            low = now;
    }

    return low;
}

// Runs `round` a few times to settle caches, then `rounds` more, and returns
// how many MB the process grew over the second batch.
static long growthOver(void (*round)(int), int rounds)
{
    for (int i = 0; i < 5; i++)
        round(i);

    glFinish();

    size_t before = settledFootprint();

    for (int i = 0; i < rounds; i++)
        round(i);

    glFinish();

    size_t after = settledFootprint();

    return (long)(after > before ? after - before : 0) / (1024 * 1024);
}

static void linkRound(int i)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "uniform vec4 a; uniform vec4 b; uniform mat4 m;\n"
        "out vec4 c;\n"
        "void main() { c = a + b; gl_Position = m * vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 460 core\n"
        "in vec4 c; uniform vec4 tint;\n"
        "out vec4 o;\n"
        "void main() { o = c * tint; }\n";
    char log[512];
    GLuint prog = mgl_build_program(vs, fs, log, sizeof log);
    GLfloat v[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

    (void)i;
    glUseProgram(prog);
    glUniform4fv(glGetUniformLocation(prog, "a"), 1, v);
    glUniform4fv(glGetUniformLocation(prog, "b"), 1, v);
    glUniform4fv(glGetUniformLocation(prog, "tint"), 1, v);
    glUniformMatrix4fv(glGetUniformLocation(prog, "m"), 1, GL_FALSE, v);
    glUseProgram(0);
    glDeleteProgram(prog);
}

// Every stage's glslang program, the per-uniform buffers, the reflected
// resource lists and the Metal program all used to stay behind.
GPU_TEST(memory, deleted_programs_are_given_back)
{
    long grew = growthOver(linkRound, 400);

    CHECK_MSG(grew < 12, "400 programs linked and deleted grew the process by %ld MB", grew);
}

static GLuint s_tex, s_buf, s_shader;

static void texRespecRound(int i)
{
    static GLubyte px[256 * 256 * 4];

    memset(px, i, sizeof px);   // written pages, so a leak shows
    glBindTexture(GL_TEXTURE_2D, s_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
}

// Specifying a level again kept the old copy unless the base size changed.
GPU_TEST(memory, respecified_texture_levels_are_given_back)
{
    glGenTextures(1, &s_tex);

    long grew = growthOver(texRespecRound, 60);

    CHECK_MSG(grew < 16, "60 re-specifications of a 320 KB texture grew the process by %ld MB", grew);
    glDeleteTextures(1, &s_tex);
}

static void bufRespecRound(int i)
{
    static GLubyte data[1 << 20];

    memset(data, i, sizeof data);
    glBindBuffer(GL_ARRAY_BUFFER, s_buf);
    glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_STATIC_DRAW);
}

// A buffer no draw had used yet dropped its old store on glBufferData.
GPU_TEST(memory, respecified_buffers_are_given_back)
{
    glGenBuffers(1, &s_buf);

    long grew = growthOver(bufRespecRound, 60);

    CHECK_MSG(grew < 16, "60 re-specifications of a 1 MB buffer grew the process by %ld MB", grew);
    glDeleteBuffers(1, &s_buf);
}

static void rboRound(int i)
{
    GLuint fbo = 0, rbo = 0;

    (void)i;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 256, 256);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 256, 256);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
}

// A renderbuffer's storage, and each storage it replaced, stayed behind.
GPU_TEST(memory, renderbuffers_are_given_back)
{
    long grew = growthOver(rboRound, 60);

    CHECK_MSG(grew < 16, "60 renderbuffers made, stored twice and deleted grew the process by %ld MB", grew);
}

static void recompileRound(int i)
{
    char src[256];
    const char *p = src;

    snprintf(src, sizeof src,
             "#version 460 core\n"
             "void main() { gl_Position = vec4(%d.0); }\n", i);
    glShaderSource(s_shader, 1, &p, NULL);
    glCompileShader(s_shader);
}

// A successful recompile kept the glslang shader it replaced.
GPU_TEST(memory, recompiled_shaders_are_given_back)
{
    GLint ok = 0;

    s_shader = glCreateShader(GL_VERTEX_SHADER);

    long grew = growthOver(recompileRound, 300);

    glGetShaderiv(s_shader, GL_COMPILE_STATUS, &ok);
    CHECK(ok);
    CHECK_MSG(grew < 16, "300 successful recompiles grew the process by %ld MB", grew);
    glDeleteShader(s_shader);
}

static void objectRound(int i)
{
    GLuint tex[8], vao[8];

    glGenTextures(8, tex);
    glGenVertexArrays(8, vao);

    for (int k = 0; k < 8; k++)
    {
        char label[64];

        glBindTexture(GL_TEXTURE_2D, tex[k]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
        snprintf(label, sizeof label, "texture %d.%d", i, k);
        glObjectLabel(GL_TEXTURE, tex[k], -1, label);
        glBindVertexArray(vao[k]);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteVertexArrays(8, vao);
    glDeleteTextures(8, tex);
}

// Deleted textures and vertex arrays kept their structs, a texture its
// sampler state, and every label stayed in the list.
GPU_TEST(memory, deleted_objects_and_labels_are_given_back)
{
    long grew = growthOver(objectRound, 400);
    GLuint tex = 0;
    GLsizei len = -1;
    char label[64];

    CHECK_MSG(grew < 16, "3200 textures and vertex arrays grew the process by %ld MB", grew);

    // a name used again starts out with no label
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGetObjectLabel(GL_TEXTURE, tex, sizeof label, &len, label);
    CHECK_MSG(len == 0, "a fresh texture came with the label \"%s\"", label);
    glDeleteTextures(1, &tex);
}

extern void *CppCreateMGLRendererHeadless(void *glm_ctx);

// A destroyed context gives back its objects and its renderer.
GPU_TEST(memory, destroyed_contexts_are_given_back)
{
    GLMContext mine = MGLgetCurrentContext();
    long grew;

    for (int pass = 0; pass < 2; pass++)
    {
        size_t before = settledFootprint();

        for (int i = 0; i < (pass ? 10 : 2); i++)
        {
            GLMContext c = createGLMContext(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                                            GL_DEPTH_COMPONENT, GL_FLOAT,
                                            GL_STENCIL_INDEX8, GL_UNSIGNED_BYTE);

            MGLsetCurrentContext(c);
            CppCreateMGLRendererHeadless(c);

            s_tex = 0;
            glGenTextures(1, &s_tex);
            texRespecRound(i);
            glGenBuffers(1, &s_buf);
            bufRespecRound(i);
            linkRound(i);
            glFinish();

            destroyGLMContext(c);
        }

        MGLsetCurrentContext(mine);

        size_t after = settledFootprint();

        grew = (long)(after > before ? after - before : 0) / (1024 * 1024);
    }

    CHECK_MSG(grew < 24, "10 contexts made and destroyed grew the process by %ld MB", grew);

    // the test's own context still works
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    CHECK(glIsTexture(t));
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &t);
}

static void defaultTextureRound(int i)
{
    (void)i;

    for (int k = 0; k < 200; k++)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
}

// Texture 0 is one object per target, and its state lasts. A new one was made
// every time 0 was bound and then used, and the conformance suite's reset
// between cases made 400,000 of them in 300 cases.
GPU_TEST(memory, the_default_texture_is_one_object)
{
    GLuint other = 0;
    GLint wrap = 0;

    glBindTexture(GL_TEXTURE_2D, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);

    glGenTextures(1, &other);
    glBindTexture(GL_TEXTURE_2D, other);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
    CHECK_EQ_INT(GL_MIRRORED_REPEAT, wrap);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glDeleteTextures(1, &other);

    long grew = growthOver(defaultTextureRound, 100);

    CHECK_MSG(grew < 4, "20,000 binds of texture 0 grew the process by %ld MB", grew);
}
