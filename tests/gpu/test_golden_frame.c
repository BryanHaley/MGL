/*
 * test_golden_frame.c
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
 * The end-to-end golden-frame test (roadmap item 27).
 *
 * Draws one known scene into a fixed 64x64 RGBA8 target, reads the whole
 * framebuffer back and compares it pixel by pixel against a stored golden
 * image in tests/golden_images/golden_end_to_end.tga.  The comparison allows
 * one unit of error per channel so a different GPU may still pass; alpha is
 * ignored.  Nothing else in the suite covers the full path -- vertex input,
 * a texture upload, a sampled draw and a framebuffer readback -- in one pass.
 *
 * The scene:
 *   - clear to blue (0.2, 0.2, 0.8, 1.0)
 *   - a per-vertex coloured triangle on the left
 *   - a textured quad on the right, sampling an 8x8 checkerboard uploaded
 *     at run time
 *   Both use one program with a vertex and a fragment shader and an integer
 *   uniform that switches the fragment shader between the vertex colour and
 *   the texture.
 *
 * Every edge in the scene is placed on a pixel boundary, so no fragment
 * centre lies on an edge and coverage is unambiguous.  That keeps the image
 * stable across rasterisers instead of resting on a tie-break rule.
 *
 * First run: if the golden image is missing the test draws the scene, writes
 * the image and skips, telling you to run it again.  Later runs compare.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "mgl_test.h"
#include "harness.h"

#define GOLDEN_SIZE 64
#define GOLDEN_TOL  1

/* ---------- golden image location ---------- */

static const char *kGoldenDefault = "tests/golden_images/golden_end_to_end.tga";

static int path_exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0;
}

static int dir_exists(const char *p)
{
    struct stat st;
    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

/* Prefers an override, then a path relative to the repo root, then one
 * relative to a run started from build/. */
static const char *golden_path(void)
{
    static char buf[1024];
    const char *env = getenv("MGL_GOLDEN_IMAGE");

    if (env && *env)
    {
        snprintf(buf, sizeof buf, "%s", env);
        return buf;
    }

    if (dir_exists("tests/golden_images") || path_exists(kGoldenDefault))
    {
        snprintf(buf, sizeof buf, "%s", kGoldenDefault);
        return buf;
    }

    if (dir_exists("../tests/golden_images"))
    {
        snprintf(buf, sizeof buf, "%s", "../tests/golden_images/golden_end_to_end.tga");
        return buf;
    }

    snprintf(buf, sizeof buf, "%s", kGoldenDefault);
    return buf;
}

/* ---------- uncompressed TGA, 24-bit BGR ---------- */

static int tga_write_rgb(const char *path, const unsigned char *rgba, int w, int h)
{
    unsigned char hdr[18];
    FILE *f;

    /* The directory may not exist on a fresh checkout. */
    {
        char dir[1024];
        const char *slash;

        snprintf(dir, sizeof dir, "%s", path);
        slash = strrchr(dir, '/');

        if (slash)
        {
            *((char *)slash) = 0;
            if (*dir && !dir_exists(dir))
                mkdir(dir, 0755);
        }
    }

    f = fopen(path, "wb");
    if (!f)
        return 0;

    memset(hdr, 0, sizeof hdr);
    hdr[2]  = 2;                              /* uncompressed true colour */
    hdr[12] = (unsigned char)(w & 0xFF);
    hdr[13] = (unsigned char)((w >> 8) & 0xFF);
    hdr[14] = (unsigned char)(h & 0xFF);
    hdr[15] = (unsigned char)((h >> 8) & 0xFF);
    hdr[16] = 24;
    hdr[17] = 0x00;                           /* bottom-left origin */

    if (fwrite(hdr, 1, sizeof hdr, f) != sizeof hdr)
    {
        fclose(f);
        return 0;
    }

    /* glReadPixels hands the rows back bottom-up, which is this TGA's order. */
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            const unsigned char *p = rgba + ((size_t)y * (size_t)w + (size_t)x) * 4;
            unsigned char bgr[3] = { p[2], p[1], p[0] };

            if (fwrite(bgr, 1, 3, f) != 3)
            {
                fclose(f);
                return 0;
            }
        }
    }

    fclose(f);
    return 1;
}

