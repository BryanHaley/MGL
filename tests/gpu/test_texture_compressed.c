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
#include <stdlib.h>
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

/* GL reads a compressed texture back through glGetTexImage as plain pixels.
   MGL used to refuse outright, so every RGTC readback in the CTS errored. */
GPU_TEST(texture_compressed, rgtc_reads_back_as_plain_pixels)
{
    const GLsizei w = 8, h = 8;
    GLubyte src[8 * 8], got[8 * 8];
    GLuint tex = 0;
    int worst = 0;

    for (GLsizei y = 0; y < h; y++)
        for (GLsizei x = 0; x < w; x++)
            src[y * w + x] = (GLubyte)(x * 32);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, w, h, 0,
                 GL_RED, GL_UNSIGNED_BYTE, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    /* BC4 keeps a 4x4 block to eight levels between two endpoints, so a
       horizontal ramp comes back close but not exact */
    for (GLsizei i = 0; i < w * h; i++)
    {
        int d = (int)got[i] - (int)src[i];

        if (d < 0) d = -d;
        if (d > worst) worst = d;
    }

    CHECK_MSG(worst <= 16, "worst texel is off by %d", worst);

    glDeleteTextures(1, &tex);
}

GPU_TEST(texture_compressed, signed_rgtc2_reads_back_as_plain_pixels)
{
    const GLsizei w = 8, h = 4;
    GLbyte src[8 * 4 * 2], got[8 * 4 * 2];
    GLuint tex = 0;
    int worst = 0;

    for (GLsizei y = 0; y < h; y++)
        for (GLsizei x = 0; x < w; x++)
        {
            src[(y * w + x) * 2 + 0] = (GLbyte)(-120 + x * 30);
            src[(y * w + x) * 2 + 1] = (GLbyte)(100 - y * 40);
        }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    mgl_drain_errors();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_SIGNED_RG_RGTC2, w, h, 0,
                 GL_RG, GL_BYTE, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    memset(got, 0xAB, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_BYTE, got);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    for (GLsizei i = 0; i < w * h * 2; i++)
    {
        int d = (int)got[i] - (int)src[i];

        if (d < 0) d = -d;
        if (d > worst) worst = d;
    }

    CHECK_MSG(worst <= 16, "worst texel is off by %d", worst);

    glDeleteTextures(1, &tex);
}

/* glTexStorage2D names a compressed internal format and no client format at
   all. Sizing that through the uncompressed path failed, so the level was
   never allocated and every glCompressedTexSubImage2D into it was refused --
   which is how a glTF scene full of BC5 and BC7 textures loaded nothing. */
GPU_TEST(texture_compressed, immutable_storage_takes_compressed_formats)
{
    static const struct { GLenum fmt; const char *name; } cases[] = {
        { GL_COMPRESSED_RG_RGTC2,               "GL_COMPRESSED_RG_RGTC2" },
        { GL_COMPRESSED_RGBA_BPTC_UNORM,        "GL_COMPRESSED_RGBA_BPTC_UNORM" },
        { GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM,  "GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM" },
    };
    const GLsizei w = 16, h = 16;
    const GLsizei blocks = (w / 4) * (h / 4);
    GLubyte src[16 * 16], got[16 * 16];

    for (GLsizei i = 0; i < (GLsizei)sizeof src; i++)
        src[i] = (GLubyte)(i * 7);

    for (int c = 0; c < 3; c++)
    {
        GLuint tex = 0;
        GLint bytes = blocks * 16;

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        mgl_drain_errors();

        glTexStorage2D(GL_TEXTURE_2D, 3, cases[c].fmt, w, h);
        CHECK_MSG(mgl_drain_errors() == GL_NO_ERROR, "storage for %s", cases[c].name);

        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, cases[c].fmt, bytes, src);
        CHECK_MSG(mgl_drain_errors() == GL_NO_ERROR, "sub-image for %s", cases[c].name);

        memset(got, 0, sizeof got);
        glGetCompressedTexImage(GL_TEXTURE_2D, 0, got);
        CHECK_MSG(mgl_drain_errors() == GL_NO_ERROR, "readback for %s", cases[c].name);
        CHECK_MSG(memcmp(src, got, (size_t)bytes) == 0, "%s did not round trip", cases[c].name);

        glDeleteTextures(1, &tex);
    }
}

