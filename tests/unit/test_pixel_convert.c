/*
 * test_pixel_convert.c
 * MGL
 */

#include "mgl_test.h"
#include "pixel_convert.h"

#ifndef GL_LUMINANCE
#define GL_LUMINANCE       0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif

/* ---------- half float ---------- */

TEST(half, roundtrip_exact_values)
{
    static const float vals[] = { 0.0f, 1.0f, -1.0f, 0.5f, 2.0f, -2.0f,
                                  0.25f, 1024.0f, -1024.0f, 65504.0f };

    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++)
    {
        float back = mglHalfToFloat(mglFloatToHalf(vals[i]));
        CHECK_NEAR(back, vals[i], 0.0);
    }
}

TEST(half, zero_and_sign)
{
    CHECK_EQ_UINT(mglFloatToHalf(0.0f), 0x0000u);
    CHECK_EQ_UINT(mglFloatToHalf(-0.0f), 0x8000u);
    CHECK_NEAR(mglHalfToFloat(0x0000u), 0.0, 0.0);
}

TEST(half, known_bit_patterns)
{
    CHECK_EQ_UINT(mglFloatToHalf(1.0f), 0x3C00u);
    CHECK_EQ_UINT(mglFloatToHalf(2.0f), 0x4000u);
    CHECK_EQ_UINT(mglFloatToHalf(0.5f), 0x3800u);
    CHECK_EQ_UINT(mglFloatToHalf(-1.0f), 0xBC00u);

    CHECK_NEAR(mglHalfToFloat(0x3C00u), 1.0, 0.0);
    CHECK_NEAR(mglHalfToFloat(0x4000u), 2.0, 0.0);
    CHECK_NEAR(mglHalfToFloat(0x3800u), 0.5, 0.0);
}

TEST(half, subnormals)
{
    // smallest positive subnormal is 2^-24
    float smallest = mglHalfToFloat(0x0001u);
    CHECK_NEAR(smallest, 5.9604645e-8, 1e-12);

    // smallest normal is 2^-14
    CHECK_NEAR(mglHalfToFloat(0x0400u), 6.1035156e-5, 1e-10);
}

TEST(half, infinity_and_nan)
{
    CHECK_EQ_UINT(mglFloatToHalf(INFINITY) & 0x7FFFu, 0x7C00u);
    CHECK(isinf(mglHalfToFloat(0x7C00u)));
    CHECK(isnan(mglHalfToFloat(0x7E00u)));
    CHECK(isnan(mglHalfToFloat(mglFloatToHalf(NAN))));
}

TEST(half, overflow_to_inf)
{
    CHECK(isinf(mglHalfToFloat(mglFloatToHalf(1.0e30f))));
}

/* ---------- size and format tables ---------- */

TEST(pixfmt, packed_pixel_sizes)
{
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RGBA, GL_UNSIGNED_BYTE), 4);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RGB,  GL_UNSIGNED_BYTE), 3);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RED,  GL_UNSIGNED_BYTE), 1);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RGBA, GL_FLOAT), 16);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RGBA, GL_HALF_FLOAT), 8);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RG,   GL_UNSIGNED_SHORT), 4);

    // packed types carry all components in one unit
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RGB,  GL_UNSIGNED_SHORT_5_6_5), 2);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV), 4);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV), 4);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8), 4);

    // nonsense pairs
    CHECK_EQ_UINT(mglPackedPixelSize(0x1234, GL_UNSIGNED_BYTE), 0);
    CHECK_EQ_UINT(mglPackedPixelSize(GL_RGBA, 0x1234), 0);
}

TEST(pixfmt, native_bytes_per_pixel)
{
    CHECK_EQ_UINT(mglNativeFormatBytesPerPixel(MGL_NF_R8_UNORM), 1);
    CHECK_EQ_UINT(mglNativeFormatBytesPerPixel(MGL_NF_RG8_UNORM), 2);
    CHECK_EQ_UINT(mglNativeFormatBytesPerPixel(MGL_NF_BGRA8_UNORM), 4);
    CHECK_EQ_UINT(mglNativeFormatBytesPerPixel(MGL_NF_RGBA16_FLOAT), 8);
    CHECK_EQ_UINT(mglNativeFormatBytesPerPixel(MGL_NF_RGBA32_FLOAT), 16);
    CHECK_EQ_UINT(mglNativeFormatBytesPerPixel(MGL_NF_UNKNOWN), 0);
}

