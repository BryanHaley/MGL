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
 * geometry_shaders.c
 * MGL
 *
 * Geometry shaders, rewritten into compute shaders.
 *
 * Metal has no geometry stage and SPIRV-Cross will not translate one -- it
 * emits "unknown" where the entry point should be. So the stage is turned
 * into a compute shader here, at the GLSL level, before anything downstream
 * sees it. One invocation handles one input primitive: it reads the vertex
 * stage's output out of a buffer, runs the shader's own body, and writes the
 * vertices it emits into another buffer. A generated pass-through vertex
 * shader then draws those.
 *
 * Strips are unrolled into separate primitives on the way out, so the draw
 * needs no restart index and every emitted triangle stands on its own.
 */

#include "glm_context.h"
#include "mgl_log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// text helpers, the same shape as the ones in subroutines.c
// ---------------------------------------------------------------------------

static bool identChar(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static bool wordAt(const char *src, size_t at, const char *word)
{
    size_t n = strlen(word);

    if (strncmp(src + at, word, n))
        return false;

    if (at > 0 && identChar(src[at - 1]))
        return false;

    return !identChar(src[at + n]);
}

static size_t skipSpace(const char *s, size_t i)
{
    while (s[i] && isspace((unsigned char)s[i]))
        i++;

    return i;
}

static size_t readIdent(const char *s, size_t i, char *dst, size_t dstlen)
{
    size_t n = 0;

    while (s[i] && identChar(s[i]))
    {
        if (n + 1 < dstlen)
            dst[n++] = s[i];

        i++;
    }

    dst[n] = 0;

    return i;
}

static size_t matchBracket(const char *s, size_t i)
{
    int depth = 0;

    for (; s[i]; i++)
    {
        if (s[i] == '[') depth++;
        else if (s[i] == ']' && --depth == 0) return i + 1;
    }

    return i;
}

typedef struct {
    char *s;
    size_t len;
    size_t cap;
} Buf;

static bool bufAddN(Buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap)
    {
        size_t cap = b->cap ? b->cap : 2048;

        while (cap < b->len + n + 1)
            cap *= 2;

        char *p = (char *)realloc(b->s, cap);

        if (p == NULL)
            return false;

        b->s = p;
        b->cap = cap;
    }

    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;

    return true;
}

static bool bufAdd(Buf *b, const char *s)
{
    return bufAddN(b, s, strlen(s));
}

// ---------------------------------------------------------------------------
// what the geometry shader declared
// ---------------------------------------------------------------------------

#define MAX_GS_VARYINGS 32

typedef struct {
    char type[32];
    char name[64];
    char qualifier[32];   // flat / noperspective, kept for the pass-through
} GsVarying;

typedef struct {
    GsVarying in[MAX_GS_VARYINGS];  int in_count;
    GsVarying out[MAX_GS_VARYINGS]; int out_count;
} GsScan;

// "in gl_PerVertex { ... } gl_in[];" -- a geometry shader may redeclare the
// built-in block to say which built-ins it reads. The compute shader this turns
// into has its own storage for those, and an interface block is not legal there
// at all, so the declaration is dropped. Returns the index past the semicolon,
// or 0 when this is not one.
static size_t skipPerVertexBlock(const char *s, size_t i)
{
    size_t j = i;

    if (wordAt(s, j, "in"))       j += 2;
    else if (wordAt(s, j, "out")) j += 3;
    else return 0;

    j = skipSpace(s, j);

    if (!wordAt(s, j, "gl_PerVertex"))
        return 0;

    j = skipSpace(s, j + 12);

    if (s[j] != '{')
        return 0;

    int depth = 0;

    for (; s[j]; j++)
    {
        if (s[j] == '{') depth++;
        else if (s[j] == '}' && --depth == 0) { j++; break; }
    }

    // whatever instance name and array size follow, up to the semicolon
    while (s[j] && s[j] != ';') j++;

    return s[j] == ';' ? j + 1 : 0;
}

// The declaration is "[layout(...)] [interp] in|out TYPE NAME[maybe];". Returns
// the index just past the semicolon, or 0 when this is not one.
static size_t readVarying(const char *s, size_t i, bool want_out, GsVarying *v, bool *is_varying)
{
    size_t start = i;
    char word[64];
    char prev[64] = "";
    char prev2[64] = "";
    bool seen = false;

    *is_varying = false;

    // a layout(...) prefix is allowed and carries nothing this needs
    if (wordAt(s, i, "layout"))
    {
        size_t j = skipSpace(s, i + 6);

        if (s[j] != '(')
            return 0;

        int depth = 0;

        for (; s[j]; j++)
        {
            if (s[j] == '(') depth++;
            else if (s[j] == ')' && --depth == 0) { j++; break; }
        }

        i = skipSpace(s, j);
    }

    memset(v, 0, sizeof(*v));

    // walk the words up to the semicolon, remembering the last two
    while (s[i] && s[i] != ';' && s[i] != '{' && s[i] != '(')
    {
        if (!identChar(s[i]))
        {
            if (s[i] == '[')
            {
                i = matchBracket(s, i);
                continue;
            }

            i++;
            continue;
        }

        i = readIdent(s, i, word, sizeof(word));
        i = skipSpace(s, i);

        if (!strcmp(word, "in") || !strcmp(word, "out"))
        {
            if (strcmp(word, want_out ? "out" : "in"))
                return 0;

            seen = true;
            continue;
        }

        if (!strcmp(word, "flat") || !strcmp(word, "smooth") ||
            !strcmp(word, "noperspective") || !strcmp(word, "centroid") ||
            !strcmp(word, "sample") || !strcmp(word, "invariant") ||
            !strcmp(word, "precise") || !strcmp(word, "highp") ||
            !strcmp(word, "mediump") || !strcmp(word, "lowp"))
        {
            if (!strcmp(word, "flat") || !strcmp(word, "noperspective"))
                snprintf(v->qualifier, sizeof(v->qualifier), "%s", word);

            continue;
        }

        snprintf(prev2, sizeof(prev2), "%s", prev);
        snprintf(prev, sizeof(prev), "%s", word);
    }

    // "layout(...) in;" with no name is the primitive declaration, not a varying
    if (!seen || s[i] != ';' || prev[0] == 0 || prev2[0] == 0)
        return 0;

    snprintf(v->type, sizeof(v->type), "%s", prev2);
    snprintf(v->name, sizeof(v->name), "%s", prev);

    *is_varying = true;

    return i + 1 > start ? i + 1 : 0;
}

