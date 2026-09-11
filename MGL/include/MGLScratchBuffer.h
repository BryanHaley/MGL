/*
 * MGLScratchBuffer.h
 * MGL
 *
 * Short-lived GPU buffers for work that lasts one command buffer.
 *
 * Tessellation needs several temporaries per draw, primitive expansion needs an
 * index buffer per draw, and transform feedback needs its capture buffers. All
 * of them are wanted for exactly as long as the command buffer that reads them.
 *
 * Growing one buffer by hand, which is what the expansion path used to do, only
 * works while there is one draw in flight: the next draw resizes the buffer the
 * GPU is still reading.
 */

#ifndef MGLScratchBuffer_h
#define MGLScratchBuffer_h

#import <Metal/Metal.h>

@interface MGLScratchBufferPool : NSObject

- (instancetype) initWithDevice: (id<MTLDevice>) device;

/// A buffer of at least `length` bytes, owned by the pool. Valid until the
/// command buffer it was taken for completes. Never write to one after that.
- (id<MTLBuffer>) bufferOfLength: (NSUInteger) length;

/// Hand every buffer taken since the last call back once `commandBuffer`
/// finishes on the GPU. Call this once per command buffer, before committing.
- (void) recycleWhenComplete: (id<MTLCommandBuffer>) commandBuffer;

/// Drop everything. For teardown and for recovering from a device reset.
- (void) drain;

@property (readonly) NSUInteger buffersInFlight;
@property (readonly) NSUInteger buffersPooled;

@end

#endif /* MGLScratchBuffer_h */
