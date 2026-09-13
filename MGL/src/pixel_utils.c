/*
 * Copyright (C) Michael Larson on 1/6/2022
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
 * pixel_utils.c
 * MGL
 *
 */
 
#include <Availability.h>

#include "pixel_utils.h"
#include "glm_context.h"
#include "mgl_log.h"
#include "mgl_format_table.h"

// Legacy format defines not in core profile headers
#ifndef GL_ALPHA
#define GL_ALPHA                          0x1906
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE                      0x1909
#endif

#ifndef GL_ALPHA8UI_EXT
#define GL_ALPHA8UI_EXT                   0x8D7E
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA                0x190A
#endif
#ifndef GL_ALPHA8
#define GL_ALPHA8                         0x803C
#endif
#ifndef GL_ALPHA16
#define GL_ALPHA16                        0x803E
#endif
#ifndef GL_LUMINANCE8
#define GL_LUMINANCE8                     0x8040
#endif
#ifndef GL_LUMINANCE16
#define GL_LUMINANCE16                    0x8048
#endif
#ifndef GL_ALPHA32F_ARB
#define GL_ALPHA32F_ARB                   0x8816
#endif
#ifndef GL_LUMINANCE32F_ARB
#define GL_LUMINANCE32F_ARB               0x8818
#endif
#ifndef GL_LUMINANCE_ALPHA32F_ARB
#define GL_LUMINANCE_ALPHA32F_ARB         0x8819
#endif
#ifndef GL_ALPHA16F_ARB
#define GL_ALPHA16F_ARB                   0x881C
#endif
#ifndef GL_LUMINANCE16F_ARB
#define GL_LUMINANCE16F_ARB               0x881E
#endif
#ifndef GL_LUMINANCE_ALPHA16F_ARB
#define GL_LUMINANCE_ALPHA16F_ARB         0x881F
#endif
#ifndef GL_SR8_EXT
#define GL_SR8_EXT                        0x8FBD
#endif
#ifndef GL_SRG8_EXT
#define GL_SRG8_EXT                       0x8FBE
#endif

GLuint numComponentsForFormat(GLenum format)
{
    switch(format)
    {
        case GL_RED:
        case GL_RED_INTEGER:
        case GL_GREEN:
        case GL_BLUE:
        case GL_GREEN_INTEGER:
        case GL_BLUE_INTEGER:
        case GL_STENCIL_INDEX:
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
        // Legacy single-channel formats
        case GL_ALPHA:
        case GL_ALPHA8:
        case GL_ALPHA16:
        case GL_ALPHA32F_ARB:
        case GL_ALPHA16F_ARB:
        case GL_LUMINANCE:
        case GL_LUMINANCE8:
        case GL_LUMINANCE16:
        case GL_LUMINANCE32F_ARB:
        case GL_LUMINANCE16F_ARB:
        // Sized R formats (internal formats sometimes passed as format)
        case GL_R8:
        case GL_R8_SNORM:
        case GL_R16:
        case GL_R16_SNORM:
        case GL_R16F:
        case GL_R32F:
        case GL_R8I:
        case GL_R8UI:
        case GL_R16I:
        case GL_R16UI:
        case GL_R32I:
        case GL_R32UI:
        case GL_SR8_EXT:
        case GL_ALPHA8UI_EXT:
        case 0x9014: // GL_ALPHA8_SNORM
        case 0x9018: // GL_ALPHA16_SNORM
            return 1;

        case GL_RG:
        case GL_RG_INTEGER:
        // Legacy two-channel formats
        case GL_LUMINANCE_ALPHA:
        case GL_LUMINANCE_ALPHA32F_ARB:
        case GL_LUMINANCE_ALPHA16F_ARB:
        case 0x9016: // GL_LUMINANCE8_ALPHA8_SNORM
        case 0x901a: // GL_LUMINANCE16_ALPHA16_SNORM
        // Sized RG formats
        case GL_RG8:
        case GL_RG8_SNORM:
        case GL_RG16:
        case GL_RG16_SNORM:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RG32I:
        case GL_RG32UI:
        case GL_SRG8_EXT:
            return 2;

        case 0x8d7b: // GL_ALPHA8I_EXT
        case 0x8d81: // GL_ALPHA32I_EXT
        case 0x8d87: // GL_ALPHA16I_EXT
        case 0x8d8d: // GL_ALPHA32UI_EXT
        case 0x8d93: // GL_ALPHA16UI_EXT
        case 0x8d72: // GL_ALPHA32UI_EXT
            return 1;

        case GL_RGB:
        case GL_BGR:
        case GL_RGB_INTEGER:
        case GL_BGR_INTEGER:
        // Sized RGB formats
        case GL_RGB8:
        case GL_RGB8_SNORM:
        case GL_SRGB8:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_R11F_G11F_B10F:
        case GL_RGB9_E5:
        case GL_RGB8I:
        case GL_RGB8UI:
        case GL_RGB16I:
        case GL_RGB16UI:
        case GL_RGB32I:
        case GL_RGB32UI:
        case GL_RGB565:
            return 3;

        case 0x8d75: // alternate GL_RGB8I
        case 0x8d7a: // alternate GL_RGB8UI
        case 0x8d80: // alternate GL_RGB32UI
        case 0x8d86: // alternate GL_RGB16I
        case 0x8d8c: // alternate GL_RGB32I
        case 0x8d92: // alternate GL_RGB16UI
            return 3;

        case GL_RGBA:
        case GL_BGRA:
        case GL_RGBA_INTEGER:
        case GL_BGRA_INTEGER:
        // Sized RGBA formats
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
        case GL_SRGB8_ALPHA8:
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_RGBA32I:
        case GL_RGBA32UI:
        case GL_RGB10_A2:
        case GL_RGB10_A2UI:
        case GL_RGB5_A1:
        case GL_RGBA4:
            return 4;

        case 0x8d78: // alternate GL_RGBA8UI
        // case 0x8d7e: // alternate GL_RGBA32UI - Duplicate of GL_ALPHA8UI_EXT (1 component)
        case 0x8d84: // alternate GL_RGBA16I
        case 0x8d8a: // alternate GL_RGBA32I
        case 0x8d90: // alternate GL_RGBA16UI
            return 4;

        default:
            // Unknown format - return 4 as safe fallback instead of crashing
            MGL_ERR("MGL WARNING: numComponentsForFormat unknown format 0x%x, assuming 4 components\n", format);
            return 4;
    }

    return 0;
}

