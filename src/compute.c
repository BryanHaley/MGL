/*
 * Copyright (C) Michael Larson on 1/6/2022
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
 * compute.c
 * MGL
 *
 */

#include "glm_context.h"
#include "mgl_log.h"


void mglDispatchCompute(GLMContext ctx, GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z)
{
    // the limit on a dispatch is the workgroup COUNT, not the workgroup size --
    // those are different numbers and this used to check the wrong one
    ERROR_CHECK_RETURN(num_groups_x < ctx->state.var.max_compute_work_group_count[0], GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(num_groups_y < ctx->state.var.max_compute_work_group_count[1], GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(num_groups_z < ctx->state.var.max_compute_work_group_count[2], GL_INVALID_VALUE);

    // no program, or one with no compute stage, is an error -- not an abort
    if (STATE(program) == NULL)
    {
        MGL_ERR("MGL Error: glDispatchCompute with no program in use\n");
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    // Ask what the program LINKED, not what is still attached. Detaching after
    // a successful link is ordinary GL, and it clears shader_slots.
    if (STATE(program)->spirv[_COMPUTE_SHADER].msl_str == NULL)
    {
        MGL_ERR("MGL Error: glDispatchCompute: program %u has no linked compute stage\n",
                STATE(program)->name);
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ctx->mtl_funcs.mtlDispatchCompute(ctx, num_groups_x, num_groups_y, num_groups_z);
}

void mglDispatchComputeIndirect(GLMContext ctx, GLintptr indirect)
{
    Buffer *buf = STATE(buffers[_DISPATCH_INDIRECT_BUFFER]);

    ERROR_CHECK_RETURN(buf, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(indirect >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN((indirect & 3) == 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(indirect + 12 <= buf->size, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(STATE(program), GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlDispatchComputeIndirect(ctx, indirect);
}

