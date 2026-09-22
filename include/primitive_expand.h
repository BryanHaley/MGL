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
 * primitive_expand.h
 * MGL
 *
 * Expand GL primitive modes that have no direct Metal equivalent into
 * indexed draws.  The caller creates one PrimitiveExpander per context
 * (or per draw loop) and reuses it; the expander owns the temporary
 * index buffer so repeated identical draws do not reallocate.
 *
 * The `mtlType` and `indexType` fields use the same integer values as
 * MTLPrimitiveType and MTLIndexType.  In an ObjC call site you can
 * assign them directly to those enum types.
 *
 * Primitive restart is honoured on the element path (fixed index or
 * glPrimitiveRestartIndex); it never applies to the array path, as in GL.
 */

#ifndef primitive_expand_h
#define primitive_expand_h

#include "glm_context.h"

/* Metal primitive types — same values as MTLPrimitiveType */
enum {
    PEX_MTLPoint        = 0,
    PEX_MTLLine         = 1,
    PEX_MTLLineStrip    = 2,
    PEX_MTLTriangle     = 3,
    PEX_MTLTriangleStrip = 4
};

/* Metal index types — same values as MTLIndexType */
enum {
    PEX_MTLUInt16 = 0,
    PEX_MTLUInt32 = 1
};

/* Opaque; allocate with primitive_expander_new. */
typedef struct PrimitiveExpander PrimitiveExpander;

PrimitiveExpander *primitive_expander_new(void);
void               primitive_expander_free(PrimitiveExpander *exp);

/*
 * Result of one expansion.  The `indices` pointer is owned by the
 * expander and remains valid until the next expand call on the same
 * expander, or until the expander is freed.  Do not free it yourself.
 *
 * When `mtlType` is 0 and `indices` is NULL, the mode was GL_PATCHES
 * with no tessellation shader; the caller must raise GL_INVALID_OPERATION
 * and must not issue a draw.
 */
typedef struct {
    unsigned    mtlType;      /* PEX_MTL* value                       */
    const void *indices;      /* owned by the expander                */
    GLsizei     indexCount;   /* number of indices                    */
    unsigned    indexType;    /* PEX_MTLUInt16 or PEX_MTLUInt32       */
    GLsizei     indexSize;    /* sizeof one index element (2 or 4)    */
} PrimitiveExpansion;

/*
 * Expand a draw-arrays call (no index buffer bound).
 * `count` is the GL vertex count.
 * The expander synthesises sequential indices 0 .. count-1.
 */
PrimitiveExpansion primitive_expand_arrays(PrimitiveExpander *exp,
                                           GLMContext ctx,
                                           GLenum mode,
                                           GLsizei count);

/*
 * Expand a draw-elements call.  Reads the element buffer currently bound
 * to the VAO starting at byte offset `offset`, interprets `count`
 * indices of GL type `type`, and produces the remapped index list.
 *
 * `type` may be GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT.
 */
PrimitiveExpansion primitive_expand_elements(PrimitiveExpander *exp,
                                             GLMContext ctx,
                                             GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             size_t offset);

/*
 * Convenience: true when `mode` requires expansion (i.e. is not directly
 * representable as a Metal primitive type).  The caller can use this to
 * decide whether to call the expander or to use getMTLPrimitiveType.
 */
bool primitive_mode_needs_expand(GLenum mode);

#endif /* primitive_expand_h */
