/*
 * test_uniform_block_names.c
 * Copyright (C) The MooGL Project
 *
 * GL 4.6 section 7.3.1: a member of a uniform block declared with an instance
 * name is called "Block.member" through the API. MGL reported the bare member
 * name, so glGetUniformIndices could not find any of them.
 */

#include <string.h>

#include "mgl_test.h"
#include "harness.h"

static const char *FS_SRC =
    "#version 460 core\n"
    "out vec4 o;void main(){o=vec4(1);}\n";

static GLuint build(const char *vs_src)
{
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint p = glCreateProgram();
    GLint ok = 0;

    glShaderSource(v, 1, &vs_src, NULL);
    glCompileShader(v);
    glGetShaderiv(v, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        char log[1024] = {0};
        glGetShaderInfoLog(v, sizeof log, NULL, log);
        CHECK_MSG(0, "vertex shader would not compile: %s", log);
        return 0;
    }

    glShaderSource(f, 1, &FS_SRC, NULL);
    glCompileShader(f);
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(GL_TRUE, ok);

    glDeleteShader(v);
    glDeleteShader(f);

    return ok ? p : 0;
}

GPU_TEST(uniform_block_names, instance_name_prefixes_its_members)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std140) uniform Block { mat3 m; float t[3]; } b;\n"
        "void main(){ gl_Position = vec4(b.m[0][0] + b.t[2]); }\n";

    GLuint p = build(VS);
    const char *names[2] = {"Block.m", "Block.t"};
    GLuint idx[2] = {GL_INVALID_INDEX, GL_INVALID_INDEX};

    if (!p) return;

    mgl_drain_errors();

    glGetUniformIndices(p, 2, names, idx);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_MSG(idx[0] != GL_INVALID_INDEX, "Block.m was not found");
    CHECK_MSG(idx[1] != GL_INVALID_INDEX, "Block.t was not found");

    if (idx[0] != GL_INVALID_INDEX && idx[1] != GL_INVALID_INDEX)
    {
        GLint off[2] = {-1, -1}, astride[2] = {-1, -1}, mstride[2] = {-1, -1};

        glGetActiveUniformsiv(p, 2, idx, GL_UNIFORM_OFFSET, off);
        glGetActiveUniformsiv(p, 2, idx, GL_UNIFORM_ARRAY_STRIDE, astride);
        glGetActiveUniformsiv(p, 2, idx, GL_UNIFORM_MATRIX_STRIDE, mstride);

        /* std140: mat3 at 0 with a 16-byte column stride, float[3] at 48
           with a 16-byte array stride */
        CHECK_EQ_INT(0,  off[0]);
        CHECK_EQ_INT(16, mstride[0]);
        CHECK_EQ_INT(48, off[1]);
        CHECK_EQ_INT(16, astride[1]);
    }

    glDeleteProgram(p);
}

GPU_TEST(uniform_block_names, a_block_without_an_instance_name_keeps_bare_members)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std140) uniform Block { float a; vec3 b; };\n"
        "void main(){ gl_Position = vec4(a + b.x); }\n";

    GLuint p = build(VS);
    const char *bare[2] = {"a", "b"};
    const char *prefixed[1] = {"Block.a"};
    GLuint idx[2] = {GL_INVALID_INDEX, GL_INVALID_INDEX};
    GLuint none[1] = {0};

    if (!p) return;

    mgl_drain_errors();

    glGetUniformIndices(p, 2, bare, idx);
    CHECK_MSG(idx[0] != GL_INVALID_INDEX, "a was not found");
    CHECK_MSG(idx[1] != GL_INVALID_INDEX, "b was not found");

    glGetUniformIndices(p, 1, prefixed, none);
    CHECK_MSG(none[0] == GL_INVALID_INDEX, "Block.a must not resolve without an instance name");

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDeleteProgram(p);
}

/* The block itself is always named by its block name, never the instance. */
GPU_TEST(uniform_block_names, blocks_are_indexed_in_declaration_order)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std140) uniform BlockA { float a; } ba;\n"
        "layout(std140) uniform BlockB { float c; } bb;\n"
        "void main(){ gl_Position = vec4(ba.a + bb.c); }\n";

    GLuint p = build(VS);
    GLint n = 0;

    if (!p) return;

    mgl_drain_errors();

    glGetProgramiv(p, GL_ACTIVE_UNIFORM_BLOCKS, &n);
    CHECK_EQ_INT(2, n);

    CHECK_MSG(glGetUniformBlockIndex(p, "BlockA") != GL_INVALID_INDEX, "BlockA not found");
    CHECK_MSG(glGetUniformBlockIndex(p, "BlockB") != GL_INVALID_INDEX, "BlockB not found");
    CHECK_MSG(glGetUniformBlockIndex(p, "ba") == GL_INVALID_INDEX,
              "the instance name must not name the block");
    CHECK_MSG(glGetUniformBlockIndex(p, "Nope") == GL_INVALID_INDEX,
              "an unknown name must be GL_INVALID_INDEX");

    for (GLint i = 0; i < n; i++)
    {
        char nm[64] = {0};

        glGetActiveUniformBlockName(p, (GLuint)i, sizeof nm, NULL, nm);
        CHECK_EQ_UINT((GLuint)i, glGetUniformBlockIndex(p, nm));
    }

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDeleteProgram(p);
}