/* ---------- pixel storage on compressed data ---------- */

// glPixelStorei refused the block parameters outright, and nothing in the
// pixel store could be read back through glGet at all.
GPU_TEST(texture_compressed, block_pixel_store_is_kept_and_answered)
{
    static const GLenum names[] = {
        GL_UNPACK_COMPRESSED_BLOCK_WIDTH, GL_UNPACK_COMPRESSED_BLOCK_HEIGHT,
        GL_UNPACK_COMPRESSED_BLOCK_DEPTH, GL_UNPACK_COMPRESSED_BLOCK_SIZE,
        GL_PACK_COMPRESSED_BLOCK_WIDTH,   GL_PACK_COMPRESSED_BLOCK_HEIGHT,
        GL_PACK_COMPRESSED_BLOCK_DEPTH,   GL_PACK_COMPRESSED_BLOCK_SIZE,
        GL_UNPACK_ROW_LENGTH,             GL_PACK_SKIP_ROWS,
    };

    for (unsigned i = 0; i < sizeof names / sizeof *names; i++)
    {
        GLint got = -1;

        glPixelStorei(names[i], 4 + (GLint)i);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

        glGetIntegerv(names[i], &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_MSG(got == 4 + (GLint)i, "pname 0x%x read back %d, want %d", names[i], got, 4 + (GLint)i);

        glPixelStorei(names[i], 0);
    }
}

// With a block size set, row length and the skips count whole blocks, so a
// 4x4 upload can be taken from the middle of a wider compressed image.
GPU_TEST(texture_compressed, a_block_rectangle_comes_out_of_a_wider_image)
{
    GLuint t = 0;
    unsigned char src[4 * 8], got[8];

    if (!format_is_advertised(GL_COMPRESSED_RED_RGTC1))
        SKIP("BC4/RGTC not supported by this device");

    // a 16x4 image is one row of four 8-byte blocks; each block holds its index
    for (int b = 0; b < 4; b++)
        memset(src + b * 8, 0x10 * (b + 1), 8);

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, 8);
    glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 4);
    glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 16);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 8);   // the third block

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 0, 8, src);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, 0);
    glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0);
    glPixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

    memset(got, 0xCD, sizeof got);
    glGetCompressedTexImage(GL_TEXTURE_2D, 0, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(got[0] == 0x30 && got[7] == 0x30,
              "uploaded block starts 0x%02x - want the third block, 0x30", got[0]);

    glDeleteTextures(1, &t);
}

