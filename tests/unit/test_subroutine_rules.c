/*
 * test_subroutine_rules.c
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
 * The subroutine rewrite turns subroutine uniforms into plain ints, which
 * would quietly make some bad shaders legal. These are the ones it must
 * still refuse, and one it must still accept.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "glm_context.h"

#define HEAD "#version 400\n" \
             "subroutine void st(inout vec4 v);\n" \
             "subroutine(st) void fa(inout vec4 v) { v += vec4(1.0); }\n"

static const char *rewriteError(const char *src)
{
    static SubroutineInfo info;

    mglFreeSubroutineInfo(&info);
    free(mglRewriteSubroutines(src, &info));

    return info.error;
}

TEST(subroutine_rules, a_plain_call_is_accepted)
{
    CHECK(rewriteError(HEAD "subroutine uniform st u;\n"
                            "void main() { vec4 p = vec4(0.0); u(p); }\n") == NULL);
}

TEST(subroutine_rules, assigning_a_number_is_refused)
{
    CHECK(rewriteError(HEAD "subroutine uniform st u;\n"
                            "void main() { u = 1; vec4 p; u(p); }\n") != NULL);
}

TEST(subroutine_rules, comparing_uniforms_is_refused)
{
    CHECK(rewriteError(HEAD "subroutine uniform st u;\nsubroutine uniform st w;\n"
                            "void main() { vec4 p; if (u == w) u(p); }\n") != NULL);
}

TEST(subroutine_rules, a_uniform_with_no_function_is_refused)
{
    CHECK(rewriteError("#version 400\n"
                       "subroutine void lonely(out vec4 v);\n"
                       "subroutine uniform lonely u;\n"
                       "void main() { vec4 p; u(p); }\n") != NULL);
}

TEST(subroutine_rules, a_mismatched_signature_is_refused)
{
    CHECK(rewriteError("#version 400\n"
                       "subroutine int other(inout vec3 a);\n"
                       "subroutine(other) void f(inout vec3 a) { a = vec3(1.0); }\n"
                       "void main() { }\n") != NULL);

    CHECK(rewriteError("#version 400\n"
                       "subroutine void two(inout vec3 a, out vec4 b);\n"
                       "subroutine(two) void f(inout vec3 a) { a = vec3(1.0); }\n"
                       "void main() { }\n") != NULL);
}

TEST(subroutine_rules, parameter_names_and_in_do_not_matter)
{
    CHECK(rewriteError("#version 400\n"
                       "subroutine float st(in vec3 a, float b[2]);\n"
                       "subroutine(st) float f(vec3 other, float x[2]) { return x[0]; }\n"
                       "subroutine uniform st u;\n"
                       "void main() { float q[2]; u(vec3(0.0), q); }\n") == NULL);
}

// The dispatcher forwards each parameter by name, and the name was taken as
// the last word before the comma -- which for "out float a[2][2]" is the 2.
TEST(subroutine_rules, an_array_parameter_is_forwarded_by_name_not_by_size)
{
    static const char *src =
        "#version 460\n"
        "subroutine float ft(in float a, inout vec2 b[3], out int c[2][4]);\n"
        "subroutine(ft) float one(in float a, inout vec2 b[3], out int c[2][4]) { c[1][3] = 1; return a; }\n"
        "subroutine uniform ft pick;\n"
        "void main() { vec2 bb[3]; int cc[2][4]; gl_Position = vec4(pick(1.0, bb, cc)); }\n";
    SubroutineInfo info;
    char *out;

    memset(&info, 0, sizeof info);
    out = mglRewriteSubroutines(src, &info);

    CHECK(out != NULL);
    CHECK_MSG(out && strstr(out, "one(a, b, c)") != NULL,
              "dispatcher does not forward a, b, c:\n%s", out ? out : "(null)");

    free(out);
}

// layout(index = N) picks a function's index. It was left in front of the
// plain function the rewrite made, which does not compile, and the dispatcher
// numbered the functions by position anyway.
TEST(subroutine_rules, an_explicit_index_is_the_one_used)
{
    static const char *src =
        "#version 430\n"
        "subroutine vec4 st(float p);\n"
        "layout(index = 5) subroutine(st) vec4 five(float p) { return vec4(5.0); }\n"
        "subroutine(st) vec4 zero(float p) { return vec4(0.0); }\n"
        "layout(index = 0x1) subroutine(st) vec4 one(float p) { return vec4(1.0); }\n"
        "subroutine uniform st pick;\n"
        "out vec4 o;\n"
        "void main() { o = pick(1.0); }\n";
    SubroutineInfo info;
    char *out;

    memset(&info, 0, sizeof info);
    out = mglRewriteSubroutines(src, &info);

    CHECK(out != NULL);
    CHECK_MSG(out && strstr(out, "layout") == NULL, "a layout was left behind:\n%s", out ? out : "(null)");
    CHECK_MSG(out && strstr(out, "case 5: return five") && strstr(out, "case 0: return zero") &&
              strstr(out, "case 1: return one"),
              "dispatcher cases do not follow the indices:\n%s", out ? out : "(null)");
    CHECK(info.fn_count == 3 && mglSubroutineSlot(&info, 5) == 0 && mglSubroutineSlot(&info, 3) == -1);

    free(out);
    mglFreeSubroutineInfo(&info);
}

TEST(subroutine_rules, two_functions_with_one_index_are_refused)
{
    CHECK(rewriteError(
        "#version 430\n"
        "subroutine vec4 st(float p);\n"
        "layout(index = 2) subroutine(st) vec4 a(float p) { return vec4(0.0); }\n"
        "layout(index = 2) subroutine(st) vec4 b(float p) { return vec4(1.0); }\n"
        "subroutine uniform st pick;\n"
        "out vec4 o;\n"
        "void main() { o = pick(1.0); }\n") != NULL);
}

#define ST "#version 430\n" \
           "subroutine vec4 st(float p);\n" \
           "subroutine(st) vec4 f(float p) { return vec4(p); }\n"

// GL 4.6 7.10's limits on subroutine uniform locations. These used to pass
// only because layout(index) never compiled at all.
TEST(subroutine_rules, a_location_used_twice_is_refused)
{
    CHECK(rewriteError(ST
        "layout(location = 2) subroutine uniform st a;\n"
        "layout(location = 2) subroutine uniform st b;\n"
        "out vec4 o;\nvoid main() { o = a(1.0) + b(1.0); }\n") != NULL);

    // an array covers every location it spans
    CHECK(rewriteError(ST
        "layout(location = 1) subroutine uniform st a[3];\n"
        "layout(location = 3) subroutine uniform st b;\n"
        "out vec4 o;\nvoid main() { o = a[0](1.0) + b(1.0); }\n") != NULL);
}

TEST(subroutine_rules, a_location_past_the_limit_is_refused)
{
    CHECK(rewriteError(ST
        "layout(location = 1024) subroutine uniform st a;\n"
        "out vec4 o;\nvoid main() { o = a(1.0); }\n") != NULL);

    CHECK(rewriteError(ST
        "layout(location = 1023) subroutine uniform st a;\n"
        "out vec4 o;\nvoid main() { o = a(1.0); }\n") == NULL);
}

TEST(subroutine_rules, too_many_locations_in_all_is_refused)
{
    CHECK(rewriteError(ST
        "layout(location = 0) subroutine uniform st a[1024];\n"
        "subroutine uniform st b;\n"
        "out vec4 o;\nvoid main() { o = a[0](1.0) + b(1.0); }\n") != NULL);
}

TEST(subroutine_rules, one_function_past_the_limit_is_refused)
{
    size_t cap = 300 * 96 + 1024;
    char *src = malloc(cap);
    size_t n = 0;

    n += (size_t)snprintf(src + n, cap - n, "#version 430\nsubroutine vec4 st(float p);\n");

    for (int i = 0; i < 256; i++)
        n += (size_t)snprintf(src + n, cap - n,
                              "layout(index = %d) subroutine(st) vec4 f%d(float p) { return vec4(p); }\n", i, i);

    n += (size_t)snprintf(src + n, cap - n,
                          "subroutine(st) vec4 extra(float p) { return vec4(p); }\n"
                          "subroutine uniform st a;\nout vec4 o;\nvoid main() { o = a(1.0); }\n");

    CHECK(rewriteError(src) != NULL);
    free(src);
}

// "(void)" is an empty list. The selector argument used to be put in front of
// the word, which produced "routine(int index, void)" and failed to compile.
TEST(subroutine_rules, a_void_parameter_list_is_empty)
{
    static SubroutineInfo info;
    char *out;

    mglFreeSubroutineInfo(&info);
    out = mglRewriteSubroutines("#version 400\n"
                                "subroutine int pick(void);\n"
                                "subroutine(pick) int one(void) { return 1; }\n"
                                "subroutine(pick) int two() { return 2; }\n"
                                "subroutine uniform pick routine;\n"
                                "void main() { int x = routine(); }\n", &info);

    CHECK(info.error == NULL);
    CHECK(out != NULL);

    if (out)
    {
        CHECK_MSG(strstr(out, ", void") == NULL, "selector put in front of void:\n%s", out);
        free(out);
    }

    mglFreeSubroutineInfo(&info);
}

static char *rewriteOk(const char *src)
{
    static SubroutineInfo info;
    char *out;

    mglFreeSubroutineInfo(&info);
    out = mglRewriteSubroutines(src, &info);
    CHECK_MSG(info.error == NULL, "rewrite refused: %s", info.error ? info.error : "");
    return out;
}

// A call inside another call's arguments, and inside the index, is a call too.
// Only the outermost one used to be rewritten, and the inner one reached
// glslang as a call to an int.
TEST(subroutine_rules, nested_calls_are_rewritten)
{
    char *out = rewriteOk("#version 400\n"
                          "subroutine int op(int a, int b);\n"
                          "subroutine(op) int add(int a, int b) { return a + b; }\n"
                          "subroutine uniform op routine[2];\n"
                          "void main() { int x = routine[routine[0](0, 1)](routine[1](2, 3), 4); }\n");

    CHECK(out != NULL);

    if (out)
    {
        // every call site becomes a call to the dispatcher with the selector first
        CHECK_MSG(strstr(out, "routine(routine" MGL_SUBROUTINE_SUFFIX "[routine(routine" MGL_SUBROUTINE_SUFFIX "[0], 0, 1)], "
                              "routine(routine" MGL_SUBROUTINE_SUFFIX "[1], 2, 3), 4)") != NULL,
                  "nested calls not rewritten:\n%s", out);
        free(out);
    }
}

// An array of arrays of subroutine uniforms keeps every dimension
TEST(subroutine_rules, arrays_of_arrays_keep_every_index)
{
    char *out = rewriteOk("#version 430\n"
                          "subroutine void st(inout vec4 v);\n"
                          "subroutine(st) void fa(inout vec4 v) { v += vec4(1.0); }\n"
                          "subroutine uniform st routine[2][3];\n"
                          "void main() { vec4 p; routine[1][2](p); }\n");

    CHECK(out != NULL);

    if (out)
    {
        CHECK_MSG(strstr(out, "uniform int routine" MGL_SUBROUTINE_SUFFIX "[2][3];") != NULL, "declaration:\n%s", out);
        CHECK_MSG(strstr(out, "routine(routine" MGL_SUBROUTINE_SUFFIX "[1][2], p)") != NULL, "call:\n%s", out);
        free(out);
    }
}

// A dispatcher's prototype can name a struct, so it has to come after the
// struct; it used to go straight after #version, ahead of everything.
TEST(subroutine_rules, a_struct_parameter_is_declared_before_its_prototype)
{
    char *out = rewriteOk("#version 400\n"
                          "struct S { vec4 a; };\n"
                          "subroutine vec4 st(S s);\n"
                          "subroutine(st) vec4 fa(S s) { return s.a; }\n"
                          "subroutine uniform st routine;\n"
                          "void main() { S s; s.a = vec4(1.0); vec4 v = routine(s); }\n");

    CHECK(out != NULL);

    if (out)
    {
        const char *st = strstr(out, "struct S");
        const char *proto = strstr(out, "vec4 routine(int");

        CHECK_MSG(st && proto && st < proto, "prototype ahead of the struct:\n%s", out);
        free(out);
    }
}

// A uniform's name inside a comment is not a use of the uniform
TEST(subroutine_rules, a_name_in_a_comment_is_not_a_use)
{
    char *out = rewriteOk("#version 400\n"
                          "subroutine float st(float x);\n"
                          "subroutine(st) float square(float x) { return x * x; }\n"
                          "// routine is the uniform below\n"
                          "/* routine again */\n"
                          "subroutine uniform st routine;\n"
                          "void main() { float a = square(2.0) + routine(3.0); }\n");

    free(out);
}

