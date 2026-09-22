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
 * tessellator.c
 * MGL
 *
 * GL's fixed-function tessellator (GL 4.6 section 11.2.2) on the CPU. Metal's
 * tessellator draws, but never says which triangles it made or in what order,
 * and transform feedback and the geometry stage both need exactly that. So
 * the patch is cut up here, the evaluation shader is run once per vertex, and
 * the primitives are put together from the list this makes.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "glm_context.h"

typedef struct {
    MglTessCoord *coords;
    int count, max;
    uint32_t *index;
    int index_count, index_max;
    bool cw, overflow;
} Out;

static int addVertex(Out *o, float u, float v, float w)
{
    if (o->count >= o->max)
    {
        o->overflow = true;
        return 0;
    }

    o->coords[o->count] = (MglTessCoord){ u, v, w };
    return o->count++;
}

// (u, v) as a flat plane; counter-clockwise there is what "ccw" means
static void addTriangle(Out *o, int a, int b, int c)
{
    const MglTessCoord *p = o->coords;
    float area = (p[b].u - p[a].u) * (p[c].v - p[a].v) - (p[c].u - p[a].u) * (p[b].v - p[a].v);

    if (o->index_count + 3 > o->index_max)
    {
        o->overflow = true;
        return;
    }

    if ((area < 0) != o->cw)
    {
        int t = b;
        b = c;
        c = t;
    }

    o->index[o->index_count++] = (uint32_t)a;
    o->index[o->index_count++] = (uint32_t)b;
    o->index[o->index_count++] = (uint32_t)c;
}

// How many segments an edge gets, and where each cut falls along it.
static int subdivide(float level, int spacing, float *t)
{
    float f;
    int n;

    if (!(level == level))
        level = 1.0f;

    switch (spacing)
    {
        case 1:     // fractional even
            f = fminf(fmaxf(level, 2.0f), (float)MGL_TES_MAX_LEVEL);
            n = (int)ceilf(f);
            n += n & 1;
            break;

        case 2:     // fractional odd
            f = fminf(fmaxf(level, 1.0f), (float)(MGL_TES_MAX_LEVEL - 1));
            n = (int)ceilf(f);
            n += !(n & 1);
            break;

        default:
            f = fminf(fmaxf(level, 1.0f), (float)MGL_TES_MAX_LEVEL);
            n = (int)ceilf(f);
            f = (float)n;
            break;
    }

    // n - 2 segments of length 1/f and two shorter ones, one at each end. The
    // far half mirrors the near one exactly, so a cut at x on one edge and
    // 1 - x on another are the same number, as GL's invariance rules want.
    bool even = n <= 2 || f >= (float)n;
    float full = even ? 1.0f / (float)n : 1.0f / f;
    float part = even ? full : (1.0f - (float)(n - 2) * full) * 0.5f;

    for (int i = 0; i <= n / 2; i++)
    {
        if (even)
            t[i] = (float)i / (float)n;
        else
            t[i] = i == 0 ? 0.0f : part + (float)(i - 1) * full;
    }

    for (int i = n / 2 + 1; i <= n; i++)
        t[i] = 1.0f - t[n - i];

    return n;
}

// The inner level GL uses when the one given would leave the patch uncut but
// an outer edge is still subdivided: treated as 1 + epsilon.
static float innerAtLeastTwo(float level, int spacing)
{
    if (level > 1.0f)
        return level;

    return spacing == 2 ? 3.0f : 2.0f;
}

// Triangles between two rows of vertices along one side of a ring: the outer
// row a (m segments) and the inner row b (p segments), each with its
// position along the side from 0 to 1.
static void stitch(Out *o, const int *a, const float *pa, int m, const int *b, const float *pb, int p)
{
    int i = 0, j = 0;

    while (i < m || j < p)
    {
        bool step_a = j == p || (i < m && (pa[i] + pa[i + 1]) <= (pb[j] + pb[j + 1]));

        if (step_a)
        {
            addTriangle(o, a[i], a[i + 1], b[j]);
            i++;
        }
        else
        {
            addTriangle(o, a[i], b[j + 1], b[j]);
            j++;
        }
    }
}

#define SIDE_MAX (MGL_TES_MAX_LEVEL + 2)

