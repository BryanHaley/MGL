/*
 * pixel_convert.c
 * MGL
 *
 * Everything funnels through one intermediate texel: decode a native pixel into
 * RGBA, then encode that into whatever the caller asked for. Keeps the number of
 * cases linear instead of (native x format x type).
 */

#include <string.h>
#include <math.h>

#include "pixel_convert.h"
#include "pixel_utils.h"
#include "mgl_format_table.h"

// not in glcorearb.h, but ES and older apps still ask for them
#ifndef GL_LUMINANCE
#define GL_LUMINANCE       0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif

typedef struct {
    GLfloat f[4];
    GLuint  u[4];
    GLint   i[4];
    GLboolean is_uint;   // value lives in u[]
    GLboolean is_sint;   // value lives in i[]
} MGLTexel;

/* ---------- small helpers ---------- */

static GLfloat clampf(GLfloat v, GLfloat lo, GLfloat hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static GLfloat unorm_to_float(GLuint v, GLuint bits)
{
    GLuint max = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);

    return (GLfloat)v / (GLfloat)max;
}

static GLfloat snorm_to_float(GLint v, GLuint bits)
{
    GLint max = (bits >= 32) ? 0x7FFFFFFF : (GLint)((1u << (bits - 1)) - 1u);
    GLfloat f = (GLfloat)v / (GLfloat)max;

    return f < -1.0f ? -1.0f : f;
}

static GLuint float_to_unorm(GLfloat f, GLuint bits)
{
    GLuint max = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    double v;

    if (f != f) f = 0.0f;   // NaN

    // double, not float: at 24 bits and up, max + 0.5 rounds past max in float32
    v = (double)clampf(f, 0.0f, 1.0f) * (double)max + 0.5;

    if (v <= 0.0) return 0u;
    if (v >= (double)max) return max;

    return (GLuint)v;
}

static GLint float_to_snorm(GLfloat f, GLuint bits)
{
    GLint max = (bits >= 32) ? 0x7FFFFFFF : (GLint)((1u << (bits - 1)) - 1u);
    double s;

    if (f != f) f = 0.0f;

    s = (double)clampf(f, -1.0f, 1.0f) * (double)max;
    s = s < 0.0 ? s - 0.5 : s + 0.5;

    if (s <= (double)-max) return -max;
    if (s >= (double)max)  return max;

    return (GLint)s;
}

static GLfloat srgb_to_linear(GLfloat c)
{
    return (c <= 0.04045f) ? (c / 12.92f) : powf((c + 0.055f) / 1.055f, 2.4f);
}

GLfloat mglHalfToFloat(GLushort h)
{
    GLuint sign = (GLuint)(h >> 15) & 0x1u;
    GLuint exp  = (GLuint)(h >> 10) & 0x1Fu;
    GLuint mant = (GLuint)h & 0x3FFu;
    GLuint bits;

    if (exp == 0)
    {
        if (mant == 0)
        {
            bits = sign << 31;                       // +-0
        }
        else
        {
            // subnormal: normalise it
            exp = 127 - 15 + 1;
            while ((mant & 0x400u) == 0)
            {
                mant <<= 1;
                exp--;
            }
            mant &= 0x3FFu;
            bits = (sign << 31) | (exp << 23) | (mant << 13);
        }
    }
    else if (exp == 31)
    {
        bits = (sign << 31) | 0x7F800000u | (mant << 13);   // Inf / NaN
    }
    else
    {
        bits = (sign << 31) | ((exp - 15 + 127) << 23) | (mant << 13);
    }

    GLfloat out;
    memcpy(&out, &bits, sizeof out);

    return out;
}

GLushort mglFloatToHalf(GLfloat f)
{
    GLuint bits;
    memcpy(&bits, &f, sizeof bits);

    GLuint sign = (bits >> 31) & 0x1u;
    GLint  exp  = (GLint)((bits >> 23) & 0xFFu) - 127 + 15;
    GLuint mant = bits & 0x7FFFFFu;

    if (((bits >> 23) & 0xFFu) == 0xFFu)
    {
        // Inf or NaN; keep NaN non-zero so it stays NaN
        return (GLushort)((sign << 15) | 0x7C00u | (mant ? 0x200u : 0u));
    }

    if (exp >= 31)
        return (GLushort)((sign << 15) | 0x7C00u);          // overflow to Inf

    if (exp <= 0)
    {
        if (exp < -10)
            return (GLushort)(sign << 15);                  // underflow to zero

        mant |= 0x800000u;                                  // restore implicit 1
        GLuint shift = (GLuint)(14 - exp);
        GLuint half  = mant >> shift;

        // round to nearest even
        if ((mant >> (shift - 1)) & 1u)
            half++;

        return (GLushort)((sign << 15) | half);
    }

    GLushort half = (GLushort)((sign << 15) | ((GLuint)exp << 10) | (mant >> 13));

    // round to nearest even: bit 12 is the guard, bits 0-11 the sticky, and
    // bit 13 is the low bit of the result
    if ((mant & 0x1000u) && (mant & 0x2FFFu))
        half++;

    return half;
}

// 10/11-bit unsigned floats used by R11F_G11F_B10F. No sign bit.
static GLfloat smallfloat_to_float(GLuint v, GLuint mant_bits, GLuint exp_bits)
{
    GLuint exp_mask = (1u << exp_bits) - 1u;
    GLuint exp  = (v >> mant_bits) & exp_mask;
    GLuint mant = v & ((1u << mant_bits) - 1u);
    GLint  bias = (GLint)((1u << (exp_bits - 1)) - 1u);

    if (exp == 0)
    {
        if (mant == 0) return 0.0f;

        return ldexpf((GLfloat)mant / (GLfloat)(1u << mant_bits), 1 - bias);
    }

    if (exp == exp_mask)
        return mant ? NAN : INFINITY;

    return ldexpf(1.0f + (GLfloat)mant / (GLfloat)(1u << mant_bits), (GLint)exp - bias);
}

// same decoder, for the packed vertex attribute formats
GLfloat mglSmallFloatToFloat(GLuint v, GLuint mant_bits, GLuint exp_bits)
{
    return smallfloat_to_float(v, mant_bits, exp_bits);
}

static GLuint float_to_smallfloat(GLfloat f, GLuint mant_bits, GLuint exp_bits)
{
    GLuint exp_mask = (1u << exp_bits) - 1u;
    GLint  bias = (GLint)((1u << (exp_bits - 1)) - 1u);
    int e;
    GLfloat m;

    if (f != f) return (exp_mask << mant_bits) | 1u;        // NaN
    if (f <= 0.0f) return 0u;                               // no sign bit
    if (isinf(f)) return exp_mask << mant_bits;

    m = frexpf(f, &e);      // f = m * 2^e, m in [0.5,1)
    m *= 2.0f; e -= 1;      // now m in [1,2)

    if (e + bias <= 0)
    {
        // subnormal
        GLuint mant = (GLuint)(ldexpf(f, (GLint)mant_bits + bias - 1) + 0.5f);
        return mant & ((1u << mant_bits) - 1u);
    }

    if (e + bias >= (GLint)exp_mask)
        return exp_mask << mant_bits;                       // overflow to Inf

    GLuint mant = (GLuint)((m - 1.0f) * (GLfloat)(1u << mant_bits) + 0.5f);
    GLuint exp  = (GLuint)(e + bias);

    if (mant >> mant_bits)                                   // rounding carried
    {
        mant = 0;
        exp++;
        if (exp >= exp_mask) return exp_mask << mant_bits;
    }

    return (exp << mant_bits) | mant;
}

/* ---------- native format table ---------- */

GLuint mglNativeFormatBytesPerPixel(MGLNativeFormat fmt)
{
    switch(fmt)
    {
        case MGL_NF_R8_UNORM: case MGL_NF_R8_SNORM:
        case MGL_NF_R8_UINT:  case MGL_NF_R8_SINT:
        case MGL_NF_STENCIL8:
            return 1;

        case MGL_NF_RG8_UNORM: case MGL_NF_RG8_SNORM:
        case MGL_NF_RG8_UINT:  case MGL_NF_RG8_SINT:
        case MGL_NF_R16_UNORM: case MGL_NF_R16_SNORM:
        case MGL_NF_R16_UINT:  case MGL_NF_R16_SINT:
        case MGL_NF_R16_FLOAT:
        case MGL_NF_DEPTH16_UNORM:
        case MGL_NF_B5G6R5_UNORM:
        case MGL_NF_A1BGR5_UNORM:
        case MGL_NF_ABGR4_UNORM:
            return 2;

        case MGL_NF_RGBA8_UNORM: case MGL_NF_BGRA8_UNORM:
        case MGL_NF_RGBA8_UNORM_SRGB: case MGL_NF_BGRA8_UNORM_SRGB:
        case MGL_NF_RGBA8_SNORM:
        case MGL_NF_RGBA8_UINT:  case MGL_NF_RGBA8_SINT:
        case MGL_NF_RG16_UNORM:  case MGL_NF_RG16_SNORM:
        case MGL_NF_RG16_UINT:   case MGL_NF_RG16_SINT:
        case MGL_NF_RG16_FLOAT:
        case MGL_NF_R32_UINT: case MGL_NF_R32_SINT: case MGL_NF_R32_FLOAT:
        case MGL_NF_RGB10A2_UNORM: case MGL_NF_RGB10A2_UINT:
        case MGL_NF_RG11B10_FLOAT: case MGL_NF_RGB9E5_FLOAT:
        case MGL_NF_DEPTH32_FLOAT:
        case MGL_NF_DEPTH24_UNORM_STENCIL8:
            return 4;

        case MGL_NF_RGBA16_UNORM: case MGL_NF_RGBA16_SNORM:
        case MGL_NF_RGBA16_UINT:  case MGL_NF_RGBA16_SINT:
        case MGL_NF_RGBA16_FLOAT:
        case MGL_NF_RG32_UINT: case MGL_NF_RG32_SINT: case MGL_NF_RG32_FLOAT:
        case MGL_NF_DEPTH32_FLOAT_STENCIL8:
            return 8;

        case MGL_NF_RGBA32_UINT: case MGL_NF_RGBA32_SINT: case MGL_NF_RGBA32_FLOAT:
            return 16;

        default:
            return 0;
    }
}

GLboolean mglNativeFormatIsInteger(MGLNativeFormat fmt)
{
    switch(fmt)
    {
        case MGL_NF_R8_UINT:  case MGL_NF_RG8_UINT:  case MGL_NF_RGBA8_UINT:
        case MGL_NF_R8_SINT:  case MGL_NF_RG8_SINT:  case MGL_NF_RGBA8_SINT:
        case MGL_NF_R16_UINT: case MGL_NF_RG16_UINT: case MGL_NF_RGBA16_UINT:
        case MGL_NF_R16_SINT: case MGL_NF_RG16_SINT: case MGL_NF_RGBA16_SINT:
        case MGL_NF_R32_UINT: case MGL_NF_RG32_UINT: case MGL_NF_RGBA32_UINT:
        case MGL_NF_R32_SINT: case MGL_NF_RG32_SINT: case MGL_NF_RGBA32_SINT:
        case MGL_NF_RGB10A2_UINT:
            return GL_TRUE;
        default:
            return GL_FALSE;
    }
}

GLboolean mglNativeFormatIsDepth(MGLNativeFormat fmt)
{
    return (fmt == MGL_NF_DEPTH16_UNORM || fmt == MGL_NF_DEPTH32_FLOAT ||
            fmt == MGL_NF_DEPTH24_UNORM_STENCIL8 || fmt == MGL_NF_DEPTH32_FLOAT_STENCIL8);
}

