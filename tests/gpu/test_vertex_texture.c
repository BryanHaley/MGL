/*
 * test_vertex_texture.c
 * Copyright (C) The MooGL Project
 *
 * Sampling from a vertex shader. Textures were only ever handed to the
 * fragment stage, so every vertex fetch read black -- which is most of what
 * the texture_swizzle suite does.
 */

#include <string.h>

#include "mgl_test.h"
#include "harness.h"

static const char *VS_FETCH =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(binding = 0) uniform sampler2D src;\n"
    "flat out vec4 value;\n"
    "void main() {\n"
    "    value = texelFetch(src, ivec2(0, 0), 0);\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char *FS_PASS =
    "#version 460 core\n"
    "flat in vec4 value;\n"
    "out vec4 frag;\n"
    "void main() { frag = value; }\n";

/* Both stages read the same sampler, so a stage that gets no texture shows up
   as a channel that stays black. */
static const char *VS_BOTH =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(binding = 0) uniform sampler2D src;\n"
    "flat out vec4 value;\n"
    "void main() {\n"
    "    value = texelFetch(src, ivec2(0, 0), 0);\n"
    "    gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char *FS_BOTH =
    "#version 460 core\n"
    "flat in vec4 value;\n"
    "layout(binding = 0) uniform sampler2D src;\n"
    "out vec4 frag;\n"
    "void main() {\n"
    "    vec4 f = texelFetch(src, ivec2(0, 0), 0);\n"
    "    frag = vec4(value.r, f.g, value.b, 1.0);\n"
    "}\n";

static GLuint make_source(GLenum internalformat, GLenum format, const void *data)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalformat, 1, 1, 0,
                 format, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return tex;
}

GPU_TEST(vertex_texture, vertex_stage_sees_the_bound_texture)
{
    MGLTestTarget t;
    GLuint prog, vao, vbo, tex;
    const GLubyte texel[4] = { 10, 60, 120, 255 };
    unsigned char *px, rgba[4];
    char log[512] = { 0 };

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0))
        return;

    prog = mgl_build_program(VS_FETCH, FS_PASS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    tex = make_source(GL_RGBA8, GL_RGBA, texel);
    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "src"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    px = mgl_read_rgba8(&t);
    CHECK(px != NULL);
    if (px)
    {
        mgl_pixel_at(px, &t, 4, 4, rgba);
        CHECK_EQ_UINT(10u,  rgba[0]);
        CHECK_EQ_UINT(60u,  rgba[1]);
        CHECK_EQ_UINT(120u, rgba[2]);
        free(px);
    }

    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(vertex_texture, both_stages_read_the_same_sampler)
{
    MGLTestTarget t;
    GLuint prog, vao, vbo, tex;
    const GLubyte texel[4] = { 10, 60, 120, 255 };
    unsigned char *px, rgba[4];
    char log[512] = { 0 };

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0))
        return;

    prog = mgl_build_program(VS_BOTH, FS_BOTH, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    tex = make_source(GL_RGBA8, GL_RGBA, texel);
    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "src"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    px = mgl_read_rgba8(&t);
    CHECK(px != NULL);
    if (px)
    {
        mgl_pixel_at(px, &t, 4, 4, rgba);
        CHECK_EQ_UINT(10u,  rgba[0]);   /* from the vertex stage */
        CHECK_EQ_UINT(60u,  rgba[1]);   /* from the fragment stage */
        CHECK_EQ_UINT(120u, rgba[2]);   /* from the vertex stage */
        free(px);
    }

    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* A swizzle set after the texture exists has to reach the vertex stage too. */
GPU_TEST(vertex_texture, swizzle_applies_to_a_vertex_fetch)
{
    MGLTestTarget t;
    GLuint prog, vao, vbo, tex;
    const GLubyte texel[4] = { 10, 60, 120, 200 };
    unsigned char *px, rgba[4];
    char log[512] = { 0 };

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0))
        return;

    prog = mgl_build_program(VS_FETCH, FS_PASS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    tex = make_source(GL_RGBA8, GL_RGBA, texel);
    vao = mgl_fullscreen_quad(&vbo);

    mgl_target_bind(&t);
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "src"), 0);
    glBindVertexArray(vao);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ALPHA);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_BLUE);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    CHECK_EQ_UINT(GL_NO_ERROR, mgl_drain_errors());

    px = mgl_read_rgba8(&t);
    CHECK(px != NULL);
    if (px)
    {
        mgl_pixel_at(px, &t, 4, 4, rgba);
        CHECK_EQ_UINT(200u, rgba[0]);
        CHECK_EQ_UINT(120u, rgba[1]);
        free(px);
    }

    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