static void tessTriangles(Out *o, int spacing, const float *outer, const float *inner)
{
    float t_in[SIDE_MAX], t_out[3][SIDE_MAX];
    int n_out[3];

    for (int e = 0; e < 3; e++)
        n_out[e] = subdivide(outer[e], spacing, t_out[e]);

    int n_in = subdivide(inner[0], spacing, t_in);

    if (n_in == 1 && n_out[0] == 1 && n_out[1] == 1 && n_out[2] == 1)
    {
        int a = addVertex(o, 1, 0, 0), b = addVertex(o, 0, 1, 0), c = addVertex(o, 0, 0, 1);

        addTriangle(o, a, b, c);
        return;
    }

    if (n_in == 1)
        n_in = subdivide(innerAtLeastTwo(inner[0], spacing), spacing, t_in);

    // corners P=(0,0,1), Q=(0,1,0), R=(1,0,0); outer[0] runs P->Q (u=0),
    // outer[2] Q->R (w=0), outer[1] R->P (v=0)
    static const float corner[3][3] = { { 0, 0, 1 }, { 0, 1, 0 }, { 1, 0, 0 } };
    static const int edge_level[3] = { 0, 2, 1 };

    int ring[3][SIDE_MAX];
    float ring_t[3][SIDE_MAX];
    int ring_n[3];

    for (int s = 0; s < 3; s++)
    {
        const float *from = corner[s], *to = corner[(s + 1) % 3];
        int e = edge_level[s];
        int n = n_out[e];

        ring_n[s] = n;

        for (int k = 0; k <= n; k++)
        {
            float t = t_out[e][k];

            ring_t[s][k] = t;

            // straight from the table: the coordinate that grows along the
            // side is t[k], the one that shrinks t[n-k], the third zero
            float a = t, b = t_out[e][n - k];

            (void)from;
            (void)to;

            if (k == n)
                ring[s][k] = -1;    // the next side's first vertex, below
            else if (s == 0)
                ring[s][k] = addVertex(o, 0, a, b);
            else if (s == 1)
                ring[s][k] = addVertex(o, a, b, 0);
            else
                ring[s][k] = addVertex(o, b, 0, a);
        }
    }

    // each side ends where the next one starts
    for (int s = 0; s < 3; s++)
        ring[s][ring_n[s]] = ring[(s + 1) % 3][0];

    for (int k = 1;; k++)
    {
        int L = n_in - 2 * k;

        if (L < 0)
            break;

        int inner_ring[3][SIDE_MAX];
        float inner_t[3][SIDE_MAX];

        if (L == 0)
        {
            int c = addVertex(o, 1.0f / 3, 1.0f / 3, 1.0f / 3);

            for (int s = 0; s < 3; s++)
            {
                float zero[2] = { 0, 1 };
                int only[2] = { c, c };

                stitch(o, ring[s], ring_t[s], ring_n[s], only, zero, 0);
            }

            break;
        }

        // the ring's corners sit two thirds of the way in along the lines
        // through the k-th cut of the inner subdivision
        float d = (2.0f / 3.0f) * t_in[k];
        float span = t_in[n_in - k] - t_in[k];
        float c[3][3];

        for (int s = 0; s < 3; s++)
            for (int x = 0; x < 3; x++)
                c[s][x] = corner[s][x] > 0.5f ? 1.0f - 2.0f * d : d;

        int start = o->count;

        for (int s = 0; s < 3; s++)
        {
            for (int j = 0; j <= L; j++)
            {
                float t = span > 0 ? (t_in[k + j] - t_in[k]) / span : 0.0f;

                inner_t[s][j] = t;

                if (j == L)
                    inner_ring[s][j] = -1;
                else
                    inner_ring[s][j] = addVertex(o, c[s][0] + (c[(s + 1) % 3][0] - c[s][0]) * t,
                                                    c[s][1] + (c[(s + 1) % 3][1] - c[s][1]) * t,
                                                    c[s][2] + (c[(s + 1) % 3][2] - c[s][2]) * t);
            }
        }

        (void)start;

        for (int s = 0; s < 3; s++)
            inner_ring[s][L] = inner_ring[(s + 1) % 3][0];

        for (int s = 0; s < 3; s++)
            stitch(o, ring[s], ring_t[s], ring_n[s], inner_ring[s], inner_t[s], L);

        if (L == 1)
        {
            addTriangle(o, inner_ring[0][0], inner_ring[1][0], inner_ring[2][0]);
            break;
        }

        memcpy(ring, inner_ring, sizeof(ring));
        memcpy(ring_t, inner_t, sizeof(ring_t));

        for (int s = 0; s < 3; s++)
            ring_n[s] = L;
    }
}