GLboolean mglNativeFormatIsStencil(MGLNativeFormat fmt)
{
    return (fmt == MGL_NF_STENCIL8 ||
            fmt == MGL_NF_DEPTH24_UNORM_STENCIL8 || fmt == MGL_NF_DEPTH32_FLOAT_STENCIL8);
}

/* ---------- decode ---------- */

static GLushort rd16(const GLubyte *p) { GLushort v; memcpy(&v, p, 2); return v; }
static GLuint   rd32(const GLubyte *p) { GLuint   v; memcpy(&v, p, 4); return v; }
static GLfloat  rdf (const GLubyte *p) { GLfloat  v; memcpy(&v, p, 4); return v; }

static void texel_zero(MGLTexel *t)
{
    t->f[0] = t->f[1] = t->f[2] = 0.0f; t->f[3] = 1.0f;
    t->u[0] = t->u[1] = t->u[2] = 0u;   t->u[3] = 1u;
    t->i[0] = t->i[1] = t->i[2] = 0;    t->i[3] = 1;
    t->is_uint = GL_FALSE;
    t->is_sint = GL_FALSE;
}

static GLboolean decode_native(const GLubyte *p, MGLNativeFormat fmt, MGLTexel *t)
{
    GLuint n, k;

    texel_zero(t);

    switch(fmt)
    {
        /* 8 bit unorm */
        case MGL_NF_R8_UNORM:    n = 1; goto u8;
        case MGL_NF_RG8_UNORM:   n = 2; goto u8;
        case MGL_NF_RGBA8_UNORM: n = 4; goto u8;
        u8:
            for (k = 0; k < n; k++) t->f[k] = unorm_to_float(p[k], 8);
            if (n < 4) t->f[3] = 1.0f;
            return GL_TRUE;

        case MGL_NF_BGRA8_UNORM:
            t->f[0] = unorm_to_float(p[2], 8);
            t->f[1] = unorm_to_float(p[1], 8);
            t->f[2] = unorm_to_float(p[0], 8);
            t->f[3] = unorm_to_float(p[3], 8);
            return GL_TRUE;

        // GL 4.6 section 8.11.4: a readback returns what is stored. The sRGB
        // decode belongs to sampling, and the encoder never applied one, so
        // decoding here turned every round trip through an sRGB format dark.
        case MGL_NF_RGBA8_UNORM_SRGB:
            for (k = 0; k < 4; k++) t->f[k] = unorm_to_float(p[k], 8);
            return GL_TRUE;

        case MGL_NF_BGRA8_UNORM_SRGB:
            t->f[0] = unorm_to_float(p[2], 8);
            t->f[1] = unorm_to_float(p[1], 8);
            t->f[2] = unorm_to_float(p[0], 8);
            t->f[3] = unorm_to_float(p[3], 8);
            return GL_TRUE;

        /* 8 bit snorm */
        case MGL_NF_R8_SNORM:    n = 1; goto s8;
        case MGL_NF_RG8_SNORM:   n = 2; goto s8;
        case MGL_NF_RGBA8_SNORM: n = 4; goto s8;
        s8:
            for (k = 0; k < n; k++) t->f[k] = snorm_to_float((GLint)(GLbyte)p[k], 8);
            if (n < 4) t->f[3] = 1.0f;
            return GL_TRUE;

        /* 8 bit integer */
        case MGL_NF_R8_UINT:    n = 1; goto ui8;
        case MGL_NF_RG8_UINT:   n = 2; goto ui8;
        case MGL_NF_RGBA8_UINT: n = 4; goto ui8;
        ui8:
            for (k = 0; k < n; k++) t->u[k] = p[k];
            t->is_uint = GL_TRUE;
            return GL_TRUE;

        case MGL_NF_R8_SINT:    n = 1; goto si8;
        case MGL_NF_RG8_SINT:   n = 2; goto si8;
        case MGL_NF_RGBA8_SINT: n = 4; goto si8;
        si8:
            for (k = 0; k < n; k++) t->i[k] = (GLbyte)p[k];
            t->is_sint = GL_TRUE;
            return GL_TRUE;

        /* 16 bit */
        case MGL_NF_R16_UNORM:    n = 1; goto u16;
        case MGL_NF_RG16_UNORM:   n = 2; goto u16;
        case MGL_NF_RGBA16_UNORM: n = 4; goto u16;
        u16:
            for (k = 0; k < n; k++) t->f[k] = unorm_to_float(rd16(p + 2*k), 16);
            if (n < 4) t->f[3] = 1.0f;
            return GL_TRUE;

        case MGL_NF_R16_SNORM:    n = 1; goto s16;
        case MGL_NF_RG16_SNORM:   n = 2; goto s16;
        case MGL_NF_RGBA16_SNORM: n = 4; goto s16;
        s16:
            for (k = 0; k < n; k++) t->f[k] = snorm_to_float((GLshort)rd16(p + 2*k), 16);
            if (n < 4) t->f[3] = 1.0f;
            return GL_TRUE;

        case MGL_NF_R16_UINT:    n = 1; goto ui16;
        case MGL_NF_RG16_UINT:   n = 2; goto ui16;
        case MGL_NF_RGBA16_UINT: n = 4; goto ui16;
        ui16:
            for (k = 0; k < n; k++) t->u[k] = rd16(p + 2*k);
            t->is_uint = GL_TRUE;
            return GL_TRUE;

        case MGL_NF_R16_SINT:    n = 1; goto si16;
        case MGL_NF_RG16_SINT:   n = 2; goto si16;
        case MGL_NF_RGBA16_SINT: n = 4; goto si16;
        si16:
            for (k = 0; k < n; k++) t->i[k] = (GLshort)rd16(p + 2*k);
            t->is_sint = GL_TRUE;
            return GL_TRUE;

        case MGL_NF_R16_FLOAT:    n = 1; goto h16;
        case MGL_NF_RG16_FLOAT:   n = 2; goto h16;
        case MGL_NF_RGBA16_FLOAT: n = 4; goto h16;
        h16:
            for (k = 0; k < n; k++) t->f[k] = mglHalfToFloat(rd16(p + 2*k));
            if (n < 4) t->f[3] = 1.0f;
            return GL_TRUE;

        /* 32 bit */
        case MGL_NF_R32_UINT:    n = 1; goto ui32;
        case MGL_NF_RG32_UINT:   n = 2; goto ui32;
        case MGL_NF_RGBA32_UINT: n = 4; goto ui32;
        ui32:
            for (k = 0; k < n; k++) t->u[k] = rd32(p + 4*k);
            t->is_uint = GL_TRUE;
            return GL_TRUE;

        case MGL_NF_R32_SINT:    n = 1; goto si32;
        case MGL_NF_RG32_SINT:   n = 2; goto si32;
        case MGL_NF_RGBA32_SINT: n = 4; goto si32;
        si32:
            for (k = 0; k < n; k++) t->i[k] = (GLint)rd32(p + 4*k);
            t->is_sint = GL_TRUE;
            return GL_TRUE;

        case MGL_NF_R32_FLOAT:    n = 1; goto f32;
        case MGL_NF_RG32_FLOAT:   n = 2; goto f32;
        case MGL_NF_RGBA32_FLOAT: n = 4; goto f32;
        f32:
            for (k = 0; k < n; k++) t->f[k] = rdf(p + 4*k);
            if (n < 4) t->f[3] = 1.0f;
            return GL_TRUE;

        /* packed */
        case MGL_NF_B5G6R5_UNORM:
        {
            GLushort v; memcpy(&v, p, 2);
            texel_zero(t);
            t->f[0] = unorm_to_float((v >> 11) & 0x1Fu, 5);
            t->f[1] = unorm_to_float((v >> 5) & 0x3Fu, 6);
            t->f[2] = unorm_to_float(v & 0x1Fu, 5);
            t->f[3] = 1.0f;
            return GL_TRUE;
        }

        // Metal lists the components of a packed format from the low bits up,
        // so A1BGR5 has red at the top -- the same place GL's 5_5_5_1 puts it.
        case MGL_NF_A1BGR5_UNORM:
        {
            GLushort v; memcpy(&v, p, 2);
            texel_zero(t);
            t->f[0] = unorm_to_float((v >> 11) & 0x1Fu, 5);
            t->f[1] = unorm_to_float((v >> 6) & 0x1Fu, 5);
            t->f[2] = unorm_to_float((v >> 1) & 0x1Fu, 5);
            t->f[3] = unorm_to_float(v & 0x1u, 1);
            return GL_TRUE;
        }

        case MGL_NF_ABGR4_UNORM:
        {
            GLushort v; memcpy(&v, p, 2);
            texel_zero(t);
            t->f[0] = unorm_to_float((v >> 12) & 0xFu, 4);
            t->f[1] = unorm_to_float((v >> 8) & 0xFu, 4);
            t->f[2] = unorm_to_float((v >> 4) & 0xFu, 4);
            t->f[3] = unorm_to_float(v & 0xFu, 4);
            return GL_TRUE;
        }

        case MGL_NF_RGB10A2_UNORM:
        {
            GLuint v = rd32(p);
            t->f[0] = unorm_to_float(v & 0x3FFu, 10);
            t->f[1] = unorm_to_float((v >> 10) & 0x3FFu, 10);
            t->f[2] = unorm_to_float((v >> 20) & 0x3FFu, 10);
            t->f[3] = unorm_to_float((v >> 30) & 0x3u, 2);
            return GL_TRUE;
        }

        case MGL_NF_RGB10A2_UINT:
        {
            GLuint v = rd32(p);
            t->u[0] = v & 0x3FFu;
            t->u[1] = (v >> 10) & 0x3FFu;
            t->u[2] = (v >> 20) & 0x3FFu;
            t->u[3] = (v >> 30) & 0x3u;
            t->is_uint = GL_TRUE;
            return GL_TRUE;
        }

        case MGL_NF_RG11B10_FLOAT:
        {
            GLuint v = rd32(p);
            t->f[0] = smallfloat_to_float(v & 0x7FFu, 6, 5);
            t->f[1] = smallfloat_to_float((v >> 11) & 0x7FFu, 6, 5);
            t->f[2] = smallfloat_to_float((v >> 22) & 0x3FFu, 5, 5);
            t->f[3] = 1.0f;
            return GL_TRUE;
        }

        case MGL_NF_RGB9E5_FLOAT:
        {
            GLuint v = rd32(p);
            GLint  e = (GLint)((v >> 27) & 0x1Fu) - 15 - 9;
            GLfloat s = ldexpf(1.0f, e);
            t->f[0] = (GLfloat)(v & 0x1FFu) * s;
            t->f[1] = (GLfloat)((v >> 9) & 0x1FFu) * s;
            t->f[2] = (GLfloat)((v >> 18) & 0x1FFu) * s;
            t->f[3] = 1.0f;
            return GL_TRUE;
        }

        /* depth / stencil */
        case MGL_NF_DEPTH16_UNORM:
            t->f[0] = unorm_to_float(rd16(p), 16);
            return GL_TRUE;

        case MGL_NF_DEPTH32_FLOAT:
            t->f[0] = rdf(p);
            return GL_TRUE;

        case MGL_NF_STENCIL8:
            t->u[0] = p[0];
            t->is_uint = GL_TRUE;
            return GL_TRUE;

        case MGL_NF_DEPTH24_UNORM_STENCIL8:
        {
            GLuint v = rd32(p);
            t->f[0] = unorm_to_float(v & 0x00FFFFFFu, 24);
            t->u[1] = (v >> 24) & 0xFFu;
            return GL_TRUE;
        }

        case MGL_NF_DEPTH32_FLOAT_STENCIL8:
            t->f[0] = rdf(p);
            t->u[1] = p[4];
            return GL_TRUE;

        default:
            return GL_FALSE;
    }
}

/* ---------- encode ---------- */

