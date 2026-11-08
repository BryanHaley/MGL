/*
 * Copyright (C) The MooGL Project
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
 * ffx_fsr2_metalfx.mm
 * MGL
 *
 * FidelityFX FSR 2's API, for OpenGL apps on MGL, with Apple's MetalFX
 * temporal scaler doing the upscaling instead of AMD's shaders. One library,
 * libffx_fsr2_api, holds the functions AMD splits between its core and GL ones.
 *
 * The jitter, quality-mode, format and RCAS code follows AMD's FSR 2.2
 * (MIT licence, Copyright (c) 2022-2023 Advanced Micro Devices, Inc.).
 */

#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#import <Foundation/Foundation.h>

#include <math.h>
#include <string.h>

#include "ffx_fsr2.h"
#include "gl/ffx_fsr2_gl.h"
#include "MGLContext.h"

// glad turns GL names into its own function pointers; this is MGL's export
extern "C" void mglDebugMessageInsert(GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *)
    __asm__("_glDebugMessageInsert");

namespace {

const uint32_t kMagic = 0x4d465832;   // "MFX2"

struct MetalFxContext {
    uint32_t magic;
    FfxFsr2ContextDescription desc;
    void *scaler;               // id<MTLFXTemporalScaler>
    void *scaled;               // scaler output when it cannot be the app's
    void *reactive;             // reactive and composition masks folded together
    MTLPixelFormat color, depth, motion, output, mask;
    uint32_t inW, inH;
    bool contentScale;
    bool started;
};

static_assert(sizeof(MetalFxContext) <= sizeof(FfxFsr2Context), "context does not fit");

MetalFxContext *priv(FfxFsr2Context *c)
{
    MetalFxContext *p = (MetalFxContext *)c;
    return p && p->magic == kMagic ? p : nullptr;
}

void release(void **obj)
{
    if (*obj) CFRelease(*obj);
    *obj = nullptr;
}

id<MTLTexture> glTexture(GLMContext ctx, const FfxResource &r)
{
    if (r.resource == nullptr) return nil;
    return (__bridge id<MTLTexture>)MGLgetMetalTexture(ctx, (GLuint)(uintptr_t)r.resource);
}

float halton(int32_t index, int32_t base)
{
    float f = 1.0f, result = 0.0f;

    for (int32_t i = index; i > 0;) {
        f /= (float)base;
        result = result + f * (float)(i % base);
        i = (uint32_t)(floorf((float)i / (float)base));
    }

    return result;
}

// AMD's ffxFsr2ContextCreate checks these are set, so they are, but nothing
// here calls them
FfxErrorCode stubCaps(FfxFsr2Interface *, FfxDeviceCapabilities *caps, FfxDevice)
{
    if (caps) memset(caps, 0, sizeof(*caps));
    return FFX_OK;
}
FfxErrorCode stubCreate(FfxFsr2Interface *, FfxDevice) { return FFX_OK; }
FfxErrorCode stubDestroy(FfxFsr2Interface *) { return FFX_OK; }
FfxErrorCode stubUnused() { return FFX_ERROR_BACKEND_API_ERROR; }

const char *kKernels = R"(
#include <metal_stdlib>
using namespace metal;

struct RcasArgs { float sharp; float preExposure; uint2 size; };

static float3 rcasLoad(texture2d<float, access::read> t, int2 p, uint2 size, float e, float pe)
{
    p = clamp(p, int2(0), int2(size) - 1);
    return clamp(t.read(uint2(p)).rgb / pe * e, 0.0, 65504.0);
}

kernel void rcas(texture2d<float, access::read> src [[texture(0)]],
                 texture2d<float, access::write> dst [[texture(1)]],
                 texture2d<float, access::read> exposure [[texture(2)]],
                 constant RcasArgs &a [[buffer(0)]],
                 uint2 id [[thread_position_in_grid]])
{
    if (any(id >= a.size)) return;

    float e = is_null_texture(exposure) ? 1.0 : exposure.read(uint2(0)).r;
    int2 p = int2(id);
    float3 b = rcasLoad(src, p + int2(0, -1), a.size, e, a.preExposure);
    float3 d = rcasLoad(src, p + int2(-1, 0), a.size, e, a.preExposure);
    float3 c = rcasLoad(src, p, a.size, e, a.preExposure);
    float3 f = rcasLoad(src, p + int2(1, 0), a.size, e, a.preExposure);
    float3 h = rcasLoad(src, p + int2(0, 1), a.size, e, a.preExposure);

    float bL = b.b * 0.5 + (b.r * 0.5 + b.g);
    float dL = d.b * 0.5 + (d.r * 0.5 + d.g);
    float eL = c.b * 0.5 + (c.r * 0.5 + c.g);
    float fL = f.b * 0.5 + (f.r * 0.5 + f.g);
    float hL = h.b * 0.5 + (h.r * 0.5 + h.g);

    float nz = 0.25 * bL + 0.25 * dL + 0.25 * fL + 0.25 * hL - eL;
    float range = max3(max3(bL, dL, eL), fL, hL) - min3(min3(bL, dL, eL), fL, hL);
    nz = saturate(abs(nz) / max(range, 1e-20));
    nz = -0.5 * nz + 1.0;

    float3 mn4 = min(min3(b, d, f), h);
    float3 mx4 = max(max3(b, d, f), h);
    float3 hitMin = mn4 / max(4.0 * mx4, 1e-20);
    float3 hitMax = (1.0 - mx4) / (4.0 * mn4 - 4.0);
    float3 lobe3 = max(-hitMin, hitMax);
    float lobe = max(-(0.25 - 1.0 / 16.0), min(max3(lobe3.r, lobe3.g, lobe3.b), 0.0)) * a.sharp;
    lobe *= nz;

    float3 o = (lobe * (b + d + h + f) + c) / (4.0 * lobe + 1.0);
    dst.write(float4(o / e * a.preExposure, 1.0), id);
}

struct ReactiveArgs { float scale; float threshold; float binaryValue; uint flags; uint2 size; };

static float3 tonemap(float3 c) { return c / (max(max(0.0, c.r), max(c.g, c.b)) + 1.0); }
static float3 untonemap(float3 c) { return c / max(1.0 / 65504.0, 1.0 - max(c.r, max(c.g, c.b))); }

kernel void reactive(texture2d<float, access::read> opaque [[texture(0)]],
                     texture2d<float, access::read> color [[texture(1)]],
                     texture2d<float, access::write> dst [[texture(2)]],
                     constant ReactiveArgs &a [[buffer(0)]],
                     uint2 id [[thread_position_in_grid]])
{
    if (any(id >= a.size)) return;

    float3 pre = opaque.read(id).rgb;
    float3 post = color.read(id).rgb;

    if (a.flags & 1) { pre = tonemap(pre); post = tonemap(post); }
    if (a.flags & 2) { pre = untonemap(pre); post = untonemap(post); }

    float3 delta = abs(post - pre);
    float v = (a.flags & 8) ? max(delta.x, max(delta.y, delta.z)) : length(delta);
    v *= a.scale;

    if (a.flags & 4) v = v < a.threshold ? 0.0 : a.binaryValue;

    dst.write(float4(v), id);
}

kernel void fold(texture2d<float, access::read> reactive [[texture(0)]],
                 texture2d<float, access::read> composition [[texture(1)]],
                 texture2d<float, access::write> dst [[texture(2)]],
                 constant uint2 &size [[buffer(0)]],
                 uint2 id [[thread_position_in_grid]])
{
    if (any(id >= size)) return;

    float r = is_null_texture(reactive) ? 0.0 : reactive.read(id).r;
    dst.write(float4(saturate(max(r, composition.read(id).r))), id);
}
)";

