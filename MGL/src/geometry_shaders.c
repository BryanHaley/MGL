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

        // the shader's own entry point becomes a function the generated one calls
        if (wordAt(src, i, "main"))
        {
            if (!bufAddN(out, src + copied, i - copied) || !bufAdd(out, "mglGsBody"))
                return false;

            i += 4;
            copied = i;
            continue;
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
        "void mglEndPrimitive()\n{\n  mglStripLen = 0;\n  mglStripFlip = false;\n}\n\n"))
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

    if (!bufAdd(&out,
        "\nvoid main()\n{\n"
        "  mglGsPrims = mglGsPrimsU;\n"
        "  mglGsIndexed = mglGsIndexedU;\n"
        "  mglGsFirst = mglGsFirstU;\n"
        "  mglGsStride = mglGsStrideU;\n"
        "  int id = int(gl_GlobalInvocationID.x);\n"
        "  mglPrimitiveID = id / mglGsInvocations;\n"
        "  mglInvocationID = id - mglPrimitiveID * mglGsInvocations;\n"
        "  if (mglPrimitiveID >= mglGsPrims) return;\n"
        "  mglBase = id * mglGsCap;\n"
        "  mglWritten = 0;\n  mglStripLen = 0;\n  mglStripFlip = false;\n"
        "  mglGsBody();\n}\n"))
        goto done;

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

    for (int i = 0; i < sc.out_count; i++)
    {
        snprintf(line, sizeof(line), "layout(location = %d) %s%sout %s %s;\n", i,
                 sc.out[i].qualifier, sc.out[i].qualifier[0] ? " " : "",
                 sc.out[i].type, sc.out[i].name);

        if (!bufAdd(&pass, line))
            goto done;
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

    return out.s;
}
