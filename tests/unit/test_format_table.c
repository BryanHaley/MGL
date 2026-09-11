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
 * test_format_table.c
 * MGL
 *
 * Format capability data transcribed from MoltenVK's MVKPixelFormats.mm
 * (Copyright (c) 2015-2026 The Brenwill Workshop Ltd., Apache 2.0).
 */

#include "mgl_test.h"
#include "mgl_format_table.h"
#include "pixel_utils.h"
#include <stdio.h>

/* ---- DIFF tracking ---- */
static int diff_count = 0;

TEST(format_table, row_counts)
{
    size_t n = mglFormatTableCount();
    printf("GL rows: %zu\n", n);
    CHECK(n > 0);

    size_t comp = 0, color = 0, depth = 0, stencil = 0, ds = 0;
    for (size_t i = 0; i < n; i++) {
        const MGLFormatDesc *d = mglFormatTableRow(i);
        switch (d->kind) {
            case MGL_FMT_COMPRESSED: comp++; break;
            case MGL_FMT_COLOR_FLOAT:
            case MGL_FMT_COLOR_INT:
            case MGL_FMT_COLOR_UINT: color++; break;
            case MGL_FMT_DEPTH: depth++; break;
            case MGL_FMT_STENCIL: stencil++; break;
            case MGL_FMT_DEPTH_STENCIL: ds++; break;
            default: break;
        }
    }
    printf("  compressed: %zu, colour: %zu, depth: %zu, stencil: %zu, ds: %zu\n",
           comp, color, depth, stencil, ds);
}

TEST(format_table, lookup_self_consistent)
{
    size_t n = mglFormatTableCount();
    for (size_t i = 0; i < n; i++) {
        const MGLFormatDesc *row = mglFormatTableRow(i);
        if (row->gl_format == 0) continue;
        const MGLFormatDesc *found = mglFormatDesc(row->gl_format);
        CHECK(found == row);
    }
}

TEST(format_table, basic_invariants)
{
    size_t n = mglFormatTableCount();
    for (size_t i = 0; i < n; i++) {
        const MGLFormatDesc *d = mglFormatTableRow(i);
        if (d->gl_format == 0) continue;
        CHECK(d->block_w >= 1);
        CHECK(d->block_h >= 1);
        if (d->kind != MGL_FMT_NONE)
            CHECK(d->bytes_per_block > 0);
        if (d->kind == MGL_FMT_COMPRESSED)
            CHECK(d->block_w > 1);
    }
}

TEST(format_table, cross_check_mtl_format)
{
    size_t n = mglFormatTableCount();
    int local_diffs = 0;
    for (size_t i = 0; i < n; i++) {
        const MGLFormatDesc *d = mglFormatTableRow(i);
        if (d->gl_format == 0) continue;
        if (d->kind == MGL_FMT_NONE) continue;

        unsigned old_mtl = (unsigned)mtlFormatForGLInternalFormat(d->gl_format);
        unsigned new_mtl = (unsigned)mglFormatMetalFormat(d->gl_format);

        if (old_mtl != new_mtl) {
            local_diffs++;
            printf("DIFF mtl GL_%s old=0x%x(%u) new=0x%x(%u)\n",
                   d->name, old_mtl, old_mtl, new_mtl, new_mtl);
        }
    }
    diff_count += local_diffs;
    printf("cross_check_mtl_format: %d diffs\n", local_diffs);
}

TEST(format_table, cross_check_size)
{
    size_t n = mglFormatTableCount();
    int local_diffs = 0;
    for (size_t i = 0; i < n; i++) {
        const MGLFormatDesc *d = mglFormatTableRow(i);
        if (d->gl_format == 0) continue;
        if (d->kind == MGL_FMT_NONE) continue;
        if (d->kind == MGL_FMT_COMPRESSED) continue;

        unsigned old_sz = (unsigned)sizeForInternalFormat(d->gl_format, 0, 0);
        unsigned new_sz = (unsigned)d->bytes_per_block;

        if (old_sz != new_sz) {
            local_diffs++;
            printf("DIFF size GL_%s old=%u new=%u\n", d->name, old_sz, new_sz);
        }
    }
    diff_count += local_diffs;
    printf("cross_check_size: %d diffs\n", local_diffs);
}

TEST(format_table, cross_check_bits)
{
    size_t n = mglFormatTableCount();
    int local_diffs = 0;
    static const GLenum comps[] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA,
                                    GL_DEPTH_COMPONENT, GL_STENCIL_INDEX };
    static const char *comp_names[] = { "R", "G", "B", "A", "D", "S" };

    for (size_t i = 0; i < n; i++) {
        const MGLFormatDesc *d = mglFormatTableRow(i);
        if (d->gl_format == 0) continue;
        if (d->kind == MGL_FMT_NONE) continue;
        if (d->kind == MGL_FMT_COMPRESSED) continue;

        for (int c = 0; c < 6; c++) {
            unsigned old_bits = (unsigned)bitcountForInternalFormat(d->gl_format, comps[c]);
            unsigned new_bits = (unsigned)mglFormatComponentBits(d->gl_format, comps[c]);
            if (old_bits != new_bits) {
                local_diffs++;
                printf("DIFF bits GL_%s.%s old=%u new=%u\n",
                       d->name, comp_names[c], old_bits, new_bits);
            }
        }
    }
    diff_count += local_diffs;
    printf("cross_check_bits: %d diffs\n", local_diffs);
}

TEST(format_table, total_diffs_pinned)
{
    printf("TOTAL DIFFS: %d\n", diff_count);
    // pixel_utils.c now answers from this table, so old and new must agree.
    // The 255 differences the transition found (56 Metal formats, 52 sizes,
    // 147 bit counts, all bugs in the old switches) are in the Phase 1 notes.
    CHECK_EQ_INT(diff_count, 0);
}