GLuint mglComponentsForFormat(GLenum format)
{
    switch(format)
    {
        case GL_RED: case GL_GREEN: case GL_BLUE: case GL_ALPHA:
        case GL_RED_INTEGER: case GL_GREEN_INTEGER: case GL_BLUE_INTEGER:
        case GL_DEPTH_COMPONENT: case GL_STENCIL_INDEX: case GL_LUMINANCE:
            return 1;

        case GL_RG: case GL_RG_INTEGER: case GL_LUMINANCE_ALPHA:
        case GL_DEPTH_STENCIL:
            return 2;

        case GL_RGB: case GL_BGR: case GL_RGB_INTEGER: case GL_BGR_INTEGER:
            return 3;

        case GL_RGBA: case GL_BGRA: case GL_RGBA_INTEGER: case GL_BGRA_INTEGER:
            return 4;

        default:
            return 0;
    }
}

static GLboolean format_is_integer(GLenum format)
{
    switch(format)
    {
        case GL_RED_INTEGER: case GL_GREEN_INTEGER: case GL_BLUE_INTEGER:
        case GL_RG_INTEGER: case GL_RGB_INTEGER: case GL_BGR_INTEGER:
        case GL_RGBA_INTEGER: case GL_BGRA_INTEGER:
        case GL_STENCIL_INDEX:
            return GL_TRUE;
        default:
            return GL_FALSE;
    }
}

// Packed types carry their own component count, so they only pair with matching formats.
static GLuint packed_type_size(GLenum type)
{
    switch(type)
    {
        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return 1;

        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return 2;

        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
        case GL_UNSIGNED_INT_24_8:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return 4;

        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
            return 8;

        default:
            return 0;
    }
}

static GLuint plain_type_size(GLenum type)
{
    switch(type)
    {
        case GL_UNSIGNED_BYTE: case GL_BYTE:            return 1;
        case GL_UNSIGNED_SHORT: case GL_SHORT: case GL_HALF_FLOAT: return 2;
        case GL_UNSIGNED_INT: case GL_INT: case GL_FLOAT:          return 4;
        default: return 0;
    }
}

GLuint mglPackedPixelSize(GLenum format, GLenum type)
{
    GLuint comps = mglComponentsForFormat(format);
    GLuint psz;

    if (comps == 0)
        return 0;

    psz = packed_type_size(type);
    if (psz)
        return psz;

    psz = plain_type_size(type);
    if (psz == 0)
        return 0;

    return psz * comps;
}

// Pull the components format wants out of the texel, in destination order.
static void select_components(const MGLTexel *t, GLenum format, GLfloat *fo, GLint *io, GLuint *uo, GLuint *count)
{
    #define TAKE(idx) do { fo[n] = t->f[idx]; io[n] = t->i[idx]; uo[n] = t->u[idx]; n++; } while(0)
    GLuint n = 0;

    switch(format)
    {
        case GL_RED: case GL_RED_INTEGER:
        case GL_DEPTH_COMPONENT: case GL_LUMINANCE:
            TAKE(0); break;

        case GL_GREEN: case GL_GREEN_INTEGER: TAKE(1); break;
        case GL_BLUE:  case GL_BLUE_INTEGER:  TAKE(2); break;
        case GL_ALPHA:                        TAKE(3); break;

        case GL_RG: case GL_RG_INTEGER:       TAKE(0); TAKE(1); break;
        case GL_LUMINANCE_ALPHA:              TAKE(0); TAKE(3); break;
        case GL_DEPTH_STENCIL:                TAKE(0); TAKE(1); break;

        case GL_RGB: case GL_RGB_INTEGER:     TAKE(0); TAKE(1); TAKE(2); break;
        case GL_BGR: case GL_BGR_INTEGER:     TAKE(2); TAKE(1); TAKE(0); break;

        case GL_RGBA: case GL_RGBA_INTEGER:   TAKE(0); TAKE(1); TAKE(2); TAKE(3); break;
        case GL_BGRA: case GL_BGRA_INTEGER:   TAKE(2); TAKE(1); TAKE(0); TAKE(3); break;

        case GL_STENCIL_INDEX:
            fo[0] = t->f[1]; io[0] = t->i[1]; uo[0] = t->u[1]; n = 1;
            break;

        default:
            break;
    }

    *count = n;
    #undef TAKE
}

// A packed field holds fewer bits than the integer it is given, and GL clamps
// to what fits rather than dropping the high bits.
static GLuint clamp_field(GLuint v, GLuint max)
{
    return v > max ? max : v;
}

static GLboolean encode_packed(GLubyte *d, GLenum format, GLenum type, const MGLTexel *t)
{
    GLuint comps = mglComponentsForFormat(format);
    GLboolean bgr = (format == GL_BGR || format == GL_BGRA ||
                     format == GL_BGR_INTEGER || format == GL_BGRA_INTEGER);
    GLfloat r = t->f[0], g = t->f[1], b = t->f[2], a = t->f[3];
    GLuint v;

    if (bgr) { GLfloat tmp = r; r = b; b = tmp; }

    // An integer client format carries raw integers in the packed fields, not
    // normalised values. Packing them through float_to_unorm made every
    // integer readback through a packed type come back as 0 or saturated.
    if (format_is_integer(format))
    {
        GLuint ui[4];
        GLuint k;

        for (k = 0; k < 4; k++)
            ui[k] = t->is_sint ? (t->i[k] < 0 ? 0u : (GLuint)t->i[k]) : t->u[k];

        if (bgr) { GLuint tmp = ui[0]; ui[0] = ui[2]; ui[2] = tmp; }

        switch(type)
        {
            case GL_UNSIGNED_BYTE_3_3_2:
                if (comps != 3) return GL_FALSE;
                d[0] = (GLubyte)(((clamp_field(ui[0], 0x7u)) << 5) | ((clamp_field(ui[1], 0x7u)) << 2) | (clamp_field(ui[2], 0x3u)));
                return GL_TRUE;

            case GL_UNSIGNED_BYTE_2_3_3_REV:
                if (comps != 3) return GL_FALSE;
                d[0] = (GLubyte)(((clamp_field(ui[2], 0x3u)) << 6) | ((clamp_field(ui[1], 0x7u)) << 3) | (clamp_field(ui[0], 0x7u)));
                return GL_TRUE;

            case GL_UNSIGNED_SHORT_5_6_5:
            {
                GLushort sv;
                if (comps != 3) return GL_FALSE;
                sv = (GLushort)(((clamp_field(ui[0], 0x1Fu)) << 11) | ((clamp_field(ui[1], 0x3Fu)) << 5) | (clamp_field(ui[2], 0x1Fu)));
                memcpy(d, &sv, 2); return GL_TRUE;
            }

            case GL_UNSIGNED_SHORT_5_6_5_REV:
            {
                GLushort sv;
                if (comps != 3) return GL_FALSE;
                sv = (GLushort)(((clamp_field(ui[2], 0x1Fu)) << 11) | ((clamp_field(ui[1], 0x3Fu)) << 5) | (clamp_field(ui[0], 0x1Fu)));
                memcpy(d, &sv, 2); return GL_TRUE;
            }

            case GL_UNSIGNED_SHORT_4_4_4_4:
            {
                GLushort sv;
                if (comps != 4) return GL_FALSE;
                sv = (GLushort)(((clamp_field(ui[0], 0xFu)) << 12) | ((clamp_field(ui[1], 0xFu)) << 8) |
                                ((clamp_field(ui[2], 0xFu)) << 4)  | (clamp_field(ui[3], 0xFu)));
                memcpy(d, &sv, 2); return GL_TRUE;
            }

            case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            {
                GLushort sv;
                if (comps != 4) return GL_FALSE;
                sv = (GLushort)(((clamp_field(ui[3], 0xFu)) << 12) | ((clamp_field(ui[2], 0xFu)) << 8) |
                                ((clamp_field(ui[1], 0xFu)) << 4)  | (clamp_field(ui[0], 0xFu)));
                memcpy(d, &sv, 2); return GL_TRUE;
            }

            case GL_UNSIGNED_SHORT_5_5_5_1:
            {
                GLushort sv;
                if (comps != 4) return GL_FALSE;
                sv = (GLushort)(((clamp_field(ui[0], 0x1Fu)) << 11) | ((clamp_field(ui[1], 0x1Fu)) << 6) |
                                ((clamp_field(ui[2], 0x1Fu)) << 1)  | (clamp_field(ui[3], 0x1u)));
                memcpy(d, &sv, 2); return GL_TRUE;
            }

            case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            {
                GLushort sv;
                if (comps != 4) return GL_FALSE;
                sv = (GLushort)(((clamp_field(ui[3], 0x1u)) << 15) | ((clamp_field(ui[2], 0x1Fu)) << 10) |
                                ((clamp_field(ui[1], 0x1Fu)) << 5)  | (clamp_field(ui[0], 0x1Fu)));
                memcpy(d, &sv, 2); return GL_TRUE;
            }

            case GL_UNSIGNED_INT_8_8_8_8:
                if (comps != 4) return GL_FALSE;
                v = ((clamp_field(ui[0], 0xFFu)) << 24) | ((clamp_field(ui[1], 0xFFu)) << 16) |
                    ((clamp_field(ui[2], 0xFFu)) << 8)  | (clamp_field(ui[3], 0xFFu));
                memcpy(d, &v, 4); return GL_TRUE;

            case GL_UNSIGNED_INT_8_8_8_8_REV:
                if (comps != 4) return GL_FALSE;
                v = ((clamp_field(ui[3], 0xFFu)) << 24) | ((clamp_field(ui[2], 0xFFu)) << 16) |
                    ((clamp_field(ui[1], 0xFFu)) << 8)  | (clamp_field(ui[0], 0xFFu));
                memcpy(d, &v, 4); return GL_TRUE;

            case GL_UNSIGNED_INT_10_10_10_2:
                if (comps != 4) return GL_FALSE;
                v = ((clamp_field(ui[0], 0x3FFu)) << 22) | ((clamp_field(ui[1], 0x3FFu)) << 12) |
                    ((clamp_field(ui[2], 0x3FFu)) << 2)  | (clamp_field(ui[3], 0x3u));
                memcpy(d, &v, 4); return GL_TRUE;

            case GL_UNSIGNED_INT_2_10_10_10_REV:
                if (comps != 4) return GL_FALSE;
                v = ((clamp_field(ui[3], 0x3u)) << 30) | ((clamp_field(ui[2], 0x3FFu)) << 20) |
                    ((clamp_field(ui[1], 0x3FFu)) << 10) | (clamp_field(ui[0], 0x3FFu));
                memcpy(d, &v, 4); return GL_TRUE;

            default:
                return GL_FALSE;
        }
    }

    switch(type)
    {
        case GL_UNSIGNED_BYTE_3_3_2:
            if (comps != 3) return GL_FALSE;
            d[0] = (GLubyte)((float_to_unorm(r,3) << 5) | (float_to_unorm(g,3) << 2) | float_to_unorm(b,2));
            return GL_TRUE;

        case GL_UNSIGNED_BYTE_2_3_3_REV:
            if (comps != 3) return GL_FALSE;
            d[0] = (GLubyte)((float_to_unorm(b,2) << 6) | (float_to_unorm(g,3) << 3) | float_to_unorm(r,3));
            return GL_TRUE;

        case GL_UNSIGNED_SHORT_5_6_5:
        {
            if (comps != 3) return GL_FALSE;
            GLushort s = (GLushort)((float_to_unorm(r,5) << 11) | (float_to_unorm(g,6) << 5) | float_to_unorm(b,5));
            memcpy(d, &s, 2); return GL_TRUE;
        }

        case GL_UNSIGNED_SHORT_5_6_5_REV:
        {
            if (comps != 3) return GL_FALSE;
            GLushort s = (GLushort)((float_to_unorm(b,5) << 11) | (float_to_unorm(g,6) << 5) | float_to_unorm(r,5));
            memcpy(d, &s, 2); return GL_TRUE;
        }

        case GL_UNSIGNED_SHORT_4_4_4_4:
        {
            if (comps != 4) return GL_FALSE;
            GLushort s = (GLushort)((float_to_unorm(r,4) << 12) | (float_to_unorm(g,4) << 8) |
                                    (float_to_unorm(b,4) << 4)  | float_to_unorm(a,4));
            memcpy(d, &s, 2); return GL_TRUE;
        }

        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        {
            if (comps != 4) return GL_FALSE;
            GLushort s = (GLushort)((float_to_unorm(a,4) << 12) | (float_to_unorm(b,4) << 8) |
                                    (float_to_unorm(g,4) << 4)  | float_to_unorm(r,4));
            memcpy(d, &s, 2); return GL_TRUE;
        }

        case GL_UNSIGNED_SHORT_5_5_5_1:
        {
            if (comps != 4) return GL_FALSE;
            GLushort s = (GLushort)((float_to_unorm(r,5) << 11) | (float_to_unorm(g,5) << 6) |
                                    (float_to_unorm(b,5) << 1)  | float_to_unorm(a,1));
            memcpy(d, &s, 2); return GL_TRUE;
        }

        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        {
            if (comps != 4) return GL_FALSE;
            GLushort s = (GLushort)((float_to_unorm(a,1) << 15) | (float_to_unorm(b,5) << 10) |
                                    (float_to_unorm(g,5) << 5)  | float_to_unorm(r,5));
            memcpy(d, &s, 2); return GL_TRUE;
        }

        case GL_UNSIGNED_INT_8_8_8_8:
            if (comps != 4) return GL_FALSE;
            v = (float_to_unorm(r,8) << 24) | (float_to_unorm(g,8) << 16) |
                (float_to_unorm(b,8) << 8)  | float_to_unorm(a,8);
            memcpy(d, &v, 4); return GL_TRUE;

        case GL_UNSIGNED_INT_8_8_8_8_REV:
            if (comps != 4) return GL_FALSE;
            v = (float_to_unorm(a,8) << 24) | (float_to_unorm(b,8) << 16) |
                (float_to_unorm(g,8) << 8)  | float_to_unorm(r,8);
            memcpy(d, &v, 4); return GL_TRUE;

        case GL_UNSIGNED_INT_10_10_10_2:
            if (comps != 4) return GL_FALSE;
            v = (float_to_unorm(r,10) << 22) | (float_to_unorm(g,10) << 12) |
                (float_to_unorm(b,10) << 2)  | float_to_unorm(a,2);
            memcpy(d, &v, 4); return GL_TRUE;

        case GL_UNSIGNED_INT_2_10_10_10_REV:
            if (comps != 4) return GL_FALSE;
            v = (float_to_unorm(a,2) << 30) | (float_to_unorm(b,10) << 20) |
                (float_to_unorm(g,10) << 10) | float_to_unorm(r,10);
            memcpy(d, &v, 4); return GL_TRUE;

        case GL_UNSIGNED_INT_24_8:
            if (format != GL_DEPTH_STENCIL) return GL_FALSE;
            v = (float_to_unorm(t->f[0], 24) << 8) | (t->u[1] & 0xFFu);
            memcpy(d, &v, 4); return GL_TRUE;

        case GL_UNSIGNED_INT_10F_11F_11F_REV:
            if (comps != 3) return GL_FALSE;
            v = float_to_smallfloat(r, 6, 5) |
                (float_to_smallfloat(g, 6, 5) << 11) |
                (float_to_smallfloat(b, 5, 5) << 22);
            memcpy(d, &v, 4); return GL_TRUE;

        case GL_UNSIGNED_INT_5_9_9_9_REV:
        {
            if (comps != 3) return GL_FALSE;
            // shared exponent, per the GL spec's RGB9_E5 rules
            const GLfloat max_val = 65408.0f;
            GLfloat rc = clampf(r, 0.0f, max_val);
            GLfloat gc = clampf(g, 0.0f, max_val);
            GLfloat bc = clampf(b, 0.0f, max_val);
            GLfloat maxc = rc > gc ? (rc > bc ? rc : bc) : (gc > bc ? gc : bc);
            GLint e = -16;

            if (maxc > 0.0f)
            {
                int ex;
                frexpf(maxc, &ex);
                e = ex - 1;
                if (e < -16) e = -16;
            }

            GLfloat denom = ldexpf(1.0f, e - 9 + 1);
            GLint maxs = (GLint)(maxc / denom + 0.5f);

            if (maxs == 512) { e++; denom *= 2.0f; }

            GLuint rs = (GLuint)clampf(rc / denom + 0.5f, 0.0f, 511.0f);
            GLuint gs = (GLuint)clampf(gc / denom + 0.5f, 0.0f, 511.0f);
            GLuint bs = (GLuint)clampf(bc / denom + 0.5f, 0.0f, 511.0f);
            GLuint es = (GLuint)(e + 15 + 1) & 0x1Fu;

            v = rs | (gs << 9) | (bs << 18) | (es << 27);
            memcpy(d, &v, 4); return GL_TRUE;
        }

        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        {
            if (format != GL_DEPTH_STENCIL) return GL_FALSE;
            GLfloat df = t->f[0];
            GLuint  s  = t->u[1] & 0xFFu;
            memcpy(d, &df, 4);
            memcpy(d + 4, &s, 4);
            return GL_TRUE;
        }

        default:
            return GL_FALSE;
    }
}