static void tessQuads(Out *o, int spacing, const float *outer, const float *inner)
{
    float t_out[4][SIDE_MAX], t0[SIDE_MAX], t1[SIDE_MAX];
    int n_out[4];

    for (int e = 0; e < 4; e++)
        n_out[e] = subdivide(outer[e], spacing, t_out[e]);

    int n0 = subdivide(inner[0], spacing, t0);
    int n1 = subdivide(inner[1], spacing, t1);

    if (n0 == 1 && n1 == 1 && n_out[0] == 1 && n_out[1] == 1 && n_out[2] == 1 && n_out[3] == 1)
    {
        int a = addVertex(o, 0, 0, 0), b = addVertex(o, 1, 0, 0);
        int c = addVertex(o, 1, 1, 0), d = addVertex(o, 0, 1, 0);

        addTriangle(o, a, b, c);
        addTriangle(o, a, c, d);
        return;
    }

    if (n0 == 1)
        n0 = subdivide(innerAtLeastTwo(inner[0], spacing), spacing, t0);

    if (n1 == 1)
        n1 = subdivide(innerAtLeastTwo(inner[1], spacing), spacing, t1);

    // the inner grid, from the first cut to the last in each direction
    static int grid[SIDE_MAX][SIDE_MAX];

    for (int j = 1; j < n1; j++)
        for (int i = 1; i < n0; i++)
            grid[i][j] = addVertex(o, t0[i], t1[j], 0);

    for (int j = 1; j < n1 - 1; j++)
        for (int i = 1; i < n0 - 1; i++)
        {
            addTriangle(o, grid[i][j], grid[i + 1][j], grid[i + 1][j + 1]);
            addTriangle(o, grid[i][j], grid[i + 1][j + 1], grid[i][j + 1]);
        }

    // outer sides, going round: v=0 (outer[1]), u=1 (outer[2]), v=1
    // (outer[3]) and u=0 (outer[0]); corners shared with the next side
    static const float corner[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    static const int edge_level[4] = { 1, 2, 3, 0 };
    int side[4][SIDE_MAX];
    float side_t[4][SIDE_MAX];

    for (int s = 0; s < 4; s++)
    {
        int e = edge_level[s], n = n_out[e];
        const float *a = corner[s], *b = corner[(s + 1) % 4];

        (void)a;
        (void)b;

        for (int k = 0; k < n; k++)
        {
            float t = t_out[e][k], back = t_out[e][n - k];

            // from the table, never 1 - t, so opposite edges match exactly
            switch (s)
            {
                case 0:  side[s][k] = addVertex(o, t, 0, 0); break;
                case 1:  side[s][k] = addVertex(o, 1, t, 0); break;
                case 2:  side[s][k] = addVertex(o, back, 1, 0); break;
                default: side[s][k] = addVertex(o, 0, back, 0); break;
            }

            side_t[s][k] = t;
        }

        side_t[s][n] = 1.0f;
    }

    for (int s = 0; s < 4; s++)
        side[s][n_out[edge_level[s]]] = side[(s + 1) % 4][0];

    // the inner ring's sides, in the same direction, from the grid
    int h = n0 - 2, vv = n1 - 2;
    int in[4][SIDE_MAX];
    float in_t[4][SIDE_MAX];
    int in_n[4] = { h, vv, h, vv };

    for (int k = 0; k <= h; k++)
    {
        in[0][k] = grid[1 + k][1];
        in[2][k] = grid[n0 - 1 - k][n1 - 1];
        in_t[0][k] = h ? (t0[1 + k] - t0[1]) / (t0[n0 - 1] - t0[1]) : 0.0f;
        in_t[2][k] = in_t[0][k];
    }

    for (int k = 0; k <= vv; k++)
    {
        in[1][k] = grid[n0 - 1][1 + k];
        in[3][k] = grid[1][n1 - 1 - k];
        in_t[1][k] = vv ? (t1[1 + k] - t1[1]) / (t1[n1 - 1] - t1[1]) : 0.0f;
        in_t[3][k] = in_t[1][k];
    }

    // a side with no segments still has its one point; give it a length so
    // stitch() compares positions sensibly
    for (int s = 0; s < 4; s++)
        if (in_n[s] == 0)
            in_t[s][1] = 1.0f;

    for (int s = 0; s < 4; s++)
        stitch(o, side[s], side_t[s], n_out[edge_level[s]], in[s], in_t[s], in_n[s]);
}

// outer[0] lines, always equally spaced, each cut into outer[1] segments;
// v = 1 has no line
static void tessIsolines(Out *o, int spacing, const float *outer, bool point_mode)
{
    float t_lines[SIDE_MAX], t_seg[SIDE_MAX];
    int lines = subdivide(outer[0], 0, t_lines);
    int segs = subdivide(outer[1], spacing, t_seg);

    for (int y = 0; y < lines; y++)
    {
        int first = o->count;

        for (int x = 0; x <= segs; x++)
            addVertex(o, t_seg[x], t_lines[y], 0);

        if (point_mode)
            continue;

        for (int x = 0; x < segs; x++)
        {
            if (o->index_count + 2 > o->index_max)
            {
                o->overflow = true;
                return;
            }

            o->index[o->index_count++] = (uint32_t)(first + x);
            o->index[o->index_count++] = (uint32_t)(first + x + 1);
        }
    }
}

int mglTessellate(int domain, int spacing, bool point_mode, bool cw,
                  const float outer[4], const float inner[2],
                  MglTessCoord *coords, int max_coords,
                  uint32_t *index, int max_index, int *prim_count)
{
    Out o = { coords, 0, max_coords, index, 0, max_index, cw, false };
    int edges = domain == 0 ? 3 : domain == 1 ? 4 : 2;

    *prim_count = 0;

    // a patch with an outer level of zero or less, or not a number, is dropped
    for (int e = 0; e < edges; e++)
        if (!(outer[e] > 0.0f))
            return 0;

    if (domain == 0)
        tessTriangles(&o, spacing, outer, inner);
    else if (domain == 1)
        tessQuads(&o, spacing, outer, inner);
    else
        tessIsolines(&o, spacing, outer, point_mode);

    if (o.overflow)
        return -1;

    if (point_mode)
    {
        if (o.count > max_index)
            return -1;

        for (int i = 0; i < o.count; i++)
            index[i] = (uint32_t)i;

        *prim_count = o.count;
    }
    else
    {
        *prim_count = o.index_count / (domain == 2 ? 2 : 3);
    }

    return o.count;
}