// std430 layout for the handful of types a varying can have. Returns the size
// and sets *align; zero means MGL does not know the type.
static int std430Size(const char *type, int *align)
{
    static const struct { const char *name; int size, align; } table[] = {
        { "float", 4, 4 },  { "int", 4, 4 },    { "uint", 4, 4 },   { "bool", 4, 4 },
        { "vec2", 8, 8 },   { "ivec2", 8, 8 },  { "uvec2", 8, 8 },  { "bvec2", 8, 8 },
        { "vec3", 12, 16 }, { "ivec3", 12, 16 },{ "uvec3", 12, 16 },{ "bvec3", 12, 16 },
        { "vec4", 16, 16 }, { "ivec4", 16, 16 },{ "uvec4", 16, 16 },{ "bvec4", 16, 16 },
        { "mat2", 16, 8 },  { "mat3", 48, 16 }, { "mat4", 64, 16 },
        { "mat2x2", 16, 8 },{ "mat2x3", 32, 16 },{ "mat2x4", 32, 16 },
        { "mat3x2", 24, 8 },{ "mat3x3", 48, 16 },{ "mat3x4", 48, 16 },
        { "mat4x2", 32, 8 },{ "mat4x3", 64, 16 },{ "mat4x4", 64, 16 },
        { "double", 8, 8 }, { "dvec2", 16, 16 },{ "dvec3", 24, 32 },{ "dvec4", 32, 32 },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (!strcmp(type, table[i].name))
        {
            *align = table[i].align;
            return table[i].size;
        }

    *align = 4;

    return 0;
}

static int roundUp(int v, int a)
{
    return a ? ((v + a - 1) / a) * a : v;
}

// The size of the shared vertex struct, laid out the way glslang will lay it
// out, so the driver can size the buffers it allocates for these shaders.
static int structStride(GsVarying *v, int count, bool is_out)
{
    int offset = 0, worst = 16;

    // vec4 mglPos, float mglPointSize, and for the output side mglValid,
    // mglLayer and mglPrimitiveID
    offset = 16 + 4;

    if (is_out)
        offset += 4 + 4 + 4;

    for (int i = 0; i < count; i++)
    {
        int align = 4;
        int size = std430Size(v[i].type, &align);

        if (size == 0)
            return 0;

        if (align > worst)
            worst = align;

        offset = roundUp(offset, align) + size;
    }

    return roundUp(offset, worst);
}

// GL's five input primitives, and how many vertices each has
static int inputVertexCount(GLenum prim)
{
    switch (prim)
    {
        case GL_LINES:                      return 2;
        case GL_LINES_ADJACENCY:            return 4;
        case GL_TRIANGLES:                  return 3;
        case GL_TRIANGLES_ADJACENCY:        return 6;
        default:                            return 1;   // GL_POINTS
    }
}

// vertices per primitive the emitter writes out
static int outputVertexCount(GLenum prim)
{
    switch (prim)
    {
        case GL_LINE_STRIP:     return 2;
        case GL_TRIANGLE_STRIP: return 3;
        default:                return 1;   // GL_POINTS
    }
}

// ---------------------------------------------------------------------------
// pass 1: read the layout declarations and strip them out
// ---------------------------------------------------------------------------

static bool scanGeometry(const char *src, GeometryInfo *gi, GsScan *sc, Buf *body)
{
    size_t i = 0, copied = 0;

    gi->in_primitive = GL_POINTS;
    gi->out_primitive = GL_POINTS;
    gi->max_vertices = 1;
    gi->invocations = 1;

    while (src[i])
    {
        if (src[i] == '/' && src[i + 1] == '/')
        {
            while (src[i] && src[i] != '\n') i++;
            continue;
        }

        if (src[i] == '/' && src[i + 1] == '*')
        {
            i += 2;
            while (src[i] && !(src[i] == '*' && src[i + 1] == '/')) i++;
            if (src[i]) i += 2;
            continue;
        }

        if (!identChar(src[i]) || (i && identChar(src[i - 1])))
        {
            i++;
            continue;
        }

        // ---- layout(...) in|out; -- the primitive declarations ----
        if (wordAt(src, i, "layout"))
        {
            size_t j = skipSpace(src, i + 6);

            if (src[j] == '(')
            {
                size_t close = j;
                int depth = 0;

                for (; src[close]; close++)
                {
                    if (src[close] == '(') depth++;
                    else if (src[close] == ')' && --depth == 0) { close++; break; }
                }

                size_t after = skipSpace(src, close);
                bool is_in = wordAt(src, after, "in");
                bool is_out = wordAt(src, after, "out");
                size_t semi = after + (is_in ? 2 : is_out ? 3 : 0);

                semi = skipSpace(src, semi);

                if ((is_in || is_out) && src[semi] == ';')
                {
                    char inner[256];
                    size_t n = close - j - 2 < sizeof(inner) - 1 ? close - j - 2 : sizeof(inner) - 1;

                    memcpy(inner, src + j + 1, n);
                    inner[n] = 0;

                    if (is_in)
                    {
                        if (strstr(inner, "triangles_adjacency")) gi->in_primitive = GL_TRIANGLES_ADJACENCY;
                        else if (strstr(inner, "lines_adjacency")) gi->in_primitive = GL_LINES_ADJACENCY;
                        else if (strstr(inner, "triangles")) gi->in_primitive = GL_TRIANGLES;
                        else if (strstr(inner, "lines")) gi->in_primitive = GL_LINES;
                        else if (strstr(inner, "points")) gi->in_primitive = GL_POINTS;

                        const char *inv = strstr(inner, "invocations");

                        if (inv)
                        {
                            const char *eq = strchr(inv, '=');

                            if (eq)
                                gi->invocations = atoi(eq + 1);
                        }
                    }
                    else
                    {
                        if (strstr(inner, "triangle_strip")) gi->out_primitive = GL_TRIANGLE_STRIP;
                        else if (strstr(inner, "line_strip")) gi->out_primitive = GL_LINE_STRIP;
                        else if (strstr(inner, "points")) gi->out_primitive = GL_POINTS;

                        const char *mv = strstr(inner, "max_vertices");

                        if (mv)
                        {
                            const char *eq = strchr(mv, '=');

                            if (eq)
                                gi->max_vertices = atoi(eq + 1);
                        }
                    }

                    // the declaration has no equivalent in a compute shader
                    if (!bufAddN(body, src + copied, i - copied))
                        return false;

                    i = semi + 1;
                    copied = i;
                    continue;
                }
            }
        }

        // ---- in|out gl_PerVertex { ... }; -- the built-in block, dropped ----
        {
            size_t end = skipPerVertexBlock(src, i);

            if (end)
            {
                if (!bufAddN(body, src + copied, i - copied))
                    return false;

                i = end;
                copied = i;
                continue;
            }
        }

        // ---- a varying declaration ----
        {
            GsVarying v;
            bool is_varying = false;
            size_t end = readVarying(src, i, false, &v, &is_varying);

            if (is_varying && end && sc->in_count < MAX_GS_VARYINGS)
            {
                sc->in[sc->in_count++] = v;

                if (!bufAddN(body, src + copied, i - copied))
                    return false;

                i = end;
                copied = i;
                continue;
            }

            end = readVarying(src, i, true, &v, &is_varying);

            if (is_varying && end && sc->out_count < MAX_GS_VARYINGS)
            {
                sc->out[sc->out_count++] = v;

                if (!bufAddN(body, src + copied, i - copied))
                    return false;

                i = end;
                copied = i;
                continue;
            }
        }

        while (src[i] && identChar(src[i])) i++;
    }

    if (gi->invocations < 1)
        gi->invocations = 1;

    if (gi->max_vertices < 1)
        gi->max_vertices = 1;

    return bufAdd(body, src + copied);
}

// How many locations a varying of this type takes: one per matrix column,
// two per column of doubles wider than two.
static int locationsFor(const char *type)
{
    const char *t = type;
    int cols = 1, wide = 1;

    if (!strncmp(t, "dmat", 4))
    {
        cols = t[4] - '0';
        wide = (t[5] == 'x' ? t[6] - '0' : cols) > 2 ? 2 : 1;
    }
    else if (!strncmp(t, "mat", 3))
        cols = t[3] - '0';
    else if (!strncmp(t, "dvec", 4))
        wide = t[4] - '2' > 0 ? 2 : 1;

    return cols * wide;
}

// what every generated geometry kernel has to do before the shader's own code
#define MGL_GS_PROLOGUE \
    "\n  mglGsPrims = mglGsPrimsU;\n" \
    "  mglGsIndexed = mglGsIndexedU;\n" \
    "  mglGsFirst = mglGsFirstU;\n" \
    "  mglGsStride = mglGsStrideU;\n" \
    "  int mglId = int(gl_GlobalInvocationID.x);\n" \
    "  mglPrimitiveID = mglId / mglGsInvocations;\n" \
    "  mglInvocationID = mglId - mglPrimitiveID * mglGsInvocations;\n" \
    "  if (mglPrimitiveID >= mglGsPrims) return;\n" \
    "  if (mglGsIn[mglFetch(0)].mglPointSize == 0.0) return;\n" \
    "  mglBase = mglId * mglGsCap;\n" \
    "  mglCur.mglPointSize = mglGsIn[mglFetch(0)].mglPointSize;\n" \
    "  mglWritten = 0;\n  mglStripLen = 0;\n  mglStripFlip = false;\n"

// ---------------------------------------------------------------------------
// pass 2: rewrite what the body says into what the compute shader means
// ---------------------------------------------------------------------------

static bool rewriteBody(const char *src, GsScan *sc, Buf *out)
{
    size_t i = 0, copied = 0;

    while (src[i])
    {
        if (!identChar(src[i]) || (i && identChar(src[i - 1])))
        {
            i++;
            continue;
        }

        const char *simple = NULL;
        char repl[256];
        size_t skip = 0;

        if (wordAt(src, i, "EmitVertex"))              { simple = "mglEmitVertex"; skip = 10; }
        else if (wordAt(src, i, "EndPrimitive"))       { simple = "mglEndPrimitive"; skip = 12; }
        else if (wordAt(src, i, "EmitStreamVertex"))   { simple = "mglEmitVertex"; skip = 16; }
        else if (wordAt(src, i, "EndStreamPrimitive")) { simple = "mglEndPrimitive"; skip = 18; }
        else if (wordAt(src, i, "gl_PrimitiveIDIn"))   { simple = "mglPrimitiveID"; skip = 16; }
        else if (wordAt(src, i, "gl_InvocationID"))    { simple = "mglInvocationID"; skip = 15; }
        else if (wordAt(src, i, "gl_Position"))        { simple = "mglCur.mglPos"; skip = 11; }
        else if (wordAt(src, i, "gl_PointSize"))       { simple = "mglCur.mglPointSize"; skip = 12; }
        else if (wordAt(src, i, "gl_Layer"))           { simple = "mglCur.mglLayer"; skip = 8; }
        else if (wordAt(src, i, "gl_PrimitiveID"))     { simple = "mglCur.mglPrimitiveID"; skip = 14; }
        else if (wordAt(src, i, "gl_in"))
        {
            // gl_in[EXPR].member -> mglGsIn[mglFetch(EXPR)].member
            size_t k = skipSpace(src, i + 5);

            if (src[k] == '[')
            {
                size_t close = matchBracket(src, k);

                if (!bufAddN(out, src + copied, i - copied) ||
                    !bufAdd(out, "mglGsIn[mglFetch(") ||
                    !bufAddN(out, src + k + 1, close - k - 2) ||
                    !bufAdd(out, ")]"))
                    return false;

                i = close;
                copied = i;

                // the member name that follows has its own spelling here
                size_t d = skipSpace(src, i);

                if (src[d] == '.')
                {
                    size_t m = skipSpace(src, d + 1);

                    if (wordAt(src, m, "gl_Position"))
                    {
                        if (!bufAdd(out, ".mglPos")) return false;
                        i = m + 11;
                        copied = i;
                    }
                    else if (wordAt(src, m, "gl_PointSize"))
                    {
                        if (!bufAdd(out, ".mglPointSize")) return false;
                        i = m + 12;
                        copied = i;
                    }
                }

                continue;
            }

            // gl_in.length()
            if (src[k] == '.' && wordAt(src, skipSpace(src, k + 1), "length"))
            {
                size_t e = strchr(src + k, ')') ? (size_t)(strchr(src + k, ')') - src) + 1 : k;

                if (!bufAddN(out, src + copied, i - copied) ||
                    !bufAdd(out, "mglGsInPerPrim"))
                    return false;

                i = e;
                copied = i;
                continue;
            }
        }

        if (simple)
        {
            if (!bufAddN(out, src + copied, i - copied) || !bufAdd(out, simple))
                return false;

            i += skip;
            copied = i;
            continue;
        }

        // an input varying is an array indexed by vertex; an output is not
        {
            int hit = -1;

            for (int v = 0; v < sc->in_count; v++)
                if (wordAt(src, i, sc->in[v].name)) { hit = v; break; }

            if (hit >= 0)
            {
                size_t k = skipSpace(src, i + strlen(sc->in[hit].name));

                if (src[k] == '[')
                {
                    size_t close = matchBracket(src, k);

                    snprintf(repl, sizeof(repl), "mglGsIn[mglFetch(");

                    if (!bufAddN(out, src + copied, i - copied) ||
                        !bufAdd(out, repl) ||
                        !bufAddN(out, src + k + 1, close - k - 2) ||
                        !bufAdd(out, ")].") ||
                        !bufAdd(out, sc->in[hit].name))
                        return false;

                    i = close;
                    copied = i;
                    continue;
                }
            }

            hit = -1;

            for (int v = 0; v < sc->out_count; v++)
                if (wordAt(src, i, sc->out[v].name)) { hit = v; break; }

            if (hit >= 0)
            {
                snprintf(repl, sizeof(repl), "mglCur.%s", sc->out[hit].name);

                if (!bufAddN(out, src + copied, i - copied) || !bufAdd(out, repl))
                    return false;

                i += strlen(sc->out[hit].name);
                copied = i;
                continue;
            }
        }

        // The shader's own entry point stays the entry point and the prologue is
        // injected into it. Calling a separate function instead makes every
        // global it touches a parameter, and Metal refuses a uniform passed that
        // way -- it lives in constant space and the parameter does not.
        if (wordAt(src, i, "main"))
        {
            size_t b = i + 4;

            while (src[b] && src[b] != '{')
                b++;

            if (src[b] == '{')
            {
                if (!bufAddN(out, src + copied, b + 1 - copied) ||
                    !bufAdd(out, MGL_GS_PROLOGUE))
                    return false;

                i = b + 1;
                copied = i;
                continue;
            }
        }

        while (src[i] && identChar(src[i])) i++;
    }

    return bufAdd(out, src + copied);
}

// ---------------------------------------------------------------------------
// the shared vertex struct, declared identically everywhere it is used
// ---------------------------------------------------------------------------

static bool emitStructs(GsScan *sc, Buf *b)
{
    char line[256];

    if (!bufAdd(b, "struct MglGsInV {\n  vec4 mglPos;\n  float mglPointSize;\n"))
        return false;

    for (int i = 0; i < sc->in_count; i++)
    {
        snprintf(line, sizeof(line), "  %s %s;\n", sc->in[i].type, sc->in[i].name);

        if (!bufAdd(b, line))
            return false;
    }

    if (!bufAdd(b, "};\n\nstruct MglGsOutV {\n  vec4 mglPos;\n  float mglPointSize;\n"
                   "  float mglValid;\n  int mglLayer;\n  int mglPrimitiveID;\n"))
        return false;

    for (int i = 0; i < sc->out_count; i++)
    {
        snprintf(line, sizeof(line), "  %s %s;\n", sc->out[i].type, sc->out[i].name);

        if (!bufAdd(b, line))
            return false;
    }

    return bufAdd(b, "};\n\n");
}

// ---------------------------------------------------------------------------
// the entry points
// ---------------------------------------------------------------------------

void mglFreeGeometryInfo(GeometryInfo *gi)
{
    if (gi == NULL)
        return;

    free(gi->compute_src);
    free(gi->passthrough_src);
    free(gi->capture_decl);

    memset(gi, 0, sizeof(*gi));
}

// Turns a geometry shader into a compute shader, and generates the vertex
// shader that draws what it produced. Returns false when the source has no
// geometry shader shape this understands.
bool mglRewriteGeometryShader(const char *src, GeometryInfo *gi)
{
    GsScan sc;
    Buf stripped = {0}, body = {0}, out = {0}, pass = {0}, decl = {0};
    bool ok = false;
    char line[512];
    int in_verts, out_verts, cap;

    memset(&sc, 0, sizeof(sc));
    memset(gi, 0, sizeof(*gi));

    if (!scanGeometry(src, gi, &sc, &stripped))
        goto done;

    if (!rewriteBody(stripped.s ? stripped.s : "", &sc, &body))
        goto done;

    in_verts = inputVertexCount(gi->in_primitive);
    out_verts = outputVertexCount(gi->out_primitive);

    // worst case every emit after the first completes a primitive
    cap = gi->max_vertices * out_verts;

    gi->in_vertices = in_verts;
    gi->out_vertices_per_primitive = out_verts;
    gi->slot_capacity = cap;
    gi->in_stride = structStride(sc.in, sc.in_count, false);
    gi->out_stride = structStride(sc.out, sc.out_count, true);

    // the same walk as structStride, keeping each output's offset
    {
        int offset = 16 + 4 + 4 + 4 + 4;

        gi->out_count = 0;

        for (int i = 0; i < sc.out_count && i < 32; i++)
        {
            int align = 4;
            int size = std430Size(sc.out[i].type, &align);

            offset = roundUp(offset, align);
            snprintf(gi->out_names[i], sizeof(gi->out_names[i]), "%s", sc.out[i].name);
            snprintf(gi->out_types[i], sizeof(gi->out_types[i]), "%s", sc.out[i].type);
            gi->out_offsets[i] = offset;
            gi->out_count = i + 1;
            offset += size;
        }
    }

    if (gi->in_stride == 0 || gi->out_stride == 0)
    {
        MGL_ERR("MGL Error: a geometry varying has a type MGL cannot size\n");
        goto done;
    }

    // ---- the compute shader ----
    if (!bufAdd(&out, "#version 460\n\nlayout(local_size_x = 1) in;\n\n"))
        goto done;

    if (!emitStructs(&sc, &out))
        goto done;

    snprintf(line, sizeof(line),
        "layout(std430, binding = %d) readonly buffer MglGsInB { MglGsInV mglGsIn[]; };\n"
        "layout(std430, binding = %d) buffer MglGsOutB { MglGsOutV mglGsOut[]; };\n"
        "layout(std430, binding = %d) readonly buffer MglGsIdxB { uint mglGsIdx[]; };\n\n",
        MGL_GS_IN_BINDING, MGL_GS_OUT_BINDING, MGL_GS_INDEX_BINDING);

    if (!bufAdd(&out, line))
        goto done;

    snprintf(line, sizeof(line),
        // SPIRV-Cross hoists a uniform a function reads into a reference
        // parameter, and Metal will not bind a constant to a thread reference
        // -- so the draw's own numbers are copied into ordinary variables and
        // only those are read below.
        "uniform int mglGsPrimsU;\nuniform int mglGsIndexedU;\nuniform int mglGsFirstU;\nuniform int mglGsStrideU;\n"
        "int mglGsPrims;\nint mglGsIndexed;\nint mglGsFirst;\nint mglGsStride;\n\n"
        "const int mglGsInPerPrim = %d;\nconst int mglGsOutVerts = %d;\n"
        "const int mglGsCap = %d;\nconst int mglGsInvocations = %d;\n\n",
        in_verts, out_verts, cap, gi->invocations);

    if (!bufAdd(&out, line))
        goto done;

    if (!bufAdd(&out,
        "int mglPrimitiveID;\nint mglInvocationID;\nint mglBase;\nint mglWritten;\n"
        "MglGsOutV mglCur;\nMglGsOutV mglStrip[3];\nint mglStripLen;\nbool mglStripFlip;\n\n"
        "int mglFetch(int i)\n{\n"
        "  int v = mglPrimitiveID * mglGsStride + i;\n"
        "  return mglGsIndexed != 0 ? int(mglGsIdx[v]) : mglGsFirst + v;\n}\n\n"
        "void mglPut(MglGsOutV v)\n{\n"
        "  if (mglWritten >= mglGsCap) return;\n"
        "  v.mglValid = 1.0;\n"
        "  mglGsOut[mglBase + mglWritten] = v;\n"
        "  mglWritten++;\n}\n\n"
        "void mglEmitVertex()\n{\n"
        "  if (mglGsOutVerts == 1) { mglPut(mglCur); return; }\n"
        "  if (mglStripLen < mglGsOutVerts - 1) { mglStrip[mglStripLen] = mglCur; mglStripLen++; return; }\n"
        "  if (mglGsOutVerts == 2) { mglPut(mglStrip[0]); mglPut(mglCur); mglStrip[0] = mglCur; return; }\n"
        "  if (mglStripFlip) { mglPut(mglStrip[1]); mglPut(mglStrip[0]); mglPut(mglCur); }\n"
        "  else { mglPut(mglStrip[0]); mglPut(mglStrip[1]); mglPut(mglCur); }\n"
        "  mglStrip[0] = mglStrip[1];\n  mglStrip[1] = mglCur;\n"
        "  mglStripFlip = !mglStripFlip;\n}\n\n"
        "void mglEndPrimitive()\n{\n  mglStripLen = 0;\n  mglStripFlip = false;\n}\n\n"
        // EmitStreamVertex and EndStreamPrimitive name a stream. Only stream
        // zero is rasterised, and it is the only one MGL records; anything
        // written to another stream is dropped here.
        "void mglEmitVertex(int mglStream)\n{\n  if (mglStream == 0) mglEmitVertex();\n}\n\n"
        "void mglEndPrimitive(int mglStream)\n{\n  if (mglStream == 0) mglEndPrimitive();\n}\n\n"))
        goto done;

    // the body carries the shader's own #version, which cannot appear in the
    // middle of the generated one
    {
        const char *b = body.s ? body.s : "";
        const char *v = strstr(b, "#version");

        if (v)
        {
            const char *nl = strchr(v, '\n');

            if (!bufAddN(&out, b, (size_t)(v - b)) || !bufAdd(&out, nl ? nl + 1 : ""))
                goto done;
        }
        else if (!bufAdd(&out, b))
        {
            goto done;
        }
    }


    // ---- the vertex shader that draws the result ----
    if (!bufAdd(&pass, "#version 460\n\n"))
        goto done;

    if (!emitStructs(&sc, &pass))
        goto done;

    snprintf(line, sizeof(line),
        "layout(std430, binding = %d) readonly buffer MglGsOutB { MglGsOutV mglGsOut[]; };\n\n",
        MGL_GS_OUT_BINDING);

    if (!bufAdd(&pass, line))
        goto done;

    for (int i = 0, loc = 0; i < sc.out_count; i++)
    {
        snprintf(line, sizeof(line), "layout(location = %d) %s%sout %s %s;\n", loc,
                 sc.out[i].qualifier, sc.out[i].qualifier[0] ? " " : "",
                 sc.out[i].type, sc.out[i].name);

        if (!bufAdd(&pass, line))
            goto done;

        loc += locationsFor(sc.out[i].type);
    }

    if (!bufAdd(&pass, "\nvoid main()\n{\n  MglGsOutV v = mglGsOut[gl_VertexID];\n"))
        goto done;

    for (int i = 0; i < sc.out_count; i++)
    {
        snprintf(line, sizeof(line), "  %s = v.%s;\n", sc.out[i].name, sc.out[i].name);

        if (!bufAdd(&pass, line))
            goto done;
    }

    // a slot the geometry shader never wrote is pushed outside the frustum,
    // where it covers nothing rather than drawing a stray primitive
    if (!bufAdd(&pass,
        "  gl_Position = v.mglValid > 0.5 ? v.mglPos : vec4(2.0, 2.0, 2.0, 1.0);\n"
        "  gl_PointSize = v.mglPointSize;\n}\n"))
        goto done;

    // ---- what the vertex shader has to be given to feed all this ----
    if (!emitStructs(&sc, &decl))
        goto done;

    snprintf(line, sizeof(line),
        "layout(std430, binding = %d) buffer MglGsInB { MglGsInV mglGsIn[]; };\n",
        MGL_GS_IN_BINDING);

    if (!bufAdd(&decl, line))
        goto done;

    if (!bufAdd(&decl, "void mglGsCapture()\n{\n  mglGsIn[gl_VertexID].mglPos = gl_Position;\n"
                       "  mglGsIn[gl_VertexID].mglPointSize = 1.0;\n"))
        goto done;

    for (int i = 0; i < sc.in_count; i++)
    {
        snprintf(line, sizeof(line), "  mglGsIn[gl_VertexID].%s = %s;\n",
                 sc.in[i].name, sc.in[i].name);

        if (!bufAdd(&decl, line))
            goto done;
    }

    if (!bufAdd(&decl, "}\n"))
        goto done;

    gi->compute_src = out.s;      out.s = NULL;
    gi->passthrough_src = pass.s; pass.s = NULL;
    gi->capture_decl = decl.s;    decl.s = NULL;
    ok = true;

done:
    free(stripped.s);
    free(body.s);
    free(out.s);
    free(pass.s);
    free(decl.s);

    if (!ok)
        mglFreeGeometryInfo(gi);

    return ok;
}

// The vertex shader keeps its own job and gains one more: writing what it
// produced where the geometry stage can read it.
// std430 storage blocks need GLSL 4.30, and plenty of vertex shaders are
// older than that. The rest of core GLSL is upward compatible, so raising the
// version is enough to let the capture block in.
static bool addRaisedVersion(Buf *out, const char *src, const char **body_start)
{
    const char *at = strstr(src, "#version");
    const char *nl;
    int version = 110;

    *body_start = src;

    if (at == NULL)
        return bufAdd(out, "#version 430 core\n");

    sscanf(at, "#version %d", &version);
    nl = strchr(at, '\n');

    if (nl == NULL)
        return false;

    // everything before the directive is whitespace or comments; keep it
    if (!bufAddN(out, src, (size_t)(at - src)))
        return false;

    *body_start = nl + 1;

    if (version >= 430)
        return bufAddN(out, at, (size_t)(nl - at) + 1);

    return bufAdd(out, "#version 430 core\n");
}

// The capture records a point size of one unless the shader sets its own.
static char *pointSizeIntoCapture(char *result, const char *src)
{
    static const char from[] = ".mglPointSize = 1.0;";
    static const char to[] = ".mglPointSize = gl_PointSize;";
    char *at;
    bool writes = false;

    // only an assignment counts; reading gl_in[n].gl_PointSize or declaring
    // it in the output block is not setting our own
    for (const char *p = src; (p = strstr(p, "gl_PointSize")) != NULL; p++)
    {
        if (p > src && p[-1] == '.')
            continue;

        const char *q = p + strlen("gl_PointSize");

        while (*q == ' ' || *q == '\t')
            q++;

        if ((q[0] == '=' && q[1] != '=') || (strchr("+-*/", q[0]) && q[0] && q[1] == '='))
            writes = true;
    }

    if (result == NULL || !writes || (at = strstr(result, from)) == NULL)
        return result;

    size_t head = (size_t)(at - result);
    char *out = (char *)malloc(strlen(result) + sizeof(to));

    if (out == NULL)
        return result;

    memcpy(out, result, head);
    strcpy(out + head, to);
    strcat(out, at + sizeof(from) - 1);
    free(result);

    return out;
}

char *mglAddGeometryCapture(const char *vs_src, const GeometryInfo *gi)
{
    Buf out = {0};
    const char *p = vs_src;
    const char *main_at = NULL;

    // find the entry point's definition and rename it
    for (size_t i = 0; vs_src[i]; i++)
    {
        if (wordAt(vs_src, i, "main"))
        {
            size_t k = skipSpace(vs_src, i + 4);

            if (vs_src[k] == '(')
            {
                main_at = vs_src + i;
                break;
            }
        }
    }

    if (main_at == NULL)
        return NULL;

    if (!addRaisedVersion(&out, vs_src, &p))
    {
        free(out.s);
        return NULL;
    }

    if (!bufAddN(&out, p, (size_t)(main_at - p)) ||
        !bufAdd(&out, "mglVsBody") ||
        !bufAdd(&out, main_at + 4))
    {
        free(out.s);
        return NULL;
    }

    if (!bufAdd(&out, "\n") || !bufAdd(&out, gi->capture_decl) ||
        !bufAdd(&out, "\nvoid main()\n{\n  mglVsBody();\n  mglGsCapture();\n}\n"))
    {
        free(out.s);
        return NULL;
    }

    return pointSizeIntoCapture(out.s, vs_src);
}

// "gl_in[ ... ].gl_X" or "gl_out[ ... ].gl_X" somewhere in the source
static bool accessesMember(const char *src, const char *array, const char *member)
{
    size_t n = strlen(array);

    for (const char *p = src; (p = strstr(p, array)) != NULL; p += n)
    {
        if ((p != src && identChar(p[-1])) || identChar(p[n]))
            continue;

        const char *q = p + n;

        while (*q == ' ' || *q == '\t' || *q == '\n') q++;

        if (*q != '[')
            continue;

        int depth = 0;

        for (; *q; q++)
        {
            if (*q == '[') depth++;
            else if (*q == ']' && --depth == 0) { q++; break; }
        }

        while (*q == ' ' || *q == '\t' || *q == '\n') q++;

        if (*q != '.')
            continue;

        q++;

        while (*q == ' ' || *q == '\t' || *q == '\n') q++;

        if (!strncmp(q, member, strlen(member)) && !identChar(q[strlen(member)]))
            return true;
    }

    return false;
}

// Metal lays the buffer between the control and evaluation stages out from
// each stage's own built-ins, so a gl_Position or gl_PointSize the control
// stage writes and the evaluation stage never reads puts every field after
// it in the wrong place. Give the evaluation shader a read of each one.
// Returns NULL when nothing needs adding.
char *mglTouchTessInputs(const char *tes_src, const char *writer_src, bool writer_is_control)
{
    bool pos = writer_is_control ? accessesMember(writer_src, "gl_out", "gl_Position")
                                 : strstr(writer_src, "gl_Position") != NULL;
    bool ps = writer_is_control ? accessesMember(writer_src, "gl_out", "gl_PointSize")
                                : strstr(writer_src, "gl_PointSize") != NULL;

    pos = pos && !accessesMember(tes_src, "gl_in", "gl_Position");
    ps = ps && !accessesMember(tes_src, "gl_in", "gl_PointSize");

    if (!pos && !ps)
        return NULL;

    // the body starts after main's opening brace
    const char *body = NULL;

    for (size_t i = 0; tes_src[i]; i++)
        if (wordAt(tes_src, i, "main") && tes_src[skipSpace(tes_src, i + 4)] == '(')
        {
            const char *b = strchr(tes_src + i, '{');

            if (b)
                body = b + 1;

            break;
        }

    if (body == NULL)
        return NULL;

    // the declaration goes in after any # lines at the top
    const char *decl_at = tes_src;

    for (;;)
    {
        while (*decl_at == ' ' || *decl_at == '\t' || *decl_at == '\r' || *decl_at == '\n')
            decl_at++;

        if (*decl_at != '#')
            break;

        while (*decl_at && *decl_at != '\n')
            decl_at++;
    }

    Buf out = {0};

    if (!bufAddN(&out, tes_src, (size_t)(decl_at - tes_src)) ||
        !bufAdd(&out, "float mglTouchIn;\n") ||
        !bufAddN(&out, decl_at, (size_t)(body - decl_at)) ||
        !bufAdd(&out, "\n  mglTouchIn = 0.0") ||
        (pos && !bufAdd(&out, " + gl_in[0].gl_Position.x")) ||
        (ps && !bufAdd(&out, " + gl_in[0].gl_PointSize")) ||
        !bufAdd(&out, ";\n") ||
        !bufAdd(&out, body))
    {
        free(out.s);
        return NULL;
    }

    return out.s;
}

// Copies src with every whole-word "from" swapped for "to".
static bool replaceWord(Buf *out, const char *src, const char *from, const char *to)
{
    size_t i = 0, copied = 0;

    while (src[i])
    {
        if (wordAt(src, i, from))
        {
            if (!bufAddN(out, src + copied, i - copied) || !bufAdd(out, to))
                return false;

            i += strlen(from);
            copied = i;
            continue;
        }

        i++;
    }

    return bufAdd(out, src + copied);
}


// Metal cannot tessellate isolines, but an isoline point set is the bottom
// rows of a quad grid. So the evaluation shader is built as a quad shader, and
// each point it is run for writes itself into a fixed slot of the geometry
// stage's input. Points off the isolines are skipped, and a point Metal runs
// twice just writes the same slot twice.
char *mglAddTessPointCapture(const char *tes_src, const GeometryInfo *gi)
{
    Buf out = {0}, layout = {0}, decl = {0};
    const char *src = tes_src;
    const char *main_at = NULL;
    bool isolines = false, point_mode = false;
    size_t i = 0, copied = 0;
    char *result = NULL;

    // drop every "layout(...) in;" and remember what it asked for
    while (src[i])
    {
        if (!wordAt(src, i, "layout"))
        {
            i++;
            continue;
        }

        size_t j = skipSpace(src, i + 6);

        if (src[j] != '(')
        {
            i++;
            continue;
        }

        size_t close = j;
        int depth = 0;

        for (; src[close]; close++)
        {
            if (src[close] == '(') depth++;
            else if (src[close] == ')' && --depth == 0) { close++; break; }
        }

        size_t after = skipSpace(src, close);

        if (!wordAt(src, after, "in") || src[skipSpace(src, after + 2)] != ';')
        {
            i = close;
            continue;
        }

        // each comma separated word, kept unless it is one this replaces
        for (size_t k = j + 1; k < close - 1;)
        {
            char word[64];

            k = skipSpace(src, k);

            if (!identChar(src[k]))
            {
                k++;
                continue;
            }

            k = readIdent(src, k, word, sizeof(word));

            if (!strcmp(word, "isolines"))
                isolines = true;
            else if (!strcmp(word, "point_mode"))
                point_mode = true;
            else if (!bufAdd(&layout, ", ") || !bufAdd(&layout, word))
                goto done;
        }

        if (!bufAddN(&out, src + copied, i - copied))
            goto done;

        i = skipSpace(src, after + 2) + 1;
        copied = i;
    }

    if (!isolines)
        goto done;

    if (!bufAdd(&out, src + copied))
        goto done;

    // the rest is the same as for a vertex shader, from the stripped copy
    src = out.s;
    out = (Buf){0};

    for (i = 0; src[i]; i++)
    {
        if (wordAt(src, i, "main") && src[skipSpace(src, i + 4)] == '(')
        {
            main_at = src + i;
            break;
        }
    }

    if (main_at == NULL)
        goto done_src;

    const char *p = src;

    if (!addRaisedVersion(&out, src, &p))
        goto done_src;

    if (!bufAddN(&out, p, (size_t)(main_at - p)) ||
        !bufAdd(&out, "mglVsBody") ||
        !bufAdd(&out, main_at + 4))
        goto done_src;

    if (!replaceWord(&decl, gi->capture_decl, "gl_VertexID", "mglCapIdx"))
        goto done_src;

    char line[1024];

    if (point_mode)
        snprintf(line, sizeof(line),
            "void main()\n{\n"
            "  mglVsBody();\n"
            "  int mglN0 = max(int(round(gl_TessLevelOuter[0])), 1);\n"
            "  int mglN1 = max(int(round(gl_TessLevelOuter[1])), 1);\n"
            "  int mglU = int(round(gl_TessCoord.x * float(mglN1)));\n"
            "  int mglV = int(round(gl_TessCoord.y * float(mglN0)));\n"
            "  if (mglV >= mglN0 || mglN0 > %d || mglN1 > %d) return;\n"
            "  mglCapIdx = gl_PrimitiveID * %d + mglV * (mglN1 + 1) + mglU;\n"
            "  mglGsCapture();\n}\n",
            MGL_TES_MAX_LEVEL, MGL_TES_MAX_LEVEL, MGL_TES_POINTS_PER_PATCH);
    else
        // a point ends the segment to its left and starts the one to its
        // right; each segment is two slots side by side
        snprintf(line, sizeof(line),
            "void main()\n{\n"
            "  mglVsBody();\n"
            "  int mglN0 = max(int(round(gl_TessLevelOuter[0])), 1);\n"
            "  int mglN1 = max(int(round(gl_TessLevelOuter[1])), 1);\n"
            "  int mglU = int(round(gl_TessCoord.x * float(mglN1)));\n"
            "  int mglV = int(round(gl_TessCoord.y * float(mglN0)));\n"
            "  if (mglV >= mglN0 || mglN0 > %d || mglN1 > %d) return;\n"
            "  int mglSeg = gl_PrimitiveID * %d + mglV * mglN1;\n"
            "  if (mglU > 0) { mglCapIdx = (mglSeg + mglU - 1) * 2 + 1; mglGsCapture(); }\n"
            "  if (mglU < mglN1) { mglCapIdx = (mglSeg + mglU) * 2; mglGsCapture(); }\n}\n",
            MGL_TES_MAX_LEVEL, MGL_TES_MAX_LEVEL, MGL_TES_SEGMENTS_PER_PATCH);

    // after the body, so it cannot land ahead of an #extension line
    if (!bufAdd(&out, "\nlayout(quads") || !bufAdd(&out, layout.s ? layout.s : "") ||
        !bufAdd(&out, ") in;\n\nint mglCapIdx;\n") || !bufAdd(&out, decl.s) ||
        !bufAdd(&out, "\n") || !bufAdd(&out, line))
        goto done_src;

    result = pointSizeIntoCapture(out.s, tes_src);
    out.s = NULL;

done_src:
    free((char *)src);
done:
    free(out.s);
    free(layout.s);
    free(decl.s);

    return result;
}

// Where component c of a value of this type sits, counting from the value.
// A std430 matrix is its columns, each padded like a vector of its rows.
static bool componentOffset(const char *type, int c, int *bytes)
{
    int cols = 0, rows = 0;

    if (!strncmp(type, "mat", 3) && type[3])
    {
        cols = type[3] - '0';
        rows = (type[4] == 'x') ? type[5] - '0' : cols;
    }

    if (cols)
    {
        int col_stride = rows == 2 ? 8 : 16;

        *bytes = (c / rows) * col_stride + (c % rows) * 4;
        return true;
    }

    if (!strncmp(type, "double", 6) || !strncmp(type, "dvec", 4) || !strncmp(type, "dmat", 4))
        return false;

    *bytes = c * 4;
    return true;
}

int mglGsGatherTable(const GeometryInfo *gi, const MglXfbItem *items, int count,
                     GLuint *table, int max_words)
{
    int words = 0;

    for (int i = 0; i < count; i++)
    {
        const MglXfbItem *it = &items[i];
        char base[64];
        int element = -1;
        const char *open = strchr(it->expr, '[');
        int offset = -1;
        char type[32] = "";

        if (it->kind == 'd')
            return -1;

        snprintf(base, sizeof(base), "%.*s", open ? (int)(open - it->expr) : (int)strlen(it->expr), it->expr);

        if (open)
            element = atoi(open + 1);

        if (!strcmp(base, "gl_Position"))
        {
            offset = 0;
            snprintf(type, sizeof(type), "vec4");
        }
        else if (!strcmp(base, "gl_PointSize"))
        {
            offset = 16;
            snprintf(type, sizeof(type), "float");
        }

        for (int o = 0; o < gi->out_count && offset < 0; o++)
            if (!strcmp(gi->out_names[o], base))
            {
                offset = gi->out_offsets[o];
                snprintf(type, sizeof(type), "%s", gi->out_types[o]);
            }

        if (offset < 0)
            return -1;

        for (int c = 0; c < it->components; c++)
        {
            int at;

            // "v[2]" is one component of a vector
            if (element >= 0 && it->components == 1 && !it->rows)
                at = element * 4;
            else if (!componentOffset(type, c, &at))
                return -1;

            if (words >= max_words)
                return -1;

            table[words * 3] = (GLuint)(offset + at);
            table[words * 3 + 1] = (GLuint)it->buffer;
            table[words * 3 + 2] = (GLuint)(it->offset / 4 + c);
            words++;
        }
    }

    return words;
}

// Which kind of isolines the evaluation shader asks for: 0 none, 1 point
// mode, 2 lines. Only the layout(...) in; declarations count.
int mglTesIsolineKind(const char *src)
{
    bool isolines = false, point_mode = false;

    for (size_t i = 0; src[i]; i++)
    {
        if (!wordAt(src, i, "layout"))
            continue;

        size_t j = skipSpace(src, i + 6), close = j;
        int depth = 0;

        if (src[j] != '(')
            continue;

        for (; src[close]; close++)
        {
            if (src[close] == '(') depth++;
            else if (src[close] == ')' && --depth == 0) { close++; break; }
        }

        size_t after = skipSpace(src, close);

        if (!wordAt(src, after, "in") || src[skipSpace(src, after + 2)] != ';')
            continue;

        for (size_t k = j + 1; k + 1 < close;)
        {
            char word[64];

            if (!identChar(src[k]))
            {
                k++;
                continue;
            }

            k = readIdent(src, k, word, sizeof(word));

            if (!strcmp(word, "isolines"))
                isolines = true;
            else if (!strcmp(word, "point_mode"))
                point_mode = true;
        }
    }

    return !isolines ? 0 : point_mode ? 1 : 2;
}

void mglTesLayout(const char *src, int *domain, int *spacing, bool *cw, bool *points)
{
    *domain = -1;
    *spacing = 0;
    *cw = false;
    *points = false;

    for (size_t i = 0; src[i]; i++)
    {
        if (!wordAt(src, i, "layout"))
            continue;

        size_t j = skipSpace(src, i + 6), close = j;
        int depth = 0;

        if (src[j] != '(')
            continue;

        for (; src[close]; close++)
        {
            if (src[close] == '(') depth++;
            else if (src[close] == ')' && --depth == 0) { close++; break; }
        }

        size_t after = skipSpace(src, close);

        if (!wordAt(src, after, "in") || src[skipSpace(src, after + 2)] != ';')
            continue;

        for (size_t k = j + 1; k + 1 < close;)
        {
            char word[64];

            if (!identChar(src[k]))
            {
                k++;
                continue;
            }

            k = readIdent(src, k, word, sizeof(word));

            if (!strcmp(word, "triangles"))                    *domain = 0;
            else if (!strcmp(word, "quads"))                   *domain = 1;
            else if (!strcmp(word, "isolines"))                *domain = 2;
            else if (!strcmp(word, "fractional_even_spacing")) *spacing = 1;
            else if (!strcmp(word, "fractional_odd_spacing"))  *spacing = 2;
            else if (!strcmp(word, "equal_spacing"))           *spacing = 0;
            else if (!strcmp(word, "cw"))                      *cw = true;
            else if (!strcmp(word, "ccw"))                     *cw = false;
            else if (!strcmp(word, "point_mode"))              *points = true;
        }
    }
}

// Triangles and quads, where the vertices come from the CPU tessellator. The
// evaluation shader is run by Metal over a 65 x 65 quad grid; each grid point
// stands for one vertex of the real patch, reads its coordinate and levels
// from the buffer the CPU filled, and writes its outputs into its own slot.
char *mglAddTessGeneralCapture(const char *tes_src, const GeometryInfo *gi)
{
    Buf stripped = {0}, body = {0}, t1 = {0}, t2 = {0}, out = {0}, decl = {0};
    const char *src = tes_src;
    const char *main_at = NULL;
    size_t i = 0, copied = 0;
    char *result = NULL;

    // drop every "layout(...) in;"; the grid gets one of its own below
    while (src[i])
    {
        if (!wordAt(src, i, "layout"))
        {
            i++;
            continue;
        }

        size_t j = skipSpace(src, i + 6);

        if (src[j] != '(')
        {
            i++;
            continue;
        }

        size_t close = j;
        int depth = 0;

        for (; src[close]; close++)
        {
            if (src[close] == '(') depth++;
            else if (src[close] == ')' && --depth == 0) { close++; break; }
        }

        size_t after = skipSpace(src, close);

        if (!wordAt(src, after, "in") || src[skipSpace(src, after + 2)] != ';')
        {
            i = close;
            continue;
        }

        if (!bufAddN(&stripped, src + copied, i - copied))
            goto done;

        i = skipSpace(src, after + 2) + 1;
        copied = i;
    }

    if (!bufAdd(&stripped, src + copied))
        goto done;

    // the shader reads its coordinate and levels from variables this sets
    Buf t3 = {0};

    if (!replaceWord(&t1, stripped.s, "gl_TessCoord", "mglTC") ||
        !replaceWord(&t2, t1.s, "gl_TessLevelOuter", "mglTLO") ||
        !replaceWord(&t3, t2.s, "gl_TessLevelInner", "mglTLI") ||
        !replaceWord(&body, t3.s, "gl_PatchVerticesIn", "mglPVI"))
    {
        free(t3.s);
        goto done;
    }

    free(t3.s);

    for (i = 0; body.s[i]; i++)
    {
        if (wordAt(body.s, i, "main") && body.s[skipSpace(body.s, i + 4)] == '(')
        {
            main_at = body.s + i;
            break;
        }
    }

    if (main_at == NULL)
        goto done;

    const char *p = body.s;

    if (!addRaisedVersion(&out, body.s, &p))
        goto done;

    // the body reads these, so they go in ahead of it, after any #extension
    const char *decls_at = p;

    for (;;)
    {
        while (*decls_at == ' ' || *decls_at == '\t' || *decls_at == '\r' || *decls_at == '\n')
            decls_at++;

        if (*decls_at != '#')
            break;

        while (*decls_at && *decls_at != '\n')
            decls_at++;
    }

    if (!bufAddN(&out, p, (size_t)(decls_at - p)) ||
        !bufAdd(&out, "vec3 mglTC;\nfloat mglTLO[4];\nfloat mglTLI[2];\nint mglPVI;\n") ||
        !bufAddN(&out, decls_at, (size_t)(main_at - decls_at)) ||
        !bufAdd(&out, "mglVsBody") ||
        !bufAdd(&out, main_at + 4))
        goto done;

    if (!replaceWord(&decl, gi->capture_decl, "gl_VertexID", "mglCapIdx"))
        goto done;

    char line[1600];

    snprintf(line, sizeof(line),
        "\nlayout(quads, equal_spacing, ccw) in;\n\n"
        "int mglCapIdx;\n"
        "layout(std430, binding = %d) buffer MglTessGenB { vec4 mglTG[]; };\n",
        MGL_TES_GEN_BINDING);

    // the capture declarations can be long, so they go in on their own
    if (!bufAdd(&out, line) || !bufAdd(&out, decl.s) || !bufAdd(&out, "\n"))
        goto done;

    snprintf(line, sizeof(line),
        "void main()\n{\n"
        "  int mglU = int(round(gl_TessCoord.x * %d.0));\n"
        "  int mglV = int(round(gl_TessCoord.y * %d.0));\n"
        "  int mglIdx = mglV * %d + mglU;\n"
        "  int mglAt = gl_PrimitiveID * %d;\n"
        "  vec4 mglHead = mglTG[mglAt];\n"
        "  if (mglIdx >= int(mglHead.x)) return;\n"
        "  vec4 mglOut = mglTG[mglAt + 1];\n"
        "  mglTLO[0] = mglOut.x; mglTLO[1] = mglOut.y; mglTLO[2] = mglOut.z; mglTLO[3] = mglOut.w;\n"
        "  mglTLI[0] = mglHead.y; mglTLI[1] = mglHead.z;\n"
        "  mglPVI = int(mglHead.w);\n"
        "  mglTC = mglTG[mglAt + 2 + mglIdx].xyz;\n"
        "  mglVsBody();\n"
        "  mglCapIdx = gl_PrimitiveID * %d + mglIdx;\n"
        "  mglGsCapture();\n}\n",
        MGL_TES_MAX_LEVEL, MGL_TES_MAX_LEVEL, MGL_TES_MAX_LEVEL + 1,
        MGL_TES_GEN_STRIDE, MGL_TES_GEN_VERTS);

    if (!bufAdd(&out, line))
        goto done;

    result = pointSizeIntoCapture(out.s, tes_src);
    out.s = NULL;

done:
    free(stripped.s);
    free(body.s);
    free(t1.s);
    free(t2.s);
    free(out.s);
    free(decl.s);

    return result;
}

// A geometry shader that passes each point, or each line segment, straight
// through, so isolines with no geometry shader of their own can use the same
// path.
char *mglPassThroughGeometry(const char *tes_src, int input)
{
    Buf out = {0};
    GsVarying outs[MAX_GS_VARYINGS];
    int count = 0;
    char line[256];

    for (size_t i = 0; tes_src[i]; i++)
    {
        GsVarying v;
        bool is_varying = false;

        if ((i && identChar(tes_src[i - 1])) || !identChar(tes_src[i]))
            continue;

        if (readVarying(tes_src, i, true, &v, &is_varying) && is_varying && count < MAX_GS_VARYINGS)
        {
            // "flat out int x" reads as a varying from "flat" and again from "out"
            bool seen = false;

            for (int k = 0; k < count; k++)
                if (!strcmp(outs[k].name, v.name))
                    seen = true;

            if (!seen)
                outs[count++] = v;
        }
    }

    static const char *heads[3] = {
        "#version 430 core\nlayout(points) in;\nlayout(points, max_vertices = 1) out;\n",
        "#version 430 core\nlayout(lines) in;\nlayout(line_strip, max_vertices = 2) out;\n",
        "#version 430 core\nlayout(triangles) in;\nlayout(triangle_strip, max_vertices = 3) out;\n",
    };

    if (input < 0 || input > 2 || !bufAdd(&out, heads[input]))
        goto fail;

    for (int i = 0; i < count; i++)
    {
        snprintf(line, sizeof(line), "%s%sin %s %s[];\n%s%sout %s %s;\n",
                 outs[i].qualifier, outs[i].qualifier[0] ? " " : "", outs[i].type, outs[i].name,
                 outs[i].qualifier, outs[i].qualifier[0] ? " " : "", outs[i].type, outs[i].name);

        if (!bufAdd(&out, line))
            goto fail;
    }

    if (!bufAdd(&out, "void main()\n{\n"))
        goto fail;

    for (int v = 0; v <= input; v++)
    {
        snprintf(line, sizeof(line), "  gl_Position = gl_in[%d].gl_Position;\n", v);

        if (!bufAdd(&out, line))
            goto fail;

        for (int i = 0; i < count; i++)
        {
            snprintf(line, sizeof(line), "  %s = %s[%d];\n", outs[i].name, outs[i].name, v);

            if (!bufAdd(&out, line))
                goto fail;
        }

        if (!bufAdd(&out, "  EmitVertex();\n"))
            goto fail;
    }

    if (!bufAdd(&out, "}\n"))
        goto fail;

    return out.s;

fail:
    free(out.s);
    return NULL;
}
