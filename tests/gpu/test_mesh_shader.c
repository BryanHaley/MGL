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
#include <math.h>
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

// GL_NV_mesh_shader as engines write it: a task block with 8-bit members, the
// task count set by one invocation, packed indices, a per-primitive member in
// the vertex block, a task block named like that block, and draws that start
// at a first group.
static const char *nv_task =
    "#version 460\n"
    "#extension GL_NV_mesh_shader : require\n"
    "#extension GL_NV_gpu_shader5 : require\n"
    "#extension GL_KHR_shader_subgroup_ballot : require\n"
    "layout(local_size_x = 32) in;\n"
    "taskNV out V { uint Base; uint8_t Face; uint Mul; uint8_t Quads[32]; } outData;\n"
    "layout(location = 0) uniform uint one;\n"
    "void main() {\n"
    "    if (gl_GlobalInvocationID.x >= 64u) return;\n"
    "    bool keep = gl_LocalInvocationIndex < 2u;\n"
    "    uvec4 m = subgroupBallot(keep);\n"
    "    if (keep) outData.Quads[subgroupBallotExclusiveBitCount(m)] = uint8_t(gl_LocalInvocationIndex);\n"
    "    if (gl_LocalInvocationIndex == 0u) { outData.Base = (gl_WorkGroupID.x + uint(gl_DrawID)) * 2u * one; outData.Face = uint8_t(3); outData.Mul = 1u; gl_TaskCountNV = subgroupBallotBitCount(m); }\n"
    "}\n";

static const char *nv_mesh =
    "#version 460\n"
    "#extension GL_NV_mesh_shader : require\n"
    "#extension GL_NV_gpu_shader5 : require\n"
    "layout(local_size_x = 32) in;\n"
    "layout(triangles, max_vertices = 4, max_primitives = 3) out;\n"
    "out V { vec2 uv; perprimitiveNV uint quad; } v[4];\n"
    "taskNV in V { uint Base; uint8_t Face; uint Mul; uint8_t Quads[32]; } inData;\n"
    "layout(location = 0) uniform uint one;\n"
    "void main() {\n"
    "    uint q = inData.Base + uint(inData.Quads[gl_WorkGroupID.x]);\n"
    "    vec2 o = vec2(float(q & 1u), float(q >> 1u)) - 1.0;\n"
    "    uint i = gl_LocalInvocationIndex;\n"
    "    if (i < 4u) { gl_MeshVerticesNV[i].gl_Position = vec4(o + vec2(float(i & 1u), float(i >> 1u)), 0, 1); v[i].uv = vec2(i); }\n"
    "    if (i == 0u) writePackedPrimitiveIndices4x8NV(0u, 0x01020100u);\n"
    "    if (i == 1u) writePackedPrimitiveIndices4x8NV(4u, 0x00000203u);\n"
    "    if (i < 2u) v[i].quad = q * one * inData.Mul * (uint(inData.Face) == 3u ? 1u : 0u);\n"
    "    if (i == 0u) gl_PrimitiveCountNV = 2u;\n"
    "}\n";

static const char *nv_fs =
    "#version 460\n"
    "in V { vec2 uv; flat uint quad; } v;\n"
    "layout(location = 0) out vec4 o;\n"
    "void main() { o = vec4(float(v.quad + 1u) / 4.0, 0, 0, 1); }\n";

// red in each quadrant's centre, -1 when unreadable
static void quadrantReds(MGLTestTarget *t, int red[4])
{
    unsigned char *px = mgl_read_rgba8(t);

    for (int q = 0; q < 4; q++)
        red[q] = px ? px[((16 + 32 * (q >> 1)) * 64 + 16 + 32 * (q & 1)) * 4] : -1;
    free(px);
}

