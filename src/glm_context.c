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
 * glm_context.c
 * MGL
 *
 */


#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <stdint.h>

#include <assert.h>

#include "glm_context.h"
#include "vertex_arrays.h"
#include "MGLRenderer.h"
#include "error.h"
#include "mgl_log.h"
#include "programs.h"
#include "shaders.h"
#include "buffers.h"

extern void getMacOSDefaults(GLMContext glm_ctx);
extern void init_dispatch(GLMContext ctx);

GLMContext _ctx = NULL;

/* Declared in MGLRenderer.m */
extern void* CppCreateMGLRendererHeadless(void *glm_ctx);

/* Auto-initialize MGL with headless renderer when library loads.
 * Headless = offscreen rendering, QEMU blits the framebuffer to screen.
 */
__attribute__((constructor))
static void mgl_auto_init(void) {
    if (_ctx == NULL) {
        _ctx = createGLMContext(GL_RGBA, GL_UNSIGNED_BYTE,
                               GL_DEPTH_COMPONENT24, GL_UNSIGNED_INT,
                               GL_STENCIL_INDEX8, GL_UNSIGNED_BYTE);
        CppCreateMGLRendererHeadless(_ctx);
        MGL_INFO("MGL: Initialized headless Metal renderer\n");
    }
}

/* Lazy-initialize MGL context on first GL API call if auto-init didn't run */
void mgl_lazy_init(void) {
    // If `_ctx` ever gets corrupted (e.g. memory stomp), it can become a small
    // non-NULL value and crash immediately on dereference. Detect and recover.
    if (_ctx != NULL && (uintptr_t)_ctx < 0x10000u) {
        MGL_ERR("MGL ERROR: current context pointer looks corrupted (%p); reinitializing\n", (void *)_ctx);
        _ctx = NULL;
    }

    if (_ctx == NULL) {
        mgl_auto_init();
    }
}

GLMContext mglGetContext(void)
{
    return _ctx;
}