/* ---- spot checks ---- */

TEST(format_table, spot_rgba8)
{
    const MGLFormatDesc *d = mglFormatDesc(GL_RGBA8);
    CHECK(d->gl_format == GL_RGBA8);
    CHECK_EQ_UINT(d->mtl_format, MTLPixelFormatRGBA8Unorm);
    CHECK_EQ_UINT(d->bytes_per_block, 4);
    CHECK_EQ_UINT(d->bits[0], 8);
    CHECK_EQ_UINT(d->bits[1], 8);
    CHECK_EQ_UINT(d->bits[2], 8);
    CHECK_EQ_UINT(d->bits[3], 8);
}

TEST(format_table, spot_depth24_stencil8)
{
    const MGLFormatDesc *d = mglFormatDesc(GL_DEPTH24_STENCIL8);
    CHECK(d->gl_format == GL_DEPTH24_STENCIL8);
    CHECK_EQ_UINT(d->bits[4], 24);
    CHECK_EQ_UINT(d->bits[5], 8);
}

TEST(format_table, spot_dxt5)
{
    const MGLFormatDesc *d = mglFormatDesc(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    CHECK(d->gl_format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    CHECK_EQ_UINT(d->block_w, 4);
    CHECK_EQ_UINT(d->block_h, 4);
    CHECK_EQ_UINT(d->bytes_per_block, 16);
}

TEST(format_table, spot_etc2_rgb8)
{
    const MGLFormatDesc *d = mglFormatDesc(GL_COMPRESSED_RGB8_ETC2);
    CHECK(d->gl_format == GL_COMPRESSED_RGB8_ETC2);
    CHECK_EQ_UINT(d->block_w, 4);
    CHECK_EQ_UINT(d->block_h, 4);
    CHECK_EQ_UINT(d->bytes_per_block, 8);
}

TEST(format_table, spot_astc_12x12)
{
    const MGLFormatDesc *d = mglFormatDesc(GL_COMPRESSED_RGBA_ASTC_12x12_KHR);
    CHECK(d->gl_format == GL_COMPRESSED_RGBA_ASTC_12x12_KHR);
    CHECK_EQ_UINT(d->block_w, 12);
    CHECK_EQ_UINT(d->block_h, 12);
    CHECK_EQ_UINT(d->bytes_per_block, 16);
}

TEST(format_table, spot_r11g11b10f)
{
    const MGLFormatDesc *d = mglFormatDesc(GL_R11F_G11F_B10F);
    CHECK(d->gl_format == GL_R11F_G11F_B10F);
    CHECK_EQ_UINT(d->bits[0], 11);
    CHECK_EQ_UINT(d->bits[1], 11);
    CHECK_EQ_UINT(d->bits[2], 10);
    CHECK_EQ_UINT(d->bits[3], 0);
}

TEST(format_table, bytes_per_row_dxt1)
{
    size_t bpr = mglFormatBytesPerRow(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 10);
    CHECK_EQ_UINT((unsigned)bpr, 24);
}

TEST(format_table, image_size_bptc)
{
    size_t sz = mglFormatImageSize(GL_COMPRESSED_RGBA_BPTC_UNORM, 5, 5, 1);
    CHECK_EQ_UINT((unsigned)sz, 64);
}

TEST(format_table, device_bc_disable)
{
    MGLDeviceFormatCaps caps = {0};
    caps.apple_gpu = true;
    caps.supports_bc = false;
    mglFormatTableSetDevice(&caps);

    uint16_t fmt = mglFormatMetalFormat(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    CHECK_EQ_UINT(fmt, MTLPixelFormatInvalid);

    caps.supports_bc = true;
    mglFormatTableSetDevice(&caps);
    fmt = mglFormatMetalFormat(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    CHECK_EQ_UINT(fmt, MTLPixelFormatBC3_RGBA);

    mglFormatTableSetDevice(NULL);
}

TEST(format_table, unknown_format_returns_sentinel)
{
    const MGLFormatDesc *d = mglFormatDesc(0xDEAD);
    CHECK(d->gl_format == 0);
    CHECK(d->kind == MGL_FMT_NONE);

    const MGLMetalFormatDesc *m = mglMetalFormatDesc(0xFFFF);
    CHECK(m->mtl_format == 0);
}

TEST(format_table, compressed_format_list)
{
    GLenum list[128];
    GLsizei cnt = mglFormatCompressedFormatList(list, 128);
    printf("compressed formats usable on default device: %d\n", cnt);
    CHECK(cnt > 0);
    for (GLsizei i = 0; i < cnt; i++) {
        CHECK(mglFormatIsCompressed(list[i]));
    }
}

TEST(format_table, format_kind_classification)
{
    CHECK(mglFormatDesc(GL_RGBA8)->kind == MGL_FMT_COLOR_FLOAT);
    CHECK(mglFormatDesc(GL_RGBA8I)->kind == MGL_FMT_COLOR_INT);
    CHECK(mglFormatDesc(GL_RGBA8UI)->kind == MGL_FMT_COLOR_UINT);
    CHECK(mglFormatDesc(GL_DEPTH_COMPONENT16)->kind == MGL_FMT_DEPTH);
    CHECK(mglFormatDesc(GL_STENCIL_INDEX8)->kind == MGL_FMT_STENCIL);
    CHECK(mglFormatDesc(GL_DEPTH24_STENCIL8)->kind == MGL_FMT_DEPTH_STENCIL);
    CHECK(mglFormatDesc(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)->kind == MGL_FMT_COMPRESSED);
}