GPU_TEST(mesh_shader, nv_task_and_mesh_shaders_draw_from_first_groups)
{
    void (*draw)(GLuint, GLuint) = (void (*)(GLuint, GLuint))dlsym(RTLD_DEFAULT, "glDrawMeshTasksNV");
    void (*drawCount)(GLintptr, GLintptr, GLsizei, GLsizei) =
        (void (*)(GLintptr, GLintptr, GLsizei, GLsizei))dlsym(RTLD_DEFAULT, "glMultiDrawMeshTasksIndirectCountNV");
    static const GLuint cmds[4] = { 1, 0, 1, 0 }, two = 2;
    const char *ext;
    bool listed = false;
    GLint n = 0, maxDraw = 0;
    MGLTestTarget t;
    GLuint prog, buf[2];
    int red[4];
    char log[4096];

    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; i++)
        if ((ext = (const char *)glGetStringi(GL_EXTENSIONS, i)) && !strcmp(ext, "GL_NV_mesh_shader"))
            listed = true;
    CHECK(listed);
    glGetIntegerv(GL_MAX_DRAW_MESH_TASKS_COUNT_NV, &maxDraw);
    CHECK(maxDraw >= 65535);

    prog = linkMesh(nv_task, nv_mesh, nv_fs, log, sizeof log);
    CHECK_MSG(prog != 0, "NV mesh program did not link: %s", log);
    CHECK(draw != NULL && drawCount != NULL);
    if (!prog || !draw || !drawCount || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
        return;

    mgl_target_bind(&t);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glUniform1ui(0, 1);
    draw(0, 1);
    draw(1, 1);
    draw(5, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    quadrantReds(&t, red);
    for (int q = 0; q < 4; q++)
        CHECK_MSG(abs(red[q] - (q + 1) * 255 / 4) <= 2, "direct draw, quadrant %d red %d", q, red[q]);

    glGenBuffers(2, buf);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buf[0]);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof cmds, cmds, GL_STATIC_DRAW);
    glBindBuffer(GL_PARAMETER_BUFFER, buf[1]);
    glBufferData(GL_PARAMETER_BUFFER, sizeof two, &two, GL_STATIC_DRAW);
    glClear(GL_COLOR_BUFFER_BIT);
    drawCount(0, 0, 2, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    quadrantReds(&t, red);
    for (int q = 0; q < 4; q++)
        CHECK_MSG(abs(red[q] - (q + 1) * 255 / 4) <= 2, "indirect count draw, quadrant %d red %d", q, red[q]);

    glUseProgram(0);
    glDeleteBuffers(2, buf);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// A uniform set between two mesh draws in one pass reaches the second draw.
GPU_TEST(mesh_shader, uniforms_set_between_draws_reach_the_mesh_stage)
{
    static const char *m =
        "#version 460\n"
        "#extension GL_EXT_mesh_shader : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(triangles, max_vertices = 4, max_primitives = 2) out;\n"
        "uniform uint quad;\n"
        "layout(location = 0) out perprimitiveEXT vec4 color[];\n"
        "void main() {\n"
        "    vec2 o = vec2(float(quad & 1u), float(quad >> 1u)) - 1.0;\n"
        "    SetMeshOutputsEXT(4u, 2u);\n"
        "    for (uint i = 0u; i < 4u; i++) gl_MeshVerticesEXT[i].gl_Position = vec4(o + vec2(float(i & 1u), float(i >> 1u)), 0, 1);\n"
        "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
        "    gl_PrimitiveTriangleIndicesEXT[1] = uvec3(1, 3, 2);\n"
        "    color[0] = color[1] = vec4(float(quad + 1u) / 4.0, 0, 0, 1);\n"
        "}\n";
    void (*draw)(GLuint, GLuint, GLuint) = (void (*)(GLuint, GLuint, GLuint))dlsym(RTLD_DEFAULT, "glDrawMeshTasksEXT");
    MGLTestTarget t;
    GLuint prog;
    int red[4];
    char log[4096];

    prog = linkMesh(NULL, m, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "mesh program did not link: %s", log);
    if (!prog || !draw || !mgl_target_create(&t, 64, 64, GL_RGBA8, 0))
        return;

    mgl_target_bind(&t);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    for (GLuint q = 0; q < 4; q++)
    {
        glUniform1ui(glGetUniformLocation(prog, "quad"), q);
        draw(1, 1, 1);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    quadrantReds(&t, red);
    for (int q = 0; q < 4; q++)
        CHECK_MSG(abs(red[q] - (q + 1) * 255 / 4) <= 2, "quadrant %d red %d", q, red[q]);

    glUseProgram(0);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// Each primitive picks its layer of a layered target, as cube shadow maps do.
GPU_TEST(mesh_shader, primitives_choose_their_layer)
{
    static const char *m =
        "#version 460\n"
        "#extension GL_EXT_mesh_shader : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(triangles, max_vertices = 6, max_primitives = 2) out;\n"
        "layout(location = 0) out perprimitiveEXT vec4 color[];\n"
        "void main() {\n"
        "    SetMeshOutputsEXT(6u, 2u);\n"
        "    for (uint p = 0u; p < 2u; p++) {\n"
        "        gl_MeshVerticesEXT[p * 3u + 0u].gl_Position = vec4(-1, -1, 0, 1);\n"
        "        gl_MeshVerticesEXT[p * 3u + 1u].gl_Position = vec4( 3, -1, 0, 1);\n"
        "        gl_MeshVerticesEXT[p * 3u + 2u].gl_Position = vec4(-1,  3, 0, 1);\n"
        "        gl_PrimitiveTriangleIndicesEXT[p] = uvec3(p * 3u, p * 3u + 1u, p * 3u + 2u);\n"
        "        gl_MeshPrimitivesEXT[p].gl_Layer = int(p) + 1;\n"
        "        color[p] = p == 0u ? vec4(1, 0, 0, 1) : vec4(0, 1, 0, 1);\n"
        "    }\n"
        "}\n";
    void (*draw)(GLuint, GLuint, GLuint) = (void (*)(GLuint, GLuint, GLuint))dlsym(RTLD_DEFAULT, "glDrawMeshTasksEXT");
    GLuint prog, tex, fbo;
    GLubyte px[4];
    char log[4096];

    prog = linkMesh(NULL, m, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "mesh program did not link: %s", log);
    if (!prog || !draw)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 8, 8, 3);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    glViewport(0, 0, 8, 8);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    draw(1, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int layer = 0; layer < 3; layer++)
    {
        static const GLubyte want[3][3] = { { 0, 0, 255 }, { 255, 0, 0 }, { 0, 255, 0 } };

        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, layer);
        glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        CHECK_MSG(px[0] == want[layer][0] && px[1] == want[layer][1] && px[2] == want[layer][2],
                  "layer %d is %d,%d,%d", layer, px[0], px[1], px[2]);
    }

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
}

// The same into a depth cube map with no colour, as point light shadows do.
GPU_TEST(mesh_shader, primitives_choose_their_cube_face_for_depth)
{
    static const char *m =
        "#version 460\n"
        "#extension GL_EXT_mesh_shader : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(triangles, max_vertices = 18, max_primitives = 6) out;\n"
        "void main() {\n"
        "    SetMeshOutputsEXT(18u, 6u);\n"
        "    for (uint p = 0u; p < 6u; p++) {\n"
        "        float z = float(p) / 10.0;\n"
        "        gl_MeshVerticesEXT[p * 3u + 0u].gl_Position = vec4(-1, -1, z, 1);\n"
        "        gl_MeshVerticesEXT[p * 3u + 1u].gl_Position = vec4( 3, -1, z, 1);\n"
        "        gl_MeshVerticesEXT[p * 3u + 2u].gl_Position = vec4(-1,  3, z, 1);\n"
        "        gl_PrimitiveTriangleIndicesEXT[p] = uvec3(p * 3u, p * 3u + 1u, p * 3u + 2u);\n"
        "        gl_MeshPrimitivesEXT[p].gl_Layer = int(p);\n"
        "    }\n"
        "}\n";
    static const char *empty_fs = "#version 460\nvoid main() {}\n";
    void (*draw)(GLuint, GLuint, GLuint) = (void (*)(GLuint, GLuint, GLuint))dlsym(RTLD_DEFAULT, "glDrawMeshTasksEXT");
    GLuint prog, tex, fbo;
    char log[4096];

    prog = linkMesh(NULL, m, empty_fs, log, sizeof log);
    CHECK_MSG(prog != 0, "mesh program did not link: %s", log);
    if (!prog || !draw)
        return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_DEPTH_COMPONENT32F, 8, 8);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glViewport(0, 0, 8, 8);
    glEnable(GL_DEPTH_TEST);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(prog);
    draw(1, 1, 1);
    glDisable(GL_DEPTH_TEST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int face = 0; face < 6; face++)
    {
        GLfloat d = -1;

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, tex, 0);
        glReadPixels(4, 4, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
        CHECK_MSG(d > face / 20.0f + 0.5f - 0.01f && d < face / 20.0f + 0.5f + 0.01f, "face %d depth %f", face, d);
    }

    // with glClipControl's 0..1 depth, z is the depth
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0);
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    draw(1, 1, 1);
    glDisable(GL_DEPTH_TEST);
    glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
    for (int face = 0; face < 6; face++)
    {
        GLfloat d = -1;

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, tex, 0);
        glReadPixels(4, 4, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
        CHECK_MSG(d > face / 10.0f - 0.01f && d < face / 10.0f + 0.01f, "0..1 clip, face %d depth %f", face, d);
    }

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
}

