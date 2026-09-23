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
 * glm_context.h
 * MGL
 *
 */

#ifndef glm_context_h
#define glm_context_h

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include <mach/vm_types.h>
#include <glslang_c_interface.h>
#include <glslang_c_shader_types.h>

#include "glm_dispatch.h"

#include "hash_table.h"

#include "mgl_compat.h"

// defines above set sizes in glm_params
#include "glm_params.h"
#include "mgl_reflect.h"

#ifdef DEBUG
#define DEBUG_LEVEL 3
#endif

#if defined(DEBUG_LEVEL) && DEBUG_LEVEL > 0
 #define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG: %s:%d: " fmt, \
    __func__, __LINE__, ##args)
#else
 #define DEBUG_PRINT(fmt, args...) /* Don't do anything in release builds */
#endif


// macros because I get tired of write if this and that then return
#define RETURN_ON_FAILURE(_expr_) if (_expr_ == false) { printf("failure %s:%d\n",__FUNCTION__,__LINE__); return; }
#define RETURN_FALSE_ON_FAILURE(_expr_) if (_expr_ == false) { printf("failure %s:%d\n",__FUNCTION__,__LINE__); return false; }
#define RETURN_FALSE_ON_NULL(_expr_) if (_expr_ == NULL) { printf("failure %s:%d\n",__FUNCTION__,__LINE__); return false; }
#define RETURN_NULL_ON_FAILURE(_expr_) if (_expr_ == false) { printf("failure %s:%d\n",__FUNCTION__,__LINE__); return NULL; }
#define RETURN_ON_NULL(_expr_) if (_expr_ == NULL) { printf("failure %s:%d\n",__FUNCTION__,__LINE__); return; }

#define STATE(_VAR_)     ctx->state._VAR_
#define STATE_VAR(_VAR_) ctx->state.var._VAR_

#define VAO()   ctx->state.vao
#define VAO_STATE(_val_)   ctx->state.vao->_val_
#define VAO_ATTRIB_STATE(_index_) ctx->state.vao->attrib[_index_]
// the buffer an attribute reads from, which GL reaches through its binding
#define VAO_BINDING(_vao_, _index_) (&(_vao_)->bindings[(_vao_)->attrib[_index_].buffer_bindingindex])
#define VAO_ATTRIB_BINDING(_index_) VAO_BINDING(ctx->state.vao, _index_)

// These really do return. Without that, every error path fell through and kept
// running with the arguments it had just rejected.
#define ERROR_RETURN(_type_) do { ctx->error_func(ctx, __FUNCTION__, _type_); return; } while(0)
#define ERROR_RETURN_VALUE(_type_, _val_) do { ctx->error_func(ctx, __FUNCTION__, _type_); return _val_; } while(0)
#define ERROR_CHECK_RETURN(_expr_, _type_) do { if ((_expr_) == false) { ctx->error_func(ctx, __FUNCTION__, _type_); return; } } while(0)
#define ERROR_CHECK_RETURN_VALUE(_expr_, _type_, _val_) do { if ((_expr_) == false) { ctx->error_func(ctx, __FUNCTION__, _type_); return _val_; } } while(0)

enum {
    _TEXTURE_BUFFER = 0, // duplicate of _TEXTURE_BUFFER_TARGET
    _ARRAY_BUFFER,
    _ELEMENT_ARRAY_BUFFER,
    _UNIFORM_BUFFER,
    _UNIFORM_CONSTANT,
    _SHADER_STORAGE_BUFFER,
    _TRANSFORM_FEEDBACK_BUFFER,
    _QUERY_BUFFER,
    _PIXEL_PACK_BUFFER,
    _PIXEL_UNPACK_BUFFER,
    _ATOMIC_COUNTER_BUFFER,
    _COPY_READ_BUFFER,
    _COPY_WRITE_BUFFER,
    _DISPATCH_INDIRECT_BUFFER,
    _DRAW_INDIRECT_BUFFER,
    _MAX_BUFFER_TYPES
};

enum {
    _TEXTURE_BUFFER_TARGET = 0, // duplicate of _TEXTURE_BUFFER
    _TEXTURE_1D,
    _TEXTURE_2D,
    _TEXTURE_3D,
    _TEXTURE_RECTANGLE,
    _TEXTURE_1D_ARRAY,
    _TEXTURE_2D_ARRAY,
    _TEXTURE_CUBE_MAP,
    _TEXTURE_CUBE_MAP_ARRAY,
    _TEXTURE_2D_MULTISAMPLE,
    _TEXTURE_2D_MULTISAMPLE_ARRAY,
    _RENDERBUFFER,
    _MAX_TEXTURE_TYPES
};

static_assert(_TEXTURE_BUFFER == _TEXTURE_BUFFER_TARGET, "_TEXTURE_BUFFER != _TEXTURE_BUFFER_TARGET");

enum {
    _VERTEX_SHADER = 0,
    _TESS_CONTROL_SHADER,
    _TESS_EVALUATION_SHADER,
    _GEOMETRY_SHADER,
    _FRAGMENT_SHADER,
    _COMPUTE_SHADER,
    _MAX_SHADER_TYPES
};

enum {
    _UNIFORM_BASE = 0,
    _TRANSFORM_FEEDBACK_BASE,
    _SHADER_STORAGE_BASE,
    _ATOMIC_COUNTER_BASE,
    _MAX_BASE_TARGET
};

#include "spirv_cross_c.h"

// Upstream SPIRV-Cross adopted MGL's local "uniform constant" resource class and
// named it a plain GL uniform. Enum constants are invisible to the preprocessor,
// so key off the C API version instead: the fork MGL has carried is 0.49.
#if defined(SPVC_C_API_VERSION_MINOR) && SPVC_C_API_VERSION_MINOR >= 60
#define SPVC_RESOURCE_TYPE_UNIFORM_CONSTANT SPVC_RESOURCE_TYPE_GL_PLAIN_UNIFORM
#endif

#define MAX_SPVC_RESOURCE_TYPES 20

enum {
    _UNKNOWN_RES = 0,
    _UNIFORM_BUFFER_RES,
    _UNIFORM_CONSTANT_RES,
    _STORAGE_BUFFER_RES,
    _STAGE_INPUT_RES,
    _STAGE_OUTPUT_RES,
    _STORAGE_OUTPUT_RES,
    _ATOMIC_COUNTER_RES,
    _PUSH_CONSTANT_RES,
    _SEPARATE_IMAGE_RES,
    _SEPARATE_SAMPLERS_RES,
    _ACCEL_STRUCT_RES,
    _RAY_QUERY,
    _MAX_SPIRV_RES
};

#define SHADER_MASK_BIT(_TYPE_)    (0x1 << _TYPE_)
#define VERTEX_SHADER_MASK_BIT  SHADER_MASK_BIT(_VERTEX_SHADER)
#define FRAGMENT_SHADER_MASK_BIT  SHADER_MASK_BIT(_FRAGMENT_SHADER)
#define GEOMETRY_SHADER_MASK_BIT  SHADER_MASK_BIT(_GEOMETRY_SHADER)
#define TESS_CONTROL_SHADER_MASK_BIT  SHADER_MASK_BIT(_TESS_CONTROL_SHADER)
#define TESS_EVALUATION_SHADER_MASK_BIT  SHADER_MASK_BIT(_TESS_EVALUATION_SHADER)
#define COMPUTE_SHADER_MASK_BIT  SHADER_MASK_BIT(_COMPUTE_SHADER)

#define DIRTY_BUFFER_DATA   0x1
#define DIRTY_BUFFER_ADDR   (DIRTY_BUFFER_DATA << 1)

