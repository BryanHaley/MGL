/*
 * test_texture_compressed.c
 * MGL
 *
 * Compressed texture upload, readback and sampling. The blocks below are
 * hand-built rather than produced by an encoder, so the expected texels are
 * known exactly and a decode can be checked against them.
 */

#include "mgl_test.h"
#include "harness.h"
#include <string.h>

#ifndef GL_COMPRESSED_RED_RGTC1
#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#endif
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGB8_ETC2
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#endif

/* A BC4 block is 8 bytes: two red endpoints then sixteen 3-bit indices.
   Index 0 selects red0, so an all-zero index field paints the whole 4x4
   block with red0. */
static const unsigned char bc4_white[8]  = { 0xFF, 0x00, 0, 0, 0, 0, 0, 0 };
static const unsigned char bc4_black[8]  = { 0x00, 0xFF, 0, 0, 0, 0, 0, 0 };

/* A BC1 block is 8 bytes: two RGB565 endpoints then sixteen 2-bit indices.
   Index 0 selects colour0. 0xFFFF is white, 0x0000 black. */
static const unsigned char bc1_white[8]  = { 0xFF, 0xFF, 0x00, 0x00, 0, 0, 0, 0 };

static int format_is_advertised(GLenum fmt)
{
    GLint n = 0, i;
    GLint list[128];

    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &n);
    if (n <= 0 || n > (GLint)(sizeof(list) / sizeof(list[0])))
        return 0;

    glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, list);
    for (i = 0; i < n; i++)
        if ((GLenum)list[i] == fmt)
            return 1;

    return 0;
}

/* ---------- what the driver claims to support ---------- */

GPU_TEST(texture_compressed, advertises_a_usable_format_list)
{
    GLint n = 0;

    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &n);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(n > 0);

    // Metal guarantees ETC2 and ASTC on Apple silicon; one of the three
    // families below must be present or nothing here can be tested at all.
    CHECK(format_is_advertised(GL_COMPRESSED_RED_RGTC1) ||
          format_is_advertised(GL_COMPRESSED_RGB_S3TC_DXT1_EXT) ||
          format_is_advertised(GL_COMPRESSED_RGB8_ETC2));
}

GPU_TEST(texture_compressed, format_list_holds_only_specific_formats)
{
    // GL 4.6 core, table 23.53: COMPRESSED_TEXTURE_FORMATS is "the list of
    // supported specific compressed texture formats". The generic formats
    // below are a request to pick a format, not a format, and must not appear.
    static const GLenum generic[] = {
        GL_COMPRESSED_RED, GL_COMPRESSED_RG,
        GL_COMPRESSED_RGB, GL_COMPRESSED_RGBA,
        GL_COMPRESSED_SRGB, GL_COMPRESSED_SRGB_ALPHA,
    };

    for (size_t i = 0; i < sizeof(generic) / sizeof(generic[0]); i++)
        CHECK(!format_is_advertised(generic[i]));
}

/* ---------- upload and byte-exact readback ---------- */

GPU_TEST(texture_compressed, bc4_round_trips_its_bytes)
{
    GLuint t = 0;
    unsigned char got[8];

    if (!format_is_advertised(GL_COMPRESSED_RED_RGTC1))
        SKIP("BC4/RGTC not supported by this device");

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 4, 0, sizeof(bc4_white), bc4_white);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0xCD, sizeof(got));
    glGetCompressedTexImage(GL_TEXTURE_2D, 0, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(memcmp(got, bc4_white, sizeof(bc4_white)) == 0);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_compressed, bc1_round_trips_its_bytes)
{
    GLuint t = 0;
    unsigned char got[8];

    if (!format_is_advertised(GL_COMPRESSED_RGB_S3TC_DXT1_EXT))
        SKIP("BC1/S3TC not supported by this device");

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                           4, 4, 0, sizeof(bc1_white), bc1_white);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    memset(got, 0xCD, sizeof(got));
    glGetCompressedTexImage(GL_TEXTURE_2D, 0, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(memcmp(got, bc1_white, sizeof(bc1_white)) == 0);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_compressed, sub_image_lands_in_the_addressed_block)
{
    GLuint t = 0;
    unsigned char src[4 * 8];   /* 16x4 = four 4x4 blocks in a row */
    unsigned char got[4 * 8];
    int b;

    if (!format_is_advertised(GL_COMPRESSED_RED_RGTC1))
        SKIP("BC4/RGTC not supported by this device");

    for (b = 0; b < 4; b++)
        memcpy(src + b * 8, bc4_black, 8);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           16, 4, 0, sizeof(src), src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // replace only the third block, at x = 8
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 8, 0, 4, 4,
                              GL_COMPRESSED_RED_RGTC1, 8, bc4_white);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetCompressedTexImage(GL_TEXTURE_2D, 0, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK(memcmp(got + 0 * 8, bc4_black, 8) == 0);
    CHECK(memcmp(got + 1 * 8, bc4_black, 8) == 0);
    CHECK(memcmp(got + 2 * 8, bc4_white, 8) == 0);   /* the one we wrote */
    CHECK(memcmp(got + 3 * 8, bc4_black, 8) == 0);

    glDeleteTextures(1, &t);
}

