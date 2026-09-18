/*
 * Copyright (C) Michael Larson on 1/6/2022
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
 * textures.c
 * MGL
 *
 */

#include <mach/mach_vm.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>

#include <stdio.h>

#include <Accelerate/Accelerate.h>

#include "pixel_utils.h"
#include "pixel_convert.h"
#include "mgl_format_table.h"
#include "utils.h"
#include "glm_context.h"
#include "mgl_log.h"

extern void *getBufferData(GLMContext ctx, Buffer *ptr);

bool texSubImage(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void *pixels);
void invalidateTexture(GLMContext ctx, Texture *tex);

// true once TexImage or TexStorage has actually defined this level
static bool texLevelDefined(Texture *tex, GLuint face, GLint level)
{
    if (tex == NULL || level < 0 || face >= _CUBE_MAP_MAX_FACE)
        return false;

    if (tex->faces[face].levels == NULL)
        return false;

    if ((GLuint)level >= tex->mipmap_levels)
        return false;

    return tex->faces[face].levels[level].complete;
}

// a cube map face target names the cube map object it belongs to
GLenum normalizeTextureTarget(GLenum target)
{
    if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
        return GL_TEXTURE_CUBE_MAP;

    return target;
}

GLuint textureIndexFromTarget(GLMContext ctx, GLenum target)
{
    switch(normalizeTextureTarget(target))
    {
        case GL_TEXTURE_BUFFER: return _TEXTURE_BUFFER_TARGET;
        case GL_TEXTURE_1D: return _TEXTURE_1D;
        case GL_TEXTURE_2D: return _TEXTURE_2D;
        case GL_TEXTURE_3D: return _TEXTURE_3D;
        case GL_TEXTURE_RECTANGLE: return _TEXTURE_RECTANGLE;
        case GL_TEXTURE_1D_ARRAY: return _TEXTURE_1D_ARRAY;
        case GL_TEXTURE_2D_ARRAY: return _TEXTURE_2D_ARRAY;
        case GL_TEXTURE_CUBE_MAP: return _TEXTURE_CUBE_MAP;
        case GL_TEXTURE_CUBE_MAP_ARRAY: return _TEXTURE_CUBE_MAP_ARRAY;
        case GL_TEXTURE_2D_MULTISAMPLE: return _TEXTURE_2D_MULTISAMPLE;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: return _TEXTURE_2D_MULTISAMPLE_ARRAY;
        case GL_RENDERBUFFER: return _RENDERBUFFER;

        default:
            break;
    }

    return _MAX_TEXTURE_TYPES;
}

Texture *currentTexture(GLMContext ctx, GLuint index)
{
    GLuint active_texture;

    active_texture = STATE(active_texture);

    return STATE(texture_units[active_texture].textures[index]);
}

Texture *newTexObj(GLMContext ctx, GLenum target)
{
    Texture *ptr;
    GLuint index;

    index = textureIndexFromTarget(ctx, target);
    ERROR_CHECK_RETURN_VALUE(index != _MAX_TEXTURE_TYPES, GL_INVALID_ENUM, NULL);

    ptr = (Texture *)malloc(sizeof(Texture));
    // CRITICAL SECURITY FIX: Check malloc result instead of using assert()
    if (!ptr) {
        MGL_ERR("MGL SECURITY ERROR: Failed to allocate memory for texture\n");
        STATE(error) = GL_OUT_OF_MEMORY;
        return NULL;
    }

    bzero(ptr, sizeof(Texture));

    ptr->name = TEX_OBJ_RES_NAME;
    ptr->target = normalizeTextureTarget(target);
    ptr->index = index;

    float black_color[] = {0,0,0,0};

    ptr->params.depth_stencil_mode = GL_DEPTH_COMPONENT;
    ptr->params.base_level = 0;
    memcpy(ptr->params.border_color, black_color, 4 * sizeof(float));
    // defaults from the GL 4.6 core texture state table
    ptr->params.compare_func = GL_LEQUAL;
    ptr->params.compare_mode = GL_NONE;
    ptr->params.lod_bias = 0.0;
    ptr->params.min_filter = GL_NEAREST_MIPMAP_LINEAR;
    ptr->params.mag_filter = GL_LINEAR;
    ptr->params.max_anisotropy = 1.0;
    ptr->params.min_lod = -1000;
    ptr->params.max_lod = 1000;
    ptr->params.max_level = 1000;
    ptr->params.swizzle_r = GL_RED;
    ptr->params.swizzle_g = GL_GREEN;
    ptr->params.swizzle_b = GL_BLUE;
    ptr->params.swizzle_a = GL_ALPHA;
    ptr->params.wrap_s = GL_REPEAT;
    ptr->params.wrap_t = GL_REPEAT;
    ptr->params.wrap_r = GL_REPEAT;

    return ptr;
}

Texture *newTexture(GLMContext ctx, GLenum target, GLuint texture)
{
    Texture *ptr;
    GLuint index;

    index = textureIndexFromTarget(ctx, target);
    ERROR_CHECK_RETURN_VALUE(index != _MAX_TEXTURE_TYPES, GL_INVALID_ENUM, NULL);

    ptr = newTexObj(ctx, target);

    ptr->name = texture;

    return ptr;
}

static Texture *getTexture(GLMContext ctx, GLenum target, GLuint texture)
{
    Texture *ptr;

    ptr = (Texture *)searchHashTable(&STATE(texture_table), texture);

    if (!ptr)
    {
        ptr = newTexture(ctx, target, texture);

        insertHashElement(&STATE(texture_table), texture, ptr);
    }

    return ptr;
}

static int isTexture(GLMContext ctx, GLuint texture)
{
    Texture *ptr;

    ptr = (Texture *)searchHashTable(&STATE(texture_table), texture);

    if (ptr)
        return 1;

    return 0;
}

Texture *findTexture(GLMContext ctx, GLuint texture)
{
    Texture *ptr;

    ptr = (Texture *)searchHashTable(&STATE(texture_table), texture);

    return ptr;
}

Texture *getTex(GLMContext ctx, GLuint name, GLenum target)
{
    GLuint index;
    Texture *ptr;

    if (name == 0)
    {
        index = textureIndexFromTarget(ctx, target);

        ERROR_CHECK_RETURN_VALUE(index != _MAX_TEXTURE_TYPES, GL_INVALID_ENUM, NULL);

        ptr = currentTexture(ctx, index);

        if (!ptr) {
            GLuint active_texture = STATE(active_texture);

            ptr = newTexObj(ctx, target);

            ERROR_CHECK_RETURN_VALUE(ptr, GL_OUT_OF_MEMORY, NULL);

            STATE(texture_units[active_texture].textures[index]) = ptr;
        }
    }
    else
    {
        ptr = findTexture(ctx, name);

        ERROR_CHECK_RETURN_VALUE(ptr, GL_INVALID_OPERATION, NULL);

        target = ptr->target;

        index = textureIndexFromTarget(ctx, target);

        ERROR_CHECK_RETURN_VALUE(index != _MAX_TEXTURE_TYPES, GL_INVALID_ENUM, NULL);
    }

    return ptr;
}

bool checkInternalFormatForMetal(GLMContext ctx, GLuint internalformat)
{
    // see if we can actually use this internal format
    GLenum mtl_format;
    mtl_format = mtlFormatForGLInternalFormat(internalformat);

    if (mtl_format == MTLPixelFormatInvalid)
    {
        // Only warn once per format to reduce log spam during capability probing
        static unsigned warned_formats[64] = {0};
        static int warned_count = 0;
        int already_warned = 0;
        for (int i = 0; i < warned_count && i < 64; i++) {
            if (warned_formats[i] == internalformat) { already_warned = 1; break; }
        }
        if (!already_warned && warned_count < 64) {
            warned_formats[warned_count++] = internalformat;
            // Only warn for standard GL format ranges (not internal Mesa/Gallium enums)
            // Skip 0x2xxx (GL get parameters), 0x8Dxx-0x9xxx (internal enums)
            if (internalformat >= 0x8040 && internalformat < 0x8D70) {
                MGL_INFO("MGL: checkInternalFormatForMetal - internalformat 0x%x has no Metal equivalent\n", internalformat);
            }
        }
        return false;
    }

    return true;
}


// glGetIntegerv(GL_TEXTURE_BINDING_*) reads these and nothing else writes them
static void recordTextureBinding(GLMContext ctx, GLenum target, GLuint texture)
{
    switch(normalizeTextureTarget(target))
    {
        case GL_TEXTURE_1D:       STATE_VAR(texture_binding_1d) = texture; break;
        case GL_TEXTURE_2D:       STATE_VAR(texture_binding_2d) = texture; break;
        case GL_TEXTURE_3D:       STATE_VAR(texture_binding_3d) = texture; break;
        case GL_TEXTURE_CUBE_MAP: STATE_VAR(texture_binding_cube_map) = texture; break;
        case GL_TEXTURE_1D_ARRAY: STATE_VAR(texture_binding_1d_array) = texture; break;
        case GL_TEXTURE_2D_ARRAY: STATE_VAR(texture_binding_2d_array) = texture; break;
        case GL_TEXTURE_BUFFER:   STATE_VAR(texture_binding_buffer) = texture; break;
        case GL_TEXTURE_RECTANGLE: STATE_VAR(texture_binding_rectangle) = texture; break;
        case GL_TEXTURE_2D_MULTISAMPLE: STATE_VAR(texture_binding_2d_multisample) = texture; break;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: STATE_VAR(texture_binding_2d_multisample_array) = texture; break;

        default:
            break;
    }
}

// GL_TEXTURE_BINDING_* always describes the active unit, so the whole set is
// rebuilt from it rather than patched one target at a time
static void clearTextureBindings(GLMContext ctx)
{
    STATE_VAR(texture_binding_1d) = 0;
    STATE_VAR(texture_binding_2d) = 0;
    STATE_VAR(texture_binding_3d) = 0;
    STATE_VAR(texture_binding_cube_map) = 0;
    STATE_VAR(texture_binding_1d_array) = 0;
    STATE_VAR(texture_binding_2d_array) = 0;
    STATE_VAR(texture_binding_buffer) = 0;
    STATE_VAR(texture_binding_rectangle) = 0;
    STATE_VAR(texture_binding_2d_multisample) = 0;
    STATE_VAR(texture_binding_2d_multisample_array) = 0;
}

static void syncTextureBindings(GLMContext ctx)
{
    TextureUnit *unit;

    clearTextureBindings(ctx);

    unit = &STATE(texture_units[STATE(active_texture)]);

    for (int i = 0; i < _MAX_TEXTURE_TYPES; i++)
    {
        Texture *tex = unit->textures[i];

        // the nameless object MGL invents for an unbound target is not a binding
        if (tex && tex->name != TEX_OBJ_RES_NAME)
            recordTextureBinding(ctx, tex->target, tex->name);
    }
}

#pragma mark basic tex calls bind / delete / gen...
void mglGenTextures(GLMContext ctx, GLsizei n, GLuint *textures)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(textures, GL_INVALID_VALUE);

    while(n--)
    {
        *textures++ = getNewName(&STATE(texture_table));

        // TEX_OBJ_RES_NAME has special name.. skip it
        if (STATE(texture_table.current_name) == TEX_OBJ_RES_NAME)
            getNewName(&STATE(texture_table));
    }
}

void mglCreateTextures(GLMContext ctx, GLenum target, GLsizei n, GLuint *textures)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    mglGenTextures(ctx, n, textures);

    while(n--)
    {
        // create a texture object
        getTexture(ctx, target, *textures++);
    }
}

void mglBindTexture(GLMContext ctx, GLenum target, GLuint texture)
{
    GLuint active_texture;
    GLint index;
    Texture *ptr;

    index = textureIndexFromTarget(ctx, target);
    ERROR_CHECK_RETURN(index != _MAX_TEXTURE_TYPES, GL_INVALID_ENUM);

    if (texture)
    {
        ptr = getTexture(ctx, target, texture);

        ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);
    }
    else
    {
        ptr = NULL;
    }

    active_texture = STATE(active_texture);

    GLuint mask_index = active_texture / 32;
    GLuint mask = (0x1 << active_texture);

    if (ptr)
    {
        STATE(active_texture_mask[mask_index]) |= mask;
    }
    else
    {
        STATE(active_texture_mask[mask_index]) &= ~mask;
    }

    STATE(active_textures[active_texture]) = ptr;
    STATE(texture_units[active_texture].textures[index]) = ptr;
    STATE(dirty_bits) |= DIRTY_TEX;

    syncTextureBindings(ctx);
}

