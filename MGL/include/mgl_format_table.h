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
 * mgl_format_table.h
 * MGL
 *
 * Format capability data transcribed from MoltenVK's MVKPixelFormats.mm
 * (Copyright (c) 2015-2026 The Brenwill Workshop Ltd., Apache 2.0).
 */

#ifndef mgl_format_table_h
#define mgl_format_table_h

#include "glcorearb.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Metal formats are plain uint16_t here so Objective-C files that already
   have Metal.h can include this; the .c uses pixel_utils.h's C enum copy */

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- */
/*  format kind                                                      */
/* ---------------------------------------------------------------- */
typedef enum {
    MGL_FMT_NONE = 0,
    MGL_FMT_COLOR_FLOAT,      /* unorm/snorm/float colour, sampled as float */
    MGL_FMT_COLOR_INT,
    MGL_FMT_COLOR_UINT,
    MGL_FMT_DEPTH,
    MGL_FMT_STENCIL,
    MGL_FMT_DEPTH_STENCIL,
    MGL_FMT_COMPRESSED,
} MGLFormatKind;

/* ---------------------------------------------------------------- */
/*  Metal format capability bits — same values as MVKMTLFmtCaps     */
/* ---------------------------------------------------------------- */
enum {
    MGL_FMT_CAP_NONE      = 0,
    MGL_FMT_CAP_READ      = 1 << 0,
    MGL_FMT_CAP_FILTER    = 1 << 1,
    MGL_FMT_CAP_WRITE     = 1 << 2,
    MGL_FMT_CAP_ATOMIC    = 1 << 3,
    MGL_FMT_CAP_COLOR_ATT = 1 << 4,
    MGL_FMT_CAP_DS_ATT    = 1 << 5,
    MGL_FMT_CAP_BLEND     = 1 << 6,
    MGL_FMT_CAP_MSAA      = 1 << 7,
    MGL_FMT_CAP_RESOLVE   = 1 << 8,
};

#define MGL_FMT_CAPS_RF       (MGL_FMT_CAP_READ | MGL_FMT_CAP_FILTER)
#define MGL_FMT_CAPS_RC       (MGL_FMT_CAP_READ | MGL_FMT_CAP_COLOR_ATT)
#define MGL_FMT_CAPS_RCB      (MGL_FMT_CAPS_RC | MGL_FMT_CAP_BLEND)
#define MGL_FMT_CAPS_RCM      (MGL_FMT_CAPS_RC | MGL_FMT_CAP_MSAA)
#define MGL_FMT_CAPS_RCMB     (MGL_FMT_CAPS_RCM | MGL_FMT_CAP_BLEND)
#define MGL_FMT_CAPS_RWC      (MGL_FMT_CAPS_RC | MGL_FMT_CAP_WRITE)
#define MGL_FMT_CAPS_RWCB     (MGL_FMT_CAPS_RWC | MGL_FMT_CAP_BLEND)
#define MGL_FMT_CAPS_RWCM     (MGL_FMT_CAPS_RWC | MGL_FMT_CAP_MSAA)
#define MGL_FMT_CAPS_RWCMB    (MGL_FMT_CAPS_RWCM | MGL_FMT_CAP_BLEND)
#define MGL_FMT_CAPS_RFCMRB   (MGL_FMT_CAPS_RCMB | MGL_FMT_CAP_FILTER | MGL_FMT_CAP_RESOLVE)
#define MGL_FMT_CAPS_RFWCMB   (MGL_FMT_CAPS_RWCMB | MGL_FMT_CAP_FILTER)
#define MGL_FMT_CAPS_ALL      (MGL_FMT_CAPS_RFWCMB | MGL_FMT_CAP_RESOLVE)
#define MGL_FMT_CAPS_DRM      (MGL_FMT_CAP_DS_ATT | MGL_FMT_CAP_READ | MGL_FMT_CAP_MSAA)
#define MGL_FMT_CAPS_DRFM     (MGL_FMT_CAPS_DRM | MGL_FMT_CAP_FILTER)
#define MGL_FMT_CAPS_DRMR     (MGL_FMT_CAPS_DRM | MGL_FMT_CAP_RESOLVE)
#define MGL_FMT_CAPS_DRFMR    (MGL_FMT_CAPS_DRFM | MGL_FMT_CAP_RESOLVE)