// A compressed level keeps one row per row of 4x4 blocks. The array upload
// stepped from layer to layer by pixel rows, so every layer after the first
// was read from four times too far in.
GPU_TEST(texture_compressed, each_layer_of_a_compressed_array_is_its_own)
{
    static const char *vs =
        "#version 430 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 430 core\n"
        "uniform sampler2DArray t;\n"
        "uniform int layer;\n"
        "out vec4 o;\n"
        "void main() { o = texture(t, vec3(0.5, 0.5, float(layer))); }\n";
    // one solid colour a layer: red, green, blue, as DXT1 blocks whose every
    // index picks colour 0
    static const GLushort colours[3] = { 0xF800, 0x07E0, 0x001F };
    GLubyte data[3 * 4 * 8];
    GLuint tex = 0, prog, vao, vbo;
    MGLTestTarget t;
    char log[1024];

    for (int l = 0; l < 3; l++)
        for (int b = 0; b < 4; b++)
        {
            GLubyte *blk = data + (l * 4 + b) * 8;

            memset(blk, 0, 8);
            blk[0] = (GLubyte)(colours[l] & 0xFF);
            blk[1] = (GLubyte)(colours[l] >> 8);
        }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, 0x83F0 /* RGB_S3TC_DXT1 */, 8, 8, 3, 0, sizeof data, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "program did not build: %s", log);

    if (prog && mgl_target_create(&t, 8, 8, GL_RGBA8, 0))
    {
        mgl_target_bind(&t);
        glViewport(0, 0, 8, 8);
        vao = mgl_fullscreen_quad(&vbo);
        glUseProgram(prog);
        glUniform1i(glGetUniformLocation(prog, "t"), 0);

        for (int l = 0; l < 3; l++)
        {
            unsigned char c[4] = { 0 };
            unsigned char *px;

            glUniform1i(glGetUniformLocation(prog, "layer"), l);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            px = mgl_read_rgba8(&t);

            if (px)
            {
                mgl_pixel_at(px, &t, 4, 4, c);
                CHECK_MSG(c[l] > 200 && c[(l + 1) % 3] < 50 && c[(l + 2) % 3] < 50,
                          "layer %d drew %d,%d,%d", l, c[0], c[1], c[2]);
                free(px);
            }
        }

        glUseProgram(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        mgl_target_destroy(&t);
    }

    glDeleteProgram(prog);
    glDeleteTextures(1, &tex);
}

/* Plain pixels compressed on upload still start where the unpack skips say. */
GPU_TEST(texture_compressed, rgtc_upload_honours_unpack_skips)
{
    GLubyte src[9 * 9], got[8 * 8];
    GLuint tex = 0;

    /* the first row and column are skipped; the 8x8 image is all 200 */
    for (int y = 0; y < 9; y++)
        for (int x = 0; x < 9; x++)
            src[y * 9 + x] = (x == 0 || y == 0) ? 0 : 200;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 9);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 1);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 8, 0, GL_RED, GL_UNSIGNED_BYTE, src);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    memset(got, 0, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, got);

    for (int k = 0; k < 64; k++)
        if (got[k] < 198 || got[k] > 202)
        {
            CHECK_MSG(0, "texel %d read %u, want 200", k, got[k]);
            break;
        }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glDeleteTextures(1, &tex);
}

/* GL 4.6 8.5 and table 8.17: no depth, stencil or RGTC in a 3D texture. */
GPU_TEST(texture_compressed, three_d_refuses_depth_and_rgtc)
{
    GLuint tex = 0;
    GLubyte zeros[64] = {0};

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    mgl_drain_errors();

    glTexImage3D(GL_TEXTURE_3D, 0, GL_COMPRESSED_RED_RGTC1, 4, 4, 1, 0, GL_RED, GL_UNSIGNED_BYTE, zeros);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, glGetError());
    glTexImage3D(GL_TEXTURE_3D, 0, GL_DEPTH_COMPONENT24, 4, 4, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, glGetError());
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_DEPTH24_STENCIL8, 4, 4, 1);
    CHECK_EQ_UINT(GL_INVALID_OPERATION, glGetError());
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, zeros);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDeleteTextures(1, &tex);
}

/* UNPACK_SWAP_BYTES applies to pixels compressed on upload too. */
GPU_TEST(texture_compressed, rgtc_upload_honours_swap_bytes)
{
    GLushort src[8 * 8];
    GLubyte got[8 * 8];
    GLuint tex = 0;

    /* 0x8080 is the same either way round, so use one that is not */
    for (int k = 0; k < 64; k++)
        src[k] = 0x00c8;    /* stored swapped: reads as 0xc800 */

    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 8, 0, GL_RED, GL_UNSIGNED_SHORT, src);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    memset(got, 0, sizeof got);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, got);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    /* 0xc800 / 0xffff is about 200 / 255 */
    CHECK_MSG(got[0] >= 198 && got[0] <= 202, "texel 0 read %u, want about 200", got[0]);

    glDeleteTextures(1, &tex);
}
