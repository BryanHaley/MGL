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
 * mgl_format_table.c
 * MGL
 *
 * Format capability data transcribed from MoltenVK's MVKPixelFormats.mm
 * (Copyright (c) 2015-2026 The Brenwill Workshop Ltd., Apache 2.0).
 */

#include "mgl_format_table.h"
#include "pixel_utils.h"
#include <stdlib.h>
#include <string.h>

/* ---- legacy / EXT enum defines ------------------------------------ */
#ifndef GL_ALPHA
#define GL_ALPHA                          0x1906
#endif
#ifndef GL_LUMINANCE
#define GL_LUMINANCE                      0x1909
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
#ifndef GL_LUMINANCE8_ALPHA8
#define GL_LUMINANCE8_ALPHA8              0x8045
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
#ifndef GL_ALPHA8UI_EXT
#define GL_ALPHA8UI_EXT                   0x8D7E
#endif
#ifndef GL_RGB16
#define GL_RGB16                          0x8054
#endif
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT  0x8C4C
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR   0x93B0
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#endif
#ifndef GL_ALPHA8I_EXT
#define GL_ALPHA8I_EXT                    0x8D7B
#endif
#ifndef GL_ALPHA32I_EXT
#define GL_ALPHA32I_EXT                   0x8D81
#endif
#ifndef GL_ALPHA16I_EXT
#define GL_ALPHA16I_EXT                   0x8D87
#endif
#ifndef GL_ALPHA32UI_EXT
#define GL_ALPHA32UI_EXT                  0x8D72
#endif
#ifndef GL_ALPHA16UI_EXT
#define GL_ALPHA16UI_EXT                  0x8D93
#endif

/* ---- helpers ------------------------------------------------------ */
#define _INV MTLPixelFormatInvalid
#define _NONE MGL_FMT_NONE
#define _CF MGL_FMT_COLOR_FLOAT
#define _CI MGL_FMT_COLOR_INT
#define _CU MGL_FMT_COLOR_UINT
#define _D MGL_FMT_DEPTH
#define _S MGL_FMT_STENCIL
#define _DS MGL_FMT_DEPTH_STENCIL
#define _CX MGL_FMT_COMPRESSED

#define _V0  MGL_VIEW_NONE
#define _V8  MGL_VIEW_COLOR8
#define _V16 MGL_VIEW_COLOR16
#define _V32 MGL_VIEW_COLOR32
#define _V64 MGL_VIEW_COLOR64
#define _V128 MGL_VIEW_COLOR128
#define _VP2 MGL_VIEW_PVRTC_RGBA_2BPP
#define _VP4 MGL_VIEW_PVRTC_RGBA_4BPP
#define _VE1 MGL_VIEW_EAC_R11
#define _VE2 MGL_VIEW_EAC_RG11
#define _VEA MGL_VIEW_EAC_RGBA8
#define _VET MGL_VIEW_ETC2_RGB8
#define _VE8 MGL_VIEW_ETC2_RGB8A1
#define _VA4 MGL_VIEW_ASTC_4x4
#define _VA54 MGL_VIEW_ASTC_5x4
#define _VA5 MGL_VIEW_ASTC_5x5
#define _VA65 MGL_VIEW_ASTC_6x5
#define _VA6 MGL_VIEW_ASTC_6x6
#define _VA85 MGL_VIEW_ASTC_8x5
#define _VA86 MGL_VIEW_ASTC_8x6
#define _VA8 MGL_VIEW_ASTC_8x8
#define _VA105 MGL_VIEW_ASTC_10x5
#define _VA106 MGL_VIEW_ASTC_10x6
#define _VA108 MGL_VIEW_ASTC_10x8
#define _VA10 MGL_VIEW_ASTC_10x10
#define _VA1210 MGL_VIEW_ASTC_12x10
#define _VA12 MGL_VIEW_ASTC_12x12
#define _VB1 MGL_VIEW_BC1_RGBA
#define _VB2 MGL_VIEW_BC2_RGBA
#define _VB3 MGL_VIEW_BC3_RGBA
#define _VB4 MGL_VIEW_BC4_R
#define _VB5 MGL_VIEW_BC5_RG
#define _VB6 MGL_VIEW_BC6H_RGB
#define _VB7 MGL_VIEW_BC7_RGBA
#define _VD24 MGL_VIEW_DEPTH24_STENCIL8
#define _VD32 MGL_VIEW_DEPTH32_STENCIL8
#define _VXR MGL_VIEW_BGRA10_XR
#define _VX10 MGL_VIEW_BGR10_XR

#define _BR  GL_RED
#define _BRG GL_RG
#define _BRGB GL_RGB
#define _BRGBA GL_RGBA
#define _BD GL_DEPTH_COMPONENT
#define _BS GL_STENCIL_INDEX
#define _BDS GL_DEPTH_STENCIL

static const MGLFormatDesc _unknown_gl = {0};
static const MGLMetalFormatDesc _unknown_mtl = {0};