static void store_int(GLubyte *d, GLenum type, GLint iv, GLuint uv, GLboolean src_signed)
{
    // a large unsigned value would wrap negative on the way to a signed type
    if (!src_signed && uv > 0x7FFFFFFFu && (type == GL_BYTE || type == GL_SHORT || type == GL_INT))
        uv = 0x7FFFFFFFu;

    switch(type)
    {
        case GL_UNSIGNED_BYTE:  d[0] = (GLubyte)(src_signed ? (iv < 0 ? 0 : (iv > 255 ? 255 : iv)) : (uv > 255u ? 255u : uv)); break;
        case GL_BYTE:           { GLint v = src_signed ? iv : (GLint)uv; d[0] = (GLbyte)(v < -128 ? -128 : (v > 127 ? 127 : v)); break; }
        case GL_UNSIGNED_SHORT: { GLuint v = src_signed ? (GLuint)(iv < 0 ? 0 : iv) : uv; GLushort s = (GLushort)(v > 65535u ? 65535u : v); memcpy(d,&s,2); break; }
        case GL_SHORT:          { GLint v = src_signed ? iv : (GLint)uv; GLshort s = (GLshort)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v)); memcpy(d,&s,2); break; }
        case GL_UNSIGNED_INT:   { GLuint v = src_signed ? (GLuint)(iv < 0 ? 0 : iv) : uv; memcpy(d,&v,4); break; }
        case GL_INT:            { GLint v = src_signed ? iv : (GLint)uv; memcpy(d,&v,4); break; }
        default: break;
    }
}

static void store_norm(GLubyte *d, GLenum type, GLfloat fv)
{
    switch(type)
    {
        case GL_UNSIGNED_BYTE:  d[0] = (GLubyte)float_to_unorm(fv, 8); break;
        case GL_BYTE:           d[0] = (GLbyte)float_to_snorm(fv, 8); break;
        case GL_UNSIGNED_SHORT: { GLushort v = (GLushort)float_to_unorm(fv, 16); memcpy(d,&v,2); break; }
        case GL_SHORT:          { GLshort  v = (GLshort)float_to_snorm(fv, 16);  memcpy(d,&v,2); break; }
        case GL_UNSIGNED_INT:   { GLuint   v = float_to_unorm(fv, 32);           memcpy(d,&v,4); break; }
        case GL_INT:            { GLint    v = float_to_snorm(fv, 32);           memcpy(d,&v,4); break; }
        case GL_HALF_FLOAT:     { GLushort v = mglFloatToHalf(fv);               memcpy(d,&v,2); break; }
        case GL_FLOAT:          memcpy(d, &fv, 4); break;
        default: break;
    }
}

static GLboolean encode_plain(GLubyte *d, GLenum format, GLenum type, const MGLTexel *t)
{
    GLfloat fo[4]; GLint io[4]; GLuint uo[4];
    GLuint count = 0, k;
    GLuint tsz = plain_type_size(type);
    GLboolean want_int = format_is_integer(format);

    if (tsz == 0)
        return GL_FALSE;

    // integer formats only pair with integer types
    if (want_int && (type == GL_FLOAT || type == GL_HALF_FLOAT))
        return GL_FALSE;

    select_components(t, format, fo, io, uo, &count);
    if (count == 0)
        return GL_FALSE;

    for (k = 0; k < count; k++)
    {
        if (want_int)
            store_int(d + k*tsz, type, io[k], uo[k], t->is_sint);
        else if (t->is_uint)
            store_norm(d + k*tsz, type, (GLfloat)uo[k]);
        else if (t->is_sint)
            store_norm(d + k*tsz, type, (GLfloat)io[k]);
        else
            store_norm(d + k*tsz, type, fo[k]);
    }

    return GL_TRUE;
}

/* ---------- entry point ---------- */

static GLboolean identity_pair(MGLNativeFormat fmt, GLenum *format, GLenum *type);

GLboolean mglConvertPixels(const void *src, size_t src_row_pitch, MGLNativeFormat src_fmt,
                           void *dst, size_t dst_row_pitch, GLenum format, GLenum type,
                           GLsizei width, GLsizei height, GLboolean flip_vertical)
{
    GLenum same_format, same_type;
    GLuint src_bpp = mglNativeFormatBytesPerPixel(src_fmt);
    GLuint dst_bpp = mglPackedPixelSize(format, type);
    GLboolean packed = packed_type_size(type) != 0;
    GLsizei row, col;

    if (!src || !dst || src_bpp == 0 || dst_bpp == 0)
        return GL_FALSE;

    if (width < 0 || height < 0)
        return GL_FALSE;

    if (width == 0 || height == 0)
        return GL_TRUE;

    // the caller asked for the layout the texture already holds, so the bytes
    // go straight across. Decoding and re-encoding would be slower and, for a
    // format with more than one encoding per colour, not even the same bits.
    if (identity_pair(src_fmt, &same_format, &same_type) == GL_TRUE &&
        format == same_format && type == same_type && src_bpp == dst_bpp)
    {
        for (row = 0; row < height; row++)
        {
            GLsizei drow = flip_vertical ? (height - 1 - row) : row;

            memcpy((GLubyte *)dst + (size_t)drow * dst_row_pitch,
                   (const GLubyte *)src + (size_t)row * src_row_pitch,
                   (size_t)width * src_bpp);
        }

        return GL_TRUE;
    }

    for (row = 0; row < height; row++)
    {
        const GLubyte *s = (const GLubyte *)src + (size_t)row * src_row_pitch;
        GLsizei drow = flip_vertical ? (height - 1 - row) : row;
        GLubyte *d = (GLubyte *)dst + (size_t)drow * dst_row_pitch;

        for (col = 0; col < width; col++)
        {
            MGLTexel t;

            if (!decode_native(s + (size_t)col * src_bpp, src_fmt, &t))
                return GL_FALSE;

            if (packed)
            {
                if (!encode_packed(d + (size_t)col * dst_bpp, format, type, &t))
                    return GL_FALSE;
            }
            else
            {
                if (!encode_plain(d + (size_t)col * dst_bpp, format, type, &t))
                    return GL_FALSE;
            }
        }
    }

    return GL_TRUE;
}

/* ---------- the upload direction: client pixels into native storage ---------- */

