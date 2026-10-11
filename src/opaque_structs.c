/*
 * opaque_structs.c
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
 * GL lets a uniform struct hold samplers. SPIR-V for Metal cannot carry a
 * texture inside a constant struct, so each sampler such a uniform holds is
 * moved out into a uniform of its own, and the struct the uniform keeps is a
 * copy without them. "uniform S s[2];" with a sampler "c" in S becomes a
 * uniform of the copied struct plus "uniform sampler2D mglS_s_c[2];", and
 * "s[1].c" in the code becomes "mglS_s_c[1]".
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"

#define MAX_STRUCTS 32
#define MAX_MEMBERS 32
#define MAX_DIMS    4

typedef struct {
    char type[64];
    char name[64];
    char quals[64];
    int  dims[MAX_DIMS];
    int  dim_count;
} Member;

typedef struct {
    char   name[64];
    Member m[MAX_MEMBERS];
    int    count;
    bool   opaque;      // holds a sampler or image, itself or in a member
} Struct;

typedef struct {
    char  *s;
    size_t len, cap;
} Buf;

static bool add(Buf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap)
    {
        size_t cap = b->cap ? b->cap * 2 : 1024;

        while (cap < b->len + n + 1)
            cap *= 2;

        char *g = realloc(b->s, cap);

        if (g == NULL)
            return false;

        b->s = g;
        b->cap = cap;
    }

    memcpy(b->s + b->len, s, n);
    b->len += n;
    b->s[b->len] = 0;

    return true;
}

static bool adds(Buf *b, const char *s)
{
    return add(b, s, strlen(s));
}

static bool ident(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static bool opaqueType(const char *t)
{
    static const char *pre[] = { "sampler", "isampler", "usampler", "image", "iimage", "uimage", NULL };

    for (int i = 0; pre[i]; i++)
        if (!strncmp(t, pre[i], strlen(pre[i])) && isupper((unsigned char)t[strlen(pre[i])]) )
            return true;

    return !strncmp(t, "sampler", 7) && isdigit((unsigned char)t[7]);
}

static Struct *findStruct(Struct *st, int n, const char *name)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(st[i].name, name))
            return &st[i];

    return NULL;
}

// "[2][3]" after a name, read into dims; returns how far it went
static size_t readDims(const char *p, int *dims, int *count)
{
    const char *q = p;

    for (;;)
    {
        while (*q == ' ' || *q == '\t')
            q++;

        if (*q != '[' || *count >= MAX_DIMS)
            break;

        dims[(*count)++] = atoi(q + 1);

        while (*q && *q != ']')
            q++;

        if (*q == ']')
            q++;
    }

    return (size_t)(q - p);
}

// The members of one struct body, "type name[N]; ..." -- one name a line
static int readMembers(const char *body, size_t len, Member *m)
{
    int n = 0;
    size_t i = 0;

    while (i < len && n < MAX_MEMBERS)
    {
        size_t end = i;

        while (end < len && body[end] != ';')
            end++;

        // the words of the statement: qualifiers, then the type, then the name
        char words[8][64];
        int nw = 0;
        size_t k = i;
        size_t name_end = 0;

        while (k < end && nw < 8)
        {
            while (k < end && !ident(body[k]))
                k++;

            size_t s = k;

            while (k < end && ident(body[k]))
                k++;

            if (k > s)
            {
                snprintf(words[nw++], 64, "%.*s", (int)(k - s), body + s);
                name_end = k;
            }

            // stop at the first bracket: what follows is an array size
            size_t b = k;

            while (b < end && (body[b] == ' ' || body[b] == '\t'))
                b++;

            if (b < end && body[b] == '[')
                break;
        }

        if (nw >= 2)
        {
            Member *mm = &m[n++];

            memset(mm, 0, sizeof(*mm));
            snprintf(mm->name, sizeof(mm->name), "%s", words[nw - 1]);
            snprintf(mm->type, sizeof(mm->type), "%s", words[nw - 2]);

            for (int w = 0; w < nw - 2; w++)
            {
                strncat(mm->quals, words[w], sizeof(mm->quals) - strlen(mm->quals) - 2);
                strcat(mm->quals, " ");
            }

            readDims(body + name_end, mm->dims, &mm->dim_count);
        }

        i = end + 1;
    }

    return n;
}

static void markOpaque(Struct *st, int n)
{
    bool changed = true;

    while (changed)
    {
        changed = false;

        for (int i = 0; i < n; i++)
        {
            if (st[i].opaque)
                continue;

            for (int k = 0; k < st[i].count; k++)
            {
                Struct *inner = findStruct(st, n, st[i].m[k].type);

                if (opaqueType(st[i].m[k].type) || (inner && inner->opaque))
                {
                    st[i].opaque = true;
                    changed = true;
                    break;
                }
            }
        }
    }
}

// the sampler uniforms a struct uniform turns into, declared after it
static bool declareLeaves(Buf *out, Struct *st, int ns, Struct *s, const char *path,
                          const int *dims, int dim_count)
{
    for (int k = 0; k < s->count; k++)
    {
        Member *m = &s->m[k];
        Struct *inner = findStruct(st, ns, m->type);
        char sub[256];
        int d[MAX_DIMS * 4];
        int dc = dim_count;

        memcpy(d, dims, sizeof(int) * (size_t)dim_count);

        for (int j = 0; j < m->dim_count && dc < MAX_DIMS * 4; j++)
            d[dc++] = m->dims[j];

        snprintf(sub, sizeof(sub), "%s_%s", path, m->name);

        if (opaqueType(m->type))
        {
            char line[512];
            int at = snprintf(line, sizeof(line), "uniform %s%s %s", m->quals, m->type, sub);

            for (int j = 0; j < dc && at < (int)sizeof(line) - 16; j++)
                at += snprintf(line + at, sizeof(line) - (size_t)at, "[%d]", d[j]);

            snprintf(line + at, sizeof(line) - (size_t)at, ";\n");

            if (!adds(out, line))
                return false;
        }
        else if (inner && inner->opaque)
        {
            if (!declareLeaves(out, st, ns, inner, sub, d, dc))
                return false;
        }
    }

    return true;
}

// the copy of a struct without its samplers, and of the structs it holds
static bool declareCopy(Buf *out, Struct *s, Struct *st, int ns)
{
    char line[512];
    int kept = 0;

    snprintf(line, sizeof(line), "struct %s_mglNoOpaque {\n", s->name);

    if (!adds(out, line))
        return false;

    for (int k = 0; k < s->count; k++)
    {
        Member *m = &s->m[k];
        Struct *inner = findStruct(st, ns, m->type);
        int at;

        if (opaqueType(m->type))
            continue;

        at = snprintf(line, sizeof(line), "    %s%s%s %s", m->quals, m->type,
                      inner && inner->opaque ? "_mglNoOpaque" : "", m->name);

        for (int j = 0; j < m->dim_count; j++)
            at += snprintf(line + at, sizeof(line) - (size_t)at, "[%d]", m->dims[j]);

        snprintf(line + at, sizeof(line) - (size_t)at, ";\n");

        if (!adds(out, line))
            return false;

        kept++;
    }

    // GLSL has no empty struct
    if (kept == 0 && !adds(out, "    float mglUnused;\n"))
        return false;

    return adds(out, "};\n");
}

typedef struct {
    char name[64];
    Struct *type;
} Uniform;

// An access chain from a struct uniform: "s[1].b.a" or "s.c". When it reaches
// a sampler, write the flattened name with the indices it collected and
// return how far it went; otherwise 0, and it is left as it was.
static size_t rewriteChain(const char *p, Uniform *u, Struct *st, int ns, Buf *out)
{
    char path[256];
    char idx[512] = "";
    const char *q = p + strlen(u->name);
    Struct *cur = u->type;

    snprintf(path, sizeof(path), "mglS_%s", u->name);

    for (;;)
    {
        // indices on what came before
        while (*q == '[')
        {
            int depth = 0;
            const char *s = q;

            do
            {
                if (*q == '[') depth++;
                else if (*q == ']') depth--;
                q++;
            } while (*q && depth > 0);

            if (strlen(idx) + (size_t)(q - s) + 1 >= sizeof(idx))
                return 0;

            strncat(idx, s, (size_t)(q - s));
        }

        if (*q != '.' || cur == NULL)
            return 0;

        const char *m0 = q + 1, *m1 = m0;

        while (ident(*m1))
            m1++;

        Member *mem = NULL;

        for (int k = 0; k < cur->count; k++)
            if ((size_t)(m1 - m0) == strlen(cur->m[k].name) && !strncmp(m0, cur->m[k].name, (size_t)(m1 - m0)))
                mem = &cur->m[k];

        if (mem == NULL)
            return 0;

        strncat(path, "_", sizeof(path) - strlen(path) - 1);
        strncat(path, mem->name, sizeof(path) - strlen(path) - 1);
        q = m1;

        if (opaqueType(mem->type))
        {
            // the sampler's own index, if any, comes last
            while (*q == '[')
            {
                int depth = 0;
                const char *s = q;

                do
                {
                    if (*q == '[') depth++;
                    else if (*q == ']') depth--;
                    q++;
                } while (*q && depth > 0);

                if (strlen(idx) + (size_t)(q - s) + 1 >= sizeof(idx))
                    return 0;

                strncat(idx, s, (size_t)(q - s));
            }

            if (!adds(out, path) || !adds(out, idx))
                return 0;

            return (size_t)(q - p);
        }

        cur = findStruct(st, ns, mem->type);

        if (cur == NULL || !cur->opaque)
            return 0;
    }
}

char *mglFlattenOpaqueStructs(const char *src)
{
    Struct *st = calloc(MAX_STRUCTS, sizeof(Struct));
    Uniform uni[16];
    int ns = 0, nu = 0;
    Buf out = { 0 };
    char *result = NULL;

    if (st == NULL || strstr(src, "struct") == NULL || strstr(src, "uniform") == NULL)
    {
        free(st);
        return NULL;
    }

    // the struct definitions
    for (const char *p = strstr(src, "struct"); p; p = strstr(p + 6, "struct"))
    {
        if ((p > src && ident(p[-1])) || ident(p[6]))
            continue;

        const char *q = p + 6;

        while (isspace((unsigned char)*q))
            q++;

        const char *n0 = q;

        while (ident(*q))
            q++;

        const char *n1 = q;

        while (isspace((unsigned char)*q))
            q++;

        if (*q != '{' || n1 == n0 || ns >= MAX_STRUCTS)
            continue;

        const char *close = strchr(q, '}');

        if (close == NULL)
            break;

        snprintf(st[ns].name, sizeof(st[ns].name), "%.*s", (int)(n1 - n0), n0);
        st[ns].count = readMembers(q + 1, (size_t)(close - q - 1), st[ns].m);
        ns++;
    }

    markOpaque(st, ns);

    // the uniforms of those structs: "uniform S s;" or "uniform S s[2];"
    for (const char *p = strstr(src, "uniform"); p && nu < 16; p = strstr(p + 7, "uniform"))
    {
        if ((p > src && ident(p[-1])) || ident(p[7]))
            continue;

        const char *q = p + 7, *t0, *t1, *v0, *v1;

        while (isspace((unsigned char)*q))
            q++;

        t0 = q;

        while (ident(*q))
            q++;

        t1 = q;

        while (isspace((unsigned char)*q))
            q++;

        v0 = q;

        while (ident(*q))
            q++;

        v1 = q;

        char tn[64];

        snprintf(tn, sizeof(tn), "%.*s", (int)(t1 - t0), t0);

        Struct *s = findStruct(st, ns, tn);

        if (s && s->opaque && v1 > v0)
        {
            snprintf(uni[nu].name, sizeof(uni[nu].name), "%.*s", (int)(v1 - v0), v0);
            uni[nu].type = s;
            nu++;
        }
    }

    if (nu == 0)
        goto done;

    // copy through, rewriting as we go
    const char *p = src;

    while (*p)
    {
        // a struct definition gains its copy straight after it
        if (!strncmp(p, "struct", 6) && (p == src || !ident(p[-1])) && !ident(p[6]))
        {
            const char *q = p + 6;

            while (isspace((unsigned char)*q))
                q++;

            const char *n0 = q;

            while (ident(*q))
                q++;

            char name[64];

            snprintf(name, sizeof(name), "%.*s", (int)(q - n0), n0);

            Struct *s = findStruct(st, ns, name);
            const char *close = strchr(p, '}');
            const char *semi = close ? strchr(close, ';') : NULL;

            if (s && s->opaque && semi)
            {
                if (!add(&out, p, (size_t)(semi + 1 - p)) || !adds(&out, "\n") || !declareCopy(&out, s, st, ns))
                    goto done;

                p = semi + 1;
                continue;
            }
        }

        // a struct uniform takes the copy, and its samplers are declared
        if (!strncmp(p, "uniform", 7) && (p == src || !ident(p[-1])) && !ident(p[7]))
        {
            const char *q = p + 7;

            while (isspace((unsigned char)*q))
                q++;

            const char *t0 = q;

            while (ident(*q))
                q++;

            char tn[64];

            snprintf(tn, sizeof(tn), "%.*s", (int)(q - t0), t0);

            Struct *s = findStruct(st, ns, tn);
            const char *semi = strchr(q, ';');

            if (s && s->opaque && semi)
            {
                const char *v0 = q;

                while (isspace((unsigned char)*v0))
                    v0++;

                const char *v1 = v0;

                while (ident(*v1))
                    v1++;

                int dims[MAX_DIMS * 4], dc = 0;
                char path[128];

                readDims(v1, dims, &dc);
                snprintf(path, sizeof(path), "mglS_%.*s", (int)(v1 - v0), v0);

                if (!adds(&out, "uniform ") || !adds(&out, tn) || !adds(&out, "_mglNoOpaque") ||
                    !add(&out, q, (size_t)(semi + 1 - q)) || !adds(&out, "\n") ||
                    !declareLeaves(&out, st, ns, s, path, dims, dc))
                    goto done;

                p = semi + 1;
                continue;
            }
        }

        // a use of one of those uniforms that reaches a sampler
        if (ident(*p) && (p == src || (!ident(p[-1]) && p[-1] != '.')))
        {
            bool done_here = false;

            for (int i = 0; i < nu && !done_here; i++)
            {
                size_t n = strlen(uni[i].name);

                if (strncmp(p, uni[i].name, n) || ident(p[n]))
                    continue;

                size_t used = rewriteChain(p, &uni[i], st, ns, &out);

                if (used)
                {
                    p += used;
                    done_here = true;
                }
            }

            if (done_here)
                continue;

            // the rest of the word, unchanged
            const char *e = p;

            while (ident(*e))
                e++;

            if (!add(&out, p, (size_t)(e - p)))
                goto done;

            p = e;
            continue;
        }

        if (!add(&out, p, 1))
            goto done;

        p++;
    }

    result = out.s;
    out.s = NULL;

done:
    free(out.s);
    free(st);

    return result;
}

// "s[1].b.a" names what the rewrite above calls "mglS_s_b_a[1]"
bool mglOpaqueStructUniformName(const char *gl_name, char *out, size_t size)
{
    char idx[128] = "";
    size_t at;

    if (strchr(gl_name, '.') == NULL)
        return false;

    at = (size_t)snprintf(out, size, "mglS_");

    for (const char *p = gl_name; *p && at < size - 1; p++)
    {
        if (*p == '[')
        {
            const char *e = strchr(p, ']');

            if (e == NULL)
                return false;

            if ((size_t)(e + 1 - p) < sizeof(idx) - strlen(idx))
                strncat(idx, p, (size_t)(e + 1 - p));

            p = e;
            continue;
        }

        out[at++] = *p == '.' ? '_' : *p;
    }

    out[at] = 0;
    strncat(out, idx, size - strlen(out) - 1);

    return true;
}
