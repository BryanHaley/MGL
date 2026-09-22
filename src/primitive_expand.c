/*
 * Copyright (C) The Moogle Project
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
 * primitive_expand.c
 * MGL
 *
 * Expand GL primitive modes that have no direct Metal equivalent into
 * indexed draws.  The expander owns a reusable malloc buffer; every call
 * reuses or grows it, so repeated identical draws do not allocate.
 *
 * Primitive restart is honoured on the element path: the index stream is
 * cut into runs at the restart index and each run is expanded on its own.
 * Modes that expand to a strip get Metal's own restart sentinel between
 * runs, which Metal always honours.  Restart never applies to DrawArrays.
 */

#include "primitive_expand.h"
#include "glm_context.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  helper: max of two size_t values                                  */
/* ------------------------------------------------------------------ */
static size_t max_sz(size_t a, size_t b) { return a > b ? a : b; }

/* ------------------------------------------------------------------ */
/*  helper: read one index from a source element buffer               */
/* ------------------------------------------------------------------ */
static GLuint read_index(const uint8_t *base, GLenum type, GLsizei i)
{
    switch (type) {
    case GL_UNSIGNED_BYTE:
        return ((const uint8_t *)base)[i];
    case GL_UNSIGNED_SHORT:
        return ((const uint16_t *)base)[i];
    case GL_UNSIGNED_INT:
        return ((const uint32_t *)base)[i];
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/*  helper: write one index to the output buffer                      */
/* ------------------------------------------------------------------ */
static void write_index(void *buf, unsigned itype, GLsizei i, GLuint val)
{
    if (itype == PEX_MTLUInt16) {
        ((uint16_t *)buf)[i] = (uint16_t)val;
    } else {
        ((uint32_t *)buf)[i] = val;
    }
}

/* ------------------------------------------------------------------ */
/*  helper: choose the narrowest index type that fits `maxVal`        */
/*  0xFFFF itself is left free so it can serve as Metal's restart.    */
/* ------------------------------------------------------------------ */
static unsigned choose_type(GLuint maxVal)
{
    return maxVal < 0xFFFFu ? PEX_MTLUInt16 : PEX_MTLUInt32;
}

static GLsizei type_size(unsigned t)
{
    return t == PEX_MTLUInt16 ? 2 : 4;
}

static GLuint mtl_restart_index(unsigned t)
{
    return t == PEX_MTLUInt16 ? 0xFFFFu : 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/*  expander state                                                    */
/* ------------------------------------------------------------------ */
struct PrimitiveExpander {
    uint8_t *buf;
    size_t   cap;
};

PrimitiveExpander *primitive_expander_new(void)
{
    return calloc(1, sizeof(PrimitiveExpander));
}

void primitive_expander_free(PrimitiveExpander *e)
{
    if (e) {
        free(e->buf);
        free(e);
    }
}

/* ensure the internal buffer has at least `bytes` of space */
static void ensure(PrimitiveExpander *e, size_t bytes)
{
    if (e->cap >= bytes) return;
    size_t n = max_sz(e->cap ? e->cap * 2 : 256, bytes);
    uint8_t *p = realloc(e->buf, n);
    if (!p) return;
    e->buf = p;
    e->cap = n;
}

/* ------------------------------------------------------------------ */
/*  how many output indices does each expandable mode need?           */
/*  Returns 0 when the input count is degenerate.                     */
/* ------------------------------------------------------------------ */
static GLsizei expanded_count(GLenum mode, GLsizei inCount)
{
    switch (mode) {
    case GL_TRIANGLE_FAN:
        return inCount >= 3 ? 3 * (inCount - 2) : 0;

    case GL_LINE_LOOP:
        /* a line list, so several loops can share one draw */
        return inCount >= 2 ? inCount * 2 : 0;

    case GL_LINES_ADJACENCY:
        return inCount >= 4 ? (inCount / 4) * 2 : 0;

    case GL_LINE_STRIP_ADJACENCY:
        return inCount >= 4 ? inCount - 2 : 0;

    case GL_TRIANGLES_ADJACENCY:
        return inCount >= 6 ? (inCount / 6) * 3 : 0;

    case GL_TRIANGLE_STRIP_ADJACENCY:
        return inCount >= 6 ? (inCount / 2) : 0;

    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/*  Metal primitive type for each expandable mode                     */
/* ------------------------------------------------------------------ */
static unsigned expanded_mtl_type(GLenum mode)
{
    switch (mode) {
    case GL_TRIANGLE_FAN:            return PEX_MTLTriangle;
    case GL_LINE_LOOP:               return PEX_MTLLine;
    case GL_LINES_ADJACENCY:         return PEX_MTLLine;
    case GL_LINE_STRIP_ADJACENCY:    return PEX_MTLLineStrip;
    case GL_TRIANGLES_ADJACENCY:     return PEX_MTLTriangle;
    case GL_TRIANGLE_STRIP_ADJACENCY:return PEX_MTLTriangleStrip;
    default:                         return 0;
    }
}

/* strip outputs need a restart sentinel between runs; lists do not */
static bool expands_to_strip(GLenum mode)
{
    return mode == GL_LINE_STRIP_ADJACENCY || mode == GL_TRIANGLE_STRIP_ADJACENCY;
}

/* ------------------------------------------------------------------ */
/*  public predicate                                                  */
/* ------------------------------------------------------------------ */
bool primitive_mode_needs_expand(GLenum mode)
{
    switch (mode) {
    case GL_TRIANGLE_FAN:
    case GL_LINE_LOOP:
    case GL_LINES_ADJACENCY:
    case GL_LINE_STRIP_ADJACENCY:
    case GL_TRIANGLES_ADJACENCY:
    case GL_TRIANGLE_STRIP_ADJACENCY:
    case GL_PATCHES:
        return true;
    default:
        return false;
    }
}

/* ------------------------------------------------------------------ */
/*  one run of source indices -> output indices                       */
/*  `get(i)` reads source index i of the run; returns the new write   */
/*  position.                                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    const uint8_t *src;     /* NULL means sequential 0..count-1 */
    GLenum         type;
    GLsizei        first;   /* offset of the run in the source stream */
} RunSource;

static GLuint run_get(const RunSource *r, GLsizei i)
{
    if (r->src == NULL)
        return (GLuint)(r->first + i);
    return read_index(r->src, r->type, r->first + i);
}

static GLsizei expand_run(GLenum mode, const RunSource *r, GLsizei count,
                          void *buf, unsigned it, GLsizei k)
{
    switch (mode) {

    case GL_TRIANGLE_FAN: {
        /* triangles: (0, i, i+1) for i=1..count-2 */
        GLuint i0 = run_get(r, 0);
        for (GLsizei i = 1; i <= count - 2; i++) {
            write_index(buf, it, k++, i0);
            write_index(buf, it, k++, run_get(r, i));
            write_index(buf, it, k++, run_get(r, i + 1));
        }
        break;
    }

    case GL_LINE_LOOP: {
        /* (0,1) (1,2) ... (n-1,0) */
        for (GLsizei i = 0; i < count; i++) {
            write_index(buf, it, k++, run_get(r, i));
            write_index(buf, it, k++, run_get(r, (i + 1) % count));
        }
        break;
    }

    case GL_LINES_ADJACENCY: {
        /* each group of 4: line from vertex 1 to 2 */
        GLsizei groups = count / 4;
        for (GLsizei g = 0; g < groups; g++) {
            write_index(buf, it, k++, run_get(r, g * 4 + 1));
            write_index(buf, it, k++, run_get(r, g * 4 + 2));
        }
        break;
    }

    case GL_LINE_STRIP_ADJACENCY: {
        /* strip of vertices 1 .. count-2 */
        for (GLsizei i = 0; i < count - 2; i++)
            write_index(buf, it, k++, run_get(r, i + 1));
        break;
    }

    case GL_TRIANGLES_ADJACENCY: {
        /* each group of 6: triangle from vertices 0, 2, 4 */
        GLsizei groups = count / 6;
        for (GLsizei g = 0; g < groups; g++) {
            write_index(buf, it, k++, run_get(r, g * 6));
            write_index(buf, it, k++, run_get(r, g * 6 + 2));
            write_index(buf, it, k++, run_get(r, g * 6 + 4));
        }
        break;
    }

    case GL_TRIANGLE_STRIP_ADJACENCY: {
        /* even-position vertices 0,2,4,... form a triangle strip
           (the odd positions are adjacency vertices) */
        for (GLsizei i = 0; i < count; i += 2)
            write_index(buf, it, k++, run_get(r, i));
        break;
    }

    default:
        break;
    }

    return k;
}

/* ------------------------------------------------------------------ */
/*  array expansion — synthesise sequential vertex indices            */
/* ------------------------------------------------------------------ */

PrimitiveExpansion primitive_expand_arrays(PrimitiveExpander *exp,
                                           GLMContext ctx,
                                           GLenum mode,
                                           GLsizei count)
{
    PrimitiveExpansion out = {0};
    (void)ctx;

    /* GL_PATCHES without tessellation: error sentinel */
    if (mode == GL_PATCHES)
        return out;

    GLsizei n = expanded_count(mode, count);
    if (n <= 0) {
        out.mtlType = expanded_mtl_type(mode);
        return out;
    }

    GLuint maxVal = (GLuint)(count - 1);
    unsigned it = choose_type(maxVal);
    size_t bytes = (size_t)n * type_size(it);
    ensure(exp, bytes);
    if (!exp->buf) return out;

    RunSource r = { NULL, 0, 0 };
    GLsizei k = expand_run(mode, &r, count, exp->buf, it, 0);

    out.mtlType    = expanded_mtl_type(mode);
    out.indices    = exp->buf;
    out.indexCount = k;
    out.indexType  = it;
    out.indexSize  = type_size(it);
    return out;
}

/* ------------------------------------------------------------------ */
/*  element expansion — remap from an existing index buffer           */
/* ------------------------------------------------------------------ */

/* the restart index in force for this index type, or false if none */
static bool restart_index(GLMContext ctx, GLenum type, GLuint *out)
{
    GLuint mask = type == GL_UNSIGNED_BYTE  ? 0xFFu :
                  type == GL_UNSIGNED_SHORT ? 0xFFFFu : 0xFFFFFFFFu;

    if (ctx->state.caps.primitive_restart_fixed_index) {
        *out = mask;
        return true;
    }
    if (ctx->state.caps.primitive_restart) {
        /* only the low bits that fit the index type take part */
        *out = ctx->state.var.primitive_restart_index & mask;
        return true;
    }
    return false;
}

PrimitiveExpansion primitive_expand_elements(PrimitiveExpander *exp,
                                             GLMContext ctx,
                                             GLenum mode,
                                             GLsizei count,
                                             GLenum type,
                                             size_t offset)
{
    PrimitiveExpansion out = {0};

    if (mode == GL_PATCHES)
        return out;

    if (expanded_mtl_type(mode) == 0)
        return out;

    Buffer *eb = ctx->state.vao->element_array.buffer;
    if (!eb || !eb->data.buffer_data) return out;

    const uint8_t *src = ((const uint8_t *)eb->data.buffer_data) + offset;

    GLuint restart = 0;
    bool   has_restart = restart_index(ctx, type, &restart);
    bool   strip = expands_to_strip(mode);

    /* first pass: largest real index, and the output size over all runs */
    GLuint  maxVal = 0;
    GLsizei n = 0;
    GLsizei runs = 0;
    GLsizei run_start = 0;

    for (GLsizei i = 0; i <= count; i++) {
        bool cut = (i == count);
        GLuint v = 0;

        if (!cut) {
            v = read_index(src, type, i);
            cut = has_restart && v == restart;
        }

        if (cut) {
            GLsizei len = i - run_start;
            GLsizei m = expanded_count(mode, len);
            if (m > 0) {
                if (strip && runs > 0)
                    n += 1;     /* sentinel between strips */
                n += m;
                runs++;
            }
            run_start = i + 1;
        } else if (v > maxVal) {
            maxVal = v;
        }
    }

    out.mtlType = expanded_mtl_type(mode);

    if (n <= 0)
        return out;

    unsigned it = choose_type(maxVal);
    size_t bytes = (size_t)n * type_size(it);
    ensure(exp, bytes);
    if (!exp->buf) {
        out.mtlType = 0;
        return out;
    }

    /* second pass: expand each run in place */
    GLsizei k = 0;
    runs = 0;
    run_start = 0;

    for (GLsizei i = 0; i <= count; i++) {
        bool cut = (i == count);

        if (!cut)
            cut = has_restart && read_index(src, type, i) == restart;

        if (cut) {
            GLsizei len = i - run_start;
            if (expanded_count(mode, len) > 0) {
                if (strip && runs > 0)
                    write_index(exp->buf, it, k++, mtl_restart_index(it));
                RunSource r = { src, type, run_start };
                k = expand_run(mode, &r, len, exp->buf, it, k);
                runs++;
            }
            run_start = i + 1;
        }
    }

    out.indices    = exp->buf;
    out.indexCount = k;
    out.indexType  = it;
    out.indexSize  = type_size(it);
    return out;
}
