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
 * MGLInterop.m
 * MGL
 *
 * Hands the Metal objects behind GL out to code that wants to encode its own
 * Metal work in order with GL's, such as the FSR 2 library on MetalFX.
 */

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "glm_context.h"

@interface NSObject (MGLInteropInternal)
- (id<MTLCommandBuffer>) liveCommandBuffer;
- (void) endRenderEncoding;
- (void) endComputeEncoding;
- (void) applyPendingClears;
- (bool) bindMTLTexture:(Texture *)tex;
@end

Texture *findTexture(GLMContext ctx, GLuint texture);

void *MGLgetMetalTexture(GLMContext ctx, GLuint texture)
{
    if (ctx == NULL)
        ctx = MGLgetCurrentContext();

    if (ctx == NULL || texture == 0)
        return NULL;

    Texture *tex = findTexture(ctx, texture);

    if (tex == NULL)
        return NULL;

    @autoreleasepool {
        id renderer = (__bridge id)ctx->mtl_funcs.mtlObj;

        [renderer applyPendingClears];

        if (![renderer bindMTLTexture:tex])
            return NULL;
    }

    return tex->mtl_data;
}

void *MGLgetMetalCommandBuffer(GLMContext ctx)
{
    if (ctx == NULL)
        ctx = MGLgetCurrentContext();

    if (ctx == NULL)
        return NULL;

    id renderer = (__bridge id)ctx->mtl_funcs.mtlObj;

    [renderer applyPendingClears];
    [renderer endRenderEncoding];
    [renderer endComputeEncoding];

    return (__bridge void *)[renderer liveCommandBuffer];
}