GLuint sizeForType(GLenum type)
{
    switch(type)
    {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
            return sizeof(uint8_t);

        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
            return sizeof(uint16_t);

        case GL_UNSIGNED_INT:
        case GL_INT:
            return sizeof(uint32_t);

        case GL_FLOAT:
            return sizeof(float);

        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return sizeof(uint8_t);

        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return sizeof(uint16_t);

        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return sizeof(uint32_t);

        default:
            MGL_ERR("MGL WARNING: sizeForType unknown type 0x%x, assuming 4 bytes\n", type);
            return sizeof(uint32_t);
    }

    return 0;
}

GLuint sizeForFormatType(GLenum format, GLenum type)
{
    // Handle type=0 case (used for sized internal formats)
    // The format parameter is actually the internal format in this case
    if (type == 0) {
        switch (format) {
            // Alpha formats (1 component)
            case 0x803c: // GL_ALPHA8
            case 0x8040: // GL_LUMINANCE8
                return 1;
            case 0x803e: // GL_ALPHA16
            case 0x8042: // GL_LUMINANCE16
            case 0x8816: // GL_ALPHA16F_ARB
            case 0x8818: // GL_LUMINANCE16F_ARB
                return 2;
            case 0x881c: // GL_ALPHA32F_ARB
            case 0x881e: // GL_LUMINANCE32F_ARB
                return 4;
            // Luminance-alpha formats (2 components)
            case 0x8045: // GL_LUMINANCE8_ALPHA8
                return 2;
            case 0x8048: // GL_LUMINANCE16_ALPHA16
            case 0x8819: // GL_LUMINANCE_ALPHA16F_ARB
                return 4;
            case 0x881f: // GL_LUMINANCE_ALPHA32F_ARB
                return 8;
            // RGB10_A2UI and SNORM formats
            case 0x8fbd: // GL_RGB10_A2UI
                return 4;
            case 0x8fbe: // GL_RGBA16_SNORM
                return 8;
            // Integer formats
            case 0x8d72: case 0x8d78: // RGBA8I/UI variants
                return 4;
            case 0x8d75: case 0x8d7a: // RGB8I/UI variants
                return 3;
            case 0x8d84: case 0x8d90: // RGBA16I/UI variants
                return 8;
            case 0x8d86: case 0x8d92: // RGB16I/UI variants
                return 6;
            case 0x8d8a: case 0x8d7e: // RGBA32I/UI variants
                return 16;
            case 0x8d8c: case 0x8d80: // RGB32I/UI variants  
                return 12;
            default:
                // Return a reasonable default for unknown internal formats
                return 4;
        }
    }

    switch(type)
    {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
            return sizeof(uint8_t) * numComponentsForFormat(format);

        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
            return sizeof(uint16_t) * numComponentsForFormat(format);

        case GL_UNSIGNED_INT:
        case GL_INT:
            return sizeof(uint32_t) * numComponentsForFormat(format);

        case GL_FLOAT:
            return sizeof(float) * numComponentsForFormat(format);

        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return sizeof(uint8_t);

        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return sizeof(uint16_t);

        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return sizeof(uint32_t);

        case GL_HALF_FLOAT:
            return sizeof(uint16_t) * numComponentsForFormat(format);

        // These pack every component into one word, so the component count
        // must not multiply the size. Guessing 4 x components made the pitch
        // two to three times too wide.
        case GL_UNSIGNED_INT_24_8:
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return sizeof(uint32_t);

        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
            return sizeof(float) + sizeof(uint32_t);

        default:
            MGL_ERR("MGL WARNING: sizeForFormatType unknown type 0x%x, format 0x%x\n", type, format);
            return sizeof(uint32_t) * numComponentsForFormat(format);
    }

    return 0;
}


