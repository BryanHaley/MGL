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
 * MGLBlit.m
 * MGL
 *
 * Metal blit operations for texture-to-texture and framebuffer-to-texture
 * copies. Provides the two mtl_funcs entries that were declared in
 * glm_context.h but never assigned in MGLRenderer.m:
 *   mtlCopyTexSubImage   — read framebuffer → texture
 *   mtlCopyImageSubData  — texture → texture (glCopyImageSubData)
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "glm_context.h"

// Private MGLRenderer methods reachable through mtl_funcs.mtlObj.
@interface NSObject (MGLBlitInternal)
- (id<MTLCommandBuffer>) liveCommandBuffer;
- (void) endRenderEncoding;
- (id<MTLTexture>) readSourceTexture:(GLMContext)glm_ctx forFormat:(GLenum)format;
- (bool) bindMTLTexture:(Texture *)tex;
@end

void mtlCopyTexSubImage(GLMContext glm_ctx, Texture *tex, GLint level,
    GLint xoffset, GLint yoffset, GLint x, GLint y,
    GLsizei width, GLsizei height)
{
    if (!tex || width <= 0 || height <= 0) return;

    id renderer = (__bridge id)glm_ctx->mtl_funcs.mtlObj;

    if (![renderer bindMTLTexture:tex] || !tex->mtl_data) return;

    id<MTLTexture> srcTex = [renderer readSourceTexture:glm_ctx forFormat:GL_RGBA];
    if (!srcTex) return;

    id<MTLTexture> dstTex = (__bridge id<MTLTexture>)(tex->mtl_data);

    [renderer endRenderEncoding];

    id<MTLBlitCommandEncoder> blit = [[renderer liveCommandBuffer] blitCommandEncoder];

    // GL counts rows from the bottom, Metal from the top.  User FBOs are
    // already rasterised flipped (renderTargetIsFlipped) so their Metal
    // pixels are in GL order and need no adjustment.  The default
    // framebuffer is Metal-native and must be flipped.
    bool srcFlipped = (glm_ctx->state.readbuffer != NULL);
    NSUInteger srcY = srcFlipped
        ? (NSUInteger)y
        : srcTex.height - (NSUInteger)y - (NSUInteger)height;

    [blit copyFromTexture:srcTex
             sourceSlice:0
             sourceLevel:0
            sourceOrigin:MTLOriginMake((NSUInteger)x, srcY, 0)
              sourceSize:MTLSizeMake((NSUInteger)width, (NSUInteger)height, 1)
               toTexture:dstTex
      destinationSlice:0
      destinationLevel:(NSUInteger)level
     destinationOrigin:MTLOriginMake((NSUInteger)xoffset, (NSUInteger)yoffset, 0)];

    [blit endEncoding];
}

void mtlCopyImageSubData(GLMContext glm_ctx,
    Texture *srcTex, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ,
    Texture *dstTex, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ,
    GLsizei width, GLsizei height, GLsizei depth)
{
    if (!srcTex || !dstTex || width <= 0 || height <= 0 || depth <= 0) return;

    id renderer = (__bridge id)glm_ctx->mtl_funcs.mtlObj;

    if (![renderer bindMTLTexture:srcTex] || !srcTex->mtl_data) return;
    if (![renderer bindMTLTexture:dstTex] || !dstTex->mtl_data) return;

    id<MTLTexture> srcMetal = (__bridge id<MTLTexture>)(srcTex->mtl_data);
    id<MTLTexture> dstMetal = (__bridge id<MTLTexture>)(dstTex->mtl_data);

    [renderer endRenderEncoding];

    id<MTLBlitCommandEncoder> blit = [[renderer liveCommandBuffer] blitCommandEncoder];

    // Metal's copyFromTexture: treats the slice parameter differently for
    // 3D textures (ignored; depth is in origin.z / size.depth) vs. array
    // and cube textures (slice selects the layer / face).
    bool srcIs3D = (srcTex->target == GL_TEXTURE_3D);
    bool dstIs3D = (dstTex->target == GL_TEXTURE_3D);

    // sourceSize.depth only moves multiple images for a 3D texture. An array
    // or cube face needs one call per layer, or only the first one is copied.
    NSUInteger layers = (srcIs3D && dstIs3D) ? 1 : (NSUInteger)depth;
    NSUInteger copyDepth = (srcIs3D && dstIs3D) ? (NSUInteger)depth : 1;

    for (NSUInteger i = 0; i < layers; i++)
    {
        NSUInteger srcSlice   = srcIs3D ? 0 : (NSUInteger)srcZ + i;
        NSUInteger dstSlice   = dstIs3D ? 0 : (NSUInteger)dstZ + i;
        NSUInteger srcZOrigin = srcIs3D ? (NSUInteger)srcZ + i : 0;
        NSUInteger dstZOrigin = dstIs3D ? (NSUInteger)dstZ + i : 0;

        [blit copyFromTexture:srcMetal
                 sourceSlice:srcSlice
                 sourceLevel:(NSUInteger)srcLevel
                sourceOrigin:MTLOriginMake((NSUInteger)srcX, (NSUInteger)srcY, srcZOrigin)
                  sourceSize:MTLSizeMake((NSUInteger)width, (NSUInteger)height, copyDepth)
                   toTexture:dstMetal
          destinationSlice:dstSlice
          destinationLevel:(NSUInteger)dstLevel
         destinationOrigin:MTLOriginMake((NSUInteger)dstX, (NSUInteger)dstY, dstZOrigin)];
    }

    [blit endEncoding];
}