// Mirror of select_components: put the components format carries back into RGBA
// slots. Anything the format does not name keeps the texel's default.
static void scatter_components(MGLTexel *t, GLenum format,
                               const GLfloat *fi, const GLint *ii, const GLuint *ui)
{
    #define PUT(idx) do { t->f[idx] = fi[n]; t->i[idx] = ii[n]; t->u[idx] = ui[n]; n++; } while(0)
    GLuint n = 0;

    switch(format)
    {
        case GL_RED: case GL_RED_INTEGER:
        case GL_DEPTH_COMPONENT: case GL_LUMINANCE:
            PUT(0); break;

        case GL_GREEN: case GL_GREEN_INTEGER: PUT(1); break;
        case GL_BLUE:  case GL_BLUE_INTEGER:  PUT(2); break;
        case GL_ALPHA:                        PUT(3); break;

        case GL_RG: case GL_RG_INTEGER:       PUT(0); PUT(1); break;
        case GL_LUMINANCE_ALPHA:              PUT(0); PUT(3); break;
        case GL_DEPTH_STENCIL:                PUT(0); PUT(1); break;

        case GL_RGB: case GL_RGB_INTEGER:     PUT(0); PUT(1); PUT(2); break;
        case GL_BGR: case GL_BGR_INTEGER:     PUT(2); PUT(1); PUT(0); break;

        case GL_RGBA: case GL_RGBA_INTEGER:   PUT(0); PUT(1); PUT(2); PUT(3); break;
        case GL_BGRA: case GL_BGRA_INTEGER:   PUT(2); PUT(1); PUT(0); PUT(3); break;

        case GL_STENCIL_INDEX:
            t->f[1] = fi[0]; t->i[1] = ii[0]; t->u[1] = ui[0];
            break;

        default:
            break;
    }
    #undef PUT
}