enum { KERNEL_RCAS, KERNEL_REACTIVE, KERNEL_FOLD, KERNEL_COUNT };

id<MTLComputePipelineState> kernel(id<MTLDevice> device, int which)
{
    static id<MTLComputePipelineState> pipes[KERNEL_COUNT];
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        NSError *err = nil;
        id<MTLLibrary> lib = [device newLibraryWithSource:@(kKernels) options:nil error:&err];
        const char *names[KERNEL_COUNT] = { "rcas", "reactive", "fold" };

        if (!lib) {
            NSLog(@"FSR 2 on MetalFX: kernels did not compile: %@", err);
            return;
        }

        for (int i = 0; i < KERNEL_COUNT; i++)
            pipes[i] = [device newComputePipelineStateWithFunction:[lib newFunctionWithName:@(names[i])] error:&err];
    });

    return pipes[which];
}

void dispatch2D(id<MTLComputeCommandEncoder> enc, id<MTLComputePipelineState> pipe, uint32_t w, uint32_t h)
{
    [enc setComputePipelineState:pipe];
    [enc dispatchThreads:MTLSizeMake(w, h, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
}

id<MTLTexture> scratchTexture(void **slot, id<MTLDevice> device, MTLPixelFormat format,
                              NSUInteger w, NSUInteger h, MTLTextureUsage usage)
{
    id<MTLTexture> t = (__bridge id<MTLTexture>)*slot;

    if (t && t.pixelFormat == format && t.width == w && t.height == h && (t.usage & usage) == usage)
        return t;

    release(slot);

    MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
    d.usage = usage | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    d.storageMode = MTLStorageModePrivate;
    t = [device newTextureWithDescriptor:d];
    *slot = t ? (void *)CFBridgingRetain(t) : nullptr;

    return t;
}

bool usable(id<MTLTexture> t, MTLTextureUsage need)
{
    return (t.usage & need) == need;
}

} // namespace

extern "C" {

FfxErrorCode ffxFsr2ContextCreate(FfxFsr2Context *context, const FfxFsr2ContextDescription *contextDescription)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    memset(context, 0, sizeof(FfxFsr2Context));
    FFX_RETURN_ON_ERROR(contextDescription, FFX_ERROR_INVALID_POINTER);

    FFX_RETURN_ON_ERROR(contextDescription->callbacks.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->callbacks.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->callbacks.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    if (contextDescription->callbacks.scratchBuffer)
        FFX_RETURN_ON_ERROR(contextDescription->callbacks.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);

    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();

        if (!device || ![MTLFXTemporalScalerDescriptor supportsDevice:device])
            return FFX_ERROR_BACKEND_API_ERROR;
    }

    MetalFxContext *p = (MetalFxContext *)context;
    p->magic = kMagic;
    p->desc = *contextDescription;

    const char *note = "FSR 2 API: upscaling runs on Apple's MetalFX temporal scaler, not AMD's FSR 2 shaders";
    mglDebugMessageInsert(GL_DEBUG_SOURCE_THIRD_PARTY, GL_DEBUG_TYPE_OTHER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, note);

    return FFX_OK;
}

FfxErrorCode ffxFsr2ContextDispatch(FfxFsr2Context *context, const FfxFsr2DispatchDescription *params)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(params, FFX_ERROR_INVALID_POINTER);

    MetalFxContext *p = priv(context);
    FFX_RETURN_ON_ERROR(p, FFX_ERROR_INVALID_POINTER);

    const uint32_t rw = params->renderSize.width, rh = params->renderSize.height;
    FFX_RETURN_ON_ERROR(rw <= p->desc.maxRenderSize.width, FFX_ERROR_OUT_OF_RANGE);
    FFX_RETURN_ON_ERROR(rh <= p->desc.maxRenderSize.height, FFX_ERROR_OUT_OF_RANGE);
    FFX_RETURN_ON_ERROR(rw && rh, FFX_ERROR_INVALID_ARGUMENT);

    GLMContext gl = MGLgetCurrentContext();
    FFX_RETURN_ON_ERROR(gl, FFX_ERROR_NULL_DEVICE);

    @autoreleasepool {
        id<MTLTexture> color = glTexture(gl, params->color);
        id<MTLTexture> depth = glTexture(gl, params->depth);
        id<MTLTexture> motion = glTexture(gl, params->motionVectors);
        id<MTLTexture> output = glTexture(gl, params->output);
        id<MTLTexture> exposure = glTexture(gl, params->exposure);
        id<MTLTexture> reactive = glTexture(gl, params->reactive);
        id<MTLTexture> composition = glTexture(gl, params->transparencyAndComposition);

        FFX_RETURN_ON_ERROR(color && depth && motion && output, FFX_ERROR_INVALID_ARGUMENT);

        id<MTLDevice> device = color.device;
        id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)MGLgetMetalCommandBuffer(gl);
        FFX_RETURN_ON_ERROR(cb, FFX_ERROR_BACKEND_API_ERROR);

        const uint32_t flags = p->desc.flags;
        const uint32_t mw = p->desc.maxRenderSize.width, mh = p->desc.maxRenderSize.height;
        const uint32_t dw = p->desc.displaySize.width, dh = p->desc.displaySize.height;

        if (composition) {
            reactive = scratchTexture(&p->reactive, device, MTLPixelFormatR16Float, mw, mh, MTLTextureUsageShaderWrite);
            FFX_RETURN_ON_ERROR(reactive, FFX_ERROR_OUT_OF_MEMORY);

            uint32_t size[2] = { rw, rh };
            id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
            [enc setTexture:glTexture(gl, params->reactive) atIndex:0];
            [enc setTexture:composition atIndex:1];
            [enc setTexture:reactive atIndex:2];
            [enc setBytes:size length:sizeof(size) atIndex:0];
            dispatch2D(enc, kernel(device, KERNEL_FOLD), rw, rh);
            [enc endEncoding];
        }

        // before macOS 14.4 MetalFX has no reactive mask, and it is left out
        if (@available(macOS 14.4, *)) {
        } else {
            reactive = nil;
        }

        MTLPixelFormat mask = reactive ? reactive.pixelFormat : MTLPixelFormatInvalid;
        bool contentScale = rw != mw || rh != mh || (flags & FFX_FSR2_ENABLE_DYNAMIC_RESOLUTION);

        id<MTLFXTemporalScaler> scaler = (__bridge id<MTLFXTemporalScaler>)p->scaler;

        if (!scaler || p->color != color.pixelFormat || p->depth != depth.pixelFormat ||
            p->motion != motion.pixelFormat || p->output != output.pixelFormat || p->mask != mask ||
            p->contentScale != contentScale)
        {
            release(&p->scaler);

            MTLFXTemporalScalerDescriptor *d = [MTLFXTemporalScalerDescriptor new];
            d.colorTextureFormat = color.pixelFormat;
            d.depthTextureFormat = depth.pixelFormat;
            d.motionTextureFormat = motion.pixelFormat;
            d.outputTextureFormat = output.pixelFormat;
            d.inputWidth = mw;
            d.inputHeight = mh;
            d.outputWidth = dw;
            d.outputHeight = dh;
            d.autoExposureEnabled = (flags & FFX_FSR2_ENABLE_AUTO_EXPOSURE) != 0;

            if (contentScale) {
                d.inputContentPropertiesEnabled = YES;

                if (@available(macOS 14.0, *)) {
                    d.inputContentMinScale = [MTLFXTemporalScalerDescriptor supportedInputContentMinScaleForDevice:device];
                    d.inputContentMaxScale = [MTLFXTemporalScalerDescriptor supportedInputContentMaxScaleForDevice:device];
                } else {
                    d.inputContentMinScale = 1.0f;
                    d.inputContentMaxScale = 3.0f;
                }
            }

            if (reactive) {
                if (@available(macOS 14.4, *)) {
                    d.reactiveMaskTextureEnabled = YES;
                    d.reactiveMaskTextureFormat = mask;
                }
            }

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 270000
            if (@available(macOS 27.0, *)) {
                d.jitteredMotionVectorsEnabled = (flags & FFX_FSR2_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION) != 0;
                d.outputResolutionMotionVectorsEnabled = (flags & FFX_FSR2_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS) != 0;
            }
#endif

            scaler = [d newTemporalScalerWithDevice:device];

            if (!scaler) {
                NSLog(@"FSR 2 on MetalFX: no temporal scaler for colour %lu, depth %lu, motion %lu, output %lu",
                      (unsigned long)color.pixelFormat, (unsigned long)depth.pixelFormat,
                      (unsigned long)motion.pixelFormat, (unsigned long)output.pixelFormat);
                return FFX_ERROR_BACKEND_API_ERROR;
            }

            p->scaler = (void *)CFBridgingRetain(scaler);
            p->color = color.pixelFormat;
            p->depth = depth.pixelFormat;
            p->motion = motion.pixelFormat;
            p->output = output.pixelFormat;
            p->mask = mask;
            p->contentScale = contentScale;
            p->started = false;
        }

        FFX_RETURN_ON_ERROR(usable(color, scaler.colorTextureUsage) && usable(depth, scaler.depthTextureUsage) &&
                            usable(motion, scaler.motionTextureUsage), FFX_ERROR_INVALID_ARGUMENT);

        // the scaler writes its own texture when sharpening follows, or when
        // the app's cannot take its writes
        bool sharpen = params->enableSharpening;
        id<MTLTexture> target = output;

        if (sharpen || !usable(output, scaler.outputTextureUsage) || output.storageMode != MTLStorageModePrivate) {
            target = scratchTexture(&p->scaled, device, output.pixelFormat, dw, dh, scaler.outputTextureUsage);
            FFX_RETURN_ON_ERROR(target, FFX_ERROR_OUT_OF_MEMORY);
        }

        scaler.colorTexture = color;
        scaler.depthTexture = depth;
        scaler.motionTexture = motion;
        scaler.outputTexture = target;
        scaler.reactiveMaskTexture = reactive;
        scaler.exposureTexture = (flags & FFX_FSR2_ENABLE_AUTO_EXPOSURE) ? nil : exposure;
        scaler.preExposure = params->preExposure > 0.0f ? params->preExposure : 1.0f;
        scaler.jitterOffsetX = params->jitterOffset.x;
        scaler.jitterOffsetY = params->jitterOffset.y;
        scaler.motionVectorScaleX = params->motionVectorScale.x;
        scaler.motionVectorScaleY = params->motionVectorScale.y;
        scaler.depthReversed = (flags & FFX_FSR2_ENABLE_DEPTH_INVERTED) != 0;
        scaler.reset = params->reset || !p->started;

        if (contentScale) {
            scaler.inputContentWidth = rw;
            scaler.inputContentHeight = rh;
        }

        [scaler encodeToCommandBuffer:cb];
        p->started = true;

        if (sharpen) {
            struct { float sharp, preExposure; uint32_t size[2]; } args = {
                exp2f(-((-2.0f * params->sharpness) + 2.0f)), scaler.preExposure, { dw, dh }
            };

            id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
            [enc setTexture:target atIndex:0];
            [enc setTexture:output atIndex:1];
            [enc setTexture:scaler.exposureTexture atIndex:2];
            [enc setBytes:&args length:sizeof(args) atIndex:0];
            dispatch2D(enc, kernel(device, KERNEL_RCAS), dw, dh);
            [enc endEncoding];
        } else if (target != output) {
            id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
            [blit copyFromTexture:target sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
                       sourceSize:MTLSizeMake(dw, dh, 1) toTexture:output destinationSlice:0
                 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
            [blit endEncoding];
        }
    }

    return FFX_OK;
}