// A subroutine type may leave its parameters unnamed, and "in" is the default
TEST(subroutine_rules, unnamed_type_parameters_match_named_ones)
{
    char *out = rewriteOk("#version 400\n"
                          "subroutine vec4 sample_type(in vec2);\n"
                          "subroutine(sample_type) vec4 first(in vec2 coord) { return vec4(coord, 0, 1); }\n"
                          "subroutine(sample_type) vec4 second(vec2 uv) { return vec4(uv, 1, 1); }\n"
                          "subroutine uniform sample_type routine;\n"
                          "void main() { vec4 c = routine(vec2(0.5)); }\n");

    free(out);
}

// One function may implement several subroutine types, one uniform of each
TEST(subroutine_rules, one_function_for_three_types)
{
    static SubroutineInfo info;
    char *out;

    mglFreeSubroutineInfo(&info);
    out = mglRewriteSubroutines("#version 400\n"
                                "subroutine void t1(inout float v);\n"
                                "subroutine void t2(inout float v);\n"
                                "subroutine void t3(inout float v);\n"
                                "subroutine(t1, t2, t3) void f(inout float v) { v += 1.0; }\n"
                                "subroutine uniform t1 u1;\n"
                                "subroutine uniform t2 u2;\n"
                                "subroutine uniform t3 u3;\n"
                                "void main() { float v = 0.0; u1(v); u2(v); u3(v); }\n", &info);

    CHECK_MSG(info.error == NULL, "refused: %s", info.error ? info.error : "");
    CHECK(out != NULL);

    if (out)
    {
        CHECK_MSG(strstr(out, "void u3(int") != NULL && strstr(out, "case 0: f(v)") != NULL,
                  "third uniform has no function:\n%s", out);
        free(out);
    }

    mglFreeSubroutineInfo(&info);
}

