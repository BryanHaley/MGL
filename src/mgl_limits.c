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
 * mgl_limits.c
 * MGL
 *
 * One place that says how big everything is, so glGetIntegerv and the GLSL
 * built-in constants cannot disagree.
 */

#include <string.h>

#include "glm_context.h"
#include "glslang_c_interface.h"

const glslang_resource_t* glslang_default_resource(void);

// raise a limit to at least what GL 4.6 requires, keeping anything larger
static void at_least(GLuint *v, GLuint floor)
{
    if (*v < floor)
        *v = floor;
}

// What the spec says a stage's combined uniform component count must reach:
// everything its uniform blocks can hold, plus its own default block.
static GLuint combined_components(GLuint block_size, GLuint blocks, GLuint components)
{
    return block_size / 4 * blocks + components;
}

// MGL used to fill these by asking Apple's OpenGL framework, which answers for
// GL 4.1 and leaves everything newer at zero.
void mglInitLimits(GLMContext ctx)
{
    GLMParams *v = &ctx->state.var;

    at_least(&v->max_texture_size, 16384);
    at_least(&v->max_3d_texture_size, 2048);
    at_least(&v->max_cube_map_texture_size, 16384);
    at_least(&v->max_rectangle_texture_size, 16384);
    at_least(&v->max_array_texture_layers, 2048);
    at_least(&v->max_renderbuffer_size, 16384);
    at_least(&v->max_texture_buffer_size, 65536);
    at_least(&v->max_viewports, 16);
    at_least(&v->max_elements_indices, 65536);
    at_least(&v->max_elements_vertices, 65536);
    at_least(&v->max_element_index, 0xFFFFFFFFu);
    at_least(&v->max_server_wait_timeout, 1000000);
    at_least(&v->max_sample_mask_words, 1);

    // the default block and the blocks around it
    at_least(&v->max_uniform_block_size, 16384);
    at_least(&v->max_uniform_buffer_bindings, 84);
    at_least(&v->max_uniform_locations, 1024);
    at_least(&v->max_shader_storage_block_size, 134217728);
    at_least(&v->max_shader_storage_buffer_bindings, MAX_SHADER_STORAGE_BUFFER_BINDINGS);
    at_least(&v->min_map_buffer_alignment, 64);

    at_least(&v->max_vertex_attribs, 16);
    at_least(&v->max_vertex_attrib_bindings, 16);
    at_least(&v->max_vertex_attrib_relative_offset, 2047);
    at_least(&v->max_vertex_attrib_stride, 2048);
    at_least(&v->max_vertex_uniform_components, 1024);
    at_least(&v->max_vertex_uniform_vectors, 256);
    at_least(&v->max_vertex_uniform_blocks, 14);
    at_least(&v->max_vertex_output_components, 128);
    at_least(&v->max_vertex_texture_image_units, 16);
    at_least(&v->max_vertex_image_uniforms, 8);
    at_least(&v->max_vertex_atomic_counters, 8);
    at_least(&v->max_vertex_atomic_counter_buffers, 1);
    at_least(&v->max_vertex_streams, 4);

    at_least(&v->max_fragment_uniform_components, 1024);
    at_least(&v->max_fragment_uniform_vectors, 256);
    at_least(&v->max_fragment_uniform_blocks, 14);
    at_least(&v->max_fragment_input_components, 128);
    at_least(&v->max_texture_image_units, 16);
    at_least(&v->max_fragment_image_uniforms, 8);
    at_least(&v->max_fragment_atomic_counters, 8);
    at_least(&v->max_fragment_atomic_counter_buffers, 1);
    at_least(&v->max_draw_buffers, 8);
    at_least(&v->max_dual_source_draw_buffers, 1);

    at_least(&v->max_geometry_uniform_components, 1024);
    at_least(&v->max_geometry_uniform_blocks, 14);
    at_least(&v->max_geometry_input_components, 128);
    at_least(&v->max_geometry_output_components, 128);
    at_least(&v->max_geometry_output_vertices, 256);
    at_least(&v->max_geometry_total_output_components, 1024);
    at_least(&v->max_geometry_texture_image_units, 16);
    at_least(&v->max_geometry_shader_invocations, 32);
    at_least(&v->max_geometry_varying_components, 64);
    at_least(&v->max_geometry_image_uniforms, 0);
    at_least(&v->max_geometry_atomic_counters, 8);
    at_least(&v->max_geometry_atomic_counter_buffers, 1);

    at_least(&v->max_patch_vertices, 32);
    at_least(&v->max_tess_gen_level, 64);
    at_least(&v->max_tess_patch_components, 120);
    at_least(&v->max_tess_control_input_components, 128);
    at_least(&v->max_tess_control_output_components, 128);
    at_least(&v->max_tess_control_total_output_components, 4096);
    at_least(&v->max_tess_control_texture_image_units, 16);
    at_least(&v->max_tess_control_uniform_components, 1024);
    at_least(&v->max_tess_control_uniform_blocks, 14);
    at_least(&v->max_tess_control_atomic_counters, 8);
    at_least(&v->max_tess_control_atomic_counter_buffers, 1);
    at_least(&v->max_tess_evaluation_input_components, 128);
    at_least(&v->max_tess_evaluation_output_components, 128);
    at_least(&v->max_tess_evaluation_texture_image_units, 16);
    at_least(&v->max_tess_evaluation_uniform_components, 1024);
    at_least(&v->max_tess_evaluation_uniform_blocks, 14);
    at_least(&v->max_tess_evaluation_atomic_counters, 8);
    at_least(&v->max_tess_evaluation_atomic_counter_buffers, 1);

    at_least(&v->max_compute_uniform_components, 1024);
    at_least(&v->max_compute_uniform_blocks, 14);
    at_least(&v->max_compute_texture_image_units, 16);
    at_least(&v->max_compute_image_uniforms, 8);
    at_least(&v->max_compute_atomic_counters, 8);
    at_least(&v->max_compute_atomic_counter_buffers, 8);
    at_least(&v->max_compute_shared_memory_size, 32768);
    at_least(&v->max_compute_work_group_invocations, 1024);

    at_least(&v->max_combined_texture_image_units, 80);
    at_least(&v->max_combined_uniform_blocks, 70);
    at_least(&v->max_combined_atomic_counters, 8);
    at_least(&v->max_combined_atomic_counter_buffers, 1);
    at_least(&v->max_combined_image_uniforms, 8);
    at_least(&v->max_combined_image_units_and_fragment_outputs, 8);
    at_least(&v->max_combined_shader_output_resources, 16);

    at_least(&v->max_clip_distances, 8);
    at_least(&v->max_clip_planes, 8);
    at_least(&v->max_cull_distances, 8);
    at_least(&v->max_combined_clip_and_cull_distances, 8);

    at_least(&v->max_image_units, 8);
    at_least(&v->max_image_samples, 0);
    at_least(&v->max_atomic_counter_buffer_bindings, 8);
    at_least(&v->max_atomic_counter_buffer_size, 32768);

    // these are binding-point counts MGL really has, not just a floor
    v->max_transform_feedback_buffers = MAX_TRANSFORM_FEEDBACK_BUFFERS;
    v->max_transform_feedback_separate_attribs = MAX_TRANSFORM_FEEDBACK_BUFFERS;
    at_least(&v->max_transform_feedback_interleaved_components, 128);
    at_least(&v->max_transform_feedback_separate_components, 4);

    at_least(&v->max_framebuffer_width, 16384);
    at_least(&v->max_framebuffer_height, 16384);
    at_least(&v->max_framebuffer_layers, 2048);
    at_least(&v->max_framebuffer_samples, 4);

    at_least(&v->max_debug_group_stack_depth, 64);
    at_least(&v->max_label_length, 256);

    at_least(&v->max_varying_vectors, 15);

    // one varying vector is four components, and the two have to agree
    at_least(&v->max_varying_components, v->max_varying_vectors * 4);
    at_least(&v->max_varying_components, 60);
    v->max_varying_floats = v->max_varying_components;

    at_least(&v->max_texture_lod_bias, 2);
    at_least(&v->max_color_attachments, 8);

    // GL's own defaults, which nothing else was setting
    if (v->clip_origin == 0)
        v->clip_origin = GL_LOWER_LEFT;

    if (v->clip_depth == 0)
        v->clip_depth = GL_NEGATIVE_ONE_TO_ONE;

    if (v->patch_vertices == 0)
    {
        v->patch_vertices = 3;

        for (int i = 0; i < 4; i++)
            v->patch_default_outer[i] = 1.0f;

        v->patch_default_inner[0] = v->patch_default_inner[1] = 1.0f;
    }

    v->min_program_texel_offset = (GLuint)-8;
    v->max_program_texel_offset = 7;
    v->min_program_texture_gather_offset = -8;
    v->max_program_texture_gather_offset = 7;

    // these follow from the numbers above rather than standing on their own
    v->max_combined_vertex_uniform_components =
        combined_components(v->max_uniform_block_size, v->max_vertex_uniform_blocks,
                            v->max_vertex_uniform_components);
    v->max_combined_fragment_uniform_components =
        combined_components(v->max_uniform_block_size, v->max_fragment_uniform_blocks,
                            v->max_fragment_uniform_components);
    v->max_combined_geometry_uniform_components =
        combined_components(v->max_uniform_block_size, v->max_geometry_uniform_blocks,
                            v->max_geometry_uniform_components);
    v->max_combined_tess_control_uniform_components =
        combined_components(v->max_uniform_block_size, v->max_tess_control_uniform_blocks,
                            v->max_tess_control_uniform_components);
    v->max_combined_tess_evaluation_uniform_components =
        combined_components(v->max_uniform_block_size, v->max_tess_evaluation_uniform_blocks,
                            v->max_tess_evaluation_uniform_components);
    v->max_combined_compute_uniform_components =
        combined_components(v->max_uniform_block_size, v->max_compute_uniform_blocks,
                            v->max_compute_uniform_components);
}

