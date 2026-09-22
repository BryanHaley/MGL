/*
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
 * mgl_reflect.h
 * MGL
 *
 * A program's resources the way GL names and counts them, for the program
 * interface queries. glslang works these out from the shaders themselves.
 */

#ifndef mgl_reflect_h
#define mgl_reflect_h

#include <stdbool.h>
#include <stddef.h>

#include "glcorearb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MGL_RES_UNIFORM,
    MGL_RES_UNIFORM_BLOCK,
    MGL_RES_PROGRAM_INPUT,
    MGL_RES_PROGRAM_OUTPUT,
    MGL_RES_BUFFER_VARIABLE,
    MGL_RES_STORAGE_BLOCK,
    MGL_RES_ATOMIC_BUFFER,
    // what the last stage before the rasteriser writes; transform feedback
    // looks its varyings up here
    MGL_RES_CAPTURE_SOURCE,
    // the feedback buffers a shader's own xfb qualifiers fill
    MGL_RES_XFB_BUFFER,
    MGL_RES_KINDS
} MglResourceKind;

typedef struct MglResource_t {
    char   *name;
    GLenum  type;
    GLint   array_size;         // 0 for an array with no size
    GLint   offset;             // -1 outside a block
    GLint   block_index;        // -1 outside a block
    GLint   array_stride;       // -1 outside a block
    GLint   matrix_stride;      // -1 outside a block
    GLint   row_major;
    GLint   top_level_size;
    GLint   top_level_stride;
    GLint   binding;            // -1 when the shader gives none
    GLint   data_size;          // blocks and atomic counter buffers
    GLint   location;           // layout(location), -1 when none
    GLint   location_index;     // layout(index), 0 when none
    GLint   component;          // layout(component), 0 when none
    GLint   atomic_buffer;      // an atomic counter's buffer, -1 otherwise
    GLint   per_patch;
    GLint   xfb_buffer;         // outputs: xfb_buffer, -1 when none
    GLint   xfb_offset;         // outputs: xfb_offset, -1 when none
    GLuint  stages;             // bit n set when MGL stage n uses it
    GLint   num_active;         // blocks and buffers: how many variables
    GLint  *active;             // and their indices
} MglResource;

typedef struct MglResourceTable_t {
    MglResource *list[MGL_RES_KINDS];
    GLint        count[MGL_RES_KINDS];
} MglResourceTable;

// shaders holds one compiled glslang shader per MGL stage slot, NULL where
// there is none, and sources the text each was compiled from. Returns false
// if nothing could be reflected.
bool mglReflectProgram(void *const *shaders, const char *const *sources, int stage_count,
                       MglResourceTable *out);
void mglFreeResourceTable(MglResourceTable *table);

// One captured value, as the shader's xfb_buffer and xfb_offset lay it out.
typedef struct MglXfbItem_t {
    char  expr[160];            // how the capturing stage names it
    GLint buffer;
    GLint offset;               // bytes into one recorded vertex
    char  kind;                 // 'f' float, 'd' double, 'i' int, 'u' uint, 'b' bool
    GLint components;           // scalars in all, a matrix counting every one
    GLint rows;                 // a matrix's rows, 0 otherwise
} MglXfbItem;

// The capture a shader asks for with its own xfb qualifiers. Returns how many
// items it filled, 0 when the shader declares none, -1 when there are too
// many. stride_bytes gets each buffer's recorded size, 0 for an unused one.
int mglXfbLayout(void *shader, MglXfbItem *items, int max_items, GLint *stride_bytes, int max_buffers);

// False, with a message, when an input or output's layout(location) runs past
// the stage's limit. The limits are in locations; 0 skips that direction.
bool mglVaryingLocationsFit(void *shader, int max_in, int max_out, char *msg, size_t msg_size);

#ifdef __cplusplus
}
#endif

#endif
