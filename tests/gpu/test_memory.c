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
#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

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