TEST(pixfmt, format_classification)
{
    CHECK(mglNativeFormatIsInteger(MGL_NF_RGBA8_UINT));
    CHECK(mglNativeFormatIsInteger(MGL_NF_R32_SINT));
    CHECK(!mglNativeFormatIsInteger(MGL_NF_RGBA8_UNORM));

    CHECK(mglNativeFormatIsDepth(MGL_NF_DEPTH32_FLOAT));
    CHECK(mglNativeFormatIsDepth(MGL_NF_DEPTH24_UNORM_STENCIL8));
    CHECK(!mglNativeFormatIsDepth(MGL_NF_RGBA8_UNORM));

    CHECK(mglNativeFormatIsStencil(MGL_NF_STENCIL8));
    CHECK(mglNativeFormatIsStencil(MGL_NF_DEPTH24_UNORM_STENCIL8));
    CHECK(!mglNativeFormatIsStencil(MGL_NF_DEPTH32_FLOAT));
}

TEST(pixfmt, components_for_format)
{
    CHECK_EQ_UINT(mglComponentsForFormat(GL_RED), 1);
    CHECK_EQ_UINT(mglComponentsForFormat(GL_RG), 2);
    CHECK_EQ_UINT(mglComponentsForFormat(GL_RGB), 3);
    CHECK_EQ_UINT(mglComponentsForFormat(GL_BGR), 3);
    CHECK_EQ_UINT(mglComponentsForFormat(GL_RGBA), 4);
    CHECK_EQ_UINT(mglComponentsForFormat(GL_BGRA), 4);
    CHECK_EQ_UINT(mglComponentsForFormat(GL_DEPTH_STENCIL), 2);
    CHECK_EQ_UINT(mglComponentsForFormat(0x9999), 0);
}

/* ---------- conversion ---------- */

TEST(convert, bgra8_to_rgba8_swaps_channels)
{
    // one BGRA pixel: B=0x10 G=0x20 R=0x30 A=0x40
    unsigned char src[4] = { 0x10, 0x20, 0x30, 0x40 };
    unsigned char dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_BGRA8_UNORM,
                           dst, 4, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 0x30);   // R
    CHECK_EQ_UINT(dst[1], 0x20);   // G
    CHECK_EQ_UINT(dst[2], 0x10);   // B
    CHECK_EQ_UINT(dst[3], 0x40);   // A
}

TEST(convert, rgba8_identity)
{
    unsigned char src[4] = { 0x11, 0x22, 0x33, 0x44 };
    unsigned char dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           dst, 4, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    for (int i = 0; i < 4; i++) CHECK_EQ_UINT(dst[i], src[i]);
}

TEST(convert, rgba8_to_bgra8)
{
    unsigned char src[4] = { 0x11, 0x22, 0x33, 0x44 };
    unsigned char dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           dst, 4, GL_BGRA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 0x33);
    CHECK_EQ_UINT(dst[1], 0x22);
    CHECK_EQ_UINT(dst[2], 0x11);
    CHECK_EQ_UINT(dst[3], 0x44);
}

TEST(convert, vertical_flip)
{
    // 1 wide, 3 tall; rows are 10, 20, 30
    unsigned char src[3] = { 10, 20, 30 };
    unsigned char dst[3] = { 0 };

    CHECK(mglConvertPixels(src, 1, MGL_NF_R8_UNORM,
                           dst, 1, GL_RED, GL_UNSIGNED_BYTE, 1, 3, GL_TRUE));

    CHECK_EQ_UINT(dst[0], 30);
    CHECK_EQ_UINT(dst[1], 20);
    CHECK_EQ_UINT(dst[2], 10);
}

