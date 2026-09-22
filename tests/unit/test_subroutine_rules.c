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
