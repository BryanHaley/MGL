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
 * MGLScratchBuffer.m
 * MGL
 */

#import "MGLScratchBuffer.h"

// Buckets are powers of two from 4 KB up, so a draw that asks for a slightly
// different size each frame still reuses the same allocation.
#define MGL_SCRATCH_MIN_SHIFT   12      // 4 KB
#define MGL_SCRATCH_MAX_SHIFT   28      // 256 MB
#define MGL_SCRATCH_BUCKETS     (MGL_SCRATCH_MAX_SHIFT - MGL_SCRATCH_MIN_SHIFT + 1)

@implementation MGLScratchBufferPool
{
    id<MTLDevice>       _device;
    NSMutableArray     *_free[MGL_SCRATCH_BUCKETS];   // buffers ready to hand out
    NSMutableArray     *_inFlight;                    // taken since the last recycle
    MTLResourceOptions  _options;
}

static NSUInteger bucketForLength(NSUInteger length)
{
    NSUInteger shift = MGL_SCRATCH_MIN_SHIFT;

    while (shift < MGL_SCRATCH_MAX_SHIFT && ((NSUInteger)1 << shift) < length)
        shift++;

    return shift - MGL_SCRATCH_MIN_SHIFT;
}

- (instancetype) initWithDevice: (id<MTLDevice>) device
{
    self = [super init];

    if (self == nil)
        return nil;

    _device = device;
    _inFlight = [NSMutableArray array];

    for (int i = 0; i < MGL_SCRATCH_BUCKETS; i++)
        _free[i] = [NSMutableArray array];

    // Same reasoning as the main buffer path: on unified memory there is one
    // copy and Shared costs nothing, while Managed would need a synchronise
    // every time the CPU filled one of these.
    _options = [device hasUnifiedMemory]
             ? (MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModeShared)
             : (MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModeManaged);

    return self;
}

- (id<MTLBuffer>) bufferOfLength: (NSUInteger) length
{
    if (length == 0)
        length = 1;

    NSUInteger bucket = bucketForLength(length);
    NSUInteger size = (NSUInteger)1 << (bucket + MGL_SCRATCH_MIN_SHIFT);

    // anything past the largest bucket gets its own allocation, used once
    if (size < length)
        size = length;

    id<MTLBuffer> buffer = nil;

    if (bucket < MGL_SCRATCH_BUCKETS && [_free[bucket] count] > 0)
    {
        buffer = [_free[bucket] lastObject];
        [_free[bucket] removeLastObject];
    }
    else
    {
        buffer = [_device newBufferWithLength: size options: _options];

        if (buffer == nil)
            return nil;
    }

    [_inFlight addObject: buffer];

    return buffer;
}

- (void) recycleWhenComplete: (id<MTLCommandBuffer>) commandBuffer
{
    if ([_inFlight count] == 0)
        return;

    NSArray *taken = [_inFlight copy];
    [_inFlight removeAllObjects];

    if (commandBuffer == nil)
    {
        // no command buffer to wait on, so they were never used by the GPU
        [self returnBuffers: taken];
        return;
    }

    __weak MGLScratchBufferPool *weakSelf = self;

    [commandBuffer addCompletedHandler: ^(id<MTLCommandBuffer> cb) {
        (void)cb;

        MGLScratchBufferPool *pool = weakSelf;

        if (pool)
        {
            // the handler runs off the main thread
            @synchronized (pool) { [pool returnBuffers: taken]; }
        }
    }];
}

- (void) returnBuffers: (NSArray *) buffers
{
    for (id<MTLBuffer> buffer in buffers)
    {
        NSUInteger bucket = bucketForLength([buffer length]);

        // an oversized one-off is dropped rather than held forever
        if (bucket < MGL_SCRATCH_BUCKETS &&
            [buffer length] == ((NSUInteger)1 << (bucket + MGL_SCRATCH_MIN_SHIFT)))
        {
            [_free[bucket] addObject: buffer];
        }
    }
}

- (void) drain
{
    @synchronized (self)
    {
        [_inFlight removeAllObjects];

        for (int i = 0; i < MGL_SCRATCH_BUCKETS; i++)
            [_free[i] removeAllObjects];
    }
}

- (NSUInteger) buffersInFlight
{
    return [_inFlight count];
}

- (NSUInteger) buffersPooled
{
    NSUInteger n = 0;

    for (int i = 0; i < MGL_SCRATCH_BUCKETS; i++)
        n += [_free[i] count];

    return n;
}

@end
