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
 * cull_distance.c
 * MGL
 *
 * gl_CullDistance throws away a whole primitive when every one of its vertices
 * agrees. Metal clips, it does not cull, so MGL runs the vertex stage once to
 * record the distances, picks the surviving primitives in a compute pass, and
 * draws those by index.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glm_context.h"
#include "mgl_log.h"

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

// Is this mention of the array its declaration, as in "out float gl_Cull..."?
static bool isDeclaration(const char *src, size_t at)
{
    size_t i = at;

    while (i > 0 && isspace((unsigned char)src[i - 1]))
        i--;

    return i >= 5 && !strncmp(src + i - 5, "float", 5);
}

// How many cull distances the shader uses: the size it redeclared the array
// with, or one past the highest constant index it writes.
static bool cull_was_declared;

GLint mglCullDistanceSize(const char *src)
{
    GLint declared = 0;
    GLint by_index = 0;
    bool mentioned = false;
    bool dynamic = false;

    if (src == NULL)
        return 0;

    for (size_t i = 0; src[i]; i++)
    {
        if (!wordAt(src, i, "gl_CullDistance"))
            continue;

        mentioned = true;

        size_t k = skipSpace(src, i + strlen("gl_CullDistance"));

        if (src[k] != '[')
            continue;

        k = skipSpace(src, k + 1);

        if (!isdigit((unsigned char)src[k]))
        {
            dynamic = true;
            continue;
        }

        GLint n = atoi(src + k);

        if (isDeclaration(src, i))
        {
            if (n > declared)
                declared = n;
        }
        else if (n + 1 > by_index)
        {
            by_index = n + 1;
        }
    }

    cull_was_declared = declared > 0;

    GLint best = declared ? declared : by_index;

    // a dynamic index says nothing about the size, so the array has to be
    // whatever the shader declared, or the whole thing
    if (best == 0 && (dynamic || mentioned))
        best = MAX_CLIP_DISTANCES;

    if (best > MAX_CLIP_DISTANCES)
        best = MAX_CLIP_DISTANCES;

    return best;
}

// std430 blocks need GLSL 4.30 and gl_CullDistance needs 4.50, so the capture
// shader is raised to 4.50 whatever the application wrote.
static bool addRaisedVersion(Buf *out, const char *src, const char **body_start)
{
    const char *at = strstr(src, "#version");
    const char *nl;
    int version = 110;

    *body_start = src;

    if (at == NULL)
        return bufAdd(out, "#version 450 core\n");

    sscanf(at, "#version %d", &version);
    nl = strchr(at, '\n');

    if (nl == NULL)
        return false;

    if (!bufAddN(out, src, (size_t)(at - src)))
        return false;

    *body_start = nl + 1;

    if (version >= 450)
        return bufAddN(out, at, (size_t)(nl - at) + 1);

    return bufAdd(out, "#version 450 core\n");
}

// The vertex shader keeps its job and gains one more: writing the cull
// distances where the compute pass can read them.
static char *buildCapture(const char *vs_src, GLint n, bool declared)
{
    Buf out = {0};
    const char *p = vs_src;
    const char *main_at = NULL;
    char decl[512];

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
        goto fail;

    if (!bufAddN(&out, p, (size_t)(main_at - p)) ||
        !bufAdd(&out, "mglVsBody") ||
        !bufAdd(&out, main_at + 4))
        goto fail;

    // a variable index needs a sized array, and an application that left the
    // size implicit has not given us one
    if (!declared)
    {
        char redecl[128];

        snprintf(redecl, sizeof(redecl), "\nout float gl_CullDistance[%d];\n", n);

        if (!bufAdd(&out, redecl))
            goto fail;
    }

    snprintf(decl, sizeof(decl),
             "\nlayout(std430, binding = %d) buffer MglCullB { float mglCullOut[]; };\n"
             "void main()\n"
             "{\n"
             "  for (int i = 0; i < %d; i++) gl_CullDistance[i] = 1.0;\n"
             "  mglVsBody();\n"
             "  for (int i = 0; i < %d; i++) mglCullOut[gl_VertexID * %d + i] = gl_CullDistance[i];\n"
             "}\n",
             MGL_CULL_FIRST_BINDING, n, n, n);

    if (!bufAdd(&out, decl))
        goto fail;

    return out.s;

fail:
    free(out.s);

    return NULL;
}

