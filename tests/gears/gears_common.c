/*
 * gears_common.c
 * MGL
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gears_common.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------- mesh building ---------- */

static void push(GearVertex **arr, size_t *count, size_t *cap, GearVertex v)
{
    if (*count == *cap)
    {
        size_t n = *cap ? *cap * 2 : 256;
        GearVertex *p = (GearVertex *)realloc(*arr, n * sizeof(GearVertex));

        if (!p) return;

        *arr = p;
        *cap = n;
    }

    (*arr)[(*count)++] = v;
}

void gears_mesh_init(GearMesh *m)
{
    memset(m, 0, sizeof *m);
    m->cur_nz = 1.0f;
}

void gears_mesh_free(GearMesh *m)
{
    free(m->verts);
    free(m->scratch);
    memset(m, 0, sizeof *m);
}

void gears_begin(GearMesh *m, int mode)
{
    m->mode = mode;
    m->scratch_count = 0;
}

void gears_normal(GearMesh *m, float x, float y, float z)
{
    m->cur_nx = x; m->cur_ny = y; m->cur_nz = z;
}

void gears_vertex(GearMesh *m, float x, float y, float z)
{
    GearVertex v;

    v.px = x; v.py = y; v.pz = z;
    v.nx = m->cur_nx; v.ny = m->cur_ny; v.nz = m->cur_nz;

    push(&m->scratch, &m->scratch_count, &m->scratch_cap, v);
}

static void tri(GearMesh *m, size_t a, size_t b, size_t c)
{
    push(&m->verts, &m->count, &m->cap, m->scratch[a]);
    push(&m->verts, &m->count, &m->cap, m->scratch[b]);
    push(&m->verts, &m->count, &m->cap, m->scratch[c]);
}

void gears_end(GearMesh *m)
{
    size_t n = m->scratch_count;

    if (m->mode == GEARS_QUADS)
    {
        for (size_t i = 0; i + 3 < n; i += 4)
        {
            tri(m, i, i + 1, i + 2);
            tri(m, i, i + 2, i + 3);
        }
    }
    else // GEARS_QUAD_STRIP
    {
        // quad k is made from 2k, 2k+1, 2k+3, 2k+2, which is what GL does
        for (size_t k = 0; 2 * k + 3 < n; k++)
        {
            size_t a = 2 * k, b = 2 * k + 1, c = 2 * k + 3, d = 2 * k + 2;

            tri(m, a, b, c);
            tri(m, a, c, d);
        }
    }

    m->scratch_count = 0;
}

/* The body below follows Brian Paul's original gear() line for line, with the
   glBegin/glNormal3f/glVertex3f calls swapped for the emitter above. */
void gears_build(GearMesh *m, float inner_radius, float outer_radius,
                 float width, int teeth, float tooth_depth)
{
    int i;
    float r0, r1, r2;
    float angle, da;
    float u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0f;
    r2 = outer_radius + tooth_depth / 2.0f;

    da = 2.0f * (float)M_PI / teeth / 4.0f;

    gears_normal(m, 0.0f, 0.0f, 1.0f);

    /* draw front face */
    gears_begin(m, GEARS_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.0f * (float)M_PI / teeth;
        gears_vertex(m, r0 * cosf(angle), r0 * sinf(angle), width * 0.5f);
        gears_vertex(m, r1 * cosf(angle), r1 * sinf(angle), width * 0.5f);
        if (i < teeth) {
            gears_vertex(m, r0 * cosf(angle), r0 * sinf(angle), width * 0.5f);
            gears_vertex(m, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da),
                         width * 0.5f);
        }
    }
    gears_end(m);

    /* draw front sides of teeth */
    gears_begin(m, GEARS_QUADS);
    da = 2.0f * (float)M_PI / teeth / 4.0f;
    for (i = 0; i < teeth; i++) {
        angle = i * 2.0f * (float)M_PI / teeth;

        gears_vertex(m, r1 * cosf(angle), r1 * sinf(angle), width * 0.5f);
        gears_vertex(m, r2 * cosf(angle + da), r2 * sinf(angle + da), width * 0.5f);
        gears_vertex(m, r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da),
                     width * 0.5f);
        gears_vertex(m, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da),
                     width * 0.5f);
    }
    gears_end(m);

    gears_normal(m, 0.0f, 0.0f, -1.0f);

    /* draw back face */
    gears_begin(m, GEARS_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.0f * (float)M_PI / teeth;
        gears_vertex(m, r1 * cosf(angle), r1 * sinf(angle), -width * 0.5f);
        gears_vertex(m, r0 * cosf(angle), r0 * sinf(angle), -width * 0.5f);
        if (i < teeth) {
            gears_vertex(m, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da),
                         -width * 0.5f);
            gears_vertex(m, r0 * cosf(angle), r0 * sinf(angle), -width * 0.5f);
        }
    }
    gears_end(m);

    /* draw back sides of teeth */
    gears_begin(m, GEARS_QUADS);
    da = 2.0f * (float)M_PI / teeth / 4.0f;
    for (i = 0; i < teeth; i++) {
        angle = i * 2.0f * (float)M_PI / teeth;

        gears_vertex(m, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da),
                     -width * 0.5f);
        gears_vertex(m, r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da),
                     -width * 0.5f);
        gears_vertex(m, r2 * cosf(angle + da), r2 * sinf(angle + da), -width * 0.5f);
        gears_vertex(m, r1 * cosf(angle), r1 * sinf(angle), -width * 0.5f);
    }
    gears_end(m);

    /* draw outward faces of teeth */
    gears_begin(m, GEARS_QUAD_STRIP);
    for (i = 0; i < teeth; i++) {
        angle = i * 2.0f * (float)M_PI / teeth;

        gears_vertex(m, r1 * cosf(angle), r1 * sinf(angle), width * 0.5f);
        gears_vertex(m, r1 * cosf(angle), r1 * sinf(angle), -width * 0.5f);
        u = r2 * cosf(angle + da) - r1 * cosf(angle);
        v = r2 * sinf(angle + da) - r1 * sinf(angle);
        len = sqrtf(u * u + v * v);
        u /= len;
        v /= len;
        gears_normal(m, v, -u, 0.0f);
        gears_vertex(m, r2 * cosf(angle + da), r2 * sinf(angle + da), width * 0.5f);
        gears_vertex(m, r2 * cosf(angle + da), r2 * sinf(angle + da), -width * 0.5f);
        gears_normal(m, cosf(angle), sinf(angle), 0.0f);
        gears_vertex(m, r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da),
                     width * 0.5f);
        gears_vertex(m, r2 * cosf(angle + 2 * da), r2 * sinf(angle + 2 * da),
                     -width * 0.5f);
        u = r1 * cosf(angle + 3 * da) - r2 * cosf(angle + 2 * da);
        v = r1 * sinf(angle + 3 * da) - r2 * sinf(angle + 2 * da);
        gears_normal(m, v, -u, 0.0f);
        gears_vertex(m, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da),
                     width * 0.5f);
        gears_vertex(m, r1 * cosf(angle + 3 * da), r1 * sinf(angle + 3 * da),
                     -width * 0.5f);
        gears_normal(m, cosf(angle), sinf(angle), 0.0f);
    }

    gears_vertex(m, r1 * cosf(0.0f), r1 * sinf(0.0f), width * 0.5f);
    gears_vertex(m, r1 * cosf(0.0f), r1 * sinf(0.0f), -width * 0.5f);
    gears_end(m);

    /* draw inside radius cylinder */
    gears_begin(m, GEARS_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        angle = i * 2.0f * (float)M_PI / teeth;
        gears_normal(m, -cosf(angle), -sinf(angle), 0.0f);
        gears_vertex(m, r0 * cosf(angle), r0 * sinf(angle), -width * 0.5f);
        gears_vertex(m, r0 * cosf(angle), r0 * sinf(angle), width * 0.5f);
    }
    gears_end(m);
}

