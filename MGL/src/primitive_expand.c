/*
 * primitive_expand.c
 * MGL
 *
 * Expand GL primitive modes that have no direct Metal equivalent into
 * indexed draws.  The expander owns a reusable malloc buffer; every call
 * reuses or grows it, so repeated identical draws do not allocate.
 *
 * Primitive restart is not handled.  A draw with GL_PRIMITIVE_RESTART
 * enabled on TRIANGLE_FAN or LINE_LOOP will produce incorrect results.
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
/* ------------------------------------------------------------------ */
static unsigned choose_type(GLuint maxVal)
{
    return maxVal <= 0xFFFFu ? PEX_MTLUInt16 : PEX_MTLUInt32;
}

static GLsizei type_size(unsigned t)
{
    return t == PEX_MTLUInt16 ? 2 : 4;
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
        return inCount >= 2 ? inCount + 1 : 0;

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
    case GL_LINE_LOOP:               return PEX_MTLLineStrip;
    case GL_LINES_ADJACENCY:         return PEX_MTLLine;
    case GL_LINE_STRIP_ADJACENCY:    return PEX_MTLLineStrip;
    case GL_TRIANGLES_ADJACENCY:     return PEX_MTLTriangle;
    case GL_TRIANGLE_STRIP_ADJACENCY:return PEX_MTLTriangleStrip;
    default:                         return 0;
    }
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

    switch (mode) {

    case GL_TRIANGLE_FAN: {
        /* triangles: (0, i, i+1) for i=1..count-2 */
        GLsizei k = 0;
        for (GLsizei i = 1; i <= count - 2; i++) {
            write_index(exp->buf, it, k++, 0);
            write_index(exp->buf, it, k++, (GLuint)i);
            write_index(exp->buf, it, k++, (GLuint)(i + 1));
        }
        break;
    }

    case GL_LINE_LOOP: {
        /* 0,1,...,count-1,0 */
        GLsizei i;
        for (i = 0; i < count; i++)
            write_index(exp->buf, it, i, (GLuint)i);
        write_index(exp->buf, it, i, 0);
        break;
    }

    case GL_LINES_ADJACENCY: {
        /* each group of 4: line from vertex 1 to 2 */
        GLsizei groups = count / 4;
        for (GLsizei g = 0; g < groups; g++) {
            write_index(exp->buf, it, g * 2,     (GLuint)(g * 4 + 1));
            write_index(exp->buf, it, g * 2 + 1, (GLuint)(g * 4 + 2));
        }
        break;
    }

    case GL_LINE_STRIP_ADJACENCY: {
        /* strip of vertices 1 .. count-2 */
        for (GLsizei i = 0; i < count - 2; i++)
            write_index(exp->buf, it, i, (GLuint)(i + 1));
        break;
    }

    case GL_TRIANGLES_ADJACENCY: {
        /* each group of 6: triangle from vertices 0, 2, 4 */
        GLsizei groups = count / 6;
        for (GLsizei g = 0; g < groups; g++) {
            write_index(exp->buf, it, g * 3,     (GLuint)(g * 6));
            write_index(exp->buf, it, g * 3 + 1, (GLuint)(g * 6 + 2));
            write_index(exp->buf, it, g * 3 + 2, (GLuint)(g * 6 + 4));
        }
        break;
    }

    case GL_TRIANGLE_STRIP_ADJACENCY: {
        /* even-position vertices 0,2,4,... form a triangle strip
           (the odd positions are adjacency vertices) */
        GLsizei k = 0;
        for (GLsizei i = 0; i < count; i += 2)
            write_index(exp->buf, it, k++, (GLuint)i);
        break;
    }

    default:
        return out;
    }

    out.mtlType    = expanded_mtl_type(mode);
    out.indices    = exp->buf;
    out.indexCount = n;
    out.indexType  = it;
    out.indexSize  = type_size(it);
    return out;
}

/* ------------------------------------------------------------------ */
/*  element expansion — remap from an existing index buffer           */
/* ------------------------------------------------------------------ */

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

    GLsizei n = expanded_count(mode, count);
    if (n <= 0) {
        out.mtlType = expanded_mtl_type(mode);
        return out;
    }

    Buffer *eb = ctx->state.vao->element_array.buffer;
    if (!eb || !eb->data.buffer_data) return out;

    const uint8_t *src = ((const uint8_t *)eb->data.buffer_data) + offset;

    /* first pass: find the maximum source index */
    GLuint maxVal = 0;
    for (GLsizei i = 0; i < count; i++) {
        GLuint v = read_index(src, type, i);
        if (v > maxVal) maxVal = v;
    }

    unsigned it = choose_type(maxVal);
    size_t bytes = (size_t)n * type_size(it);
    ensure(exp, bytes);
    if (!exp->buf) return out;

    switch (mode) {

    case GL_TRIANGLE_FAN: {
        GLuint i0 = read_index(src, type, 0);
        GLsizei k = 0;
        for (GLsizei i = 1; i <= count - 2; i++) {
            write_index(exp->buf, it, k++, i0);
            write_index(exp->buf, it, k++, read_index(src, type, i));
            write_index(exp->buf, it, k++, read_index(src, type, i + 1));
        }
        break;
    }

    case GL_LINE_LOOP: {
        GLsizei i;
        for (i = 0; i < count; i++)
            write_index(exp->buf, it, i, read_index(src, type, i));
        write_index(exp->buf, it, i, read_index(src, type, 0));
        break;
    }

    case GL_LINES_ADJACENCY: {
        GLsizei groups = count / 4;
        for (GLsizei g = 0; g < groups; g++) {
            write_index(exp->buf, it, g * 2,
                        read_index(src, type, g * 4 + 1));
            write_index(exp->buf, it, g * 2 + 1,
                        read_index(src, type, g * 4 + 2));
        }
        break;
    }

    case GL_LINE_STRIP_ADJACENCY: {
        for (GLsizei i = 0; i < count - 2; i++)
            write_index(exp->buf, it, i, read_index(src, type, i + 1));
        break;
    }

    case GL_TRIANGLES_ADJACENCY: {
        GLsizei groups = count / 6;
        for (GLsizei g = 0; g < groups; g++) {
            write_index(exp->buf, it, g * 3,
                        read_index(src, type, g * 6));
            write_index(exp->buf, it, g * 3 + 1,
                        read_index(src, type, g * 6 + 2));
            write_index(exp->buf, it, g * 3 + 2,
                        read_index(src, type, g * 6 + 4));
        }
        break;
    }

    case GL_TRIANGLE_STRIP_ADJACENCY: {
        /* even-position vertices form a triangle strip */
        GLsizei k = 0;
        for (GLsizei i = 0; i < count; i += 2)
            write_index(exp->buf, it, k++, read_index(src, type, i));
        break;
    }

    default:
        return out;
    }

    out.mtlType    = expanded_mtl_type(mode);
    out.indices    = exp->buf;
    out.indexCount = n;
    out.indexType  = it;
    out.indexSize  = type_size(it);
    return out;
}