// The same through the task stage, set with glProgramUniform before each draw.
GPU_TEST(mesh_shader, task_uniforms_set_between_draws_reach_the_task_stage)
{
    static const char *t =
        "#version 460\n"
        "#extension GL_EXT_mesh_shader : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(location = 0) uniform uint quad;\n"
        "struct P { uint q; };\n"
        "taskPayloadSharedEXT P p;\n"
        "void main() { p.q = quad; EmitMeshTasksEXT(1u, 1u, 1u); }\n";
    static const char *m =
        "#version 460\n"
        "#extension GL_EXT_mesh_shader : require\n"
        "layout(local_size_x = 1) in;\n"
        "layout(triangles, max_vertices = 4, max_primitives = 2) out;\n"
        "struct P { uint q; };\n"
        "taskPayloadSharedEXT P p;\n"
        "layout(location = 0) out perprimitiveEXT vec4 color[];\n"
        "void main() {\n"
        "    vec2 o = vec2(float(p.q & 1u), float(p.q >> 1u)) - 1.0;\n"
        "    SetMeshOutputsEXT(4u, 2u);\n"
        "    for (uint i = 0u; i < 4u; i++) gl_MeshVerticesEXT[i].gl_Position = vec4(o + vec2(float(i & 1u), float(i >> 1u)), 0, 1);\n"
        "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);\n"
        "    gl_PrimitiveTriangleIndicesEXT[1] = uvec3(1, 3, 2);\n"
        "    color[0] = color[1] = vec4(float(p.q + 1u) / 4.0, 0, 0, 1);\n"
        "}\n";
    void (*draw)(GLuint, GLuint, GLuint) = (void (*)(GLuint, GLuint, GLuint))dlsym(RTLD_DEFAULT, "glDrawMeshTasksEXT");
    MGLTestTarget tg;
    GLuint prog;
    int red[4];
    char log[4096];

    prog = linkMesh(t, m, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "mesh program did not link: %s", log);
    if (!prog || !draw || !mgl_target_create(&tg, 64, 64, GL_RGBA8, 0))
        return;

    mgl_target_bind(&tg);
    glViewport(0, 0, 64, 64);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    for (GLuint q = 0; q < 4; q++)
    {
        glProgramUniform1ui(prog, 0, q);
        glUseProgram(prog);
        draw(1, 1, 1);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    quadrantReds(&tg, red);
    for (int q = 0; q < 4; q++)
        CHECK_MSG(abs(red[q] - (q + 1) * 255 / 4) <= 2, "quadrant %d red %d", q, red[q]);

    glUseProgram(0);
    glDeleteProgram(prog);
    mgl_target_destroy(&tg);
}

// The point light shadow pass of a meshlet renderer, end to end: a compute
// pass culls (instance, face) pairs into a command list and a count, the NV
// count draw runs one draw per pair, each task group culls its meshlets with a
// ballot, and each mesh group fills one cell of its face.
GPU_TEST(mesh_shader, nv_culled_cube_shadow_pass_fills_the_right_cells)
{
    static const char *cull =
        "#version 460\n"
        "layout(local_size_x = 18) in;\n"
        "layout(std430, binding = 1) buffer Ids { uint ids[]; };\n"
        "layout(std430, binding = 2) buffer Cmds { uvec2 cmds[]; };\n"
        "layout(std430, binding = 3) buffer Count { uint count; };\n"
        "void main() {\n"
        "    uint inst = gl_LocalInvocationIndex / 6u, face = gl_LocalInvocationIndex % 6u;\n"
        "    if (face == 4u) return;\n"
        "    uint k = atomicAdd(count, 1u);\n"
        "    ids[k] = (face << 29) | inst;\n"
        "    cmds[k].x = 2u;\n"
        "}\n";
    static const char *task =
        "#version 460\n"
        "#extension GL_NV_mesh_shader : require\n"
        "#extension GL_NV_gpu_shader5 : require\n"
        "#extension GL_KHR_shader_subgroup_ballot : require\n"
        "layout(local_size_x = 32) in;\n"
        "layout(std430, binding = 1) readonly buffer Ids { uint ids[]; };\n"
        "layout(location = 0) uniform int shadowIndex;\n"
        "taskNV out Data { uint Inst; uint Pad; uint8_t Face; uint First; uint8_t Keep[32]; } outData;\n"
        "void main() {\n"
        "    uint id = ids[gl_DrawID];\n"
        "    uint8_t face = uint8_t(id >> 29);\n"
        "    uint inst = id & ((1u << 29) - 1u);\n"
        "    if (gl_GlobalInvocationID.x >= 64u) return;\n"
        "    uint cell = gl_GlobalInvocationID.x;\n"
        "    bool keep = (cell % 3u) != (uint(face) % 3u) && shadowIndex == 2;\n"
        "    uvec4 m = subgroupBallot(keep);\n"
        "    if (keep) outData.Keep[subgroupBallotExclusiveBitCount(m)] = uint8_t(gl_LocalInvocationIndex);\n"
        "    if (gl_LocalInvocationIndex == 0u) {\n"
        "        outData.Inst = inst; outData.Face = face; outData.First = gl_WorkGroupID.x * 32u;\n"
        "        gl_TaskCountNV = subgroupBallotBitCount(m);\n"
        "    }\n"
        "}\n";
    static const char *mesh =
        "#version 460\n"
        "#extension GL_NV_mesh_shader : require\n"
        "#extension GL_NV_gpu_shader5 : require\n"
        "layout(local_size_x = 32) in;\n"
        "layout(triangles, max_vertices = 64, max_primitives = 125) out;\n"
        "layout(location = 0) uniform int shadowIndex;\n"
        "taskNV in Data { uint Inst; uint Pad; uint8_t Face; uint First; uint8_t Keep[32]; } inData;\n"
        "void main() {\n"
        "    uint cell = inData.First + uint(inData.Keep[gl_WorkGroupID.x]);\n"
        "    vec2 o = vec2(float(cell % 8u), float(cell / 8u)) / 4.0 - 1.0;\n"
        "    float z = 0.1 + 0.1 * float(inData.Inst) + 0.01 * float(shadowIndex - 2);\n"
        "    uint i = gl_LocalInvocationIndex;\n"
        "    for (uint v = i; v < 64u; v += 32u)\n"
        "        gl_MeshVerticesNV[v].gl_Position = vec4(o + vec2(float(v & 1u), float((v >> 1) & 1u)) / 4.0, z, 1);\n"
        "    for (uint p = i; p < 31u; p += 32u) writePackedPrimitiveIndices4x8NV(p * 4u, p == 0u ? 0x01020100u : 0x00000203u);\n"
        "    for (uint p = i; p < 125u; p += 32u) gl_MeshPrimitivesNV[p].gl_Layer = int(inData.Face);\n"
        "    if (i == 0u) gl_PrimitiveCountNV = 2u;\n"
        "}\n";
    static const char *empty_fs = "#version 460\nvoid main() {}\n";
    void (*drawCount)(GLintptr, GLintptr, GLsizei, GLsizei) =
        (void (*)(GLintptr, GLintptr, GLsizei, GLsizei))dlsym(RTLD_DEFAULT, "glMultiDrawMeshTasksIndirectCountNV");
    GLuint cullProg, prog, tex, fbo, bufs[3], zero = 0;
    char log[4096];

    cullProg = mgl_build_compute_program(cull, log, sizeof log);
    CHECK_MSG(cullProg != 0, "cull program did not link: %s", log);
    prog = linkMesh(task, mesh, empty_fs, log, sizeof log);
    CHECK_MSG(prog != 0, "NV mesh program did not link: %s", log);
    if (!cullProg || !prog || !drawCount)
        return;

    glGenBuffers(3, bufs);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, 18 * 4, NULL, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, 18 * 8, NULL, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[2]);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, 4, NULL, GL_DYNAMIC_STORAGE_BIT);
    for (int i = 0; i < 3; i++)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, i + 1, bufs[i]);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_DEPTH_COMPONENT32F, 32, 32);
    glGenFramebuffers(1, &fbo);

    // three lights' worth of passes into the same map; only the last keeps cells
    for (int light = 0; light < 3; light++)
    {
        glNamedBufferSubData(bufs[2], 0, 4, &zero);
        glUseProgram(cullProg);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0);
        glDrawBuffer(GL_NONE);
        glViewport(0, 0, 32, 32);
        glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
        glEnable(GL_DEPTH_TEST);
        if (light == 0)
        {
            glClearDepth(1.0);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
        glProgramUniform1i(prog, 0, light);
        glUseProgram(prog);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, bufs[1]);
        glBindBuffer(GL_PARAMETER_BUFFER, bufs[2]);
        drawCount(0, 0, 18, 0);
        glDisable(GL_DEPTH_TEST);
        glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    for (int face = 0; face < 6; face++)
    {
        GLfloat d[32 * 32];
        int wrong = 0;

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, tex, 0);
        glReadPixels(0, 0, 32, 32, GL_DEPTH_COMPONENT, GL_FLOAT, d);
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++)
            {
                int cell = (y / 4) * 8 + x / 4;
                float want = face == 4 || cell % 3 == face % 3 ? 1.0f : 0.1f;

                wrong += fabsf(d[y * 32 + x] - want) > 0.001f;
            }
        CHECK_MSG(wrong == 0, "face %d: %d of 1024 texels wrong", face, wrong);
    }

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(3, bufs);
    glDeleteProgram(prog);
    glDeleteProgram(cullProg);
}