static GLboolean decode_plain(const GLubyte *s, GLenum format, GLenum type, MGLTexel *t)
{
    GLuint comps = mglComponentsForFormat(format);
    GLuint sz = plain_type_size(type);
    GLboolean is_int = format_is_integer(format);
    GLfloat fv[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    GLint   iv[4] = { 0, 0, 0, 0 };
    GLuint  uv[4] = { 0u, 0u, 0u, 0u };
    GLuint k;

    if (comps == 0 || sz == 0 || comps > 4)
        return GL_FALSE;

    // Each value is kept both signed and unsigned, for whichever kind of
    // integer storage it lands in. Crossing between them saturates: a large
    // unsigned value into a signed format is its maximum, not a negative
    // number, and a negative value into an unsigned format is zero.
    for (k = 0; k < comps; k++)
    {
        const GLubyte *p = s + (size_t)k * sz;

        switch(type)
        {
            case GL_UNSIGNED_BYTE:
                uv[k] = p[0]; iv[k] = (GLint)p[0];
                fv[k] = is_int ? (GLfloat)p[0] : unorm_to_float(p[0], 8);
                break;

            case GL_BYTE:
                iv[k] = (GLbyte)p[0]; uv[k] = iv[k] < 0 ? 0u : (GLuint)iv[k];
                fv[k] = is_int ? (GLfloat)iv[k] : snorm_to_float(iv[k], 8);
                break;

            case GL_UNSIGNED_SHORT:
                uv[k] = rd16(p); iv[k] = (GLint)uv[k];
                fv[k] = is_int ? (GLfloat)uv[k] : unorm_to_float(uv[k], 16);
                break;

            case GL_SHORT:
                iv[k] = (GLshort)rd16(p); uv[k] = iv[k] < 0 ? 0u : (GLuint)iv[k];
                fv[k] = is_int ? (GLfloat)iv[k] : snorm_to_float(iv[k], 16);
                break;

            // 32 bit components are normalised for a non-integer format just
            // like the 8 and 16 bit ones. Handing the raw value on saturated
            // every GL_UNSIGNED_INT upload to white.
            case GL_UNSIGNED_INT:
                uv[k] = rd32(p); iv[k] = uv[k] > 0x7FFFFFFFu ? 0x7FFFFFFF : (GLint)uv[k];
                fv[k] = is_int ? (GLfloat)uv[k] : unorm_to_float(uv[k], 32);
                break;

            case GL_INT:
                iv[k] = (GLint)rd32(p); uv[k] = iv[k] < 0 ? 0u : (GLuint)iv[k];
                fv[k] = is_int ? (GLfloat)iv[k] : snorm_to_float(iv[k], 32);
                break;

            case GL_HALF_FLOAT:
                fv[k] = mglHalfToFloat(rd16(p));
                iv[k] = (GLint)fv[k]; uv[k] = (GLuint)(fv[k] < 0.0f ? 0.0f : fv[k]);
                break;

            case GL_FLOAT:
                fv[k] = rdf(p);
                iv[k] = (GLint)fv[k]; uv[k] = (GLuint)(fv[k] < 0.0f ? 0.0f : fv[k]);
                break;

            default:
                return GL_FALSE;
        }
    }

    texel_zero(t);

    if (is_int)
    {
        t->is_uint = (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT);
        t->is_sint = !t->is_uint;
    }

    scatter_components(t, format, fv, iv, uv);

    return GL_TRUE;
}

static void wr16(GLubyte *p, GLushort v) { memcpy(p, &v, 2); }
static void wr32(GLubyte *p, GLuint v)   { memcpy(p, &v, 4); }
static void wrf (GLubyte *p, GLfloat v)  { memcpy(p, &v, 4); }

/* sRGB storage takes the client's bytes as they are: GL does not apply the
   transfer function on upload, the data is already encoded. */
static GLboolean encode_native(GLubyte *d, MGLNativeFormat fmt, const MGLTexel *t)
{
    GLuint n, k;

    switch(fmt)
    {
        case MGL_NF_R8_UNORM:    n = 1; goto u8;
        case MGL_NF_RG8_UNORM:   n = 2; goto u8;
        case MGL_NF_RGBA8_UNORM: n = 4; goto u8;
        case MGL_NF_RGBA8_UNORM_SRGB: n = 4; goto u8;
        u8:
            for (k = 0; k < n; k++) d[k] = (GLubyte)float_to_unorm(t->f[k], 8);
            return GL_TRUE;

        case MGL_NF_BGRA8_UNORM:
        case MGL_NF_BGRA8_UNORM_SRGB:
            d[0] = (GLubyte)float_to_unorm(t->f[2], 8);
            d[1] = (GLubyte)float_to_unorm(t->f[1], 8);
            d[2] = (GLubyte)float_to_unorm(t->f[0], 8);
            d[3] = (GLubyte)float_to_unorm(t->f[3], 8);
            return GL_TRUE;

        case MGL_NF_R8_SNORM:    n = 1; goto s8;
        case MGL_NF_RG8_SNORM:   n = 2; goto s8;
        case MGL_NF_RGBA8_SNORM: n = 4; goto s8;
        s8:
            for (k = 0; k < n; k++) d[k] = (GLubyte)(GLbyte)float_to_snorm(t->f[k], 8);
            return GL_TRUE;

        case MGL_NF_R8_UINT:    n = 1; goto ui8;
        case MGL_NF_RG8_UINT:   n = 2; goto ui8;
        case MGL_NF_RGBA8_UINT: n = 4; goto ui8;
        ui8:
            for (k = 0; k < n; k++) d[k] = (GLubyte)(t->u[k] > 255u ? 255u : t->u[k]);
            return GL_TRUE;

        case MGL_NF_R8_SINT:    n = 1; goto si8;
        case MGL_NF_RG8_SINT:   n = 2; goto si8;
        case MGL_NF_RGBA8_SINT: n = 4; goto si8;
        si8:
            for (k = 0; k < n; k++)
            {
                GLint v = t->i[k];
                if (v > 127) v = 127;
                if (v < -128) v = -128;
                d[k] = (GLubyte)(GLbyte)v;
            }
            return GL_TRUE;

        case MGL_NF_R16_UNORM:    n = 1; goto u16;
        case MGL_NF_RG16_UNORM:   n = 2; goto u16;
        case MGL_NF_RGBA16_UNORM: n = 4; goto u16;
        u16:
            for (k = 0; k < n; k++) wr16(d + 2*k, (GLushort)float_to_unorm(t->f[k], 16));
            return GL_TRUE;

        case MGL_NF_R16_SNORM:    n = 1; goto s16;
        case MGL_NF_RG16_SNORM:   n = 2; goto s16;
        case MGL_NF_RGBA16_SNORM: n = 4; goto s16;
        s16:
            for (k = 0; k < n; k++) wr16(d + 2*k, (GLushort)(GLshort)float_to_snorm(t->f[k], 16));
            return GL_TRUE;

        case MGL_NF_R16_UINT:    n = 1; goto ui16;
        case MGL_NF_RG16_UINT:   n = 2; goto ui16;
        case MGL_NF_RGBA16_UINT: n = 4; goto ui16;
        ui16:
            for (k = 0; k < n; k++) wr16(d + 2*k, (GLushort)(t->u[k] > 65535u ? 65535u : t->u[k]));
            return GL_TRUE;

        case MGL_NF_R16_SINT:    n = 1; goto si16;
        case MGL_NF_RG16_SINT:   n = 2; goto si16;
        case MGL_NF_RGBA16_SINT: n = 4; goto si16;
        si16:
            for (k = 0; k < n; k++)
            {
                GLint v = t->i[k];
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;
                wr16(d + 2*k, (GLushort)(GLshort)v);
            }
            return GL_TRUE;

        case MGL_NF_R16_FLOAT:    n = 1; goto f16;
        case MGL_NF_RG16_FLOAT:   n = 2; goto f16;
        case MGL_NF_RGBA16_FLOAT: n = 4; goto f16;
        f16:
            for (k = 0; k < n; k++) wr16(d + 2*k, mglFloatToHalf(t->f[k]));
            return GL_TRUE;

        case MGL_NF_R32_UINT:    n = 1; goto ui32;
        case MGL_NF_RG32_UINT:   n = 2; goto ui32;
        case MGL_NF_RGBA32_UINT: n = 4; goto ui32;
        ui32:
            for (k = 0; k < n; k++) wr32(d + 4*k, t->u[k]);
            return GL_TRUE;

        case MGL_NF_R32_SINT:    n = 1; goto si32;
        case MGL_NF_RG32_SINT:   n = 2; goto si32;
        case MGL_NF_RGBA32_SINT: n = 4; goto si32;
        si32:
            for (k = 0; k < n; k++) wr32(d + 4*k, (GLuint)t->i[k]);
            return GL_TRUE;

        case MGL_NF_R32_FLOAT:    n = 1; goto f32;
        case MGL_NF_RG32_FLOAT:   n = 2; goto f32;
        case MGL_NF_RGBA32_FLOAT: n = 4; goto f32;
        f32:
            for (k = 0; k < n; k++) wrf(d + 4*k, t->f[k]);
            return GL_TRUE;

        case MGL_NF_B5G6R5_UNORM:
            wr16(d, (GLushort)((float_to_unorm(t->f[0], 5) << 11) |
                               (float_to_unorm(t->f[1], 6) << 5) |
                                float_to_unorm(t->f[2], 5)));
            return GL_TRUE;

        case MGL_NF_A1BGR5_UNORM:
            wr16(d, (GLushort)((float_to_unorm(t->f[0], 5) << 11) |
                               (float_to_unorm(t->f[1], 5) << 6) |
                               (float_to_unorm(t->f[2], 5) << 1) |
                                float_to_unorm(t->f[3], 1)));
            return GL_TRUE;

        case MGL_NF_ABGR4_UNORM:
            wr16(d, (GLushort)((float_to_unorm(t->f[0], 4) << 12) |
                               (float_to_unorm(t->f[1], 4) << 8) |
                               (float_to_unorm(t->f[2], 4) << 4) |
                                float_to_unorm(t->f[3], 4)));
            return GL_TRUE;

        case MGL_NF_RGB10A2_UNORM:
            wr32(d, (float_to_unorm(t->f[3], 2) << 30) |
                    (float_to_unorm(t->f[2], 10) << 20) |
                    (float_to_unorm(t->f[1], 10) << 10) |
                     float_to_unorm(t->f[0], 10));
            return GL_TRUE;

        case MGL_NF_RGB10A2_UINT:
            wr32(d, (clamp_field(t->u[3], 0x3u) << 30) | (clamp_field(t->u[2], 0x3FFu) << 20) |
                    (clamp_field(t->u[1], 0x3FFu) << 10) | clamp_field(t->u[0], 0x3FFu));
            return GL_TRUE;

        case MGL_NF_RG11B10_FLOAT:
            wr32(d, (float_to_smallfloat(t->f[2], 5, 5) << 22) |
                    (float_to_smallfloat(t->f[1], 6, 5) << 11) |
                     float_to_smallfloat(t->f[0], 6, 5));
            return GL_TRUE;

        case MGL_NF_DEPTH16_UNORM:
            wr16(d, (GLushort)float_to_unorm(t->f[0], 16));
            return GL_TRUE;

        case MGL_NF_DEPTH32_FLOAT:
            wrf(d, t->f[0]);
            return GL_TRUE;

        case MGL_NF_STENCIL8:
            d[0] = (GLubyte)(t->u[1] > 255u ? 255u : t->u[1]);
            return GL_TRUE;

        case MGL_NF_RGB9E5_FLOAT:
        {
            // one 5-bit exponent shared by all three channels, so it has to be
            // the largest of them; see GL 4.6 table 8.5
            const GLfloat maxval = 65408.0f;   /* (2^9-1)/2^9 * 2^(31-15) */
            GLfloat c[3];
            GLfloat biggest = 0.0f;
            GLint e;
            GLuint m[3];
            int i;

            for (i = 0; i < 3; i++)
            {
                c[i] = t->f[i] < 0.0f ? 0.0f : (t->f[i] > maxval ? maxval : t->f[i]);
                if (c[i] > biggest) biggest = c[i];
            }

            e = biggest > 0.0f ? (GLint)floorf(log2f(biggest)) + 1 : -15;
            if (e < -15) e = -15;
            if (e > 16)  e = 16;

            // one more step if rounding pushed the mantissa over
            {
                GLfloat s = ldexpf(1.0f, -(e - 9));
                GLuint mx = (GLuint)(biggest * s + 0.5f);
                if (mx == 512u) e++;
            }

            {
                GLfloat s = ldexpf(1.0f, -(e - 9));
                for (i = 0; i < 3; i++)
                {
                    GLuint v = (GLuint)(c[i] * s + 0.5f);
                    m[i] = v > 511u ? 511u : v;
                }
            }

            wr32(d, (((GLuint)(e + 15) & 0x1Fu) << 27) |
                    ((m[2] & 0x1FFu) << 18) | ((m[1] & 0x1FFu) << 9) | (m[0] & 0x1FFu));
            return GL_TRUE;
        }

        case MGL_NF_DEPTH24_UNORM_STENCIL8:
            wr32(d, ((t->u[1] & 0xFFu) << 24) | (float_to_unorm(t->f[0], 24) & 0x00FFFFFFu));
            return GL_TRUE;

        case MGL_NF_DEPTH32_FLOAT_STENCIL8:
            wrf(d, t->f[0]);
            d[4] = (GLubyte)(t->u[1] > 255u ? 255u : t->u[1]);
            return GL_TRUE;

        default:
            return GL_FALSE;
    }
}


// The inverse of encode_packed: client pixels that stuff every component into
// one word. Without this, any upload using a packed type had to be stored in
// whatever format the data happened to be, which quietly changed the texture's
// internal format out from under the app.
// Where each field of a packed type sits, in the order the client format
// names its components. A _REV type simply lists them low bits first.
typedef struct {
    GLenum type;
    GLuint bytes, fields;
    GLubyte shift[4], bits[4];
} PackedLayout;

static const PackedLayout packed_layouts[] = {
    { GL_UNSIGNED_BYTE_3_3_2,         1, 3, {5, 2, 0, 0},     {3, 3, 2, 0} },
    { GL_UNSIGNED_BYTE_2_3_3_REV,     1, 3, {0, 3, 6, 0},     {3, 3, 2, 0} },
    { GL_UNSIGNED_SHORT_5_6_5,        2, 3, {11, 5, 0, 0},    {5, 6, 5, 0} },
    { GL_UNSIGNED_SHORT_5_6_5_REV,    2, 3, {0, 5, 11, 0},    {5, 6, 5, 0} },
    { GL_UNSIGNED_SHORT_4_4_4_4,      2, 4, {12, 8, 4, 0},    {4, 4, 4, 4} },
    { GL_UNSIGNED_SHORT_4_4_4_4_REV,  2, 4, {0, 4, 8, 12},    {4, 4, 4, 4} },
    { GL_UNSIGNED_SHORT_5_5_5_1,      2, 4, {11, 6, 1, 0},    {5, 5, 5, 1} },
    { GL_UNSIGNED_SHORT_1_5_5_5_REV,  2, 4, {0, 5, 10, 15},   {5, 5, 5, 1} },
    { GL_UNSIGNED_INT_8_8_8_8,        4, 4, {24, 16, 8, 0},   {8, 8, 8, 8} },
    { GL_UNSIGNED_INT_8_8_8_8_REV,    4, 4, {0, 8, 16, 24},   {8, 8, 8, 8} },
    { GL_UNSIGNED_INT_10_10_10_2,     4, 4, {22, 12, 2, 0},   {10, 10, 10, 2} },
    { GL_UNSIGNED_INT_2_10_10_10_REV, 4, 4, {0, 10, 20, 30},  {10, 10, 10, 2} },
};

// The inverse of encode_packed: client pixels that stuff every component into
// one word. Without this, any upload using a packed type had to be stored in
// whatever format the data happened to be, which quietly changed the texture's
// internal format out from under the app.
static GLboolean decode_packed(const GLubyte *sp, GLenum format, GLenum type, MGLTexel *t)
{
    GLuint comps = mglComponentsForFormat(format);
    GLfloat fv[4] = {0,0,0,1};
    GLint   iv[4] = {0,0,0,1};
    GLuint  uv[4] = {0,0,0,1};
    GLuint v = 0;

    for (size_t n = 0; n < sizeof(packed_layouts) / sizeof(packed_layouts[0]); n++)
    {
        const PackedLayout *L = &packed_layouts[n];

        if (L->type != type)
            continue;

        if (comps != L->fields)
            return GL_FALSE;

        if (L->bytes == 1)      v = sp[0];
        else if (L->bytes == 2) { GLushort h; memcpy(&h, sp, 2); v = h; }
        else                    memcpy(&v, sp, 4);

        // The fields come out in the order the format names them. Mapping
        // that order onto RGBA is scatter_components' job -- swapping red and
        // blue here as well undid it for every BGR format.
        for (GLuint k = 0; k < L->fields; k++)
        {
            GLuint raw = (v >> L->shift[k]) & ((1u << L->bits[k]) - 1u);

            fv[k] = unorm_to_float(raw, L->bits[k]);
            uv[k] = raw;
            iv[k] = (GLint)raw;
        }

        texel_zero(t);

        // an integer format takes the fields as they are, not normalised
        if (format_is_integer(format))
        {
            t->is_uint = GL_TRUE;
            t->is_sint = GL_FALSE;
        }

        scatter_components(t, format, fv, iv, uv);

        return GL_TRUE;
    }

    switch(type)
    {
        case GL_UNSIGNED_INT_24_8:
            if (format != GL_DEPTH_STENCIL) return GL_FALSE;
            memcpy(&v, sp, 4);
            texel_zero(t);
            t->f[0] = unorm_to_float((v >> 8) & 0xFFFFFFu, 24);
            t->u[1] = v & 0xFFu;
            return GL_TRUE;

        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
            if (format != GL_DEPTH_STENCIL) return GL_FALSE;
            texel_zero(t);
            memcpy(&t->f[0], sp, 4);
            memcpy(&v, sp + 4, 4);
            t->u[1] = v & 0xFFu;
            return GL_TRUE;

        case GL_UNSIGNED_INT_10F_11F_11F_REV:
            if (comps != 3) return GL_FALSE;
            memcpy(&v, sp, 4);
            fv[0] = smallfloat_to_float(v & 0x7FFu, 6, 5);
            fv[1] = smallfloat_to_float((v >> 11) & 0x7FFu, 6, 5);
            fv[2] = smallfloat_to_float((v >> 22) & 0x3FFu, 5, 5);
            break;

        case GL_UNSIGNED_INT_5_9_9_9_REV:
        {
            if (comps != 3) return GL_FALSE;
            memcpy(&v, sp, 4);
            GLint exp = (GLint)((v >> 27) & 0x1Fu) - 15 - 9;
            GLfloat scale = ldexpf(1.0f, exp);
            fv[0] = (GLfloat)(v & 0x1FFu) * scale;
            fv[1] = (GLfloat)((v >> 9) & 0x1FFu) * scale;
            fv[2] = (GLfloat)((v >> 18) & 0x1FFu) * scale;
            break;
        }

        default:
            return GL_FALSE;
    }

    for (GLuint k = 0; k < 4; k++)
    {
        uv[k] = (GLuint)(fv[k] < 0.0f ? 0.0f : fv[k]);
        iv[k] = (GLint)uv[k];
    }

    texel_zero(t);
    scatter_components(t, format, fv, iv, uv);

    return GL_TRUE;
}

GLboolean mglConvertPixelsToNative(const void *src, size_t src_row_pitch, GLenum format, GLenum type,
                                   void *dst, size_t dst_row_pitch, MGLNativeFormat dst_fmt,
                                   GLsizei width, GLsizei height)
{
    GLuint src_bpp = mglPackedPixelSize(format, type);
    GLuint dst_bpp = mglNativeFormatBytesPerPixel(dst_fmt);
    GLsizei row, col;

    if (!src || !dst || src_bpp == 0 || dst_bpp == 0)
        return GL_FALSE;

    if (width < 0 || height < 0)
        return GL_FALSE;

    if (width == 0 || height == 0)
        return GL_TRUE;

    for (row = 0; row < height; row++)
    {
        const GLubyte *s = (const GLubyte *)src + (size_t)row * src_row_pitch;
        GLubyte *d = (GLubyte *)dst + (size_t)row * dst_row_pitch;

        for (col = 0; col < width; col++)
        {
            MGLTexel t;

            const GLubyte *sp = s + (size_t)col * src_bpp;
            GLboolean got = packed_type_size(type) ? decode_packed(sp, format, type, &t)
                                                   : decode_plain(sp, format, type, &t);

            if (!got)
                return GL_FALSE;

            if (!encode_native(d + (size_t)col * dst_bpp, dst_fmt, &t))
                return GL_FALSE;
        }
    }

    return GL_TRUE;
}

/* ---------- which native layout a GL internal format lands in ---------- */

// One table, three callers. The renderer and the format table used to keep
// their own copies of this and drift apart.
MGLNativeFormat mglNativeFormatForMTLFormat(GLuint mtl_format)
{
    switch(mtl_format)
    {
        case MTLPixelFormatR8Unorm:      return MGL_NF_R8_UNORM;
        case MTLPixelFormatRG8Unorm:     return MGL_NF_RG8_UNORM;
        case MTLPixelFormatRGBA8Unorm:   return MGL_NF_RGBA8_UNORM;
        case MTLPixelFormatBGRA8Unorm:   return MGL_NF_BGRA8_UNORM;
        case MTLPixelFormatRGBA8Unorm_sRGB: return MGL_NF_RGBA8_UNORM_SRGB;
        case MTLPixelFormatBGRA8Unorm_sRGB: return MGL_NF_BGRA8_UNORM_SRGB;
        case MTLPixelFormatR8Snorm:      return MGL_NF_R8_SNORM;
        case MTLPixelFormatRG8Snorm:     return MGL_NF_RG8_SNORM;
        case MTLPixelFormatRGBA8Snorm:   return MGL_NF_RGBA8_SNORM;
        case MTLPixelFormatR8Uint:       return MGL_NF_R8_UINT;
        case MTLPixelFormatRG8Uint:      return MGL_NF_RG8_UINT;
        case MTLPixelFormatRGBA8Uint:    return MGL_NF_RGBA8_UINT;
        case MTLPixelFormatR8Sint:       return MGL_NF_R8_SINT;
        case MTLPixelFormatRG8Sint:      return MGL_NF_RG8_SINT;
        case MTLPixelFormatRGBA8Sint:    return MGL_NF_RGBA8_SINT;

        case MTLPixelFormatR16Unorm:     return MGL_NF_R16_UNORM;
        case MTLPixelFormatRG16Unorm:    return MGL_NF_RG16_UNORM;
        case MTLPixelFormatRGBA16Unorm:  return MGL_NF_RGBA16_UNORM;
        case MTLPixelFormatR16Snorm:     return MGL_NF_R16_SNORM;
        case MTLPixelFormatRG16Snorm:    return MGL_NF_RG16_SNORM;
        case MTLPixelFormatRGBA16Snorm:  return MGL_NF_RGBA16_SNORM;
        case MTLPixelFormatR16Uint:      return MGL_NF_R16_UINT;
        case MTLPixelFormatRG16Uint:     return MGL_NF_RG16_UINT;
        case MTLPixelFormatRGBA16Uint:   return MGL_NF_RGBA16_UINT;
        case MTLPixelFormatR16Sint:      return MGL_NF_R16_SINT;
        case MTLPixelFormatRG16Sint:     return MGL_NF_RG16_SINT;
        case MTLPixelFormatRGBA16Sint:   return MGL_NF_RGBA16_SINT;
        case MTLPixelFormatR16Float:     return MGL_NF_R16_FLOAT;
        case MTLPixelFormatRG16Float:    return MGL_NF_RG16_FLOAT;
        case MTLPixelFormatRGBA16Float:  return MGL_NF_RGBA16_FLOAT;

        case MTLPixelFormatR32Uint:      return MGL_NF_R32_UINT;
        case MTLPixelFormatRG32Uint:     return MGL_NF_RG32_UINT;
        case MTLPixelFormatRGBA32Uint:   return MGL_NF_RGBA32_UINT;
        case MTLPixelFormatR32Sint:      return MGL_NF_R32_SINT;
        case MTLPixelFormatRG32Sint:     return MGL_NF_RG32_SINT;
        case MTLPixelFormatRGBA32Sint:   return MGL_NF_RGBA32_SINT;
        case MTLPixelFormatR32Float:     return MGL_NF_R32_FLOAT;
        case MTLPixelFormatRG32Float:    return MGL_NF_RG32_FLOAT;
        case MTLPixelFormatRGBA32Float:  return MGL_NF_RGBA32_FLOAT;

        case MTLPixelFormatB5G6R5Unorm:  return MGL_NF_B5G6R5_UNORM;
        case MTLPixelFormatA1BGR5Unorm:  return MGL_NF_A1BGR5_UNORM;
        case MTLPixelFormatBGR5A1Unorm:  return MGL_NF_A1BGR5_UNORM;
        case MTLPixelFormatABGR4Unorm:   return MGL_NF_ABGR4_UNORM;
        case MTLPixelFormatRGB10A2Unorm: return MGL_NF_RGB10A2_UNORM;
        case MTLPixelFormatRGB10A2Uint:  return MGL_NF_RGB10A2_UINT;
        case MTLPixelFormatRG11B10Float: return MGL_NF_RG11B10_FLOAT;
        case MTLPixelFormatRGB9E5Float:  return MGL_NF_RGB9E5_FLOAT;

        case MTLPixelFormatDepth16Unorm: return MGL_NF_DEPTH16_UNORM;
        case MTLPixelFormatDepth32Float: return MGL_NF_DEPTH32_FLOAT;
        case MTLPixelFormatStencil8:     return MGL_NF_STENCIL8;
        case MTLPixelFormatDepth24Unorm_Stencil8: return MGL_NF_DEPTH24_UNORM_STENCIL8;
        case MTLPixelFormatDepth32Float_Stencil8: return MGL_NF_DEPTH32_FLOAT_STENCIL8;

        default: return MGL_NF_UNKNOWN;
    }
}

MGLNativeFormat mglNativeFormatForGLInternalFormat(GLenum internalformat)
{
    // The level's own copy stays in GL's four byte packing even when Metal
    // holds it as a float and a stencil byte; the upload converts.
    if (internalformat == GL_DEPTH24_STENCIL8)
        return MGL_NF_DEPTH24_UNORM_STENCIL8;

    return mglNativeFormatForMTLFormat((GLuint)mglFormatMetalFormat(internalformat));
}

// The one format/type pair that already matches a native layout byte for byte.
static GLboolean identity_pair(MGLNativeFormat fmt, GLenum *format, GLenum *type)
{
    switch(fmt)
    {
        case MGL_NF_R8_UNORM:    *format = GL_RED;  *type = GL_UNSIGNED_BYTE; return GL_TRUE;
        case MGL_NF_RG8_UNORM:   *format = GL_RG;   *type = GL_UNSIGNED_BYTE; return GL_TRUE;
        case MGL_NF_RGBA8_UNORM:
        case MGL_NF_RGBA8_UNORM_SRGB: *format = GL_RGBA; *type = GL_UNSIGNED_BYTE; return GL_TRUE;
        case MGL_NF_BGRA8_UNORM:
        case MGL_NF_BGRA8_UNORM_SRGB: *format = GL_BGRA; *type = GL_UNSIGNED_BYTE; return GL_TRUE;

        case MGL_NF_R8_SNORM:    *format = GL_RED;  *type = GL_BYTE; return GL_TRUE;
        case MGL_NF_RG8_SNORM:   *format = GL_RG;   *type = GL_BYTE; return GL_TRUE;
        case MGL_NF_RGBA8_SNORM: *format = GL_RGBA; *type = GL_BYTE; return GL_TRUE;

        case MGL_NF_R8_UINT:    *format = GL_RED_INTEGER;  *type = GL_UNSIGNED_BYTE; return GL_TRUE;
        case MGL_NF_RG8_UINT:   *format = GL_RG_INTEGER;   *type = GL_UNSIGNED_BYTE; return GL_TRUE;
        case MGL_NF_RGBA8_UINT: *format = GL_RGBA_INTEGER; *type = GL_UNSIGNED_BYTE; return GL_TRUE;
        case MGL_NF_R8_SINT:    *format = GL_RED_INTEGER;  *type = GL_BYTE; return GL_TRUE;
        case MGL_NF_RG8_SINT:   *format = GL_RG_INTEGER;   *type = GL_BYTE; return GL_TRUE;
        case MGL_NF_RGBA8_SINT: *format = GL_RGBA_INTEGER; *type = GL_BYTE; return GL_TRUE;

        case MGL_NF_R16_UNORM:    *format = GL_RED;  *type = GL_UNSIGNED_SHORT; return GL_TRUE;
        case MGL_NF_RG16_UNORM:   *format = GL_RG;   *type = GL_UNSIGNED_SHORT; return GL_TRUE;
        case MGL_NF_RGBA16_UNORM: *format = GL_RGBA; *type = GL_UNSIGNED_SHORT; return GL_TRUE;
        case MGL_NF_R16_SNORM:    *format = GL_RED;  *type = GL_SHORT; return GL_TRUE;
        case MGL_NF_RG16_SNORM:   *format = GL_RG;   *type = GL_SHORT; return GL_TRUE;
        case MGL_NF_RGBA16_SNORM: *format = GL_RGBA; *type = GL_SHORT; return GL_TRUE;
        case MGL_NF_R16_UINT:     *format = GL_RED_INTEGER;  *type = GL_UNSIGNED_SHORT; return GL_TRUE;
        case MGL_NF_RG16_UINT:    *format = GL_RG_INTEGER;   *type = GL_UNSIGNED_SHORT; return GL_TRUE;
        case MGL_NF_RGBA16_UINT:  *format = GL_RGBA_INTEGER; *type = GL_UNSIGNED_SHORT; return GL_TRUE;
        case MGL_NF_R16_SINT:     *format = GL_RED_INTEGER;  *type = GL_SHORT; return GL_TRUE;
        case MGL_NF_RG16_SINT:    *format = GL_RG_INTEGER;   *type = GL_SHORT; return GL_TRUE;
        case MGL_NF_RGBA16_SINT:  *format = GL_RGBA_INTEGER; *type = GL_SHORT; return GL_TRUE;
        case MGL_NF_R16_FLOAT:    *format = GL_RED;  *type = GL_HALF_FLOAT; return GL_TRUE;
        case MGL_NF_RG16_FLOAT:   *format = GL_RG;   *type = GL_HALF_FLOAT; return GL_TRUE;
        case MGL_NF_RGBA16_FLOAT: *format = GL_RGBA; *type = GL_HALF_FLOAT; return GL_TRUE;

        case MGL_NF_R32_UINT:    *format = GL_RED_INTEGER;  *type = GL_UNSIGNED_INT; return GL_TRUE;
        case MGL_NF_RG32_UINT:   *format = GL_RG_INTEGER;   *type = GL_UNSIGNED_INT; return GL_TRUE;
        case MGL_NF_RGBA32_UINT: *format = GL_RGBA_INTEGER; *type = GL_UNSIGNED_INT; return GL_TRUE;
        case MGL_NF_R32_SINT:    *format = GL_RED_INTEGER;  *type = GL_INT; return GL_TRUE;
        case MGL_NF_RG32_SINT:   *format = GL_RG_INTEGER;   *type = GL_INT; return GL_TRUE;
        case MGL_NF_RGBA32_SINT: *format = GL_RGBA_INTEGER; *type = GL_INT; return GL_TRUE;
        case MGL_NF_R32_FLOAT:    *format = GL_RED;  *type = GL_FLOAT; return GL_TRUE;
        case MGL_NF_RG32_FLOAT:   *format = GL_RG;   *type = GL_FLOAT; return GL_TRUE;
        case MGL_NF_RGBA32_FLOAT: *format = GL_RGBA; *type = GL_FLOAT; return GL_TRUE;

        case MGL_NF_RGB10A2_UNORM: *format = GL_RGBA;         *type = GL_UNSIGNED_INT_2_10_10_10_REV; return GL_TRUE;
        case MGL_NF_RGB10A2_UINT:  *format = GL_RGBA_INTEGER; *type = GL_UNSIGNED_INT_2_10_10_10_REV; return GL_TRUE;

        // the shared exponent and the small floats are stored exactly as the
        // client type spells them, and RGB9_E5 has more than one encoding for
        // the same colour -- so going through a float loses the one it had
        case MGL_NF_RG11B10_FLOAT: *format = GL_RGB; *type = GL_UNSIGNED_INT_10F_11F_11F_REV; return GL_TRUE;
        case MGL_NF_RGB9E5_FLOAT:  *format = GL_RGB; *type = GL_UNSIGNED_INT_5_9_9_9_REV; return GL_TRUE;

        case MGL_NF_DEPTH16_UNORM: *format = GL_DEPTH_COMPONENT; *type = GL_UNSIGNED_SHORT; return GL_TRUE;
        case MGL_NF_DEPTH32_FLOAT: *format = GL_DEPTH_COMPONENT; *type = GL_FLOAT; return GL_TRUE;
        case MGL_NF_STENCIL8:      *format = GL_STENCIL_INDEX;   *type = GL_UNSIGNED_BYTE; return GL_TRUE;

        default: return GL_FALSE;
    }
}

GLboolean mglUploadNeedsNoConversion(GLenum internalformat, GLenum format, GLenum type)
{
    MGLNativeFormat nf = mglNativeFormatForGLInternalFormat(internalformat);
    GLenum want_format, want_type;

    if (identity_pair(nf, &want_format, &want_type) == GL_FALSE)
        return GL_FALSE;

    return (format == want_format && type == want_type) ? GL_TRUE : GL_FALSE;
}

/* ---------------------------------------------------------------- */
/*  RGTC compression                                                 */
/* ---------------------------------------------------------------- */

// GL lets an application hand uncompressed pixels to a compressed internal
// format and expects the driver to compress them. RGTC is one 4x4 block of one
// channel in 8 bytes: two endpoints and sixteen 3-bit indices.
// src holds the block by position (y * 4 + x); valid marks the texels that
// are inside the image, which is all of them except at the right and top edge.
static void encodeRGTCBlock(const GLint *src, GLuint valid, GLubyte *out)
{
    GLint lo = 255, hi = 0;
    GLuint64 bits = 0;
    GLuint i;

    for (i = 0; i < 16; i++)
    {
        if (!(valid & (1u << i)))
            continue;

        if (src[i] < lo) lo = src[i];
        if (src[i] > hi) hi = src[i];
    }

    if (valid == 0) { lo = 0; hi = 0; }

    out[0] = (GLubyte)hi;
    out[1] = (GLubyte)lo;

    // hi > lo selects the eight-value mode: six interpolated steps between them
    GLint range = hi - lo;

    for (i = 0; i < 16; i++)
    {
        GLuint idx = 0;

        if ((valid & (1u << i)) && range > 0)
        {
            GLint t = ((src[i] - lo) * 14 + range) / (2 * range); /* 0..7 */

            if (t > 7) t = 7;
            if (t < 0) t = 0;

            // block layout: 0 is hi, 1 is lo, 2..7 walk from hi down to lo
            idx = (t == 7) ? 0u : (t == 0 ? 1u : (GLuint)(8 - t));
        }

        bits |= ((GLuint64)idx) << (3 * i);
    }

    for (i = 0; i < 6; i++)
        out[2 + i] = (GLubyte)((bits >> (8 * i)) & 0xFFu);
}

// How many channels the format keeps, and whether they are signed.
GLboolean mglFormatIsRGTC(GLenum internalformat, GLuint *channels, GLboolean *is_signed)
{
    switch (internalformat)
    {
        case GL_COMPRESSED_RED_RGTC1:        *channels = 1; *is_signed = GL_FALSE; return GL_TRUE;
        case GL_COMPRESSED_SIGNED_RED_RGTC1: *channels = 1; *is_signed = GL_TRUE;  return GL_TRUE;
        case GL_COMPRESSED_RG_RGTC2:         *channels = 2; *is_signed = GL_FALSE; return GL_TRUE;
        case GL_COMPRESSED_SIGNED_RG_RGTC2:  *channels = 2; *is_signed = GL_TRUE;  return GL_TRUE;
    }

    return GL_FALSE;
}

GLboolean mglCompressToRGTC(const void *src, size_t src_row_pitch, GLenum format, GLenum type,
                            void *dst, GLenum internalformat, GLsizei width, GLsizei height)
{
    GLuint channels = 0;
    GLboolean is_signed = GL_FALSE;
    GLubyte *out = (GLubyte *)dst;
    GLsizei bx, by;

    if (!src || !dst || width < 0 || height < 0)
        return GL_FALSE;

    if (!mglFormatIsRGTC(internalformat, &channels, &is_signed))
        return GL_FALSE;

    for (by = 0; by < height; by += 4)
    {
        for (bx = 0; bx < width; bx += 4)
        {
            GLint chan[2][16] = {{0}};
            GLuint valid = 0;
            GLsizei y, x;
            GLuint c;

            for (y = by; y < by + 4 && y < height; y++)
                for (x = bx; x < bx + 4 && x < width; x++)
                {
                    const GLubyte *p = (const GLubyte *)src + (size_t)y * src_row_pitch
                                     + (size_t)x * mglPackedPixelSize(format, type);
                    GLuint pos = (GLuint)((y - by) * 4 + (x - bx));
                    MGLTexel t;
                    GLboolean got = packed_type_size(type) ? decode_packed(p, format, type, &t)
                                                           : decode_plain(p, format, type, &t);

                    if (!got)
                        return GL_FALSE;

                    for (c = 0; c < channels; c++)
                    {
                        GLfloat v = t.f[c];

                        if (is_signed)
                        {
                            GLint sv = (GLint)(v * 127.0f + (v < 0.0f ? -0.5f : 0.5f));

                            chan[c][pos] = sv > 127 ? 127 : (sv < -127 ? -127 : sv);
                        }
                        else
                        {
                            GLfloat cl = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);

                            chan[c][pos] = (GLint)(cl * 255.0f + 0.5f);
                        }
                    }

                    valid |= 1u << pos;
                }

            for (c = 0; c < channels; c++)
            {
                if (is_signed)
                {
                    // the same search, on values shifted into 0..254
                    GLint shifted[16];
                    GLubyte blk[8];

                    for (GLuint k = 0; k < 16; k++)
                        shifted[k] = chan[c][k] + 127;

                    encodeRGTCBlock(shifted, valid, blk);
                    out[0] = (GLubyte)(GLbyte)((GLint)blk[0] - 127);
                    out[1] = (GLubyte)(GLbyte)((GLint)blk[1] - 127);
                    memcpy(out + 2, blk + 2, 6);
                }
                else
                {
                    encodeRGTCBlock(chan[c], valid, out);
                }

                out += 8;
            }
        }
    }

    return GL_TRUE;
}