/* Returns an RGBA8 buffer in glReadPixels order (bottom row first). */
static int tga_read_rgb(const char *path, unsigned char **out_rgba, int *out_w, int *out_h)
{
    unsigned char hdr[18];
    unsigned char *rgba;
    int idlen, cmap, type, w, h, bpp, desc;
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;

    if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr)
    {
        fclose(f);
        return 0;
    }

    idlen = hdr[0];
    cmap  = hdr[1];
    type  = hdr[2];
    w     = hdr[12] | (hdr[13] << 8);
    h     = hdr[14] | (hdr[15] << 8);
    bpp   = hdr[16];
    desc  = hdr[17];

    if (type != 2 || cmap != 0 || bpp != 24 || w <= 0 || h <= 0)
    {
        fclose(f);
        return 0;
    }

    if (idlen && fseek(f, idlen, SEEK_CUR) != 0)
    {
        fclose(f);
        return 0;
    }

    rgba = (unsigned char *)malloc((size_t)w * (size_t)h * 4);
    if (!rgba)
    {
        fclose(f);
        return 0;
    }

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            unsigned char bgr[3];
            int row = (desc & 0x20) ? (h - 1 - y) : y;   /* 0x20 = top-left origin */
            unsigned char *p = rgba + ((size_t)row * (size_t)w + (size_t)x) * 4;

            if (fread(bgr, 1, 3, f) != 3)
            {
                free(rgba);
                fclose(f);
                return 0;
            }

            p[0] = bgr[2];
            p[1] = bgr[1];
            p[2] = bgr[0];
            p[3] = 255;
        }
    }

    fclose(f);

    *out_rgba = rgba;
    *out_w = w;
    *out_h = h;
    return 1;
}

/* ---------- the scene ---------- */

static const char *kVS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "layout(location = 1) in vec3 aColor;\n"
    "layout(location = 2) in vec2 aUV;\n"
    "out vec3 vColor;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "    vColor = aColor;\n"
    "    vUV = aUV;\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char *kFS =
    "#version 460 core\n"
    "in vec3 vColor;\n"
    "in vec2 vUV;\n"
    "uniform sampler2D uTex;\n"
    "uniform int uUseTex;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "    if (uUseTex != 0)\n"
    "        frag = texture(uTex, vUV);\n"
    "    else\n"
    "        frag = vec4(vColor, 1.0);\n"
    "}\n";

typedef struct {
    GLuint prog, vao, vbo, tex;
} GoldenRig;

/* pos.xy, colour.rgb, uv.xy -- 7 floats per vertex */
static const float kVertices[] = {
    /* triangle: red, blue, green */
    -0.50f, -0.50f,   1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
     0.00f, -0.50f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
    -0.50f,  0.50f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
    /* quad: two triangles over the right half */
     0.00f, -0.625f,  0.0f, 0.0f, 0.0f,   0.0f, 0.0f,
     0.75f, -0.625f,  0.0f, 0.0f, 0.0f,   1.0f, 0.0f,
     0.75f,  0.625f,  0.0f, 0.0f, 0.0f,   1.0f, 1.0f,
     0.00f, -0.625f,  0.0f, 0.0f, 0.0f,   0.0f, 0.0f,
     0.75f,  0.625f,  0.0f, 0.0f, 0.0f,   1.0f, 1.0f,
     0.00f,  0.625f,  0.0f, 0.0f, 0.0f,   0.0f, 1.0f,
};

static int rig_build(GoldenRig *r)
{
    unsigned char checker[8 * 8 * 4];
    char log[2048] = { 0 };
    GLuint loc_tex, loc_use;

    memset(r, 0, sizeof *r);

    r->prog = mgl_build_program(kVS, kFS, log, sizeof log);
    if (!r->prog)
        return 0;

    glGenVertexArrays(1, &r->vao);
    glBindVertexArray(r->vao);
    glGenBuffers(1, &r->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof kVertices, kVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void *)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    /* An 8x8 checkerboard, white where (x + y) is even. */
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            unsigned char *p = checker + (y * 8 + x) * 4;
            unsigned char v = ((x + y) & 1) ? 0 : 255;

            p[0] = v; p[1] = v; p[2] = v; p[3] = 255;
        }
    }

    glGenTextures(1, &r->tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUseProgram(r->prog);
    loc_tex = glGetUniformLocation(r->prog, "uTex");
    loc_use = glGetUniformLocation(r->prog, "uUseTex");
    glUniform1i(loc_tex, 0);
    glUniform1i(loc_use, 0);
    glUseProgram(0);

    return 1;
}