FfxErrorCode ffxFsr2ContextGenerateReactiveMask(FfxFsr2Context *context, const FfxFsr2GenerateReactiveDescription *params)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(params, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(params->commandList, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(priv(context), FFX_ERROR_INVALID_POINTER);

    GLMContext gl = MGLgetCurrentContext();
    FFX_RETURN_ON_ERROR(gl, FFX_ERROR_NULL_DEVICE);

    @autoreleasepool {
        id<MTLTexture> opaque = glTexture(gl, params->colorOpaqueOnly);
        id<MTLTexture> color = glTexture(gl, params->colorPreUpscale);
        id<MTLTexture> out = glTexture(gl, params->outReactive);
        FFX_RETURN_ON_ERROR(opaque && color && out, FFX_ERROR_INVALID_ARGUMENT);

        id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)MGLgetMetalCommandBuffer(gl);
        FFX_RETURN_ON_ERROR(cb, FFX_ERROR_BACKEND_API_ERROR);

        struct { float scale, threshold, binaryValue; uint32_t flags; uint32_t size[2]; } args = {
            params->scale, params->cutoffThreshold, params->binaryValue, params->flags,
            { params->renderSize.width, params->renderSize.height }
        };

        id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
        [enc setTexture:opaque atIndex:0];
        [enc setTexture:color atIndex:1];
        [enc setTexture:out atIndex:2];
        [enc setBytes:&args length:sizeof(args) atIndex:0];
        dispatch2D(enc, kernel(color.device, KERNEL_REACTIVE), args.size[0], args.size[1]);
        [enc endEncoding];
    }

    return FFX_OK;
}