// The CTS shape: unnamed parameters on the types, and one function for all three
TEST(subroutine_rules, one_function_three_unnamed_types)
{
    char *out = rewriteOk("#version 400\n"
                          "#extension GL_ARB_shader_subroutine : require\n"
                          "subroutine void subroutineType1(inout float);\n"
                          "subroutine void subroutineType2(inout float);\n"
                          "subroutine void subroutineType3(inout float);\n"
                          "subroutine(subroutineType1, subroutineType2, subroutineType3) void function(inout float result)\n"
                          "{ result += 0.123; }\n"
                          "subroutine uniform subroutineType1 subroutine_uniform1;\n"
                          "subroutine uniform subroutineType2 subroutine_uniform2;\n"
                          "subroutine uniform subroutineType3 subroutine_uniform3;\n"
                          "out vec4 result;\n"
                          "void main() { result = vec4(0); subroutine_uniform1(result.x);"
                          " subroutine_uniform2(result.y); subroutine_uniform3(result.z); }\n");

    free(out);
}

// GLSL reserves words like sizeof; the rewrite hides the type's name from
// glslang, so it has to refuse them itself
TEST(subroutine_rules, reserved_names_are_refused)
{
    CHECK(rewriteError("#version 400\n"
                       "subroutine void sizeof(inout vec4 v);\n"
                       "subroutine(sizeof) void fa(inout vec4 v) { v += vec4(1.0); }\n"
                       "subroutine uniform sizeof u;\n"
                       "void main() { vec4 p; u(p); }\n") != NULL);

    CHECK(rewriteError(HEAD "subroutine uniform st gl_mine;\n"
                            "void main() { vec4 p; gl_mine(p); }\n") != NULL);
}

// "float[2] a" puts the size on the type. The name was taken as the word in
// front of the first '[', which there is the type, so the two never matched.
TEST(subroutine_rules, a_size_on_the_type_matches)
{
    CHECK(rewriteError("#version 400\n"
                       "subroutine float st(float[2] a);\n"
                       "subroutine(st) float f(float[2] b) { return b[0]; }\n"
                       "subroutine uniform st u;\n"
                       "void main() { float q[2]; float r = u(q); }\n") == NULL);
}
