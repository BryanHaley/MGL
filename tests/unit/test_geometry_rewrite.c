/*
 * test_geometry_rewrite.c
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
 * The geometry stage is rewritten as a compute shader by reading its source.
 * Only file scope declares the stage's inputs and outputs; the same words in
 * a function's parameters are just parameters.
 */

#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "glm_context.h"

TEST(geometry_rewrite, an_out_parameter_is_not_a_stage_output)
{
    static const char *GS =
        "#version 460\n"
        "layout(points) in;\n"
        "layout(points, max_vertices = 1) out;\n"
        "out float result;\n"
        "subroutine void fill_t(out int output_array[2][2]);\n"
        "subroutine(fill_t) void fill(out int output_array[2][2]) {\n"
        "    output_array[0][0] = 1;\n"
        "}\n"
        "subroutine uniform fill_t routine;\n"
        "void main() {\n"
        "    int a[2][2];\n"
        "    routine(a);\n"
        "    result = float(a[0][0]);\n"
        "    gl_Position = vec4(0.0);\n"
        "    EmitVertex();\n"
        "}\n";
    GeometryInfo gi;

    CHECK(mglRewriteGeometryShader(GS, &gi));

    if (gi.compute_src)
    {
        // the stage's one output is renamed, the parameter is left alone
        CHECK(strstr(gi.compute_src, "mglCur.result") != NULL);
        CHECK_MSG(strstr(gi.compute_src, "mglCur.output_array") == NULL,
                  "a parameter was taken for an output:\n%s", gi.compute_src);
        CHECK(strstr(gi.compute_src, "out int output_array[2][2]") != NULL);
    }

    CHECK_EQ_INT(1, gi.out_count);

    mglFreeGeometryInfo(&gi);
}