TEST(convert, no_flip_preserves_order)
{
    unsigned char src[3] = { 10, 20, 30 };
    unsigned char dst[3] = { 0 };

    CHECK(mglConvertPixels(src, 1, MGL_NF_R8_UNORM,
                           dst, 1, GL_RED, GL_UNSIGNED_BYTE, 1, 3, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 10);
    CHECK_EQ_UINT(dst[1], 20);
    CHECK_EQ_UINT(dst[2], 30);
}

TEST(convert, honours_row_pitch)
{
    // 2x2 image stored with 8-byte rows but only 2 bytes used
    unsigned char src[16] = { 1, 2, 0,0,0,0,0,0,
                              3, 4, 0,0,0,0,0,0 };
    unsigned char dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 8, MGL_NF_R8_UNORM,
                           dst, 2, GL_RED, GL_UNSIGNED_BYTE, 2, 2, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 1); CHECK_EQ_UINT(dst[1], 2);
    CHECK_EQ_UINT(dst[2], 3); CHECK_EQ_UINT(dst[3], 4);
}

TEST(convert, unorm8_to_float_endpoints)
{
    unsigned char src[4] = { 0, 255, 128, 255 };
    float dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           dst, sizeof dst, GL_RGBA, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(dst[0], 0.0, 0.0);
    CHECK_NEAR(dst[1], 1.0, 0.0);
    CHECK_NEAR(dst[2], 128.0 / 255.0, 1e-6);
    CHECK_NEAR(dst[3], 1.0, 0.0);
}

TEST(convert, float_to_unorm8_rounds_and_clamps)
{
    float src[4] = { -0.5f, 0.5f, 1.5f, 1.0f };
    unsigned char dst[4] = { 0 };

    CHECK(mglConvertPixels(src, sizeof src, MGL_NF_RGBA32_FLOAT,
                           dst, 4, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 0);      // clamped low
    CHECK_EQ_UINT(dst[1], 128);    // 0.5 * 255 = 127.5 -> 128
    CHECK_EQ_UINT(dst[2], 255);    // clamped high
    CHECK_EQ_UINT(dst[3], 255);
}

TEST(convert, single_channel_selectors)
{
    unsigned char src[4] = { 10, 20, 30, 40 };
    unsigned char r = 0, g = 0, b = 0, a = 0;

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM, &r, 1, GL_RED,   GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));
    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM, &g, 1, GL_GREEN, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));
    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM, &b, 1, GL_BLUE,  GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));
    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM, &a, 1, GL_ALPHA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(r, 10); CHECK_EQ_UINT(g, 20);
    CHECK_EQ_UINT(b, 30); CHECK_EQ_UINT(a, 40);
}

TEST(convert, rgb_drops_alpha)
{
    unsigned char src[4] = { 1, 2, 3, 4 };
    unsigned char dst[3] = { 9, 9, 9 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           dst, 3, GL_RGB, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 1); CHECK_EQ_UINT(dst[1], 2); CHECK_EQ_UINT(dst[2], 3);
}

TEST(convert, bgr_reverses_rgb)
{
    unsigned char src[4] = { 1, 2, 3, 4 };
    unsigned char dst[3] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           dst, 3, GL_BGR, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 3); CHECK_EQ_UINT(dst[1], 2); CHECK_EQ_UINT(dst[2], 1);
}

TEST(convert, one_channel_source_defaults_alpha_to_one)
{
    unsigned char src[1] = { 77 };
    unsigned char dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 1, MGL_NF_R8_UNORM,
                           dst, 4, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 77);
    CHECK_EQ_UINT(dst[1], 0);
    CHECK_EQ_UINT(dst[2], 0);
    CHECK_EQ_UINT(dst[3], 255);
}

TEST(convert, integer_formats_do_not_normalize)
{
    unsigned char src[4] = { 5, 6, 7, 8 };
    unsigned int dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UINT,
                           dst, sizeof dst, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 5); CHECK_EQ_UINT(dst[1], 6);
    CHECK_EQ_UINT(dst[2], 7); CHECK_EQ_UINT(dst[3], 8);
}

