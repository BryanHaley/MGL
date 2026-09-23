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