#define DIRTY_TEXTURE_LEVEL 0x1
#define DIRTY_TEXTURE_DATA  (DIRTY_TEXTURE_LEVEL << 1)
#define DIRTY_TEXTURE_PARAM (DIRTY_TEXTURE_DATA << 1)
#define DIRTY_TEXTURE_ACCESS (DIRTY_TEXTURE_PARAM << 1)
// Metal fixes a texture's swizzle in its descriptor, so changing it needs a
// new texture or a view -- the sampler alone will not do it.
#define DIRTY_TEXTURE_SWIZZLE (DIRTY_TEXTURE_ACCESS << 1)

#define DIRTY_FBO_BINDING   0x1
#define DIRTY_FBO_TEX      (DIRTY_FBO_BINDING << 1)

#define DIRTY_RENDBUF       0x1
#define DIRTY_RENDBUF_TEX   (DIRTY_RENDBUF << 1)

#define DIRTY_VAO_BUFFER_BASE  0x1
#define DIRTY_VAO_ATTRIB       (DIRTY_VAO_BUFFER_BASE << 1)

typedef struct {
    unsigned int  count;
    unsigned int  instanceCount;
    unsigned int  first;
    unsigned int  baseInstance;
} DrawArraysIndirectCommand;

typedef struct {
    unsigned int  count;
    unsigned int  instanceCount;
    unsigned int  first;
    int  baseVertex;
    unsigned int  baseInstance;
} DrawElementsIndirectCommand;

typedef struct BufferData_t {
    GLuint          dirty_bits;
    size_t          buffer_size;
    vm_address_t    buffer_data;
    void            *mtl_data;
} BufferData;

#define BUFFER_IMMUTABLE_STORAGE_FLAG   0x1
#define BUFFER_MAP_PERSISTENT_BIT       (BUFFER_IMMUTABLE_STORAGE_FLAG << 1)

typedef struct Buffer_t {
    GLuint name;
    GLenum target;
    GLuint index;
    GLsizeiptr size;
    GLenum usage;
    GLenum access;
    GLbitfield access_flags;
    GLboolean immutable_storage; // GL_BUFFER_IMMUTABLE_STORAGE
    GLboolean mapped;
    GLuint storage_flags; // GL_BUFFER_STORAGE_FLAGS
    GLsizeiptr mapped_offset;
    GLsizeiptr mapped_length;
    BufferData data;
} Buffer;

typedef struct BufferBaseTarget_t {
    GLuint      buffer;
    GLsizeiptr  offset;
    GLsizeiptr  size;
    Buffer      *buf;
} BufferBaseTarget;

// Vertex buffer binding slots -- glBindVertexBuffer and friends. GL asks for 16.
#define MAX_BINDABLE_BUFFERS    16

// The indexed buffer targets each have their own binding space and their own
// spec minimum, so they get their own ceilings rather than one shared number.
#define MAX_UNIFORM_BUFFER_BINDINGS         96
#define MAX_SHADER_STORAGE_BUFFER_BINDINGS  32
#define MAX_ATOMIC_COUNTER_BUFFER_BINDINGS  16
// what the capture machinery can really lay out, which is the spec minimum;
// MGL_XFB_MAX_BUFFERS and MAX_TF_BUFFERS are the same number seen from the
// shader-rewrite and the binding side
#define MAX_TRANSFORM_FEEDBACK_BUFFERS      4

// one array wide enough for the largest of them
#define MAX_BUFFER_BASE_BINDINGS            MAX_UNIFORM_BUFFER_BINDINGS

typedef struct BufferBase_t {
    BufferBaseTarget    buffers[MAX_BUFFER_BASE_BINDINGS];
} BufferBase;

// plain uniforms are stored by their layout location, which GL lets run well
// past the number of buffer binding points
#define MAX_UNIFORM_LOCATIONS   1024
typedef struct UniformConstants_t {
    BufferBaseTarget    buffers[MAX_UNIFORM_LOCATIONS];
    // 4 for float/int uniforms, 8 for double ones. Without this a readback
    // cannot tell stored doubles from stored floats.
    GLubyte             elem_size[MAX_UNIFORM_LOCATIONS];
} UniformConstants;

typedef struct TextureParameter_t {
    GLenum  depth_stencil_mode;
    GLuint  base_level;
    GLfloat border_color[4];
    GLint   border_color_i[4];
    GLuint   border_color_ui[4];
    GLenum  compare_func;
    GLenum  compare_mode;
    GLfloat lod_bias;
    GLenum  min_filter;
    GLenum  mag_filter;
    GLfloat max_anisotropy;
    GLfloat min_lod;
    GLfloat max_lod;
    GLuint  max_level;
    GLboolean swizzled;
    GLenum  swizzle_r;
    GLenum  swizzle_g;
    GLenum  swizzle_b;
    GLenum  swizzle_a;
    GLenum  wrap_s;
    GLenum  wrap_t;
    GLenum  wrap_r;
    void *mtl_data;
} TextureParameter;

typedef struct TextureLevel_t {
    GLboolean complete;
    GLuint width;
    GLuint height;
    GLuint depth;
    size_t pitch;
    GLuint mtl_format;
    size_t  data_size;
    vm_address_t data;
} TextureLevel;

enum {
    _CUBE_MAP_POSITIVE_X = 0,
    _CUBE_MAP_NEGATIVE_X,
    _CUBE_MAP_POSITIVE_Y,
    _CUBE_MAP_NEGATIVE_Y,
    _CUBE_MAP_POSITIVE_Z,
    _CUBE_MAP_NEGATIVE_Z,
    _CUBE_MAP_MAX_FACE
};

typedef struct TextureFace_t {
    TextureLevel    *levels;
} TextureFace;

#define DIRTY_SAMPLER_PARAM   0x1
typedef struct Sampler_t {
    GLuint dirty_bits;
    GLuint name;
    TextureParameter params;
    void *mtl_data;
} Sampler;

typedef struct Texture_t {
    GLuint dirty_bits;
    GLuint dirty_on_gpu;
    GLboolean is_render_target;
    GLenum access;
    GLboolean immutable_storage;
    GLuint name;
    GLuint target;
    GLuint index;
    GLuint mipmapped;
    GLboolean genmipmaps;
    GLboolean mtl_requires_private_storage; // depth, multi sample
    TextureParameter params;

    // base level params
    GLenum internalformat;
    GLuint width;
    GLuint height;
    GLuint depth;
    GLboolean is_array;
    GLboolean complete;
    GLuint num_levels;
    GLuint mipmap_levels;
    TextureFace faces[6];
    void    *mtl_data;
    // the swizzle the Metal texture was built with, so a later change is seen
    GLuint  mtl_swizzle;
    GLsizei samples;
    // a buffer texture is a view of one MTLBuffer; which one, so a buffer given
    // new storage gets a new view instead of reading the freed one
    void    *mtl_buffer_src;
} Texture;

typedef struct TextureUnit_t {
    Texture *textures[_MAX_TEXTURE_TYPES];
} TextureUnit;

typedef struct ImageUnit_t {
    GLuint unit;
    GLuint texture;
    GLuint level;
    GLboolean layered;
    GLint layer;
    GLenum access;
    GLenum internalformat;
    Texture *tex;
} ImageUnit;

typedef struct BufferBinding_t {
    Buffer  *buffer;
    GLintptr offset;
    GLsizei stride;
    GLuint divisor;
} BufferBinding;

typedef struct VertexAttrib_t {
    GLuint  size;
    GLenum  type;
    GLuint  normalized;
    // the stride glVertexAttribPointer was handed, which GL reports back
    // unchanged; the stride the hardware walks lives on the binding
    GLuint  stride;
    // what glVertexAttribPointer was handed, which GL hands back; the offset
    // the hardware reads from lives on the binding
    GLintptr  pointer;
    GLintptr  relativeoffset;
    GLuint  buffer_bindingindex;
} VertexAttrib;