GLboolean validFormat(GLuint format)
{
    switch(format)
    {
        case GL_RED:
        case GL_RG:
        case GL_RGB:
        case GL_BGR:
        case GL_RGBA:
        case GL_BGRA:
        case GL_RED_INTEGER:
        case GL_RG_INTEGER:
        case GL_RGB_INTEGER:
        case GL_BGR_INTEGER:
        case GL_RGBA_INTEGER:
        case GL_BGRA_INTEGER:
        // table 8.3 lists the single-channel selectors too
        case GL_GREEN:
        case GL_BLUE:
        case GL_GREEN_INTEGER:
        case GL_BLUE_INTEGER:
        case GL_STENCIL_INDEX:
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
            return true;

        default:
            return false;
    }

    return false;
}

GLboolean validFormatType(GLuint format, GLuint type)
{
    RETURN_FALSE_ON_FAILURE(validFormat(format));

    switch(type)
    {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
            return true;

        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_SHORT_5_6_5:
            RETURN_FALSE_ON_FAILURE(format == GL_RGB);
            break;

        case GL_UNSIGNED_BYTE_2_3_3_REV:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            RETURN_FALSE_ON_FAILURE(format == GL_BGR);
            break;

        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_10_10_10_2:
            RETURN_FALSE_ON_FAILURE(format == GL_RGBA);
            break;

        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            RETURN_FALSE_ON_FAILURE(format == GL_BGRA);
            break;

        default:
            return false;
    }

    return true;
}

GLboolean validInternalFormat(GLint internalformat)
{
    return mglFormatDesc((GLenum)internalformat)->gl_format != 0;
}
GLuint sizeForInternalFormat(GLenum internalformat, GLenum format, GLenum type)
{
    const MGLFormatDesc *d = mglFormatDesc(internalformat);

    (void)format;
    (void)type;

    // compressed levels are sized by block, not by pixel; callers that
    // want that use mglFormatImageSize
    if (d->kind == MGL_FMT_COMPRESSED)
        return 0;

    return d->bytes_per_block;
}
GLuint bicountForFormatType(GLenum format, GLenum type, GLenum component)
{
    switch(type)
    {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
            return 8;

        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
            return 16;

        case GL_UNSIGNED_INT:
        case GL_INT:
            return 32;

        case GL_FLOAT:
            return 32;

        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return 8;

        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            switch(component)
            {
                case GL_RED: return 5;
                case GL_GREEN: return 6;
                case GL_BLUE: return 5;
                case GL_ALPHA: return 0;
            }
            break;

        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            switch(component)
            {
                case GL_RED: return 4;
                case GL_GREEN: return 4;
                case GL_BLUE: return 4;
                case GL_ALPHA: return 4;
            }
            break;


        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            switch(component)
            {
                case GL_RED: return 5;
                case GL_GREEN: return 5;
                case GL_BLUE: return 5;
                case GL_ALPHA: return 1;
            }
            break;

            return 16;

        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
            return 8;

        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            switch(component)
            {
                case GL_RED: return 10;
                case GL_GREEN: return 10;
                case GL_BLUE: return 10;
                case GL_ALPHA: return 2;
            }
            break;

        default:
            break;
    }

    return 0;
}