/* ================================================================ */
/*  mtl_table  —  one row per Metal pixel format                    */
/* ================================================================ */
static const MGLMetalFormatDesc mtl_table[] = {
    { _INV, _INV, _V0, MGL_FMT_CAP_NONE, MGL_FMT_CAP_NONE, "MTLPixelFormatInvalid" },
    { MTLPixelFormatA8Unorm,         MTLPixelFormatA8Unorm,       _V8,  MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatA8Unorm" },
    { MTLPixelFormatR8Unorm,         MTLPixelFormatR8Unorm,       _V8,  MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatR8Unorm" },
    { MTLPixelFormatR8Unorm_sRGB,    MTLPixelFormatR8Unorm,       _V8,  MGL_FMT_CAPS_ALL,    MGL_FMT_CAP_NONE,    "MTLPixelFormatR8Unorm_sRGB" },
    { MTLPixelFormatR8Snorm,         MTLPixelFormatR8Snorm,       _V8,  MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatR8Snorm" },
    { MTLPixelFormatR8Uint,          MTLPixelFormatR8Uint,        _V8,  MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatR8Uint" },
    { MTLPixelFormatR8Sint,          MTLPixelFormatR8Sint,        _V8,  MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatR8Sint" },
    { MTLPixelFormatR16Unorm,        MTLPixelFormatR16Unorm,      _V16, MGL_FMT_CAPS_RFWCMB, MGL_FMT_CAPS_ALL,    "MTLPixelFormatR16Unorm" },
    { MTLPixelFormatR16Snorm,        MTLPixelFormatR16Snorm,      _V16, MGL_FMT_CAPS_RFWCMB, MGL_FMT_CAPS_ALL,    "MTLPixelFormatR16Snorm" },
    { MTLPixelFormatR16Uint,         MTLPixelFormatR16Uint,       _V16, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatR16Uint" },
    { MTLPixelFormatR16Sint,         MTLPixelFormatR16Sint,       _V16, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatR16Sint" },
    { MTLPixelFormatR16Float,        MTLPixelFormatR16Float,      _V16, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatR16Float" },
    { MTLPixelFormatRG8Unorm,        MTLPixelFormatRG8Unorm,      _V16, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRG8Unorm" },
    { MTLPixelFormatRG8Unorm_sRGB,   MTLPixelFormatRG8Unorm,      _V16, MGL_FMT_CAPS_ALL,    MGL_FMT_CAP_NONE,    "MTLPixelFormatRG8Unorm_sRGB" },
    { MTLPixelFormatRG8Snorm,        MTLPixelFormatRG8Snorm,      _V16, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRG8Snorm" },
    { MTLPixelFormatRG8Uint,         MTLPixelFormatRG8Uint,       _V16, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRG8Uint" },
    { MTLPixelFormatRG8Sint,         MTLPixelFormatRG8Sint,       _V16, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRG8Sint" },
    { MTLPixelFormatB5G6R5Unorm,     MTLPixelFormatB5G6R5Unorm,   _V16, MGL_FMT_CAPS_RFCMRB, MGL_FMT_CAP_NONE,    "MTLPixelFormatB5G6R5Unorm" },
    { MTLPixelFormatA1BGR5Unorm,     MTLPixelFormatA1BGR5Unorm,   _V16, MGL_FMT_CAPS_RFCMRB, MGL_FMT_CAP_NONE,    "MTLPixelFormatA1BGR5Unorm" },
    { MTLPixelFormatABGR4Unorm,      MTLPixelFormatABGR4Unorm,    _V16, MGL_FMT_CAPS_RFCMRB, MGL_FMT_CAP_NONE,    "MTLPixelFormatABGR4Unorm" },
    { MTLPixelFormatBGR5A1Unorm,     MTLPixelFormatBGR5A1Unorm,   _V16, MGL_FMT_CAPS_RFCMRB, MGL_FMT_CAP_NONE,    "MTLPixelFormatBGR5A1Unorm" },
    { MTLPixelFormatR32Uint,         MTLPixelFormatR32Uint,       _V32, MGL_FMT_CAPS_RWC,    MGL_FMT_CAPS_RWCM,   "MTLPixelFormatR32Uint" },
    { MTLPixelFormatR32Sint,         MTLPixelFormatR32Sint,       _V32, MGL_FMT_CAPS_RWC,    MGL_FMT_CAPS_RWCM,   "MTLPixelFormatR32Sint" },
    { MTLPixelFormatR32Float,        MTLPixelFormatR32Float,      _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatR32Float" },
    { MTLPixelFormatRG16Unorm,       MTLPixelFormatRG16Unorm,     _V32, MGL_FMT_CAPS_RFWCMB, MGL_FMT_CAPS_ALL,    "MTLPixelFormatRG16Unorm" },
    { MTLPixelFormatRG16Snorm,       MTLPixelFormatRG16Snorm,     _V32, MGL_FMT_CAPS_RFWCMB, MGL_FMT_CAPS_ALL,    "MTLPixelFormatRG16Snorm" },
    { MTLPixelFormatRG16Uint,        MTLPixelFormatRG16Uint,      _V32, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRG16Uint" },
    { MTLPixelFormatRG16Sint,        MTLPixelFormatRG16Sint,      _V32, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRG16Sint" },
    { MTLPixelFormatRG16Float,       MTLPixelFormatRG16Float,     _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRG16Float" },
    { MTLPixelFormatRGBA8Unorm,      MTLPixelFormatRGBA8Unorm,    _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRGBA8Unorm" },
    { MTLPixelFormatRGBA8Unorm_sRGB, MTLPixelFormatRGBA8Unorm,    _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_RFCMRB, "MTLPixelFormatRGBA8Unorm_sRGB" },
    { MTLPixelFormatRGBA8Snorm,      MTLPixelFormatRGBA8Snorm,    _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRGBA8Snorm" },
    { MTLPixelFormatRGBA8Uint,       MTLPixelFormatRGBA8Uint,     _V32, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRGBA8Uint" },
    { MTLPixelFormatRGBA8Sint,       MTLPixelFormatRGBA8Sint,     _V32, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRGBA8Sint" },
    { MTLPixelFormatBGRA8Unorm,      MTLPixelFormatBGRA8Unorm,    _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatBGRA8Unorm" },
    { MTLPixelFormatBGRA8Unorm_sRGB, MTLPixelFormatBGRA8Unorm,    _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_RFCMRB, "MTLPixelFormatBGRA8Unorm_sRGB" },
    { MTLPixelFormatRGB10A2Unorm,    MTLPixelFormatRGB10A2Unorm,  _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRGB10A2Unorm" },
    { MTLPixelFormatBGR10A2Unorm,    MTLPixelFormatBGR10A2Unorm,  _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatBGR10A2Unorm" },
    { MTLPixelFormatRGB10A2Uint,     MTLPixelFormatRGB10A2Uint,   _V32, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRGB10A2Uint" },
    { MTLPixelFormatRG11B10Float,    MTLPixelFormatRG11B10Float,  _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRG11B10Float" },
    { MTLPixelFormatRGB9E5Float,     MTLPixelFormatRGB9E5Float,   _V32, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_RF,     "MTLPixelFormatRGB9E5Float" },
    { MTLPixelFormatRG32Uint,        MTLPixelFormatRG32Uint,      _V64, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRG32Uint" },
    { MTLPixelFormatRG32Sint,        MTLPixelFormatRG32Sint,      _V64, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRG32Sint" },
    { MTLPixelFormatRG32Float,       MTLPixelFormatRG32Float,     _V64, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRG32Float" },
    { MTLPixelFormatRGBA16Unorm,     MTLPixelFormatRGBA16Unorm,   _V64, MGL_FMT_CAPS_RFWCMB, MGL_FMT_CAPS_ALL,    "MTLPixelFormatRGBA16Unorm" },
    { MTLPixelFormatRGBA16Snorm,     MTLPixelFormatRGBA16Snorm,   _V64, MGL_FMT_CAPS_RFWCMB, MGL_FMT_CAPS_ALL,    "MTLPixelFormatRGBA16Snorm" },
    { MTLPixelFormatRGBA16Uint,      MTLPixelFormatRGBA16Uint,    _V64, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRGBA16Uint" },
    { MTLPixelFormatRGBA16Sint,      MTLPixelFormatRGBA16Sint,    _V64, MGL_FMT_CAPS_RWCM,   MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRGBA16Sint" },
    { MTLPixelFormatRGBA16Float,     MTLPixelFormatRGBA16Float,   _V64, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRGBA16Float" },
    { MTLPixelFormatRGBA32Uint,      MTLPixelFormatRGBA32Uint,    _V128, MGL_FMT_CAPS_RWC,    MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRGBA32Uint" },
    { MTLPixelFormatRGBA32Sint,      MTLPixelFormatRGBA32Sint,    _V128, MGL_FMT_CAPS_RWC,    MGL_FMT_CAPS_RWCM,   "MTLPixelFormatRGBA32Sint" },
    { MTLPixelFormatRGBA32Float,     MTLPixelFormatRGBA32Float,   _V128, MGL_FMT_CAPS_ALL,    MGL_FMT_CAPS_ALL,    "MTLPixelFormatRGBA32Float" },
    { MTLPixelFormatPVRTC_RGBA_2BPP,       MTLPixelFormatPVRTC_RGBA_2BPP,       _VP2, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatPVRTC_RGBA_2BPP" },
    { MTLPixelFormatPVRTC_RGBA_2BPP_sRGB,  MTLPixelFormatPVRTC_RGBA_2BPP,       _VP2, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatPVRTC_RGBA_2BPP_sRGB" },
    { MTLPixelFormatPVRTC_RGBA_4BPP,       MTLPixelFormatPVRTC_RGBA_4BPP,       _VP4, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatPVRTC_RGBA_4BPP" },
    { MTLPixelFormatPVRTC_RGBA_4BPP_sRGB,  MTLPixelFormatPVRTC_RGBA_4BPP,       _VP4, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatPVRTC_RGBA_4BPP_sRGB" },
    { MTLPixelFormatETC2_RGB8,        MTLPixelFormatETC2_RGB8,        _VET, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatETC2_RGB8" },
    { MTLPixelFormatETC2_RGB8_sRGB,   MTLPixelFormatETC2_RGB8,        _VET, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatETC2_RGB8_sRGB" },
    { MTLPixelFormatETC2_RGB8A1,      MTLPixelFormatETC2_RGB8A1,      _VE8, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatETC2_RGB8A1" },
    { MTLPixelFormatETC2_RGB8A1_sRGB, MTLPixelFormatETC2_RGB8A1,      _VE8, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatETC2_RGB8A1_sRGB" },
    { MTLPixelFormatEAC_RGBA8,        MTLPixelFormatEAC_RGBA8,        _VEA, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatEAC_RGBA8" },
    { MTLPixelFormatEAC_RGBA8_sRGB,   MTLPixelFormatEAC_RGBA8,        _VEA, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatEAC_RGBA8_sRGB" },
    { MTLPixelFormatEAC_R11Unorm,     MTLPixelFormatEAC_R11Unorm,     _VE1, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatEAC_R11Unorm" },
    { MTLPixelFormatEAC_R11Snorm,     MTLPixelFormatEAC_R11Snorm,     _VE1, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatEAC_R11Snorm" },
    { MTLPixelFormatEAC_RG11Unorm,    MTLPixelFormatEAC_RG11Unorm,    _VE2, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatEAC_RG11Unorm" },
    { MTLPixelFormatEAC_RG11Snorm,    MTLPixelFormatEAC_RG11Snorm,    _VE2, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatEAC_RG11Snorm" },
    { MTLPixelFormatASTC_4x4_LDR,     MTLPixelFormatASTC_4x4_LDR,     _VA4,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_4x4_LDR" },
    { MTLPixelFormatASTC_4x4_sRGB,    MTLPixelFormatASTC_4x4_LDR,     _VA4,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_4x4_sRGB" },
    { MTLPixelFormatASTC_4x4_HDR,     MTLPixelFormatASTC_4x4_HDR,     _VA4,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_4x4_HDR" },
    { MTLPixelFormatASTC_5x4_LDR,     MTLPixelFormatASTC_5x4_LDR,     _VA54,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_5x4_LDR" },
    { MTLPixelFormatASTC_5x4_sRGB,    MTLPixelFormatASTC_5x4_LDR,     _VA54,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_5x4_sRGB" },
    { MTLPixelFormatASTC_5x4_HDR,     MTLPixelFormatASTC_5x4_HDR,     _VA54,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_5x4_HDR" },
    { MTLPixelFormatASTC_5x5_LDR,     MTLPixelFormatASTC_5x5_LDR,     _VA5,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_5x5_LDR" },
    { MTLPixelFormatASTC_5x5_sRGB,    MTLPixelFormatASTC_5x5_LDR,     _VA5,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_5x5_sRGB" },
    { MTLPixelFormatASTC_5x5_HDR,     MTLPixelFormatASTC_5x5_HDR,     _VA5,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_5x5_HDR" },
    { MTLPixelFormatASTC_6x5_LDR,     MTLPixelFormatASTC_6x5_LDR,     _VA65,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_6x5_LDR" },
    { MTLPixelFormatASTC_6x5_sRGB,    MTLPixelFormatASTC_6x5_LDR,     _VA65,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_6x5_sRGB" },
    { MTLPixelFormatASTC_6x5_HDR,     MTLPixelFormatASTC_6x5_HDR,     _VA65,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_6x5_HDR" },
    { MTLPixelFormatASTC_6x6_LDR,     MTLPixelFormatASTC_6x6_LDR,     _VA6,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_6x6_LDR" },
    { MTLPixelFormatASTC_6x6_sRGB,    MTLPixelFormatASTC_6x6_LDR,     _VA6,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_6x6_sRGB" },
    { MTLPixelFormatASTC_6x6_HDR,     MTLPixelFormatASTC_6x6_HDR,     _VA6,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_6x6_HDR" },
    { MTLPixelFormatASTC_8x5_LDR,     MTLPixelFormatASTC_8x5_LDR,     _VA85,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x5_LDR" },
    { MTLPixelFormatASTC_8x5_sRGB,    MTLPixelFormatASTC_8x5_LDR,     _VA85,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x5_sRGB" },
    { MTLPixelFormatASTC_8x5_HDR,     MTLPixelFormatASTC_8x5_HDR,     _VA85,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x5_HDR" },
    { MTLPixelFormatASTC_8x6_LDR,     MTLPixelFormatASTC_8x6_LDR,     _VA86,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x6_LDR" },
    { MTLPixelFormatASTC_8x6_sRGB,    MTLPixelFormatASTC_8x6_LDR,     _VA86,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x6_sRGB" },
    { MTLPixelFormatASTC_8x6_HDR,     MTLPixelFormatASTC_8x6_HDR,     _VA86,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x6_HDR" },
    { MTLPixelFormatASTC_8x8_LDR,     MTLPixelFormatASTC_8x8_LDR,     _VA8,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x8_LDR" },
    { MTLPixelFormatASTC_8x8_sRGB,    MTLPixelFormatASTC_8x8_LDR,     _VA8,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x8_sRGB" },
    { MTLPixelFormatASTC_8x8_HDR,     MTLPixelFormatASTC_8x8_HDR,     _VA8,   MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_8x8_HDR" },
    { MTLPixelFormatASTC_10x5_LDR,    MTLPixelFormatASTC_10x5_LDR,    _VA105, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x5_LDR" },
    { MTLPixelFormatASTC_10x5_sRGB,   MTLPixelFormatASTC_10x5_LDR,    _VA105, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x5_sRGB" },
    { MTLPixelFormatASTC_10x5_HDR,    MTLPixelFormatASTC_10x5_HDR,    _VA105, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x5_HDR" },
    { MTLPixelFormatASTC_10x6_LDR,    MTLPixelFormatASTC_10x6_LDR,    _VA106, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x6_LDR" },
    { MTLPixelFormatASTC_10x6_sRGB,   MTLPixelFormatASTC_10x6_LDR,    _VA106, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x6_sRGB" },
    { MTLPixelFormatASTC_10x6_HDR,    MTLPixelFormatASTC_10x6_HDR,    _VA106, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x6_HDR" },
    { MTLPixelFormatASTC_10x8_LDR,    MTLPixelFormatASTC_10x8_LDR,    _VA108, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x8_LDR" },
    { MTLPixelFormatASTC_10x8_sRGB,   MTLPixelFormatASTC_10x8_LDR,    _VA108, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x8_sRGB" },
    { MTLPixelFormatASTC_10x8_HDR,    MTLPixelFormatASTC_10x8_HDR,    _VA108, MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x8_HDR" },
    { MTLPixelFormatASTC_10x10_LDR,   MTLPixelFormatASTC_10x10_LDR,   _VA10,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x10_LDR" },
    { MTLPixelFormatASTC_10x10_sRGB,  MTLPixelFormatASTC_10x10_LDR,   _VA10,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x10_sRGB" },
    { MTLPixelFormatASTC_10x10_HDR,   MTLPixelFormatASTC_10x10_HDR,   _VA10,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_10x10_HDR" },
    { MTLPixelFormatASTC_12x10_LDR,   MTLPixelFormatASTC_12x10_LDR,   _VA1210,MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_12x10_LDR" },
    { MTLPixelFormatASTC_12x10_sRGB,  MTLPixelFormatASTC_12x10_LDR,   _VA1210,MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_12x10_sRGB" },
    { MTLPixelFormatASTC_12x10_HDR,   MTLPixelFormatASTC_12x10_HDR,   _VA1210,MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_12x10_HDR" },
    { MTLPixelFormatASTC_12x12_LDR,   MTLPixelFormatASTC_12x12_LDR,   _VA12,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_12x12_LDR" },
    { MTLPixelFormatASTC_12x12_sRGB,  MTLPixelFormatASTC_12x12_LDR,   _VA12,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_12x12_sRGB" },
    { MTLPixelFormatASTC_12x12_HDR,   MTLPixelFormatASTC_12x12_HDR,   _VA12,  MGL_FMT_CAPS_RF, MGL_FMT_CAP_NONE, "MTLPixelFormatASTC_12x12_HDR" },
    { MTLPixelFormatBC1_RGBA,          MTLPixelFormatBC1_RGBA,          _VB1, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC1_RGBA" },
    { MTLPixelFormatBC1_RGBA_sRGB,     MTLPixelFormatBC1_RGBA,          _VB1, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC1_RGBA_sRGB" },
    { MTLPixelFormatBC2_RGBA,          MTLPixelFormatBC2_RGBA,          _VB2, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC2_RGBA" },
    { MTLPixelFormatBC2_RGBA_sRGB,     MTLPixelFormatBC2_RGBA,          _VB2, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC2_RGBA_sRGB" },
    { MTLPixelFormatBC3_RGBA,          MTLPixelFormatBC3_RGBA,          _VB3, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC3_RGBA" },
    { MTLPixelFormatBC3_RGBA_sRGB,     MTLPixelFormatBC3_RGBA,          _VB3, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC3_RGBA_sRGB" },
    { MTLPixelFormatBC4_RUnorm,        MTLPixelFormatBC4_RUnorm,        _VB4, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC4_RUnorm" },
    { MTLPixelFormatBC4_RSnorm,        MTLPixelFormatBC4_RSnorm,        _VB4, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC4_RSnorm" },
    { MTLPixelFormatBC5_RGUnorm,       MTLPixelFormatBC5_RGUnorm,       _VB5, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC5_RGUnorm" },
    { MTLPixelFormatBC5_RGSnorm,       MTLPixelFormatBC5_RGSnorm,       _VB5, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC5_RGSnorm" },
    { MTLPixelFormatBC6H_RGBUfloat,    MTLPixelFormatBC6H_RGBUfloat,    _VB6, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC6H_RGBUfloat" },
    { MTLPixelFormatBC6H_RGBFloat,     MTLPixelFormatBC6H_RGBFloat,     _VB6, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC6H_RGBFloat" },
    { MTLPixelFormatBC7_RGBAUnorm,     MTLPixelFormatBC7_RGBAUnorm,     _VB7, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC7_RGBAUnorm" },
    { MTLPixelFormatBC7_RGBAUnorm_sRGB,MTLPixelFormatBC7_RGBAUnorm,     _VB7, MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBC7_RGBAUnorm_sRGB" },
    { MTLPixelFormatGBGR422,           MTLPixelFormatGBGR422,           _V0,  MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatGBGR422" },
    { MTLPixelFormatBGRG422,           MTLPixelFormatBGRG422,           _V0,  MGL_FMT_CAPS_RF, MGL_FMT_CAPS_RF, "MTLPixelFormatBGRG422" },
    { MTLPixelFormatBGRA10_XR,         MTLPixelFormatBGRA10_XR,         _VXR,  MGL_FMT_CAPS_ALL,    MGL_FMT_CAP_NONE, "MTLPixelFormatBGRA10_XR" },
    { MTLPixelFormatBGRA10_XR_sRGB,    MTLPixelFormatBGRA10_XR,         _VXR,  MGL_FMT_CAPS_ALL,    MGL_FMT_CAP_NONE, "MTLPixelFormatBGRA10_XR_sRGB" },
    { MTLPixelFormatBGR10_XR,          MTLPixelFormatBGR10_XR,          _VX10, MGL_FMT_CAPS_ALL,    MGL_FMT_CAP_NONE, "MTLPixelFormatBGR10_XR" },
    { MTLPixelFormatBGR10_XR_sRGB,     MTLPixelFormatBGR10_XR,          _VX10, MGL_FMT_CAPS_ALL,    MGL_FMT_CAP_NONE, "MTLPixelFormatBGR10_XR_sRGB" },
    { MTLPixelFormatDepth16Unorm,          MTLPixelFormatDepth16Unorm,          _V0,   MGL_FMT_CAPS_DRFMR, MGL_FMT_CAPS_DRFMR, "MTLPixelFormatDepth16Unorm" },
    { MTLPixelFormatDepth32Float,          MTLPixelFormatDepth32Float,          _V0,   MGL_FMT_CAPS_DRMR,  MGL_FMT_CAPS_DRFMR, "MTLPixelFormatDepth32Float" },
    { MTLPixelFormatStencil8,              MTLPixelFormatStencil8,              _V0,   MGL_FMT_CAPS_DRM,   MGL_FMT_CAPS_DRM,   "MTLPixelFormatStencil8" },
    { MTLPixelFormatDepth24Unorm_Stencil8, MTLPixelFormatDepth24Unorm_Stencil8, _VD24, MGL_FMT_CAP_NONE,  MGL_FMT_CAPS_DRFMR, "MTLPixelFormatDepth24Unorm_Stencil8" },
    { MTLPixelFormatDepth32Float_Stencil8, MTLPixelFormatDepth32Float_Stencil8, _VD32, MGL_FMT_CAPS_DRMR,  MGL_FMT_CAPS_DRFMR, "MTLPixelFormatDepth32Float_Stencil8" },
    { MTLPixelFormatX24_Stencil8,          MTLPixelFormatX24_Stencil8,          _VD24, MGL_FMT_CAP_NONE,  MGL_FMT_CAPS_DRM,   "MTLPixelFormatX24_Stencil8" },
    { MTLPixelFormatX32_Stencil8,          MTLPixelFormatX32_Stencil8,          _VD32, MGL_FMT_CAPS_DRM,   MGL_FMT_CAPS_DRM,   "MTLPixelFormatX32_Stencil8" },
};
static const size_t mtl_table_count = sizeof(mtl_table) / sizeof(mtl_table[0]);

/* ================================================================ */
/*  gl_table  —  one row per GL sized internal format               */
/* ================================================================ */
static const MGLFormatDesc gl_table[] = {

    /* unsized base formats */
    { GL_RED,             _INV, _INV, 1,1, 0, _NONE, {0,0,0,0,0,0}, _BR,   GL_RED,  GL_UNSIGNED_BYTE,  false, false, false, "GL_RED" },
    { GL_RG,              _INV, _INV, 1,1, 0, _NONE, {0,0,0,0,0,0}, _BRG,  GL_RG,   GL_UNSIGNED_BYTE,  false, false, false, "GL_RG" },
    { GL_RGB,             _INV, _INV, 1,1, 0, _NONE, {0,0,0,0,0,0}, _BRGB, GL_RGB,  GL_UNSIGNED_BYTE,  false, false, false, "GL_RGB" },
    { GL_RGBA,            _INV, _INV, 1,1, 0, _NONE, {0,0,0,0,0,0}, _BRGBA,GL_RGBA, GL_UNSIGNED_BYTE,  false, false, false, "GL_RGBA" },
    { GL_DEPTH_COMPONENT, _INV, _INV, 1,1, 0, _NONE, {0,0,0,0,0,0}, _BD,   GL_DEPTH_COMPONENT, GL_FLOAT, false, false, false, "GL_DEPTH_COMPONENT" },
    { GL_STENCIL_INDEX,   _INV, _INV, 1,1, 0, _NONE, {0,0,0,0,0,0}, _BS,   GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, false, false, false, "GL_STENCIL_INDEX" },
    { GL_DEPTH_STENCIL,   _INV, _INV, 1,1, 0, _NONE, {0,0,0,0,0,0}, _BDS,  GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, false, false, false, "GL_DEPTH_STENCIL" },

    /* unsized compressed — resolve to ETC2/EAC sized equivalents */
    { GL_COMPRESSED_RED,            _INV, MTLPixelFormatEAC_R11Unorm,  4,4, 8, _CX, {0,0,0,0,0,0}, _BR,   GL_RED,  GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RED" },
    { GL_COMPRESSED_RG,             _INV, MTLPixelFormatEAC_RG11Unorm, 4,4,16, _CX, {0,0,0,0,0,0}, _BRG,  GL_RG,   GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RG" },
    { GL_COMPRESSED_RGB,            _INV, MTLPixelFormatETC2_RGB8,     4,4, 8, _CX, {0,0,0,0,0,0}, _BRGB, GL_RGB,  GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGB" },
    { GL_COMPRESSED_RGBA,           _INV, MTLPixelFormatEAC_RGBA8,     4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA,GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA" },
    { GL_COMPRESSED_SRGB,           _INV, MTLPixelFormatETC2_RGB8_sRGB, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGB, GL_RGB,  GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB" },
    { GL_COMPRESSED_SRGB_ALPHA,     _INV, MTLPixelFormatEAC_RGBA8_sRGB, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA,GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB_ALPHA" },

    /* ======== colour (table 8.12) ======== */

    /* R 8-bit */
    { GL_R8,        MTLPixelFormatR8Unorm,      _INV, 1,1, 1, _CF, {8,0,0,0,0,0}, _BR,   GL_RED,  GL_UNSIGNED_BYTE,  false, true,  true,  "GL_R8" },
    { GL_R8_SNORM,  MTLPixelFormatR8Snorm,      _INV, 1,1, 1, _CF, {8,0,0,0,0,0}, _BR,   GL_RED,  GL_BYTE,           false, true,  true,  "GL_R8_SNORM" },
    { GL_R8I,       MTLPixelFormatR8Sint,       _INV, 1,1, 1, _CI, {8,0,0,0,0,0}, _BR,   GL_RED_INTEGER,  GL_BYTE,   false, true,  false, "GL_R8I" },
    { GL_R8UI,      MTLPixelFormatR8Uint,       _INV, 1,1, 1, _CU, {8,0,0,0,0,0}, _BR,   GL_RED_INTEGER,  GL_UNSIGNED_BYTE, false, true, false, "GL_R8UI" },
    { GL_SR8_EXT,   MTLPixelFormatR8Unorm_sRGB, _INV, 1,1, 1, _CF, {8,0,0,0,0,0}, _BR,   GL_RED,  GL_UNSIGNED_BYTE,  true,  true,  true,  "GL_SR8_EXT" },

    /* R 16-bit */
    { GL_R16,       MTLPixelFormatR16Unorm,      _INV, 1,1, 2, _CF, {16,0,0,0,0,0}, _BR,  GL_RED,  GL_UNSIGNED_SHORT, false, true,  true,  "GL_R16" },
    { GL_R16_SNORM, MTLPixelFormatR16Snorm,      _INV, 1,1, 2, _CF, {16,0,0,0,0,0}, _BR,  GL_RED,  GL_SHORT,          false, true,  true,  "GL_R16_SNORM" },
    { GL_R16F,      MTLPixelFormatR16Float,      _INV, 1,1, 2, _CF, {16,0,0,0,0,0}, _BR,  GL_RED,  GL_HALF_FLOAT,      false, true,  true,  "GL_R16F" },
    { GL_R32F,      MTLPixelFormatR32Float,      _INV, 1,1, 4, _CF, {32,0,0,0,0,0}, _BR,  GL_RED,  GL_FLOAT,           false, true,  true,  "GL_R32F" },
    { GL_R16I,      MTLPixelFormatR16Sint,       _INV, 1,1, 2, _CI, {16,0,0,0,0,0}, _BR,  GL_RED_INTEGER,  GL_SHORT,   false, true,  false, "GL_R16I" },
    { GL_R16UI,     MTLPixelFormatR16Uint,       _INV, 1,1, 2, _CU, {16,0,0,0,0,0}, _BR,  GL_RED_INTEGER,  GL_UNSIGNED_SHORT, false, true, false, "GL_R16UI" },
    { GL_R32I,      MTLPixelFormatR32Sint,       _INV, 1,1, 4, _CI, {32,0,0,0,0,0}, _BR,  GL_RED_INTEGER,  GL_INT,     false, true,  false, "GL_R32I" },
    { GL_R32UI,     MTLPixelFormatR32Uint,       _INV, 1,1, 4, _CU, {32,0,0,0,0,0}, _BR,  GL_RED_INTEGER,  GL_UNSIGNED_INT, false, true, false, "GL_R32UI" },

    /* RG 8-bit */
    { GL_RG8,       MTLPixelFormatRG8Unorm,       _INV, 1,1, 2, _CF, {8,8,0,0,0,0}, _BRG, GL_RG,  GL_UNSIGNED_BYTE,  false, true,  true,  "GL_RG8" },
    { GL_RG8_SNORM, MTLPixelFormatRG8Snorm,       _INV, 1,1, 2, _CF, {8,8,0,0,0,0}, _BRG, GL_RG,  GL_BYTE,           false, true,  true,  "GL_RG8_SNORM" },
    { GL_RG8I,      MTLPixelFormatRG8Sint,        _INV, 1,1, 2, _CI, {8,8,0,0,0,0}, _BRG, GL_RG_INTEGER,  GL_BYTE,   false, true,  false, "GL_RG8I" },
    { GL_RG8UI,     MTLPixelFormatRG8Uint,        _INV, 1,1, 2, _CU, {8,8,0,0,0,0}, _BRG, GL_RG_INTEGER,  GL_UNSIGNED_BYTE, false, true, false, "GL_RG8UI" },
    { GL_SRG8_EXT,  MTLPixelFormatRG8Unorm_sRGB,  _INV, 1,1, 2, _CF, {8,8,0,0,0,0}, _BRG, GL_RG,  GL_UNSIGNED_BYTE,  true,  true,  true,  "GL_SRG8_EXT" },

    /* RG 16-bit */
    { GL_RG16,      MTLPixelFormatRG16Unorm,      _INV, 1,1, 4, _CF, {16,16,0,0,0,0}, _BRG, GL_RG,  GL_UNSIGNED_SHORT, false, true,  true,  "GL_RG16" },
    { GL_RG16_SNORM,MTLPixelFormatRG16Snorm,      _INV, 1,1, 4, _CF, {16,16,0,0,0,0}, _BRG, GL_RG,  GL_SHORT,          false, true,  true,  "GL_RG16_SNORM" },
    { GL_RG16F,     MTLPixelFormatRG16Float,      _INV, 1,1, 4, _CF, {16,16,0,0,0,0}, _BRG, GL_RG,  GL_HALF_FLOAT,      false, true,  true,  "GL_RG16F" },
    { GL_RG32F,     MTLPixelFormatRG32Float,      _INV, 1,1, 8, _CF, {32,32,0,0,0,0}, _BRG, GL_RG,  GL_FLOAT,           false, true,  true,  "GL_RG32F" },
    { GL_RG16I,     MTLPixelFormatRG16Sint,       _INV, 1,1, 4, _CI, {16,16,0,0,0,0}, _BRG, GL_RG_INTEGER,  GL_SHORT,   false, true,  false, "GL_RG16I" },
    { GL_RG16UI,    MTLPixelFormatRG16Uint,       _INV, 1,1, 4, _CU, {16,16,0,0,0,0}, _BRG, GL_RG_INTEGER,  GL_UNSIGNED_SHORT, false, true, false, "GL_RG16UI" },
    { GL_RG32I,     MTLPixelFormatRG32Sint,       _INV, 1,1, 8, _CI, {32,32,0,0,0,0}, _BRG, GL_RG_INTEGER,  GL_INT,     false, true,  false, "GL_RG32I" },
    { GL_RG32UI,    MTLPixelFormatRG32Uint,       _INV, 1,1, 8, _CU, {32,32,0,0,0,0}, _BRG, GL_RG_INTEGER,  GL_UNSIGNED_INT, false, true, false, "GL_RG32UI" },

    /* RGB (Metal lacks RGB-only; substitute RGBA) */
    { GL_R3_G3_B2,  _INV, MTLPixelFormatRGBA8Unorm,   1,1, 4, _CF, {3,3,2,0,0,0},  _BRGB, GL_RGB,  GL_UNSIGNED_BYTE_3_3_2, false, true,  true,  "GL_R3_G3_B2" },
    { GL_RGB4,      _INV, MTLPixelFormatRGBA8Unorm,   1,1, 4, _CF, {4,4,4,0,0,0},  _BRGB, GL_RGB,  GL_UNSIGNED_BYTE,      false, true,  true,  "GL_RGB4" },
    { GL_RGB5,      _INV, MTLPixelFormatRGBA8Unorm,   1,1, 4, _CF, {5,5,5,0,0,0},  _BRGB, GL_RGB,  GL_UNSIGNED_BYTE,      false, true,  true,  "GL_RGB5" },
    { GL_RGB8,      _INV, MTLPixelFormatRGBA8Unorm,   1,1, 4, _CF, {8,8,8,0,0,0},  _BRGB, GL_RGB,  GL_UNSIGNED_BYTE,      false, true,  true,  "GL_RGB8" },
    { GL_RGB8_SNORM,_INV, MTLPixelFormatRGBA8Snorm,   1,1, 4, _CF, {8,8,8,0,0,0},  _BRGB, GL_RGB,  GL_BYTE,               false, true,  true,  "GL_RGB8_SNORM" },
    { GL_SRGB8,     _INV, MTLPixelFormatRGBA8Unorm_sRGB, 1,1, 4, _CF, {8,8,8,0,0,0}, _BRGB, GL_RGB,  GL_UNSIGNED_BYTE,  true,  true,  true,  "GL_SRGB8" },
    { GL_RGB8I,     _INV, MTLPixelFormatRGBA8Sint,    1,1, 4, _CI, {8,8,8,0,0,0},  _BRGB, GL_RGB_INTEGER,  GL_BYTE,       false, true,  false, "GL_RGB8I" },
    { GL_RGB8UI,    _INV, MTLPixelFormatRGBA8Uint,    1,1, 4, _CU, {8,8,8,0,0,0},  _BRGB, GL_RGB_INTEGER,  GL_UNSIGNED_BYTE, false, true, false, "GL_RGB8UI" },
    { GL_RGB10,     _INV, MTLPixelFormatRGB10A2Unorm, 1,1, 4, _CF, {10,10,10,0,0,0}, _BRGB, GL_RGB, GL_UNSIGNED_SHORT,   false, true,  true,  "GL_RGB10" },
    { GL_RGB12,     _INV, MTLPixelFormatRGBA8Unorm,   1,1, 4, _CF, {12,12,12,0,0,0}, _BRGB, GL_RGB, GL_UNSIGNED_BYTE,   false, true,  true,  "GL_RGB12" },
    { GL_RGB16,     _INV, MTLPixelFormatRGBA16Unorm,  1,1, 8, _CF, {16,16,16,0,0,0}, _BRGB, GL_RGB, GL_UNSIGNED_SHORT,  false, true,  true,  "GL_RGB16" },
    { GL_RGB16_SNORM,_INV, MTLPixelFormatRGBA16Snorm, 1,1, 8, _CF, {16,16,16,0,0,0}, _BRGB, GL_RGB, GL_SHORT,           false, true,  true,  "GL_RGB16_SNORM" },
    { GL_RGB16F,    _INV, MTLPixelFormatRGBA16Float,  1,1, 8, _CF, {16,16,16,0,0,0}, _BRGB, GL_RGB,  GL_HALF_FLOAT,      false, true,  true,  "GL_RGB16F" },
    { GL_RGB32F,    _INV, MTLPixelFormatRGBA32Float,  1,1,16, _CF, {32,32,32,0,0,0}, _BRGB, GL_RGB,  GL_FLOAT,           false, true,  true,  "GL_RGB32F" },
    { GL_RGB16I,    _INV, MTLPixelFormatRGBA16Sint,   1,1, 8, _CI, {16,16,16,0,0,0}, _BRGB, GL_RGB_INTEGER,  GL_SHORT,   false, true,  false, "GL_RGB16I" },
    { GL_RGB16UI,   _INV, MTLPixelFormatRGBA16Uint,   1,1, 8, _CU, {16,16,16,0,0,0}, _BRGB, GL_RGB_INTEGER,  GL_UNSIGNED_SHORT, false, true, false, "GL_RGB16UI" },
    { GL_RGB32I,    _INV, MTLPixelFormatRGBA32Sint,   1,1,16, _CI, {32,32,32,0,0,0}, _BRGB, GL_RGB_INTEGER,  GL_INT,     false, true,  false, "GL_RGB32I" },
    { GL_RGB32UI,   _INV, MTLPixelFormatRGBA32Uint,   1,1,16, _CU, {32,32,32,0,0,0}, _BRGB, GL_RGB_INTEGER,  GL_UNSIGNED_INT, false, true, false, "GL_RGB32UI" },

    /* RGB special */
    { GL_R11F_G11F_B10F, MTLPixelFormatRG11B10Float, _INV, 1,1, 4, _CF, {11,11,10,0,0,0}, _BRGB, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, false, true, true, "GL_R11F_G11F_B10F" },
    { GL_RGB9_E5,        MTLPixelFormatRGB9E5Float,   _INV, 1,1, 4, _CF, {9,9,9,0,0,0},   _BRGB, GL_RGB, GL_UNSIGNED_INT_5_9_9_9_REV,    false, true, true, "GL_RGB9_E5" },
    { GL_RGB565,         MTLPixelFormatB5G6R5Unorm,   _INV, 1,1, 2, _CF, {5,6,5,0,0,0},   _BRGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,        false, true, true, "GL_RGB565" },

    /* RGBA 8-bit */
    { GL_RGBA2,     _INV, MTLPixelFormatRGBA8Unorm,   1,1, 4, _CF, {2,2,2,2,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE,      false, true,  true,  "GL_RGBA2" },
    { GL_RGBA4,     MTLPixelFormatABGR4Unorm,          _INV, 1,1, 2, _CF, {4,4,4,4,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, false, true, true, "GL_RGBA4" },
    { GL_RGB5_A1,   MTLPixelFormatA1BGR5Unorm,         _INV, 1,1, 2, _CF, {5,5,5,1,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, false, true, true, "GL_RGB5_A1" },
    { GL_RGBA8,     MTLPixelFormatRGBA8Unorm,          _INV, 1,1, 4, _CF, {8,8,8,8,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE,      false, true,  true,  "GL_RGBA8" },
    { GL_RGBA8_SNORM,MTLPixelFormatRGBA8Snorm,         _INV, 1,1, 4, _CF, {8,8,8,8,0,0}, _BRGBA, GL_RGBA, GL_BYTE,               false, true,  true,  "GL_RGBA8_SNORM" },
    { GL_SRGB8_ALPHA8, MTLPixelFormatRGBA8Unorm_sRGB,  _INV, 1,1, 4, _CF, {8,8,8,8,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE,      true,  true,  true,  "GL_SRGB8_ALPHA8" },
    { GL_RGBA8I,    MTLPixelFormatRGBA8Sint,           _INV, 1,1, 4, _CI, {8,8,8,8,0,0}, _BRGBA, GL_RGBA_INTEGER,  GL_BYTE,       false, true,  false, "GL_RGBA8I" },
    { GL_RGBA8UI,   MTLPixelFormatRGBA8Uint,           _INV, 1,1, 4, _CU, {8,8,8,8,0,0}, _BRGBA, GL_RGBA_INTEGER,  GL_UNSIGNED_BYTE, false, true, false, "GL_RGBA8UI" },

    /* RGBA 16-bit */
    { GL_RGBA12,    _INV, MTLPixelFormatRGBA16Unorm,   1,1, 8, _CF, {12,12,12,12,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_SHORT,  false, true,  true,  "GL_RGBA12" },
    { GL_RGBA16,    MTLPixelFormatRGBA16Unorm,          _INV, 1,1, 8, _CF, {16,16,16,16,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_SHORT, false, true, true, "GL_RGBA16" },
    { GL_RGBA16_SNORM,MTLPixelFormatRGBA16Snorm,        _INV, 1,1, 8, _CF, {16,16,16,16,0,0}, _BRGBA, GL_RGBA, GL_SHORT,      false, true,  true,  "GL_RGBA16_SNORM" },
    { GL_RGBA16F,   MTLPixelFormatRGBA16Float,          _INV, 1,1, 8, _CF, {16,16,16,16,0,0}, _BRGBA, GL_RGBA, GL_HALF_FLOAT,  false, true,  true,  "GL_RGBA16F" },
    { GL_RGBA32F,   MTLPixelFormatRGBA32Float,          _INV, 1,1,16, _CF, {32,32,32,32,0,0}, _BRGBA, GL_RGBA, GL_FLOAT,       false, true,  true,  "GL_RGBA32F" },
    { GL_RGBA16I,   MTLPixelFormatRGBA16Sint,           _INV, 1,1, 8, _CI, {16,16,16,16,0,0}, _BRGBA, GL_RGBA_INTEGER,  GL_SHORT, false, true, false, "GL_RGBA16I" },
    { GL_RGBA16UI,  MTLPixelFormatRGBA16Uint,           _INV, 1,1, 8, _CU, {16,16,16,16,0,0}, _BRGBA, GL_RGBA_INTEGER,  GL_UNSIGNED_SHORT, false, true, false, "GL_RGBA16UI" },
    { GL_RGBA32I,   MTLPixelFormatRGBA32Sint,           _INV, 1,1,16, _CI, {32,32,32,32,0,0}, _BRGBA, GL_RGBA_INTEGER,  GL_INT, false, true,  false, "GL_RGBA32I" },
    { GL_RGBA32UI,  MTLPixelFormatRGBA32Uint,           _INV, 1,1,16, _CU, {32,32,32,32,0,0}, _BRGBA, GL_RGBA_INTEGER,  GL_UNSIGNED_INT, false, true, false, "GL_RGBA32UI" },

    /* RGBA special */
    { GL_RGB10_A2,  MTLPixelFormatRGB10A2Unorm,         _INV, 1,1, 4, _CF, {10,10,10,2,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2, false, true, true, "GL_RGB10_A2" },
    { GL_RGB10_A2UI,MTLPixelFormatRGB10A2Uint,          _INV, 1,1, 4, _CU, {10,10,10,2,0,0}, _BRGBA, GL_RGBA_INTEGER, GL_UNSIGNED_INT_10_10_10_2, false, true, false, "GL_RGB10_A2UI" },

    /* ======== depth / stencil (table 8.13) ======== */
    { GL_DEPTH_COMPONENT16,  MTLPixelFormatDepth16Unorm,          MTLPixelFormatDepth32Float,          1,1, 2, _D,  {0,0,0,0,16,0}, _BD,  GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, false, true, true, "GL_DEPTH_COMPONENT16" },
    { GL_DEPTH_COMPONENT24,  MTLPixelFormatDepth32Float,         _INV,                                1,1, 4, _D,  {0,0,0,0,24,0}, _BD,  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,   false, true, true, "GL_DEPTH_COMPONENT24" },
    { GL_DEPTH_COMPONENT32,  MTLPixelFormatDepth32Float,         _INV,                                1,1, 4, _D,  {0,0,0,0,32,0}, _BD,  GL_DEPTH_COMPONENT, GL_FLOAT,         false, true, true, "GL_DEPTH_COMPONENT32" },
    { GL_DEPTH_COMPONENT32F, MTLPixelFormatDepth32Float,         _INV,                                1,1, 4, _D,  {0,0,0,0,32,0}, _BD,  GL_DEPTH_COMPONENT, GL_FLOAT,         false, true, true, "GL_DEPTH_COMPONENT32F" },
    { GL_DEPTH24_STENCIL8,   MTLPixelFormatDepth24Unorm_Stencil8, MTLPixelFormatDepth32Float_Stencil8, 1,1, 4, _DS, {0,0,0,0,24,8}, _BDS, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, false, true, true, "GL_DEPTH24_STENCIL8" },
    { GL_DEPTH32F_STENCIL8,  MTLPixelFormatDepth32Float_Stencil8, _INV,                               1,1, 5, _DS, {0,0,0,0,32,8}, _BDS, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, false, true, true, "GL_DEPTH32F_STENCIL8" },
    { GL_STENCIL_INDEX1,  _INV, _INV, 1,1, 1, _S,  {0,0,0,0,0,1},  _BS, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, false, true, false, "GL_STENCIL_INDEX1" },
    { GL_STENCIL_INDEX4,  _INV, _INV, 1,1, 1, _S,  {0,0,0,0,0,4},  _BS, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, false, true, false, "GL_STENCIL_INDEX4" },
    { GL_STENCIL_INDEX8,  MTLPixelFormatStencil8, _INV, 1,1, 1, _S, {0,0,0,0,0,8},  _BS, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, false, true, false, "GL_STENCIL_INDEX8" },
    { GL_STENCIL_INDEX16, _INV, _INV, 1,1, 2, _S,  {0,0,0,0,0,16}, _BS, GL_STENCIL_INDEX, GL_UNSIGNED_SHORT, false, true, false, "GL_STENCIL_INDEX16" },

    /* ======== compressed — RGTC ======== */
    { GL_COMPRESSED_RED_RGTC1,        MTLPixelFormatBC4_RUnorm,   _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BR,   GL_RED, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RED_RGTC1" },
    { GL_COMPRESSED_SIGNED_RED_RGTC1, MTLPixelFormatBC4_RSnorm,   _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BR,   GL_RED, GL_BYTE,          false, false, false, "GL_COMPRESSED_SIGNED_RED_RGTC1" },
    { GL_COMPRESSED_RG_RGTC2,         MTLPixelFormatBC5_RGUnorm,  _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRG,  GL_RG,  GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RG_RGTC2" },
    { GL_COMPRESSED_SIGNED_RG_RGTC2,  MTLPixelFormatBC5_RGSnorm,  _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRG,  GL_RG,  GL_BYTE,          false, false, false, "GL_COMPRESSED_SIGNED_RG_RGTC2" },

    /* ======== compressed — BPTC ======== */
    { GL_COMPRESSED_RGBA_BPTC_UNORM,         MTLPixelFormatBC7_RGBAUnorm,      _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_BPTC_UNORM" },
    { GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM,   MTLPixelFormatBC7_RGBAUnorm_sRGB, _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM" },
    { GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT,   MTLPixelFormatBC6H_RGBFloat,      _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGB,  GL_RGB,  GL_FLOAT,         false, false, false, "GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT" },
    { GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, MTLPixelFormatBC6H_RGBUfloat,     _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGB,  GL_RGB,  GL_FLOAT,         false, false, false, "GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT" },

    /* ======== compressed — ETC2 / EAC ======== */
    { GL_COMPRESSED_RGB8_ETC2,                       MTLPixelFormatETC2_RGB8,      _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGB,  GL_RGB,  GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGB8_ETC2" },
    { GL_COMPRESSED_SRGB8_ETC2,                      MTLPixelFormatETC2_RGB8_sRGB, _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGB,  GL_RGB,  GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ETC2" },
    { GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,   MTLPixelFormatETC2_RGB8A1,   _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2" },
    { GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,  MTLPixelFormatETC2_RGB8A1_sRGB, _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true, false, false, "GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2" },
    { GL_COMPRESSED_RGBA8_ETC2_EAC,                  MTLPixelFormatEAC_RGBA8,     _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA8_ETC2_EAC" },
    { GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,           MTLPixelFormatEAC_RGBA8_sRGB,_INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC" },
    { GL_COMPRESSED_R11_EAC,                         MTLPixelFormatEAC_R11Unorm,  _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BR,    GL_RED,  GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_R11_EAC" },
    { GL_COMPRESSED_SIGNED_R11_EAC,                  MTLPixelFormatEAC_R11Snorm,  _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BR,    GL_RED,  GL_BYTE,          false, false, false, "GL_COMPRESSED_SIGNED_R11_EAC" },
    { GL_COMPRESSED_RG11_EAC,                        MTLPixelFormatEAC_RG11Unorm, _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRG,   GL_RG,   GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RG11_EAC" },
    { GL_COMPRESSED_SIGNED_RG11_EAC,                 MTLPixelFormatEAC_RG11Snorm, _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRG,   GL_RG,   GL_BYTE,          false, false, false, "GL_COMPRESSED_SIGNED_RG11_EAC" },

    /* ======== compressed — S3TC/DXT (EXT) ======== */
    { GL_COMPRESSED_RGB_S3TC_DXT1_EXT,              MTLPixelFormatBC1_RGBA,       _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGB,  GL_RGB,  GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGB_S3TC_DXT1_EXT" },
    { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,             MTLPixelFormatBC1_RGBA,       _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_S3TC_DXT1_EXT" },
    { GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,             MTLPixelFormatBC2_RGBA,       _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_S3TC_DXT3_EXT" },
    { GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,             MTLPixelFormatBC3_RGBA,       _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT" },
    { GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,             MTLPixelFormatBC1_RGBA_sRGB,  _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGB,  GL_RGB,  GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB_S3TC_DXT1_EXT" },
    { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,       MTLPixelFormatBC1_RGBA_sRGB,  _INV, 4,4, 8, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT" },
    { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,       MTLPixelFormatBC2_RGBA_sRGB,  _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT" },
    { GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,       MTLPixelFormatBC3_RGBA_sRGB,  _INV, 4,4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT" },

    /* ======== compressed — ASTC LDR (KHR) 14 LDR + 14 sRGB = 28 ======== */
    { 0x93B0, MTLPixelFormatASTC_4x4_LDR,   _INV,  4, 4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_4x4_KHR" },
    { 0x93B1, MTLPixelFormatASTC_5x4_LDR,   _INV,  5, 4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_5x4_KHR" },
    { 0x93B2, MTLPixelFormatASTC_5x5_LDR,   _INV,  5, 5,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_5x5_KHR" },
    { 0x93B3, MTLPixelFormatASTC_6x5_LDR,   _INV,  6, 5,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_6x5_KHR" },
    { 0x93B4, MTLPixelFormatASTC_6x6_LDR,   _INV,  6, 6,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_6x6_KHR" },
    { 0x93B5, MTLPixelFormatASTC_8x5_LDR,   _INV,  8, 5,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_8x5_KHR" },
    { 0x93B6, MTLPixelFormatASTC_8x6_LDR,   _INV,  8, 6,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_8x6_KHR" },
    { 0x93B7, MTLPixelFormatASTC_8x8_LDR,   _INV,  8, 8,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_8x8_KHR" },
    { 0x93B8, MTLPixelFormatASTC_10x5_LDR,  _INV, 10, 5,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_10x5_KHR" },
    { 0x93B9, MTLPixelFormatASTC_10x6_LDR,  _INV, 10, 6,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_10x6_KHR" },
    { 0x93BA, MTLPixelFormatASTC_10x8_LDR,  _INV, 10, 8,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_10x8_KHR" },
    { 0x93BB, MTLPixelFormatASTC_10x10_LDR, _INV, 10,10,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_10x10_KHR" },
    { 0x93BC, MTLPixelFormatASTC_12x10_LDR, _INV, 12,10,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_12x10_KHR" },
    { 0x93BD, MTLPixelFormatASTC_12x12_LDR, _INV, 12,12,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, false, false, false, "GL_COMPRESSED_RGBA_ASTC_12x12_KHR" },
    { 0x93D0, MTLPixelFormatASTC_4x4_sRGB,  _INV,  4, 4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR" },
    { 0x93D1, MTLPixelFormatASTC_5x4_sRGB,  _INV,  5, 4,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR" },
    { 0x93D2, MTLPixelFormatASTC_5x5_sRGB,  _INV,  5, 5,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR" },
    { 0x93D3, MTLPixelFormatASTC_6x5_sRGB,  _INV,  6, 5,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR" },
    { 0x93D4, MTLPixelFormatASTC_6x6_sRGB,  _INV,  6, 6,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR" },
    { 0x93D5, MTLPixelFormatASTC_8x5_sRGB,  _INV,  8, 5,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR" },
    { 0x93D6, MTLPixelFormatASTC_8x6_sRGB,  _INV,  8, 6,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR" },
    { 0x93D7, MTLPixelFormatASTC_8x8_sRGB,  _INV,  8, 8,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR" },
    { 0x93D8, MTLPixelFormatASTC_10x5_sRGB, _INV, 10, 5,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR" },
    { 0x93D9, MTLPixelFormatASTC_10x6_sRGB, _INV, 10, 6,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR" },
    { 0x93DA, MTLPixelFormatASTC_10x8_sRGB, _INV, 10, 8,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR" },
    { 0x93DB, MTLPixelFormatASTC_10x10_sRGB,_INV, 10,10,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR" },
    { 0x93DC, MTLPixelFormatASTC_12x10_sRGB,_INV, 12,10,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR" },
    { 0x93DD, MTLPixelFormatASTC_12x12_sRGB,_INV, 12,12,16, _CX, {0,0,0,0,0,0}, _BRGBA, GL_RGBA, GL_UNSIGNED_BYTE, true,  false, false, "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR" },

    /* ======== legacy / EXT formats ======== */

    /* ALPHA / LUMINANCE */
    { GL_ALPHA,                    _INV, MTLPixelFormatR8Unorm,    1,1, 1, _CF, {0,0,0,8,0,0}, _BR,   GL_ALPHA,           GL_UNSIGNED_BYTE, false, true, true, "GL_ALPHA" },
    { GL_ALPHA8,                   _INV, MTLPixelFormatR8Unorm,    1,1, 1, _CF, {0,0,0,8,0,0}, _BR,   GL_RED,              GL_UNSIGNED_BYTE, false, true, true, "GL_ALPHA8" },
    { GL_ALPHA16,                  _INV, MTLPixelFormatR16Unorm,   1,1, 2, _CF, {0,0,0,16,0,0}, _BR,  GL_RED,              GL_UNSIGNED_SHORT, false, true, true, "GL_ALPHA16" },
    { GL_ALPHA16F_ARB,             _INV, MTLPixelFormatR16Float,   1,1, 2, _CF, {0,0,0,16,0,0}, _BR,  GL_RED,              GL_HALF_FLOAT,     false, true, true, "GL_ALPHA16F_ARB" },
    { GL_ALPHA32F_ARB,             _INV, MTLPixelFormatR32Float,   1,1, 4, _CF, {0,0,0,32,0,0}, _BR,  GL_RED,              GL_FLOAT,          false, true, true, "GL_ALPHA32F_ARB" },
    { GL_LUMINANCE,                _INV, MTLPixelFormatR8Unorm,    1,1, 1, _CF, {8,0,0,0,0,0}, _BR,   GL_LUMINANCE,        GL_UNSIGNED_BYTE, false, true, true, "GL_LUMINANCE" },
    { GL_LUMINANCE8,               _INV, MTLPixelFormatR8Unorm,    1,1, 1, _CF, {8,0,0,0,0,0}, _BR,   GL_RED,              GL_UNSIGNED_BYTE, false, true, true, "GL_LUMINANCE8" },
    { GL_LUMINANCE16,              _INV, MTLPixelFormatR16Unorm,   1,1, 2, _CF, {16,0,0,0,0,0}, _BR,  GL_RED,              GL_UNSIGNED_SHORT, false, true, true, "GL_LUMINANCE16" },
    { GL_LUMINANCE16F_ARB,         _INV, MTLPixelFormatR16Float,   1,1, 2, _CF, {16,0,0,0,0,0}, _BR,  GL_RED,              GL_HALF_FLOAT,     false, true, true, "GL_LUMINANCE16F_ARB" },
    { GL_LUMINANCE32F_ARB,         _INV, MTLPixelFormatR32Float,   1,1, 4, _CF, {32,0,0,0,0,0}, _BR,  GL_RED,              GL_FLOAT,          false, true, true, "GL_LUMINANCE32F_ARB" },
    { GL_LUMINANCE_ALPHA,          _INV, MTLPixelFormatRG8Unorm,   1,1, 2, _CF, {8,0,0,8,0,0}, _BRG,  GL_LUMINANCE_ALPHA,  GL_UNSIGNED_BYTE, false, true, true, "GL_LUMINANCE_ALPHA" },
    { GL_LUMINANCE8_ALPHA8,        _INV, MTLPixelFormatRG8Unorm,   1,1, 2, _CF, {8,0,0,8,0,0}, _BRG,  GL_RG,               GL_UNSIGNED_BYTE, false, true, true, "GL_LUMINANCE8_ALPHA8" },
    { GL_LUMINANCE_ALPHA16F_ARB,   _INV, MTLPixelFormatRG16Float,  1,1, 4, _CF, {16,0,0,16,0,0}, _BRG, GL_RG,              GL_HALF_FLOAT,     false, true, true, "GL_LUMINANCE_ALPHA16F_ARB" },
    { GL_LUMINANCE_ALPHA32F_ARB,   _INV, MTLPixelFormatRG32Float,  1,1, 8, _CF, {32,0,0,32,0,0}, _BRG, GL_RG,              GL_FLOAT,          false, true, true, "GL_LUMINANCE_ALPHA32F_ARB" },

    /* alternate integer enums */
    { GL_ALPHA8UI_EXT,             MTLPixelFormatR8Uint,          _INV, 1,1, 1, _CU, {0,0,0,8,0,0},  _BR, GL_RED_INTEGER, GL_UNSIGNED_BYTE, false, true, false, "GL_ALPHA8UI_EXT" },
    { GL_ALPHA8I_EXT,              MTLPixelFormatR8Sint,          _INV, 1,1, 1, _CI, {0,0,0,8,0,0},  _BR, GL_RED_INTEGER, GL_BYTE,          false, true, false, "GL_ALPHA8I_EXT" },
    { GL_ALPHA16UI_EXT,            MTLPixelFormatR16Uint,         _INV, 1,1, 2, _CU, {0,0,0,16,0,0}, _BR, GL_RED_INTEGER, GL_UNSIGNED_SHORT, false, true, false, "GL_ALPHA16UI_EXT" },
    { GL_ALPHA16I_EXT,             MTLPixelFormatR16Sint,         _INV, 1,1, 2, _CI, {0,0,0,16,0,0}, _BR, GL_RED_INTEGER, GL_SHORT,          false, true, false, "GL_ALPHA16I_EXT" },
    { GL_ALPHA32UI_EXT,            MTLPixelFormatR32Uint,         _INV, 1,1, 4, _CU, {0,0,0,32,0,0}, _BR, GL_RED_INTEGER, GL_UNSIGNED_INT,   false, true, false, "GL_ALPHA32UI_EXT" },
    { GL_ALPHA32I_EXT,             MTLPixelFormatR32Sint,         _INV, 1,1, 4, _CI, {0,0,0,32,0,0}, _BR, GL_RED_INTEGER, GL_INT,           false, true, false, "GL_ALPHA32I_EXT" },

    /* alternate integer enums for RGB (0x8Dxx) */
    { 0x8D75, _INV, MTLPixelFormatRGBA8Sint,  1,1, 4, _CI, {8,8,8,0,0,0},   _BRGB, GL_RGB_INTEGER, GL_BYTE,          false, true, false, "GL_RGB8I_ALT" },
    { 0x8D7A, _INV, MTLPixelFormatRGBA8Uint,  1,1, 4, _CU, {8,8,8,0,0,0},   _BRGB, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, false, true, false, "GL_RGB8UI_ALT" },
    { 0x8D80, _INV, MTLPixelFormatRGBA32Uint, 1,1,16, _CU, {32,32,32,0,0,0}, _BRGB, GL_RGB_INTEGER, GL_UNSIGNED_INT,  false, true, false, "GL_RGB32UI_ALT" },
    { 0x8D86, _INV, MTLPixelFormatRGBA16Sint, 1,1, 8, _CI, {16,16,16,0,0,0}, _BRGB, GL_RGB_INTEGER, GL_SHORT,         false, true, false, "GL_RGB16I_ALT" },
    { 0x8D8C, _INV, MTLPixelFormatRGBA32Sint, 1,1,16, _CI, {32,32,32,0,0,0}, _BRGB, GL_RGB_INTEGER, GL_INT,           false, true, false, "GL_RGB32I_ALT" },
    { 0x8D92, _INV, MTLPixelFormatRGBA16Uint, 1,1, 8, _CU, {16,16,16,0,0,0}, _BRGB, GL_RGB_INTEGER, GL_UNSIGNED_SHORT,false, true, false, "GL_RGB16UI_ALT" },

    /* alternate integer enums for RGBA (0x8Dxx) */
    { 0x8D78, MTLPixelFormatRGBA8Uint,  _INV, 1,1, 4, _CU, {8,8,8,8,0,0},    _BRGBA, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE,  false, true, false, "GL_RGBA8UI_ALT" },
    { 0x8D84, MTLPixelFormatRGBA16Sint, _INV, 1,1, 8, _CI, {16,16,16,16,0,0}, _BRGBA, GL_RGBA_INTEGER, GL_SHORT,          false, true, false, "GL_RGBA16I_ALT" },
    { 0x8D8A, MTLPixelFormatRGBA32Sint, _INV, 1,1,16, _CI, {32,32,32,32,0,0}, _BRGBA, GL_RGBA_INTEGER, GL_INT,            false, true, false, "GL_RGBA32I_ALT" },
    { 0x8D90, MTLPixelFormatRGBA16Uint, _INV, 1,1, 8, _CU, {16,16,16,16,0,0}, _BRGBA, GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, false, true, false, "GL_RGBA16UI_ALT" },

    /* SNORM alternate enums */
    { 0x9014, _INV, MTLPixelFormatR8Snorm,   1,1, 1, _CF, {0,0,0,8,0,0},  _BR,   GL_RED, GL_BYTE, false, true, true, "GL_ALPHA8_SNORM" },
    { 0x9016, _INV, MTLPixelFormatRG8Snorm,  1,1, 2, _CF, {8,0,0,8,0,0},  _BRG,  GL_RG,  GL_BYTE, false, true, true, "GL_LUMINANCE8_ALPHA8_SNORM" },
    { 0x9018, _INV, MTLPixelFormatR16Snorm,  1,1, 2, _CF, {0,0,0,16,0,0}, _BR,   GL_RED, GL_SHORT, false, true, true, "GL_ALPHA16_SNORM" },
    { 0x901A, _INV, MTLPixelFormatRG16Snorm, 1,1, 4, _CF, {16,0,0,16,0,0}, _BRG,  GL_RG,  GL_SHORT, false, true, true, "GL_LUMINANCE16_ALPHA16_SNORM" },
};
static const size_t gl_table_count = sizeof(gl_table) / sizeof(gl_table[0]);

/* ================================================================ */
/*  device state                                                     */
/* ================================================================ */

static MGLDeviceFormatCaps _dev_caps = { false, false, false, false, false, false };
static bool _dev_set = false;

void mglFormatTableSetDevice(const MGLDeviceFormatCaps *caps)
{
    if (caps) {
        _dev_caps = *caps;
        _dev_set = true;
    } else {
        memset(&_dev_caps, 0, sizeof(_dev_caps));
        _dev_set = false;
    }
}

/* ================================================================ */
/*  effective Metal caps with device adjustments                     */
/* ================================================================ */

static uint16_t effective_mtl_caps(uint16_t mtl_format)
{
    if (!_dev_set) {
        const MGLMetalFormatDesc *d = mglMetalFormatDesc(mtl_format);
        return d ? d->caps_apple : 0;
    }

    const MGLMetalFormatDesc *d = mglMetalFormatDesc(mtl_format);
    if (!d) return 0;

    uint16_t caps = _dev_caps.apple_gpu ? d->caps_apple : d->caps_mac;

    /* BC -> None when !supports_bc */
    if (!_dev_caps.supports_bc) {
        switch (mtl_format) {
            case MTLPixelFormatBC1_RGBA: case MTLPixelFormatBC1_RGBA_sRGB:
            case MTLPixelFormatBC2_RGBA: case MTLPixelFormatBC2_RGBA_sRGB:
            case MTLPixelFormatBC3_RGBA: case MTLPixelFormatBC3_RGBA_sRGB:
            case MTLPixelFormatBC4_RUnorm: case MTLPixelFormatBC4_RSnorm:
            case MTLPixelFormatBC5_RGUnorm: case MTLPixelFormatBC5_RGSnorm:
            case MTLPixelFormatBC6H_RGBUfloat: case MTLPixelFormatBC6H_RGBFloat:
            case MTLPixelFormatBC7_RGBAUnorm: case MTLPixelFormatBC7_RGBAUnorm_sRGB:
                caps = 0; break;
            default: break;
        }
    }

    /* Depth24Unorm_Stencil8 -> None when !supports_depth24_stencil8 */
    if (!_dev_caps.supports_depth24_stencil8 &&
        mtl_format == MTLPixelFormatDepth24Unorm_Stencil8)
        caps = 0;

    /* 32-bit MSAA */
    if (_dev_caps.supports_32bit_msaa) {
        switch (mtl_format) {
            case MTLPixelFormatR32Uint: case MTLPixelFormatR32Sint:
                caps |= MGL_FMT_CAP_MSAA; break;
            case MTLPixelFormatR32Float:
                caps |= MGL_FMT_CAP_RESOLVE; break;
            case MTLPixelFormatRG32Uint: case MTLPixelFormatRG32Sint:
                caps |= MGL_FMT_CAP_MSAA; break;
            case MTLPixelFormatRG32Float:
                caps |= MGL_FMT_CAP_RESOLVE; break;
            case MTLPixelFormatRGBA32Uint: case MTLPixelFormatRGBA32Sint:
                caps |= MGL_FMT_CAP_MSAA; break;
            case MTLPixelFormatRGBA32Float:
                caps |= MGL_FMT_CAP_RESOLVE; break;
            default: break;
        }
    }

    /* 32-bit float filtering */
    if (_dev_caps.supports_32bit_float_filtering) {
        if (mtl_format == MTLPixelFormatR32Float ||
            mtl_format == MTLPixelFormatRG32Float ||
            mtl_format == MTLPixelFormatRGBA32Float)
            caps |= MGL_FMT_CAP_FILTER;
        if (mtl_format == MTLPixelFormatRGBA32Float)
            caps |= MGL_FMT_CAP_BLEND;
    }

    /* ASTC HDR -> None when !supports_astc_hdr */
    if (!_dev_caps.supports_astc_hdr) {
        switch (mtl_format) {
            case MTLPixelFormatASTC_4x4_HDR: case MTLPixelFormatASTC_5x4_HDR:
            case MTLPixelFormatASTC_5x5_HDR: case MTLPixelFormatASTC_6x5_HDR:
            case MTLPixelFormatASTC_6x6_HDR: case MTLPixelFormatASTC_8x5_HDR:
            case MTLPixelFormatASTC_8x6_HDR: case MTLPixelFormatASTC_8x8_HDR:
            case MTLPixelFormatASTC_10x5_HDR: case MTLPixelFormatASTC_10x6_HDR:
            case MTLPixelFormatASTC_10x8_HDR: case MTLPixelFormatASTC_10x10_HDR:
            case MTLPixelFormatASTC_12x10_HDR: case MTLPixelFormatASTC_12x12_HDR:
                caps = 0; break;
            default: break;
        }
    }

    /* RGB9E5Float loses Blend on non-Apple GPU */
    if (!_dev_caps.apple_gpu && mtl_format == MTLPixelFormatRGB9E5Float)
        caps &= ~MGL_FMT_CAP_BLEND;

    return caps;
}

static bool mtl_format_usable(uint16_t mtl_format)
{
    return mtl_format != _INV && effective_mtl_caps(mtl_format) != 0;
}

/* ================================================================ */
/*  lookups                                                          */
/* ================================================================ */

const MGLFormatDesc *mglFormatDesc(GLenum gl_internal_format)
{
    for (size_t i = 0; i < gl_table_count; i++) {
        if (gl_table[i].gl_format == gl_internal_format)
            return &gl_table[i];
    }
    return &_unknown_gl;
}

const MGLMetalFormatDesc *mglMetalFormatDesc(uint16_t mtl_format)
{
    if (mtl_format == _INV) return &mtl_table[0];
    for (size_t i = 1; i < mtl_table_count; i++) {
        if (mtl_table[i].mtl_format == mtl_format)
            return &mtl_table[i];
    }
    return &_unknown_mtl;
}

const MGLFormatDesc *mglFormatDescForMetal(uint16_t mtl_format)
{
    for (size_t i = 0; i < gl_table_count; i++) {
        if (gl_table[i].mtl_format == mtl_format)
            return &gl_table[i];
    }
    for (size_t i = 0; i < gl_table_count; i++) {
        if (gl_table[i].mtl_substitute == mtl_format)
            return &gl_table[i];
    }
    return &_unknown_gl;
}

/* ================================================================ */
/*  everyday answers                                                 */
/* ================================================================ */

/* GL lets you ask for a plain GL_RGBA and leaves the exact bit depth to us,
 * so pick the obvious sized format for each of those. */
// GL 4.6 table 8.5: a packed type names the exact client formats it may be
// paired with, and section 8.4.4 forbids a float type with an integer format.
bool mglFormatTypeAgrees(GLenum format, GLenum type)
{
    bool integer = mglClientFormatIsInteger(format);

    if (integer && (type == GL_FLOAT || type == GL_HALF_FLOAT))
        return false;

    if (format == GL_DEPTH_STENCIL)
        return type == GL_UNSIGNED_INT_24_8 || type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV;

    switch (type)
    {
        case GL_UNSIGNED_INT_10F_11F_11F_REV:
        case GL_UNSIGNED_INT_5_9_9_9_REV:
            return format == GL_RGB;

        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            return format == GL_RGB || format == GL_RGB_INTEGER;

        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
            return format == GL_RGBA || format == GL_BGRA ||
                   format == GL_RGBA_INTEGER || format == GL_BGRA_INTEGER;

        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            return format == GL_RGBA || format == GL_BGRA ||
                   format == GL_RGBA_INTEGER || format == GL_BGRA_INTEGER;

        case GL_UNSIGNED_INT_24_8:
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
            return false;   // DEPTH_STENCIL only, handled above
    }

    return true;
}

GLenum mglFormatSizedForBase(GLenum gl_internal_format)
{
    switch (gl_internal_format) {
        case GL_RED:             return GL_R8;
        case GL_RG:              return GL_RG8;
        case GL_RGB:             return GL_RGB8;
        case GL_RGBA:            return GL_RGBA8;
        case GL_SRGB:            return GL_SRGB8;
        case GL_SRGB_ALPHA:      return GL_SRGB8_ALPHA8;
        case GL_COMPRESSED_RED:  return GL_R8;
        case GL_COMPRESSED_RG:   return GL_RG8;
        case GL_COMPRESSED_RGB:  return GL_RGB8;
        case GL_COMPRESSED_RGBA: return GL_RGBA8;
        case GL_COMPRESSED_SRGB: return GL_SRGB8;
        case GL_COMPRESSED_SRGB_ALPHA: return GL_SRGB8_ALPHA8;
        case GL_DEPTH_COMPONENT: return GL_DEPTH_COMPONENT24;
        case GL_DEPTH_STENCIL:   return GL_DEPTH24_STENCIL8;
        case GL_STENCIL_INDEX:   return GL_STENCIL_INDEX8;
        default:                 return gl_internal_format;
    }
}

uint8_t mglFormatKind(GLenum gl_internal_format)
{
    return mglFormatDesc(gl_internal_format)->kind;
}

uint16_t mglFormatMetalFormat(GLenum gl_internal_format)
{
    const MGLFormatDesc *d = mglFormatDesc(gl_internal_format);
    if (d->gl_format == 0) return _INV;
    if (mtl_format_usable(d->mtl_format))
        return d->mtl_format;
    if (mtl_format_usable(d->mtl_substitute))
        return d->mtl_substitute;

    GLenum sized = mglFormatSizedForBase(gl_internal_format);
    if (sized != gl_internal_format) {
        d = mglFormatDesc(sized);
        if (mtl_format_usable(d->mtl_format))
            return d->mtl_format;
        if (mtl_format_usable(d->mtl_substitute))
            return d->mtl_substitute;
    }

    return _INV;
}

uint16_t mglFormatCaps(GLenum gl_internal_format)
{
    uint16_t fmt = mglFormatMetalFormat(gl_internal_format);
    return fmt ? effective_mtl_caps(fmt) : 0;
}

bool mglFormatIsCompressed(GLenum gl_internal_format)
{
    const MGLFormatDesc *d = mglFormatDesc(gl_internal_format);
    return d->kind == _CX;
}

size_t mglFormatBytesPerRow(GLenum gl_internal_format, GLsizei width)
{
    const MGLFormatDesc *d = mglFormatDesc(gl_internal_format);
    if (d->gl_format == 0 || width <= 0) return 0;
    GLsizei blocks = (width + d->block_w - 1) / d->block_w;
    return (size_t)blocks * d->bytes_per_block;
}

size_t mglFormatImageSize(GLenum gl_internal_format, GLsizei w, GLsizei h, GLsizei d)
{
    if (w <= 0 || h <= 0 || d <= 0) return 0;
    const MGLFormatDesc *desc = mglFormatDesc(gl_internal_format);
    GLsizei bw = (w + desc->block_w - 1) / desc->block_w;
    GLsizei bh = (h + desc->block_h - 1) / desc->block_h;
    return (size_t)bw * bh * d * desc->bytes_per_block;
}

GLint mglFormatComponentBits(GLenum gl_internal_format, GLenum component)
{
    const MGLFormatDesc *d = mglFormatDesc(gl_internal_format);
    if (d->gl_format == 0) return 0;
    switch (component) {
        case GL_RED:              return d->bits[0];
        case GL_GREEN:            return d->bits[1];
        case GL_BLUE:             return d->bits[2];
        case GL_ALPHA:            return d->bits[3];
        case GL_DEPTH_COMPONENT:  return d->bits[4];
        case GL_STENCIL_INDEX:    return d->bits[5];
        default:                  return 0;
    }
}

// What GL calls the channel's type: a float, a signed or unsigned integer, or
// a fixed-point value that reads back between -1 and 1 or 0 and 1.
GLenum mglFormatComponentType(GLenum gl_internal_format)
{
    const MGLFormatDesc *d = mglFormatDesc(gl_internal_format);

    if (d->gl_format == 0)
        return GL_NONE;

    switch (d->kind) {
        case MGL_FMT_COLOR_INT:   return GL_INT;
        case MGL_FMT_COLOR_UINT:  return GL_UNSIGNED_INT;
        default: break;
    }

    switch (gl_internal_format) {
        case GL_R16F: case GL_RG16F: case GL_RGB16F: case GL_RGBA16F:
        case GL_R32F: case GL_RG32F: case GL_RGB32F: case GL_RGBA32F:
        case GL_R11F_G11F_B10F: case GL_RGB9_E5:
        case GL_DEPTH_COMPONENT32F: case GL_DEPTH32F_STENCIL8:
            return GL_FLOAT;

        case GL_R8_SNORM: case GL_RG8_SNORM: case GL_RGB8_SNORM: case GL_RGBA8_SNORM:
        case GL_R16_SNORM: case GL_RG16_SNORM: case GL_RGB16_SNORM: case GL_RGBA16_SNORM:
        case GL_COMPRESSED_SIGNED_RED_RGTC1: case GL_COMPRESSED_SIGNED_RG_RGTC2:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
            return GL_SIGNED_NORMALIZED;

        default:
            return GL_UNSIGNED_NORMALIZED;
    }
}

bool mglFormatIsSRGB(GLenum gl_internal_format)
{
    return mglFormatDesc(gl_internal_format)->srgb;
}

// The unsized names above are a request to pick a format, not a format, so
// they must not show up in GL_COMPRESSED_TEXTURE_FORMATS.
static bool is_generic_compressed(GLenum fmt)
{
    switch (fmt) {
        case GL_COMPRESSED_RED:
        case GL_COMPRESSED_RG:
        case GL_COMPRESSED_RGB:
        case GL_COMPRESSED_RGBA:
        case GL_COMPRESSED_SRGB:
        case GL_COMPRESSED_SRGB_ALPHA:
            return true;
        default:
            return false;
    }
}

GLsizei mglFormatCompressedFormatList(GLenum *out, GLsizei max)
{
    GLsizei n = 0;

    // a NULL out just counts
    for (size_t i = 0; i < gl_table_count; i++) {
        if (gl_table[i].kind != _CX) continue;
        if (is_generic_compressed(gl_table[i].gl_format)) continue;
        uint16_t fmt = mglFormatMetalFormat(gl_table[i].gl_format);
        if (fmt != _INV && mtl_format_usable(fmt)) {
            if (out && n < max)
                out[n] = gl_table[i].gl_format;
            n++;
        }
    }
    return n;
}

size_t mglFormatTableCount(void) { return gl_table_count; }
const MGLFormatDesc *mglFormatTableRow(size_t i)
{
    return i < gl_table_count ? &gl_table[i] : &_unknown_gl;
}