static void rig_destroy(GoldenRig *r)
{
    if (r->tex)  glDeleteTextures(1, &r->tex);
    if (r->vbo)  glDeleteBuffers(1, &r->vbo);
    if (r->vao)  glDeleteVertexArrays(1, &r->vao);
    if (r->prog) glDeleteProgram(r->prog);

    memset(r, 0, sizeof *r);
}

static void scene_draw(const GoldenRig *r)
{
    GLuint loc_use = glGetUniformLocation(r->prog, "uUseTex");

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, GOLDEN_SIZE, GOLDEN_SIZE);

    glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(r->prog);
    glBindVertexArray(r->vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->tex);

    /* the coloured triangle, no geometry shader in sight */
    glUniform1i(loc_use, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    /* the textured quad */
    glUniform1i(loc_use, 1);
    glDrawArrays(GL_TRIANGLES, 3, 6);

    glUseProgram(0);
}

/* ---------- the test ---------- */

GPU_TEST(golden_frame, end_to_end)
{
    const char *path = golden_path();
    MGLTestTarget t;
    GoldenRig rig = { 0 };
    unsigned char *px = NULL, *golden = NULL;
    int gw = 0, gh = 0;
    int mismatches = 0, first_x = 0, first_y = 0;
    unsigned char got[4] = { 0 }, want[4] = { 0 };

    if (!mgl_target_create(&t, GOLDEN_SIZE, GOLDEN_SIZE, GL_RGBA8, 0))
        SKIP("could not create the 64x64 RGBA8 target");

    if (!rig_build(&rig))
    {
        rig_destroy(&rig);
        mgl_target_destroy(&t);
        SKIP("golden-frame programs did not build");
    }

    mgl_target_bind(&t);
    scene_draw(&rig);

    if (mgl_drain_errors() != GL_NO_ERROR)
    {
        rig_destroy(&rig);
        mgl_target_destroy(&t);
        CHECK_MSG(0, "the golden-frame scene raised a GL error");
        return;
    }

    px = mgl_read_rgba8(&t);
    if (!px)
    {
        rig_destroy(&rig);
        mgl_target_destroy(&t);
        SKIP("could not read the framebuffer back");
    }

    rig_destroy(&rig);
    mgl_target_destroy(&t);

    /* First run on a fresh checkout: draw, store and skip. A failed write is
     * a failure, not a skip, or a read-only checkout would pass silently. */
    if (!path_exists(path))
    {
        int ok = tga_write_rgb(path, px, GOLDEN_SIZE, GOLDEN_SIZE);

        free(px);

        if (!ok)
        {
            CHECK_MSG(0, "could not write the golden image to %s", path);
            return;
        }

        SKIP("golden image generated, re-run to verify");
    }

    if (!tga_read_rgb(path, &golden, &gw, &gh))
    {
        free(px);
        SKIP("could not read the golden image");
    }

    CHECK_EQ_UINT((unsigned)gw, GOLDEN_SIZE);
    CHECK_EQ_UINT((unsigned)gh, GOLDEN_SIZE);

    if (gw != GOLDEN_SIZE || gh != GOLDEN_SIZE)
    {
        free(px);
        free(golden);
        return;
    }

    for (int y = 0; y < GOLDEN_SIZE; y++)
    {
        for (int x = 0; x < GOLDEN_SIZE; x++)
        {
            const unsigned char *a = px + ((size_t)y * GOLDEN_SIZE + (size_t)x) * 4;
            const unsigned char *e = golden + ((size_t)y * GOLDEN_SIZE + (size_t)x) * 4;
            int diff = 0;

            /* alpha is ignored: the scene writes 1.0 everywhere anyway */
            for (int c = 0; c < 3; c++)
            {
                int d = (int)a[c] - (int)e[c];

                if (d < 0) d = -d;
                if (d > diff) diff = d;
            }

            if (diff > GOLDEN_TOL)
            {
                if (mismatches == 0)
                {
                    first_x = x;
                    first_y = y;
                    memcpy(got, a, 4);
                    memcpy(want, e, 4);
                }

                mismatches++;
            }
        }
    }

    CHECK_MSG(mismatches == 0,
              "%d of %d pixels differ by more than %d against %s; first at (%d,%d): "
              "got (%d,%d,%d,%d), want (%d,%d,%d,%d)",
              mismatches, GOLDEN_SIZE * GOLDEN_SIZE, GOLDEN_TOL, path,
              first_x, first_y,
              got[0], got[1], got[2], got[3],
              want[0], want[1], want[2], want[3]);

    free(px);
    free(golden);
}