GLuint bitcountForInternalFormat(GLenum internalformat, GLenum component)
{
    return (GLuint)mglFormatComponentBits(internalformat, component);
}
GLenum internalFormatForGLFormatType(GLenum format, GLenum type)
{
    switch(type)
    {
        case GL_UNSIGNED_BYTE:
            switch(format)
            {
                case GL_RED: return GL_R8;
                case GL_RG: return GL_RG8;
                case GL_RGB: return GL_RGB8;
                case GL_BGR: return GL_RGB8;  /* BGR treated as RGB */
                case GL_RGBA: return GL_RGBA8;
                case GL_BGRA: return GL_RGBA8;  /* BGRA treated as RGBA */
                case GL_RED_INTEGER: return GL_R8UI;
                case GL_RG_INTEGER: return GL_RG8UI;
                case GL_RGB_INTEGER: return GL_RGB8UI;
                case GL_BGR_INTEGER: return GL_RGB8UI;
                case GL_RGBA_INTEGER: return GL_RGBA8UI;
                case GL_BGRA_INTEGER: return GL_RGBA8UI;
                default:
                    return 0;
            }
            break;

        case GL_BYTE:
            switch(format)
            {
                case GL_RED: return GL_R8_SNORM;
                case GL_RG: return GL_RG8_SNORM;
                case GL_RGB: return GL_RGB8_SNORM;
                case GL_RGBA: return GL_RGBA8_SNORM;
                case GL_RED_INTEGER: return GL_R8I;
                case GL_RG_INTEGER: return GL_RG8I;
                case GL_RGB_INTEGER: return GL_RGB8I;
                case GL_BGR_INTEGER: return GL_RGB8I;
                case GL_RGBA_INTEGER: return GL_RGBA8I;
                case GL_BGRA_INTEGER: return GL_RGBA8I;
                default:
                    return 0;
            }
            break;

        case GL_UNSIGNED_SHORT:
            switch(format)
            {
                case GL_RED: return GL_R16;
                case GL_RG: return GL_RG16;
                case GL_RGB: return GL_RGB16;
                case GL_RGBA: return GL_RGBA16;
                case GL_RED_INTEGER: return GL_R16UI;
                case GL_RG_INTEGER: return GL_RG16UI;
                case GL_RGB_INTEGER: return GL_RGB16UI;
                case GL_BGR_INTEGER: return GL_RGB16UI;
                case GL_RGBA_INTEGER: return GL_RGBA16UI;
                case GL_BGRA_INTEGER: return GL_RGBA16UI;
                default:
                    return 0;
            }
            break;

        case GL_SHORT:
            switch(format)
            {
                case GL_RED: return GL_R16_SNORM;
                case GL_RG: return GL_RG16_SNORM;
                case GL_RGB: return GL_RGB16_SNORM;
                case GL_RGBA: return GL_RGBA16_SNORM;
                case GL_RED_INTEGER: return GL_R16I;
                case GL_RG_INTEGER: return GL_RG16I;
                case GL_RGB_INTEGER: return GL_RGB16I;
                case GL_BGR_INTEGER: return GL_RGB16I;
                case GL_RGBA_INTEGER: return GL_RGBA16I;
                case GL_BGRA_INTEGER: return GL_RGBA16I;
                default:
                    return 0;
            }
            break;

        case GL_UNSIGNED_INT:
            switch(format)
            {
                case GL_RED: return GL_R32UI;
                case GL_RG: return GL_RG32UI;
                case GL_RGB: return GL_RGB32UI;
                case GL_RGBA: return GL_RGBA32UI;
                case GL_RED_INTEGER: return GL_R32UI;
                case GL_RG_INTEGER: return GL_RG32UI;
                case GL_RGB_INTEGER: return GL_RGB32UI;
                case GL_BGR_INTEGER: return GL_RGB32UI;
                case GL_RGBA_INTEGER: return GL_RGBA32UI;
                case GL_BGRA_INTEGER: return GL_RGBA32UI;
                default:
                    return 0;
            }
            break;

        case GL_INT:
            switch(format)
            {
                case GL_RED: return GL_R32I;
                case GL_RG: return GL_RG32I;
                case GL_RGB: return GL_RGB32I;
                case GL_RGBA: return GL_RGBA32I;
                case GL_RED_INTEGER: return GL_R32I;
                case GL_RG_INTEGER: return GL_RG32I;
                case GL_RGB_INTEGER: return GL_RGB32I;
                case GL_BGR_INTEGER: return GL_RGB32I;
                case GL_RGBA_INTEGER: return GL_RGBA32I;
                case GL_BGRA_INTEGER: return GL_RGBA32I;
                default:
                    return 0;
            }
            break;

        case GL_HALF_FLOAT:
            switch(format)
            {
                case GL_RED: return GL_R16F;
                case GL_RG: return GL_RG16F;
                case GL_RGB: return GL_RGB16F;
                case GL_BGR: return GL_RGB16F;
                case GL_RGBA: return GL_RGBA16F;
                case GL_BGRA: return GL_RGBA16F;
                default:
                    return 0;
            }
            break;

        case GL_FLOAT:
            switch(format)
            {
                case GL_RED: return GL_R32F;
                case GL_RG: return GL_RG32F;
                case GL_RGB: return GL_RGB32F;
                case GL_RGBA: return GL_RGBA32F;
                case GL_DEPTH_COMPONENT: return GL_DEPTH_COMPONENT32F;
                case GL_DEPTH_STENCIL: return GL_DEPTH32F_STENCIL8;

                default:
                    return 0;
            }
            break;

        case GL_UNSIGNED_BYTE_3_3_2:
            return 0;

        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return 0;

        case GL_UNSIGNED_SHORT_5_6_5:
            return GL_RGB565;

        case GL_UNSIGNED_SHORT_5_6_5_REV:
            return 0;

        case GL_UNSIGNED_SHORT_4_4_4_4:
            return GL_RGBA4;

        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
            return 0;

        case GL_UNSIGNED_INT_8_8_8_8:
            return GL_RGBA8;

        case GL_UNSIGNED_INT_8_8_8_8_REV:
            return GL_RGBA8;

        default:
            break;
    }
}

