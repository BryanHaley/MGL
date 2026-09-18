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
 * transform_capture.c
 * MGL
 *
 * Transform feedback, written by the shader itself.
 *
 * Metal has no transform feedback stage. What it does have is a shader that
 * can write to a buffer, which is all transform feedback is -- so the last
 * stage before the rasteriser is rewritten to copy the recorded varyings
 * into the bound feedback buffers on its way past.
 *
 * The buffer is declared as a flat array of uints and every component is
 * written by hand, because GL packs captured varyings tightly and std430
 * would pad a vec3 out to four words.
 */

#include "glm_context.h"
#include "mgl_log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// text helpers
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
// what a recorded varying looks like
// ---------------------------------------------------------------------------

// Components a GLSL type occupies in the capture buffer. GL counts a matrix as
// its columns laid end to end. Returns 0 for a type MGL cannot capture.
static int componentsOf(const char *type, char *base_kind)
{
    static const struct { const char *name; int n; char kind; } table[] = {
        { "float", 1, 'f' }, { "vec2", 2, 'f' }, { "vec3", 3, 'f' }, { "vec4", 4, 'f' },
        { "int", 1, 'i' },   { "ivec2", 2, 'i' },{ "ivec3", 3, 'i' },{ "ivec4", 4, 'i' },
        { "uint", 1, 'u' },  { "uvec2", 2, 'u' },{ "uvec3", 3, 'u' },{ "uvec4", 4, 'u' },
        { "mat2", 4, 'f' },  { "mat3", 9, 'f' }, { "mat4", 16, 'f' },
        { "mat2x2", 4, 'f' },{ "mat2x3", 6, 'f' },{ "mat2x4", 8, 'f' },
        { "mat3x2", 6, 'f' },{ "mat3x3", 9, 'f' },{ "mat3x4", 12, 'f' },
        { "mat4x2", 8, 'f' },{ "mat4x3", 12, 'f' },{ "mat4x4", 16, 'f' },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (!strcmp(type, table[i].name))
        {
            *base_kind = table[i].kind;
            return table[i].n;
        }

    *base_kind = 'f';

    return 0;
}

// Columns and rows, so a matrix can be written a column at a time.
static void matrixShape(const char *type, int *cols, int *rows)
{
    *cols = *rows = 0;

    if (strncmp(type, "mat", 3))
        return;

    if (type[3] == 0)
        return;

    if (type[4] == 'x' && type[5])
    {
        *cols = type[3] - '0';
        *rows = type[5] - '0';
    }
    else
    {
        *cols = *rows = type[3] - '0';
    }
}

// Finds "out TYPE name;" for the named varying in the shader source, and hands
// back the type. gl_Position and friends are built in and have known types.
static bool varyingType(const char *src, const char *name, char *type, size_t typelen)
{
    size_t i = 0;

    if (!strcmp(name, "gl_Position")) { snprintf(type, typelen, "vec4"); return true; }
    if (!strcmp(name, "gl_PointSize")) { snprintf(type, typelen, "float"); return true; }

    while (src[i])
    {
        if (!identChar(src[i]) || (i && identChar(src[i - 1])))
        {
            i++;
            continue;
        }

        if (wordAt(src, i, name))
        {
            // walk back over the declaration to the two words before the name
            size_t k = i;
            char words[4][64];
            int n = 0;

            while (k > 0 && n < 4)
            {
                size_t end;

                while (k > 0 && isspace((unsigned char)src[k - 1]))
                    k--;

                if (k == 0 || !identChar(src[k - 1]))
                    break;

                end = k;

                while (k > 0 && identChar(src[k - 1]))
                    k--;

                snprintf(words[n], sizeof(words[n]), "%.*s", (int)(end - k), src + k);
                n++;
            }

            // words[0] is the type, and an "out" behind it says this is an output
            for (int w = 1; w < n; w++)
                if (!strcmp(words[w], "out"))
                {
                    snprintf(type, typelen, "%s", words[0]);
                    return true;
                }
        }

        while (src[i] && identChar(src[i])) i++;
    }

    return false;
}

// ---------------------------------------------------------------------------
// the rewrite
// ---------------------------------------------------------------------------

void mglFreeCaptureInfo(CaptureInfo *ci)
{
    if (ci == NULL)
        return;

    free(ci->rewritten_src);
    free(ci->gather);
    memset(ci, 0, sizeof(*ci));
}

// Turns the names glTransformFeedbackVaryings recorded into laid-out items:
// one buffer with everything in a row, or one buffer each, with
// gl_NextBuffer and gl_SkipComponents moving the write position.
static int itemsFromVaryings(const char *src, char *const *varyings, GLsizei count, bool separate,
                             MglXfbItem *items, int max_items, GLint *stride_bytes)
{
    int n = 0, buffer = 0, offset = 0;

    for (int b = 0; b < MGL_XFB_MAX_BUFFERS; b++)
        stride_bytes[b] = 0;

    for (GLsizei v = 0; v < count; v++)
    {
        const char *name = varyings[v];
        char type[64];
        char kind = 'f';
        int comps, cols, rows;

        if (name == NULL)
            return -1;

        if (!strcmp(name, "gl_NextBuffer"))
        {
            buffer++;
            offset = 0;

            if (buffer >= MGL_XFB_MAX_BUFFERS)
                return -1;

            continue;
        }

        if (!strncmp(name, "gl_SkipComponents", 17))
        {
            offset += 4 * atoi(name + 17);

            if (offset > stride_bytes[buffer])
                stride_bytes[buffer] = offset;

            continue;
        }

        if (separate)
        {
            buffer = (int)v;
            offset = 0;
        }

        if (buffer >= MGL_XFB_MAX_BUFFERS || n >= max_items)
            return -1;

        if (!varyingType(src, name, type, sizeof type))
        {
            MGL_ERR("MGL Error: transform feedback varying '%s' is not an output of this stage\n", name);
            return -1;
        }

        comps = componentsOf(type, &kind);

        if (comps == 0)
        {
            MGL_ERR("MGL Error: transform feedback cannot lay out '%s' of type %s\n", name, type);
            return -1;
        }

        matrixShape(type, &cols, &rows);

        memset(&items[n], 0, sizeof(items[n]));
        snprintf(items[n].expr, sizeof(items[n].expr), "%s", name);
        items[n].buffer = buffer;
        items[n].offset = offset;
        items[n].kind = kind;
        items[n].components = comps;
        items[n].rows = cols ? rows : 0;
        n++;

        offset += 4 * comps;

        if (offset > stride_bytes[buffer])
            stride_bytes[buffer] = offset;
    }

    return n;
}

// Rewrites the last pre-rasterisation stage to copy the laid-out items into
// the feedback buffers. slot_expr says which recorded vertex this call is.
static bool buildCapture(const char *src, const MglXfbItem *items, int count,
                         const GLint *stride_bytes, const char *slot_expr, CaptureInfo *ci)
{
    Buf out = {0};
    Buf body = {0};
    const char *main_at = NULL;
    char line[512];

    for (size_t i = 0; src[i]; i++)
        if (wordAt(src, i, "main") && src[skipSpace(src, i + 4)] == '(')
        {
            main_at = src + i;
            break;
        }

    if (main_at == NULL || count <= 0)
        return false;

    ci->buffer_count = 0;

    for (int b = 0; b < MGL_XFB_MAX_BUFFERS; b++)
    {
        ci->stride_bytes[b] = stride_bytes[b];

        if (stride_bytes[b] > 0)
            ci->buffer_count = b + 1;
    }

    if (!bufAdd(&body, "\nvoid mglXfbCapture(int mglSlot, int mglOn)\n{\n  if (mglOn == 0) return;\n"))
        goto fail;

    for (int v = 0; v < count; v++)
    {
        const MglXfbItem *it = &items[v];
        int word = it->offset / 4;
        int stride_words = (stride_bytes[it->buffer] + 3) / 4;

        for (int c = 0; c < it->components; c++)
        {
            char access[240];

            if (it->rows)
                snprintf(access, sizeof(access), "%s[%d][%d]", it->expr, c / it->rows, c % it->rows);
            else if (it->components == 1)
                snprintf(access, sizeof(access), "%s", it->expr);
            else
                snprintf(access, sizeof(access), "%s[%d]", it->expr, c);

            if (it->kind == 'd')
            {
                snprintf(line, sizeof(line),
                         "  { uvec2 w = packDouble2x32(%s);\n"
                         "    mglXfbBuf%d[mglSlot * %d + %d] = w.x;\n"
                         "    mglXfbBuf%d[mglSlot * %d + %d] = w.y; }\n",
                         access, it->buffer, stride_words, word + 2 * c,
                         it->buffer, stride_words, word + 2 * c + 1);
            }
            else
            {
                const char *cast = it->kind == 'f' ? "floatBitsToUint" : it->kind == 'u' ? "" : "uint";

                snprintf(line, sizeof(line), "  mglXfbBuf%d[mglSlot * %d + %d] = %s(%s);\n",
                         it->buffer, stride_words, word + c, cast, access);
            }

            if (!bufAdd(&body, line))
                goto fail;
        }
    }

    if (!bufAdd(&body, "}\n"))
        goto fail;

    // ---- put the pieces together ----
    {
        const char *rest = src;
        const char *at = strstr(src, "#version");
        int version = 110;

        if (at)
        {
            const char *nl = strchr(at, '\n');

            if (nl == NULL)
                goto fail;

            sscanf(at, "#version %d", &version);

            if (!bufAddN(&out, src, (size_t)(at - src)))
                goto fail;

            rest = nl + 1;
        }

        // std430 storage blocks need 4.30, and plenty of shaders are older
        snprintf(line, sizeof(line), "#version %d core\n", version > 430 ? version : 430);

        if (!bufAdd(&out, line))
            goto fail;

        for (int b = 0; b < ci->buffer_count; b++)
        {
            if (stride_bytes[b] <= 0)
                continue;

            snprintf(line, sizeof(line),
                     "layout(std430, binding = %d) buffer MglXfbB%d { uint mglXfbBuf%d[]; };\n",
                     MGL_XFB_FIRST_BINDING + b, b, b);

            if (!bufAdd(&out, line))
                goto fail;
        }

        // SPIRV-Cross hoists a uniform a function reads into a thread
        // reference parameter, which Metal will not bind a constant to, so
        // these are copied into ordinary variables first.
        if (!bufAdd(&out, "uniform int mglXfbOnU;\nuniform int mglXfbBaseU;\n"))
            goto fail;

        if (!bufAddN(&out, rest, (size_t)(main_at - rest)) ||
            !bufAdd(&out, "mglXfbBody") ||
            !bufAdd(&out, main_at + 4))
            goto fail;

        snprintf(line, sizeof(line),
                 "\nvoid main()\n{\n"
                 "  int mglOn = mglXfbOnU;\n"
                 "  int mglBase = mglXfbBaseU;\n"
                 "  mglXfbBody();\n"
                 "  mglXfbCapture(%s, mglOn);\n}\n", slot_expr);

        if (!bufAdd(&out, body.s ? body.s : "") || !bufAdd(&out, line))
            goto fail;
    }

    ci->rewritten_src = out.s;
    free(body.s);

    return true;

fail:
    free(out.s);
    free(body.s);
    memset(ci, 0, sizeof(*ci));

    return false;
}

// Rewrites the last pre-rasterisation stage to copy the recorded varyings
// into the feedback buffers. The shader's own xfb qualifiers win over the
// names glTransformFeedbackVaryings gave. Returns false when there is nothing
// to capture or it cannot be laid out, which leaves capture off.
int mglTransformCaptureItems(const char *src, void *shader, char *const *varyings, GLsizei count,
                             GLenum buffer_mode, MglXfbItem *items, int max_items,
                             GLint *stride_bytes, GLboolean *from_shader)
{
    int n = mglXfbLayout(shader, items, max_items, stride_bytes, MGL_XFB_MAX_BUFFERS);

    *from_shader = n > 0;

    if (n == 0 && count > 0)
        n = itemsFromVaryings(src, varyings, count, buffer_mode == GL_SEPARATE_ATTRIBS,
                              items, max_items, stride_bytes);

    return n;
}

bool mglBuildTransformCapture(const char *src, void *shader, char *const *varyings, GLsizei count,
                              GLenum buffer_mode, const char *slot_expr, CaptureInfo *ci)
{
    MglXfbItem items[64];
    GLint strides[MGL_XFB_MAX_BUFFERS];
    int n;

    memset(ci, 0, sizeof(*ci));

    n = mglTransformCaptureItems(src, shader, varyings, count, buffer_mode, items, 64,
                                 strides, &ci->from_shader);

    if (n <= 0)
        return false;

    ci->varying_count = n;
    ci->separate = (buffer_mode == GL_SEPARATE_ATTRIBS) && !ci->from_shader;

    return buildCapture(src, items, n, strides, slot_expr, ci);
}
