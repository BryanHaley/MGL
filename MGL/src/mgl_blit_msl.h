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
 * mgl_blit_msl.h
 * MGL
 *
 * A textured triangle, for the framebuffer blits Metal's own copy cannot do:
 * it only copies between matching pixel formats, and the window is BGRA8 while
 * almost nothing an app renders into is.
 */

#ifndef mgl_blit_msl_h
#define mgl_blit_msl_h

static const char *const mgl_blit_msl_source =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct MGLBlitVary {\n"
"    float4 position [[position]];\n"
"    float2 uv;\n"
"};\n"
"\n"
"struct MGLBlitArgs {\n"
"    float2 src_origin;   // in source texels\n"
"    float2 src_size;\n"
"};\n"
"\n"
"vertex MGLBlitVary mglBlitVertex(uint vid [[vertex_id]],\n"
"                                 constant MGLBlitArgs &args [[buffer(0)]],\n"
"                                 texture2d<float> src [[texture(0)]])\n"
"{\n"
"    // one oversized triangle covering the target\n"
"    float2 p = float2((vid << 1) & 2, vid & 2);\n"
"\n"
"    MGLBlitVary out;\n"
"    out.position = float4(p * 2.0 - 1.0, 0.0, 1.0);\n"
"\n"
"    float2 texel = float2(src.get_width(), src.get_height());\n"
"    out.uv = (args.src_origin + p * args.src_size) / texel;\n"
"\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 mglBlitFragment(MGLBlitVary in [[stage_in]],\n"
"                                texture2d<float> src [[texture(0)]],\n"
"                                sampler samp [[sampler(0)]])\n"
"{\n"
"    return src.sample(samp, in.uv);\n"
"}\n"
"\n"
;

#endif /* mgl_blit_msl_h */