GLMContext createGLMContext(GLenum format, GLenum type,
                            GLenum depth_format, GLenum depth_type,
                            GLenum stencil_format, GLenum stencil_type)
{
    GLMContext ctx = (GLMContext)malloc(sizeof(GLMContextRec));
    GLMContext save = _ctx;
    int err;

    bzero((void *)ctx, sizeof(GLMContextRec));

    _ctx = ctx;

    // vsync on unless the application asks for something else, which is what
    // MGL did before there was a way to ask
    ctx->swap_interval = 1;

    ctx->pixel_format.format = format;
    ctx->pixel_format.type = type;

    if ((format == 0) && (type == 0))
    {
        format = GL_UNSIGNED_INT;
        type = GL_UNSIGNED_INT_8_8_8_8_REV;
    }

    ctx->pixel_format.mtl_pixel_format = mtlPixelFormatForGLFormatType(format, type);

    if (depth_format)
    {
        ctx->depth_format.format = depth_format;
        ctx->depth_format.type = depth_type;
        ctx->depth_format.mtl_pixel_format = mtlPixelFormatForGLFormatType(depth_format, depth_type);
    }

    if (stencil_format)
    {
        ctx->stencil_format.format = stencil_format;
        ctx->stencil_format.type = stencil_type;
        ctx->stencil_format.mtl_pixel_format = mtlPixelFormatForGLFormatType(stencil_format, stencil_type);
    }

    // use a CGL context to read guestimates of gl params for installed GPU
    getMacOSDefaults(ctx);

    assert(STATE(max_color_attachments) <= MAX_COLOR_ATTACHMENTS);
    assert(STATE(max_vertex_attribs) <= MAX_ATTRIBS);

    STATE(draw_buffer) = GL_FRONT;
    STATE(default_draw_buffer) = GL_FRONT;
    STATE(read_buffer) = GL_FRONT;
    STATE(active_texture) = 0;

    STATE(pack.swap_bytes) = false;
    STATE(pack.lsb_first) = false;
    STATE(pack.row_length) = 0;
    STATE(pack.image_height) = 0;
    STATE(pack.skip_rows) = 0;
    STATE(pack.skip_pixels) = 0;
    STATE(pack.skip_images) = 0;
    STATE(pack.alignment) = 4;

    STATE(unpack.swap_bytes) = false;
    STATE(unpack.lsb_first) = false;
    STATE(unpack.row_length) = 0;
    STATE(unpack.image_height) = 0;
    STATE(unpack.skip_rows) = 0;
    STATE(unpack.skip_pixels) = 0;
    STATE(unpack.skip_images) = 0;
    STATE(unpack.alignment) = 4;

    STATE(caps.blend) = false;
    STATE(caps.line_smooth) = false;
    STATE(caps.polygon_smooth) = false;
    STATE(caps.cull_face) = false;
    STATE(caps.depth_test) = false;
    STATE(caps.stencil_test) = false;
    STATE(caps.dither) = true;
    STATE(caps.scissor_test) = false;
    STATE(caps.color_logic_op) = false;
    STATE(caps.polygon_offset_point) = false;
    STATE(caps.polygon_offset_line) = false;
    STATE(caps.polygon_offset_fill) = false;
    STATE(caps.index_logic_op) = false;
    STATE(caps.multisample) = true;
    STATE(caps.sample_alpha_to_coverage) = false;
    STATE(caps.sample_alpha_to_one) = false;
    STATE(caps.sample_coverage) = false;
    STATE(caps.rasterizer_discard) = false;
    STATE(caps.framebuffer_srgb) = false;
    STATE(caps.primitive_restart) = false;
    STATE(caps.depth_clamp) = false;
    STATE(caps.texture_cube_map_seamless) = false;
    STATE(caps.sample_mask) = false;
    STATE(caps.sample_shading) = false;
    STATE(caps.primitive_restart_fixed_index) = false;
    STATE(caps.debug_output_synchronous) = false;
    STATE(caps.debug_output) = false;

    STATE(var.cull_face_mode) = GL_BACK;
    STATE(var.front_face) = GL_CCW;

    STATE(hints.line_smooth_hint) = GL_DONT_CARE;
    STATE(hints.polygon_smooth_hint) = GL_DONT_CARE;
    STATE(hints.texture_compression_hint) = GL_DONT_CARE;
    STATE(hints.fragment_shader_derivative_hint) = GL_DONT_CARE;

    STATE(var.line_width) = 1.0f;
    STATE(var.point_size) = 1.0f;
    STATE(var.polygon_mode) = GL_FILL;

    STATE(scissor[0].x) = 0;
    STATE(scissor[0].y) = 0;
    STATE(scissor[0].width) = 0;   // needs to be set on binding to window
    STATE(scissor[0].height) = 0;  // needs to be set on binding to window

    // Initialize viewport to default size - critical for rendering
    STATE(viewport[0].x) = 0;
    STATE(viewport[0].y) = 0;
    STATE(viewport[0].w) = 1024;  // Default width - should be updated when window is bound
    STATE(viewport[0].h) = 768;   // Default height - should be updated when window is bound

    for(int i=0; i<MAX_VIEWPORTS; i++)
    {
        STATE(depth_range[i].znear) = 0.0;
        STATE(depth_range[i].zfar) = 1.0;
    }

    // a disabled array feeds (0,0,0,1) until glVertexAttrib says otherwise
    for(int i=0; i<MAX_ATTRIBS; i++)
    {
        STATE(attrib_constant[i]).v.f[0] = 0.0f;
        STATE(attrib_constant[i]).v.f[1] = 0.0f;
        STATE(attrib_constant[i]).v.f[2] = 0.0f;
        STATE(attrib_constant[i]).v.f[3] = 1.0f;
        STATE(attrib_constant[i]).type = _ATTRIB_CONST_FLOAT;
        STATE(attrib_constant[i]).d_valid = GL_FALSE;
    }

    for(int i=0; i<MAX_COLOR_ATTACHMENTS; i++)
    {
        STATE(var.blend_src_rgb[i]) = GL_ONE;
        STATE(var.blend_src_alpha[i]) = GL_ONE;
        STATE(var.blend_dst_rgb[i]) = GL_ZERO;
        STATE(var.blend_dst_alpha[i]) = GL_ZERO;
        STATE(var.blend_equation_rgb[i]) = GL_FUNC_ADD;
        STATE(var.blend_equation_alpha[i]) = GL_FUNC_ADD;
    }

    STATE(var.depth_func) = GL_LESS;
    STATE(var.depth_clear_value) = 1.0;

    // Initialize default clear color to opaque black as per OpenGL spec
    STATE(color_clear_value[0]) = 0.0f;
    STATE(color_clear_value[1]) = 0.0f;
    STATE(color_clear_value[2]) = 0.0f;
    STATE(color_clear_value[3]) = 1.0f;

#ifdef MGL_COMPAT_PROFILE
    STATE(var.alpha_test_func) = GL_ALWAYS;
    STATE(var.alpha_test_ref) = 0.0f;
#endif

    STATE(var.logic_op) = GL_COPY;
    STATE(var.stencil_func) = GL_ALWAYS;

    STATE(var.stencil_fail) = GL_KEEP;
    STATE(var.stencil_pass_depth_fail) = GL_KEEP;
    STATE(var.stencil_pass_depth_pass) = GL_KEEP;

    for(int i=0; i<MAX_CLIP_DISTANCES; i++)
    {
        STATE(caps.clip_distances[i]) = false;
    }

    STATE(var.stencil_fail) = GL_KEEP;
    STATE(var.stencil_pass_depth_fail) = GL_KEEP;
    STATE(var.stencil_pass_depth_pass) = GL_KEEP;
    STATE(var.stencil_back_fail) = GL_KEEP;
    STATE(var.stencil_fail) = GL_KEEP;
    STATE(var.stencil_back_pass_depth_fail) = GL_KEEP;
    STATE(var.stencil_back_pass_depth_pass) = GL_KEEP;

    STATE(var.stencil_func) = GL_ALWAYS;
    STATE(var.stencil_ref) = 0;
    STATE(var.stencil_writemask) = 0xFFFFFFFF;

    STATE(var.stencil_back_func) = GL_ALWAYS;
    STATE(var.stencil_back_ref) = 0;
    STATE(var.stencil_back_writemask) = 0xFFFFFFFF;

    // plain uniforms are indexed by location, so report what we can actually hold
    STATE(var.max_uniform_locations) = MAX_UNIFORM_LOCATIONS;

    // left at zero these rejected every texture unit past the first
    STATE(var.max_combined_texture_image_units) = TEXTURE_UNITS;
    STATE(var.max_texture_image_units) = TEXTURE_UNITS;
    STATE(var.max_texture_size) = 16384;

    STATE(var.max_compute_work_group_invocations) = 1024;

    STATE(var.max_compute_work_group_count[0]) = 65535;
    STATE(var.max_compute_work_group_count[1]) = 65535;
    STATE(var.max_compute_work_group_count[2]) = 65535;

    STATE(var.max_compute_work_group_size[0]) = 1024;
    STATE(var.max_compute_work_group_size[1]) = 1024;
    STATE(var.max_compute_work_group_size[2]) = 256;

    for(int attachment=0; attachment<MAX_COLOR_ATTACHMENTS; attachment++)
    {
        STATE(caps.use_color_mask[attachment]) = false;

        for(int i=0; i<4; i++)
            STATE(var.color_writemask[attachment][i]) = GL_TRUE;
    }


    STATE(var.cull_face_mode) = GL_BACK;

    STATE(sync_name) = 1;

    STATE(dirty_bits) = DIRTY_ALL;

    initHashTable(&STATE(vao_table), 32);
    initHashTable(&STATE(buffer_table), 32);
    initHashTable(&STATE(texture_table), 32);
    initHashTable(&STATE(shader_table), 32);
    initHashTable(&STATE(program_table), 32);
    initHashTable(&STATE(program_pipeline_table), 32);
    initHashTable(&STATE(query_table), 32);

    STATE(debug).messages_enabled = GL_TRUE;
    initHashTable(&STATE(transform_feedback_table), 32);

    // the default transform feedback object is bound from the start
    STATE(transform_feedback) = getTransformFeedback(ctx, 0);

    // the default object is always there
    if (STATE(transform_feedback))
        STATE(transform_feedback)->created = GL_TRUE;
    initHashTable(&STATE(renderbuffer_table), 32);
    initHashTable(&STATE(framebuffer_table), 32);
    initHashTable(&STATE(sampler_table), 32);
    
    init_dispatch(ctx);

    ctx->assert_on_error = GL_TRUE;
    ctx->error_func = error_func;

    ctx->temp_element_buffer = NULL;
    
    err = glslang_initialize_process();

    if (!err)
        MGL_ERR("MGL Error: glslang would not initialise; shader compilation will fail\n");
    
    _ctx = save;

    return ctx;
}

