/*
 * test_geometry_blocks.c
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Geometry shaders that carry their varyings in a user-defined interface
 * block. The stage becomes a compute shader, where an in/out block is not
 * legal at all, so each member has to travel flattened and the block has to
 * be rebuilt for the fragment stage to link against.
 */

#include <stdlib.h>
#include "mgl_test.h"
#include "harness.h"

#define W 64
#define H 64

static GLuint compileOne(GLenum stage, const char *src, char *log, int log_size)
{
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (!ok && log && log_size)
        glGetShaderInfoLog(sh, log_size, NULL, log);

    return ok ? sh : 0;
}

static GLuint linkVGF(const char *vs, const char *gs, const char *fs,
                      char *log, int log_size)
{
    GLuint prog = glCreateProgram();
    GLuint v, g, f;
    GLint ok = 0;

    if (log && log_size)
        log[0] = 0;

    v = compileOne(GL_VERTEX_SHADER, vs, log, log_size);
    g = compileOne(GL_GEOMETRY_SHADER, gs, log, log_size);
    f = compileOne(GL_FRAGMENT_SHADER, fs, log, log_size);

    if (!v || !g || !f)
        return 0;

    glAttachShader(prog, v);
    glAttachShader(prog, g);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);

    if (!ok && log && log_size)
        glGetProgramInfoLog(prog, log_size, NULL, log);

    return ok ? prog : 0;
}

static GLuint quadVAO(GLuint *vbo_out)
{
    static const GLfloat quad[12] = { -1,-1, 1,-1, -1,1,  1,-1, 1,1, -1,1 };
    GLuint vao = 0, vbo = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);
    glEnableVertexAttribArray(0);

    *vbo_out = vbo;

    return vao;
}

// draws the program over the whole target and returns the centre pixel
static int drawAndRead(GLuint prog, unsigned char out[4])
{
    MGLTestTarget t;
    GLuint vao, vbo;
    unsigned char *px;

    if (!mgl_target_create(&t, W, H, GL_RGBA8, 0))
        return 0;

    mgl_target_bind(&t);
    glUseProgram(prog);
    vao = quadVAO(&vbo);
    glViewport(0, 0, W, H);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    px = mgl_read_rgba8(&t);

    if (px)
    {
        mgl_pixel_at(px, &t, W / 2, H / 2, out);
        free(px);
    }

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    mgl_target_destroy(&t);

    return px != NULL;
}

/* ---------- a block carried all the way through ---------- */

GPU_TEST(geometry_blocks, a_block_carries_its_members_across_the_stage)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "out Goku { vec4 tint; float gain; } goku;\n"
        "void main() {\n"
        "  goku.tint = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "  goku.gain = 1.0;\n"
        "  gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "in Goku { vec4 tint; float gain; } goku[];\n"
        "out Vegeta { vec4 tint; } vegeta;\n"
        "void main() {\n"
        "  for (int i = 0; i < 3; i++) {\n"
        "    vegeta.tint = goku[i].tint * goku[i].gain;\n"
        "    gl_Position = gl_in[i].gl_Position;\n"
        "    EmitVertex();\n"
        "  }\n"
        "  EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "in Vegeta { vec4 tint; } vegeta;\n"
        "out vec4 o;\n"
        "void main() { o = vegeta.tint; }\n";
    GLuint prog;
    char log[4096];
    unsigned char c[4] = { 0 };

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "block geometry program did not link: %s", log);

    if (!prog) return;

    if (!drawAndRead(prog, c)) { glDeleteProgram(prog); SKIP("no target"); }

    CHECK_MSG(c[1] > 200 && c[0] < 60, "centre = %d,%d,%d - want green", c[0], c[1], c[2]);

    glDeleteProgram(prog);
}

/* ---------- a block member that is itself an array ---------- */

GPU_TEST(geometry_blocks, a_block_member_can_be_an_array)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "out Goku { float ch[3]; } goku;\n"
        "void main() {\n"
        "  goku.ch[0] = 0.0; goku.ch[1] = 1.0; goku.ch[2] = 0.0;\n"
        "  gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "in Goku { float ch[3]; } goku[];\n"
        "out Vegeta { float ch[3]; } vegeta;\n"
        "void main() {\n"
        "  for (int i = 0; i < 3; i++) {\n"
        "    vegeta.ch[0] = goku[i].ch[0];\n"
        "    vegeta.ch[1] = goku[i].ch[1];\n"
        "    vegeta.ch[2] = goku[i].ch[2];\n"
        "    gl_Position = gl_in[i].gl_Position;\n"
        "    EmitVertex();\n"
        "  }\n"
        "  EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "in Vegeta { float ch[3]; } vegeta;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(vegeta.ch[0], vegeta.ch[1], vegeta.ch[2], 1.0); }\n";
    GLuint prog;
    char log[4096];
    unsigned char c[4] = { 0 };

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "array-member block did not link: %s", log);

    if (!prog) return;

    if (!drawAndRead(prog, c)) { glDeleteProgram(prog); SKIP("no target"); }

    CHECK_MSG(c[1] > 200 && c[0] < 60, "centre = %d,%d,%d - want green", c[0], c[1], c[2]);

    glDeleteProgram(prog);
}

