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
 * subroutines.c
 * MGL
 *
 * GLSL subroutines, rewritten into something glslang will accept.
 *
 * glslang refuses the subroutine keyword outright when it is generating
 * SPIR-V, because Vulkan has no such feature -- but subroutines have been
 * core GL since 4.0. So the source is rewritten before it ever reaches
 * glslang: the subroutine uniform becomes a plain int, each call to it
 * becomes a call to a generated function, and that function switches on
 * the int. glUniformSubroutinesuiv then just writes the int.
 */

#include "glm_context.h"
#include "mgl_log.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// small text helpers
// ---------------------------------------------------------------------------

static bool identChar(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

// true when src[at..] is the word `word` and not part of a longer identifier
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

// copies the identifier starting at i into dst; returns the index after it
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

// index just past the ')' matching the '(' at `i`
static size_t matchParen(const char *s, size_t i)
{
    int depth = 0;

    for (; s[i]; i++)
    {
        if (s[i] == '(') depth++;
        else if (s[i] == ')' && --depth == 0) return i + 1;
    }

    return i;
}

// index just past the ']' matching the '[' at `i`
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

// ---------------------------------------------------------------------------
// a growable string, so the rewrite does not have to size its output up front
// ---------------------------------------------------------------------------

typedef struct {
    char *s;
    size_t len;
    size_t cap;
} Buf;

static bool bufGrow(Buf *b, size_t need)
{
    if (b->len + need + 1 <= b->cap)
        return true;

    size_t cap = b->cap ? b->cap : 1024;

    while (cap < b->len + need + 1)
        cap *= 2;

    char *s = (char *)realloc(b->s, cap);

    if (s == NULL)
        return false;

    b->s = s;
    b->cap = cap;

    return true;
}

static bool bufAddN(Buf *b, const char *s, size_t n)
{
    if (!bufGrow(b, n))
        return false;

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
// what the scan found
// ---------------------------------------------------------------------------

#define MAX_SUB_TYPES     64
#define MAX_SUB_FNS      MAX_SUB_FNS_LIMIT
#define MAX_SUB_UNIFORMS 128
#define MAX_TYPES_PER_FN   8

typedef struct {
    char name[64];
    char ret[64];
    char params[256];   // the text between the parens, as written
} SubType;

typedef struct {
    char name[64];
    char ret[64];
    char params[256];
    int  types[MAX_TYPES_PER_FN];
    int  type_count;
} SubFn;

typedef struct {
    char name[64];
    int  type;          // index into types[]
    int  array_size;    // 1 when it is not an array
    int  location;      // layout(location), -1 when none
} SubUniform;

typedef struct {
    SubType    types[MAX_SUB_TYPES];       int type_count;
    SubFn      fns[MAX_SUB_FNS];           int fn_count;
    SubUniform uniforms[MAX_SUB_UNIFORMS]; int uniform_count;
    const char *error;
} Scan;

static int findType(Scan *sc, const char *name)
{
    for (int i = 0; i < sc->type_count; i++)
        if (!strcmp(sc->types[i].name, name))
            return i;

    return -1;
}

// The parameter list as written names its arguments; the generated wrapper
// needs to pass them on, so pull the names out. "vec4 a, float b" -> "a, b".
static void paramNames(const char *params, char *dst, size_t dstlen)
{
    size_t out = 0;
    const char *p = params;

    dst[0] = 0;

    if (skipSpace(params, 0) == strlen(params))
        return;

    while (*p)
    {
        const char *end = p;
        int depth = 0;

        for (; *end; end++)
        {
            if (*end == '(' || *end == '[') depth++;
            else if (*end == ')' || *end == ']') depth--;
            else if (*end == ',' && depth == 0) break;
        }

        // the last identifier before the comma is the parameter's name, unless
        // an array size follows it
        const char *q = end;

        while (q > p && !identChar(q[-1]))
            q--;

        const char *stop = q;

        while (q > p && identChar(q[-1]))
            q--;

        if (q < stop)
        {
            if (out && out + 2 < dstlen)
            {
                dst[out++] = ',';
                dst[out++] = ' ';
            }

            size_t n = (size_t)(stop - q);

            if (out + n < dstlen)
            {
                memcpy(dst + out, q, n);
                out += n;
            }
        }

        if (*end == 0)
            break;

        p = end + 1;
    }

    dst[out] = 0;
}

// A subroutine function may return void, in which case the wrapper has no
// fallback value to produce.
static bool returnsVoid(const char *ret)
{
    size_t i = skipSpace(ret, 0);

    return wordAt(ret, i, "void");
}

// The words of a parameter list with the parameter names left out, so two
// lists can be compared: "inout vec3 a, float b[2]" -> "inout vec3 , float [2]".
static void signatureOf(const char *params, char *dst, size_t dstlen)
{
    char tmp[256];
    size_t out = 0, n = 0;
    const char *p = params;

    dst[0] = 0;

    while (*p)
    {
        const char *end = p;
        int depth = 0;

        for (; *end; end++)
        {
            if (*end == '(' || *end == '[') depth++;
            else if (*end == ')' || *end == ']') depth--;
            else if (*end == ',' && depth == 0) break;
        }

        // the words of one parameter, spaces collapsed
        n = 0;

        for (const char *q = p; q < end && n + 2 < sizeof(tmp); q++)
        {
            if (isspace((unsigned char)*q))
            {
                if (n && tmp[n - 1] != ' ')
                    tmp[n++] = ' ';
            }
            else
            {
                tmp[n++] = *q;
            }
        }

        while (n && tmp[n - 1] == ' ')
            n--;

        tmp[n] = 0;

        // drop the name: the last identifier before any array size
        char *br = strchr(tmp, '[');
        size_t stop = br ? (size_t)(br - tmp) : n;

        while (stop && tmp[stop - 1] == ' ')
            stop--;

        size_t start = stop;

        while (start && identChar(tmp[start - 1]))
            start--;

        // a lone type ("void", or an unnamed parameter) has no name to drop
        if (start > 0)
            memmove(tmp + start, tmp + stop, strlen(tmp + stop) + 1);

        if (strcmp(tmp, "void") && tmp[0])
        {
            if (out && out + 1 < dstlen)
                dst[out++] = ',';

            // "in" and precision words do not change the signature
            for (char *w = strtok(tmp, " "); w; w = strtok(NULL, " "))
            {
                if (!strcmp(w, "in") || !strcmp(w, "highp") ||
                    !strcmp(w, "mediump") || !strcmp(w, "lowp"))
                    continue;

                for (char *c = w; *c && out + 1 < dstlen; c++)
                    dst[out++] = *c;
            }
        }

        if (*end == 0)
            break;

        p = end + 1;
    }

    dst[out] = 0;
}

static bool sameWords(const char *a, const char *b)
{
    for (;;)
    {
        while (isspace((unsigned char)*a)) a++;
        while (isspace((unsigned char)*b)) b++;

        if (*a == 0 || *b == 0)
            return *a == *b;

        if (*a != *b)
            return false;

        a++;
        b++;
    }
}

// GL 4.6 section 6.1.2: each function has to match every subroutine type it
// names, and every subroutine uniform needs a function to call.
static const char *checkSubroutines(Scan *sc)
{
    for (int f = 0; f < sc->fn_count; f++)
    {
        char fs[256];

        signatureOf(sc->fns[f].params, fs, sizeof(fs));

        for (int k = 0; k < sc->fns[f].type_count; k++)
        {
            SubType *t = &sc->types[sc->fns[f].types[k]];
            char ts[256];

            signatureOf(t->params, ts, sizeof(ts));

            if (strcmp(fs, ts) || !sameWords(sc->fns[f].ret, t->ret))
                return "a subroutine function does not match its subroutine type";
        }
    }

    for (int u = 0; u < sc->uniform_count; u++)
    {
        bool found = false;

        for (int f = 0; f < sc->fn_count && !found; f++)
            for (int k = 0; k < sc->fns[f].type_count; k++)
                if (sc->fns[f].types[k] == sc->uniforms[u].type)
                    found = true;

        if (!found)
            return "a subroutine uniform has no subroutine function to call";
    }

    return NULL;
}

// ---------------------------------------------------------------------------
// pass 1: find the declarations and strip the subroutine keyword
// ---------------------------------------------------------------------------

static bool scanAndStrip(const char *src, Scan *sc, Buf *out)
{
    size_t i = 0;
    size_t copied = 0;

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

        if (!wordAt(src, i, "subroutine"))
        {
            i++;
            continue;
        }

        // everything up to the keyword goes out unchanged
        if (!bufAddN(out, src + copied, i - copied))
            return false;

        size_t j = skipSpace(src, i + strlen("subroutine"));

        // ---- subroutine(T, ...) RET FN(params)  -- an implementation ----
        if (src[j] == '(')
        {
            size_t close = matchParen(src, j);
            char list[256];
            size_t n = close - j - 2 < sizeof(list) - 1 ? close - j - 2 : sizeof(list) - 1;

            memcpy(list, src + j + 1, n);
            list[n] = 0;

            size_t k = skipSpace(src, close);

            // RET may be several words (precision qualifiers and the like), so
            // take the identifier just before the '(' as the function name
            size_t open = k;

            while (src[open] && src[open] != '(' && src[open] != ';' && src[open] != '{')
                open++;

            size_t nameend = open;

            while (nameend > k && !identChar(src[nameend - 1]))
                nameend--;

            size_t namestart = nameend;

            while (namestart > k && identChar(src[namestart - 1]))
                namestart--;

            if (sc->fn_count < MAX_SUB_FNS && nameend > namestart)
            {
                SubFn *f = &sc->fns[sc->fn_count];

                memset(f, 0, sizeof(*f));
                snprintf(f->name, sizeof(f->name), "%.*s",
                         (int)(nameend - namestart), src + namestart);
                snprintf(f->ret, sizeof(f->ret), "%.*s", (int)(namestart - k), src + k);

                if (src[open] == '(')
                {
                    size_t pclose = matchParen(src, open);

                    snprintf(f->params, sizeof(f->params), "%.*s",
                             (int)(pclose - open - 2), src + open + 1);
                }

                // the parenthesised list names the types it implements
                char *tok = strtok(list, ",");

                while (tok && f->type_count < MAX_TYPES_PER_FN)
                {
                    char t[64];
                    size_t a = skipSpace(tok, 0);

                    readIdent(tok, a, t, sizeof(t));

                    int ti = findType(sc, t);

                    if (ti >= 0)
                        f->types[f->type_count++] = ti;

                    tok = strtok(NULL, ",");
                }

                if (f->type_count)
                    sc->fn_count++;
            }

            // drop only the "subroutine(...)" prefix; the function itself stays
            i = close;
            copied = i;
            continue;
        }

        // ---- subroutine uniform TYPE NAME[n];  -- the switch ----
        if (wordAt(src, j, "uniform"))
        {
            char type[64], name[64];
            size_t k = skipSpace(src, j + strlen("uniform"));

            k = readIdent(src, k, type, sizeof(type));
            k = skipSpace(src, k);
            k = readIdent(src, k, name, sizeof(name));
            k = skipSpace(src, k);

            int arr = 1;

            if (src[k] == '[')
            {
                arr = atoi(src + k + 1);

                if (arr < 1)
                    arr = 1;

                k = matchBracket(src, k);
                k = skipSpace(src, k);
            }

            if (src[k] == ';')
                k++;

            int ti = findType(sc, type);

            // "layout(location = N)" in front belongs to the subroutine
            // uniform, not to the plain int it becomes
            int location = -1;
            {
                size_t end = out->len;

                while (end > 0 && isspace((unsigned char)out->s[end - 1]))
                    end--;

                if (end > 0 && out->s[end - 1] == ')')
                {
                    size_t open = end - 1;
                    int depth = 0;

                    while (open > 0)
                    {
                        if (out->s[open] == ')') depth++;
                        else if (out->s[open] == '(' && --depth == 0) break;
                        open--;
                    }

                    size_t w = open;

                    while (w > 0 && isspace((unsigned char)out->s[w - 1]))
                        w--;

                    if (w >= 6 && !strncmp(out->s + w - 6, "layout", 6))
                    {
                        const char *loc = strstr(out->s + open, "location");

                        if (loc && loc < out->s + end)
                        {
                            const char *eq = strchr(loc, '=');

                            if (eq && eq < out->s + end)
                                location = (int)strtol(eq + 1, NULL, 0);
                        }

                        out->len = w - 6;
                        out->s[out->len] = 0;
                    }
                }
            }

            if (ti >= 0 && sc->uniform_count < MAX_SUB_UNIFORMS)
            {
                SubUniform *u = &sc->uniforms[sc->uniform_count++];

                snprintf(u->name, sizeof(u->name), "%s", name);
                u->type = ti;
                u->array_size = arr;
                u->location = location;

                // the selector the driver writes, plus a prototype so calls
                // earlier in the file still resolve
                // Only the selector goes in here. The dispatcher's prototype
                // is inserted after the calls are rewritten, or the rewriter
                // would treat the prototype as a call site too.
                char decl[128];

                if (arr > 1)
                    snprintf(decl, sizeof(decl), "uniform int %s%s[%d];",
                             name, MGL_SUBROUTINE_SUFFIX, arr);
                else
                    snprintf(decl, sizeof(decl), "uniform int %s%s;",
                             name, MGL_SUBROUTINE_SUFFIX);

                if (!bufAdd(out, decl))
                    return false;
            }

            i = k;
            copied = i;
            continue;
        }

        // ---- subroutine RET TYPE(params);  -- the type declaration ----
        {
            size_t k = j;
            size_t open = k;

            while (src[open] && src[open] != '(' && src[open] != ';')
                open++;

            if (src[open] != '(')
            {
                // not a shape we understand; leave the text alone
                i = j;
                continue;
            }

            size_t nameend = open;

            while (nameend > k && !identChar(src[nameend - 1]))
                nameend--;

            size_t namestart = nameend;

            while (namestart > k && identChar(src[namestart - 1]))
                namestart--;

            size_t close = matchParen(src, open);
            size_t end = close;

            while (src[end] && src[end] != ';')
                end++;

            if (src[end] == ';')
                end++;

            if (sc->type_count < MAX_SUB_TYPES && nameend > namestart)
            {
                SubType *t = &sc->types[sc->type_count++];

                memset(t, 0, sizeof(*t));
                snprintf(t->name, sizeof(t->name), "%.*s",
                         (int)(nameend - namestart), src + namestart);
                snprintf(t->ret, sizeof(t->ret), "%.*s",
                         (int)(namestart - k), src + k);
                snprintf(t->params, sizeof(t->params), "%.*s",
                         (int)(close - open - 2), src + open + 1);

                // trim the trailing space the return type picked up
                size_t rl = strlen(t->ret);

                while (rl && isspace((unsigned char)t->ret[rl - 1]))
                    t->ret[--rl] = 0;
            }

            // the declaration itself has no equivalent, so it goes away
            i = end;
            copied = i;
        }
    }

    return bufAdd(out, src + copied);
}

// ---------------------------------------------------------------------------
// pass 2: a call to a subroutine uniform becomes a call to its dispatcher with
// the selector as the first argument. name(a) -> name(name__mglsr, a), and
// name[i](a) -> name(name__mglsr[i], a).
// ---------------------------------------------------------------------------

static bool rewriteCalls(const char *src, Scan *sc, Buf *out)
{
    size_t i = 0, copied = 0;

    while (src[i])
    {
        if (!identChar(src[i]) || (i && identChar(src[i - 1])))
        {
            i++;
            continue;
        }

        int hit = -1;
        size_t sel_start = 0, sel_end = 0, argopen = 0;

        for (int u = 0; u < sc->uniform_count; u++)
        {
            SubUniform *su = &sc->uniforms[u];

            if (!wordAt(src, i, su->name))
                continue;

            size_t k = skipSpace(src, i + strlen(su->name));

            if (su->array_size > 1 && src[k] == '[')
            {
                size_t close = matchBracket(src, k);
                size_t after = skipSpace(src, close);

                if (src[after] != '(')
                {
                    sc->error = "a subroutine uniform can only be called";
                    break;
                }

                sel_start = k + 1;
                sel_end = close - 1;
                argopen = after;
                hit = u;
            }
            else if (su->array_size == 1 && src[k] == '(')
            {
                sel_start = sel_end = 0;
                argopen = k;
                hit = u;
            }
            else
            {
                // anything but a call: assigned, compared, passed around
                sc->error = "a subroutine uniform can only be called";
            }

            break;
        }

        if (hit < 0)
        {
            while (src[i] && identChar(src[i])) i++;
            continue;
        }

        SubUniform *su = &sc->uniforms[hit];
        size_t argclose = matchParen(src, argopen);
        bool empty = skipSpace(src, argopen + 1) == argclose - 1;
        char open[192];

        if (!bufAddN(out, src + copied, i - copied))
            return false;

        snprintf(open, sizeof(open), "%s(%s%s", su->name, su->name, MGL_SUBROUTINE_SUFFIX);

        if (!bufAdd(out, open))
            return false;

        if (sel_end > sel_start)
        {
            if (!bufAdd(out, "[") ||
                !bufAddN(out, src + sel_start, sel_end - sel_start) ||
                !bufAdd(out, "]"))
                return false;
        }

        if (!empty)
        {
            if (!bufAdd(out, ", ") ||
                !bufAddN(out, src + argopen + 1, argclose - argopen - 2))
                return false;
        }

        if (!bufAdd(out, ")"))
            return false;

        i = argclose;
        copied = i;
    }

    return bufAdd(out, src + copied);
}

// ---------------------------------------------------------------------------
// pass 3: declare every dispatcher up front, since GLSL wants a function
// declared before it is called and the bodies go at the end.
// ---------------------------------------------------------------------------

static bool insertPrototypes(const char *src, Scan *sc, Buf *out)
{
    const char *at = strstr(src, "#version");
    size_t head = 0;

    if (at)
    {
        const char *nl = strchr(at, '\n');

        head = nl ? (size_t)(nl - src) + 1 : strlen(src);
    }

    // #extension has to come before any declaration, so the prototypes go
    // after the ones the shader opens with rather than straight after #version
    for (;;)
    {
        size_t k = head;

        while (src[k] == ' ' || src[k] == '\t' || src[k] == '\r' || src[k] == '\n')
            k++;

        if (src[k] != '#')
            break;

        size_t d = k + 1;

        while (src[d] == ' ' || src[d] == '\t')
            d++;

        if (strncmp(src + d, "extension", 9) != 0)
            break;

        const char *nl = strchr(src + k, '\n');

        head = nl ? (size_t)(nl - src) + 1 : strlen(src);
    }

    if (!bufAddN(out, src, head))
        return false;

    for (int u = 0; u < sc->uniform_count; u++)
    {
        SubType *t = &sc->types[sc->uniforms[u].type];
        char line[512];

        snprintf(line, sizeof(line), "%s %s(int %s%s%s);\n",
                 t->ret, sc->uniforms[u].name, MGL_SUBROUTINE_INDEX,
                 t->params[0] ? ", " : "", t->params);

        if (!bufAdd(out, line))
            return false;
    }

    return bufAdd(out, src + head);
}

// ---------------------------------------------------------------------------
// pass 3: append one dispatch function per subroutine uniform
// ---------------------------------------------------------------------------

static bool appendDispatchers(Scan *sc, Buf *out)
{
    for (int u = 0; u < sc->uniform_count; u++)
    {
        SubUniform *su = &sc->uniforms[u];
        SubType *t = &sc->types[su->type];
        char args[256];
        char line[1024];

        paramNames(t->params, args, sizeof(args));

        snprintf(line, sizeof(line), "\n%s %s(int %s%s%s)\n{\n  switch (%s) {\n",
                 t->ret, su->name, MGL_SUBROUTINE_INDEX,
                 t->params[0] ? ", " : "", t->params, MGL_SUBROUTINE_INDEX);

        if (!bufAdd(out, line))
            return false;

        for (int f = 0; f < sc->fn_count; f++)
        {
            SubFn *fn = &sc->fns[f];
            bool compatible = false;

            for (int k = 0; k < fn->type_count; k++)
                if (fn->types[k] == su->type)
                    compatible = true;

            if (!compatible)
                continue;

            if (returnsVoid(t->ret))
                snprintf(line, sizeof(line), "  case %d: %s(%s); return;\n",
                         f, fn->name, args);
            else
                snprintf(line, sizeof(line), "  case %d: return %s(%s);\n",
                         f, fn->name, args);

            if (!bufAdd(out, line))
                return false;
        }

        // GL leaves the selector undefined until it is set, so an unmatched
        // value has to produce something rather than fall off the end
        if (returnsVoid(t->ret))
            snprintf(line, sizeof(line), "  }\n}\n");
        else
            snprintf(line, sizeof(line), "  }\n  return %s(0);\n}\n", t->ret);

        if (!bufAdd(out, line))
            return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// the entry point shaders.c calls
// ---------------------------------------------------------------------------

void mglFreeSubroutineInfo(SubroutineInfo *info)
{
    if (info == NULL)
        return;

    for (GLuint i = 0; i < info->fn_count; i++)
        free(info->fn_names[i]);

    for (GLuint i = 0; i < info->uniform_count; i++)
        free(info->uniform_names[i]);

    free(info->fn_names);
    free(info->uniform_names);
    free(info->uniform_array_size);
    free(info->uniform_compatible);
    free(info->uniform_type);
    free(info->uniform_location);
    free(info->fn_types);

    memset(info, 0, sizeof(*info));
}

bool mglSubroutineCompatible(const SubroutineInfo *info, GLuint uniform, GLuint fn)
{
    if (info == NULL || uniform >= info->uniform_count || fn >= info->fn_count ||
        info->uniform_type == NULL || info->fn_types == NULL)
        return false;

    return (info->fn_types[fn] >> info->uniform_type[uniform]) & 1;
}

// Copies what the rewrite found, so a program keeps it after the application
// has detached and deleted the shader it came from.
bool mglCopySubroutineInfo(SubroutineInfo *dst, const SubroutineInfo *src)
{
    mglFreeSubroutineInfo(dst);

    if (src == NULL || (src->fn_count == 0 && src->uniform_count == 0))
        return true;

    dst->fn_names = (char **)calloc(src->fn_count ? src->fn_count : 1, sizeof(char *));
    dst->uniform_names = (char **)calloc(src->uniform_count ? src->uniform_count : 1, sizeof(char *));
    dst->uniform_array_size = (GLuint *)calloc(src->uniform_count ? src->uniform_count : 1, sizeof(GLuint));
    dst->uniform_compatible = (GLuint *)calloc(src->uniform_count ? src->uniform_count : 1, sizeof(GLuint));
    dst->uniform_type = (GLuint *)calloc(src->uniform_count ? src->uniform_count : 1, sizeof(GLuint));
    dst->uniform_location = (GLint *)calloc(src->uniform_count ? src->uniform_count : 1, sizeof(GLint));
    dst->fn_types = (GLuint64 *)calloc(src->fn_count ? src->fn_count : 1, sizeof(GLuint64));

    if (dst->fn_names == NULL || dst->uniform_names == NULL ||
        dst->uniform_array_size == NULL || dst->uniform_compatible == NULL ||
        dst->uniform_type == NULL || dst->uniform_location == NULL || dst->fn_types == NULL)
    {
        mglFreeSubroutineInfo(dst);
        return false;
    }

    for (GLuint i = 0; i < src->fn_count; i++)
    {
        dst->fn_names[i] = strdup(src->fn_names[i] ? src->fn_names[i] : "");
        dst->fn_types[i] = src->fn_types ? src->fn_types[i] : 0;
    }

    for (GLuint i = 0; i < src->uniform_count; i++)
    {
        dst->uniform_names[i] = strdup(src->uniform_names[i] ? src->uniform_names[i] : "");
        dst->uniform_array_size[i] = src->uniform_array_size[i];
        dst->uniform_compatible[i] = src->uniform_compatible[i];
        dst->uniform_type[i] = src->uniform_type ? src->uniform_type[i] : 0;
        dst->uniform_location[i] = src->uniform_location ? src->uniform_location[i] : -1;
    }

    dst->fn_count = src->fn_count;
    dst->uniform_count = src->uniform_count;

    return true;
}

// Rewrites GLSL that uses subroutines into GLSL that does not, and records
// what it found so the GL queries can answer. Returns NULL when the source
// has no subroutines, which is the common case and costs one strstr.
char *mglRewriteSubroutines(const char *src, SubroutineInfo *info)
{
    Scan sc;
    Buf stripped = {0}, arrays = {0}, final = {0};
    char *result = NULL;

    if (info)
        info->error = NULL;

    if (strstr(src, "subroutine") == NULL)
        return NULL;

    memset(&sc, 0, sizeof(sc));

    if (!scanAndStrip(src, &sc, &stripped))
        goto done;

    if (sc.type_count == 0 && sc.uniform_count == 0)
        goto done;

    if (!rewriteCalls(stripped.s ? stripped.s : "", &sc, &arrays))
        goto done;

    if (sc.error == NULL)
        sc.error = checkSubroutines(&sc);

    if (!insertPrototypes(arrays.s ? arrays.s : "", &sc, &final))
        goto done;

    if (!appendDispatchers(&sc, &final))
        goto done;

    if (info)
    {
        mglFreeSubroutineInfo(info);

        info->fn_names = (char **)calloc(sc.fn_count ? sc.fn_count : 1, sizeof(char *));
        info->uniform_names = (char **)calloc(sc.uniform_count ? sc.uniform_count : 1, sizeof(char *));
        info->uniform_array_size = (GLuint *)calloc(sc.uniform_count ? sc.uniform_count : 1, sizeof(GLuint));
        info->uniform_compatible = (GLuint *)calloc(sc.uniform_count ? sc.uniform_count : 1, sizeof(GLuint));
        info->uniform_type = (GLuint *)calloc(sc.uniform_count ? sc.uniform_count : 1, sizeof(GLuint));
        info->uniform_location = (GLint *)calloc(sc.uniform_count ? sc.uniform_count : 1, sizeof(GLint));
        info->fn_types = (GLuint64 *)calloc(sc.fn_count ? sc.fn_count : 1, sizeof(GLuint64));

        if (info->fn_names == NULL || info->uniform_names == NULL ||
            info->uniform_array_size == NULL || info->uniform_compatible == NULL ||
            info->uniform_type == NULL || info->uniform_location == NULL || info->fn_types == NULL)
        {
            mglFreeSubroutineInfo(info);
            goto done;
        }

        for (int i = 0; i < sc.fn_count; i++)
        {
            info->fn_names[i] = strdup(sc.fns[i].name);

            for (int k = 0; k < sc.fns[i].type_count; k++)
                info->fn_types[i] |= (GLuint64)1 << sc.fns[i].types[k];
        }

        for (int i = 0; i < sc.uniform_count; i++)
        {
            int compat = 0;

            info->uniform_names[i] = strdup(sc.uniforms[i].name);
            info->uniform_array_size[i] = (GLuint)sc.uniforms[i].array_size;
            info->uniform_type[i] = (GLuint)sc.uniforms[i].type;
            info->uniform_location[i] = sc.uniforms[i].location;

            for (int f = 0; f < sc.fn_count; f++)
                for (int k = 0; k < sc.fns[f].type_count; k++)
                    if (sc.fns[f].types[k] == sc.uniforms[i].type)
                        compat++;

            info->uniform_compatible[i] = (GLuint)compat;
        }

        info->fn_count = (GLuint)sc.fn_count;
        info->uniform_count = (GLuint)sc.uniform_count;
        info->error = sc.error;
    }

    result = final.s;
    final.s = NULL;

done:
    free(stripped.s);
    free(arrays.s);
    free(final.s);

    return result;
}