typedef struct VertexElementArray_t {
    Buffer  *buffer;
    GLenum  type;
    GLuint  size;
    const void *ptr;
} VertexElementArray;

#define MAX_VIEWPORTS 16

typedef struct ViewportRect_t {
    GLfloat x, y, w, h;
} ViewportRect;

typedef struct ScissorRect_t {
    GLint   x, y;
    GLsizei width, height;
} ScissorRect;

typedef struct DepthRangeVal_t {
    GLdouble znear, zfar;
} DepthRangeVal;

typedef enum {
    _ATTRIB_CONST_FLOAT = 0,
    _ATTRIB_CONST_INT   = 1,
    _ATTRIB_CONST_UINT  = 2
} AttribConstType;

// value a disabled vertex array feeds the shader
typedef struct AttribConstant_t {
    union { GLfloat f[4]; GLint i[4]; GLuint u[4]; } v;
    AttribConstType type;
    // glVertexAttribL* doubles do not fit in the union above, and the shader
    // still wants the narrowed copy, so the exact value rides alongside it.
    GLdouble  d[4];
    GLboolean d_valid;
} AttribConstant;

typedef struct VertexArray_t {
    GLuint dirty_bits;
    unsigned name;
    unsigned enabled_attribs;
    VertexAttrib attrib[MAX_ATTRIBS];
    BufferBinding bindings[MAX_BINDABLE_BUFFERS];
    VertexElementArray element_array;
    void *mtl_data;
} VertexArray;

// The buffers the geometry emulation uses, high enough not to collide with
// the storage buffers a shader declares for itself.
#define MGL_GS_IN_BINDING     12
#define MGL_GS_OUT_BINDING    13
#define MGL_GS_INDEX_BINDING  14

// and where they are pinned in Metal's buffer slots, above everything the
// vertex descriptor and the uniform blocks use
#define MGL_GS_IN_MSL_SLOT    27
#define MGL_GS_OUT_MSL_SLOT   28
#define MGL_GS_INDEX_MSL_SLOT 29

// Point-mode isolines ahead of a geometry shader: each patch gets a fixed
// block of input slots, one per point the highest level could make.
#define MGL_TES_MAX_LEVEL        64
#define MGL_TES_POINTS_PER_PATCH (MGL_TES_MAX_LEVEL * (MGL_TES_MAX_LEVEL + 1))
// isolines drawn as lines: every segment of every line gets two slots
#define MGL_TES_SEGMENTS_PER_PATCH (MGL_TES_MAX_LEVEL * MGL_TES_MAX_LEVEL)
// Triangles and quads cut up on the CPU (tessellator.c): a patch has at most
// 65 x 65 vertices, which is also the grid Metal is asked to run the
// evaluation shader over. Each patch's block in the coordinate buffer is a
// header, the outer levels, then one vec4 per vertex.
#define MGL_TES_GEN_VERTS    ((MGL_TES_MAX_LEVEL + 1) * (MGL_TES_MAX_LEVEL + 1))
#define MGL_TES_GEN_STRIDE   (MGL_TES_GEN_VERTS + 2)
#define MGL_TES_GEN_BINDING  15

typedef struct { float u, v, w; } MglTessCoord;

// Cuts one patch up the way GL does. domain 0 triangles, 1 quads, 2 isolines;
// spacing 0 equal, 1 fractional even, 2 fractional odd. Fills coords with the
// vertices and index with the primitives' vertices (three a triangle, two a
// line, one a point).
// Returns the vertex count, 0 for a dropped patch, -1 when it does not fit.
int mglTessellate(int domain, int spacing, bool point_mode, bool cw,
                  const float outer[4], const float inner[2],
                  MglTessCoord *coords, int max_coords,
                  uint32_t *index, int max_index, int *prim_count);

// Transform feedback writes through storage blocks of its own, starting here.
#define MGL_XFB_FIRST_BINDING 16
#define MGL_XFB_MAX_BUFFERS   MAX_TRANSFORM_FEEDBACK_BUFFERS
#define MGL_XFB_FIRST_MSL_SLOT 22

// where a stage finds the table of buffer lengths .length() reads
#define MGL_BUFFER_SIZES_MSL_SLOT 21

// Cull distance needs a pass of its own before the draw, and five buffers.
#define MGL_CULL_FIRST_BINDING  20
#define MGL_CULL_FIRST_MSL_SLOT 24

// Bindless handles are slots in one table of Metal texture IDs and one of
// sampler IDs. Each texture type a shader reads by handle is its own argument
// buffer. They take the lowest Metal buffer slots; SPIRV-Cross numbers the
// shader's other buffers after them.
#define MGL_BINDLESS_FIRST_MSL_SLOT 0
#define MGL_BINDLESS_MAX_SETS       6
#define MGL_BINDLESS_TEXTURES       32768
#define MGL_BINDLESS_SAMPLERS       1024

// One handle from glGetTextureHandleARB, glGetTextureSamplerHandleARB or
// glGetImageHandleARB. The value is the texture slot in the low half and the
// sampler slot, tagged, in the high half.
typedef struct MglHandle_t {
    GLuint64 value;
    struct Texture_t *tex;          // NULL once the texture is deleted
    GLuint sampler_name;            // 0 for the texture's own sampling state
    GLboolean image;
    GLint level;
    GLboolean layered;
    GLint layer;
    GLenum format;
    GLboolean resident;
    GLenum access;
    GLuint tex_slot;
    GLuint smp_slot;
    void *mtl_texture;              // what the table slot holds now, retained
    void *mtl_base;                 // the texture's Metal object at that time
} MglHandle;

typedef struct MglBindless_t {
    MglHandle *handles;
    GLuint count, cap;
    GLuint next_slot;
    GLuint serial;                  // changes whenever what must be resident does
} MglBindless;

typedef struct MglBindlessSets_t {
    int count;
    GLuint slot[MGL_BINDLESS_MAX_SETS];
    GLboolean is_sampler[MGL_BINDLESS_MAX_SETS];
} MglBindlessSets;

// What the cull distance rewrite found and built.
typedef struct CullInfo_t {
    GLint  count;            // gl_CullDistance array size, zero when unused
    char  *capture_src;      // the vertex shader that records the distances
    char  *kernel_src;       // the compute shader that drops whole primitives
    GLint  cap_out_slot;     // MglCullB in the capture vertex shader
    GLint  k_cull_slot, k_src_slot, k_out_slot, k_arg_slot, k_cfg_slot;
    GLint  building;         // 1 while the capture compiles, 2 for the kernel
} CullInfo;

void  mglReadColorFormatAndType(GLMContext ctx, GLenum *format, GLenum *type);
bool  mglDrawFramebufferComplete(GLMContext ctx);
GLint mglCullDistanceSize(const char *src);
bool  mglBuildCullShaders(const char *vs_src, CullInfo *ci);
void  mglFreeCullInfo(CullInfo *ci);

// What the transform feedback rewrite produced and how it laid the capture out.
typedef struct CaptureInfo_t {
    char   *rewritten_src;
    GLint   varying_count;
    GLint   stride_bytes[MGL_XFB_MAX_BUFFERS];  // one recorded vertex, per buffer
    GLint   buffer_count;
    GLboolean separate;
    GLboolean from_shader;      // laid out by the shader's xfb qualifiers
    // behind a geometry stage there is no rewrite, just a table of words to copy
    GLuint *gather;
    GLint   gather_words;
    // where the rewrite's own uniforms and buffers ended up
    GLint   on_loc, base_loc;
    GLint   buffer_slot[MGL_XFB_MAX_BUFFERS];
} CaptureInfo;

bool mglBuildTransformCapture(const char *src, void *shader, char *const *varyings, GLsizei count,
                              GLenum buffer_mode, const char *slot_expr, CaptureInfo *ci);