#include "mesh_meshlet_data.h"

// A real shadow meshlet, many triangles crossing behind the light, draws the
// same with a per-primitive layer as without, while other draws keep the GPU
// busy: Metal's payload and mesh grid sizes have to be what the shader needs.
static void drawMeshlet(bool layered, bool nv, int light, GLfloat *depth)
{
    static const char *nv_src =
        "#version 460\n"
        "#extension GL_NV_mesh_shader : require\n"
        "#extension GL_NV_gpu_shader5 : require\n"
        "layout(local_size_x = 32) in;\n"
        "layout(triangles, max_primitives = 253, max_vertices = 128) out;\n"
        "layout(std430, binding = 0) readonly buffer P { vec4 pos[128]; };\n"
        "layout(std430, binding = 1) readonly buffer I { uint idx[324]; };\n"
        "taskNV in Data { uint8_t Face; } inData;\n"
        "void main() {\n"
        "    for (int i = 0; i < 4; i++) {\n"
        "        uint8_t v = uint8_t(min(gl_LocalInvocationIndex + i * 32u, 127u));\n"
        "        gl_MeshVerticesNV[v].gl_Position = pos[v];\n"
        "    }\n"
        "    for (uint i = 0u; i < 6u; i++) {\n"
        "        uint w = gl_LocalInvocationIndex + i * 32u, at = min(w * 4u, 324u);\n"
        "        uint p = 0u;\n"
        "        for (uint k = 0u; k < 4u; k++) if (w * 4u + k < 324u) p |= idx[w * 4u + k] << (8u * k);\n"
        "        writePackedPrimitiveIndices4x8NV(at, p);\n"
        "    }\n"
        "    LAYER\n"
        "    if (gl_LocalInvocationIndex == 0u) gl_PrimitiveCountNV = 108u;\n"
        "}\n";
    static const char *nv_layer =
        "for (int i = 0; i < 8; i++) {\n"
        "        uint8_t t = uint8_t(min(gl_LocalInvocationIndex + i * 32u, 107u));\n"
        "        gl_MeshPrimitivesNV[t].gl_Layer = int(inData.Face);\n"
        "    }";
    static const char *nv_task =
        "#version 460\n"
        "#extension GL_NV_mesh_shader : require\n"
        "#extension GL_NV_gpu_shader5 : require\n"
        "layout(local_size_x = 32) in;\n"
        "taskNV out Data { uint8_t Face; } outData;\n"
        "layout(std430, binding = 0) readonly buffer P { vec4 pos[128]; };\n"
        "void main() {\n"
        "    vec4 acc = vec4(0);\n"
        "    for (uint k = 0u; k < 256u; k++) acc += pos[(k + gl_LocalInvocationIndex) & 127u] * float(k);\n"
        "    if (gl_LocalInvocationIndex == 0u) {\n"
        "        outData.Face = uint8_t(2);\n"
        "        gl_TaskCountNV = gl_DrawID == 5 && acc.x != 12345.0 ? 1u : 0u;\n"
        "    }\n"
        "}\n";
    static const char *head =
        "#version 460\n"
        "#extension GL_EXT_mesh_shader : require\n"
        "layout(local_size_x = 32) in;\n"
        "layout(triangles, max_vertices = 128, max_primitives = 108) out;\n"
        "layout(std430, binding = 0) readonly buffer P { vec4 pos[128]; };\n"
        "layout(std430, binding = 1) readonly buffer I { uint idx[324]; };\n"
        "void main() {\n"
        "    uint i = gl_LocalInvocationIndex;\n"
        "    SetMeshOutputsEXT(128u, 108u);\n"
        "    for (uint v = i; v < 128u; v += 32u) gl_MeshVerticesEXT[v].gl_Position = pos[v];\n"
        "    for (uint t = i; t < 108u; t += 32u) {\n"
        "        gl_PrimitiveTriangleIndicesEXT[t] = uvec3(idx[3u * t], idx[3u * t + 1u], idx[3u * t + 2u]);\n";
    static const char *empty_fs = "#version 460\nvoid main() {}\n";
    void (*draw)(GLuint, GLuint, GLuint) = (void (*)(GLuint, GLuint, GLuint))dlsym(RTLD_DEFAULT, "glDrawMeshTasksEXT");
    void (*drawCount)(GLintptr, GLintptr, GLsizei, GLsizei) =
        (void (*)(GLintptr, GLintptr, GLsizei, GLsizei))dlsym(RTLD_DEFAULT, "glMultiDrawMeshTasksIndirectCountNV");
    char src[4096], log[4096];
    GLuint prog, tex, fbo, bufs[2], idx[324];

    if (nv)
    {
        const char *mark = strstr(nv_src, "LAYER");

        snprintf(src, sizeof src, "%.*s%s%s", (int)(mark - nv_src), nv_src, layered ? nv_layer : "", mark + 5);
    }
    else
        snprintf(src, sizeof src, "%s%s", head,
                 layered ? "        gl_MeshPrimitivesEXT[t].gl_Layer = 2;\n    }\n}\n" : "    }\n}\n");
    prog = linkMesh(nv ? nv_task : NULL, src, empty_fs, log, sizeof log);
    CHECK_MSG(prog != 0, "mesh program did not link: %s", log);
    if (!prog || !draw)
        return;

    for (int k = 0; k < 324; k++)
        idx[k] = mlIdx[k];
    glGenBuffers(2, bufs);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[0]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof mlPos[0], mlPos[light], GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufs[0]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufs[1]);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof idx, idx, GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufs[1]);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_DEPTH_COMPONENT16, 128, 128);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tex, 0);
    glDrawBuffer(GL_NONE);
    glViewport(0, 0, 128, 128);
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
    glEnable(GL_DEPTH_TEST);
    for (int face = 0; face < 6; face++)
    {
        static const GLfloat one = 1.0f;

        glClearTexSubImage(tex, 0, 0, 0, face, 128, 128, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &one);
    }
    glUseProgram(prog);
    if (nv)
    {
        static GLuint cmds[4000];
        GLuint count = 2000, cbuf[2];

        for (int k = 0; k < 2000; k++)
            cmds[k * 2] = 64, cmds[k * 2 + 1] = 0;
        glGenBuffers(2, cbuf);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, cbuf[0]);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof cmds, cmds, GL_STATIC_DRAW);
        glBindBuffer(GL_PARAMETER_BUFFER, cbuf[1]);
        glBufferData(GL_PARAMETER_BUFFER, sizeof count, &count, GL_STATIC_DRAW);
        drawCount(0, 0, 2000, 0);
        glDeleteBuffers(2, cbuf);
    }
    else
        draw(1, 1, 1);
    glDisable(GL_DEPTH_TEST);
    glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           layered ? GL_TEXTURE_CUBE_MAP_POSITIVE_Y : GL_TEXTURE_CUBE_MAP_POSITIVE_X, tex, 0);
    glReadPixels(0, 0, 128, 128, GL_DEPTH_COMPONENT, GL_FLOAT, depth);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(2, bufs);
    glDeleteProgram(prog);
}

GPU_TEST(mesh_shader, a_shadow_meshlet_draws_the_same_with_a_layer)
{
    static GLfloat plain[128 * 128], layered[128 * 128];

    for (int run = 0; run < 4; run++)
    {
        int nv = run & 1, light = run >> 1, wrong = 0, drawn = 0;

        drawMeshlet(false, nv, light, plain);
        drawMeshlet(true, nv, light, layered);
        for (int k = 0; k < 128 * 128; k++)
        {
            wrong += fabsf(plain[k] - layered[k]) > 1e-5f;
            drawn += plain[k] < 1.0f;
        }
        CHECK_MSG(drawn > 1000, "%s light %d: only %d texels drawn", nv ? "NV" : "EXT", light, drawn);
        CHECK_MSG(wrong == 0, "%s light %d: %d of 16384 texels differ", nv ? "NV" : "EXT", light, wrong);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

