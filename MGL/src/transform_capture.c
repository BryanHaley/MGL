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
    memset(ci, 0, sizeof(*ci));
}

// Rewrites the last pre-rasterisation stage to copy the recorded varyings into
// the feedback buffers. Returns false when the shader records something MGL
// cannot lay out, which leaves transform feedback off rather than wrong.
bool mglBuildTransformCapture(const char *src, char *const *varyings, GLsizei count,
                              GLenum buffer_mode, CaptureInfo *ci)
{
    Buf out = {0};
    Buf body = {0};
    const char *main_at = NULL;
    bool separate = (buffer_mode == GL_SEPARATE_ATTRIBS);
    int words = 0;
    char line[512];

    memset(ci, 0, sizeof(*ci));

    if (count <= 0 || count > MGL_XFB_MAX_BUFFERS)
        return false;

    for (size_t i = 0; src[i]; i++)
        if (wordAt(src, i, "main"))
        {
            size_t k = skipSpace(src, i + 4);

            if (src[k] == '(')
            {
                main_at = src + i;
                break;
            }
        }

    if (main_at == NULL)
        return false;

    // one buffer holds everything, or one buffer per varying
    ci->buffer_count = separate ? count : 1;

    if (!bufAdd(&body, "\nvoid mglXfbCapture(int mglSlot, int mglOn)\n{\n  if (mglOn == 0) return;\n"))
        goto fail;

    for (GLsizei v = 0; v < count; v++)
    {
        char type[64];
        char kind = 'f';
        int n, cols, rows;
        int buf = separate ? (int)v : 0;
        int base = separate ? 0 : words;

        if (!varyings[v] || !varyingType(src, varyings[v], type, sizeof type))
        {
            MGL_ERR("MGL Error: transform feedback varying '%s' is not an output of this stage\n",
                    varyings[v] ? varyings[v] : "(null)");
            goto fail;
        }

        n = componentsOf(type, &kind);

        if (n == 0)
        {
            MGL_ERR("MGL Error: transform feedback cannot lay out '%s' of type %s\n",
                    varyings[v], type);
            goto fail;
        }

        matrixShape(type, &cols, &rows);

        snprintf(line, sizeof(line), "  // %s (%s)\n", varyings[v], type);

        if (!bufAdd(&body, line))
            goto fail;

        for (int c = 0; c < n; c++)
        {
            const char *cast = kind == 'f' ? "floatBitsToUint" : kind == 'i' ? "uint" : "";
            char access[160];

            if (cols)
                snprintf(access, sizeof(access), "%s[%d][%d]", varyings[v], c / rows, c % rows);
            else if (n == 1)
                snprintf(access, sizeof(access), "%s", varyings[v]);
            else
                snprintf(access, sizeof(access), "%s[%d]", varyings[v], c);

            if (cast[0])
                snprintf(line, sizeof(line), "  mglXfbBuf%d[mglSlot * %d + %d] = %s(%s);\n",
                         buf, separate ? n : 0, 0, cast, access);
            else
                snprintf(line, sizeof(line), "  mglXfbBuf%d[mglSlot * %d + %d] = %s;\n",
                         buf, separate ? n : 0, 0, access);

            // the stride and offset above depend on the mode, so write them here
            if (separate)
                snprintf(line, sizeof(line), "  mglXfbBuf%d[mglSlot * %d + %d] = %s%s%s%s;\n",
                         buf, n, c, cast[0] ? cast : "", cast[0] ? "(" : "",
                         access, cast[0] ? ")" : "");
            else
                snprintf(line, sizeof(line), "  mglXfbBuf0[mglSlot * MGL_XFB_STRIDE + %d] = %s%s%s%s;\n",
                         base + c, cast[0] ? cast : "", cast[0] ? "(" : "",
                         access, cast[0] ? ")" : "");

            if (!bufAdd(&body, line))
                goto fail;
        }

        ci->components[v] = n;
        words += n;
    }

    if (!bufAdd(&body, "}\n"))
        goto fail;

    ci->stride_words = separate ? 0 : words;
    ci->varying_count = count;
    ci->separate = separate;

    // ---- put the pieces together ----
    {
        const char *rest = src;
        const char *at = strstr(src, "#version");

        if (at)
        {
            const char *nl = strchr(at, '\n');

            if (nl == NULL)
                goto fail;

            if (!bufAddN(&out, src, (size_t)(at - src)))
                goto fail;

            rest = nl + 1;
        }

        // std430 storage blocks need 4.30, and plenty of shaders are older
        if (!bufAdd(&out, "#version 430 core\n"))
            goto fail;

        snprintf(line, sizeof(line), "#define MGL_XFB_STRIDE %d\n", words ? words : 1);

        if (!bufAdd(&out, line))
            goto fail;

        for (int b = 0; b < ci->buffer_count; b++)
        {
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

        if (!bufAdd(&out, body.s ? body.s : "") ||
            !bufAdd(&out, "\nvoid main()\n{\n"
                          "  int mglOn = mglXfbOnU;\n"
                          "  int mglBase = mglXfbBaseU;\n"
                          "  mglXfbBody();\n"
                          "  mglXfbCapture(gl_VertexID - mglBase, mglOn);\n}\n"))
            goto fail;
    }

    ci->rewritten_src = out.s;
    out.s = NULL;
    free(body.s);

    return true;

fail:
    free(out.s);
    free(body.s);
    memset(ci, 0, sizeof(*ci));

    return false;
}