FfxErrorCode ffxFsr2ContextDestroy(FfxFsr2Context *context)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    MetalFxContext *p = priv(context);

    if (p) {
        release(&p->scaler);
        release(&p->scaled);
        release(&p->reactive);
        p->magic = 0;
    }

    return FFX_OK;
}

float ffxFsr2GetUpscaleRatioFromQualityMode(FfxFsr2QualityMode qualityMode)
{
    switch (qualityMode) {
    case FFX_FSR2_QUALITY_MODE_QUALITY:           return 1.5f;
    case FFX_FSR2_QUALITY_MODE_BALANCED:          return 1.7f;
    case FFX_FSR2_QUALITY_MODE_PERFORMANCE:       return 2.0f;
    case FFX_FSR2_QUALITY_MODE_ULTRA_PERFORMANCE: return 3.0f;
    default:                                      return 0.0f;
    }
}

FfxErrorCode ffxFsr2GetRenderResolutionFromQualityMode(uint32_t *renderWidth, uint32_t *renderHeight,
                                                       uint32_t displayWidth, uint32_t displayHeight,
                                                       FfxFsr2QualityMode qualityMode)
{
    FFX_RETURN_ON_ERROR(renderWidth, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(renderHeight, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(FFX_FSR2_QUALITY_MODE_QUALITY <= qualityMode &&
                        qualityMode <= FFX_FSR2_QUALITY_MODE_ULTRA_PERFORMANCE, FFX_ERROR_INVALID_ENUM);

    const float ratio = ffxFsr2GetUpscaleRatioFromQualityMode(qualityMode);
    *renderWidth = (uint32_t)((float)displayWidth / ratio);
    *renderHeight = (uint32_t)((float)displayHeight / ratio);

    return FFX_OK;
}

int32_t ffxFsr2GetJitterPhaseCount(int32_t renderWidth, int32_t displayWidth)
{
    const float basePhaseCount = 8.0f;
    return (int32_t)(basePhaseCount * powf((float)displayWidth / renderWidth, 2.0f));
}

FfxErrorCode ffxFsr2GetJitterOffset(float *outX, float *outY, int32_t index, int32_t phaseCount)
{
    FFX_RETURN_ON_ERROR(outX, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(outY, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(phaseCount > 0, FFX_ERROR_INVALID_ARGUMENT);

    *outX = halton((index % phaseCount) + 1, 2) - 0.5f;
    *outY = halton((index % phaseCount) + 1, 3) - 0.5f;

    return FFX_OK;
}

bool ffxFsr2ResourceIsNull(FfxResource resource)
{
    return resource.resource == NULL;
}

size_t ffxFsr2GetScratchMemorySizeGL()
{
    return 64;
}

FfxErrorCode ffxFsr2GetInterfaceGL(FfxFsr2Interface *outInterface, void *scratchBuffer, size_t scratchBufferSize,
                                   ffx_glGetProcAddress getProcAddress)
{
    (void)getProcAddress;

    FFX_RETURN_ON_ERROR(outInterface, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(scratchBuffer, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(scratchBufferSize >= ffxFsr2GetScratchMemorySizeGL(), FFX_ERROR_INSUFFICIENT_MEMORY);

    outInterface->fpGetDeviceCapabilities = stubCaps;
    outInterface->fpCreateBackendContext = stubCreate;
    outInterface->fpDestroyBackendContext = stubDestroy;
    outInterface->fpCreateResource = (FfxFsr2CreateResourceFunc)stubUnused;
    outInterface->fpRegisterResource = (FfxFsr2RegisterResourceFunc)stubUnused;
    outInterface->fpUnregisterResources = (FfxFsr2UnregisterResourcesFunc)stubUnused;
    outInterface->fpGetResourceDescription = nullptr;
    outInterface->fpDestroyResource = (FfxFsr2DestroyResourceFunc)stubUnused;
    outInterface->fpCreatePipeline = (FfxFsr2CreatePipelineFunc)stubUnused;
    outInterface->fpDestroyPipeline = (FfxFsr2DestroyPipelineFunc)stubUnused;
    outInterface->fpScheduleGpuJob = (FfxFsr2ScheduleGpuJobFunc)stubUnused;
    outInterface->fpExecuteGpuJobs = (FfxFsr2ExecuteGpuJobsFunc)stubUnused;
    outInterface->scratchBuffer = scratchBuffer;
    outInterface->scratchBufferSize = scratchBufferSize;

    return FFX_OK;
}

static FfxSurfaceFormat surfaceFormat(GLenum fmt)
{
    switch (fmt) {
    case GL_RGBA32F:        return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
    case GL_RGBA16F:        return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    case GL_RGBA16:         return FFX_SURFACE_FORMAT_R16G16B16A16_UNORM;
    case GL_RG32F:          return FFX_SURFACE_FORMAT_R32G32_FLOAT;
    case GL_R32UI:          return FFX_SURFACE_FORMAT_R32_UINT;
    case GL_RGBA8:          return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
    case GL_R11F_G11F_B10F: return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
    case GL_RG16F:          return FFX_SURFACE_FORMAT_R16G16_FLOAT;
    case GL_RG16UI:         return FFX_SURFACE_FORMAT_R16G16_UINT;
    case GL_R16F:           return FFX_SURFACE_FORMAT_R16_FLOAT;
    case GL_R16UI:          return FFX_SURFACE_FORMAT_R16_UINT;
    case GL_R16:            return FFX_SURFACE_FORMAT_R16_UNORM;
    case GL_R16_SNORM:      return FFX_SURFACE_FORMAT_R16_SNORM;
    case GL_R8:             return FFX_SURFACE_FORMAT_R8_UNORM;
    case GL_R32F:           return FFX_SURFACE_FORMAT_R32_FLOAT;
    case GL_R8UI:           return FFX_SURFACE_FORMAT_R8_UINT;
    default:                return FFX_SURFACE_FORMAT_UNKNOWN;
    }
}

FfxResource ffxGetTextureResourceGL(GLuint textureGL, uint32_t width, uint32_t height, GLenum imgFormat, const wchar_t *name)
{
    (void)name;

    FfxResource resource = {};
    resource.resource = (void *)(uintptr_t)textureGL;
    resource.description.flags = FFX_RESOURCE_FLAGS_NONE;
    resource.description.type = FFX_RESOURCE_TYPE_TEXTURE2D;
    resource.description.width = width;
    resource.description.height = height;
    resource.description.depth = 1;
    resource.description.mipCount = 1;
    resource.description.format = surfaceFormat(imgFormat);

    switch (imgFormat) {
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
        resource.isDepth = true;
        break;
    default:
        resource.isDepth = false;
        break;
    }

    return resource;
}

FfxResource ffxGetBufferResourceGL(GLuint bufferGL, uint32_t size, const wchar_t *name)
{
    (void)name;

    FfxResource resource = {};
    resource.resource = (void *)(uintptr_t)bufferGL;
    resource.description.flags = FFX_RESOURCE_FLAGS_NONE;
    resource.description.type = FFX_RESOURCE_TYPE_BUFFER;
    resource.description.width = size;
    resource.description.height = 1;
    resource.description.depth = 1;
    resource.description.mipCount = 1;
    resource.description.format = FFX_SURFACE_FORMAT_UNKNOWN;
    resource.isDepth = false;

    return resource;
}

// MetalFX keeps its history to itself, so no internal resource has a GL name
GLuint ffxGetGLImage(FfxFsr2Context *context, uint32_t resId)
{
    (void)context;
    (void)resId;
    return 0;
}

} // extern "C"