int  mglTransformCaptureItems(const char *src, void *shader, char *const *varyings, GLsizei count,
                              GLenum buffer_mode, MglXfbItem *items, int max_items,
                              GLint *stride_bytes, GLboolean *from_shader);
void mglFreeCaptureInfo(CaptureInfo *ci);

// What a geometry shader turned into, and what it needs to run.
typedef struct GeometryInfo_t {
    GLenum in_primitive;            // points / lines / triangles (+ adjacency)
    GLenum out_primitive;           // points / line_strip / triangle_strip
    GLint  max_vertices;
    GLint  invocations;
    GLint  in_vertices;             // vertices per input primitive
    GLint  out_vertices_per_primitive;
    GLint  slot_capacity;           // vertices one invocation may write
    char  *compute_src;             // the geometry shader, as a compute shader
    char  *passthrough_src;         // the vertex shader that draws its output
    char  *capture_decl;            // what the real vertex shader gains
    GLint  in_stride;               // bytes one captured vertex takes
    GLint  out_stride;              // bytes one emitted vertex takes
    // where each generated buffer landed in Metal, per stage that uses it
    GLint  vs_in_slot;
    GLint  gs_in_slot, gs_out_slot, gs_index_slot;
    GLint  pass_out_slot;
    GLint  tes_gen_slot;            // the CPU tessellator's coordinates
    // the three numbers the compute pass is told about this draw
    GLint  prims_loc, indexed_loc, first_loc, stride_loc;
    // where each output sits in one emitted vertex, for transform feedback
    // the input interface blocks the geometry stage declared, so the capture
    // injected into the vertex shader can spell them the way IT declares them
    GLint  in_block_count;
    char   in_block_names[8][64];
    char   in_block_insts[8][64];

    GLint  out_count;
    char   out_names[32][64];
    // what an application calls it: a block member is "Block.member", which is
    // the name transform feedback asks for
    char   out_gl_names[32][96];
    char   out_types[32][32];
    GLint  out_offsets[32];
} GeometryInfo;

// Transform feedback behind a geometry stage copies words straight out of the
// emitted vertices: three numbers per word, where from, which buffer, where to.
int mglGsGatherTable(const GeometryInfo *gi, const MglXfbItem *items, int count,
                     GLuint *table, int max_words);

// Names MGL generates for its own use, which GL must not see and the buffer
// mapping must not try to find a GL object for.
static inline bool mglResourceIsInternal(const char *name)
{
    // MGL's own names run straight on in capitals: mglGsIn, MglXfbB0
    return name && (!strncmp(name, "Mgl", 3) || !strncmp(name, "mgl", 3)) &&
           name[3] >= 'A' && name[3] <= 'Z';
}

void  mglInitLimits(GLMContext ctx);
// really a const glslang_resource_t *, but glslang's header is C++-adjacent
const void *mglGlslangResource(GLMContext ctx);

bool  mglRewriteGeometryShader(const char *src, GeometryInfo *gi);
char *mglAddTessPointCapture(const char *tes_src, const GeometryInfo *gi);
char *mglAddTessGeneralCapture(const char *tes_src, const GeometryInfo *gi);
char *mglTouchTessInputs(const char *tes_src, const char *writer_src, bool writer_is_control);
int   mglTesIsolineKind(const char *tes_src);    // 0 none, 1 point_mode, 2 lines
// what an evaluation shader's layout(...) in; says: domain 0 triangles, 1
// quads, 2 isolines, -1 none; spacing 0 equal, 1 fractional even, 2 odd
void  mglTesLayout(const char *tes_src, int *domain, int *spacing, bool *cw, bool *points);
// input 0 points, 1 lines, 2 triangles
char *mglPassThroughGeometry(const char *tes_src, int input);
char *mglAddGeometryCapture(const char *vs_src, const GeometryInfo *gi);
void  mglFreeGeometryInfo(GeometryInfo *gi);

// What a subroutine uniform turns into: an int selector named NAME__mglsr,
// and for arrays a dispatch function named NAME__mglcall taking the index.
// How many the rewrite can hold; GL's floor is 256 and 1024.
#define MAX_SUB_FNS_LIMIT     256
#define MAX_SUB_UNIFORM_LIMIT 1024

#define MGL_SUBROUTINE_SUFFIX "_mglsr"
#define MGL_SUBROUTINE_CALL   "__mglcall"
#define MGL_SUBROUTINE_INDEX  "mglsrIndex"

// What the rewrite found, so glGetSubroutineIndex and friends can answer.
typedef struct SubroutineInfo_t {
    char   **fn_names;           // every subroutine function in this stage
    GLuint   fn_count;
    char   **uniform_names;      // every subroutine uniform in this stage
    GLuint  *uniform_array_size;
    GLuint  *uniform_compatible; // how many functions each one accepts
    GLuint  *uniform_type;       // which subroutine type each uniform has
    GLint   *uniform_location;   // layout(location), -1 when none was given
    GLuint64 *fn_types;          // the types each function implements, one bit each
    GLuint   *fn_index;          // each function's GL index, from layout(index) or given
    GLuint   uniform_count;
    const char *error;           // set when the shader misuses a subroutine
} SubroutineInfo;

char *mglRewriteSubroutines(const char *src, SubroutineInfo *info);
bool  mglBufferTextureSource(GLMContext ctx, const Texture *tex, Buffer **buf,
                             GLintptr *offset, GLsizeiptr *size);
bool  mglSubroutineCompatible(const SubroutineInfo *info, GLuint uniform, GLuint fn);
GLint mglSubroutineSlot(const SubroutineInfo *info, GLuint index);
void  mglFreeSubroutineInfo(SubroutineInfo *info);
bool  mglCopySubroutineInfo(SubroutineInfo *dst, const SubroutineInfo *src);

typedef struct Shader_t {
    GLuint dirty_bits;
    GLuint name;
    GLuint type;
    GLuint glm_type;
    const char *mtl_shader_type_name;
    size_t src_len;
    const char *src;
    // what the source looks like once the preprocessor has had it; the cull
    // distance rewrite needs to see through the application's own macros
    char *pp_src;
    glslang_shader_t *compiled_glsl_shader;
    const char *entry_point;
    char *log;
    int refcount;
    GLboolean delete_status;
    struct {
        void *function;
        void *library;
    } mtl_data;
    // set by glShaderBinary; a SPIR-V module skips the GLSL front end
    void *spirv_binary;
    GLsizei spirv_binary_length;
    GLboolean specialized;
    // what the subroutine rewrite found, empty when the source had none
    SubroutineInfo subroutines;
} Shader;

typedef struct Spirv_t {
    GLuint stage;
    size_t size;
    unsigned int *ir;
    char *msl_str;
    char *entry_point;
    void *mtl_function;
    void *mtl_library;
    // SPIRV-Cross made this a vertex function that returns nothing -- it writes
    // no outputs at all -- and Metal refuses to rasterise from one of those
    GLboolean raster_off;
    // the shader asks how long a buffer is, so the sizes go in a table
    GLboolean needs_sizes;
} Spirv;

#define MGL_NO_LOCATION ((GLuint)-1)

// Metal buffer slots the tessellation plumbing owns. They sit at the top of
// the 31 a stage gets, out of the way of the vertex and uniform buffers.
#define MGL_TESS_VERTEX_OUT_INDEX   30   // vertex-as-compute -> tess control
#define MGL_TESS_CONTROL_OUT_INDEX  29   // tess control -> tess eval, per vertex
#define MGL_TESS_PATCH_OUT_INDEX    28   // tess control -> tess eval, per patch
#define MGL_TESS_LEVEL_INDEX        27   // the tessellation factors
#define MGL_TESS_PARAMS_INDEX       26   // patch size and patch count
#define MGL_TESS_INDEX_INDEX        25   // the element buffer, for indexed patches