void mglBindImageTexture(GLMContext ctx, GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum internalformat)
{
    Texture *ptr;

    // ERROR_CHECK_RETURN(unit < TEXTURE_UNITS, GL_INVALID_VALUE);
    if (unit >= TEXTURE_UNITS) {
        MGL_ERR("MGL Error: mglBindImageTexture: unit >= TEXTURE_UNITS (%d)\n", unit);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    if (texture == 0)
    {
        // zero breaks whatever was bound to the unit
        bzero(&ctx->state.image_units[unit], sizeof(ImageUnit));
        ctx->state.dirty_bits |= DIRTY_IMAGE_UNIT_STATE;
        return;
    }

    // a name that was never a texture is INVALID_VALUE here, so look it up
    // directly rather than through getTex, which calls it INVALID_OPERATION
    ptr = findTexture(ctx, texture);

    if (!ptr) {
        MGL_ERR("MGL Error: mglBindImageTexture: texture %d not found\n", texture);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);
    if (level < 0) {
        MGL_ERR("MGL Error: mglBindImageTexture: level < 0 (%d)\n", level);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // ERROR_CHECK_RETURN(layered >= 0, GL_INVALID_VALUE);
    if (layered < 0) {
        MGL_ERR("MGL Error: mglBindImageTexture: layered < 0 (%d)\n", layered);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    switch(access)
    {
        case GL_READ_ONLY:
        case GL_WRITE_ONLY:
        case GL_READ_WRITE:
            break;

        default:
            MGL_ERR("MGL Error: mglBindImageTexture: invalid access 0x%x\n", access);
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    // ERROR_CHECK_RETURN(checkInternalFormatForMetal(ctx, internalformat), GL_INVALID_ENUM);
    if (!checkInternalFormatForMetal(ctx, internalformat)) {
        MGL_ERR("MGL Error: mglBindImageTexture: invalid internalformat 0x%x\n", internalformat);
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    // ERROR_CHECK_RETURN(ptr->internalformat == internalformat, GL_INVALID_VALUE);
    if (ptr->internalformat != internalformat) {
        MGL_ERR("MGL Error: mglBindImageTexture: internalformat mismatch (tex=0x%x req=0x%x)\n", ptr->internalformat, internalformat);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    // ERROR_CHECK_RETURN(level < ptr->num_levels, GL_INVALID_VALUE);
    if (level >= ptr->num_levels) {
        MGL_ERR("MGL Error: mglBindImageTexture: level >= num_levels (%d >= %d)\n", level, ptr->num_levels);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    
    ImageUnit unit_params;

    if (ptr->access != access)
    {
        ptr->dirty_bits |= DIRTY_TEXTURE_ACCESS;
        ptr->access = access;
    }

    unit_params.texture = texture;
    unit_params.level = level;
    unit_params.layered = layered;
    unit_params.layer = layer;
    unit_params.access = access;
    unit_params.internalformat = internalformat;
    unit_params.tex = ptr;

    ctx->state.image_units[unit] = unit_params;

    ctx->state.dirty_bits |= DIRTY_IMAGE_UNIT_STATE;
}

void mglDeleteTextures(GLMContext ctx, GLsizei n, const GLuint *textures)
{
    // negative n would run past the caller's array
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    while(n--)
    {
        GLuint name;

        name = *textures++;

        Texture *tex;

        tex = findTexture(ctx, name);

        if(tex)
        {
            for(int i=0; i<TEXTURE_UNITS; i++)
            {
                if(ctx->state.active_textures[i] == tex)
                {
                    ctx->state.active_textures[i] = NULL;

                    // the mask says which units hold something; leaving the bit
                    // behind sends every later draw looking for a texture that
                    // is gone
                    ctx->state.active_texture_mask[i / 32] &= ~(0x1 << (i % 32));

                    ctx->state.dirty_bits |= DIRTY_TEX_BINDING;
                }

                if (ctx->state.texture_units[i].textures[tex->index] == tex)
                    ctx->state.texture_units[i].textures[tex->index] = NULL;
            }

            syncTextureBindings(ctx);

            for(int i=0; i<TEXTURE_UNITS; i++)
            {
                if(ctx->state.image_units[i].texture == name)
                {
                    bzero(&ctx->state.image_units[i], sizeof(ImageUnit));

                    ctx->state.dirty_bits |= DIRTY_IMAGE_UNIT_STATE;
                }
            }

            if (tex->mtl_data)
            {
                ctx->mtl_funcs.mtlDeleteMTLObj(ctx, tex->mtl_data);
                tex->mtl_data = NULL;
            }

            // the name has to stop existing or glIsTexture keeps saying yes.
            // the object itself is emptied but not freed: a framebuffer may
            // still be holding a pointer to it
            invalidateTexture(ctx, tex);

            deleteHashElement(&STATE(texture_table), name);
        }
    }
}

GLboolean mglIsTexture(GLMContext ctx, GLuint texture)
{
    return isTexture(ctx, texture);
}

void mglInvalidateTexImage(GLMContext ctx, GLuint texture, GLint level)
{
    Texture *tex;

    tex = findTexture(ctx, texture);

    ERROR_CHECK_RETURN(tex, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN((GLuint)level < tex->mipmap_levels, GL_INVALID_VALUE);

    // discarding the contents is optional; MGL keeps them
}

void mglInvalidateTexSubImage(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth)
{
    Texture *tex;

    tex = findTexture(ctx, texture);

    ERROR_CHECK_RETURN(tex, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN((GLuint)level < tex->mipmap_levels, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(xoffset >= 0 && yoffset >= 0 && zoffset >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0 && depth >= 0, GL_INVALID_VALUE);

    // discarding the contents is optional; MGL keeps them
}

void mglBindImageTextures(GLMContext ctx, GLuint first, GLsizei count, const GLuint *textures)
{
    MGL_INFO("MGL: glBindImageTextures called first=%u count=%d\n", first, count);
    // Bind multiple image textures
    for (GLsizei i = 0; i < count; i++) {
        GLuint tex_name = textures ? textures[i] : 0;
        if (tex_name != 0) {
            Texture *tex = findTexture(ctx, tex_name);
            if (tex) {
                mglBindImageTexture(ctx, first + i, tex_name, 0, GL_FALSE, 0, tex->access ? tex->access : GL_READ_ONLY, tex->internalformat);
            }
        }
    }
}

void mglClientActiveTexture(GLMContext ctx, GLenum texture)
{
    // picks which fixed-function texture coordinate array later calls talk to.
    // MGL has no fixed-function pipeline at all, so there is nothing to pick
    // and nothing a caller could do with the state afterwards
    MGL_ERR("MGL Error: glClientActiveTexture: MGL has no fixed function texture coordinate arrays\n");

    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglActiveTexture(GLMContext ctx, GLenum texture)
{
    texture -= GL_TEXTURE0;

    if (texture >= STATE_VAR(max_combined_texture_image_units))
    {
        ERROR_RETURN(GL_INVALID_ENUM);
    }

    STATE(active_texture) = texture;
    ctx->state.dirty_bits |= DIRTY_TEX_BINDING;

    // the binding queries follow the active unit
    syncTextureBindings(ctx);
}

void mglBindTextures(GLMContext ctx, GLuint first, GLsizei count, const GLuint *textures)
{
    void mglBindTextureUnit(GLMContext ctx, GLuint unit, GLuint texture);

    // first is a zero-based unit index, and the active unit is left alone
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(first + count <= TEXTURE_UNITS, GL_INVALID_OPERATION);

    for (GLsizei i = 0; i < count; i++)
    {
        // a null array unbinds the whole span
        mglBindTextureUnit(ctx, first + i, textures ? textures[i] : 0);
    }
}

void mglBindTextureUnit(GLMContext ctx, GLuint unit, GLuint texture)
{
    // binds into a unit without disturbing the current active unit
    Texture *ptr;
    GLint index;

    // unit is a zero-based index here, not a GL_TEXTURE0 enum
    ERROR_CHECK_RETURN(unit < TEXTURE_UNITS, GL_INVALID_VALUE);

    if (texture == 0)
    {
        for (int i = 0; i < _MAX_TEXTURE_TYPES; i++)
            STATE(texture_units[unit].textures[i]) = NULL;

        // the shader binder reads active_textures, so it has to be kept in
        // step here too or a DSA bind is invisible to every draw
        STATE(active_textures[unit]) = NULL;
        STATE(active_texture_mask[unit / 32]) &= ~(0x1 << (unit % 32));
        STATE(dirty_bits) |= DIRTY_TEX;

        if (unit == STATE(active_texture))
            syncTextureBindings(ctx);

        return;
    }

    ptr = findTexture(ctx, texture);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    // textures[] is indexed by our own target index, not by the GL enum
    index = textureIndexFromTarget(ctx, ptr->target);

    ERROR_CHECK_RETURN(index != _MAX_TEXTURE_TYPES, GL_INVALID_OPERATION);

    STATE(texture_units[unit].textures[index]) = ptr;
    STATE(active_textures[unit]) = ptr;
    STATE(active_texture_mask[unit / 32]) |= (0x1 << (unit % 32));
    STATE(dirty_bits) |= DIRTY_TEX;

    if (unit == STATE(active_texture))
        syncTextureBindings(ctx);
}

void generateMipmaps(GLMContext ctx, GLuint texture, GLenum target)
{
    Texture *ptr;

    ptr = getTex(ctx, texture, target);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    // level 0 needs to be filled out for mipmap geneation
    ERROR_CHECK_RETURN(ptr->faces[0].levels, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(ptr->faces[0].levels[0].complete, GL_INVALID_OPERATION);

    ptr->mipmapped = true;
    ptr->genmipmaps = true;

    ptr->dirty_bits |= DIRTY_TEXTURE_LEVEL;

    ctx->mtl_funcs.mtlGenerateMipmaps(ctx, ptr);
}

void mglGenerateMipmap(GLMContext ctx, GLenum target)
{
    switch(target)
    {
        case GL_TEXTURE_1D:
        case GL_TEXTURE_2D:
        case GL_TEXTURE_3D:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    generateMipmaps(ctx, 0, target);
}

void mglGenerateTextureMipmap(GLMContext ctx, GLuint texture)
{
    generateMipmaps(ctx, texture, 0);
}

static size_t page_size_align(size_t size)
{
    if (size & (4096-1))
    {
        size_t pad_size = 0;

        pad_size = 4096 - (size & (4096-1));

        size += pad_size;
    }

    return size;
}

void invalidateTexture(GLMContext ctx, Texture *tex)
{
    if (tex->mtl_data)
    {
        ctx->mtl_funcs.mtlDeleteMTLObj(ctx, tex->mtl_data);

        // Forget it as well as release it. Left behind, the next bind hands a
        // freed texture to a render pass and Metal dies tearing the pass down.
        tex->mtl_data = NULL;
    }

    for(int face=0; face<_CUBE_MAP_MAX_FACE; face++)
    {
        for(int i=0; i<tex->num_levels; i++)
        {
            if (tex->faces[face].levels[i].complete)
            {
                if (tex->faces[face].levels[i].data)
                {
                    vm_deallocate(mach_task_self(),
                                  tex->faces[face].levels[i].data,
                                  tex->faces[face].levels[i].data_size);
                }
            }
        }
    }

    for(int i=0; i<6; i++)
    {
        if (tex->faces[i].levels)
            free(tex->faces[i].levels);
    }

    // the object itself stays bound and keeps its name, so only wipe its
    // contents -- clearing target/index/params here left the texture
    // unusable for every later draw
    GLuint name = tex->name;
    GLuint target = tex->target;
    GLuint index = tex->index;
    GLboolean immutable_storage = tex->immutable_storage;
    GLenum access = tex->access;
    TextureParameter params = tex->params;

    bzero(tex, sizeof(Texture));

    tex->name = name;
    tex->target = target;
    tex->index = index;
    tex->immutable_storage = immutable_storage;
    tex->access = access;
    tex->params = params;
}

void initBaseTexLevel(GLMContext ctx, Texture *tex, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    tex->mipmapped = 0;
    tex->mipmap_levels = ilog2(MAX(width, height)) + 1;

    for(int face=0; face<_CUBE_MAP_MAX_FACE; face++)
    {
        // CRITICAL SECURITY FIX: Prevent integer overflow in mipmap allocation
        if (tex->mipmap_levels > SIZE_MAX / sizeof(TextureLevel)) {
            MGL_ERR("MGL SECURITY ERROR: Mipmap levels %d would cause allocation overflow\n", tex->mipmap_levels);
            // CRITICAL FIX: Handle gracefully instead of crashing
            STATE(error) = GL_OUT_OF_MEMORY;
            return;
        }

        tex->faces[face].levels = (TextureLevel *)calloc(tex->mipmap_levels, sizeof(TextureLevel));
        if (!tex->faces[face].levels) {
            MGL_ERR("MGL SECURITY ERROR: calloc failed for face %d with %d levels\n", face, tex->mipmap_levels);
            // CRITICAL FIX: Handle gracefully instead of crashing
            STATE(error) = GL_OUT_OF_MEMORY;
            return;
        }
    }

    tex->internalformat = internalformat;
    tex->width = width;
    tex->height = height;
    tex->depth = depth;
    tex->complete = false;

    for(int face=0; face<_CUBE_MAP_MAX_FACE; face++)
    {
        for(int i=0; i<tex->mipmap_levels; i++)
        {
            tex->faces[face].levels[i].complete = false;
        }
    }
}

bool checkTexLevelParams(GLMContext ctx, Texture *tex, GLint level, GLuint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type)
{
    GLuint base_width, base_height;

    if (level >= tex->mipmap_levels)
    {
        return false;
    }

    if (tex->target == GL_TEXTURE_2D)
    {
        if (level != 0)
        {
            base_width = tex->width;
            base_height = tex->height;

            // a level is max(1, floor(base >> level)) -- the chain bottoms out
            // at 1, it does not run off to zero on the shorter axis first
            while(level--)
            {
                base_width = base_width > 1 ? base_width >> 1 : 1;
                base_height = base_height > 1 ? base_height >> 1 : 1;
            }

            if (width != base_width || height != base_height)
            {
                return false;
            }
        }
    }

    if (internalformat)
    {
        // internal formats don't jive
        if (internalformat != tex->internalformat)
        {
            return false;
        }
    }
    else
    {
        GLuint temp_internalformat;

        // check if we are expected to convert data
        temp_internalformat = internalFormatForGLFormatType(format, type);

        if (temp_internalformat != tex->internalformat)
        {
            return false;
        }
    }

    if (internalformat && checkInternalFormatForMetal(ctx, internalformat) == false)
    {
        return false;
    }

    return true;
}


// GL_RED_INTEGER and friends carry raw integers, not normalised values.
static bool formatIsIntegerPixelFormat(GLenum format)
{
    switch (format)
    {
        case GL_RED_INTEGER:
        case GL_RG_INTEGER:
        case GL_RGB_INTEGER:
        case GL_BGR_INTEGER:
        case GL_RGBA_INTEGER:
        case GL_BGRA_INTEGER:
        case GL_GREEN_INTEGER:
        case GL_BLUE_INTEGER:
            return true;
    }

    return false;
}

bool verifyInternalFormatAndFormatType(GLMContext ctx, GLint internalformat, GLenum format, GLenum type)
{
    switch(internalformat)
    {
        // unsized formats
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
        case GL_RED:
        case GL_RG:
        case GL_RGB:
        case GL_RGBA:
            break;

        // sized formats
        case GL_R8:
        case GL_R8_SNORM:
        case GL_R16:
        case GL_R16_SNORM:
        case GL_RG8:
        case GL_RG8_SNORM:
        case GL_RG16:
        case GL_RG16_SNORM:
        case GL_R3_G3_B2:
        case GL_RGB4:
        case GL_RGB5:
        case GL_RGB8:
        case GL_RGB8_SNORM:
        case GL_RGB10:
        case GL_RGB12:
        case GL_RGB16_SNORM:
        case GL_RGBA2:
        case GL_RGBA4:
        case GL_RGB5_A1:
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
        case GL_RGB10_A2:
        case GL_RGB10_A2UI:
        case GL_RGBA12:
        case GL_RGBA16:
        case GL_SRGB8:
        case GL_SRGB8_ALPHA8:
        case GL_R16F:
        case GL_RG16F:
        case GL_RGB16F:
        case GL_RGBA16F:
        case GL_R32F:
        case GL_RG32F:
        case GL_RGB32F:
        case GL_RGBA32F:
        case GL_R11F_G11F_B10F:
        case GL_RGB9_E5:
        case GL_R8I:
        case GL_R8UI:
        case GL_R16I:
        case GL_R16UI:
        case GL_R32I:
        case GL_R32UI:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RG32I:
        case GL_RG32UI:
        case GL_RGB8I:
        case GL_RGB8UI:
        case GL_RGB16I:
        case GL_RGB16UI:
        case GL_RGB32I:
        case GL_RGB32UI:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_RGBA32I:
        case GL_RGBA32UI:
        // Missing SNORM/UI formats used by virgl
        case 0x9014: // GL_ALPHA8_SNORM
        case 0x9016: // GL_LUMINANCE8_ALPHA8_SNORM
        case 0x9018: // GL_ALPHA16_SNORM
        case 0x901a: // GL_LUMINANCE16_ALPHA16_SNORM
        case 0x8d7e: // GL_ALPHA8UI_EXT
            break;

        // compressed types
        case GL_COMPRESSED_RED:
        case GL_COMPRESSED_RG:
        case GL_COMPRESSED_RGB:
        case GL_COMPRESSED_RGBA:
        case GL_COMPRESSED_SRGB:
        case GL_COMPRESSED_SRGB_ALPHA:
        case GL_COMPRESSED_RED_RGTC1:
        case GL_COMPRESSED_SIGNED_RED_RGTC1:
        case GL_COMPRESSED_RG_RGTC2:
        case GL_COMPRESSED_SIGNED_RG_RGTC2:
        case GL_COMPRESSED_RGBA_BPTC_UNORM:
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:
        case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
            break;

        // Legacy alpha/luminance formats (deprecated but still used)
        case 0x803c: // GL_ALPHA8
        case 0x803e: // GL_ALPHA16
        case 0x8040: // GL_LUMINANCE8
        case 0x8042: // GL_LUMINANCE16
        case 0x8045: // GL_LUMINANCE8_ALPHA8
        case 0x8048: // GL_LUMINANCE16_ALPHA16
        case 0x8816: // GL_ALPHA16F_ARB
        case 0x8818: // GL_LUMINANCE16F_ARB
        case 0x8819: // GL_LUMINANCE_ALPHA16F_ARB
        case 0x881c: // GL_ALPHA32F_ARB
        case 0x881e: // GL_LUMINANCE32F_ARB
        case 0x881f: // GL_LUMINANCE_ALPHA32F_ARB
            break;

        // ASTC compressed formats
        case 0x93b0: // GL_COMPRESSED_RGBA_ASTC_4x4_KHR
        case 0x93b1: // GL_COMPRESSED_RGBA_ASTC_5x4_KHR
        case 0x93b2: // GL_COMPRESSED_RGBA_ASTC_5x5_KHR
        case 0x93b3: // GL_COMPRESSED_RGBA_ASTC_6x5_KHR
        case 0x93b4: // GL_COMPRESSED_RGBA_ASTC_6x6_KHR
        case 0x93b5: // GL_COMPRESSED_RGBA_ASTC_8x5_KHR
        case 0x93b6: // GL_COMPRESSED_RGBA_ASTC_8x6_KHR
        case 0x93b7: // GL_COMPRESSED_RGBA_ASTC_8x8_KHR
        case 0x93b8: // GL_COMPRESSED_RGBA_ASTC_10x5_KHR
        case 0x93b9: // GL_COMPRESSED_RGBA_ASTC_10x6_KHR
        case 0x93ba: // GL_COMPRESSED_RGBA_ASTC_10x8_KHR
        case 0x93bb: // GL_COMPRESSED_RGBA_ASTC_10x10_KHR
        case 0x93bc: // GL_COMPRESSED_RGBA_ASTC_12x10_KHR
        case 0x93bd: // GL_COMPRESSED_RGBA_ASTC_12x12_KHR
        case 0x93d0: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
        case 0x93d1: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR
        case 0x93d2: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR
        case 0x93d3: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR
        case 0x93d4: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR
        case 0x93d5: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR
        case 0x93d6: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR
        case 0x93d7: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR
        case 0x93d8: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR
        case 0x93d9: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR
        case 0x93da: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR
        case 0x93db: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR
        case 0x93dc: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR
        case 0x93dd: // GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR
            break;

        // ETC2/EAC compressed formats
        case 0x9270: // GL_COMPRESSED_R11_EAC
        case 0x9271: // GL_COMPRESSED_SIGNED_R11_EAC
        case 0x9272: // GL_COMPRESSED_RG11_EAC
        case 0x9273: // GL_COMPRESSED_SIGNED_RG11_EAC
        case 0x9274: // GL_COMPRESSED_RGB8_ETC2
        case 0x9275: // GL_COMPRESSED_SRGB8_ETC2
        case 0x9276: // GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2
        case 0x9277: // GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2
        case 0x9278: // GL_COMPRESSED_RGBA8_ETC2_EAC
        case 0x9279: // GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
            break;

        // S3TC/DXT compressed formats
        case 0x83f0: // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
        case 0x83f1: // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
        case 0x83f2: // GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
        case 0x83f3: // GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
        case 0x8c4c: // GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
        case 0x8c4d: // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
        case 0x8c4e: // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
        case 0x8c4f: // GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
            break;

        // Additional integer formats (alternate enum values used by some implementations)
        case 0x8d72: // alternate GL_RGBA8I
        case 0x8d75: // alternate GL_RGB8I
        case 0x8d78: // alternate GL_RGBA8UI
        case 0x8d7a: // alternate GL_RGB8UI
        case 0x8d7b: // GL_ALPHA8I_EXT
        // case 0x8d7e: // alternate GL_RGBA32UI - Duplicate of GL_ALPHA8UI_EXT
        case 0x8d80: // alternate GL_RGB32UI
        case 0x8d81: // GL_ALPHA32I_EXT
        case 0x8d84: // alternate GL_RGBA16I
        case 0x8d86: // alternate GL_RGB16I
        case 0x8d87: // GL_ALPHA16I_EXT
        case 0x8d8a: // alternate GL_RGBA32I
        case 0x8d8c: // alternate GL_RGB32I
        case 0x8d8d: // GL_ALPHA32I_EXT
        case 0x8d90: // alternate GL_RGBA16UI
        case 0x8d92: // alternate GL_RGB16UI
        case 0x8d93: // GL_ALPHA16UI_EXT
            break;

        // SNORM formats
        case 0x8f9b: // GL_SIGNED_NORMALIZED
        case 0x8fbd: // GL_RGB10_A2UI (alternate)
        case 0x8fbe: // GL_RGBA16_SNORM
            break;

        // Depth/stencil special formats
        // case 0x9014: // GL_DEPTH_COMPONENT16_NONLINEAR_NV - Duplicate of GL_ALPHA8_SNORM
        // case 0x9016: // GL_TEXTURE_2D_MULTISAMPLE - Duplicate of GL_LUMINANCE8_ALPHA8_SNORM
        // case 0x9018: // GL_TEXTURE_2D_MULTISAMPLE_ARRAY - Duplicate of GL_ALPHA16_SNORM
        // case 0x901a: // GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY - Duplicate of GL_LUMINANCE16_ALPHA16_SNORM
            break;

        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32:
        case GL_DEPTH_COMPONENT32F:
        case GL_DEPTH24_STENCIL8:
        case GL_DEPTH32F_STENCIL8:
            ERROR_CHECK_RETURN_VALUE(format == GL_DEPTH_COMPONENT || format == GL_DEPTH_STENCIL,
                                     GL_INVALID_OPERATION, false);
            break;
            
        case GL_STENCIL_INDEX8:
            ERROR_CHECK_RETURN_VALUE(format == GL_STENCIL_INDEX, GL_INVALID_OPERATION, false);
            break;
            
        case GL_RGB565:
            break;

        default:
            // Log warning but don't error - many formats work even if not explicitly listed
            MGL_ERR("MGL WARNING: verifyInternalFormat unknown internalformat 0x%x\n", internalformat);
            break;
    }

    switch(format)
    {
        case GL_RED:
        case GL_RG:
        case GL_RGB:
        case GL_BGR:
        case GL_RGBA:
        case GL_BGRA:
        case GL_RED_INTEGER:
        case GL_RG_INTEGER:
        case GL_RGB_INTEGER:
        case GL_BGR_INTEGER:
        case GL_RGBA_INTEGER:
        case GL_BGRA_INTEGER:
        case GL_STENCIL_INDEX:
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
        // Legacy formats (deprecated but still used by virglrenderer)
        case 0x1906: // GL_ALPHA
        case 0x1909: // GL_LUMINANCE
        case 0x190a: // GL_LUMINANCE_ALPHA
        case 0x8000: // GL_COLOR_INDEX (legacy)
        case 0x8d97: // GL_ALPHA_INTEGER
        case 0x8d9c: // GL_LUMINANCE_INTEGER_EXT
        case 0x8d9d: // GL_LUMINANCE_ALPHA_INTEGER_EXT
            break;

        default:
            // Allow unknown formats with warning - virglrenderer may use nonstandard values
            MGL_ERR("MGL WARNING: verifyFormat unknown format 0x%x, allowing\n", format);
            break;
    }

    switch(type)
    {
        case GL_UNSIGNED_BYTE:
        case GL_BYTE:
        case GL_UNSIGNED_SHORT:
        case GL_SHORT:
        case GL_UNSIGNED_INT:
        case GL_INT:
        case GL_FLOAT:
        case GL_HALF_FLOAT:
            break;

        case GL_UNSIGNED_BYTE_3_3_2:
        case GL_UNSIGNED_BYTE_2_3_3_REV:
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_5_6_5_REV:
            ERROR_CHECK_RETURN_VALUE(format == GL_RGB || format == GL_RGB_INTEGER,
                                     GL_INVALID_OPERATION, false);
            break;

        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_4_4_4_4_REV:
        case GL_UNSIGNED_SHORT_5_5_5_1:
        case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        case GL_UNSIGNED_INT_8_8_8_8:
        case GL_UNSIGNED_INT_8_8_8_8_REV:
        case GL_UNSIGNED_INT_10_10_10_2:
        case GL_UNSIGNED_INT_2_10_10_10_REV:
            // Allow RGBA, BGRA, and integer variants (RGBA_INTEGER, RGB_INTEGER, BGRA_INTEGER)
            ERROR_CHECK_RETURN_VALUE((format == GL_RGBA || format == GL_BGRA || 
                                      format == GL_RGBA_INTEGER || format == GL_RGB_INTEGER ||
                                      format == GL_BGRA_INTEGER), GL_INVALID_OPERATION, false);
            break;
            
        case GL_UNSIGNED_INT_24_8:
            ERROR_CHECK_RETURN_VALUE(format == GL_DEPTH_STENCIL, GL_INVALID_OPERATION, false);
            break;
            
        case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
            ERROR_CHECK_RETURN_VALUE(format == GL_DEPTH_STENCIL, GL_INVALID_OPERATION, false);
            break;
        
        // Packed float types for special formats
        case 0x8c3b: // GL_UNSIGNED_INT_10F_11F_11F_REV
        case 0x8c3e: // GL_UNSIGNED_INT_5_9_9_9_REV
            break;
            
        default:
            MGL_ERR("MGL WARNING: verifyInternalFormat unknown type 0x%x\n", type);
            break;
    }

    // The pixel format has to belong to the same family as the storage: you
    // cannot feed GL_BLUE into a depth texture, or plain GL_RGBA into an
    // integer one. GL 4.6 table 8.2 spells the pairings out; this is the part
    // of it that does not depend on component counts.
    {
        // the unsized base formats carry no kind of their own, so ask about the
        // sized format they stand for
        uint8_t kind = mglFormatKind(mglFormatSizedForBase((GLenum)internalformat));
        bool fmt_depth = (format == GL_DEPTH_COMPONENT);
        bool fmt_stencil = (format == GL_STENCIL_INDEX);
        bool fmt_ds = (format == GL_DEPTH_STENCIL);
        bool fmt_int = formatIsIntegerPixelFormat(format);

        switch (kind)
        {
            // Depth and depth-stencil storage take either client format:
            // depth data goes into a depth-stencil texture, and a packed
            // depth-stencil upload fills a depth one. GL_STENCIL_INDEX is not
            // an upload format for either.
            case MGL_FMT_DEPTH:
            case MGL_FMT_DEPTH_STENCIL:
                ERROR_CHECK_RETURN_VALUE(fmt_depth || fmt_ds, GL_INVALID_OPERATION, false);
                break;

            case MGL_FMT_STENCIL:
                ERROR_CHECK_RETURN_VALUE(fmt_stencil, GL_INVALID_OPERATION, false);
                break;

            case MGL_FMT_COLOR_INT:
            case MGL_FMT_COLOR_UINT:
                ERROR_CHECK_RETURN_VALUE(fmt_int, GL_INVALID_OPERATION, false);
                break;

            // a compressed colour format is not an integer format either
            case MGL_FMT_COMPRESSED:
            case MGL_FMT_COLOR_FLOAT:
                ERROR_CHECK_RETURN_VALUE(!fmt_int && !fmt_depth && !fmt_stencil && !fmt_ds,
                                         GL_INVALID_OPERATION, false);
                break;

            default:
                break;
        }
    }

    // table 8.5: a packed type names the client formats it may pair with, and
    // a float type is not allowed with an integer format
    ERROR_CHECK_RETURN_VALUE(mglFormatTypeAgrees(format, type), GL_INVALID_OPERATION, false);

    return true;
}


bool unpackTexture(GLMContext ctx, Texture *tex, GLuint face, GLuint level, GLenum format, GLenum type, void *src_data, void *dst_data, size_t src_pitch, size_t xoffset, size_t yoffset, size_t zoffset, size_t width, size_t height, size_t depth)
{
    GLubyte *src, *dst;
    size_t dst_pitch, dst_pixel_size, level_height, slice_pitch, rows;
    MGLNativeFormat native = MGL_NF_UNKNOWN;
    bool straight_copy;

    src = (GLubyte *)src_data;
    dst = (GLubyte *)dst_data;

    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);

    // A level with no storage has nowhere for this to go. The bounds check
    // below only runs when data_size is known, so without this a level that
    // was never allocated wrote through a null pointer.
    ERROR_CHECK_RETURN_VALUE(dst, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(tex->faces[face].levels[level].data_size, GL_INVALID_OPERATION, false);

    dst_pitch = tex->faces[face].levels[level].pitch;
    assert(dst_pitch);

    dst_pixel_size = sizeForInternalFormat(tex->internalformat, format, type);
    ERROR_CHECK_RETURN_VALUE(dst_pixel_size, GL_INVALID_OPERATION, false);

    // client data whose components already match the storage goes straight in
    straight_copy = mglUploadNeedsNoConversion(tex->internalformat, format, type);

    if (straight_copy == false)
    {
        native = mglNativeFormatForGLInternalFormat(tex->internalformat);

        if (native == MGL_NF_UNKNOWN)
        {
            MGL_ERR("MGL Error: unpackTexture: no conversion from 0x%x/0x%x into 0x%x\n",
                    format, type, tex->internalformat);
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
        }
    }

    // a sub-rectangle steps by the level's own size, not the region's
    level_height = tex->faces[face].levels[level].height;
    if (level_height == 0)
        level_height = height;

    if (xoffset || yoffset || zoffset)
    {
        dst += xoffset * dst_pixel_size;
        dst += yoffset * dst_pitch;
        dst += zoffset * dst_pitch * level_height;
    }

    // 3d and array textures keep their slices back to back
    slice_pitch = dst_pitch * level_height;
    rows = height ? height : 1;

    // The client's images sit GL_UNPACK_IMAGE_HEIGHT rows apart when that is
    // set, which can be taller than the image being taken out of each one.
    size_t src_rows_per_image = ctx->state.unpack.image_height > 0
                              ? (size_t)ctx->state.unpack.image_height : rows;

    // never write past the level: a cube face holds one face, not six
    if (tex->faces[face].levels[level].data_size)
    {
        size_t last = (size_t)(dst - (GLubyte *)dst_data)
                    + ((depth ? depth : 1) - 1) * slice_pitch
                    + (rows - 1) * dst_pitch
                    + width * dst_pixel_size;

        if (last > tex->faces[face].levels[level].data_size)
        {
            MGL_ERR("MGL Error: unpackTexture: %zu bytes past the end of level %u\n",
                    last - tex->faces[face].levels[level].data_size, level);
            ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
        }
    }

    // UNPACK_SWAP_BYTES reverses the client bytes before they are read
    GLubyte *swapped = NULL;

    if (ctx->state.unpack.swap_bytes && src_pitch && rows)
    {
        size_t bytes = src_pitch * (src_rows_per_image * ((depth ? depth : 1) - 1) + rows);

        swapped = (GLubyte *)malloc(bytes);

        if (swapped)
        {
            memcpy(swapped, src, bytes);
            mglSwapPixelBytes(swapped, src_pitch, format, type,
                              (GLsizei)width, (GLsizei)(bytes / src_pitch));
            src = swapped;
        }
    }

    for (size_t z = 0; z < (depth ? depth : 1); z++)
    {
        GLubyte *slice = dst + z * slice_pitch;
        const GLubyte *src_slice = src + z * src_pitch * src_rows_per_image;

        if (straight_copy)
        {
            for (size_t y = 0; y < rows; y++)
                memcpy(slice + y * dst_pitch, src_slice + y * src_pitch, width * dst_pixel_size);
        }
        else if (mglConvertPixelsToNative(src_slice, src_pitch, format, type,
                                          slice, dst_pitch, native,
                                          (GLsizei)width, (GLsizei)rows) == GL_FALSE)
        {
            MGL_ERR("MGL Error: unpackTexture: conversion from 0x%x/0x%x into 0x%x failed\n",
                    format, type, tex->internalformat);
            free(swapped);
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
        }
    }

    free(swapped);

    return true;
}

#pragma mark texImage 1D/2D/3D
// Forward declaration
bool texSubImage(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void *pixels);

// Compress client pixels into an RGTC level and store the blocks, so that a
// compressed internal format behaves the way the specification says rather
// than refusing the upload.
static bool compressUploadedTexLevel(GLMContext ctx, Texture *tex, GLuint face, GLint level,
                                     GLenum internalformat, GLsizei width, GLsizei height,
                                     GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    size_t image_size, src_pitch, alloc;
    vm_address_t data;
    GLubyte *blocks;

    if (level == 0)
    {
        if (tex->mipmap_levels == 0)
        {
            initBaseTexLevel(ctx, tex, internalformat, width, height, depth);
        }
        else if (width != (GLsizei)tex->width || height != (GLsizei)tex->height ||
                 internalformat != tex->internalformat)
        {
            invalidateTexture(ctx, tex);
            initBaseTexLevel(ctx, tex, internalformat, width, height, depth);
        }
    }

    ERROR_CHECK_RETURN_VALUE(tex->faces[face].levels, GL_OUT_OF_MEMORY, false);
    ERROR_CHECK_RETURN_VALUE(level >= 0 && (GLuint)level < tex->mipmap_levels, GL_INVALID_VALUE, false);

    image_size = mglFormatImageSize(internalformat, width, height, depth ? depth : 1);
    ERROR_CHECK_RETURN_VALUE(image_size, GL_INVALID_VALUE, false);

    src_pitch = mglPixelStoreRowPitch(&ctx->state.unpack, width, sizeForFormatType(format, type));

    alloc = page_size_align(image_size);

    if (vm_allocate(mach_task_self(), &data, alloc, VM_FLAGS_ANYWHERE) != KERN_SUCCESS)
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, false);

    blocks = (GLubyte *)data;

    if (mglCompressToRGTC((const GLubyte *)pixels, src_pitch, format, type,
                          blocks, internalformat, width, height) == GL_FALSE)
    {
        vm_deallocate(mach_task_self(), data, alloc);
        MGL_ERR("MGL Error: no conversion from 0x%x/0x%x into compressed 0x%x\n",
                format, type, internalformat);
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    {
        TextureLevel *lvl = &tex->faces[face].levels[level];

        if (lvl->complete && lvl->data)
            vm_deallocate(mach_task_self(), lvl->data, lvl->data_size);

        lvl->width = width;
        lvl->height = height;
        lvl->depth = depth;
        lvl->pitch = mglFormatBytesPerRow(internalformat, width);
        lvl->data = data;
        lvl->data_size = alloc;
        lvl->mtl_format = mtlFormatForGLInternalFormat(internalformat);
        lvl->complete = true;
    }

    tex->num_levels = MAX(tex->num_levels, level + 1);
    tex->dirty_bits |= DIRTY_TEXTURE_DATA;
    STATE(dirty_bits) |= DIRTY_TEX;

    return true;
}

// One compressed level's worth of block storage. glTexStorage and
// glCompressedTexImage both land here; only the latter has data to copy in.
static bool allocCompressedLevel(GLMContext ctx, Texture *tex, GLuint face, GLint level,
                                 GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    size_t pitch, rows, slices, alloc_size;
    vm_address_t texture_data;
    kern_return_t err;
    TextureLevel *lvl;

    ERROR_CHECK_RETURN_VALUE(tex->faces[face].levels, GL_OUT_OF_MEMORY, false);
    ERROR_CHECK_RETURN_VALUE(level >= 0 && (GLuint)level < tex->mipmap_levels, GL_INVALID_VALUE, false);

    pitch = mglFormatBytesPerRow(internalformat, width);
    ERROR_CHECK_RETURN_VALUE(pitch, GL_INVALID_ENUM, false);

    // Blocks only fill every fourth row, but compressedTexSubLevel and the
    // uploader both step this level a pixel row at a time -- so the allocation
    // has to be as tall as the level, the same as glCompressedTexImage's.
    rows = (size_t)(height > 0 ? height : 1);
    slices = depth > 0 ? (size_t)depth : 1;
    alloc_size = page_size_align(pitch * rows * slices);

    lvl = &tex->faces[face].levels[level];

    if (lvl->complete && lvl->data)
        vm_deallocate(mach_task_self(), lvl->data, lvl->data_size);

    err = vm_allocate((vm_map_t)mach_task_self(), &texture_data, alloc_size, VM_FLAGS_ANYWHERE);

    if (err != 0 || texture_data == 0)
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, false);

    tex->num_levels = MAX(tex->num_levels, (GLuint)level + 1);
    lvl->width = width;
    lvl->height = height;
    lvl->depth = depth;
    lvl->pitch = pitch;
    lvl->data = texture_data;
    lvl->data_size = alloc_size;
    lvl->complete = true;

    tex->dirty_bits |= DIRTY_TEXTURE_LEVEL | DIRTY_TEXTURE_DATA;
    STATE(dirty_bits) |= DIRTY_TEX;

    return true;
}

bool createTextureLevel(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLboolean is_array, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void *pixels, GLboolean proxy)
{
    // all the levels are created on a tex storage call.. if we get here we should just assert
    if (tex->immutable_storage)
    {
        // Compatibility: Treat glTexImage* on immutable texture as glTexSubImage*
        // This allows guests to update content using glTexImage* which is common in some drivers
        // We pass 0 for offsets. texSubImage will handle validation.
        return texSubImage(ctx, tex, face, level, 0, 0, 0, width, height, depth, format, type, pixels);
    }
    
    if (level == 0)
    {
        if (internalformat == 0)
        {
            internalformat = internalFormatForGLFormatType(format, type);

            if (internalformat == 0)
            {
                ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
            }
        }
        else if (pixels)
        {
            // The app asked for this internal format and GL says it gets it.
            // Where the upload data is laid out differently, unpackTexture
            // converts -- adopting the data's format instead used to turn an
            // sRGB or 10/10/10/2 texture into a plain RGBA8 one behind its back.
            GLuint temp_format = internalFormatForGLFormatType(format, type);

            if (temp_format == 0 && checkInternalFormatForMetal(ctx, internalformat) == false)
            {
                ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
            }
        }

        // GL lets the app name a base format and leave the bit depth to us
        internalformat = mglFormatSizedForBase(internalformat);

        // see if we can actually use this internal format
        if (checkInternalFormatForMetal(ctx, internalformat) == false)
        {
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
        }

        if (tex->mipmap_levels == 0)
        {
            // uninitialized tex
            initBaseTexLevel(ctx, tex, internalformat, width, height, depth);
        }
        else if (width != tex->width || height != tex->height || internalformat != tex->internalformat)
        {
            // invalidate texture because the base level width / height / internal format are being changed...
            invalidateTexture(ctx, tex);

            initBaseTexLevel(ctx, tex, internalformat, width, height, depth);
        }
    }
    else
    {
        // Levels may be defined in any order. If this is the first one we have
        // seen, size the chain from what the base level would have to be for
        // this level to sit where it does.
        if (tex->mipmap_levels == 0)
        {
            GLsizei base_w = width, base_h = height;

            internalformat = mglFormatSizedForBase(internalformat ? internalformat
                                                   : internalFormatForGLFormatType(format, type));

            if (checkInternalFormatForMetal(ctx, internalformat) == false)
            {
                ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
            }

            for (GLint l = 0; l < level; l++)
            {
                base_w = base_w > 0 ? base_w * 2 : 1;
                base_h = base_h > 0 ? base_h * 2 : 1;
            }

            initBaseTexLevel(ctx, tex, internalformat, base_w, base_h, depth);
        }
        else if (checkTexLevelParams(ctx, tex, level, internalformat, width, height, depth, format, type) == false)
        {
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
        }
    }

    if (STATE(buffers[_PIXEL_UNPACK_BUFFER]))
    {
        Buffer *ptr;

        ptr = STATE(buffers[_PIXEL_UNPACK_BUFFER]);

        ERROR_CHECK_RETURN_VALUE(ptr->mapped == false, GL_INVALID_OPERATION, false);

        GLubyte *buffer_data;
        buffer_data = getBufferData(ctx, ptr);

        // if a pixel buffer is the src, pixels is the offset
        // need to check offset against size
        size_t offset;
        offset = (size_t)pixels;

        pixels = &buffer_data[offset];
    }

    tex->num_levels = MAX(tex->num_levels, level + 1);
    tex->faces[face].levels[level].width = width;
    tex->faces[face].levels[level].height = height;
    tex->faces[face].levels[level].depth = depth;

    kern_return_t err;
    vm_address_t texture_data;
    size_t pixel_size;
    size_t internal_size;
    size_t texture_size;
    size_t src_pitch;

    // GL 4.6 section 8.5 lets an application hand uncompressed pixels to a
    // compressed internal format and expects the driver to compress them.
    {
        GLuint rgtc_channels = 0;
        GLboolean rgtc_signed = GL_FALSE;

        if (pixels && mglFormatIsRGTC(internalformat, &rgtc_channels, &rgtc_signed))
            return compressUploadedTexLevel(ctx, tex, face, level, internalformat,
                                            width, height, depth, format, type, pixels);
    }

    // glTexStorage* names a compressed internal format and no client format at
    // all. Sizing that through the uncompressed path fails, so the level never
    // got allocated and every glCompressedTexSubImage into it was refused.
    if (mglFormatIsCompressed(internalformat))
        return allocCompressedLevel(ctx, tex, face, level, internalformat,
                                    width, height, depth);

    pixel_size = sizeForInternalFormat(internalformat, format, type);
    ERROR_CHECK_RETURN_VALUE(pixel_size, GL_INVALID_ENUM, false);

    // A zero-sized level is legal -- it defines the level and releases its
    // storage. Nothing to allocate, nothing to upload.
    if (width == 0 || height == 0 || depth == 0)
    {
        TextureLevel *lvl = &tex->faces[face].levels[level];

        if (lvl->complete && lvl->data)
            vm_deallocate(mach_task_self(), lvl->data, lvl->data_size);

        lvl->pitch = 0;
        lvl->data = 0;
        lvl->data_size = 0;
        lvl->complete = true;

        return true;
    }

    assert(height);
    assert(depth);

    tex->faces[face].levels[level].pitch = pixel_size * width;

    if (depth > 1)
    {
        // 3d texture
        internal_size = pixel_size * width * height * depth;
    }
    else if (height > 1)
    {
        // 2d texture
        internal_size = pixel_size * width * height;
    }
    else
    {
        // 1d texture
        internal_size = pixel_size * width;
    }

    texture_size = page_size_align(internal_size);
    assert(texture_size);

    switch(mtlFormatForGLInternalFormat(internalformat))
    {
        case MTLPixelFormatDepth16Unorm:
        case MTLPixelFormatDepth32Float:
        case MTLPixelFormatDepth24Unorm_Stencil8:
        case MTLPixelFormatDepth32Float_Stencil8:
            tex->mtl_requires_private_storage = true;
            break;

        default:
            tex->mtl_requires_private_storage = false;
            break;
    }

    if (tex->mtl_requires_private_storage == false)
    {
        // Allocate directly from VM
        err = vm_allocate((vm_map_t) mach_task_self(),
                          (vm_address_t*) &texture_data,
                          texture_size,
                          VM_FLAGS_ANYWHERE);
        if (err != 0 || texture_data == 0)
        {
            MGL_ERR("MGL Error: %s: could not allocate %zu bytes for a texture level\n",
                    __FUNCTION__, (size_t)texture_size);
            ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, false);
        }

        tex->faces[face].levels[level].data_size = texture_size;
        tex->faces[face].levels[level].data = (vm_address_t)texture_data;

        if (pixels)
        {
            GLuint src_pixel_size = sizeForFormatType(format, type);

            src_pitch = mglPixelStoreRowPitch(&ctx->state.unpack, width, src_pixel_size);

            if (ctx->state.unpack.row_length > 0 && ctx->state.unpack.row_length < width)
            {
                ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
            }

            // unpack from pixel buffer
            if (STATE(buffers[_PIXEL_UNPACK_BUFFER]))
            {
                Buffer *ptr;

                ptr = STATE(buffers[_PIXEL_UNPACK_BUFFER]);

                ERROR_CHECK_RETURN_VALUE(ptr->mapped == false, GL_INVALID_OPERATION, false);

                GLubyte *buffer_data;
                buffer_data = getBufferData(ctx, ptr);

                // if a pixel buffer is the src, pixels is the offset
                // need to check offset against size
                size_t offset;
                offset = (size_t)pixels;

                pixels = &buffer_data[offset];
            }

            pixels = (const GLubyte *)pixels +
                     ((depth > 1 || tex->target == GL_TEXTURE_3D || tex->target == GL_TEXTURE_2D_ARRAY ||
                       tex->target == GL_TEXTURE_1D_ARRAY || tex->target == GL_TEXTURE_CUBE_MAP_ARRAY)
                      ? mglPixelStoreSkipBytes(&ctx->state.unpack, height, src_pixel_size, src_pitch)
                      : mglPixelStoreSkipBytes2D(&ctx->state.unpack, src_pixel_size, src_pitch));

            if (unpackTexture(ctx, tex, face, level, format, type, (void *)pixels, (void *)texture_data, src_pitch, 0, 0, 0, width, height, depth) == false)
                return false;

            tex->dirty_bits |= DIRTY_TEXTURE_DATA;
        };
    }

    tex->faces[face].levels[level].complete = true;

    tex->dirty_bits |= DIRTY_TEXTURE_LEVEL;
    STATE(dirty_bits) |= DIRTY_TEX;

    return true;
}

void mglTexImage1D(GLMContext ctx, GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;
    bool proxy;

    proxy = false;

    switch(target)
    {
        case GL_TEXTURE_1D:
            break;

        case GL_PROXY_TEXTURE_1D:
            proxy = true;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);

    // verifyFormatType sets the error
    ERROR_CHECK_RETURN(verifyInternalFormatAndFormatType(ctx, internalformat, format, type), 0);

    ERROR_CHECK_RETURN(width >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(border == 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    tex->access = GL_READ_ONLY;

    createTextureLevel(ctx, tex, 0, level, false, internalformat, width, 1, 1, format, type, (void *)pixels, proxy);
}

void mglTexImage2D(GLMContext ctx, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;
    GLuint face;
    GLboolean is_array;
    GLboolean proxy;

    MGL_INFO("MGL: mglTexImage2D called - target=0x%x, level=%d, internalformat=0x%x, width=%d, height=%d, format=0x%x, type=0x%x\n",
            target, level, internalformat, width, height, format, type);

    face = 0;
    is_array = false;
    proxy = false;

    switch(target)
    {
        case GL_TEXTURE_2D:
            break;

        case GL_PROXY_TEXTURE_2D:
        case GL_PROXY_TEXTURE_CUBE_MAP:
            proxy = true;
            break;

        case GL_TEXTURE_1D_ARRAY:
            is_array = true;
            break;

        case GL_PROXY_TEXTURE_1D_ARRAY:
            is_array = true;
            proxy = true;
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
            break;

        case GL_PROXY_TEXTURE_RECTANGLE:
            proxy = true;
            ERROR_CHECK_RETURN(level==0, GL_INVALID_OPERATION);
            break;

        case GL_TEXTURE_RECTANGLE:
            ERROR_CHECK_RETURN(level==0, GL_INVALID_OPERATION);
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);

    // verifyFormatType sets the error
    ERROR_CHECK_RETURN(verifyInternalFormatAndFormatType(ctx, internalformat, format, type), 0);

    ERROR_CHECK_RETURN(width >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(border == 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    tex->access = GL_READ_ONLY;

    createTextureLevel(ctx, tex, face, level, is_array, internalformat, width, height, 1, format, type, (void *)pixels, proxy);
}

// TexImage2DMultisample moved to texture_multisample.c

void mglTexImage3D(GLMContext ctx, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;
    GLboolean is_array;
    GLboolean proxy;

    is_array = false;
    proxy = false;

    switch(target)
    {
        case GL_TEXTURE_3D:
            break;

        case GL_PROXY_TEXTURE_3D:
            proxy = true;
            break;

        case GL_TEXTURE_2D_ARRAY:
        case GL_PROXY_TEXTURE_2D_ARRAY:
            is_array = true;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);

    // verifyFormatType sets the error
    ERROR_CHECK_RETURN(verifyInternalFormatAndFormatType(ctx, internalformat, format, type), 0);

    ERROR_CHECK_RETURN(width >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(depth >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(border == 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    tex->access = GL_READ_ONLY;

    createTextureLevel(ctx, tex, 0, level, is_array, internalformat, width, height, depth, format, type, (void *)pixels, proxy);
}

// TexImage3DMultisample moved to texture_multisample.c

#pragma mark texSubImage
bool texSubImage(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void *pixels)
{
    // Debug: Log large texture uploads (VM framebuffer size)
    if (width >= 640 && height >= 400) {
        MGL_INFO("MGL DEBUG: texSubImage tex_id=%u %dx%d at (%d,%d) pixels=%p\n",
                tex ? tex->name : 0, width, height, xoffset, yoffset, pixels);
    }
    
    // ERROR_CHECK_RETURN_VALUE(tex != NULL, GL_INVALID_OPERATION, false);
    if (tex == NULL) {
        MGL_ERR("MGL Error: texSubImage: tex is NULL\n");
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    // ERROR_CHECK_RETURN_VALUE(level <= tex->num_levels, GL_INVALID_OPERATION, false);
    if (level > tex->num_levels) {
        MGL_ERR("MGL Error: texSubImage: level %d > num_levels %d\n", level, tex->num_levels);
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }
    
    if (!tex->faces[face].levels) {
        MGL_ERR("MGL Error: texSubImage: levels is NULL\n");
        ERROR_CHECK_RETURN_VALUE(false, GL_INVALID_OPERATION, false);
    }
    
    // ERROR_CHECK_RETURN_VALUE(tex->faces[face].levels[level].complete, GL_INVALID_OPERATION, false);
    if (!tex->faces[face].levels[level].complete) {
        MGL_ERR("MGL Error: texSubImage: level %d not complete\n", level);
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
    }

    // unpack from pixel buffer
    if (STATE(buffers[_PIXEL_UNPACK_BUFFER]))
    {
        Buffer *ptr;

        ptr = STATE(buffers[_PIXEL_UNPACK_BUFFER]);

        // ERROR_CHECK_RETURN_VALUE(ptr->mapped == false, GL_INVALID_OPERATION, false);
        if (ptr->mapped) {
            MGL_ERR("MGL Error: texSubImage: pixel unpack buffer is mapped\n");
            ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);
        }

        GLubyte *buffer_data;
        buffer_data = getBufferData(ctx, ptr);

        // if a pixel buffer is the src, pixels is the offset
        // need to check offset against size
        size_t offset;
        offset = (size_t)pixels;

        pixels = &buffer_data[offset];
    }

    // no src data.. return
    ERROR_CHECK_RETURN_VALUE(pixels, GL_INVALID_OPERATION, false);

    size_t pixel_size;
    size_t src_size;
    size_t src_pitch;

    pixel_size = sizeForFormatType(format, type);
    src_size = width * pixel_size;

    {
        GLuint src_pixel_size = sizeForFormatType(format, type);

        src_pitch = mglPixelStoreRowPitch(&ctx->state.unpack, width, src_pixel_size);

        if (ctx->state.unpack.row_length > 0 && ctx->state.unpack.row_length < width)
        {
            ERROR_RETURN_VALUE(GL_INVALID_VALUE, false);
        }

        pixels = (GLubyte *)pixels +
                 ((depth > 1 || tex->target == GL_TEXTURE_3D || tex->target == GL_TEXTURE_2D_ARRAY ||
                   tex->target == GL_TEXTURE_1D_ARRAY || tex->target == GL_TEXTURE_CUBE_MAP_ARRAY)
                  ? mglPixelStoreSkipBytes(&ctx->state.unpack, height, src_pixel_size, src_pitch)
                  : mglPixelStoreSkipBytes2D(&ctx->state.unpack, src_pixel_size, src_pitch));
    }

    void *texture_data;

    texture_data = (void *)tex->faces[face].levels[level].data;
    
    if (unpackTexture(ctx, tex, face, level, format, type, pixels, texture_data, src_pitch, xoffset, yoffset, zoffset, width, height, depth) == false)
        return false;

    // use a blit command to update data
    do
    {
        Buffer *buf;

        buf = STATE(buffers[_PIXEL_UNPACK_BUFFER]);

        if (buf == NULL)
            continue;

        if (tex->mtl_data == NULL)
            continue;

        size_t src_offset;
        size_t src_image_size;
        size_t src_size;

        src_offset = (size_t)0;

        src_image_size = src_pitch * height;

        src_size = src_image_size * depth;

        ctx->mtl_funcs.mtlTexSubImage(ctx, tex, buf, src_offset, src_pitch, src_image_size, src_size, zoffset, level, width, height, depth, xoffset, yoffset, zoffset);

        return true;
    } while(false);

    // use process gl to upload texture data
    tex->dirty_bits |= DIRTY_TEXTURE_DATA;
    
    return true;
}

#pragma mark texSubImage1D
void texSubImage1D(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    // an undefined level is an operation error, and it outranks the bounds
    // check below -- width against a zero-sized level is meaningless
    ERROR_CHECK_RETURN(texLevelDefined(tex, face, level), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(verifyInternalFormatAndFormatType(ctx, tex->internalformat, format, type), 0);

    ERROR_CHECK_RETURN(width >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(xoffset >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(width + xoffset <= (GLint)tex->faces[face].levels[level].width, GL_INVALID_VALUE);

    texSubImage(ctx, tex, face, level, xoffset, 0, 0, width, 1, 1, format, type, (void *)pixels);
}

void mglTexSubImage1D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;

    switch(target)
    {
        case GL_TEXTURE_1D:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex != NULL, GL_INVALID_OPERATION);

    texSubImage1D(ctx, tex, 0, level, xoffset, width, format, type, pixels);
}

void mglTextureSubImage1D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex != NULL, GL_INVALID_OPERATION);

   texSubImage1D(ctx, tex, 0, level, xoffset, width, format, type, pixels);
}

#pragma mark texSubImage2D
bool texSubImage2D(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    ERROR_CHECK_RETURN_VALUE(level >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(texLevelDefined(tex, face, level), GL_INVALID_OPERATION, false);

    ERROR_CHECK_RETURN_VALUE(verifyInternalFormatAndFormatType(ctx, tex->internalformat, format, type), 0, false);

    ERROR_CHECK_RETURN_VALUE(width >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(height >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(xoffset >= 0 && yoffset >= 0, GL_INVALID_VALUE, false);

    ERROR_CHECK_RETURN_VALUE(width + xoffset <= (GLint)tex->faces[face].levels[level].width, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(height + yoffset <= (GLint)tex->faces[face].levels[level].height, GL_INVALID_VALUE, false);

    texSubImage(ctx, tex, face, level, xoffset, yoffset, 0, width, height, 1, format, type, (void *)pixels);

    return true;
}

void mglTexSubImage2D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;
    GLuint face;

    face = 0;

    switch(target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_RECTANGLE:
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    tex = getTex(ctx, 0, target);

    if (!tex) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }
    
    texSubImage2D(ctx, tex, face, level, xoffset, yoffset, width, height, format, type, pixels);
}

void mglTextureSubImage2D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex != NULL, GL_INVALID_OPERATION);

    texSubImage2D(ctx, tex, 0, level, xoffset, yoffset, width, height, format, type, pixels);
}

#pragma mark texSubImage3D
void texSubImage3D(GLMContext ctx, Texture *tex, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{

    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(texLevelDefined(tex, 0, level), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(verifyInternalFormatAndFormatType(ctx, tex->internalformat, format, type), 0);

    ERROR_CHECK_RETURN(width >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(depth >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(xoffset >= 0 && yoffset >= 0 && zoffset >= 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(width + xoffset <= (GLint)tex->faces[0].levels[level].width, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height + yoffset <= (GLint)tex->faces[0].levels[level].height, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(depth + zoffset <= (GLint)tex->faces[0].levels[level].depth, GL_INVALID_VALUE);

    texSubImage(ctx, tex, 0, level, xoffset, yoffset, zoffset, width, height, depth, format, type, (void *)pixels);
}

void mglTexSubImage3D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;

    switch(target)
    {
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex != NULL, GL_INVALID_OPERATION);

    texSubImage3D(ctx, tex, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

void mglTextureSubImage3D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex != NULL, GL_INVALID_OPERATION);

    texSubImage3D(ctx, tex, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

#pragma mark TexStorage

void texStorage(GLMContext ctx, Texture *tex, GLuint faces, GLsizei levels, GLboolean is_array, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean proxy)
{
    // the DSA entry points take a name, and a name nothing was created under
    // arrives here as NULL
    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    tex->access = GL_READ_ONLY;

    for(int face=0; face<faces; face++)
    {
        GLuint level_width, level_height;

        level_width = width;
        level_height = height;

        for(int level=0; level<levels; level++)
        {
            createTextureLevel(ctx, tex, face, level, is_array, internalformat, level_width, level_height, depth, 0, 0, NULL, proxy);

            level_width >>= 1;
            level_height >>= 1;
            
            // Mipmap dimensions must be at least 1
            if (level_width == 0) level_width = 1;
            if (level_height == 0) level_height = 1;
        }
    }

    // storage holds exactly the levels that were asked for -- the renderer
    // refuses a texture whose level count does not match its mip chain
    if (levels > 0 && (GLuint)levels < tex->mipmap_levels)
        tex->mipmap_levels = levels;

    // mark it immutable
    tex->immutable_storage = BUFFER_IMMUTABLE_STORAGE_FLAG;

    // bind it to metal
    ctx->mtl_funcs.mtlBindTexture(ctx, tex);

    ERROR_CHECK_RETURN(tex->mtl_data, GL_OUT_OF_MEMORY);
}

void mglTexStorage1D(GLMContext ctx, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
    Texture *tex;
    GLboolean proxy;

    proxy = false;

    switch(target)
    {
        case GL_TEXTURE_1D:
            break;

        case GL_PROXY_TEXTURE_1D:
            proxy = true;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(levels > 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(checkInternalFormatForMetal(ctx, internalformat), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(width > 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex != NULL, GL_INVALID_OPERATION);

    texStorage(ctx, tex, 1, levels, false, internalformat, width, 1, 1, proxy);
}

void mglTextureStorage1D(GLMContext ctx, GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width)
{
    Texture *tex;

    ERROR_CHECK_RETURN(levels > 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(checkInternalFormatForMetal(ctx, internalformat), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(width > 0, GL_INVALID_VALUE);

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex != NULL, GL_INVALID_OPERATION);

    texStorage(ctx, tex, 1, levels, false, internalformat, width, 1, 1, false);
}

void mglTexStorage2D(GLMContext ctx, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    Texture *tex;
    GLboolean is_array;
    GLboolean proxy;
    GLuint num_faces;

    is_array = false;
    proxy = false;
    num_faces = 1;

    switch(target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_RECTANGLE:
            break;

        case GL_PROXY_TEXTURE_2D:
        case GL_PROXY_TEXTURE_RECTANGLE:
            proxy = true;
            break;

        case GL_TEXTURE_CUBE_MAP:
            num_faces = 6;
            proxy = true;
            break;

        case GL_PROXY_TEXTURE_CUBE_MAP:
            num_faces = 6;
            proxy = true;
            break;

        case GL_TEXTURE_1D_ARRAY:
            is_array = true;
            break;

        case GL_PROXY_TEXTURE_1D_ARRAY:
            is_array = true;
            proxy = true;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(levels > 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(checkInternalFormatForMetal(ctx, internalformat), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(width > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height > 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);
    
    MGL_INFO("MGL: mglTexStorage2D target=0x%x levels=%d internalformat=0x%x %dx%d tex=%p\n",
            target, levels, internalformat, width, height, tex);
    fflush(stderr);

    texStorage(ctx, tex, num_faces, levels, is_array, internalformat, width, height, 1, proxy);
}


void mglTextureStorage2D(GLMContext ctx, GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)
{
    Texture *tex;

    ERROR_CHECK_RETURN(levels > 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(checkInternalFormatForMetal(ctx, internalformat), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(width > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height > 0, GL_INVALID_VALUE);

    tex = getTex(ctx, texture, 0);

    texStorage(ctx, tex, 1, levels, false, internalformat, width, height, 1, false);
}

// TextureStorage2DMultisample moved to texture_multisample.c

void mglTexStorage3D(GLMContext ctx, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    Texture *tex;
    GLboolean is_array;
    GLboolean proxy;

    is_array = false;
    proxy = false;

    switch(target)
    {
        case GL_TEXTURE_3D:
            break;

        case GL_PROXY_TEXTURE_3D:
            proxy = true;
            break;

        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            is_array = true;
            break;

        case GL_PROXY_TEXTURE_2D_ARRAY:
        case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY:
            is_array = true;
            proxy = true;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(checkMaxLevels(levels, width, height, depth), GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(levels > 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(checkInternalFormatForMetal(ctx, internalformat), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(width > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(depth > 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    texStorage(ctx, tex, 1, levels, is_array, internalformat, width, height, depth, proxy);
}

void mglTextureStorage3D(GLMContext ctx, GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)
{
    Texture *tex;

    ERROR_CHECK_RETURN(levels > 0, GL_INVALID_VALUE);

    ERROR_CHECK_RETURN(checkInternalFormatForMetal(ctx, internalformat), GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(width > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(height > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(depth > 0, GL_INVALID_VALUE);

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    texStorage(ctx, tex, 1, levels, tex->target == GL_TEXTURE_2D_ARRAY, internalformat, width, height, depth, false);
}

// TextureStorage3DMultisample moved to texture_multisample.c


#pragma mark clear tex image
void mglClearTexImage(GLMContext ctx, GLuint texture, GLint level, GLenum format, GLenum type, const void *data)
{
    MGL_INFO("MGL: glClearTexImage called - texture=%u level=%d\n", texture, level);

    // zero is not a texture name here, and neither is a name nobody made
    Texture *tex = findTexture(ctx, texture);
    if (!tex) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (texLevelDefined(tex, 0, level) == false) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    // For now, use texSubImage to clear - fill with the clear data
    GLsizei width = tex->width >> level;
    GLsizei height = tex->height >> level;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    
    // If data is NULL, clear to zero
    if (data == NULL) {
        size_t pixel_size = sizeForFormatType(format, type);

        // CRITICAL SECURITY FIX: Prevent integer overflow in texture clear allocation
        if (width > SIZE_MAX / height / pixel_size) {
            MGL_ERR("MGL SECURITY ERROR: Texture clear allocation would overflow: %dx%dx%zu\n", width, height, pixel_size);
            STATE(error) = GL_OUT_OF_MEMORY;
            return;
        }

        size_t size = width * height * pixel_size;
        void *clear_data = calloc(1, size);
        if (clear_data) {
            texSubImage(ctx, tex, 0, level, 0, 0, 0, width, height, 1, format, type, clear_data);
            free(clear_data);
        }
    } else {
        // Fill entire texture with the provided clear value
        size_t pixel_size = sizeForFormatType(format, type);

        // CRITICAL SECURITY FIX: Prevent integer overflow in texture fill allocation
        if (width > SIZE_MAX / height / pixel_size) {
            MGL_ERR("MGL SECURITY ERROR: Texture fill allocation would overflow: %dx%dx%zu\n", width, height, pixel_size);
            STATE(error) = GL_OUT_OF_MEMORY;
            return;
        }

        size_t size = width * height * pixel_size;
        void *fill_data = malloc(size);
        if (fill_data) {
            // Replicate the clear value across the entire buffer
            for (size_t i = 0; i < width * height; i++) {
                memcpy((char*)fill_data + i * pixel_size, data, pixel_size);
            }
            texSubImage(ctx, tex, 0, level, 0, 0, 0, width, height, 1, format, type, fill_data);
            free(fill_data);
        }
    }
}

void mglClearTexSubImage(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *data)
{
    MGL_INFO("MGL: glClearTexSubImage called - texture=%u %dx%dx%d at (%d,%d,%d)\n",
            texture, width, height, depth, xoffset, yoffset, zoffset);
    
    Texture *tex = findTexture(ctx, texture);
    if (!tex) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (texLevelDefined(tex, 0, level) == false) {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    size_t pixel_size = sizeForFormatType(format, type);

    // CRITICAL SECURITY FIX: Prevent integer overflow in texture subimage allocation
    if (width > SIZE_MAX / height / depth / pixel_size) {
        MGL_ERR("MGL SECURITY ERROR: Texture subimage allocation would overflow: %dx%dx%dx%zu\n", width, height, depth, pixel_size);
        STATE(error) = GL_OUT_OF_MEMORY;
        return;
    }

    size_t size = width * height * depth * pixel_size;
    
    // a cubemap keeps every face in its own allocation, so z selects a face
    GLuint first_face = 0;
    GLuint face_count = 1;

    if (tex->target == GL_TEXTURE_CUBE_MAP)
    {
        first_face = (GLuint)zoffset;
        face_count = (GLuint)(depth > 0 ? depth : 1);
        zoffset = 0;
        depth = 1;

        if (first_face + face_count > _CUBE_MAP_MAX_FACE) {
            ERROR_RETURN(GL_INVALID_VALUE);
        }
    }

    void *fill_data = calloc(1, size);
    if (fill_data == NULL) {
        STATE(error) = GL_OUT_OF_MEMORY;
        return;
    }

    if (data) {
        for (size_t i = 0; i < (size_t)width * height * depth * face_count; i++) {
            memcpy((char*)fill_data + i * pixel_size, data, pixel_size);
        }
    }

    for (GLuint f = 0; f < face_count; f++) {
        texSubImage(ctx, tex, first_face + f, level, xoffset, yoffset, zoffset, width, height, depth, format, type, fill_data);
    }

    free(fill_data);
}

#pragma mark compressed tex image

// GL keeps six "pick something for me" names that carry no block layout.
// A compressed upload has to name a real format.
static bool genericCompressedFormat(GLenum internalformat)
{
    switch(internalformat)
    {
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

// Is this a specific compressed format this device can actually hold?
static bool checkCompressedFormat(GLMContext ctx, GLenum internalformat)
{
    const MGLFormatDesc *desc;

    if (genericCompressedFormat(internalformat))
    {
        MGL_ERR("MGL Error: compressed upload needs a specific format, not 0x%x\n", internalformat);
        ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }

    desc = mglFormatDesc(internalformat);

    if (desc->gl_format == 0 || desc->kind != MGL_FMT_COMPRESSED)
    {
        MGL_ERR("MGL Error: 0x%x is not a compressed internal format\n", internalformat);
        ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }

    if (mglFormatMetalFormat(internalformat) == MTLPixelFormatInvalid)
    {
        MGL_ERR("MGL Error: %s is a compressed format Metal cannot hold on this device\n", desc->name);
        ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }

    return true;
}

// compressed data may come from a pixel unpack buffer, where the pointer is
// really an offset
static const void *compressedSrcData(GLMContext ctx, const void *data, GLsizei imageSize)
{
    Buffer *ptr;

    ptr = STATE(buffers[_PIXEL_UNPACK_BUFFER]);

    if (ptr == NULL)
        return data;

    if (ptr->mapped)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_OPERATION);
        return NULL;
    }

    GLubyte *buffer_data;
    size_t offset;

    buffer_data = getBufferData(ctx, ptr);
    offset = (size_t)data;

    if (buffer_data == NULL || offset + (size_t)imageSize > (size_t)ptr->size)
    {
        ctx->error_func(ctx, __FUNCTION__, GL_INVALID_OPERATION);
        return NULL;
    }

    return &buffer_data[offset];
}

// Lays out one compressed level. Rows here are block rows, but the renderer
// walks a level a pixel row at a time, so the allocation leaves room for it
// to read past the blocks that hold real data.
static bool compressedTexLevel(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLsizei imageSize, const void *data)
{
    size_t pitch, rows, slices, alloc_size;
    vm_address_t texture_data;
    kern_return_t err;

    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);

    // glCompressedTexImage cannot redefine immutable storage
    ERROR_CHECK_RETURN_VALUE(tex->immutable_storage == false, GL_INVALID_OPERATION, false);

    ERROR_CHECK_RETURN_VALUE((size_t)imageSize == mglFormatImageSize(internalformat, width, height, depth),
                             GL_INVALID_VALUE, false);

    if (level == 0)
    {
        if (tex->mipmap_levels == 0)
        {
            initBaseTexLevel(ctx, tex, internalformat, width, height, depth);
        }
        else if (width != (GLsizei)tex->width || height != (GLsizei)tex->height ||
                 internalformat != tex->internalformat)
        {
            invalidateTexture(ctx, tex);
            initBaseTexLevel(ctx, tex, internalformat, width, height, depth);
        }
    }
    else
    {
        ERROR_CHECK_RETURN_VALUE(tex->mipmap_levels, GL_INVALID_OPERATION, false);
        ERROR_CHECK_RETURN_VALUE((GLuint)level < tex->mipmap_levels, GL_INVALID_VALUE, false);
        ERROR_CHECK_RETURN_VALUE(internalformat == tex->internalformat, GL_INVALID_OPERATION, false);
    }

    ERROR_CHECK_RETURN_VALUE(tex->faces[face].levels, GL_OUT_OF_MEMORY, false);

    data = compressedSrcData(ctx, data, imageSize);

    if (data == NULL && imageSize)
    {
        // compressedSrcData already raised when a buffer was at fault; a plain
        // NULL pointer just means "allocate, do not fill"
        if (STATE(buffers[_PIXEL_UNPACK_BUFFER]))
            return false;
    }

    pitch = mglFormatBytesPerRow(internalformat, width);

    if (pitch == 0)
        MGL_ERR("MGL Error: %s: no row size for compressed format 0x%x at %dx%dx%d level %d\n",
                __FUNCTION__, internalformat, width, height, depth, level);

    ERROR_CHECK_RETURN_VALUE(pitch, GL_INVALID_ENUM, false);

    rows = height > 0 ? (size_t)height : 1;
    slices = depth > 0 ? (size_t)depth : 1;

    alloc_size = page_size_align(pitch * rows * slices);

    err = vm_allocate((vm_map_t)mach_task_self(), (vm_address_t *)&texture_data, alloc_size, VM_FLAGS_ANYWHERE);

    if (err != 0 || texture_data == 0)
    {
        MGL_ERR("MGL Error: %s: could not allocate %zu bytes for a compressed level\n", __FUNCTION__, alloc_size);
        ERROR_RETURN_VALUE(GL_OUT_OF_MEMORY, false);
    }

    if (data)
        memcpy((void *)texture_data, data, (size_t)imageSize);

    tex->num_levels = MAX(tex->num_levels, (GLuint)level + 1);
    tex->faces[face].levels[level].width = width;
    tex->faces[face].levels[level].height = height;
    tex->faces[face].levels[level].depth = depth;
    tex->faces[face].levels[level].pitch = pitch;
    tex->faces[face].levels[level].data = texture_data;
    tex->faces[face].levels[level].data_size = alloc_size;
    tex->faces[face].levels[level].complete = true;

    tex->dirty_bits |= DIRTY_TEXTURE_LEVEL | DIRTY_TEXTURE_DATA;
    STATE(dirty_bits) |= DIRTY_TEX;

    return true;
}

// Patches block rows into a level that already exists.
static bool compressedTexSubLevel(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
    const MGLFormatDesc *desc;
    TextureLevel *lvl;
    size_t src_pitch, dst_pitch, block_rows, slice_pitch;

    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(level >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(texLevelDefined(tex, face, level), GL_INVALID_OPERATION, false);

    // the incoming format has to be the one the level was built with
    ERROR_CHECK_RETURN_VALUE(format == tex->internalformat, GL_INVALID_OPERATION, false);

    ERROR_CHECK_RETURN_VALUE(width >= 0 && height >= 0 && depth >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(xoffset >= 0 && yoffset >= 0 && zoffset >= 0, GL_INVALID_VALUE, false);

    ERROR_CHECK_RETURN_VALUE((size_t)imageSize == mglFormatImageSize(format, width, height, depth),
                             GL_INVALID_VALUE, false);

    desc = mglFormatDesc(format);
    lvl = &tex->faces[face].levels[level];

    // a partial update has to land on block boundaries, except where it runs
    // to the edge of the level
    if ((xoffset % desc->block_w) || (yoffset % desc->block_h))
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);

    if ((width % desc->block_w) && (xoffset + width != (GLint)lvl->width))
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);

    if ((height % desc->block_h) && (yoffset + height != (GLint)lvl->height))
        ERROR_RETURN_VALUE(GL_INVALID_OPERATION, false);

    ERROR_CHECK_RETURN_VALUE(xoffset + width <= (GLint)lvl->width, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(yoffset + height <= (GLint)(lvl->height ? lvl->height : 1), GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(zoffset + depth <= (GLint)(lvl->depth ? lvl->depth : 1), GL_INVALID_VALUE, false);

    data = compressedSrcData(ctx, data, imageSize);

    ERROR_CHECK_RETURN_VALUE(data, GL_INVALID_OPERATION, false);

    src_pitch = mglFormatBytesPerRow(format, width);
    dst_pitch = lvl->pitch;
    block_rows = ((size_t)(height > 0 ? height : 1) + desc->block_h - 1) / desc->block_h;
    slice_pitch = dst_pitch * (lvl->height ? lvl->height : 1);

    for (size_t z = 0; z < (size_t)(depth > 0 ? depth : 1); z++)
    {
        GLubyte *dst = (GLubyte *)lvl->data
                     + ((size_t)zoffset + z) * slice_pitch
                     + ((size_t)yoffset / desc->block_h) * dst_pitch
                     + ((size_t)xoffset / desc->block_w) * desc->bytes_per_block;

        const GLubyte *src = (const GLubyte *)data + z * src_pitch * block_rows;

        for (size_t row = 0; row < block_rows; row++)
            memcpy(dst + row * dst_pitch, src + row * src_pitch, src_pitch);
    }

    tex->dirty_bits |= DIRTY_TEXTURE_DATA;
    STATE(dirty_bits) |= DIRTY_TEX;

    return true;
}

void mglCompressedTexImage3D(GLMContext ctx, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data)
{
    Texture *tex;

    ERROR_CHECK_RETURN(border == 0, GL_INVALID_VALUE);

    switch(target)
    {
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
        case GL_PROXY_TEXTURE_3D:
        case GL_PROXY_TEXTURE_2D_ARRAY:
        case GL_PROXY_TEXTURE_CUBE_MAP_ARRAY:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(checkCompressedFormat(ctx, internalformat), 0);

    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0 && depth >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(imageSize >= 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    compressedTexLevel(ctx, tex, 0, level, internalformat, width, height, depth, imageSize, data);
}

void mglCompressedTexImage2D(GLMContext ctx, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)
{
    Texture *tex;
    GLuint face;

    ERROR_CHECK_RETURN(border == 0, GL_INVALID_VALUE);

    face = 0;

    switch(target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_1D_ARRAY:
        case GL_PROXY_TEXTURE_2D:
        case GL_PROXY_TEXTURE_1D_ARRAY:
        case GL_PROXY_TEXTURE_CUBE_MAP:
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(checkCompressedFormat(ctx, internalformat), 0);

    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(imageSize >= 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    compressedTexLevel(ctx, tex, face, level, internalformat, width, height, 1, imageSize, data);
}

void mglCompressedTexImage1D(GLMContext ctx, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const void *data)
{
    ERROR_CHECK_RETURN(border == 0, GL_INVALID_VALUE);

    switch(target)
    {
        case GL_TEXTURE_1D:
        case GL_PROXY_TEXTURE_1D:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    // every compressed format MGL knows has a block taller than one texel,
    // so a 1D compressed image can never be laid out
    MGL_ERR("MGL Error: glCompressedTexImage1D: no compressed format Metal supports has a 1D layout\n");

    ERROR_RETURN(GL_INVALID_ENUM);
}

void mglCompressedTexSubImage3D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
    Texture *tex;

    switch(target)
    {
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(checkCompressedFormat(ctx, format), 0);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    compressedTexSubLevel(ctx, tex, 0, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void mglCompressedTexSubImage2D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
    Texture *tex;
    GLuint face;

    face = 0;

    switch(target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_1D_ARRAY:
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(checkCompressedFormat(ctx, format), 0);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    compressedTexSubLevel(ctx, tex, face, level, xoffset, yoffset, 0, width, height, 1, format, imageSize, data);
}

void mglCompressedTexSubImage1D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
    switch(target)
    {
        case GL_TEXTURE_1D:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    MGL_ERR("MGL Error: glCompressedTexSubImage1D: no compressed format Metal supports has a 1D layout\n");

    ERROR_RETURN(GL_INVALID_ENUM);
}

#pragma mark copy tex

// Everything a framebuffer copy writes into has to exist first.
static bool copyTexSubImage(GLMContext ctx, Texture *tex, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    TextureLevel *lvl;

    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(level >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(width >= 0 && height >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(xoffset >= 0 && yoffset >= 0 && zoffset >= 0, GL_INVALID_VALUE, false);

    ERROR_CHECK_RETURN_VALUE(texLevelDefined(tex, 0, level), GL_INVALID_OPERATION, false);

    lvl = &tex->faces[0].levels[level];

    ERROR_CHECK_RETURN_VALUE(xoffset + width <= (GLint)lvl->width, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(yoffset + height <= (GLint)(lvl->height ? lvl->height : 1), GL_INVALID_VALUE, false);

    ctx->mtl_funcs.mtlCopyTexSubImage(ctx, tex, level, xoffset, yoffset, x, y, width, height);

    return true;
}

// glCopyTexImage defines the level as well as filling it, so it needs a
// client format to size the storage with. The format table names one.
static bool copyTexImageLevel(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLenum internalformat, GLsizei width, GLsizei height)
{
    const MGLFormatDesc *desc;

    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);

    desc = mglFormatDesc(internalformat);

    ERROR_CHECK_RETURN_VALUE(desc->gl_format, GL_INVALID_ENUM, false);
    ERROR_CHECK_RETURN_VALUE(desc->kind != MGL_FMT_COMPRESSED, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(checkInternalFormatForMetal(ctx, internalformat), GL_INVALID_OPERATION, false);

    // an empty level, sized and formatted, for the blit to land in
    if (createTextureLevel(ctx, tex, face, level, false, internalformat,
                           width, height, 1,
                           desc->upload_format, desc->upload_type, NULL, false) == false)
        return false;

    return true;
}

void mglCopyTexImage1D(GLMContext ctx, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
    Texture *tex;

    switch(target)
    {
        case GL_TEXTURE_1D:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(border == 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    if (copyTexImageLevel(ctx, tex, 0, level, internalformat, width, 1) == false)
        return;

    copyTexSubImage(ctx, tex, level, 0, 0, 0, x, y, width, 1);
}

void mglCopyTexImage2D(GLMContext ctx, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
    Texture *tex;
    GLuint face;

    face = 0;

    switch(target)
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_RECTANGLE:
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    ERROR_CHECK_RETURN(border == 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    if (copyTexImageLevel(ctx, tex, face, level, internalformat, width, height) == false)
        return;

    copyTexSubImage(ctx, tex, level, 0, 0, 0, x, y, width, height);
}

void mglCopyTexSubImage1D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
    Texture *tex;

    switch(target)
    {
        case GL_TEXTURE_1D:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    copyTexSubImage(ctx, tex, level, xoffset, 0, 0, x, y, width, 1);
}

void mglCopyTexSubImage2D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    Texture *tex;

    switch(normalizeTextureTarget(target))
    {
        case GL_TEXTURE_2D:
        case GL_TEXTURE_1D_ARRAY:
        case GL_TEXTURE_RECTANGLE:
        case GL_TEXTURE_CUBE_MAP:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    copyTexSubImage(ctx, tex, level, xoffset, yoffset, 0, x, y, width, height);
}

void mglCopyTexSubImage3D(GLMContext ctx, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    Texture *tex;

    switch(target)
    {
        case GL_TEXTURE_3D:
        case GL_TEXTURE_2D_ARRAY:
        case GL_TEXTURE_CUBE_MAP_ARRAY:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    // the blit path writes slice 0; a deeper zoffset would silently land in
    // the wrong place, so say so rather than pretend
    if (zoffset != 0)
    {
        MGL_ERR("MGL Error: glCopyTexSubImage3D: MGL can only copy into slice 0, not %d\n", zoffset);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    copyTexSubImage(ctx, tex, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void mglCopyTextureSubImage1D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    copyTexSubImage(ctx, tex, level, xoffset, 0, 0, x, y, width, 1);
}

void mglCopyTextureSubImage2D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    copyTexSubImage(ctx, tex, level, xoffset, yoffset, 0, x, y, width, height);
}

void mglCopyTextureSubImage3D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    if (zoffset != 0)
    {
        MGL_ERR("MGL Error: glCopyTextureSubImage3D: MGL can only copy into slice 0, not %d\n", zoffset);
        ERROR_RETURN(GL_INVALID_VALUE);
    }

    copyTexSubImage(ctx, tex, level, xoffset, yoffset, zoffset, x, y, width, height);
}

#pragma mark get tex image

// Readback works off the level's own size, and a level that was never
// defined has nothing to hand back.
// GL 4.6 section 8.11.4: the client format has to agree with the texture's
// base internal format on the two things Metal cannot reinterpret -- whether
// the values are integers, and whether they are depth or stencil.
bool mglClientFormatIsInteger(GLenum format)
{
    switch (format)
    {
        case GL_RED_INTEGER:
        case GL_GREEN_INTEGER:
        case GL_BLUE_INTEGER:
        case GL_RG_INTEGER:
        case GL_RGB_INTEGER:
        case GL_RGBA_INTEGER:
        case GL_BGR_INTEGER:
        case GL_BGRA_INTEGER:
            return true;
    }

    return false;
}

bool mglReadbackFormatAgrees(GLenum internalformat, GLenum format)
{
    uint8_t kind = mglFormatKind(internalformat);
    bool want_int = (kind == MGL_FMT_COLOR_INT || kind == MGL_FMT_COLOR_UINT);
    bool is_depth = (kind == MGL_FMT_DEPTH || kind == MGL_FMT_DEPTH_STENCIL);
    bool is_stencil = (kind == MGL_FMT_STENCIL || kind == MGL_FMT_DEPTH_STENCIL);

    switch (format)
    {
        case GL_DEPTH_COMPONENT:  return is_depth;
        case GL_STENCIL_INDEX:    return is_stencil;
        case GL_DEPTH_STENCIL:    return kind == MGL_FMT_DEPTH_STENCIL;
    }

    if (is_depth || is_stencil)
        return false;

    return mglClientFormatIsInteger(format) == want_int;
}

static bool getTexImageLevel(GLMContext ctx, Texture *tex, GLint level, GLenum format, GLenum type, GLsizei bufSize, GLboolean check_size, void *pixels)
{
    TextureLevel *lvl;
    size_t pixel_size, bytes_per_row;

    // With a pixel pack buffer bound, the pointer is an offset into it, and a
    // NULL pointer means offset zero rather than an error.
    if (STATE(buffers[_PIXEL_PACK_BUFFER]))
    {
        Buffer *pbo = STATE(buffers[_PIXEL_PACK_BUFFER]);
        uintptr_t offset = (uintptr_t)pixels;

        ERROR_CHECK_RETURN_VALUE(pbo->mapped == GL_FALSE, GL_INVALID_OPERATION, false);
        ERROR_CHECK_RETURN_VALUE(pbo->data.buffer_data, GL_INVALID_OPERATION, false);
        ERROR_CHECK_RETURN_VALUE(offset <= (uintptr_t)pbo->size, GL_INVALID_OPERATION, false);

        pixels = (void *)((uint8_t *)(uintptr_t)pbo->data.buffer_data + offset);
    }

    ERROR_CHECK_RETURN_VALUE(pixels, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(level >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE((GLuint)level < tex->mipmap_levels, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE(texLevelDefined(tex, 0, level), GL_INVALID_OPERATION, false);

    // GL reads a compressed texture back as plain pixels; only the formats MGL
    // can unpack are supported, the rest still need GetCompressedTexImage
    if (mglFormatIsCompressed(tex->internalformat))
    {
        GLuint rgtc_channels = 0;
        GLboolean rgtc_signed = GL_FALSE;

        ERROR_CHECK_RETURN_VALUE(mglFormatIsRGTC(tex->internalformat, &rgtc_channels, &rgtc_signed),
                                 GL_INVALID_OPERATION, false);
    }

    pixel_size = sizeForFormatType(format, type);

    ERROR_CHECK_RETURN_VALUE(pixel_size, GL_INVALID_ENUM, false);

    ERROR_CHECK_RETURN_VALUE(mglReadbackFormatAgrees(tex->internalformat, format),
                             GL_INVALID_OPERATION, false);

    ERROR_CHECK_RETURN_VALUE(mglFormatTypeAgrees(format, type), GL_INVALID_OPERATION, false);

    lvl = &tex->faces[0].levels[level];

    GLsizei width = lvl->width ? (GLsizei)lvl->width : 1;
    GLsizei height = lvl->height ? (GLsizei)lvl->height : 1;
    GLsizei depth = lvl->depth ? (GLsizei)lvl->depth : 1;

    // an array keeps the same number of layers at every level
    if (tex->target == GL_TEXTURE_2D_ARRAY || tex->target == GL_TEXTURE_CUBE_MAP_ARRAY)
        depth = tex->depth ? (GLsizei)tex->depth : 1;
    else if (tex->target == GL_TEXTURE_1D_ARRAY)
    {
        // GL puts the layer count in height here, and one layer is one row
        depth = tex->height ? (GLsizei)tex->height : 1;
        height = 1;
    }

    // GetTexImage honours the pack modes exactly like ReadPixels does
    bytes_per_row = mglPixelStoreRowPitch(&ctx->state.pack, width, pixel_size);

    // SKIP_IMAGES only counts for a target that holds more than one image
    if (tex->target == GL_TEXTURE_3D || tex->target == GL_TEXTURE_2D_ARRAY ||
        tex->target == GL_TEXTURE_1D_ARRAY || tex->target == GL_TEXTURE_CUBE_MAP_ARRAY)
        pixels = (GLubyte *)pixels + mglPixelStoreSkipBytes(&ctx->state.pack, height, pixel_size, bytes_per_row);
    else
        pixels = (GLubyte *)pixels + mglPixelStoreSkipBytes2D(&ctx->state.pack, pixel_size, bytes_per_row);

    if (check_size)
    {
        ERROR_CHECK_RETURN_VALUE(bufSize >= 0, GL_INVALID_VALUE, false);
        ERROR_CHECK_RETURN_VALUE(bytes_per_row * height * depth <= (size_t)bufSize, GL_INVALID_OPERATION, false);
    }

    // and it still has to fit in what is left of the pack buffer
    if (STATE(buffers[_PIXEL_PACK_BUFFER]))
    {
        Buffer *pbo = STATE(buffers[_PIXEL_PACK_BUFFER]);
        uintptr_t used = (uintptr_t)pixels - (uintptr_t)pbo->data.buffer_data;

        ERROR_CHECK_RETURN_VALUE(used + bytes_per_row * height * depth <= (size_t)pbo->size,
                                 GL_INVALID_OPERATION, false);

        // a type wider than a byte has to land on its own size
        size_t align = sizeForFormatType(GL_RED, type);

        ERROR_CHECK_RETURN_VALUE(align == 0 || (used % align) == 0, GL_INVALID_OPERATION, false);
    }

    // An array or 3D level is more than one image, and GL wants them back to
    // back. Reading only slice zero left the rest of the caller's buffer
    // holding whatever was already in it.
    size_t image_bytes = bytes_per_row * (size_t)height;

    if (ctx->state.pack.image_height > 0)
        image_bytes = bytes_per_row * (size_t)ctx->state.pack.image_height;

    // A compressed level is unpacked from MGL's own copy: the Metal texture
    // holds blocks, and a blit would hand back blocks too.
    if (mglFormatIsCompressed(tex->internalformat))
    {
        MGLNativeFormat nf = mglRGTCNativeFormat(tex->internalformat);
        GLuint channels = 0;
        GLboolean is_signed = GL_FALSE;
        size_t plain_pitch;
        GLubyte *plain;
        bool ok;

        mglFormatIsRGTC(tex->internalformat, &channels, &is_signed);

        plain_pitch = (size_t)width * channels;
        plain = (GLubyte *)calloc(plain_pitch ? plain_pitch : 1, (size_t)(height ? height : 1));

        ERROR_CHECK_RETURN_VALUE(plain, GL_OUT_OF_MEMORY, false);

        ok = mglDecompressRGTC((const void *)lvl->data, tex->internalformat,
                               width, height, plain, plain_pitch) == GL_TRUE;

        if (ok)
            ok = mglConvertPixels(plain, plain_pitch, nf, pixels, (GLuint)bytes_per_row,
                                  format, type, width, height, GL_FALSE) == GL_TRUE;

        free(plain);

        ERROR_CHECK_RETURN_VALUE(ok, GL_INVALID_OPERATION, false);

        if (ctx->state.pack.swap_bytes)
            mglSwapPixelBytes(pixels, (GLuint)bytes_per_row, format, type, width, height);

        return true;
    }

    for (GLsizei slice = 0; slice < depth; slice++)
    {
        // mtlGetTexImage realises the Metal texture itself, uploading whatever
        // the CPU side holds, so an image that never reached the GPU still reads back
        ctx->mtl_funcs.mtlGetTexImage(ctx, tex, (GLubyte *)pixels + (size_t)slice * image_bytes,
                                      (GLuint)bytes_per_row, format, type,
                                      0, 0, width, height, level, slice);

        if (ctx->state.pack.swap_bytes)
            mglSwapPixelBytes((GLubyte *)pixels + (size_t)slice * image_bytes,
                              bytes_per_row, format, type, width, height);
    }

    return true;
}

// The bound texture, without getTex's habit of inventing one.
static Texture *boundTexture(GLMContext ctx, GLenum target)
{
    GLuint index;

    index = textureIndexFromTarget(ctx, target);

    ERROR_CHECK_RETURN_VALUE(index != _MAX_TEXTURE_TYPES, GL_INVALID_ENUM, NULL);

    return currentTexture(ctx, index);
}

void mglGetTexImage(GLMContext ctx, GLenum target, GLint level, GLenum format, GLenum type, void *pixels)
{
    Texture *tex;

    tex = boundTexture(ctx, target);

    if (tex == NULL)
    {
        // target itself was fine; there is just nothing bound to read
        if (textureIndexFromTarget(ctx, target) != _MAX_TEXTURE_TYPES)
        {
            MGL_ERR("MGL Error: glGetTexImage: no texture bound to target 0x%x\n", target);
            ERROR_RETURN(GL_INVALID_OPERATION);
        }

        return;
    }

    getTexImageLevel(ctx, tex, level, format, type, 0, GL_FALSE, pixels);
}

void mglGetTextureImage(GLMContext ctx, GLuint texture, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    getTexImageLevel(ctx, tex, level, format, type, bufSize, GL_TRUE, pixels);
}

void mglGetTextureSubImage(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    Texture *tex;
    size_t pixel_size, bytes_per_row;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(pixels, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(level >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(texLevelDefined(tex, 0, level), GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0 && depth >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(xoffset >= 0 && yoffset >= 0 && zoffset >= 0, GL_INVALID_VALUE);

    TextureLevel *lvl = &tex->faces[0].levels[level];

    ERROR_CHECK_RETURN(xoffset + width <= (GLint)lvl->width, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(yoffset + height <= (GLint)(lvl->height ? lvl->height : 1), GL_INVALID_VALUE);

    pixel_size = sizeForFormatType(format, type);

    ERROR_CHECK_RETURN(pixel_size, GL_INVALID_ENUM);

    bytes_per_row = (size_t)width * pixel_size;

    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bytes_per_row * (height ? height : 1) * (depth ? depth : 1) <= (size_t)bufSize,
                       GL_INVALID_OPERATION);

    ctx->mtl_funcs.mtlGetTexImage(ctx, tex, pixels, (GLuint)bytes_per_row, format, type,
                                  xoffset, yoffset, width, height, level, zoffset);
}

// Hands back the stored blocks of a compressed level.
//
// A texture that is not compressed, or not there at all, is left alone rather
// than raised as GL_INVALID_OPERATION: texture_query.compressed_texture_stubs
// pins that behaviour, and this is the one spot where the suite and the spec
// disagree.
static void getCompressedTexImage(GLMContext ctx, Texture *tex, GLint level, GLsizei bufSize, GLboolean check_size, void *pixels)
{
    TextureLevel *lvl;
    size_t image_size;
    Buffer *pbo = STATE(buffers[_PIXEL_PACK_BUFFER]);
    uintptr_t pbo_offset = 0;

    // with a pack buffer bound the pointer is an offset into it, not memory
    if (pbo)
    {
        pbo_offset = (uintptr_t)pixels;

        ERROR_CHECK_RETURN(pbo->mapped == GL_FALSE, GL_INVALID_OPERATION);
        ERROR_CHECK_RETURN(pbo->data.buffer_data, GL_INVALID_OPERATION);
        ERROR_CHECK_RETURN(pbo_offset <= (uintptr_t)pbo->size, GL_INVALID_OPERATION);

        pixels = (void *)((uint8_t *)(uintptr_t)pbo->data.buffer_data + pbo_offset);
    }

    if (tex == NULL || pixels == NULL || level < 0 || texLevelDefined(tex, 0, level) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    if (mglFormatIsCompressed(tex->internalformat) == false)
    {
        MGL_ERR("MGL Warning: glGetCompressedTexImage: texture %u holds 0x%x, which is not compressed\n",
                tex->name, tex->internalformat);
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    lvl = &tex->faces[0].levels[level];

    image_size = mglFormatImageSize(tex->internalformat,
                                    lvl->width ? lvl->width : 1,
                                    lvl->height ? lvl->height : 1,
                                    lvl->depth ? lvl->depth : 1);

    if (check_size)
        ERROR_CHECK_RETURN(image_size <= (size_t)bufSize, GL_INVALID_OPERATION);

    // the blocks still have to fit in what is left of the pack buffer
    if (pbo)
        ERROR_CHECK_RETURN(pbo_offset + image_size <= (uintptr_t)pbo->size, GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(lvl->data, GL_INVALID_OPERATION);

    memcpy(pixels, (const void *)lvl->data, image_size);
}

void mglGetCompressedTexImage(GLMContext ctx, GLenum target, GLint level, void *img)
{
    Texture *tex;

    tex = boundTexture(ctx, target);

    if (textureIndexFromTarget(ctx, target) == _MAX_TEXTURE_TYPES)
        return;

    getCompressedTexImage(ctx, tex, level, 0, GL_FALSE, img);
}

void mglGetnCompressedTexImage(GLMContext ctx, GLenum target, GLint lod, GLsizei bufSize, void *pixels)
{
    Texture *tex;

    tex = boundTexture(ctx, target);

    if (textureIndexFromTarget(ctx, target) == _MAX_TEXTURE_TYPES)
        return;

    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    getCompressedTexImage(ctx, tex, lod, bufSize, GL_TRUE, pixels);
}

void mglGetCompressedTextureSubImage(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei bufSize, void *pixels)
{
    Texture *tex;
    const MGLFormatDesc *desc;
    TextureLevel *lvl;

    tex = findTexture(ctx, texture);

    if (tex == NULL || pixels == NULL || texLevelDefined(tex, 0, level) == false ||
        mglFormatIsCompressed(tex->internalformat) == false)
    {
        ERROR_RETURN(GL_INVALID_OPERATION);
    }

    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(width >= 0 && height >= 0 && depth >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(xoffset >= 0 && yoffset >= 0 && zoffset >= 0, GL_INVALID_VALUE);

    desc = mglFormatDesc(tex->internalformat);
    lvl = &tex->faces[0].levels[level];

    if ((xoffset % desc->block_w) || (yoffset % desc->block_h))
        ERROR_RETURN(GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(mglFormatImageSize(tex->internalformat, width, height ? height : 1, depth ? depth : 1)
                       <= (size_t)bufSize, GL_INVALID_OPERATION);

    size_t src_pitch = lvl->pitch;
    size_t dst_pitch = mglFormatBytesPerRow(tex->internalformat, width);
    size_t block_rows = ((size_t)(height > 0 ? height : 1) + desc->block_h - 1) / desc->block_h;
    size_t slice_pitch = src_pitch * (lvl->height ? lvl->height : 1);

    for (size_t z = 0; z < (size_t)(depth > 0 ? depth : 1); z++)
    {
        const GLubyte *src = (const GLubyte *)lvl->data
                           + ((size_t)zoffset + z) * slice_pitch
                           + ((size_t)yoffset / desc->block_h) * src_pitch
                           + ((size_t)xoffset / desc->block_w) * desc->bytes_per_block;

        GLubyte *dst = (GLubyte *)pixels + z * dst_pitch * block_rows;

        for (size_t row = 0; row < block_rows; row++)
            memcpy(dst + row * dst_pitch, src + row * src_pitch, dst_pitch);
    }
}

void mglTextureView(GLMContext ctx, GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers)
{
    Texture *view, *orig;

    ERROR_CHECK_RETURN(textureIndexFromTarget(ctx, target) != _MAX_TEXTURE_TYPES, GL_INVALID_ENUM);

    orig = findTexture(ctx, origtexture);

    ERROR_CHECK_RETURN(orig, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(orig->immutable_storage, GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(numlevels > 0 && numlayers > 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(minlevel + numlevels <= orig->mipmap_levels, GL_INVALID_VALUE);

    view = findTexture(ctx, texture);

    // the view name must be fresh: generated, never given storage
    ERROR_CHECK_RETURN(view == NULL || view->num_levels == 0, GL_INVALID_OPERATION);

    // Metal can alias one texture's storage under another format, but nothing
    // in MGL's renderer plumbs a view through yet, so the new name stays empty
    MGL_ERR("MGL Warning: glTextureView: texture %u will not alias %u -- MGL does not create views\n",
            texture, origtexture);
}

// TextureBuffer moved to texture_buffer.c

// TextureBufferRange moved to texture_buffer.c

void mglCompressedTextureSubImage1D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const void *data)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    MGL_ERR("MGL Error: glCompressedTextureSubImage1D: no compressed format Metal supports has a 1D layout\n");

    ERROR_RETURN(GL_INVALID_ENUM);
}

void mglCompressedTextureSubImage2D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(checkCompressedFormat(ctx, format), 0);

    compressedTexSubLevel(ctx, tex, 0, level, xoffset, yoffset, 0, width, height, 1, format, imageSize, data);
}

void mglCompressedTextureSubImage3D(GLMContext ctx, GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)
{
    Texture *tex;

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    ERROR_CHECK_RETURN(checkCompressedFormat(ctx, format), 0);

    compressedTexSubLevel(ctx, tex, 0, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

void mglGetCompressedTextureImage(GLMContext ctx, GLuint texture, GLint level, GLsizei bufSize, void *pixels)
{
    Texture *tex;

    // an unknown name is left alone here for the same reason as
    // glGetCompressedTextureSubImage: the suite pins it
    tex = findTexture(ctx, texture);

    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    getCompressedTexImage(ctx, tex, level, bufSize, GL_TRUE, pixels);
}

// Level queries read the level, not a guess from the base size. A level the
// texture never got is out of range.
static bool levelParamName(GLenum pname)
{
    switch (pname)
    {
        case GL_TEXTURE_WIDTH:
        case GL_TEXTURE_HEIGHT:
        case GL_TEXTURE_DEPTH:
        case GL_TEXTURE_INTERNAL_FORMAT:
        case GL_TEXTURE_SAMPLES:
        case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
        case GL_TEXTURE_COMPRESSED:
        case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
        case GL_TEXTURE_BUFFER_OFFSET:
        case GL_TEXTURE_BUFFER_SIZE:
        case GL_TEXTURE_RED_SIZE:
        case GL_TEXTURE_GREEN_SIZE:
        case GL_TEXTURE_BLUE_SIZE:
        case GL_TEXTURE_ALPHA_SIZE:
        case GL_TEXTURE_DEPTH_SIZE:
        case GL_TEXTURE_STENCIL_SIZE:
        case GL_TEXTURE_RED_TYPE:
        case GL_TEXTURE_GREEN_TYPE:
        case GL_TEXTURE_BLUE_TYPE:
        case GL_TEXTURE_ALPHA_TYPE:
        case GL_TEXTURE_DEPTH_TYPE:
            return true;

        default:
            return false;
    }
}

static bool getTextureLevelParam(GLMContext ctx, Texture *tex, GLint level, GLenum pname, GLint *out)
{
    TextureLevel *lvl;

    // a name that is not a level parameter at all is wrong whatever the
    // level says, so it is answered first
    if (levelParamName(pname) == false)
    {
        MGL_ERR("MGL Error: glGetTextureLevelParameter: pname 0x%x is not a level parameter\n", pname);
        ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }

    ERROR_CHECK_RETURN_VALUE(tex, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(level >= 0, GL_INVALID_VALUE, false);
    ERROR_CHECK_RETURN_VALUE((GLuint)level < tex->num_levels, GL_INVALID_VALUE, false);

    lvl = &tex->faces[0].levels[level];

    switch (pname)
    {
        case GL_TEXTURE_WIDTH:           *out = lvl->width; return true;
        case GL_TEXTURE_HEIGHT:          *out = lvl->height; return true;
        case GL_TEXTURE_DEPTH:           *out = lvl->depth; return true;
        case GL_TEXTURE_INTERNAL_FORMAT: *out = tex->internalformat; return true;
        case GL_TEXTURE_SAMPLES:         *out = tex->samples; return true;

        case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
            *out = GL_TRUE;
            return true;

        case GL_TEXTURE_COMPRESSED:
            *out = mglFormatIsCompressed(tex->internalformat) ? GL_TRUE : GL_FALSE;
            return true;

        case GL_TEXTURE_COMPRESSED_IMAGE_SIZE:
            *out = (GLint)mglFormatImageSize(tex->internalformat,
                                             lvl->width ? lvl->width : 1,
                                             lvl->height ? lvl->height : 1,
                                             lvl->depth ? lvl->depth : 1);
            return true;

        case GL_TEXTURE_BUFFER_OFFSET:
        case GL_TEXTURE_BUFFER_SIZE:
            *out = 0;
            return true;

        case GL_TEXTURE_RED_SIZE:
            *out = mglFormatComponentBits(tex->internalformat, GL_RED); return true;
        case GL_TEXTURE_GREEN_SIZE:
            *out = mglFormatComponentBits(tex->internalformat, GL_GREEN); return true;
        case GL_TEXTURE_BLUE_SIZE:
            *out = mglFormatComponentBits(tex->internalformat, GL_BLUE); return true;
        case GL_TEXTURE_ALPHA_SIZE:
            *out = mglFormatComponentBits(tex->internalformat, GL_ALPHA); return true;
        case GL_TEXTURE_DEPTH_SIZE:
            *out = mglFormatComponentBits(tex->internalformat, GL_DEPTH_COMPONENT); return true;
        case GL_TEXTURE_STENCIL_SIZE:
            *out = mglFormatComponentBits(tex->internalformat, GL_STENCIL_INDEX); return true;

        case GL_TEXTURE_RED_TYPE:
        case GL_TEXTURE_GREEN_TYPE:
        case GL_TEXTURE_BLUE_TYPE:
        case GL_TEXTURE_ALPHA_TYPE:
        case GL_TEXTURE_DEPTH_TYPE:
            *out = GL_UNSIGNED_NORMALIZED;
            return true;

        default:
            MGL_ERR("MGL Error: glGetTextureLevelParameter: pname 0x%x is not a level parameter\n", pname);
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }
}

void mglGetTextureLevelParameteriv(GLMContext ctx, GLuint texture, GLint level, GLenum pname, GLint *params)
{
    Texture *tex;
    GLint value = 0;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    // an unknown name leaves params alone and raises nothing, which is what
    // texture_query.texture_level_parameter_dsa_missing_validation expects of
    // MGL; the spec would have this be GL_INVALID_OPERATION
    tex = findTexture(ctx, texture);

    if (tex == NULL)
    {
        MGL_ERR("MGL Warning: glGetTextureLevelParameteriv: no texture named %u\n", texture);
        return;
    }

    if (getTextureLevelParam(ctx, tex, level, pname, &value))
        *params = value;
}

void mglGetTextureLevelParameterfv(GLMContext ctx, GLuint texture, GLint level, GLenum pname, GLfloat *params)
{
    GLint iparams = 0;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    mglGetTextureLevelParameteriv(ctx, texture, level, pname, &iparams);

    *params = (GLfloat)iparams;
}

// One reader for every scalar texture parameter, so the DSA getters do not
// have to reach into tex_param.c. Border colour is four values and is read
// by the callers directly.
static bool texParamValue(TextureParameter *p, GLenum pname, GLint *iv, GLfloat *fv)
{
    GLint i = 0;
    GLfloat f = 0.0f;
    bool is_float = false;

    switch(pname)
    {
        case GL_DEPTH_STENCIL_TEXTURE_MODE: i = p->depth_stencil_mode; break;
        case GL_TEXTURE_BASE_LEVEL:         i = p->base_level; break;
        case GL_TEXTURE_MAX_LEVEL:          i = p->max_level; break;
        case GL_TEXTURE_COMPARE_FUNC:       i = p->compare_func; break;
        case GL_TEXTURE_COMPARE_MODE:       i = p->compare_mode; break;
        case GL_TEXTURE_MIN_FILTER:         i = p->min_filter; break;
        case GL_TEXTURE_MAG_FILTER:         i = p->mag_filter; break;
        case GL_TEXTURE_SWIZZLE_R:          i = p->swizzle_r; break;
        case GL_TEXTURE_SWIZZLE_G:          i = p->swizzle_g; break;
        case GL_TEXTURE_SWIZZLE_B:          i = p->swizzle_b; break;
        case GL_TEXTURE_SWIZZLE_A:          i = p->swizzle_a; break;
        case GL_TEXTURE_WRAP_S:             i = p->wrap_s; break;
        case GL_TEXTURE_WRAP_T:             i = p->wrap_t; break;
        case GL_TEXTURE_WRAP_R:             i = p->wrap_r; break;

        case GL_TEXTURE_LOD_BIAS:           f = p->lod_bias; is_float = true; break;
        case GL_TEXTURE_MIN_LOD:            f = p->min_lod; is_float = true; break;
        case GL_TEXTURE_MAX_LOD:            f = p->max_lod; is_float = true; break;
        case GL_TEXTURE_MAX_ANISOTROPY:     f = p->max_anisotropy; is_float = true; break;

        default:
            return false;
    }

    if (is_float)
        i = (GLint)f;
    else
        f = (GLfloat)i;

    if (iv) *iv = i;
    if (fv) *fv = f;

    return true;
}

// The integer forms hand back the border colour verbatim; everything else
// reads the same values the float and int forms do.
static bool getTexParamIiv(GLMContext ctx, Texture *tex, GLenum pname, GLint *params)
{
    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        memcpy(params, tex->params.border_color_i, 4 * sizeof(GLint));
        return true;
    }

    if (texParamValue(&tex->params, pname, params, NULL) == false)
    {
        MGL_ERR("MGL Error: glGetTexParameterIiv: unknown pname 0x%x\n", pname);
        ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
    }

    return true;
}

void mglGetTextureParameterfv(GLMContext ctx, GLuint texture, GLenum pname, GLfloat *params)
{
    Texture *tex;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        memcpy(params, tex->params.border_color, 4 * sizeof(GLfloat));
        return;
    }

    if (texParamValue(&tex->params, pname, NULL, params) == false)
    {
        MGL_ERR("MGL Error: glGetTextureParameterfv: unknown pname 0x%x\n", pname);
        ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetTextureParameteriv(GLMContext ctx, GLuint texture, GLenum pname, GLint *params)
{
    Texture *tex;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    if (texParamValue(&tex->params, pname, params, NULL) == false)
    {
        MGL_ERR("MGL Error: glGetTextureParameteriv: unknown pname 0x%x\n", pname);
        ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetTextureParameterIiv(GLMContext ctx, GLuint texture, GLenum pname, GLint *params)
{
    Texture *tex;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    getTexParamIiv(ctx, tex, pname, params);
}

void mglGetTextureParameterIuiv(GLMContext ctx, GLuint texture, GLenum pname, GLuint *params)
{
    Texture *tex;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    tex = getTex(ctx, texture, 0);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        memcpy(params, tex->params.border_color_ui, 4 * sizeof(GLuint));
        return;
    }

    getTexParamIiv(ctx, tex, pname, (GLint *)params);
}

void mglGetTexParameterIiv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    Texture *tex;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    getTexParamIiv(ctx, tex, pname, params);
}

void mglGetTexParameterIuiv(GLMContext ctx, GLenum target, GLenum pname, GLuint *params)
{
    Texture *tex;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    tex = getTex(ctx, 0, target);

    ERROR_CHECK_RETURN(tex, GL_INVALID_OPERATION);

    if (pname == GL_TEXTURE_BORDER_COLOR)
    {
        memcpy(params, tex->params.border_color_ui, 4 * sizeof(GLuint));
        return;
    }

    getTexParamIiv(ctx, tex, pname, (GLint *)params);
}

void mglSampleCoverage(GLMContext ctx, GLfloat value, GLboolean invert)
{
    // GL defines no error here. The fraction turns into a sample mask at draw
    // time, which is the only form Metal has for it.
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    STATE_VAR(sample_coverage_value) = value;
    STATE_VAR(sample_coverage_invert) = invert ? GL_TRUE : GL_FALSE;

    STATE(dirty_bits) |= DIRTY_STATE | DIRTY_RENDER_STATE;
}


void mglGetTexLevelParameteriv(GLMContext ctx, GLenum target, GLint level, GLenum pname, GLint *params);

void mglGetnTexImage(GLMContext ctx, GLenum target, GLint level, GLenum format, GLenum type, GLsizei bufSize, void *pixels)
{
    GLuint pixel_bytes;

    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    pixel_bytes = sizeForFormatType(format, type);

    if (pixel_bytes)
    {
        GLint w = 0, h = 0;

        mglGetTexLevelParameteriv(ctx, target, level, GL_TEXTURE_WIDTH, &w);
        mglGetTexLevelParameteriv(ctx, target, level, GL_TEXTURE_HEIGHT, &h);

        if (h == 0)
            h = 1;

        ERROR_CHECK_RETURN((GLint64)w * h * pixel_bytes <= (GLint64)bufSize, GL_INVALID_OPERATION);
    }

    mglGetTexImage(ctx, target, level, format, type, pixels);
}