void MGLsetCurrentContext(GLMContext ctx)
{
    _ctx = ctx;
}

GLMContext MGLgetCurrentContext(void)
{
    return _ctx;
}

void MGLget(GLMContext ctx, GLenum param, GLuint *data)
{
    if (ctx == NULL)
        ctx = _ctx;
    
    if (ctx == NULL)
        return;
    
    switch(param)
    {
        case MGL_PIXEL_FORMAT: *data = ctx->pixel_format.format; break;
        case MGL_PIXEL_TYPE: *data = ctx->pixel_format.type; break;
        case MGL_DEPTH_FORMAT: *data = ctx->depth_format.format; break;
        case MGL_DEPTH_TYPE: *data = ctx->depth_format.type; break;
        case MGL_STENCIL_FORMAT: *data = ctx->stencil_format.format; break;
        case MGL_STENCIL_TYPE: *data = ctx->stencil_format.type; break;
        case MGL_CONTEXT_FLAGS: *data = ctx->context_flags; break;
        case MGL_SWAP_INTERVAL: *data = ctx->swap_interval; break;

        default:
            // MGLget is an MGL entry point, not GL, so there is no error to set
            break;
    }
}

void MGLswapBuffers(GLMContext ctx)
{
    if (ctx == NULL)
        ctx = _ctx;

    if (ctx == NULL)
        return;

    ctx->mtl_funcs.mtlSwapBuffers(ctx);
}