// The GLSL compiler has its own copy of every limit, and the CTS checks the two
// against each other, so hand glslang the numbers GL is answering with.
const void *mglGlslangResource(GLMContext ctx)
{
    static glslang_resource_t r;
    static bool built = false;
    GLMParams *v;

    if (built)
        return &r;

    r = *glslang_default_resource();

    if (ctx == NULL)
        return &r;

    v = &ctx->state.var;

    r.max_clip_planes = v->max_clip_planes;
    r.max_vertex_attribs = v->max_vertex_attribs;
    r.max_vertex_uniform_components = v->max_vertex_uniform_components;
    r.max_varying_floats = v->max_varying_floats;
    r.max_vertex_texture_image_units = v->max_vertex_texture_image_units;
    r.max_combined_texture_image_units = v->max_combined_texture_image_units;
    r.max_texture_image_units = v->max_texture_image_units;
    r.max_fragment_uniform_components = v->max_fragment_uniform_components;
    r.max_draw_buffers = v->max_draw_buffers;
    r.max_vertex_uniform_vectors = v->max_vertex_uniform_vectors;
    r.max_varying_vectors = v->max_varying_vectors;
    r.max_fragment_uniform_vectors = v->max_fragment_uniform_vectors;
    r.max_vertex_output_vectors = v->max_vertex_output_components / 4;
    r.max_fragment_input_vectors = v->max_fragment_input_components / 4;
    r.min_program_texel_offset = (int)v->min_program_texel_offset;
    r.max_program_texel_offset = (int)v->max_program_texel_offset;
    r.max_clip_distances = v->max_clip_distances;
    r.max_cull_distances = v->max_cull_distances;
    r.max_combined_clip_and_cull_distances = v->max_combined_clip_and_cull_distances;

    r.max_compute_work_group_count_x = v->max_compute_work_group_count[0];
    r.max_compute_work_group_count_y = v->max_compute_work_group_count[1];
    r.max_compute_work_group_count_z = v->max_compute_work_group_count[2];
    r.max_compute_work_group_size_x = v->max_compute_work_group_size[0];
    r.max_compute_work_group_size_y = v->max_compute_work_group_size[1];
    r.max_compute_work_group_size_z = v->max_compute_work_group_size[2];
    r.max_compute_uniform_components = v->max_compute_uniform_components;
    r.max_compute_texture_image_units = v->max_compute_texture_image_units;
    r.max_compute_image_uniforms = v->max_compute_image_uniforms;
    r.max_compute_atomic_counters = v->max_compute_atomic_counters;
    r.max_compute_atomic_counter_buffers = v->max_compute_atomic_counter_buffers;

    r.max_varying_components = v->max_varying_components;
    r.max_vertex_output_components = v->max_vertex_output_components;
    r.max_geometry_input_components = v->max_geometry_input_components;
    r.max_geometry_output_components = v->max_geometry_output_components;
    r.max_fragment_input_components = v->max_fragment_input_components;
    r.max_image_units = v->max_image_units;
    r.max_combined_image_units_and_fragment_outputs = v->max_combined_image_units_and_fragment_outputs;
    r.max_combined_shader_output_resources = v->max_combined_shader_output_resources;
    r.max_image_samples = v->max_image_samples;
    r.max_vertex_image_uniforms = v->max_vertex_image_uniforms;
    r.max_tess_control_image_uniforms = v->max_tess_control_image_uniforms;
    r.max_tess_evaluation_image_uniforms = v->max_tess_evaluation_image_uniforms;
    r.max_geometry_image_uniforms = v->max_geometry_image_uniforms;
    r.max_fragment_image_uniforms = v->max_fragment_image_uniforms;
    r.max_combined_image_uniforms = v->max_combined_image_uniforms;

    r.max_geometry_texture_image_units = v->max_geometry_texture_image_units;
    r.max_geometry_output_vertices = v->max_geometry_output_vertices;
    r.max_geometry_total_output_components = v->max_geometry_total_output_components;
    r.max_geometry_uniform_components = v->max_geometry_uniform_components;
    r.max_geometry_varying_components = v->max_geometry_varying_components;

    r.max_tess_control_input_components = v->max_tess_control_input_components;
    r.max_tess_control_output_components = v->max_tess_control_output_components;
    r.max_tess_control_texture_image_units = v->max_tess_control_texture_image_units;
    r.max_tess_control_uniform_components = v->max_tess_control_uniform_components;
    r.max_tess_control_total_output_components = v->max_tess_control_total_output_components;
    r.max_tess_evaluation_input_components = v->max_tess_evaluation_input_components;
    r.max_tess_evaluation_output_components = v->max_tess_evaluation_output_components;
    r.max_tess_evaluation_texture_image_units = v->max_tess_evaluation_texture_image_units;
    r.max_tess_evaluation_uniform_components = v->max_tess_evaluation_uniform_components;
    r.max_tess_patch_components = v->max_tess_patch_components;
    r.max_patch_vertices = v->max_patch_vertices;
    r.max_tess_gen_level = v->max_tess_gen_level;
    r.max_viewports = v->max_viewports;

    r.max_vertex_atomic_counters = v->max_vertex_atomic_counters;
    r.max_tess_control_atomic_counters = v->max_tess_control_atomic_counters;
    r.max_tess_evaluation_atomic_counters = v->max_tess_evaluation_atomic_counters;
    r.max_geometry_atomic_counters = v->max_geometry_atomic_counters;
    r.max_fragment_atomic_counters = v->max_fragment_atomic_counters;
    r.max_combined_atomic_counters = v->max_combined_atomic_counters;
    r.max_atomic_counter_bindings = v->max_atomic_counter_buffer_bindings;
    r.max_vertex_atomic_counter_buffers = v->max_vertex_atomic_counter_buffers;
    r.max_tess_control_atomic_counter_buffers = v->max_tess_control_atomic_counter_buffers;
    r.max_tess_evaluation_atomic_counter_buffers = v->max_tess_evaluation_atomic_counter_buffers;
    r.max_geometry_atomic_counter_buffers = v->max_geometry_atomic_counter_buffers;
    r.max_fragment_atomic_counter_buffers = v->max_fragment_atomic_counter_buffers;
    r.max_combined_atomic_counter_buffers = v->max_combined_atomic_counter_buffers;
    r.max_atomic_counter_buffer_size = v->max_atomic_counter_buffer_size;

    r.max_transform_feedback_buffers = v->max_transform_feedback_buffers;
    r.max_transform_feedback_interleaved_components = v->max_transform_feedback_interleaved_components;
    r.max_samples = v->max_samples;

    built = true;

    return &r;
}
