/*
 * mesh_nv.c
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
 * GL_NV_mesh_shader, as a rewrite of the shader into GL_EXT_mesh_shader.
 *
 * NV sets its counts by writing gl_TaskCountNV and gl_PrimitiveCountNV from
 * any invocation; EXT calls EmitMeshTasksEXT and SetMeshOutputsEXT once. The
 * counts go into shared variables and a new main makes the EXT call after the
 * shader's own main returns. The task block becomes a payload struct, and a
 * per-primitive member of a vertex block moves to a block of its own.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"

typedef struct {
    char *s;
    size_t n, cap;
} Str;

static void put(Str *b, const char *p, size_t n)
{
    if (b->n + n + 1 > b->cap)
    {
        size_t cap = (b->cap ? b->cap * 2 : 4096);

        while (cap < b->n + n + 1)
            cap *= 2;
        b->s = realloc(b->s, cap);
        b->cap = cap;
    }
    memcpy(b->s + b->n, p, n);
    b->n += n;
    b->s[b->n] = 0;
}

static void puts_(Str *b, const char *p)
{
    put(b, p, strlen(p));
}

static bool identChar(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static bool tokenAt(const char *src, const char *p, const char *tok)
{
    size_t n = strlen(tok);

    return !strncmp(p, tok, n) && (p == src || !identChar(p[-1])) && !identChar(p[n]);
}

static const char *skipSpace(const char *p)
{
    while (isspace((unsigned char)*p))
        p++;
    return p;
}

static const char *readIdent(const char *p, char *out, size_t cap)
{
    size_t n = 0;

    while (identChar(*p))
    {
        if (n + 1 < cap)
            out[n++] = *p;
        p++;
    }
    out[n] = 0;
    return p;
}

// matching close for the bracket at p
static const char *matchClose(const char *p)
{
    char open = *p, close = open == '{' ? '}' : open == '[' ? ']' : ')';
    int depth = 0;

    for (; *p; p++)
    {
        if (*p == open)
            depth++;
        else if (*p == close && --depth == 0)
            return p;
    }
    return NULL;
}

// every whole-word "from" becomes "to"
static char *replaceTokens(const char *src, const char *const (*pairs)[2], int count)
{
    Str b = { 0 };

    for (const char *p = src; *p; )
    {
        int k;

        for (k = 0; k < count; k++)
            if (tokenAt(src, p, pairs[k][0]))
                break;

        if (k < count)
        {
            puts_(&b, pairs[k][1]);
            p += strlen(pairs[k][0]);
        }
        else
            put(&b, p++, 1);
    }
    return b.s ? b.s : strdup(src);
}

// taskNV out Name { ... } inst;  ->  struct Name_mglTask { ... }; taskPayloadSharedEXT Name_mglTask inst;
static char *rewriteTaskBlocks(const char *src)
{
    Str b = { 0 };
    const char *p = src;

    for (;;)
    {
        const char *t = strstr(p, "taskNV");
        char dir[8], name[128];
        const char *q, *open, *close, *semi;

        while (t && !tokenAt(src, t, "taskNV"))
            t = strstr(t + 6, "taskNV");

        if (t == NULL)
            break;

        q = readIdent(skipSpace(t + 6), dir, sizeof dir);
        q = readIdent(skipSpace(q), name, sizeof name);
        open = skipSpace(q);

        if ((strcmp(dir, "in") && strcmp(dir, "out")) || !name[0] || *open != '{' ||
            (close = matchClose(open)) == NULL || (semi = strchr(close, ';')) == NULL)
        {
            put(&b, p, (size_t)(t + 6 - p));
            p = t + 6;
            continue;
        }

        // the block's name may also be the output block's
        put(&b, p, (size_t)(t - p));
        puts_(&b, "struct ");
        puts_(&b, name);
        puts_(&b, "_mglTask");
        put(&b, open, (size_t)(close + 1 - open));
        puts_(&b, ";\ntaskPayloadSharedEXT ");
        puts_(&b, name);
        puts_(&b, "_mglTask");
        put(&b, close + 1, (size_t)(semi + 1 - (close + 1)));
        p = semi + 1;
    }

    puts_(&b, p);
    return b.s;
}

// locations one member takes, for the types a varying can have
static int memberLocations(const char *decl)
{
    char type[64];
    const char *p = skipSpace(decl);
    int n = 1, arr = 1;

    for (;;)
    {
        const char *q = readIdent(p, type, sizeof type);

        if (!strcmp(type, "flat") || !strcmp(type, "smooth") || !strcmp(type, "noperspective") ||
            !strcmp(type, "centroid") || !strcmp(type, "sample"))
            p = skipSpace(q);
        else
            break;
    }

    if (!strncmp(type, "dvec3", 5) || !strncmp(type, "dvec4", 5))
        n = 2;
    else if (!strncmp(type, "mat", 3) || !strncmp(type, "dmat", 4))
    {
        const char *d = type + (type[0] == 'd' ? 4 : 3);
        int cols = d[0] - '0', rows = d[1] == 'x' ? d[2] - '0' : cols;

        n = cols * (type[0] == 'd' && rows >= 3 ? 2 : 1);
    }

    for (const char *a = strchr(decl, '['); a; a = strchr(a + 1, '['))
        arr *= atoi(a + 1) > 0 ? atoi(a + 1) : 1;

    return n * arr;
}

static char *dropToken(const char *src, const char *tok)
{
    const char *const pair[1][2] = { { tok, "" } };

    return replaceTokens(src, pair, 1);
}

// out Name { a; perprimitiveEXT b; } inst[N];
//  -> out Name { layout(location=0) a; } inst[N];
//     perprimitiveEXT out Name_mglPrim { layout(location=1) b; } inst_mglPrim[];
// and every inst[i].b becomes inst_mglPrim[i].b
static char *splitPerPrimitiveBlocks(const char *src)
{
    char *cur = strdup(src);

    for (const char *from = cur; ; )
    {
        const char *o = strstr(from, "out");
        char name[128], inst[128], prim[32][64];
        const char *q, *open, *close, *semi;
        int nprim = 0, loc = 0;
        bool explicitLoc;

        while (o && !tokenAt(cur, o, "out"))
            o = strstr(o + 3, "out");
        if (o == NULL)
            break;

        q = readIdent(skipSpace(o + 3), name, sizeof name);
        open = skipSpace(q);

        if (!name[0] || *open != '{' || (close = matchClose(open)) == NULL ||
            (semi = strchr(close, ';')) == NULL)
        {
            from = o + 3;
            continue;
        }

        readIdent(skipSpace(close + 1), inst, sizeof inst);

        // a block already per-primitive, or with no per-primitive member, stays
        {
            const char *pp = o;

            while (pp > cur && pp[-1] != ';' && pp[-1] != '}' && pp[-1] != '\n')
                pp--;
            if (strstr(pp, "perprimitiveEXT") && strstr(pp, "perprimitiveEXT") < o)
            {
                from = semi;
                continue;
            }
        }

        {
            char *body = strndup(open + 1, (size_t)(close - open - 1));
            bool any = strstr(body, "perprimitiveEXT") != NULL;

            explicitLoc = strstr(body, "layout") != NULL;
            free(body);

            if (!any || !inst[0])
            {
                from = semi;
                continue;
            }
        }

        Str vb = { 0 }, pb = { 0 }, out = { 0 };

        // a layout(location = N) just before "out" sets the first location
        {
            const char *l = o;

            while (l > cur && l[-1] != ';' && l[-1] != '}')
                l--;
            const char *lp = strstr(l, "location");

            if (lp && lp < o)
            {
                lp = strchr(lp, '=');
                if (lp)
                    loc = atoi(lp + 1);
            }
        }

        for (const char *m = open + 1; m < close; )
        {
            const char *end = memchr(m, ';', (size_t)(close - m));
            char *decl, *clean, mname[64] = "";
            bool isPrim;
            char buf[48];

            if (end == NULL)
                break;

            decl = strndup(m, (size_t)(end - m));
            m = end + 1;

            if (*skipSpace(decl) == 0)
            {
                free(decl);
                continue;
            }

            isPrim = strstr(decl, "perprimitiveEXT") != NULL;
            clean = isPrim ? dropToken(decl, "perprimitiveEXT") : strdup(decl);

            // the member's name is the last identifier before any [
            {
                const char *e = strchr(clean, '[');
                const char *s = e ? e : clean + strlen(clean);

                while (s > clean && !identChar(s[-1]))
                    s--;
                const char *st = s;
                while (st > clean && identChar(st[-1]))
                    st--;
                snprintf(mname, sizeof mname, "%.*s", (int)(s - st), st);
            }

            Str *dst = isPrim ? &pb : &vb;

            if (!explicitLoc)
            {
                snprintf(buf, sizeof buf, " layout(location = %d)", loc);
                puts_(dst, buf);
            }
            puts_(dst, clean);
            puts_(dst, ";");
            loc += memberLocations(clean);

            if (isPrim && nprim < 32)
                snprintf(prim[nprim++], sizeof prim[0], "%s", mname);

            free(decl);
            free(clean);
        }

        // the block, split
        put(&out, cur, (size_t)(o - cur));
        puts_(&out, "out ");
        puts_(&out, name);
        puts_(&out, " {");
        if (vb.s)
            puts_(&out, vb.s);
        puts_(&out, " }");
        put(&out, close + 1, (size_t)(semi + 1 - (close + 1)));
        puts_(&out, "\nperprimitiveEXT out ");
        puts_(&out, name);
        puts_(&out, "_mglPrim {");
        puts_(&out, pb.s);
        puts_(&out, " } ");
        puts_(&out, inst);
        puts_(&out, "_mglPrim[];");

        size_t resume = out.n;

        // inst[...] . member  ->  inst_mglPrim[...] . member
        for (const char *p = semi + 1; *p; )
        {
            if (tokenAt(cur, p, inst))
            {
                const char *br = skipSpace(p + strlen(inst));
                const char *cb = *br == '[' ? matchClose(br) : NULL;
                const char *dot = cb ? skipSpace(cb + 1) : NULL;
                char member[64] = "";
                bool moved = false;

                if (dot && *dot == '.')
                    readIdent(skipSpace(dot + 1), member, sizeof member);

                for (int k = 0; k < nprim && member[0]; k++)
                    if (!strcmp(member, prim[k]))
                        moved = true;

                puts_(&out, inst);
                if (moved)
                    puts_(&out, "_mglPrim");
                p += strlen(inst);
                continue;
            }
            put(&out, p++, 1);
        }

        free(vb.s);
        free(pb.s);
        free(cur);
        cur = out.s;
        from = cur + resume;
    }

    return cur;
}

// the text of name = <expr> inside a layout(...)
static bool layoutValue(const char *src, const char *name, char *out, size_t cap)
{
    const char *p = strstr(src, name);
    size_t n = 0;
    int depth = 0;

    while (p && !tokenAt(src, p, name))
        p = strstr(p + 1, name);
    if (p == NULL || (p = strchr(p, '=')) == NULL)
        return false;

    for (p++; *p; p++)
    {
        if (*p == '(')
            depth++;
        else if (*p == ')' && depth-- == 0)
            break;
        else if (*p == ',' && depth == 0)
            break;
        if (n + 1 < cap)
            out[n++] = *p;
    }
    out[n] = 0;
    return n > 0;
}

// index count per primitive, from layout(points|lines|triangles, ...) out
static int indicesPerPrimitive(const char *src)
{
    const char *p = strstr(src, "max_primitives");

    if (p == NULL)
        return 3;

    while (p > src && *p != '(')
        p--;

    for (const char *q = p; *q && *q != ')'; q++)
    {
        if (tokenAt(src, q, "points"))
            return 1;
        if (tokenAt(src, q, "lines"))
            return 2;
    }
    return 3;
}

// main(...) becomes mglNvMain; returns where its definition starts
static char *renameMain(char *s, size_t *defStart)
{
    for (char *m = strstr(s, "main"); m; m = strstr(m + 4, "main"))
    {
        if (!tokenAt(s, m, "main") || *skipSpace(m + 4) != '(')
            continue;

        char *v = m;

        while (v > s && isspace((unsigned char)v[-1]))
            v--;
        if (v - s >= 4 && !strncmp(v - 4, "void", 4))
            v -= 4;
        *defStart = (size_t)(v - s);

        Str b = { 0 };

        put(&b, s, (size_t)(m - s));
        puts_(&b, "mglNvMain");
        puts_(&b, m + 4);
        free(s);
        return b.s;
    }
    return NULL;
}

// put decl after the #version and #extension lines
static char *insertAfterDirectives(char *s, const char *decl)
{
    const char *ins = s;
    Str b = { 0 };

    for (const char *line = s; *line; )
    {
        const char *nl = strchr(line, '\n');
        const char *t = skipSpace(line);

        if (!strncmp(t, "#version", 8) || !strncmp(t, "#extension", 10))
            ins = nl ? nl + 1 : line + strlen(line);
        if (nl == NULL)
            break;
        line = nl + 1;
    }

    put(&b, s, (size_t)(ins - s));
    puts_(&b, decl);
    puts_(&b, ins);
    free(s);
    return b.s;
}

char *mglRewriteNvMesh(const char *src, bool task)
{
    const char *first = task ? MGL_NV_TASK_FIRST_NAME : MGL_NV_MESH_FIRST_NAME;
    char group[160], global[200];
    char *s, *t;
    size_t def = 0;

    if (strstr(src, "GL_NV_mesh_shader") == NULL)
        return NULL;

    snprintf(group, sizeof group, "(gl_WorkGroupID + uvec3(%s, 0u, 0u))", first);
    snprintf(global, sizeof global, "(gl_GlobalInvocationID + uvec3(%s * gl_WorkGroupSize.x, 0u, 0u))", first);

    const char *const pairs[][2] = {
        { "GL_NV_mesh_shader", "GL_EXT_mesh_shader" },
        { "perprimitiveNV", "perprimitiveEXT" },
        { "gl_MeshVerticesNV", "gl_MeshVerticesEXT" },
        { "gl_MeshPrimitivesNV", "gl_MeshPrimitivesEXT" },
        { "gl_TaskCountNV", "mglNvTaskCount" },
        { "gl_PrimitiveCountNV", "mglNvPrimCount" },
        { "gl_PrimitiveIndicesNV", "mglNvPrimIndices" },
        { "writePackedPrimitiveIndices4x8NV", "mglNvWritePacked4x8" },
        { "gl_WorkGroupID", group },
        { "gl_GlobalInvocationID", global },
    };

    s = replaceTokens(src, pairs, (int)(sizeof pairs / sizeof pairs[0]));
    t = rewriteTaskBlocks(s);
    free(s);
    s = t;

    if (!task)
    {
        t = splitPerPrimitiveBlocks(s);
        free(s);
        s = t;
    }

    s = renameMain(s, &def);
    if (s == NULL)
        return NULL;

    if (task)
    {
        char decl[256];

        snprintf(decl, sizeof decl, "uniform uint %s;\nshared uint mglNvTaskCount;\n", first);
        s = insertAfterDirectives(s, decl);

        Str b = { 0 };

        puts_(&b, s);
        puts_(&b,
              "\nvoid main()\n{\n"
              "    if (gl_LocalInvocationIndex == 0u) mglNvTaskCount = 0u;\n"
              "    barrier();\n"
              "    mglNvMain();\n"
              "    memoryBarrierShared();\n"
              "    barrier();\n"
              "    EmitMeshTasksEXT(mglNvTaskCount, 1u, 1u);\n"
              "}\n");
        free(s);
        return b.s;
    }

    // Indices are kept in a plain array and copied out a whole primitive at a
    // time: neighbouring invocations write parts of one uvec3, and Metal
    // stores a vector component as a write of the whole vector.
    char verts[128], prims[128], decl[512], helpers[512], tail[1024];
    int k = indicesPerPrimitive(s);
    const char *copy =
        k == 1 ? "gl_PrimitivePointIndicesEXT[i] = mglNvPrimIndices[i];" :
        k == 2 ? "gl_PrimitiveLineIndicesEXT[i] = uvec2(mglNvPrimIndices[2u * i], mglNvPrimIndices[2u * i + 1u]);" :
                 "gl_PrimitiveTriangleIndicesEXT[i] = uvec3(mglNvPrimIndices[3u * i], "
                 "mglNvPrimIndices[3u * i + 1u], mglNvPrimIndices[3u * i + 2u]);";

    if (!layoutValue(s, "max_vertices", verts, sizeof verts) ||
        !layoutValue(s, "max_primitives", prims, sizeof prims))
    {
        free(s);
        return NULL;
    }

    snprintf(decl, sizeof decl,
             "uniform uint %s;\nshared uint mglNvPrimCount;\n"
             "const uint mglNvMaxVerts = uint(%s);\nconst uint mglNvMaxPrims = uint(%s);\n"
             "shared uint mglNvPrimIndices[mglNvMaxPrims * %du];\n",
             first, verts, prims, k);

    snprintf(helpers, sizeof helpers,
             "void mglNvWritePacked4x8(uint i, uint p)\n{\n"
             "    for (uint j = 0u; j < 4u; j++)\n"
             "        if (i + j < mglNvMaxPrims * %du)\n"
             "            mglNvPrimIndices[i + j] = (p >> (8u * j)) & 255u;\n"
             "}\n",
             k);

    snprintf(tail, sizeof tail,
             "\nvoid main()\n{\n"
             "    if (gl_LocalInvocationIndex == 0u) mglNvPrimCount = 0u;\n"
             "    barrier();\n"
             "    mglNvMain();\n"
             "    memoryBarrierShared();\n"
             "    barrier();\n"
             "    uint mglNvCount = min(mglNvPrimCount, mglNvMaxPrims);\n"
             "    SetMeshOutputsEXT(mglNvMaxVerts, mglNvCount);\n"
             "    for (uint i = gl_LocalInvocationIndex; i < mglNvCount; i += gl_WorkGroupSize.x)\n"
             "        %s\n"
             "}\n",
             copy);

    // the helpers go just before the body, after the output layout
    {
        Str b = { 0 };

        put(&b, s, def);
        puts_(&b, helpers);
        puts_(&b, s + def);
        puts_(&b, tail);
        free(s);
        s = b.s;
    }

    return insertAfterDirectives(s, decl);
}