TEST(convert, signed_integer_source)
{
    signed char src[4] = { -5, 6, -7, 8 };
    int dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_SINT,
                           dst, sizeof dst, GL_RGBA_INTEGER, GL_INT, 1, 1, GL_FALSE));

    CHECK_EQ_INT(dst[0], -5); CHECK_EQ_INT(dst[1], 6);
    CHECK_EQ_INT(dst[2], -7); CHECK_EQ_INT(dst[3], 8);
}

TEST(convert, integer_format_rejects_float_type)
{
    unsigned char src[4] = { 1, 2, 3, 4 };
    float dst[4] = { 0 };

    CHECK(!mglConvertPixels(src, 4, MGL_NF_RGBA8_UINT,
                            dst, sizeof dst, GL_RGBA_INTEGER, GL_FLOAT, 1, 1, GL_FALSE));
}

TEST(convert, packed_565)
{
    // pure red, green, blue through 5_6_5
    unsigned char red[4]   = { 255, 0, 0, 255 };
    unsigned char green[4] = { 0, 255, 0, 255 };
    unsigned char blue[4]  = { 0, 0, 255, 255 };
    unsigned short d = 0;

    CHECK(mglConvertPixels(red, 4, MGL_NF_RGBA8_UNORM, &d, 2, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 1, 1, GL_FALSE));
    CHECK_EQ_UINT(d, 0xF800u);

    CHECK(mglConvertPixels(green, 4, MGL_NF_RGBA8_UNORM, &d, 2, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 1, 1, GL_FALSE));
    CHECK_EQ_UINT(d, 0x07E0u);

    CHECK(mglConvertPixels(blue, 4, MGL_NF_RGBA8_UNORM, &d, 2, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 1, 1, GL_FALSE));
    CHECK_EQ_UINT(d, 0x001Fu);
}

TEST(convert, packed_8888_rev_matches_little_endian_rgba)
{
    unsigned char src[4] = { 0x11, 0x22, 0x33, 0x44 };
    unsigned int d = 0;

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           &d, 4, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 1, 1, GL_FALSE));

    // REV packs A in the high byte, R in the low byte
    CHECK_EQ_UINT(d, 0x44332211u);
}

TEST(convert, packed_8888_non_rev)
{
    unsigned char src[4] = { 0x11, 0x22, 0x33, 0x44 };
    unsigned int d = 0;

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           &d, 4, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(d, 0x11223344u);
}

TEST(convert, packed_2_10_10_10_rev)
{
    unsigned char src[4] = { 255, 0, 0, 255 };
    unsigned int d = 0;

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           &d, 4, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(d & 0x3FFu, 1023u);          // R in low 10 bits
    CHECK_EQ_UINT((d >> 30) & 0x3u, 3u);       // A in top 2 bits
}

TEST(convert, packed_4444_and_rev_differ)
{
    unsigned char src[4] = { 255, 0, 0, 255 };
    unsigned short a = 0, b = 0;

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM, &a, 2, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 1, 1, GL_FALSE));
    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM, &b, 2, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(a, 0xF00Fu);   // R high, A low
    CHECK_EQ_UINT(b, 0xF00Fu);   // A high, R low - symmetric for this input
    // asymmetric input to actually separate them
    unsigned char src2[4] = { 255, 0, 0, 0 };
    CHECK(mglConvertPixels(src2, 4, MGL_NF_RGBA8_UNORM, &a, 2, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 1, 1, GL_FALSE));
    CHECK(mglConvertPixels(src2, 4, MGL_NF_RGBA8_UNORM, &b, 2, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4_REV, 1, 1, GL_FALSE));
    CHECK_EQ_UINT(a, 0xF000u);
    CHECK_EQ_UINT(b, 0x000Fu);
}

