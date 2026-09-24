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
 * MGLScratchBuffer.m
 * MGL
 */

#import "MGLScratchBuffer.h"

// Buckets are powers of two from 4 KB up, so a draw that asks for a slightly
// different size each frame still reuses the same allocation.
#define MGL_SCRATCH_MIN_SHIFT   12      // 4 KB
#define MGL_SCRATCH_MAX_SHIFT   28      // 256 MB
#define MGL_SCRATCH_BUCKETS     (MGL_SCRATCH_MAX_SHIFT - MGL_SCRATCH_MIN_SHIFT + 1)
// How much the pool keeps waiting to be reused. It used to keep everything
// it had ever handed out, so one heavy draw held its peak for good.
#define MGL_SCRATCH_IDLE_CAP    ((NSUInteger)64 << 20)

@implementation MGLScratchBufferPool
{
    id<MTLDevice>       _device;
    NSMutableArray     *_free[MGL_SCRATCH_BUCKETS];   // buffers ready to hand out
    NSMutableArray     *_inFlight;                    // taken since the last recycle
    NSUInteger          _idleBytes;                   // what the free lists hold
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

// The completion handler below returns buffers from whatever thread Metal
// runs it on, so every touch of _free and _inFlight is under the same lock.
- (id<MTLBuffer>) bufferOfLength: (NSUInteger) length
{
    @synchronized (self) {

    id<MTLBuffer> buffer = [self takeBufferOfLength: length];

    if (buffer != nil)
        [_inFlight addObject: buffer];

    return buffer;

    }
}

// The allocation on its own, without recording it as outstanding. Callers hold
// the lock. Split out so a buffer taken for one named command buffer never
// appears in _inFlight even briefly: it used to be added and then removed
// under two separate acquisitions, and a recycleWhenComplete: landing in the
// gap took the buffer with it. Both that command buffer and this one would
// then return it, putting one buffer in the free list twice and handing it to
// two callers at once.
- (id<MTLBuffer>) takeBufferOfLength: (NSUInteger) length
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
        _idleBytes -= [buffer length];
    }
    else
    {
        buffer = [_device newBufferWithLength: size options: _options];

        if (buffer == nil)
            return nil;
    }

    return buffer;
}

// Takes a buffer for one specific command buffer and hands it back when that
// one completes. recycleWhenComplete below ties every outstanding buffer to
// whichever command buffer happens to be closing, which is wrong for work
// taken part way through a draw: the draw's own buffers go back to the free
// list while it is still encoding with them.
- (id<MTLBuffer>) bufferOfLength: (NSUInteger) length
                forCommandBuffer: (id<MTLCommandBuffer>) commandBuffer
{
    id<MTLBuffer> buffer = nil;

    @synchronized (self) {
        buffer = [self takeBufferOfLength: length];

        // With no command buffer to tie it to there is nothing to wait on, so
        // it falls back to the outstanding set and the next recycle takes it.
        if (buffer != nil && commandBuffer == nil)
            [_inFlight addObject: buffer];
    }

    if (buffer == nil || commandBuffer == nil)
        return buffer;

    __weak MGLScratchBufferPool *weakSelf = self;

    [commandBuffer addCompletedHandler: ^(id<MTLCommandBuffer> cb) {
        (void)cb;

        MGLScratchBufferPool *pool = weakSelf;

        if (pool)
            @synchronized (pool) { [pool returnBuffers: @[ buffer ]]; }
    }];

    return buffer;
}

- (void) recycleWhenComplete: (id<MTLCommandBuffer>) commandBuffer
{
    NSArray *taken;

    @synchronized (self) {
        if ([_inFlight count] == 0)
            return;

        taken = [_inFlight copy];
        [_inFlight removeAllObjects];
    }

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

// Takes the lock itself. @synchronized is recursive, so the completion
// handlers that already hold it are unaffected, and the one path that called
// this without it — a recycle with no command buffer — no longer mutates the
// free lists while another thread is reading them.
- (void) returnBuffers: (NSArray *) buffers
{
    @synchronized (self)
    {
    for (id<MTLBuffer> buffer in buffers)
    {
        NSUInteger bucket = bucketForLength([buffer length]);

        // an oversized one-off is dropped rather than held forever
        if (bucket < MGL_SCRATCH_BUCKETS &&
            [buffer length] == ((NSUInteger)1 << (bucket + MGL_SCRATCH_MIN_SHIFT)) &&
            _idleBytes + [buffer length] <= MGL_SCRATCH_IDLE_CAP)
        {
            [_free[bucket] addObject: buffer];
            _idleBytes += [buffer length];
        }
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

        _idleBytes = 0;
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