/* A block declared in two stages is one block to GL, not two. Counting it
   twice made every later block's index disagree with its own name. */
GPU_TEST(uniform_block_names, a_block_in_two_stages_is_one_block)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std140) uniform Shared { vec4 a; } s;\n"
        "layout(std140) uniform OnlyV  { vec4 b; } v;\n"
        "void main(){ gl_Position = s.a + v.b; }\n";
    static const char *FS =
        "#version 460 core\n"
        "layout(std140) uniform Shared { vec4 a; } s;\n"
        "out vec4 o;void main(){ o = s.a; }\n";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint p = glCreateProgram();
    GLint ok = 0, n = 0;

    glShaderSource(vs, 1, &VS, NULL); glCompileShader(vs);
    glShaderSource(fs, 1, &FS, NULL); glCompileShader(fs);
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(GL_TRUE, ok);

    mgl_drain_errors();

    glGetProgramiv(p, GL_ACTIVE_UNIFORM_BLOCKS, &n);
    CHECK_EQ_INT(2, n);

    /* GL 4.6 section 7.6.2: indices run 0..ACTIVE_UNIFORM_BLOCKS-1 and the
       name at index i must map back to i. */
    for (GLint i = 0; i < n; i++)
    {
        char nm[64] = {0};

        glGetActiveUniformBlockName(p, (GLuint)i, sizeof nm, NULL, nm);
        CHECK_MSG(nm[0] != '\0', "block %d has no name", i);
        CHECK_MSG(glGetUniformBlockIndex(p, nm) == (GLuint)i,
                  "block %d is named '%s' but that name maps to %u",
                  i, nm, glGetUniformBlockIndex(p, nm));
    }

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteProgram(p);
}

/* An instance array is one block per element, named Block[0]..Block[n-1]. */
GPU_TEST(uniform_block_names, an_instance_array_is_one_block_per_element)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std140) uniform Block { vec4 a; } b[3];\n"
        "void main(){ gl_Position = b[0].a + b[2].a; }\n";

    GLuint p = build(VS);
    GLint n = 0;

    if (!p) return;

    mgl_drain_errors();

    glGetProgramiv(p, GL_ACTIVE_UNIFORM_BLOCKS, &n);
    CHECK_EQ_INT(3, n);

    for (GLint i = 0; i < n && i < 3; i++)
    {
        char want[32], got[64] = {0};

        snprintf(want, sizeof want, "Block[%d]", i);
        glGetActiveUniformBlockName(p, (GLuint)i, sizeof got, NULL, got);

        CHECK_STR_EQ(want, got);
        CHECK_EQ_UINT((GLuint)i, glGetUniformBlockIndex(p, want));
    }

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDeleteProgram(p);
}

/* An array uniform is named with "[0]" on the end, and either spelling
   finds it. */
GPU_TEST(uniform_block_names, array_members_carry_a_zero_subscript)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std140) uniform Block { float t[3]; } b;\n"
        "void main(){ gl_Position = vec4(b.t[2]); }\n";

    GLuint p = build(VS);
    const char *with[1] = {"Block.t[0]"};
    const char *without[1] = {"Block.t"};
    GLuint a = GL_INVALID_INDEX, c = GL_INVALID_INDEX;

    if (!p) return;

    mgl_drain_errors();

    glGetUniformIndices(p, 1, with, &a);
    glGetUniformIndices(p, 1, without, &c);

    CHECK_MSG(a != GL_INVALID_INDEX, "Block.t[0] was not found");
    CHECK_MSG(c != GL_INVALID_INDEX, "Block.t was not found");
    CHECK_EQ_UINT(a, c);

    if (a != GL_INVALID_INDEX)
    {
        char nm[64] = {0};
        GLint sz = 0;
        GLenum ty = 0;

        glGetActiveUniform(p, a, sizeof nm, NULL, &sz, &ty, nm);
        CHECK_STR_EQ("Block.t[0]", nm);
        CHECK_EQ_INT(3, sz);
    }

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDeleteProgram(p);
}