MTLPixelFormat mtlFormatForGLInternalFormat(GLenum internal_format)
{
    return (MTLPixelFormat)mglFormatMetalFormat(internal_format);
}
MTLPixelFormat mtlPixelFormatForGLFormatType(GLenum gl_format, GLenum gl_type)
{
    // The depth and stencil formats a context asks for are sized internal
    // formats, which this switch never listed -- so the default framebuffer
    // came up with no depth or stencil attachment at all and nothing was ever
    // depth tested. The format table already knows them.
    switch (gl_format)
    {
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
        case GL_DEPTH_COMPONENT32F:
        case GL_DEPTH24_STENCIL8:
        case GL_DEPTH32F_STENCIL8:
        case GL_STENCIL_INDEX8:
        {
            uint16_t mtl = mglFormatMetalFormat(gl_format);

            return (mtl == MTLPixelFormatInvalid) ? 0 : (MTLPixelFormat)mtl;
        }

        default:
            break;
    }

    switch(gl_type)
    {
        case GL_UNSIGNED_BYTE:
            switch(gl_format)
            {
                case GL_RED: return MTLPixelFormatR8Uint;
                case GL_RG: return MTLPixelFormatRG8Uint;
                case GL_RGBA: return MTLPixelFormatRGBA8Unorm;
                default:
                    return 0;
            }
            break;

        case GL_BYTE:
            switch(gl_format)
            {
                case GL_RED: return MTLPixelFormatR8Sint;
                case GL_RG: return MTLPixelFormatRG8Sint;
                case GL_RGBA: return MTLPixelFormatRGBA8Sint;
                default:
                    return 0;
            }
            break;

        case GL_UNSIGNED_SHORT:
            switch(gl_format)
            {
                case GL_RED: return MTLPixelFormatR16Uint;
                case GL_RG: return MTLPixelFormatRG16Uint;
                case GL_RGBA: return MTLPixelFormatRGBA16Uint;
                default:
                    return 0;
            }
            break;

        case GL_SHORT:
            switch(gl_format)
            {
                case GL_RED: return MTLPixelFormatR16Sint;
                case GL_RG: return MTLPixelFormatRG16Sint;
                case GL_RGBA: return MTLPixelFormatRGBA16Sint;
                default:
                    return 0;
            }
            break;

        case GL_UNSIGNED_INT:
            switch(gl_format)
            {
                case GL_RED: return MTLPixelFormatR32Uint;
                case GL_RG: return MTLPixelFormatRG32Uint;
                case GL_RGBA: return MTLPixelFormatRGBA32Uint;
                case GL_BGRA: return MTLPixelFormatRGBA32Uint;
                default:
                    return 0;
            }
            break;

        case GL_INT:
            switch(gl_format)
            {
                case GL_RED: return MTLPixelFormatR32Uint;
                case GL_RG: return MTLPixelFormatRG32Uint;
                case GL_RGBA: return MTLPixelFormatRGBA32Uint;
                default:
                    return 0;
            }
            break;

        case GL_FLOAT:
            switch(gl_format)
            {
                case GL_RED: return MTLPixelFormatR32Float;
                case GL_RG: return MTLPixelFormatRG32Float;
                case GL_RGBA: return MTLPixelFormatRGBA32Float;
                case GL_DEPTH_COMPONENT: return MTLPixelFormatDepth32Float;
                case GL_DEPTH_STENCIL: return MTLPixelFormatDepth24Unorm_Stencil8;

                default:
                    return 0;
            }
            break;

        case GL_UNSIGNED_BYTE_3_3_2:
            return 0;

        case GL_UNSIGNED_BYTE_2_3_3_REV:
            return 0;

        case GL_UNSIGNED_SHORT_5_6_5:
            if (__builtin_available(macOS 11.0, *)) {
                return MTLPixelFormatB5G6R5Unorm;
            } else {
                // Fallback on earlier versions
                return MTLPixelFormatInvalid;
            }

        case GL_UNSIGNED_SHORT_5_6_5_REV:
            if (__builtin_available(macOS 11.0, *)) {
                return MTLPixelFormatA1BGR5Unorm;
            } else {
                // Fallback on earlier versions
                return MTLPixelFormatInvalid;
            }

        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return 0;

        case GL_UNSIGNED_INT_8_8_8_8:
            return MTLPixelFormatRGBA8Unorm;

        case GL_UNSIGNED_INT_8_8_8_8_REV:
            return MTLPixelFormatBGRA8Unorm;

        case GL_UNSIGNED_INT_10_10_10_2:
            return MTLPixelFormatRGB10A2Unorm;

        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return MTLPixelFormatBGR10A2Unorm;
            return 0;

        default:
            break;
    }
}

