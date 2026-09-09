/*
 * pixel_convert.h
 * MGL
 *
 * Converts between Metal's native texture layouts and the packed
 * format/type pairs glReadPixels and glGetTexImage accept.
 */

#ifndef pixel_convert_h
#define pixel_convert_h

#include <stddef.h>

#include "glcorearb.h"

// Metal pixel layouts MGL can read back from. Kept separate from MTLPixelFormat
// so this stays plain C; the renderer maps one to the other.
typedef enum {
    MGL_NF_UNKNOWN = 0,

    MGL_NF_R8_UNORM, MGL_NF_RG8_UNORM, MGL_NF_RGBA8_UNORM, MGL_NF_BGRA8_UNORM,
    MGL_NF_RGBA8_UNORM_SRGB, MGL_NF_BGRA8_UNORM_SRGB,
    MGL_NF_R8_SNORM, MGL_NF_RG8_SNORM, MGL_NF_RGBA8_SNORM,
    MGL_NF_R8_UINT, MGL_NF_RG8_UINT, MGL_NF_RGBA8_UINT,
    MGL_NF_R8_SINT, MGL_NF_RG8_SINT, MGL_NF_RGBA8_SINT,

    MGL_NF_R16_UNORM, MGL_NF_RG16_UNORM, MGL_NF_RGBA16_UNORM,
    MGL_NF_R16_SNORM, MGL_NF_RG16_SNORM, MGL_NF_RGBA16_SNORM,
    MGL_NF_R16_UINT, MGL_NF_RG16_UINT, MGL_NF_RGBA16_UINT,
    MGL_NF_R16_SINT, MGL_NF_RG16_SINT, MGL_NF_RGBA16_SINT,
    MGL_NF_R16_FLOAT, MGL_NF_RG16_FLOAT, MGL_NF_RGBA16_FLOAT,

    MGL_NF_R32_UINT, MGL_NF_RG32_UINT, MGL_NF_RGBA32_UINT,
    MGL_NF_R32_SINT, MGL_NF_RG32_SINT, MGL_NF_RGBA32_SINT,
    MGL_NF_R32_FLOAT, MGL_NF_RG32_FLOAT, MGL_NF_RGBA32_FLOAT,

    MGL_NF_RGB10A2_UNORM, MGL_NF_RGB10A2_UINT,
    MGL_NF_RG11B10_FLOAT, MGL_NF_RGB9E5_FLOAT,

    MGL_NF_DEPTH16_UNORM, MGL_NF_DEPTH32_FLOAT,
    MGL_NF_STENCIL8,
    MGL_NF_DEPTH24_UNORM_STENCIL8, MGL_NF_DEPTH32_FLOAT_STENCIL8,

    MGL_NF_COUNT
} MGLNativeFormat;

GLuint    mglNativeFormatBytesPerPixel(MGLNativeFormat fmt);
GLboolean mglNativeFormatIsInteger(MGLNativeFormat fmt);
GLboolean mglNativeFormatIsDepth(MGLNativeFormat fmt);
GLboolean mglNativeFormatIsStencil(MGLNativeFormat fmt);

// Bytes for one pixel packed as format/type. 0 means the pair is not legal.
GLuint mglPackedPixelSize(GLenum format, GLenum type);

// Number of components format carries. 0 if format is unknown.
GLuint mglComponentsForFormat(GLenum format);

/* Converts a width x height rectangle from a native Metal layout into the
 * packed format/type an app asked for.
 *
 * flip_vertical mirrors rows, which is what glReadPixels needs because GL counts
 * rows from the bottom and Metal stores them from the top.
 *
 * Returns GL_FALSE and touches nothing if the combination is unsupported.
 */
GLboolean mglConvertPixels(const void *src, size_t src_row_pitch, MGLNativeFormat src_fmt,
                           void *dst, size_t dst_row_pitch, GLenum format, GLenum type,
                           GLsizei width, GLsizei height, GLboolean flip_vertical);

// Half float helpers, exposed for tests.
GLfloat  mglHalfToFloat(GLushort h);
GLushort mglFloatToHalf(GLfloat f);

#endif /* pixel_convert_h */