/* glslang lowers a bool in a uniform block to uint, because SPIR-V gives bools
   no memory layout. GL still has to call it a bool. */
GPU_TEST(uniform_block_names, bool_members_keep_their_type)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std140) uniform Block { bool d; bvec3 v; uint u; float f; } b;\n"
        "void main(){ gl_Position = vec4(b.d ? 1.0 : 0.0, b.v.x ? 1.0 : 0.0,"
        " float(b.u), b.f); }\n";

    GLuint p = build(VS);
    struct { const char *name; GLenum type; } want[] = {
        {"Block.d", GL_BOOL},
        {"Block.v", GL_BOOL_VEC3},
        {"Block.u", GL_UNSIGNED_INT},
        {"Block.f", GL_FLOAT},
    };
    GLint n = 0;

    if (!p) return;

    mgl_drain_errors();

    glGetProgramiv(p, GL_ACTIVE_UNIFORMS, &n);
    CHECK_EQ_INT(4, n);

    for (unsigned w = 0; w < sizeof want / sizeof *want; w++)
    {
        GLuint idx = GL_INVALID_INDEX;
        GLint ty = 0;

        glGetUniformIndices(p, 1, &want[w].name, &idx);
        CHECK_MSG(idx != GL_INVALID_INDEX, "%s was not found", want[w].name);

        if (idx == GL_INVALID_INDEX)
            continue;

        glGetActiveUniformsiv(p, 1, &idx, GL_UNIFORM_TYPE, &ty);
        CHECK_MSG((GLenum)ty == want[w].type, "%s came back as 0x%x, expected 0x%x",
                  want[w].name, ty, want[w].type);
    }

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDeleteProgram(p);
}

/* A block declared as an instance array is several GL blocks: each element
   has its own index, name, binding and buffer. MGL had one of each. */
