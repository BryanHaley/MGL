/*
 * test_stencil_ops.c
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
 * glStencilOp's three operations each reach Metal as themselves. They used
 * to all follow the stencil-fail one, and four of the eight were swapped.
 */

#include <stdlib.h>
#include <string.h>
#include "mgl_test.h"
#include "harness.h"

static const char *VS =
    "#version 460 core\n"
    "void main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));"
    "gl_Position=vec4(p[gl_VertexID],0,1);}\n";

static const char *RED =
    "#version 460 core\n"
    "out vec4 o;void main(){o=vec4(1,0,0,1);}\n";

// one column of the target, with its own stencil test and operations
static void draw_column(int x, GLenum stencil_func, GLenum depth_func,
                        GLenum fail, GLenum depth_fail, GLenum pass)
{
    glScissor(x, 0, 1, 4);
    glStencilFunc(stencil_func, 0, 0xFF);
    glDepthFunc(depth_func);
    glStencilOp(fail, depth_fail, pass);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

GPU_TEST(stencil_ops, each_operation_does_its_own_thing)
{
    char err[1024] = { 0 };
    MGLTestTarget t;
    GLuint prog, vao;
    GLubyte stencil[4 * 4];

    prog = mgl_build_program(VS, RED, err, sizeof err);
    CHECK_MSG(prog != 0, "link: %s", err);
    if (!prog) return;

    mgl_target_create(&t, 4, 4, GL_RGBA8, 1);
    mgl_target_bind(&t);
    glViewport(0, 0, 4, 4);

    glClearStencil(255);
    glClearDepth(1.0);
    glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_STENCIL_TEST);
    glEnable(GL_DEPTH_TEST);

    // both tests pass: 255 wraps up to 0
    draw_column(0, GL_ALWAYS, GL_ALWAYS, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
    // stencil passes, depth fails: 255 counts down to 254
    draw_column(1, GL_ALWAYS, GL_NEVER, GL_KEEP, GL_DECR, GL_KEEP);
    // stencil fails: 255 inverts to 0
    draw_column(2, GL_NEVER, GL_ALWAYS, GL_INVERT, GL_KEEP, GL_KEEP);
    // both pass: 255 wraps down to 254
    draw_column(3, GL_ALWAYS, GL_ALWAYS, GL_KEEP, GL_KEEP, GL_DECR_WRAP);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    memset(stencil, 0x77, sizeof stencil);
    glReadPixels(0, 0, 4, 4, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencil);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    CHECK_MSG(stencil[0] == 0, "pass INCR_WRAP: %d", stencil[0]);
    CHECK_MSG(stencil[1] == 254, "depth-fail DECR: %d", stencil[1]);
    CHECK_MSG(stencil[2] == 0, "fail INVERT: %d", stencil[2]);
    CHECK_MSG(stencil[3] == 254, "pass DECR_WRAP: %d", stencil[3]);

    glUseProgram(0);
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);
}