// Zero means present as fast as the GPU draws; anything else paces the
// presents to the display, which is all Metal offers
void MGLsetSwapInterval(GLMContext ctx, int interval)
{
    if (ctx == NULL)
        ctx = _ctx;

    if (ctx == NULL)
        return;

    ctx->swap_interval = interval;
    if (ctx->mtl_funcs.mtlSetSwapInterval)
        ctx->mtl_funcs.mtlSetSwapInterval(ctx, interval);
}

// CRITICAL FIX: Proper context destruction to prevent memory leaks
// Everything a table still holds, freed the way deleting it would.
typedef void (*FreeObjectFunc)(GLMContext ctx, void *obj);

static void freeTable(GLMContext ctx, HashTable *table, FreeObjectFunc free_obj)
{
    if (table->keys == NULL)
        return;

    for (size_t i = 0; i < table->size; i++)
    {
        void *obj = table->keys[i].data;

        if (obj == NULL)
            continue;

        table->keys[i].data = NULL;
        free_obj(ctx, obj);
    }

    free(table->keys);
    table->keys = NULL;
}

static void freeProgramObj(GLMContext ctx, void *obj)   { mglFreeProgram(ctx, (Program *)obj); }
static void freeShaderObj(GLMContext ctx, void *obj)    { mglFreeShader(ctx, (Shader *)obj); }
static void freeTextureObj(GLMContext ctx, void *obj)   { mglFreeTextureObject(ctx, (Texture *)obj); }
static void freePlainObj(GLMContext ctx, void *obj)     { (void)ctx; free(obj); }

static void freeBufferObj(GLMContext ctx, void *obj)
{
    mglReleaseBufferStorage(ctx, (Buffer *)obj);
    free(obj);
}

