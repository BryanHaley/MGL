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
 * MGLKernels.m
 * MGL
 */

#import "MGLKernels.h"
#import "mgl_log.h"
#import "mgl_kernels_msl.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

@implementation MGLKernelLibrary
{
    id<MTLDevice>       _device;
    id<MTLLibrary>      _library;
    NSMutableDictionary *_pipelines;
}

- (instancetype) initWithDevice: (id<MTLDevice>) device
{
    self = [super init];

    if (self == nil)
        return nil;

    _device = device;
    _pipelines = [NSMutableDictionary dictionary];

    MTLCompileOptions *opts = [MTLCompileOptions new];

    if (@available(macOS 15.0, *))
        opts.mathMode = MTLMathModeSafe;
    else
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        opts.fastMathEnabled = NO;
#pragma clang diagnostic pop
    }

    NSError *err = nil;
    NSString *src = [NSString stringWithUTF8String: mgl_kernels_msl_source];

    _library = [_device newLibraryWithSource: src options: opts error: &err];

    if (_library == nil)
    {
        MGL_NSERR(@"MGLKernels: MSL compile failed: %@", err);
        return nil;
    }

    return self;
}

- (id<MTLComputePipelineState>) pipelineNamed: (const char *) name
{
    NSString *key = [NSString stringWithUTF8String: name];
    id<MTLComputePipelineState> pso = _pipelines[key];

    if (pso)
        return pso;

    id<MTLFunction> fn = [_library newFunctionWithName: key];

    if (fn == nil)
    {
        MGL_NSERR(@"MGLKernels: function '%s' not found in library", name);
        return nil;
    }

    NSError *err = nil;

    pso = [_device newComputePipelineStateWithFunction: fn error: &err];

    if (pso == nil)
    {
        MGL_NSERR(@"MGLKernels: PSO for '%s' failed: %@", name, err);
        return nil;
    }

    _pipelines[key] = pso;

    return pso;
}

static inline NSUInteger clampThreads(NSUInteger count, NSUInteger max)
{
    return count < max ? count : max;
}

- (bool) encodeConvertUint8Indices: (id<MTLComputeCommandEncoder>) enc
                            source: (id<MTLBuffer>) src
                      sourceOffset: (NSUInteger) srcOffset
                       destination: (id<MTLBuffer>) dst
                             count: (NSUInteger) count
                           keepFF: (bool) mapSentinel
{
    const char *name = mapSentinel ? "convertUint8Indices" : "convertUint8IndicesRaw";
    id<MTLComputePipelineState> pso = [self pipelineNamed: name];

    if (pso == nil)
        return false;

    [enc setComputePipelineState: pso];
    [enc setBuffer: src offset: srcOffset atIndex: 0];
    [enc setBuffer: dst offset: 0 atIndex: 1];

    MTLSize threads = MTLSizeMake(count, 1, 1);
    NSUInteger w = clampThreads(count, pso.maxTotalThreadsPerThreadgroup);

    [enc dispatchThreads: threads threadsPerThreadgroup: MTLSizeMake(w, 1, 1)];

    return true;
}

- (bool) encodeRemapRestartIndex: (id<MTLComputeCommandEncoder>) enc
                          buffer: (id<MTLBuffer>) buf
                          offset: (NSUInteger) offset
                           count: (NSUInteger) count
                       indexType: (MTLIndexType) type
                    restartIndex: (uint32_t) glIndex
{
    const char *name = (type == MTLIndexTypeUInt16) ? "remapRestartIndex16"
                                                    : "remapRestartIndex32";
    id<MTLComputePipelineState> pso = [self pipelineNamed: name];

    if (pso == nil)
        return false;

    [enc setComputePipelineState: pso];
    [enc setBuffer: buf offset: offset atIndex: 0];
    [enc setBytes: &glIndex length: sizeof(glIndex) atIndex: 1];

    MTLSize threads = MTLSizeMake(count, 1, 1);
    NSUInteger w = clampThreads(count, pso.maxTotalThreadsPerThreadgroup);

    [enc dispatchThreads: threads threadsPerThreadgroup: MTLSizeMake(w, 1, 1)];

    return true;
}

- (bool) encodeFillBuffer: (id<MTLComputeCommandEncoder>) enc
                   buffer: (id<MTLBuffer>) buf
                   offset: (NSUInteger) offset
                wordCount: (NSUInteger) words
                    value: (uint32_t) v
{
    id<MTLComputePipelineState> pso = [self pipelineNamed: "cmdFillBuffer"];

    if (pso == nil)
        return false;

    [enc setComputePipelineState: pso];
    [enc setBuffer: buf offset: offset atIndex: 0];
    [enc setBytes: &v length: sizeof(v) atIndex: 1];

    MTLSize threads = MTLSizeMake(words, 1, 1);
    NSUInteger w = clampThreads(words, pso.maxTotalThreadsPerThreadgroup);

    [enc dispatchThreads: threads threadsPerThreadgroup: MTLSizeMake(w, 1, 1)];

    return true;
}

- (bool) encodeCopyBufferBytes: (id<MTLComputeCommandEncoder>) enc
                        source: (id<MTLBuffer>) src
                   sourceOffset: (NSUInteger) so
                   destination: (id<MTLBuffer>) dst
              destinationOffset: (NSUInteger) doff
                           size: (NSUInteger) size
{
    id<MTLComputePipelineState> pso = [self pipelineNamed: "cmdCopyBufferBytes"];

    if (pso == nil)
        return false;

    struct { uint32_t srcOffset; uint32_t dstOffset; uint32_t size; } info = {
        .srcOffset = (uint32_t)so,
        .dstOffset = (uint32_t)doff,
        .size      = (uint32_t)size
    };

    [enc setComputePipelineState: pso];
    [enc setBuffer: src offset: 0 atIndex: 0];
    [enc setBuffer: dst offset: 0 atIndex: 1];
    [enc setBytes: &info length: sizeof(info) atIndex: 2];

    [enc dispatchThreads: MTLSizeMake(1, 1, 1)
   threadsPerThreadgroup: MTLSizeMake(1, 1, 1)];

    return true;
}

- (bool) encodeTriFanIndexes: (id<MTLComputeCommandEncoder>) enc
                  fanIndices: (id<MTLBuffer>) fan
                      offset: (NSUInteger) fanOffset
                       count: (NSUInteger) fanCount
                  triIndices: (id<MTLBuffer>) tri
                   indexType: (MTLIndexType) type
                      vtxAdj: (MGLVtxAdj) adj
{
    const char *name = (type == MTLIndexTypeUInt16) ? "triFanIndexes16"
                                                    : "triFanIndexes32";
    id<MTLComputePipelineState> pso = [self pipelineNamed: name];

    if (pso == nil)
        return false;

    adj.idxType = (uint8_t)type;

    uint32_t fc = (uint32_t)fanCount;

    [enc setComputePipelineState: pso];
    [enc setBuffer: fan offset: fanOffset atIndex: 0];
    [enc setBuffer: tri offset: 0 atIndex: 1];
    [enc setBytes: &fc length: sizeof(fc) atIndex: 2];
    [enc setBytes: &adj length: sizeof(adj) atIndex: 3];

    [enc dispatchThreads: MTLSizeMake(1, 1, 1)
   threadsPerThreadgroup: MTLSizeMake(1, 1, 1)];

    return true;
}

@end