// What the tessellation stages said about the domain, read back out of the
// SPIR-V at link time so the render pipeline can be built to match.
typedef struct TessInfo_t {
    GLboolean active;
    GLboolean has_control;         // false when the control stage is fixed function
    GLuint    patch_kind;          // SpvExecutionModeTriangles / Quads / Isolines
    GLuint    partition;           // SpvExecutionModeSpacingEqual / Fractional*
    GLuint    winding;             // SpvExecutionModeVertexOrderCw / Ccw
    GLboolean point_mode;
    GLboolean quad_isolines;       // isolines run as quads to feed a geometry shader
    GLboolean isoline_segments;    // ... and handed over as line segments, not points
    // triangles or quads cut up on the CPU and fed to a geometry stage, for
    // transform feedback, point mode, or a geometry shader after them
    GLboolean general;
    GLint     gen_domain, gen_spacing;
    GLboolean gen_cw, gen_points;
    // what the evaluation shader itself declared, for the program queries;
    // the stage Metal runs may be a rewrite with a different layout
    GLint     gl_domain, gl_spacing;
    GLboolean gl_cw, gl_points;
    GLint     patches_loc;         // mglPatchesU, patches per instance, -1 when unused
    GLboolean lower_left;
    GLuint    out_control_points;  // vertices the control shader emits per patch
} TessInfo;

typedef struct SpirvResource_t {
    GLuint  _id;
    GLuint  base_type_id;
    GLuint  type_id;
    const char *name;
    GLuint  set;
    GLuint  binding;
    // a block declared as an instance array is one SPIR-V resource but several
    // GL blocks, each with its own binding; NULL unless array_size > 1
    GLuint  *element_binding;
    // MGL_NO_LOCATION until the linker numbers it across the whole program
    GLuint  location;
    GLboolean explicit_location;    // layout(location) gave it
    GLuint  msl_index;      // the [[buffer(n)]] / [[texture(n)]] slot SPIRV-Cross gave it
    GLuint  msl_sampler_index;  // the [[sampler(n)]] slot, for a combined sampler
    GLenum  gl_type;        // GL_FLOAT_VEC4 and friends, recorded at link time
    GLint   array_size;     // 1 unless the uniform is an array
    // GL picks the texture unit from the sampler uniform's value, not from the
    // binding baked into the SPIR-V. glUniform1i writes here.
    GLint   tex_unit;
    // for uniform and storage blocks: what glGetActiveUniformBlockiv reports
    GLint   block_size;
    GLint   member_count;
    // for a uniform that lives inside a block: where it sits in that block
    GLint   block_index;        // -1 for a plain uniform
    GLint   offset;
    GLint   array_stride;
    GLint   matrix_stride;
    GLboolean is_row_major;
    // a storage cube image the shader holds as a 2D array of its faces,
    // because Metal before MSL 4.0 has no atomics on cube textures
    GLboolean cube_as_array;
} SpirvResource;

typedef struct SpirvResourceList_t {
    GLuint  count;
    SpirvResource   *list;
} SpirvResourceList;

typedef struct BufferMap_t {
    GLuint      buffer_base_index;
    GLuint      attribute_mask;
    Buffer      *buf;
    GLintptr    offset;
    GLsizeiptr  size;       // the bound range, 0 for the rest of the buffer
    GLuint      stride;     // vertex stride of the attributes sharing this slot
    GLuint      divisor;    // how many instances share one element, 0 for none
    // what Metal is told the slot's stride is. GL's stride of zero means every
    // vertex reads the same element, which Metal spells as a constant step and
    // a stride that still has to cover the attribute
    GLuint      layout_stride;
    GLubyte     constant_step;
    // which GL buffer kind this slot came from. A shader can write a storage
    // buffer, so it must be a real MTLBuffer; a uniform can go through setBytes.
    GLubyte     gl_buffer_type;
} BufferMap;

typedef struct BufferMapList_t {
    GLuint      count;
    BufferMap   buffers[MAX_MAPPED_BUFFERS];
} BufferMapList;

typedef struct Program_t {
    GLuint dirty_bits;
    GLuint name;
    int refcount;
    GLboolean delete_status;
    Shader *shader_slots[_MAX_SHADER_TYPES];
    glslang_program_t *linked_glsl_program;
    Spirv spirv[_MAX_SHADER_TYPES];
    // indexed by the SPIRV-Cross resource enum, whose highest value has grown
    // over time; sized to cover it rather than to MGL's own shorter list
    SpirvResourceList spirv_resources_list[_MAX_SHADER_TYPES][MAX_SPVC_RESOURCE_TYPES];
    MglBindlessSets bindless[_MAX_SHADER_TYPES];
    // the uniforms declared inside uniform blocks, which GL lists as active
    // uniforms in their own right
    SpirvResourceList block_uniforms[_MAX_SHADER_TYPES];
    struct {
        unsigned x, y, z;
    } local_workgroup_size;
    void *mtl_data;
    GLboolean link_status;
    GLboolean validate_status;
    char *log;
    // uniform values belong to the program, not the context
    UniformConstants uniform_constants;
    GLboolean separable;
    GLboolean binary_retrievable_hint;
    // where the rewritten gl_NumSamples lives, or -1 when the shader never asked
    GLint num_samples_loc;
    GLint sample_mask_off_loc;
#ifdef MGL_COMPAT_PROFILE
    GLint alpha_func_loc;
    GLint alpha_ref_loc;
#endif
    // the subroutine index chosen per subroutine uniform location, per stage
    GLuint *subroutine_values[_MAX_SHADER_TYPES];
    // what the subroutine rewrite found, copied at link so the program keeps
    // it after its shaders are detached
    SubroutineInfo subroutines[_MAX_SHADER_TYPES];
    // GL keeps the recorded varyings on the program, not on the transform
    // feedback object, and they take effect at the next link
    char   **xfb_varyings;
    GLsizei  xfb_varying_count;
    GLenum   xfb_buffer_mode;
    // the feedback buffers the capturing stage's own xfb qualifiers write, a
    // bit each; zero when the shader leaves capture to the API
    GLuint   xfb_shader_buffers;
    GLint    xfb_shader_strides[MGL_XFB_MAX_BUFFERS];
    CaptureInfo xfb;
    TessInfo tess;
    // a geometry shader becomes a compute pass plus a generated vertex shader
    GeometryInfo geom;
    // the geometry shader that was linked: the attached one, or one MGL made
    // to carry point-mode isolines, which Metal cannot draw on its own
    struct Shader_t *geom_shader;
    struct Shader_t *synthetic_gs;
    Spirv gs_passthrough;
    // gl_CullDistance drops whole primitives, which Metal cannot do, so a
    // compute pass picks the survivors before the real draw
    CullInfo cull;
    Spirv cull_capture;
    Spirv cull_kernel;
    // GL's view of the program's resources, for the interface queries
    MglResourceTable resources;
    // glBindAttribLocation and glBindFragDataLocation(Indexed), applied at link
    struct { char *name; GLuint location; GLuint index; } attrib_binds[32], frag_binds[32];
    GLint attrib_bind_count, frag_bind_count;
    // what each stage was linked from, and which link this is, so a program
    // pipeline can link its stages together again (pipeline_draw.c)
    char   *stage_src[_MAX_SHADER_TYPES];
    GLuint  link_serial;
    GLboolean linked_separable;     // PROGRAM_SEPARABLE as of the last link
} Program;

// True once a program has linked a geometry stage. An application may detach
// its shaders after linking, so the attachment is not the thing to ask.
static inline bool mglProgramHasGeometry(const Program *p)
{
    return p && p->geom.compute_src != NULL;
}