/* ---------------------------------------------------------------- */
/*  SWAP_BYTES                                                       */
/* ---------------------------------------------------------------- */

// GL 4.6 table 8.1: SWAP_BYTES reverses the bytes of each component for the
// two and four byte types, and of the whole word for a packed type. It has no
// effect on single byte components. MGL recorded the flag and ignored it.
void mglSwapPixelBytes(void *data, size_t row_pitch, GLenum format, GLenum type,
                       GLsizei width, GLsizei height)
{
    GLuint unit = packed_type_size(type);
    GLuint per_pixel;
    GLsizei row, i;

    if (!data || width <= 0 || height <= 0)
        return;

    if (unit == 0)
    {
        // a plain type: every component is swapped on its own
        switch (type)
        {
            case GL_UNSIGNED_SHORT: case GL_SHORT: case GL_HALF_FLOAT: unit = 2; break;
            case GL_UNSIGNED_INT:   case GL_INT:   case GL_FLOAT:      unit = 4; break;
            default: return;   /* byte-sized components are unaffected */
        }

        per_pixel = numComponentsForFormat(format);
    }
    else
    {
        per_pixel = 1;
    }

    if (unit < 2)
        return;

    for (row = 0; row < height; row++)
    {
        GLubyte *p = (GLubyte *)data + (size_t)row * row_pitch;

        for (i = 0; i < width * (GLsizei)per_pixel; i++)
        {
            GLubyte *w = p + (size_t)i * unit;
            GLuint a = 0, b = unit - 1;

            while (a < b)
            {
                GLubyte t = w[a];

                w[a] = w[b];
                w[b] = t;
                a++;
                b--;
            }
        }
    }
}

