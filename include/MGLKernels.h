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
 * MGLKernels.h
 * MGL
 */

#ifndef MGLKernels_h
#define MGLKernels_h

#include <stdint.h>
#include <stdbool.h>

// Shared struct: must match the MSL MGLVtxAdj byte for byte.
// MSL bool is 1 byte; the enum MTLIndexType is uint8_t.
typedef struct {
    uint8_t idxType;            // 0 = UInt16, 1 = UInt32
    bool    isTriFan;
    bool    isPrimRestart;
    bool    isUint8Index;
    bool    isProvokingVertexLast;
} MGLVtxAdj;

// Query result flags — same values as the MSL MGLQueryResultFlagBits.
enum {
    MGL_QUERY_RESULT_64_BIT                = 0x00000001,
    MGL_QUERY_RESULT_WAIT_BIT              = 0x00000002,
    MGL_QUERY_RESULT_WITH_AVAILABILITY_BIT = 0x00000004,
    MGL_QUERY_RESULT_PARTIAL_BIT           = 0x00000008,
};

#ifdef __OBJC__

#import <Metal/Metal.h>

@interface MGLKernelLibrary : NSObject

- (instancetype) initWithDevice: (id<MTLDevice>) device;

- (id<MTLComputePipelineState>) pipelineNamed: (const char *) name;

- (bool) encodeConvertUint8Indices: (id<MTLComputeCommandEncoder>) enc
                            source: (id<MTLBuffer>) src
                      sourceOffset: (NSUInteger) srcOffset
                       destination: (id<MTLBuffer>) dst
                             count: (NSUInteger) count
                           keepFF: (bool) mapSentinel;

- (bool) encodeRemapRestartIndex: (id<MTLComputeCommandEncoder>) enc
                          buffer: (id<MTLBuffer>) buf
                          offset: (NSUInteger) offset
                           count: (NSUInteger) count
                       indexType: (MTLIndexType) type
                    restartIndex: (uint32_t) glIndex;

- (bool) encodeFillBuffer: (id<MTLComputeCommandEncoder>) enc
                   buffer: (id<MTLBuffer>) buf
                   offset: (NSUInteger) offset
                wordCount: (NSUInteger) words
                    value: (uint32_t) v;

- (bool) encodeCopyBufferBytes: (id<MTLComputeCommandEncoder>) enc
                        source: (id<MTLBuffer>) src
                   sourceOffset: (NSUInteger) so
                   destination: (id<MTLBuffer>) dst
              destinationOffset: (NSUInteger) doff
                           size: (NSUInteger) size;

- (bool) encodeTriFanIndexes: (id<MTLComputeCommandEncoder>) enc
                  fanIndices: (id<MTLBuffer>) fan
                      offset: (NSUInteger) fanOffset
                       count: (NSUInteger) fanCount
                  triIndices: (id<MTLBuffer>) tri
                   indexType: (MTLIndexType) type
                      vtxAdj: (MGLVtxAdj) adj;

@end

#endif // __OBJC__

#endif /* MGLKernels_h */