static void freeRenderbufferObj(GLMContext ctx, void *obj)
{
    Renderbuffer *rbo = (Renderbuffer *)obj;

    if (rbo->tex)
        mglFreeTextureObject(ctx, rbo->tex);

    free(rbo);
}

static void freeSamplerObj(GLMContext ctx, void *obj)
{
    Sampler *smp = (Sampler *)obj;

    if (smp->mtl_data)
        ctx->mtl_funcs.mtlDeleteMTLObj(ctx, smp->mtl_data);

    free(smp);
}

// Frees the context and everything in it. Whatever the GPU is still doing
// finishes first, since it may be reading any of it.
void destroyGLMContext(GLMContext ctx)
{
    if (ctx == NULL)
        return;

    MGL_INFO("MGL INFO: Destroying GLMContext\n");

    GLMContext save = _ctx;

    _ctx = ctx;

    if (ctx->mtl_funcs.mtlObj && ctx->mtl_funcs.mtlFlush)
        ctx->mtl_funcs.mtlFlush(ctx, true);

    // programs before shaders: freeing a program lets go of the shaders it
    // held, and frees the ones already deleted
    freeTable(ctx, &ctx->state.program_table, freeProgramObj);
    freeTable(ctx, &ctx->state.shader_table, freeShaderObj);
    freeTable(ctx, &ctx->state.texture_table, freeTextureObj);

    for (GLuint i = 0; i < ctx->state.retired_texture_count; i++)
        mglFreeTextureObject(ctx, ctx->state.retired_textures[i]);

    for (int i = 0; i < _MAX_TEXTURE_TYPES; i++)
        if (ctx->state.default_textures[i])
            mglFreeTextureObject(ctx, ctx->state.default_textures[i]);

    free(ctx->state.retired_textures);
    ctx->state.retired_textures = NULL;
    ctx->state.retired_texture_count = 0;

    freeTable(ctx, &ctx->state.buffer_table, freeBufferObj);
    freeTable(ctx, &ctx->state.renderbuffer_table, freeRenderbufferObj);
    freeTable(ctx, &ctx->state.framebuffer_table, freePlainObj);
    freeTable(ctx, &ctx->state.vao_table, freePlainObj);
    freeTable(ctx, &ctx->state.sampler_table, freeSamplerObj);
    freeTable(ctx, &ctx->state.query_table, freePlainObj);
    freeTable(ctx, &ctx->state.transform_feedback_table, freePlainObj);
    freeTable(ctx, &ctx->state.program_pipeline_table, freePlainObj);

    if (ctx->state.client_indices)
        freeBufferObj(ctx, ctx->state.client_indices);

    while (ctx->state.sync_list)
    {
        Sync *sync = ctx->state.sync_list;

        ctx->state.sync_list = sync->next;

        if (ctx->mtl_funcs.mtlForgetSync)
            ctx->mtl_funcs.mtlForgetSync(ctx, sync);

        free(sync);
    }

    free(ctx->bindless.handles);

    mglForgetContextLabels(ctx);

    // the renderer kept itself, and the view it draws into, alive through these
    if (ctx->mtl_funcs.mtlView)
        mtlReleaseRetained(ctx->mtl_funcs.mtlView);

    if (ctx->mtl_funcs.mtlObj)
        mtlReleaseRetained(ctx->mtl_funcs.mtlObj);

    ctx->mtl_funcs.mtlView = NULL;
    ctx->mtl_funcs.mtlObj = NULL;

    _ctx = (save == ctx) ? NULL : save;

    free(ctx);
}

// CRITICAL FIX: Library destructor for proper cleanup
__attribute__((destructor))
static void mgl_auto_cleanup(void)
{
    if (_ctx != NULL) {
        MGL_INFO("MGL INFO: Auto-cleanup - destroying GLMContext\n");

        // Signal cleanup to any in-flight operations
        // The MGLRenderer dealloc will handle Metal resource cleanup

        _ctx = NULL;
        MGL_INFO("MGL INFO: Auto-cleanup completed\n");
    }
}