// RGTC keeps two endpoints and sixteen 3-bit selectors per 4x4 block. GL lets
// glGetTexImage read a compressed texture back as plain pixels, so the blocks
// have to be unpacked on the way out.
static void decodeRGTCBlockUnsigned(const GLubyte *blk, GLubyte out[16])
{
    GLubyte pal[8];
    GLuint64 bits = 0;
    int i;

    pal[0] = blk[0];
    pal[1] = blk[1];

    if (pal[0] > pal[1])
    {
        for (i = 2; i < 8; i++)
            pal[i] = (GLubyte)(((8 - i) * pal[0] + (i - 1) * pal[1]) / 7);
    }
    else
    {
        for (i = 2; i < 6; i++)
            pal[i] = (GLubyte)(((6 - i) * pal[0] + (i - 1) * pal[1]) / 5);

        pal[6] = 0;
        pal[7] = 255;
    }

    for (i = 0; i < 6; i++)
        bits |= (GLuint64)blk[2 + i] << (8 * i);

    for (i = 0; i < 16; i++)
        out[i] = pal[(bits >> (3 * i)) & 0x7u];
}

static void decodeRGTCBlockSigned(const GLubyte *blk, GLbyte out[16])
{
    GLbyte pal[8];
    GLuint64 bits = 0;
    int i;
    int e0 = (GLbyte)blk[0];
    int e1 = (GLbyte)blk[1];

    // -128 is not representable as a snorm value, so it reads as -127
    if (e0 == -128) e0 = -127;
    if (e1 == -128) e1 = -127;

    pal[0] = (GLbyte)e0;
    pal[1] = (GLbyte)e1;

    if (e0 > e1)
    {
        for (i = 2; i < 8; i++)
            pal[i] = (GLbyte)(((8 - i) * e0 + (i - 1) * e1) / 7);
    }
    else
    {
        for (i = 2; i < 6; i++)
            pal[i] = (GLbyte)(((6 - i) * e0 + (i - 1) * e1) / 5);

        pal[6] = -127;
        pal[7] = 127;
    }

    for (i = 0; i < 6; i++)
        bits |= (GLuint64)blk[2 + i] << (8 * i);

    for (i = 0; i < 16; i++)
        out[i] = pal[(bits >> (3 * i)) & 0x7u];
}

GLboolean mglDecompressRGTC(const void *src, GLenum internalformat,
                            GLsizei width, GLsizei height,
                            void *dst, size_t dst_row_pitch)
{
    GLuint channels = 0;
    GLboolean is_signed = GL_FALSE;
    const GLubyte *blocks = (const GLubyte *)src;
    GLsizei bx, by;

    if (!src || !dst || width < 0 || height < 0)
        return GL_FALSE;

    if (!mglFormatIsRGTC(internalformat, &channels, &is_signed))
        return GL_FALSE;

    for (by = 0; by < height; by += 4)
    {
        for (bx = 0; bx < width; bx += 4)
        {
            for (GLuint c = 0; c < channels; c++)
            {
                GLubyte texels[16];

                if (is_signed)
                    decodeRGTCBlockSigned(blocks, (GLbyte *)texels);
                else
                    decodeRGTCBlockUnsigned(blocks, texels);

                blocks += 8;

                for (int y = 0; y < 4; y++)
                {
                    GLsizei ty = by + y;

                    if (ty >= height)
                        break;

                    for (int x = 0; x < 4; x++)
                    {
                        GLsizei tx = bx + x;
                        GLubyte *row;

                        if (tx >= width)
                            break;

                        row = (GLubyte *)dst + (size_t)ty * dst_row_pitch;
                        row[(size_t)tx * channels + c] = texels[y * 4 + x];
                    }
                }
            }
        }
    }

    return GL_TRUE;
}

// The uncompressed layout an RGTC format unpacks into.
MGLNativeFormat mglRGTCNativeFormat(GLenum internalformat)
{
    GLuint channels = 0;
    GLboolean is_signed = GL_FALSE;

    if (!mglFormatIsRGTC(internalformat, &channels, &is_signed))
        return MGL_NF_UNKNOWN;

    if (channels == 1)
        return is_signed ? MGL_NF_R8_SNORM : MGL_NF_R8_UNORM;

    return is_signed ? MGL_NF_RG8_SNORM : MGL_NF_RG8_UNORM;
}