MTLPixelFormat mtlPixelFormatForGLTex(Texture * tex)
{
    MTLPixelFormat mtl_format;
    GLenum internal_format;

    if (tex == NULL)
        return MTLPixelFormatInvalid;

    internal_format = tex->internalformat;
    assert(internal_format);

    mtl_format = mtlFormatForGLInternalFormat(internal_format);
    assert(mtl_format);

    return mtl_format;
}

size_t mglPixelStoreRowPitch(const PixelStore *ps, GLsizei width, GLuint pixel_size)
{
    size_t row_pixels = (ps && ps->row_length > 0) ? (size_t)ps->row_length : (size_t)width;
    size_t bytes = row_pixels * pixel_size;
    size_t align = (ps && ps->alignment > 0) ? (size_t)ps->alignment : 1;

    // alignment is only ever 1, 2, 4 or 8
    if (align > 1)
        bytes = (bytes + align - 1) & ~(align - 1);

    return bytes;
}

// SKIP_IMAGES and IMAGE_HEIGHT only mean anything to a command that moves more
// than one image. glReadPixels and a 2D texture read one, and adding the image
// skip there walked the caller's buffer past where it wrote.
size_t mglPixelStoreSkipBytes2D(const PixelStore *ps, GLuint pixel_size, size_t row_pitch)
{
    if (!ps)
        return 0;

    return (size_t)(ps->skip_pixels > 0 ? ps->skip_pixels : 0) * pixel_size
         + (size_t)(ps->skip_rows > 0 ? ps->skip_rows : 0) * row_pitch;
}

size_t mglPixelStoreSkipBytes(const PixelStore *ps, GLsizei height, GLuint pixel_size, size_t row_pitch)
{
    if (!ps)
        return 0;

    size_t rows_per_image = (ps->image_height > 0) ? (size_t)ps->image_height : (size_t)(height > 0 ? height : 1);

    return (size_t)(ps->skip_pixels > 0 ? ps->skip_pixels : 0) * pixel_size
         + (size_t)(ps->skip_rows > 0 ? ps->skip_rows : 0) * row_pitch
         + (size_t)(ps->skip_images > 0 ? ps->skip_images : 0) * rows_per_image * row_pitch;
}
