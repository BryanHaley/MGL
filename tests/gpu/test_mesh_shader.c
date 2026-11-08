/*
 * test_mesh_shader.c
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
 * GL_EXT_mesh_shader on Metal's object and mesh stages.
 */

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"
#include "glm_context.h"

static GLuint compileOne(GLenum stage, const char *src, char *log, int log_size)
{
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok)
        glGetShaderInfoLog(sh, log_size, NULL, log);
    return ok ? sh : 0;
}

static GLuint linkMesh(const char *task, const char *mesh, const char *fs, char *log, int log_size)
{
    GLuint prog = glCreateProgram(), t = 0, m, f;
    GLint ok = 0;

    log[0] = 0;
    if (task && !(t = compileOne(GL_TASK_SHADER_EXT, task, log, log_size)))
        return 0;
    if (!(m = compileOne(GL_MESH_SHADER_EXT, mesh, log, log_size)) ||
        !(f = compileOne(GL_FRAGMENT_SHADER, fs, log, log_size)))
        return 0;
    if (t)
        glAttachShader(prog, t);
    glAttachShader(prog, m);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
        glGetProgramInfoLog(prog, log_size, NULL, log);
    return ok ? prog : 0;
}

static const char *fs =
    "#version 460\n"
    "#extension GL_EXT_mesh_shader : require\n"
    "layout(location = 0) in perprimitiveEXT vec4 color;\n"
    "layout(location = 0) out vec4 o;\n"
    "void main() { o = color; }\n";

// Each work group puts one triangle over its own half of the target, its
// corners read from a uniform block and its colour from a storage buffer.
static const char *mesh =
    "#version 460\n"
    "#extension GL_EXT_mesh_shader : require\n"
    "layout(local_size_x = 32) in;\n"
    "layout(triangles, max_vertices = 3, max_primitives = 1) out;\n"
    "layout(binding = 0) uniform Corners { vec4 corner[6]; };\n"
    "layout(binding = 1, std430) readonly buffer Colors { vec4 col[2]; };\n"
    "layout(location = 0) out perprimitiveEXT vec4 color[];\n"
    "GROUP_IN\n"
    "void main() {\n"
    "    uint g = GROUP;\n"
    "    SetMeshOutputsEXT(3u, 1u);\n"
    "    uint i = gl_LocalInvocationIndex;\n"
    "    if (i < 3u) gl_MeshVerticesEXT[i].gl_Position = corner[g * 3u + i];\n"
    "    if (i == 0u) { gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2); color[0] = col[g]; }\n"
    "}\n";