static inline bool mglProgramCulls(const Program *p)
{
    return p && p->cull.count > 0 && p->cull.kernel_src != NULL;
}

typedef struct ProgramPipeline_t {
    GLuint name;
    GLboolean validated;
    Program *stage_programs[_MAX_SHADER_TYPES];  // Programs attached to each stage
    // the stages linked into one program for drawing, and the links it came from
    Program *merged;
    GLuint   merged_from[_MAX_SHADER_TYPES];
    // a generated name is only an object once it is bound or used
    GLboolean created;
    // where glUniform* goes when no program is current (glActiveShaderProgram)
    Program *active;
} ProgramPipeline;

// How many render encoders' worth of occlusion counting a frame can hold.
#define MGL_VISIBILITY_SLOTS 256

#define MAX_QUERY_STREAMS 4

typedef enum {
    _QUERY_SAMPLES_PASSED = 0,
    _QUERY_ANY_SAMPLES_PASSED,
    _QUERY_ANY_SAMPLES_PASSED_CONSERVATIVE,
    _QUERY_PRIMITIVES_GENERATED,
    _QUERY_TF_PRIMITIVES_WRITTEN,
    _QUERY_TF_OVERFLOW,
    _QUERY_TF_STREAM_OVERFLOW,
    _QUERY_TIME_ELAPSED,
    _QUERY_TIMESTAMP,
    _MAX_QUERY_TARGETS
} QueryTargetIndex;

typedef struct Query_t {
    GLuint name;
    GLenum target;
    GLuint index;
    GLboolean active;
    GLboolean have_result;
    GLuint64 result;
    // slots in the renderer's visibility buffer while occlusion counting
    GLint visibility_offset;
    GLint visibility_slots;
    GLuint64 start_time;
} Query;

#define MAX_DEBUG_MESSAGES  64
#define MAX_DEBUG_MSG_LEN   256
#define MAX_DEBUG_GROUPS    64
#define MAX_OBJECT_LABEL    256

typedef struct DebugMessage_t {
    GLenum source;
    GLenum type;
    GLuint id;
    GLenum severity;
    GLsizei length;
    char text[MAX_DEBUG_MSG_LEN];
} DebugMessage;

typedef struct DebugState_t {
    DebugMessage messages[MAX_DEBUG_MESSAGES];
    GLuint head;              // where the next message goes
    GLuint count;             // how many are queued
    GLboolean messages_enabled;
    DebugMessage groups[MAX_DEBUG_GROUPS];
    GLuint group_depth;
} DebugState;

#define MAX_TF_BUFFERS MAX_TRANSFORM_FEEDBACK_BUFFERS

typedef struct TransformFeedback_t {
    GLuint name;
    GLenum target;
    GLboolean active;
    GLboolean paused;
    GLenum primitive_mode;
    BufferBaseTarget buffers[MAX_TF_BUFFERS];
    char **varyings;
    GLsizei varying_count;
    GLenum buffer_mode;
    // where the next draw appends, in vertices
    GLuint vertices_recorded;
    // what the last finished capture came to, which is what a replay draws
    GLuint vertices_captured;
    GLboolean ever_ended;
    // glGen only reserves the name; the object itself starts existing when
    // something binds it, or when glCreate makes it outright
    GLboolean created;
} TransformFeedback;

typedef struct Renderbuffer_t {
    GLuint dirty_bits;
    GLuint  name;
    GLboolean is_draw_buffer;
    Texture *tex;
} Renderbuffer;

typedef struct FBOAttachment_t {
    GLuint dirty_bits;
    GLuint textarget;   // GL_RENDERBUFFER for renderbuffers
    GLuint texture;
    GLuint level;
    GLuint layer;
    // glFramebufferTexture on an array, cube or 3D texture attaches every
    // layer at once; the layer-at-a-time calls do not
    GLboolean layered;
    GLbitfield clear_bitmask;
    GLfloat clear_color[4];
    union {
        Texture *tex;
        Renderbuffer *rbo;
    } buf;
} FBOAttachment;

typedef struct Framebuffer_t {
    GLuint dirty_bits;
    GLuint  name;
    GLbitfield color_attachment_bitfield;
    // GL keeps the draw buffer per framebuffer, so each one remembers its own
    GLenum draw_buffer;
    // glDrawBuffers names one buffer per shader output, and an attachment named
    // here may not exist yet -- so the list lives on the framebuffer rather than
    // only as a flag on each attachment.
    GLenum draw_buffers[MAX_COLOR_ATTACHMENTS];
    GLsizei n_draw_buffers;
    FBOAttachment color_attachments[MAX_COLOR_ATTACHMENTS];
    FBOAttachment depth;
    FBOAttachment stencil;
    // Default framebuffer parameters (for FBOs with no attachments)
    GLint default_width;
    GLint default_height;
    GLint default_layers;
    GLint default_samples;
    GLboolean default_fixed_sample_locations;
} Framebuffer;

void mglApplyDrawBuffers(GLMContext ctx, struct Framebuffer_t *fbo);

typedef struct __GLsync {
    GLsizei name;
    void *mtl_event;
    struct __GLsync *next;      // live list, so a bad GLsync can be spotted
#ifdef __cplusplus
} Sync;
#else
} Sync, *__GLsync;
#endif

typedef struct PixelFormat_t {
    GLuint  format;
    GLuint  type;
    GLuint  mtl_pixel_format;
} PixelFormat;

typedef struct GLSLState_t {
    glslang_resource_t  resrc;
    glslang_limits_t    limits;
} GLSLState;

typedef struct PixelStore_t {
    GLboolean   swap_bytes;
    GLboolean   lsb_first;
    GLint row_length;
    GLint image_height;
    GLint skip_rows;
    GLint skip_pixels;
    GLint skip_images;
    GLint alignment;
    // GL 4.2's compressed pixel storage: how big a block is, so row length and
    // the skips can be counted in whole blocks. Zero means not in use.
    GLint compressed_block_width;
    GLint compressed_block_height;
    GLint compressed_block_depth;
    GLint compressed_block_size;
} PixelStore;

/* GL 4.6 8.4.4 / 18.2: rows are row_length (or width) pixels wide padded up to
   the alignment, and the skip_* modes move where the data starts. */
size_t mglPixelStoreRowPitch(const PixelStore *ps, GLsizei width, GLuint pixel_size);
bool   mglPixelStoreGet(GLMContext ctx, GLenum pname, GLint *out);
size_t mglPixelStoreSkipBytes(const PixelStore *ps, GLsizei height, GLuint pixel_size, size_t row_pitch);
size_t mglPixelStoreSkipBytes2D(const PixelStore *ps, GLuint pixel_size, size_t row_pitch);

// gl_NumSamples reaches the shader as an ordinary uniform of this name, which
// is deliberately the same length as gl_NumSamples so the rename is in place.
#define MGL_NUM_SAMPLES_NAME "mglNumSamples"
#define MGL_SAMPLE_MASK_FORCE "mglSampleMaskOff"
// same length as gl_SampleMask, so the rewrite swaps it in place
#define MGL_SAMPLE_MASK_TMP   "mglSMaskValue"
#define MGL_SAMPLE_MASK_BODY  "mglSampleMaskBody"

#ifdef MGL_COMPAT_PROFILE
// The alpha test is fixed-function state with nowhere to live in Metal, so a
// fragment shader carries it: these two uniforms hold the compare and the
// reference, and the driver writes them before every draw. A func of zero is
// the test switched off.
#define MGL_ALPHA_FUNC_NAME "mglAlphaTestFunc"
#define MGL_ALPHA_REF_NAME  "mglAlphaTestRef"
#define MGL_ALPHA_BODY      "mglAlphaTestBody"
#endif