TEST(convert, depth32f_to_float)
{
    float src = 0.25f;
    float dst = 0.0f;

    CHECK(mglConvertPixels(&src, 4, MGL_NF_DEPTH32_FLOAT,
                           &dst, 4, GL_DEPTH_COMPONENT, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(dst, 0.25, 0.0);
}

TEST(convert, depth24_stencil8_packed)
{
    unsigned int src = (0x00FFFFFFu) | (0x7Fu << 24);   // depth 1.0, stencil 0x7F
    unsigned int dst = 0;

    CHECK(mglConvertPixels(&src, 4, MGL_NF_DEPTH24_UNORM_STENCIL8,
                           &dst, 4, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst >> 8, 0x00FFFFFFu);
    CHECK_EQ_UINT(dst & 0xFFu, 0x7Fu);
}

TEST(convert, stencil_index_reads_stencil_channel)
{
    unsigned int src = (0x00000000u) | (0x2Au << 24);
    unsigned char dst = 0;

    CHECK(mglConvertPixels(&src, 4, MGL_NF_DEPTH24_UNORM_STENCIL8,
                           &dst, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst, 0x2Au);
}

TEST(convert, srgb_source_is_linearised)
{
    // 0.5 in sRGB is well below 0.5 linear
    unsigned char src[4] = { 128, 128, 128, 255 };
    float dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM_SRGB,
                           dst, sizeof dst, GL_RGBA, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(dst[0], 0.2158, 0.005);
    CHECK_NEAR(dst[3], 1.0, 0.0);       // alpha stays linear
}

TEST(convert, float16_source)
{
    unsigned short src[4];
    float dst[4] = { 0 };

    src[0] = mglFloatToHalf(0.0f);
    src[1] = mglFloatToHalf(0.5f);
    src[2] = mglFloatToHalf(1.0f);
    src[3] = mglFloatToHalf(2.0f);

    CHECK(mglConvertPixels(src, 8, MGL_NF_RGBA16_FLOAT,
                           dst, sizeof dst, GL_RGBA, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(dst[0], 0.0, 0.0);
    CHECK_NEAR(dst[1], 0.5, 0.0);
    CHECK_NEAR(dst[2], 1.0, 0.0);
    CHECK_NEAR(dst[3], 2.0, 0.0);
}

TEST(convert, float_dest_is_not_clamped)
{
    float src[4] = { -3.0f, 7.0f, 0.0f, 1.0f };
    float dst[4] = { 0 };

    CHECK(mglConvertPixels(src, sizeof src, MGL_NF_RGBA32_FLOAT,
                           dst, sizeof dst, GL_RGBA, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(dst[0], -3.0, 0.0);
    CHECK_NEAR(dst[1], 7.0, 0.0);
}

TEST(convert, rgb10a2_source)
{
    unsigned int src = 1023u | (0u << 10) | (0u << 20) | (3u << 30);
    unsigned char dst[4] = { 0 };

    CHECK(mglConvertPixels(&src, 4, MGL_NF_RGB10A2_UNORM,
                           dst, 4, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 255);
    CHECK_EQ_UINT(dst[1], 0);
    CHECK_EQ_UINT(dst[3], 255);
}

TEST(convert, r11g11b10_roundtrip)
{
    float src[4] = { 1.0f, 0.5f, 0.25f, 1.0f };
    unsigned int packed = 0;
    float back[4] = { 0 };

    CHECK(mglConvertPixels(src, sizeof src, MGL_NF_RGBA32_FLOAT,
                           &packed, 4, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, 1, 1, GL_FALSE));

    CHECK(mglConvertPixels(&packed, 4, MGL_NF_RG11B10_FLOAT,
                           back, sizeof back, GL_RGBA, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(back[0], 1.0, 0.02);
    CHECK_NEAR(back[1], 0.5, 0.02);
    CHECK_NEAR(back[2], 0.25, 0.02);
}

TEST(convert, rgb9e5_roundtrip)
{
    float src[4] = { 1.0f, 0.5f, 0.25f, 1.0f };
    unsigned int packed = 0;
    float back[4] = { 0 };

    CHECK(mglConvertPixels(src, sizeof src, MGL_NF_RGBA32_FLOAT,
                           &packed, 4, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV, 1, 1, GL_FALSE));

    CHECK(mglConvertPixels(&packed, 4, MGL_NF_RGB9E5_FLOAT,
                           back, sizeof back, GL_RGBA, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(back[0], 1.0, 0.01);
    CHECK_NEAR(back[1], 0.5, 0.01);
    CHECK_NEAR(back[2], 0.25, 0.01);
}

TEST(convert, snorm_roundtrip_endpoints)
{
    signed char src[4] = { -127, 0, 127, 127 };
    float dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_SNORM,
                           dst, sizeof dst, GL_RGBA, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(dst[0], -1.0, 1e-6);
    CHECK_NEAR(dst[1], 0.0, 0.0);
    CHECK_NEAR(dst[2], 1.0, 1e-6);
}

TEST(convert, snorm_minus128_clamps_to_minus_one)
{
    signed char src[4] = { -128, 0, 0, 0 };
    float dst[4] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_SNORM,
                           dst, sizeof dst, GL_RGBA, GL_FLOAT, 1, 1, GL_FALSE));

    CHECK_NEAR(dst[0], -1.0, 0.0);
}

TEST(convert, luminance_reads_red)
{
    unsigned char src[4] = { 42, 1, 2, 3 };
    unsigned char dst[2] = { 0 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM,
                           dst, 2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 42);   // red
    CHECK_EQ_UINT(dst[1], 3);    // alpha
}

TEST(convert, rejects_unknown_source)
{
    unsigned char src[4] = { 0 }, dst[4] = { 0 };

    CHECK(!mglConvertPixels(src, 4, MGL_NF_UNKNOWN, dst, 4, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));
}

TEST(convert, rejects_null_pointers)
{
    unsigned char buf[4] = { 0 };

    CHECK(!mglConvertPixels(NULL, 4, MGL_NF_RGBA8_UNORM, buf, 4, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));
    CHECK(!mglConvertPixels(buf, 4, MGL_NF_RGBA8_UNORM, NULL, 4, GL_RGBA, GL_UNSIGNED_BYTE, 1, 1, GL_FALSE));
}

TEST(convert, zero_size_is_a_noop_success)
{
    unsigned char src[4] = { 1, 2, 3, 4 }, dst[4] = { 9, 9, 9, 9 };

    CHECK(mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM, dst, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0, 0, GL_FALSE));
    CHECK_EQ_UINT(dst[0], 9);
}

TEST(convert, negative_size_rejected)
{
    unsigned char src[4] = { 0 }, dst[4] = { 0 };

    CHECK(!mglConvertPixels(src, 4, MGL_NF_RGBA8_UNORM, dst, 4, GL_RGBA, GL_UNSIGNED_BYTE, -1, 1, GL_FALSE));
}

TEST(convert, multi_pixel_block)
{
    // 2x2 BGRA -> RGBA, checks both stride handling and channel swap together
    unsigned char src[16] = {
        0x01,0x02,0x03,0x04,  0x05,0x06,0x07,0x08,
        0x09,0x0A,0x0B,0x0C,  0x0D,0x0E,0x0F,0x10,
    };
    unsigned char dst[16] = { 0 };

    CHECK(mglConvertPixels(src, 8, MGL_NF_BGRA8_UNORM,
                           dst, 8, GL_RGBA, GL_UNSIGNED_BYTE, 2, 2, GL_FALSE));

    CHECK_EQ_UINT(dst[0], 0x03); CHECK_EQ_UINT(dst[2], 0x01);
    CHECK_EQ_UINT(dst[4], 0x07); CHECK_EQ_UINT(dst[6], 0x05);
    CHECK_EQ_UINT(dst[8], 0x0B); CHECK_EQ_UINT(dst[10], 0x09);
    CHECK_EQ_UINT(dst[12], 0x0F); CHECK_EQ_UINT(dst[14], 0x0D);
}

TEST(half, exact_tie_rounds_to_even)
{
    // mantissa 0x1000 is a dead tie with an even low bit, so it must round down
    union { GLfloat f; GLuint u; } a, b;

    a.u = (127u << 23) | 0x001000u;   // 1.0 + tie, low bit of the half is 0
    b.u = (127u << 23) | 0x003000u;   // 1.0 + tie, low bit of the half is 1

    CHECK_EQ_UINT(mglFloatToHalf(a.f), 0x3C00u);
    CHECK_EQ_UINT(mglFloatToHalf(b.f), 0x3C02u);
}