/* ---------- matrices ---------- */

void mat4_identity(float *m)
{
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void mat4_copy(float *dst, const float *src)
{
    memcpy(dst, src, 16 * sizeof(float));
}

void mat4_mul(float *out, const float *a, const float *b)
{
    float t[16];

    for (int c = 0; c < 4; c++)
    {
        for (int r = 0; r < 4; r++)
        {
            t[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0]
                         + a[1 * 4 + r] * b[c * 4 + 1]
                         + a[2 * 4 + r] * b[c * 4 + 2]
                         + a[3 * 4 + r] * b[c * 4 + 3];
        }
    }

    memcpy(out, t, sizeof t);
}

void mat4_frustum(float *m, float l, float r, float b, float t, float n, float f)
{
    memset(m, 0, 16 * sizeof(float));

    m[0]  = 2.0f * n / (r - l);
    m[5]  = 2.0f * n / (t - b);
    m[8]  = (r + l) / (r - l);
    m[9]  = (t + b) / (t - b);
    m[10] = -(f + n) / (f - n);
    m[11] = -1.0f;
    m[14] = -2.0f * f * n / (f - n);
}

void mat4_translate(float *m, float x, float y, float z)
{
    float tm[16];

    mat4_identity(tm);
    tm[12] = x; tm[13] = y; tm[14] = z;

    mat4_mul(m, m, tm);
}

void mat4_rotate(float *m, float deg, float x, float y, float z)
{
    float rad = deg * (float)M_PI / 180.0f;
    float c = cosf(rad), s = sinf(rad);
    float len = sqrtf(x * x + y * y + z * z);
    float rm[16];

    if (len == 0.0f) return;

    x /= len; y /= len; z /= len;

    mat4_identity(rm);

    rm[0]  = x * x * (1 - c) + c;
    rm[1]  = y * x * (1 - c) + z * s;
    rm[2]  = x * z * (1 - c) - y * s;

    rm[4]  = x * y * (1 - c) - z * s;
    rm[5]  = y * y * (1 - c) + c;
    rm[6]  = y * z * (1 - c) + x * s;

    rm[8]  = x * z * (1 - c) + y * s;
    rm[9]  = y * z * (1 - c) - x * s;
    rm[10] = z * z * (1 - c) + c;

    mat4_mul(m, m, rm);
}

void mat4_normal_matrix(float *out, const float *mv)
{
    // inverse transpose of the upper-left 3x3
    float a = mv[0], b = mv[4], c = mv[8];
    float d = mv[1], e = mv[5], f = mv[9];
    float g = mv[2], h = mv[6], i = mv[10];

    float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);

    mat4_identity(out);

    if (det == 0.0f)
        return;

    float inv = 1.0f / det;

    // inverse transpose is just the cofactor matrix over the determinant
    out[0] = (e * i - f * h) * inv;   // C00
    out[4] = (f * g - d * i) * inv;   // C01
    out[8] = (d * h - e * g) * inv;   // C02

    out[1] = (c * h - b * i) * inv;   // C10
    out[5] = (a * i - c * g) * inv;   // C11
    out[9] = (b * g - a * h) * inv;   // C12

    out[2] = (b * f - c * e) * inv;   // C20
    out[6] = (c * d - a * f) * inv;   // C21
    out[10] = (a * e - b * d) * inv;  // C22
}