/* ---------- the level queries ---------- */

GPU_TEST(texture_compressed, level_query_reports_compressed_and_size)
{
    GLuint t = 0;
    GLint is_compressed = -1, size = -1, fmt = 0;

    if (!format_is_advertised(GL_COMPRESSED_RED_RGTC1))
        SKIP("BC4/RGTC not supported by this device");

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           16, 16, 0, 16 * 8, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &is_compressed);
    CHECK_EQ_INT(is_compressed, GL_TRUE);

    // 16x16 at 4x4 blocks is 16 blocks of 8 bytes
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &size);
    CHECK_EQ_INT(size, 16 * 8);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
    CHECK_EQ_INT(fmt, GL_COMPRESSED_RED_RGTC1);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_compressed, uncompressed_level_reports_not_compressed)
{
    GLuint t = 0;
    GLint is_compressed = -1;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &is_compressed);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(is_compressed, GL_FALSE);

    glDeleteTextures(1, &t);
}

/* ---------- validation ---------- */

GPU_TEST(texture_compressed, rejects_wrong_image_size)
{
    GLuint t = 0;

    if (!format_is_advertised(GL_COMPRESSED_RED_RGTC1))
        SKIP("BC4/RGTC not supported by this device");

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    // a 4x4 BC4 level is exactly 8 bytes; anything else is INVALID_VALUE
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 4, 0, 7, bc4_white);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 4, 0, 9, bc4_white);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_compressed, rejects_unaligned_sub_image)
{
    GLuint t = 0;

    if (!format_is_advertised(GL_COMPRESSED_RED_RGTC1))
        SKIP("BC4/RGTC not supported by this device");

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           16, 16, 0, 16 * 8, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // an offset has to sit on a block boundary
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 2, 0, 4, 4,
                              GL_COMPRESSED_RED_RGTC1, 8, bc4_white);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, 4, 4,
                              GL_COMPRESSED_RED_RGTC1, 8, bc4_white);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &t);
}

GPU_TEST(texture_compressed, rejects_generic_internal_format)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    // a generic format names no block layout, so it cannot describe the data
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA,
                           4, 4, 0, 8, bc4_white);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(1, &t);
}

/* ---------- the part that proves the data reached the GPU ---------- */

static const char *kQuadVS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "out vec2 uv;\n"
    "void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *kSampleFS =
    "#version 460 core\n"
    "in vec2 uv;\n"
    "layout(binding = 0) uniform sampler2D tex;\n"
    "out vec4 frag;\n"
    "void main() { frag = vec4(texture(tex, uv).rgb, 1.0); }\n";

GPU_TEST(texture_compressed, bc4_decodes_when_sampled)
{
    MGLTestTarget target;
    GLuint t = 0, prog = 0, vao = 0, vbo = 0;
    unsigned char *px = NULL, rgba[4];
    char log[512] = { 0 };

    if (!format_is_advertised(GL_COMPRESSED_RED_RGTC1))
        SKIP("BC4/RGTC not supported by this device");

    if (!mgl_target_create(&target, 16, 16, GL_RGBA8, 0))
        SKIP("could not create a render target");

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    // red0 = 0xFF with all-zero indices, so every texel decodes to red = 1.0
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1,
                           4, 4, 0, sizeof(bc4_white), bc4_white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    prog = mgl_build_program(kQuadVS, kSampleFS, log, sizeof(log));
    if (!prog)
        SKIP("sampler program did not build");

    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&target);
    glViewport(0, 0, target.width, target.height);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&target);
    if (px)
    {
        mgl_pixel_at(px, &target, 8, 8, rgba);
        // BC4 is single-channel: red comes from the block, green and blue
        // read as 0 and alpha as 1.
        CHECK_EQ_INT(rgba[0], 255);
        CHECK_EQ_INT(rgba[1], 0);
        CHECK_EQ_INT(rgba[2], 0);
        free(px);
    }
    else
    {
        CHECK(0);
    }

    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteTextures(1, &t);
    mgl_target_destroy(&target);
}