// Uniforms MGL's own shader rewrites created. GL never declared them, so
// nothing the application can enumerate should report them.
static inline bool mglIsDriverUniform(const char *name)
{
    if (name == NULL)
        return false;

    if (!strcmp(name, MGL_NUM_SAMPLES_NAME) || !strcmp(name, MGL_SAMPLE_MASK_FORCE))
        return true;

#ifdef MGL_COMPAT_PROFILE
    if (!strcmp(name, MGL_ALPHA_FUNC_NAME) || !strcmp(name, MGL_ALPHA_REF_NAME))
        return true;
#endif

    return false;
}

// GL_CLAMP is the compatibility profile's old wrap mode, and it clamped to
// the border colour. Store it as the modern spelling so nothing downstream
// has to know it existed.
static inline GLenum mglNormalizeWrapMode(GLenum wrap)
{
#ifdef MGL_COMPAT_PROFILE
    if (wrap == GL_CLAMP)
        return GL_CLAMP_TO_BORDER;
#endif
    return wrap;
}

// The two targets whose storage holds more than one sample per pixel.
static inline bool mglTargetIsMultisample(GLenum target)
{
    return target == GL_TEXTURE_2D_MULTISAMPLE ||
           target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
}

// Samples in the framebuffer GL is drawing to; 0 when it is single-sampled.
GLsizei mglDrawFramebufferSamples(GLMContext ctx);

TransformFeedback *getTransformFeedback(GLMContext ctx, GLuint name);

GLint mglFindNumSamplesLocation(Program *pptr);
GLint mglFindUniformByName(Program *pptr, const char *name);
void mglWriteNumSamples(GLMContext ctx, Program *pptr, GLint samples);
GLint mglFindSampleMaskOffLocation(Program *pptr);
void mglWriteSampleMaskOff(GLMContext ctx, Program *pptr, GLint off);
#ifdef MGL_COMPAT_PROFILE
void mglFindAlphaTestLocations(Program *pptr);
void mglWriteAlphaTest(GLMContext ctx, Program *pptr, GLint func, GLfloat ref);
#endif
void mglWriteProgramUniform(GLMContext ctx, Program *pptr, GLint location, GLint value);


enum {
    dirtyVAO = 0,
    dirtyState,
    dirtyBuffer,
    dirtyTexture,
    dirtyTexParam,
    dirtyTexBinding,
    dirtySampler,
    dirtyShader,
    dirtyProgram,
    dirtyFBO,
    dirtyDrawable,
    dirtyRenderState,
    dirtyAlphaState,
    dirtyImageUnit,
    dirtyBufferBase,
    maxDirtyState,
    dirtyAllBit = 31
};

#define DIRTY_VAO       (0x1 << dirtyVAO)
#define DIRTY_STATE     (0x1 << dirtyState)
#define DIRTY_BUFFER    (0x1 << dirtyBuffer)
#define DIRTY_TEX       (0x1 << dirtyTexture)
#define DIRTY_TEX_PARAM   (0x1 << dirtyTexParam)
#define DIRTY_TEX_BINDING (0x1 << dirtyTexBinding)
#define DIRTY_SAMPLER (0x1 << dirtySampler)
#define DIRTY_SHADER    (0x1 << dirtyShader)
#define DIRTY_PROGRAM   (0x1 << dirtyProgram)
#define DIRTY_FBO       (0x1 << dirtyFBO)
#define DIRTY_DRAWABLE      (0x1 << dirtyDrawable)
#define DIRTY_RENDER_STATE  (0x1 << dirtyRenderState)
#define DIRTY_ALPHA_STATE   (0x1 << dirtyAlphaState)
#define DIRTY_IMAGE_UNIT_STATE   (0x1 << dirtyImageUnit)
#define DIRTY_BUFFER_BASE_STATE   (0x1 << dirtyBufferBase)
#define DIRTY_ALL_BIT   ((unsigned)0x1 << dirtyAllBit)    // so we know the dirty all was set.
#define DIRTY_ALL       (0xFFFFFFFF)

typedef struct {
    GLuint dirty_bits;

    // clear request clear_bitmask from glClear to Metal
    GLbitfield  clear_bitmask;

    // the framebuffer glClear was called against; a clear that could not be
    // encoded then must not land on whatever is bound later
    struct Framebuffer_t *clear_framebuffer;

    // opengl state

    // keep these out of the var struct for debugging and access

    GLenum error;

    // KHR_debug
    void *debug_callback;
    const void *debug_user_param;   // glGetError

    GLuint draw_buffer; // GL_DRAW_BUFFER, of whichever framebuffer is bound
    GLuint default_draw_buffer; // the default framebuffer's own draw buffer
    GLuint read_buffer; // GL_READ_BUFFER
    GLuint max_color_attachments; // GL_MAX_COLOR_ATTACHMENTS
    GLuint max_vertex_attribs; // GL_MAX_VERTEX_ATTRIBS
    ViewportRect  viewport[MAX_VIEWPORTS];      // GL_VIEWPORT
    ScissorRect   scissor[MAX_VIEWPORTS];       // GL_SCISSOR_BOX
    DepthRangeVal depth_range[MAX_VIEWPORTS];   // GL_DEPTH_RANGE
    AttribConstant attrib_constant[MAX_ATTRIBS];
    GLuint         attrib_constant_dirty;
    GLfloat color_clear_value[4]; // GL_COLOR_CLEAR_VALUE

    Buffer *buffers[MAX_BINDABLE_BUFFERS];

    // where indices handed to a draw as a plain pointer are staged, since
    // Metal only ever reads them out of a buffer
    Buffer *client_indices;

    VertexArray *vao;
    Texture     *tex;
    Renderbuffer *renderbuffer;
    Framebuffer *framebuffer;
    Framebuffer *readbuffer;

    GLuint      active_texture; // GL_ACTIVE_TEXTURE
    unsigned    active_texture_mask[4];
    Texture     *active_textures[TEXTURE_UNITS];
    TextureUnit texture_units[TEXTURE_UNITS];
    Sampler     *texture_samplers[TEXTURE_UNITS];
    ImageUnit   image_units[TEXTURE_UNITS];

    GLsizei sync_name;
    Sync   *sync_list;          // every sync object still alive

    HashTable vao_table;
    HashTable buffer_table;
    HashTable texture_table;
    HashTable shader_table;
    HashTable program_table;
    HashTable program_pipeline_table;
    HashTable transform_feedback_table;
    HashTable renderbuffer_table;
    HashTable framebuffer_table;
    HashTable sampler_table;
    HashTable query_table;

    Query *active_query[_MAX_QUERY_TARGETS][MAX_QUERY_STREAMS];
    DebugState debug;

    Shader      *shaders[_MAX_SHADER_TYPES];
    Program     *program;
    ProgramPipeline *program_pipeline;
    TransformFeedback *transform_feedback;

    BufferBase  buffer_base[_MAX_BUFFER_TYPES];

    // glsl info
    GLSLState   glsl;

    // pixel pack unpack
    PixelStore  pack;
    PixelStore  unpack;
    
    // metal buffer mappings
    BufferMapList vertex_buffer_map_list;
    BufferMapList fragment_buffer_map_list;
    BufferMapList compute_buffer_map_list;
    // the tessellation stages have their own uniforms, and the control stage
    // runs as compute while the evaluation stage is the draw's vertex function
    BufferMapList tess_control_buffer_map_list;
    BufferMapList tess_eval_buffer_map_list;
    BufferMapList geometry_buffer_map_list;

    // enable / disable caps
    GLMCaps     caps;

    // hints
    GLMHints    hints;
    
    // put at end, big chunk of yuck
    GLMParams   var;
} GLMState;

static_assert(TEXTURE_UNITS == 128, "active_texture_mask relies on this");

typedef struct GLMContextRec_t *GLMContext;