static void drawAndCount(const char *task, const char *mesh_src, GLuint groups, int *green, int *blue)
{
    static const GLfloat corners[24] = { -1,-1,0,1,  0,-1,0,1,  -1,1,0,1,   0,-1,0,1,  1,-1,0,1,  0,1,0,1 };
    static const GLfloat colors[8] = { 0,1,0,1,  0,0,1,1 };
    void (*draw)(GLuint, GLuint, GLuint) = (void (*)(GLuint, GLuint, GLuint))dlsym(RTLD_DEFAULT, "glDrawMeshTasksEXT");
    MGLTestTarget t;
    GLuint prog, buf[2];
    unsigned char *px;
    char log[4096];

    *green = *blue = -1;
    prog = linkMesh(task, mesh_src, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "mesh program did not link: %s", log);
    CHECK(draw != NULL);
    if (!prog || !draw || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
        return;

    glGenBuffers(2, buf);
    glBindBuffer(GL_UNIFORM_BUFFER, buf[0]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof corners, corners, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, buf[0]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof colors, colors, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buf[1]);

    mgl_target_bind(&t);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    draw(groups, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    *green = *blue = 0;
    if (px)
    {
        for (int i = 0; i < 64 * 64; i++)
        {
            *green += px[i * 4 + 1] == 255 && px[i * 4 + 2] == 0;
            *blue += px[i * 4 + 2] == 255 && px[i * 4 + 1] == 0;
        }
        free(px);
    }

    glUseProgram(0);
    glDeleteBuffers(2, buf);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

static char *subst(const char *src, const char *in, const char *group)
{
    static char out[4096];
    const char *a = strstr(src, "GROUP_IN"), *b = strstr(src, "uint g = GROUP;");

    snprintf(out, sizeof out, "%.*s%s%.*suint g = %s;%s", (int)(a - src), src, in,
             (int)(b - a - 8), a + 8, group, b + strlen("uint g = GROUP;"));
    return out;
}

GPU_TEST(mesh_shader, two_mesh_groups_draw_their_triangles)
{
    int green, blue;

    drawAndCount(NULL, subst(mesh, "", "gl_WorkGroupID.x"), 2, &green, &blue);
    // each right triangle covers half of its 32x64 half
    CHECK_MSG(green > 900 && green < 1200 && blue > 900 && blue < 1200, "green %d, blue %d", green, blue);
}

GPU_TEST(mesh_shader, a_task_shader_launches_mesh_groups_with_its_payload)
{
    static const char *task =
        "#version 460\n"
        "#extension GL_EXT_mesh_shader : require\n"
        "layout(local_size_x = 4) in;\n"
        "struct Task { uint which; };\n"
        "taskPayloadSharedEXT Task td;\n"
        "void main() {\n"
        "    if (gl_LocalInvocationIndex == 0u) td.which = 1u - gl_WorkGroupID.x;\n"
        "    EmitMeshTasksEXT(1u, 1u, 1u);\n"
        "}\n";
    int green, blue;

    drawAndCount(task, subst(mesh, "struct Task { uint which; };\ntaskPayloadSharedEXT Task td;", "td.which"),
                 2, &green, &blue);
    CHECK_MSG(green > 900 && green < 1200 && blue > 900 && blue < 1200, "green %d, blue %d", green, blue);
}

// Each draw of a multi-draw picks its triangle by gl_DrawID.
GPU_TEST(mesh_shader, multi_draw_indirect_hands_each_draw_its_id)
{
    void (*multi)(GLintptr, GLsizei, GLsizei) =
        (void (*)(GLintptr, GLsizei, GLsizei))dlsym(RTLD_DEFAULT, "glMultiDrawMeshTasksIndirectEXT");
    static const GLuint cmds[6] = { 1, 1, 1, 1, 1, 1 };
    static const GLfloat corners[24] = { -1,-1,0,1,  0,-1,0,1,  -1,1,0,1,   0,-1,0,1,  1,-1,0,1,  0,1,0,1 };
    static const GLfloat colors[8] = { 0,1,0,1,  0,0,1,1 };
    MGLTestTarget t;
    GLuint prog, buf[3];
    unsigned char *px;
    char log[4096];
    int green = 0, blue = 0;

    prog = linkMesh(NULL, subst(mesh, "", "uint(gl_DrawID)"), fs, log, sizeof log);
    CHECK_MSG(prog != 0, "mesh program did not link: %s", log);
    CHECK(multi != NULL);
    if (!prog || !multi || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
        return;

    glGenBuffers(3, buf);
    glBindBuffer(GL_UNIFORM_BUFFER, buf[0]);
    glBufferData(GL_UNIFORM_BUFFER, sizeof corners, corners, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, buf[0]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof colors, colors, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buf[1]);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buf[2]);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof cmds, cmds, GL_STATIC_DRAW);

    mgl_target_bind(&t);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    multi(0, 2, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        for (int i = 0; i < 64 * 64; i++)
        {
            green += px[i * 4 + 1] == 255 && px[i * 4 + 2] == 0;
            blue += px[i * 4 + 2] == 255 && px[i * 4 + 1] == 0;
        }
        free(px);
    }
    CHECK_MSG(green > 900 && green < 1200 && blue > 900 && blue < 1200, "green %d, blue %d", green, blue);

    glUseProgram(0);
    glDeleteBuffers(3, buf);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