/* ---------- an explicit location has to survive the rewrite ---------- */

GPU_TEST(geometry_blocks, an_explicit_location_survives_the_rewrite)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "layout(location = 7) out vec4 tint;\n"
        "void main() { tint = vec4(0,1,0,1); gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "layout(location = 7) in vec4 tint[];\n"
        "layout(location = 3) out vec4 outTint;\n"
        "void main() {\n"
        "  for (int i = 0; i < 3; i++) {\n"
        "    outTint = tint[i];\n"
        "    gl_Position = gl_in[i].gl_Position;\n"
        "    EmitVertex();\n"
        "  }\n"
        "  EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "layout(location = 3) in vec4 outTint;\n"
        "out vec4 o;\n"
        "void main() { o = outTint; }\n";
    GLuint prog;
    char log[4096];
    unsigned char c[4] = { 0 };

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "explicit-location geometry program did not link: %s", log);

    if (!prog) return;

    if (!drawAndRead(prog, c)) { glDeleteProgram(prog); SKIP("no target"); }

    CHECK_MSG(c[1] > 200 && c[0] < 60, "centre = %d,%d,%d - want green", c[0], c[1], c[2]);

    glDeleteProgram(prog);
}

/* ---------- a location shared through component= ---------- */

// The pass-through rebuilds every output with its location. Two outputs on one
// location without their components overlap, and the generated shader will
// not compile at all.
GPU_TEST(geometry_blocks, a_location_shared_through_component_survives_the_rewrite)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "layout(location = 0, component = 0) out vec3 a;\n"
        "layout(location = 0, component = 3) out float b;\n"
        "void main() { a = vec3(0.0, 0.5, 0.0); b = 2.0; gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 460 core\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 3) out;\n"
        "layout(location = 0, component = 0) in vec3 a[];\n"
        "layout(location = 0, component = 3) in float b[];\n"
        "layout(location = 0, component = 0) flat out vec3 ga;\n"
        "layout(location = 0, component = 3) flat out float gb;\n"
        "void main() {\n"
        "  for (int i = 0; i < 3; i++) {\n"
        "    ga = a[i]; gb = b[i];\n"
        "    gl_Position = gl_in[i].gl_Position;\n"
        "    EmitVertex();\n"
        "  }\n"
        "  EndPrimitive();\n"
        "}\n";
    static const char *fs =
        "#version 460 core\n"
        "layout(location = 0, component = 0) flat in vec3 ga;\n"
        "layout(location = 0, component = 3) flat in float gb;\n"
        "out vec4 o;\n"
        "void main() { o = vec4(ga.x, ga.y * gb, ga.z, 1.0); }\n";
    GLuint prog;
    char log[4096];
    unsigned char c[4] = { 0 };

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "component-sharing geometry program did not link: %s", log);

    if (!prog) return;

    if (!drawAndRead(prog, c)) { glDeleteProgram(prog); SKIP("no target"); }

    CHECK_MSG(c[1] > 200 && c[0] < 60, "centre = %d,%d,%d - want green", c[0], c[1], c[2]);

    glDeleteProgram(prog);
}

/* ---------- GLSL 4.20's relaxed qualifiers ---------- */

// Qualifiers may come in any order and a declaration may carry several layout
// groups, the later value winning. The rewrite only read a layout group at the
// front of a declaration, so these stayed behind in the compute shader, where
// an "in" is not legal at all.
GPU_TEST(geometry_blocks, qualifiers_in_any_order_and_repeated_layouts)
{
    static const char *vs =
        "#version 420\n"
        "layout(location = 0) in vec2 p;\n"
        "layout(location = 2) out vec4 tint;\n"
        "void main() { tint = vec4(0, 1, 0, 1); gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *gs =
        "#version 420\n"
        "layout(triangles) in;\n"
        "layout(triangle_strip, max_vertices = 1) layout(max_vertices = 3) out;\n"
        "in layout(location = 5) layout(location = 2) vec4 tint[];\n"
        "out layout(location = 4) flat layout(location = 1) vec4 outTint;\n"
        "void main() {\n"
        "  for (int i = 0; i < 3; i++) {\n"
        "    outTint = tint[i];\n"
        "    gl_Position = gl_in[i].gl_Position;\n"
        "    EmitVertex();\n"
        "  }\n"
        "}\n";
    static const char *fs =
        "#version 420\n"
        "layout(location = 1) flat in vec4 outTint;\n"
        "out vec4 o;\n"
        "void main() { o = outTint; }\n";
    GLuint prog;
    char log[4096];
    unsigned char c[4] = { 0 };

    prog = linkVGF(vs, gs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "4.20 qualifier program did not link: %s", log);

    if (!prog) return;

    // three vertices only fit if the later max_vertices won
    if (!drawAndRead(prog, c)) { glDeleteProgram(prog); SKIP("no target"); }

    CHECK_MSG(c[1] > 200 && c[0] < 60, "centre = %d,%d,%d - want green", c[0], c[1], c[2]);

    glDeleteProgram(prog);
}
