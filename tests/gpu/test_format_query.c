/*
 * test_format_query.c
 * MGL
 *
 * What the driver says about formats now comes from one table, so the
 * answers have to agree with each other and with what Metal can do.
 */

#include "mgl_test.h"
#include "harness.h"

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

static GLint query(GLenum fmt, GLenum pname)
{
    GLint v = -1;
    glGetInternalformativ(GL_TEXTURE_2D, fmt, pname, 1, &v);
    return v;
}

GPU_TEST(format_query, compressed_format_list_is_consistent)
{
    GLint n = 0;
    GLint list[256] = { 0 };

    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &n);
    CHECK_MSG(n > 0 && n <= 256, "NUM_COMPRESSED_TEXTURE_FORMATS = %d", n);
    if (n <= 0 || n > 256) return;

    glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, list);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (GLint i = 0; i < n; i++)
    {
        CHECK_MSG(list[i] != 0, "entry %d is zero", i);
        // every listed format must also answer as supported and compressed
        CHECK_MSG(query(list[i], GL_INTERNALFORMAT_SUPPORTED) == GL_TRUE, "0x%x listed but unsupported", list[i]);
        CHECK_MSG(query(list[i], GL_TEXTURE_COMPRESSED) == GL_TRUE, "0x%x listed but not compressed", list[i]);
    }
}

GPU_TEST(format_query, colour_renderable_follows_metal_caps)
{
    CHECK_EQ_INT(query(GL_RGBA8, GL_COLOR_RENDERABLE), GL_TRUE);
    CHECK_EQ_INT(query(GL_RGBA8, GL_FRAMEBUFFER_RENDERABLE), GL_FULL_SUPPORT);
    CHECK_EQ_INT(query(GL_RGBA8, GL_FRAMEBUFFER_BLEND), GL_FULL_SUPPORT);
    CHECK_EQ_INT(query(GL_RGBA8, GL_FILTER), GL_FULL_SUPPORT);

    // integer formats never blend or filter
    CHECK_EQ_INT(query(GL_RGBA8UI, GL_FILTER), GL_NONE);
    CHECK_EQ_INT(query(GL_RGBA8UI, GL_FRAMEBUFFER_BLEND), GL_NONE);
    CHECK_EQ_INT(query(GL_RGBA8UI, GL_COLOR_RENDERABLE), GL_TRUE);

    // a compressed format is sampled, never drawn into
    CHECK_EQ_INT(query(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COLOR_RENDERABLE), GL_FALSE);

    CHECK_EQ_INT(query(GL_DEPTH24_STENCIL8, GL_DEPTH_RENDERABLE), GL_TRUE);
    CHECK_EQ_INT(query(GL_DEPTH24_STENCIL8, GL_STENCIL_RENDERABLE), GL_TRUE);
    CHECK_EQ_INT(query(GL_DEPTH_COMPONENT16, GL_STENCIL_RENDERABLE), GL_FALSE);
    CHECK_EQ_INT(query(GL_RGBA8, GL_DEPTH_RENDERABLE), GL_FALSE);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(format_query, block_geometry_of_compressed_formats)
{
    CHECK_EQ_INT(query(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_TEXTURE_COMPRESSED_BLOCK_WIDTH), 4);
    CHECK_EQ_INT(query(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_TEXTURE_COMPRESSED_BLOCK_HEIGHT), 4);
    CHECK_EQ_INT(query(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_TEXTURE_COMPRESSED_BLOCK_SIZE), 128);

    CHECK_EQ_INT(query(GL_COMPRESSED_RGB8_ETC2, GL_TEXTURE_COMPRESSED_BLOCK_SIZE), 64);
    CHECK_EQ_INT(query(GL_COMPRESSED_RGBA_ASTC_12x12_KHR, GL_TEXTURE_COMPRESSED_BLOCK_WIDTH), 12);

    CHECK_EQ_INT(query(GL_RGBA8, GL_TEXTURE_COMPRESSED), GL_FALSE);
    CHECK_EQ_INT(query(GL_RGBA8, GL_TEXTURE_COMPRESSED_BLOCK_WIDTH), 0);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(format_query, component_sizes_come_from_the_spec)
{
    CHECK_EQ_INT(query(GL_R11F_G11F_B10F, GL_INTERNALFORMAT_RED_SIZE), 11);
    CHECK_EQ_INT(query(GL_R11F_G11F_B10F, GL_INTERNALFORMAT_BLUE_SIZE), 10);
    CHECK_EQ_INT(query(GL_R11F_G11F_B10F, GL_INTERNALFORMAT_ALPHA_SIZE), 0);
    CHECK_EQ_INT(query(GL_RGB10_A2, GL_INTERNALFORMAT_ALPHA_SIZE), 2);
    CHECK_EQ_INT(query(GL_DEPTH24_STENCIL8, GL_INTERNALFORMAT_DEPTH_SIZE), 24);
    CHECK_EQ_INT(query(GL_DEPTH24_STENCIL8, GL_INTERNALFORMAT_STENCIL_SIZE), 8);
    CHECK_EQ_INT(query(GL_DEPTH24_STENCIL8, GL_INTERNALFORMAT_RED_SIZE), 0);
    CHECK_EQ_INT(query(GL_RGBA8, GL_INTERNALFORMAT_DEPTH_SIZE), 0);

    // the preferred upload pair is a real one, not a placeholder
    CHECK_EQ_INT(query(GL_RGBA16F, GL_TEXTURE_IMAGE_TYPE), GL_HALF_FLOAT);
    CHECK_EQ_INT(query(GL_RGBA8UI, GL_TEXTURE_IMAGE_FORMAT), GL_RGBA_INTEGER);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}