/* ---------------------------------------------------------------- */
/*  Metal view class — mirrors MVKMTLViewClass                       */
/* ---------------------------------------------------------------- */
typedef enum {
    MGL_VIEW_NONE = 0,
    MGL_VIEW_COLOR8,
    MGL_VIEW_COLOR16,
    MGL_VIEW_COLOR32,
    MGL_VIEW_COLOR64,
    MGL_VIEW_COLOR128,
    MGL_VIEW_PVRTC_RGB_2BPP,
    MGL_VIEW_PVRTC_RGB_4BPP,
    MGL_VIEW_PVRTC_RGBA_2BPP,
    MGL_VIEW_PVRTC_RGBA_4BPP,
    MGL_VIEW_EAC_R11,
    MGL_VIEW_EAC_RG11,
    MGL_VIEW_EAC_RGBA8,
    MGL_VIEW_ETC2_RGB8,
    MGL_VIEW_ETC2_RGB8A1,
    MGL_VIEW_ASTC_4x4,
    MGL_VIEW_ASTC_5x4,
    MGL_VIEW_ASTC_5x5,
    MGL_VIEW_ASTC_6x5,
    MGL_VIEW_ASTC_6x6,
    MGL_VIEW_ASTC_8x5,
    MGL_VIEW_ASTC_8x6,
    MGL_VIEW_ASTC_8x8,
    MGL_VIEW_ASTC_10x5,
    MGL_VIEW_ASTC_10x6,
    MGL_VIEW_ASTC_10x8,
    MGL_VIEW_ASTC_10x10,
    MGL_VIEW_ASTC_12x10,
    MGL_VIEW_ASTC_12x12,
    MGL_VIEW_BC1_RGBA,
    MGL_VIEW_BC2_RGBA,
    MGL_VIEW_BC3_RGBA,
    MGL_VIEW_BC4_R,
    MGL_VIEW_BC5_RG,
    MGL_VIEW_BC6H_RGB,
    MGL_VIEW_BC7_RGBA,
    MGL_VIEW_DEPTH24_STENCIL8,
    MGL_VIEW_DEPTH32_STENCIL8,
    MGL_VIEW_BGRA10_XR,
    MGL_VIEW_BGR10_XR,
} MGLViewClass;

/* ---------------------------------------------------------------- */
/*  format descriptors                                               */
/* ---------------------------------------------------------------- */

typedef struct {
    GLenum   gl_format;          /* GL sized internal format, e.g. GL_RGBA8. Key. */
    uint16_t mtl_format;         /* MTLPixelFormat, or MTLPixelFormatInvalid */
    uint16_t mtl_substitute;     /* what MGL uses when mtl_format is Invalid */
    uint8_t  block_w, block_h;   /* 1,1 for uncompressed */
    uint8_t  bytes_per_block;    /* bytes per texel for uncompressed */
    uint8_t  kind;               /* MGLFormatKind */
    uint8_t  bits[6];            /* R, G, B, A, depth, stencil */
    GLenum   base_format;        /* GL_RED, GL_RG, GL_RGB, GL_RGBA, ... */
    GLenum   upload_format;      /* best client format for glTexImage */
    GLenum   upload_type;
    bool     srgb;
    bool     color_renderable;   /* GL 4.6 spec required-format tables (CR column) */
    bool     texture_filterable; /* spec "Req. tex." + not integer */
    const char *name;            /* "GL_RGBA8" */
} MGLFormatDesc;

typedef struct {
    uint16_t mtl_format;
    uint16_t mtl_linear;         /* the non-sRGB twin, or itself */
    uint8_t  view_class;         /* MGLViewClass */
    uint16_t caps_apple;         /* capability bits on Apple-family GPU */
    uint16_t caps_mac;           /* capability bits on Intel/AMD Mac GPU */
    const char *name;            /* "MTLPixelFormatRGBA8Unorm" */
} MGLMetalFormatDesc;

/* ---------------------------------------------------------------- */
/*  lookups — never return NULL                                      */
/* ---------------------------------------------------------------- */
const MGLFormatDesc      *mglFormatDesc(GLenum gl_internal_format);
const MGLMetalFormatDesc *mglMetalFormatDesc(uint16_t mtl_format);
const MGLFormatDesc      *mglFormatDescForMetal(uint16_t mtl_format);

/* ---------------------------------------------------------------- */
/*  device facts, set once by the Metal layer at start-up            */
/* ---------------------------------------------------------------- */
typedef struct {
    bool apple_gpu;
    bool supports_bc;
    bool supports_depth24_stencil8;
    bool supports_32bit_msaa;
    bool supports_32bit_float_filtering;
    bool supports_astc_hdr;        /* Apple6+ */
} MGLDeviceFormatCaps;

void mglFormatTableSetDevice(const MGLDeviceFormatCaps *caps);

/* ---------------------------------------------------------------- */
/*  everyday answers                                                 */
/* ---------------------------------------------------------------- */
uint16_t mglFormatMetalFormat(GLenum gl_internal_format);

/* MGL_FMT_COLOR_FLOAT / _COLOR_INT / _DEPTH / ... for a GL internal format */
uint8_t mglFormatKind(GLenum gl_internal_format);

/* GL_RGBA and friends leave the bit depth to us; this picks it. Sized formats pass through. */
GLenum mglFormatSizedForBase(GLenum gl_internal_format);
uint16_t mglFormatCaps(GLenum gl_internal_format);
bool     mglFormatIsCompressed(GLenum gl_internal_format);
/* readback format rules, GL 4.6 section 8.11.4 */
bool     mglClientFormatIsInteger(GLenum format);
bool     mglReadbackFormatAgrees(GLenum internalformat, GLenum format);
bool     mglFormatTypeAgrees(GLenum format, GLenum type);
size_t   mglFormatBytesPerRow(GLenum gl_internal_format, GLsizei width);
size_t   mglFormatImageSize(GLenum gl_internal_format, GLsizei w, GLsizei h, GLsizei d);
GLint    mglFormatComponentBits(GLenum gl_internal_format, GLenum component);
GLenum   mglFormatComponentType(GLenum gl_internal_format);
bool     mglFormatIsSRGB(GLenum gl_internal_format);
GLsizei  mglFormatCompressedFormatList(GLenum *out, GLsizei max);
size_t   mglFormatTableCount(void);
const MGLFormatDesc *mglFormatTableRow(size_t i);

#ifdef __cplusplus
}
#endif

#endif /* mgl_format_table_h */
