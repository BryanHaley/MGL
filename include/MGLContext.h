/*
  MGLContext.h
  mgl

  Created by Michael Larson on 12/17/21.
*/

#ifndef MGLContext_h
#define MGLContext_h

// probably a reason for this... can't remember
#ifndef __GLM_CONTEXT_
#define __GLM_CONTEXT_
typedef struct GLMContextRec_t *GLMContext;
#endif

enum {
    MGL_PIXEL_FORMAT,
    MGL_PIXEL_TYPE,
    MGL_DEPTH_FORMAT,
    MGL_DEPTH_TYPE,
    MGL_STENCIL_FORMAT,
    MGL_STENCIL_TYPE,
    MGL_CONTEXT_FLAGS,
    MGL_SWAP_INTERVAL,

    // running counts of costly driver work, for tests and profiling
    MGL_STAT_PIPELINES_BUILT,       // Metal render pipelines compiled
    MGL_STAT_PIPELINE_LOOKUPS,      // pipeline descriptors built to look one up
    MGL_STAT_GPU_WAITS,             // times the CPU waited for the GPU to finish
    MGL_STAT_RENDER_PASSES,         // render passes started
    MGL_STAT_BUFFER_REBINDS,        // stage buffer lists rebound on the encoder
    MGL_STAT_CPU_EXPANSIONS,        // draws whose indices were expanded on the CPU
    MGL_STAT_PROGRAM_REBUILDS,      // times a program's Metal functions were remade
    MGL_STAT_ATTRIB_CONVERSIONS     // vertex attributes converted to a format Metal reads
};

#ifdef __cplusplus
extern "C" {
#endif

GLMContext createGLMContext(GLenum format, GLenum type,
                            GLenum depth_format, GLenum depth_type,
                            GLenum stencil_format, GLenum stencil_type);

// A context with the buffers, textures, renderbuffers, samplers, shaders,
// programs and syncs of share, or its own when share is NULL. Each context
// keeps its own vertex arrays, framebuffers, transform feedback objects,
// program pipelines and queries. Contexts in a group can be current on
// different threads at once.
GLMContext createGLMContextShared(GLenum format, GLenum type,
                                  GLenum depth_format, GLenum depth_type,
                                  GLenum stencil_format, GLenum stencil_type,
                                  GLMContext share);

GLuint sizeForFormatType(GLenum format, GLenum type);
GLuint bicountForFormatType(GLenum format, GLenum type, GLenum component);

// frees the context, every object in it, and its hold on the renderer
void destroyGLMContext(GLMContext ctx);

GLMContext MGLgetCurrentContext(void);
void MGLsetCurrentContext(GLMContext ctx);

// MGLswapBuffers can take NULL for the ctx, in this case it will use the current ctx
void MGLswapBuffers(GLMContext ctx);

// 0 turns vsync off, anything else on. NULL ctx means the current one.
void MGLsetSwapInterval(GLMContext ctx, int interval);

// GL_CONTEXT_FLAGS for the context, such as GL_CONTEXT_FLAG_NO_ERROR_BIT,
// which stops it recording errors. NULL ctx means the current one.
void MGLsetContextFlags(GLMContext ctx, GLuint flags);

// MGLget can take NULL for the ctx, in this case it will use the current ctx
void MGLget(GLMContext ctx, GLenum param, GLuint *data);

// Metal interop. NULL ctx means the current one.
// The id<MTLTexture> behind a GL texture, up to date with the GL work so far,
// or NULL. It can be remade later, so look it up each time.
void *MGLgetMetalTexture(GLMContext ctx, GLuint texture);
// The id<MTLCommandBuffer> GL is recording into, with no encoder open. Work
// encoded on it runs after earlier GL commands and before later ones.
void *MGLgetMetalCommandBuffer(GLMContext ctx);

#ifdef __cplusplus
};
#endif

#endif