// One thread per primitive. A primitive survives unless some cull distance is
// negative at every one of its vertices.
static char *buildKernel(GLint n)
{
    Buf out = {0};
    char src[4096];

    snprintf(src, sizeof(src),
        "#version 430 core\n"
        "layout(local_size_x = 64) in;\n"
        "layout(std430, binding = %d) readonly buffer MglCullB { float mglCull[]; };\n"
        "layout(std430, binding = %d) readonly buffer MglCullSrcB { uint mglSrcIdx[]; };\n"
        "layout(std430, binding = %d) writeonly buffer MglCullIdxB { uint mglOutIdx[]; };\n"
        "layout(std430, binding = %d) buffer MglCullArgB { uint mglArgs[]; };\n"
        "layout(std430, binding = %d) readonly buffer MglCullCfgB { int mglCfg[8]; };\n"
        "\n"
        "// 0 primitives, 1 vertices each, 2 indexed, 3 first, 4 topology, 5 count\n"
        "void main()\n"
        "{\n"
        "  int prims = mglCfg[0];\n"
        "  int p = int(gl_GlobalInvocationID.x);\n"
        "  if (p >= prims) return;\n"
        "  int vpp = mglCfg[1];\n"
        "  int indexed = mglCfg[2];\n"
        "  int first = mglCfg[3];\n"
        "  int topo = mglCfg[4];\n"
        "  int total = mglCfg[5];\n"
        "  int slot[3];\n"
        "  if (topo == 0) { for (int v = 0; v < vpp; v++) slot[v] = p * vpp + v; }\n"
        "  else if (topo == 1) {\n"
        "    for (int v = 0; v < vpp; v++) slot[v] = p + v;\n"
        "    // a strip alternates winding, and the index list has to keep it\n"
        "    if (vpp == 3 && (p & 1) != 0) { int t = slot[0]; slot[0] = slot[1]; slot[1] = t; }\n"
        "  }\n"
        "  else if (topo == 2) { slot[0] = 0; slot[1] = p + 1; slot[2] = p + 2; }\n"
        "  else { slot[0] = p; slot[1] = (p + 1) %% total; }\n"
        "  uint idx[3];\n"
        "  for (int v = 0; v < vpp; v++)\n"
        "    idx[v] = (indexed != 0) ? mglSrcIdx[slot[v]] : uint(first + slot[v]);\n"
        "  for (int c = 0; c < %d; c++)\n"
        "  {\n"
        "    bool all_negative = true;\n"
        "    for (int v = 0; v < vpp; v++)\n"
        "      if (mglCull[int(idx[v]) * %d + c] >= 0.0) { all_negative = false; break; }\n"
        "    if (all_negative) return;\n"
        "  }\n"
        "  uint at = atomicAdd(mglArgs[0], uint(vpp));\n"
        "  for (int v = 0; v < vpp; v++) mglOutIdx[int(at) + v] = idx[v];\n"
        "}\n",
        MGL_CULL_FIRST_BINDING, MGL_CULL_FIRST_BINDING + 1, MGL_CULL_FIRST_BINDING + 2,
        MGL_CULL_FIRST_BINDING + 3, MGL_CULL_FIRST_BINDING + 4, n, n);

    if (!bufAdd(&out, src))
    {
        free(out.s);
        return NULL;
    }

    return out.s;
}

bool mglBuildCullShaders(const char *vs_src, CullInfo *ci)
{
    GLint n = mglCullDistanceSize(vs_src);

    mglFreeCullInfo(ci);

    if (n <= 0)
        return false;

    ci->count = n;
    ci->capture_src = buildCapture(vs_src, n, cull_was_declared);
    ci->kernel_src = buildKernel(n);

    if (ci->capture_src == NULL || ci->kernel_src == NULL)
    {
        mglFreeCullInfo(ci);
        return false;
    }

    ci->cap_out_slot = -1;
    ci->k_cull_slot = ci->k_src_slot = ci->k_out_slot = -1;
    ci->k_arg_slot = ci->k_cfg_slot = -1;

    return true;
}

void mglFreeCullInfo(CullInfo *ci)
{
    if (ci == NULL)
        return;

    free(ci->capture_src);
    free(ci->kernel_src);

    memset(ci, 0, sizeof(*ci));
}