struct GLMMetalFuncs {
    void *mtlObj;
    void *mtlView;

    void (*mtlBindBuffer)(GLMContext glm_ctx, Buffer *ptr);
    void (*mtlBindTexture)(GLMContext glm_ctx, Texture *ptr);
    void (*mtlSetSwapInterval)(GLMContext glm_ctx, int interval);
    GLuint (*mtlBindlessSampler)(GLMContext glm_ctx, TextureParameter *params, GLenum target);
    void (*mtlBindlessRelease)(GLMContext glm_ctx, MglHandle *h);
    bool (*mtlBindProgram)(GLMContext glm_ctx, Program *ptr);

    void (*mtlDeleteMTLObj)(GLMContext glm_ctx, void *obj);

    void (*mtlGetSync)(GLMContext glm_ctx, Sync *sync);
    void (*mtlWaitForSync)(GLMContext glm_ctx, Sync *sync);
    void (*mtlForgetSync)(GLMContext glm_ctx, Sync *sync);

    // KHR_debug, so a GPU capture reads the way the GL code does
    void (*mtlPushDebugGroup)(GLMContext glm_ctx, const char *name);
    void (*mtlPopDebugGroup)(GLMContext glm_ctx);
    void (*mtlLabelObject)(GLMContext glm_ctx, GLenum identifier, GLuint name, const char *label);

    // occlusion counting: start on whatever encoder is live, stop it, and
    // add up what the GPU wrote once it has finished
    void (*mtlQueryBegin)(GLMContext glm_ctx, Query *q);
    void (*mtlQueryEnd)(GLMContext glm_ctx, Query *q);
    void (*mtlQueryResult)(GLMContext glm_ctx, Query *q);

    void (*mtlFlush)(GLMContext glm_ctx, bool finish);
    void (*mtlSwapBuffers)(GLMContext glm_ctx);
    
    void (*mtlClearBuffer)(GLMContext glm_ctx, GLuint type, GLbitfield mask);
    void (*mtlBlitFramebuffer)(GLMContext ctx, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);


    void (*mtlBufferSubData)(GLMContext glm_ctx, Buffer *buf, size_t offset, size_t size, const void *ptr);
    void *(*mtlMapUnmapBuffer)(GLMContext glm_ctx, Buffer *buf, size_t offset, size_t size, GLenum access, bool map);
    void (*mtlFlushBufferRange)(GLMContext glm_ctx, Buffer *buf, GLintptr offset, GLsizeiptr length);

    void (*mtlReadPixels)(GLMContext glm_ctx, void *pixelBytes, GLuint bytesPerRow, GLenum format, GLenum type, GLint x, GLint y, GLsizei width, GLsizei height);
    void (*mtlGetTexImage)(GLMContext glm_ctx, Texture *tex, void *pixelBytes, GLuint bytesPerRow, GLenum format, GLenum type, GLint x, GLint y, GLsizei width, GLsizei height, GLuint level, GLuint slice);

    void (*mtlGenerateMipmaps)(GLMContext glm_ctx, Texture *tex);
    void (*mtlTexSubImage)(GLMContext glm_ctx, Texture *tex, Buffer *buf, size_t src_offset, size_t src_pitch, size_t src_image_size, size_t src_size, GLuint slice, GLuint level, size_t width, size_t height, size_t depth, size_t xoffset, size_t yoffset, size_t zoffset);
    void (*mtlCopyTexSubImage)(GLMContext glm_ctx, Texture *tex, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
    void (*mtlCopyImageSubData)(GLMContext glm_ctx, Texture *srcTex, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, Texture *dstTex, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei width, GLsizei height, GLsizei depth);

    // draw arrays / elements
    void (*mtlDrawArrays)(GLMContext ctx, GLenum mode, GLint first, GLsizei count);
    void (*mtlDrawElements)(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices);
    void (*mtlDrawRangeElements)(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices);

    // draw arrays / elements instanced
    void (*mtlDrawArraysInstanced)(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
    void (*mtlDrawElementsInstanced)(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);

    // draw arrays / elements base vertex
    void (*mtlDrawElementsBaseVertex)(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex);
    void (*mtlDrawRangeElementsBaseVertex)(GLMContext ctx, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex);
    void (*mtlDrawElementsInstancedBaseVertex)(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex);

    // draw arrays / elements intanced base vertex
    void (*mtlDrawArraysIndirect)(GLMContext ctx, GLenum mode, const void *indirect);
    void (*mtlDrawElementsIndirect)(GLMContext ctx, GLenum mode, GLenum type, const void *indirect);

    void (*mtlDrawArraysInstancedBaseInstance)(GLMContext ctx, GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance);
    void (*mtlDrawElementsInstancedBaseInstance)(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance);
    // ?? running out of names here.
    void (*mtlDrawElementsInstancedBaseVertexBaseInstance)(GLMContext ctx, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance);

    // multi calls of many of the above
    void (*mtlMultiDrawArrays)(GLMContext ctx, GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount);
    void (*mtlMultiDrawElements)(GLMContext ctx, GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount);
    void (*mtlMultiDrawElementsBaseVertex)(GLMContext ctx, GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount, const GLint *basevertex);

    void (*mtlMultiDrawArraysIndirect)(GLMContext ctx, GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride);
    void (*mtlMultiDrawElementsIndirect)(GLMContext ctx, GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride);


    void (*mtlDispatchCompute)(GLMContext ctx, GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
    void (*mtlDispatchComputeIndirect)(GLMContext ctx, GLintptr indirect);
    void (*mtlMemoryBarrier)(GLMContext ctx, GLbitfield barriers);
} ;

typedef struct GLMContextRec_t {
    GLuint      context_flags;

#ifdef MGL_GL_CORE
    struct GLMDispatchTable dispatch;
#endif
    
#ifdef MGL_GL_ES
    struct GLM_ES_DispatchTable dispatch;
#endif

    struct GLMMetalFuncs mtl_funcs;

    MglBindless bindless;

    int swap_interval;

    GLMState    state;
    GLboolean   assert_on_error;
    // A shader that will not compile is reported through COMPILE_STATUS and
    // the info log, not as a GL error. Raised while a compile or link runs.
    GLuint      error_suppress;

    PixelFormat pixel_format;
    PixelFormat depth_format;
    PixelFormat stencil_format;

    BufferData  *temp_element_buffer;

    void (* error_func)(GLMContext ctx, const char *func, GLenum type);
} GLMContextRec;


GLMContext createGLMContext(GLenum format, GLenum type,
                            GLenum depth_format, GLenum depth_type,
                            GLenum stencil_format, GLenum stencil_type);

void mgl_lazy_init(void);

void MGLsetCurrentContext(GLMContext ctx);

enum {
    MGL_PIXEL_FORMAT,
    MGL_PIXEL_TYPE,
    MGL_DEPTH_FORMAT,
    MGL_DEPTH_TYPE,
    MGL_STENCIL_FORMAT,
    MGL_STENCIL_TYPE,
    MGL_CONTEXT_FLAGS,
    MGL_SWAP_INTERVAL
};

#ifdef __cplusplus
extern "C" {
#endif

GLuint sizeForFormatType(GLenum format, GLenum type);
GLuint bicountForFormatType(GLenum format, GLenum type, GLenum component);
GLMContext MGLgetCurrentContext(void);
void MGLget(GLMContext ctx, GLenum param, GLuint *data);
bool pixelConvertToInternalFormat(GLMContext ctx, GLenum internalformat, GLenum format, GLenum type, const void *src, void *dst, size_t len);

bool createTextureLevel(GLMContext ctx, Texture *tex, GLuint face, GLint level, GLboolean is_array, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void *pixels, GLboolean proxy);


#ifdef __cplusplus
};
#endif

#endif /* glm_context_h */
