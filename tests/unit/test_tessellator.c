/*
 * test_tessellator.c
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
 */

#include <math.h>
#include <stdlib.h>
#include "mgl_test.h"
#include "glm_context.h"

static int cmpFloat(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;

    return x < y ? -1 : x > y;
}

// The first inner ring of a fractional quad is cut at inner - 2, with its own
// short end segments, not at the full-length cuts of the level outside it.
TEST(tessellator, first_inner_ring_has_its_own_fractional_cuts)
{
    static MglTessCoord c[20000];
    static uint32_t idx[60000];
    static float u[256];
    const float outer[4] = { 1, 32, 1, 32 }, inner[2] = { 32, 64 };
    float vmin = 1.0f;
    int prims, n, k = 0, longer = 0, shorter = 0;

    n = mglTessellate(1, 2, false, false, outer, inner, c, 20000, idx, 60000, &prims);
    CHECK(n > 0);

    for (int i = 0; i < n; i++)
        if (c[i].v > 0.0f && c[i].v < vmin)
            vmin = c[i].v;

    for (int i = 0; i < n && k < 256; i++)
        if (c[i].v == vmin)
            u[k++] = c[i].u;

    qsort(u, (size_t)k, sizeof *u, cmpFloat);

    // 33 cuts the edge, so the ring is 31 segments: 29 full and 2 short
    CHECK_MSG(k == 32, "ring has %d points, want 32", k);

    float full = u[k / 2] - u[k / 2 - 1];

    for (int i = 1; i < k; i++)
    {
        float d = u[i] - u[i - 1];

        longer += fabsf(d - full) < 1e-5f;
        shorter += d < full - 1e-5f;
    }

    CHECK_MSG(longer == 29 && shorter == 2, "%d full and %d short segments, want 29 and 2", longer, shorter);
}

// An inner level of 1 under fractional odd spacing is 1 + epsilon: the inner
// region reaches u = 0 and u = 1, so a triangle spans the whole width.
TEST(tessellator, odd_inner_level_one_spans_the_width)
{
    static MglTessCoord c[4096];
    static uint32_t idx[12288];
    const float outer[4] = { 4, 4, 4, 4 }, inner[2] = { 1, 4 };
    float top = 1.0f, second = 0.0f;
    int prims, n, found = 0;

    n = mglTessellate(1, 2, false, false, outer, inner, c, 4096, idx, 12288, &prims);
    CHECK(n > 0);

    // the highest v below the top edge
    for (int i = 0; i < n; i++)
        if (c[i].v < top && c[i].v > second)
            second = c[i].v;

    for (int p = 0; p < prims; p++)
    {
        const MglTessCoord *a = &c[idx[p * 3]], *b = &c[idx[p * 3 + 1]], *d = &c[idx[p * 3 + 2]];
        const MglTessCoord *v[3] = { a, b, d };

        for (int k = 0; k < 3; k++)
        {
            const MglTessCoord *x = v[k], *y = v[(k + 1) % 3], *z = v[(k + 2) % 3];

            if (x->v == second && y->v == second && z->v == top &&
                ((x->u == 0.0f && y->u == 1.0f) || (x->u == 1.0f && y->u == 0.0f)))
                found = 1;
        }
    }

    CHECK_MSG(found, "no triangle spans u 0 to 1 at v %g", second);
}