GPU_TEST(uniform_block_names, instance_array_elements_are_separate_blocks)
{
    static const char *VS =
        "#version 460 core\n"
        "void main() {\n"
        "    vec2 p[4] = vec2[4](vec2(-1,-1), vec2(3,-1), vec2(-1,3), vec2(3,3));\n"
        "    gl_Position = vec4(p[gl_VertexID & 3], 0.0, 1.0);\n"
        "}\n";
    static const char *FS =
        "#version 460 core\n"
        "layout(std140) uniform Block { ivec4 var; } block[3];\n"
        "layout(location = 0) out ivec4 frag;\n"
        "void main() { frag = ivec4(block[0].var.x, block[1].var.x, block[2].var.x, 7); }\n";

    MGLTestTarget t;
    GLuint prog, vao, vbo, ubo[3];
    GLint got[4 * 4 * 4];
    char log[512] = { 0 };

    if (!mgl_target_create(&t, 4, 4, GL_RGBA32I, 0))
        return;

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    mgl_target_bind(&t);
    glUseProgram(prog);
    glBindVertexArray(vao);

    glGenBuffers(3, ubo);

    for (int i = 0; i < 3; i++)
    {
        char name[32];
        GLuint idx;
        GLint data[4] = { 100 + i, 0, 0, 0 };

        snprintf(name, sizeof name, "Block[%d]", i);
        idx = glGetUniformBlockIndex(prog, name);
        CHECK_MSG(idx != GL_INVALID_INDEX, "%s has no index", name);

        glUniformBlockBinding(prog, idx, (GLuint)i);

        {
            GLint reported = -1;

            glGetActiveUniformBlockiv(prog, idx, GL_UNIFORM_BLOCK_BINDING, &reported);
            CHECK_EQ_INT(i, reported);
        }

        glBindBuffer(GL_UNIFORM_BUFFER, ubo[i]);
        glBufferData(GL_UNIFORM_BUFFER, sizeof data, data, GL_STATIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, (GLuint)i, ubo[i]);
    }

    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glFinish();

    glBindTexture(GL_TEXTURE_2D, t.color);
    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA_INTEGER, GL_INT, got);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    CHECK_EQ_INT(100, got[0]);
    CHECK_EQ_INT(101, got[1]);
    CHECK_EQ_INT(102, got[2]);

    glDeleteBuffers(3, ubo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* An array of one is still an array, and GL names that block "Block[0]". */
GPU_TEST(uniform_block_names, a_one_element_array_is_still_named_with_its_index)
{
    static const char *VS =
        "#version 460 core\n"
        "layout(std140) uniform Only { vec4 v; } only[1];\n"
        "void main() { gl_Position = only[0].v; }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 frag;\n"
        "void main() { frag = vec4(1.0); }\n";

    GLuint prog;
    char log[512] = { 0 };

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_drain_errors();
    CHECK(glGetUniformBlockIndex(prog, "Only[0]") != GL_INVALID_INDEX);

    {
        char name[64] = { 0 };
        GLsizei len = 0;

        glGetActiveUniformBlockName(prog, glGetUniformBlockIndex(prog, "Only[0]"),
                                    (GLsizei)sizeof name, &len, name);
        CHECK_MSG(!strcmp(name, "Only[0]"), "reported \"%s\"", name);
    }

    glDeleteProgram(prog);
}

/* A bool declared inside a struct the block uses is still a bool to GL, even
   though SPIR-V carries it as a uint. */
GPU_TEST(uniform_block_names, a_bool_in_a_nested_struct_reports_as_bool)
{
    static const char *VS =
        "#version 460 core\n"
        "struct S { bool flag; vec2 xy; };\n"
        "layout(std140) uniform Outer { S s; float tail; };\n"
        "void main() { gl_Position = vec4(s.xy, tail, s.flag ? 1.0 : 0.0); }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 frag;\n"
        "void main() { frag = vec4(1.0); }\n";

    GLuint prog;
    char log[512] = { 0 };
    const char *want = "s.flag";
    GLuint index = GL_INVALID_INDEX;

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_drain_errors();
    glGetUniformIndices(prog, 1, &want, &index);
    CHECK_MSG(index != GL_INVALID_INDEX, "s.flag is not an active uniform");

    if (index != GL_INVALID_INDEX)
    {
        GLint type = 0;

        glGetActiveUniformsiv(prog, 1, &index, GL_UNIFORM_TYPE, &type);
        CHECK_EQ_UINT((unsigned)GL_BOOL, (unsigned)type);
    }

    glDeleteProgram(prog);
}

/* An array of one is still an array: GL names its members "g[0]" and
   "l[0].mA", and reports the element count as the uniform's size. */
GPU_TEST(uniform_block_names, one_element_arrays_keep_their_index)
{
    static const char *VS =
        "#version 460 core\n"
        "struct S { mat2 mA; vec3 mB[1]; };\n"
        "layout(std140) uniform Blk { float g[1]; S l[1]; };\n"
        "void main() { gl_Position = vec4(g[0] + l[0].mA[0][0], l[0].mB[0]); }\n";
    static const char *FS =
        "#version 460 core\n"
        "out vec4 frag;\n"
        "void main() { frag = vec4(1.0); }\n";

    static const char *want[3] = { "g[0]", "l[0].mA", "l[0].mB[0]" };
    GLuint prog, idx[3] = { GL_INVALID_INDEX, GL_INVALID_INDEX, GL_INVALID_INDEX };
    char log[512] = { 0 };

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_drain_errors();
    glGetUniformIndices(prog, 3, want, idx);

    for (int i = 0; i < 3; i++)
        CHECK_MSG(idx[i] != GL_INVALID_INDEX, "%s is not an active uniform", want[i]);

    if (idx[0] != GL_INVALID_INDEX)
    {
        GLint size = 0;

        glGetActiveUniformsiv(prog, 1, &idx[0], GL_UNIFORM_SIZE, &size);
        CHECK_EQ_INT(1, size);
    }

    glDeleteProgram(prog);
}

/* ---------- a block's member indices name its members ---------- */

// Every program carries two hidden driver uniforms for the alpha test. The
// block query counted them when it numbered its members, so each index it
// handed back was two past the member it meant -- and past the end of the
// program's own uniform list.
GPU_TEST(uniform_block_names, a_member_index_names_that_member)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(std140) uniform Block { vec4 tint; float gain; };\n"
        "uniform vec4 plain;\n"
        "void main() { gl_Position = tint * gain + plain; }\n";
    GLuint prog = build(vs);
    GLint count = 0, members = 0, active = 0;
    GLint idx[2] = { -1, -1 };
    char name[64];

    if (!prog) SKIP("program did not build");

    GLuint block = glGetUniformBlockIndex(prog, "Block");
    glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &active);
    glGetActiveUniformBlockiv(prog, block, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &members);
    CHECK_EQ_INT(members, 2);

    glGetActiveUniformBlockiv(prog, block, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, idx);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 2; i++)
    {
        CHECK_MSG(idx[i] >= 0 && idx[i] < active,
                  "member %d has index %d, and the program has %d uniforms", i, idx[i], active);

        name[0] = 0;
        glGetActiveUniformName(prog, (GLuint)idx[i], sizeof name, NULL, name);
        CHECK_MSG(!strcmp(name, "tint") || !strcmp(name, "gain"),
                  "index %d names '%s', not a member of Block", idx[i], name);
        count++;
    }

    CHECK_EQ_INT(count, 2);
    glDeleteProgram(prog);
}
