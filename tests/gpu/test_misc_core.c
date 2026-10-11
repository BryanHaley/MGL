/*
 * test_misc_core.c
 * MGL
 *
 * Remaining uncovered core entry points: glCopyImageSubData, glDispatchCompute,
 * glGetInternalformati64v, glGetObjectLabel, glGetObjectPtrLabel,
 * glGetShaderPrecisionFormat, glObjectPtrLabel, glReleaseShaderCompiler,
 * glTexBuffer, glTexBufferRange.
 */

#include "mgl_test.h"
#include "harness.h"
#include "MGLContext.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ---------- dispatch compute ---------- */

GPU_TEST(misc_core, dispatch_compute_rejects_no_program)
{
    // spec: GL_INVALID_OPERATION if no active program for compute shader stage
    glDispatchCompute(1, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(misc_core, dispatch_compute_rejects_zero_groups)
{
    // zero is not greater than or equal to the max, so it should not be rejected
    // by the value check; the real error is no program.  The spec says
    // GL_INVALID_VALUE only when a count >= MAX_COMPUTE_WORK_GROUP_COUNT.
    glDispatchCompute(0, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(misc_core, dispatch_compute_rejects_huge_groups)
{
    // spec: GL_INVALID_VALUE if any count >= MAX_COMPUTE_WORK_GROUP_COUNT
    glDispatchCompute(0xFFFFFFFFu, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- copy image sub data ---------- */

GPU_TEST(misc_core, copy_image_rejects_invalid_target)
{
    GLuint tex[2] = { 0 };

    glGenTextures(2, tex);
    glBindTexture(GL_TEXTURE_2D, tex[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, tex[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, 0);

    // GL_TEXTURE_BUFFER is explicitly excluded
    glCopyImageSubData(tex[0], GL_TEXTURE_BUFFER, 0, 0, 0, 0,
                       tex[1], GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glCopyImageSubData(tex[0], GL_TEXTURE_2D, 0, 0, 0, 0,
                       tex[1], GL_TEXTURE_BUFFER, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteTextures(2, tex);
}

GPU_TEST(misc_core, copy_image_rejects_invalid_name)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, 0);

    // non-existent source
    glCopyImageSubData(999123, GL_TEXTURE_2D, 0, 0, 0, 0,
                       tex, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // non-existent destination
    glCopyImageSubData(tex, GL_TEXTURE_2D, 0, 0, 0, 0,
                       999123, GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, copy_image_round_trips_pixels)
{
    // create two identical textures, write a distinctive pixel into the source,
    // copy a 1x1 region, and verify it arrives in the destination
    MGLTestTarget t;
    unsigned char *px;
    GLuint src = 0, dst = 0;

    if (!mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&t);

    glGenTextures(1, &src);
    glBindTexture(GL_TEXTURE_2D, src);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                    (const unsigned char[]){ 0x80, 0x40, 0x20, 0xFF });

    glGenTextures(1, &dst);
    glBindTexture(GL_TEXTURE_2D, dst);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);

    glBindTexture(GL_TEXTURE_2D, 0);

    glCopyImageSubData(src, GL_TEXTURE_2D, 0, 1, 1, 0,
                       dst, GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // draw the destination texture and read back the pixel at (0,0)
    // which should now contain the copied colour
    {
        char err[1024] = { 0 };
        GLuint p;
        const char *vs =
            "#version 460 core\n"
            "layout(location = 0) in vec2 pos;\n"
            "out vec2 uv;\n"
            "void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0, 1); }\n";
        const char *fs =
            "#version 460 core\n"
            "uniform sampler2D u_tex;\n"
            "in vec2 uv;\n"
            "layout(location = 0) out vec4 frag;\n"
            "void main() { frag = texture(u_tex, uv); }\n";

        p = mgl_build_program(vs, fs, err, sizeof err);
        CHECK(p != 0);

        glUseProgram(p);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, dst);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(glGetUniformLocation(p, "u_tex"), 0);

        mgl_fullscreen_quad(NULL);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        px = mgl_read_rgba8(&t);
        CHECK(px != NULL);
        if (px)
        {
            unsigned char rgba[4];
            mgl_pixel_at(px, &t, 0, 0, rgba);
            CHECK_EQ_UINT(rgba[0], 0x80u);
            CHECK_EQ_UINT(rgba[1], 0x40u);
            CHECK_EQ_UINT(rgba[2], 0x20u);
            CHECK_EQ_UINT(rgba[3], 0xFFu);
            free(px);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glDeleteProgram(p);
    }

    glDeleteTextures(1, &src);
    glDeleteTextures(1, &dst);
    mgl_target_destroy(&t);
}

/* ---------- get internal format i64v ---------- */

GPU_TEST(misc_core, get_internalformati64v_matches_32_bit)
{
    GLint  v32[4] = { -1, -1, -1, -1 };
    GLint64 v64[4] = { -1, -1, -1, -1 };

    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 4, v32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetInternalformati64v(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 4, v64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_EQ_INT((GLint)v64[0], v32[0]);
}

GPU_TEST(misc_core, get_internalformati64v_rejects_bad_args)
{
    GLint64 v = -1;

    // invalid target
    glGetInternalformati64v(0x9999, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 1, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // negative count
    glGetInternalformati64v(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, -1, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // NULL params with non-zero count
    glGetInternalformati64v(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- object labels (KHR_debug) ---------- */

GPU_TEST(misc_core, object_label_round_trip)
{
    GLuint b = 0;
    GLsizei len = 0;
    char got[64] = { 0 };

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 16, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glObjectLabel(GL_BUFFER, b, -1, "my_buffer");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetObjectLabel(GL_BUFFER, b, sizeof got, &len, got);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_STR_EQ(got, "my_buffer");
    CHECK_EQ_INT(len, 9);

    glDeleteBuffers(1, &b);
}

GPU_TEST(misc_core, object_label_rejects_bad_identifier)
{
    glObjectLabel(0x9999, 1, -1, "x");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetObjectLabel(0x9999, 1, 64, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(misc_core, object_label_rejects_nonexistent_name)
{
    // spec: GL_INVALID_OPERATION if name is not an existing object
    glObjectLabel(GL_BUFFER, 999123, -1, "nope");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetObjectLabel(GL_BUFFER, 999123, 64, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(misc_core, object_label_rejects_zero_bufsize)
{
    GLuint b = 0;
    char tmp[8] = { 0 };

    glGenBuffers(1, &b);
    glObjectLabel(GL_BUFFER, b, -1, "tag");

    // bufSize == 0 should give GL_INVALID_VALUE
    glGetObjectLabel(GL_BUFFER, b, 0, NULL, tmp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
}

GPU_TEST(misc_core, object_ptr_label_round_trip)
{
    GLsync sync;

    sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(sync != NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glObjectPtrLabel(sync, -1, "my_sync");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLsizei len = 0;
        char got[64] = { 0 };
        glGetObjectPtrLabel(sync, sizeof got, &len, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_STR_EQ(got, "my_sync");
        CHECK_EQ_INT(len, 7);
    }

    glDeleteSync(sync);
}

GPU_TEST(misc_core, object_ptr_label_rejects_null)
{
    // spec: GL_INVALID_VALUE if ptr is NULL
    glObjectPtrLabel(NULL, -1, "x");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetObjectPtrLabel(NULL, 64, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(misc_core, object_ptr_label_rejects_invalid_ptr)
{
    // spec: GL_INVALID_VALUE if ptr is not a valid sync object
    glObjectPtrLabel((const void *)0x1234, -1, "x");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetObjectPtrLabel((const void *)0x1234, 64, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(misc_core, object_ptr_label_rejects_zero_bufsize)
{
    GLsync sync;
    char tmp[8] = { 0 };

    sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(sync != NULL);
    glObjectPtrLabel(sync, -1, "tag");

    // bufSize == 0 should give GL_INVALID_VALUE
    glGetObjectPtrLabel(sync, 0, NULL, tmp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteSync(sync);
}

/* ---------- shader precision format ---------- */

GPU_TEST(misc_core, shader_precision_returns_reasonable_values)
{
    GLint range[2] = { -1, -1 };
    GLint prec = -1;

    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_FLOAT, range, &prec);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(range[0] > 0);
    CHECK(range[1] > 0);
    CHECK(prec > 0);

    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_INT, range, &prec);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(range[0] > 0);
}

GPU_TEST(misc_core, shader_precision_rejects_bad_enums)
{
    GLint range[2], prec;

    // invalid shader type
    glGetShaderPrecisionFormat(0x9999, GL_HIGH_FLOAT, range, &prec);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // invalid precision type
    glGetShaderPrecisionFormat(GL_VERTEX_SHADER, 0x9999, range, &prec);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- release shader compiler ---------- */

GPU_TEST(misc_core, release_shader_compiler_is_harmless)
{
    // spec: hint, no error conditions defined
    glReleaseShaderCompiler();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- tex buffer ---------- */

GPU_TEST(misc_core, tex_buffer_rejects_bad_target)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // target must be GL_TEXTURE_BUFFER
    glTexBuffer(GL_TEXTURE_2D, GL_RGBA8, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glTexBufferRange(GL_TEXTURE_2D, GL_RGBA8, b, 0, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteBuffers(1, &b);
}

GPU_TEST(misc_core, tex_buffer_works_on_the_default_texture)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Texture 0 is a real texture object here, so this is legal: the spec's
    // error list for TexBuffer has no "nothing bound" case.
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteBuffers(1, &b);
}

GPU_TEST(misc_core, tex_buffer_requires_valid_buffer_name)
{
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);

    // non-existent buffer
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, 999123);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, tex_buffer_detach_with_zero)
{
    // buffer = 0 is valid and detaches any existing buffer
    GLuint tex = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);

    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, tex_buffer_range_validates_offset)
{
    GLuint tex = 0, b = 0;
    GLint alignment = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // offset must be a multiple of GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT
    glGetIntegerv(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, &alignment);
    if (alignment > 1)
    {
        glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, b, alignment + 1, 64);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    }

    // offset + size must not exceed BUFFER_SIZE
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, b, 128, 256);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // size must be > 0
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, b, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // negative offset
    glTexBufferRange(GL_TEXTURE_BUFFER, GL_RGBA8, b, -8, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glDeleteTextures(1, &tex);
}

GPU_TEST(misc_core, tex_buffer_attach_and_query)
{
    // attach a buffer via glTexBuffer, then read the binding back
    GLuint tex = 0, b = 0;
    GLint binding = -1;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_BUFFER, tex);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &binding);
    CHECK_EQ_UINT((GLuint)binding, tex);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &b);
}

// glfwSwapInterval reached MGL and stopped there, so an application that asked
// for no vsync still waited for the display on every frame
GPU_TEST(misc_core, swap_interval_is_remembered)
{
    unsigned int before = 1, off = 1, on = 0;

    MGLget(NULL, MGL_SWAP_INTERVAL, &before);
    CHECK_EQ_UINT(before, 1u);      // vsync unless the application says otherwise

    MGLsetSwapInterval(NULL, 0);
    MGLget(NULL, MGL_SWAP_INTERVAL, &off);
    CHECK_EQ_UINT(off, 0u);

    MGLsetSwapInterval(NULL, 1);
    MGLget(NULL, MGL_SWAP_INTERVAL, &on);
    CHECK_EQ_UINT(on, 1u);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// Three alignments were never filled in and answered 0. GL wants a power of
// two no larger than 256, and an application rounding up to 0 divides by it.
GPU_TEST(misc_core, buffer_offset_alignments_are_real)
{
    static const GLenum pnames[] = {
        GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
        GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,
        GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT,
    };

    for (int i = 0; i < 3; i++)
    {
        GLint a = 0;

        glGetIntegerv(pnames[i], &a);
        CHECK_MSG(a > 0 && a <= 256 && (a & (a - 1)) == 0, "pname 0x%x answers %d", pnames[i], a);
    }
}

// GL_EXT_shader_integer_mix is advertised: a shader that requires it and
// picks between integer vectors with a bool vector has to build and draw.
GPU_TEST(misc_core, integer_mix_extension_builds_and_selects)
{
    static const char *VS =
        "#version 330 core\n"
        "#extension GL_EXT_shader_integer_mix : require\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); gl_PointSize = 1.0; }\n";
    static const char *FS =
        "#version 330 core\n"
        "#extension GL_EXT_shader_integer_mix : require\n"
        "out vec4 o;\n"
        "void main() {\n"
        "    ivec4 v = mix(ivec4(0), ivec4(255), bvec4(true, false, true, false));\n"
        "    o = vec4(v) / 255.0;\n"
        "}\n";
    MGLTestTarget t;
    GLuint prog, vao;
    char log[2048] = { 0 };
    unsigned char *px, c[4] = { 0 };

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    mgl_target_create(&t, 1, 1, GL_RGBA8, 0);
    mgl_target_bind(&t);
    glViewport(0, 0, 1, 1);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glDrawArrays(GL_POINTS, 0, 1);
    glDisable(GL_PROGRAM_POINT_SIZE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (px)
    {
        mgl_pixel_at(px, &t, 0, 0, c);
        free(px);
    }
    CHECK_MSG(c[0] == 255 && c[1] == 0 && c[2] == 255 && c[3] == 0, "got %d,%d,%d,%d", c[0], c[1], c[2], c[3]);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// glTextureBarrier makes what was drawn visible to a later draw that samples
// the same texture. It did nothing, so the second draw read the old texels.
GPU_TEST(misc_core, texture_barrier_shows_earlier_rendering)
{
    static const char *VS =
        "#version 450 core\n"
        "void main() {\n"
        "    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2)) * 2.0 - 1.0;\n"
        "    gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    static const char *FS =
        "#version 450 core\n"
        "layout(binding = 0) uniform sampler2D self;\n"
        "uniform int pass_no;\n"
        "out vec4 o;\n"
        "void main() {\n"
        "    vec4 seen = texelFetch(self, ivec2(gl_FragCoord.xy), 0);\n"
        "    o = pass_no == 0 ? vec4(0.25, 0.0, 0.0, 1.0) : vec4(seen.r * 2.0, 0.0, 0.0, 1.0);\n"
        "}\n";
    GLuint prog, vao, tex, fb;
    char log[2048] = { 0 };
    GLubyte px[4 * 4 * 4] = { 0 };

    prog = mgl_build_program(VS, FS, log, sizeof log);
    CHECK_MSG(prog != 0, "link: %s", log);
    if (!prog) return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, 4, 4);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glUniform1i(glGetUniformLocation(prog, "pass_no"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glTextureBarrier();
    glUniform1i(glGetUniformLocation(prog, "pass_no"), 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(px[0] >= 126 && px[0] <= 130, "second draw wrote %d, want about 128", px[0]);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glDeleteFramebuffers(1, &fb);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
}

// The multi-bind calls: a bad entry fails alone, the span is checked against
// the limits GL reports, a missing array empties the span, and each unit's
// texture binding can be read by index.
GPU_TEST(misc_core, multi_bind_entries_stand_alone)
{
    GLuint bufs[3], tex, cube;
    GLint max_atomic = 0, v = -1, units = 0;
    GLuint names[3];
    static const GLubyte texel[4 * 6] = { 0 };

    glGenBuffers(3, bufs);
    for (int i = 0; i < 3; i++)
    {
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, bufs[i]);
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, 16, NULL, GL_DYNAMIC_COPY);
    }

    // the middle name was never made: it alone fails
    names[0] = bufs[0];
    names[1] = 0x7FFFFFF0u;
    names[2] = bufs[2];
    glBindBuffersBase(GL_ATOMIC_COUNTER_BUFFER, 0, 3, names);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glGetIntegeri_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, 2, &v);
    CHECK_EQ_INT(v, (GLint)bufs[2]);

    glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &max_atomic);
    glBindBuffersBase(GL_ATOMIC_COUNTER_BUFFER, (GLuint)max_atomic - 1, 2, bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffersBase(GL_ATOMIC_COUNTER_BUFFER, 0, 3, NULL);
    glGetIntegeri_v(GL_ATOMIC_COUNTER_BUFFER_BINDING, 2, &v);
    CHECK_EQ_INT(v, 0);

    // a texture bound to unit 3 reads back through that index
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTextures(3, 1, &tex);
    glGetIntegeri_v(GL_TEXTURE_BINDING_2D, 3, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, (GLint)tex);

    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &units);
    glBindTextures((GLuint)units, 1, &tex);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glBindTextures(3, 1, NULL);

    // an image bound by the multi-bind form comes off again with no array
    glBindImageTextures(0, 1, &tex);
    glGetIntegeri_v(GL_IMAGE_BINDING_NAME, 0, &v);
    CHECK_EQ_INT(v, (GLint)tex);
    glBindImageTextures(0, 1, NULL);
    glGetIntegeri_v(GL_IMAGE_BINDING_NAME, 0, &v);
    CHECK_EQ_INT(v, 0);

    // one face of one cube of a cube map array takes a sub-image
    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, cube);
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_RGBA8, 1, 1, 6);
    glTexSubImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, 1, 1, 6, GL_RGBA, GL_UNSIGNED_BYTE, texel);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    glDeleteTextures(1, &cube);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(3, bufs);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

// GL_TEXTURE_BINDING_CUBE_MAP_ARRAY reads back like every other binding.
// It was an unknown enum.
GPU_TEST(misc_core, cube_map_array_binding_reads_back)
{
    GLuint tex;
    GLint v = -1;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, tex);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, (GLint)tex);

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
    glDeleteTextures(1, &tex);
}

static GLint compiles(const char *src)
{
    GLuint sh = glCreateShader(GL_VERTEX_SHADER);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    glDeleteShader(sh);

    return ok;
}

// mix() on integers needs GLSL 4.50 or GL_EXT_shader_integer_mix; glslang
// only warned about it.
GPU_TEST(misc_core, integer_mix_needs_its_extension_before_450)
{
    CHECK_EQ_INT(compiles("#version 330\nvoid main() { ivec2 a = mix(ivec2(1), ivec2(2), bvec2(true)); "
                          "gl_Position = vec4(a, 0, 1); }\n"), GL_FALSE);
    CHECK_EQ_INT(compiles("#version 330\n#extension GL_EXT_shader_integer_mix : enable\n"
                          "void main() { ivec2 a = mix(ivec2(1), ivec2(2), bvec2(true)); "
                          "gl_Position = vec4(a, 0, 1); }\n"), GL_TRUE);
    CHECK_EQ_INT(compiles("#version 450\nvoid main() { ivec2 a = mix(ivec2(1), ivec2(2), bvec2(true)); "
                          "gl_Position = vec4(a, 0, 1); }\n"), GL_TRUE);
}

static GLint fragmentCompiles(const char *src)
{
    GLuint sh = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    glDeleteShader(sh);

    return ok;
}

// Keywords are not names. MGL's own rewrites hid two cases from glslang: a
// subroutine type it drops, and "invariant", which it strips from inputs.
GPU_TEST(misc_core, keywords_are_not_names)
{
    CHECK_EQ_INT(fragmentCompiles("#version 460\nsubroutine void uniform(inout vec4 a);\n"
                                  "subroutine (uniform) void f(inout vec4 a) { a = vec4(1); }\n"
                                  "void main() { }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 460\nsubroutine void sampler2D(inout vec4 a);\n"
                                  "void main() { }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 460\nin vec4 invariant;\nvoid main() { }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 460\nvoid test(in vec4 invariant) { }\nvoid main() { }\n"), GL_FALSE);

    // still fine as what they are
    CHECK_EQ_INT(fragmentCompiles("#version 460\ninvariant in vec4 v;\nout vec4 o;\nvoid main() { o = v; }\n"), GL_TRUE);
    CHECK_EQ_INT(fragmentCompiles("#version 460\nsubroutine void samplerFn(inout vec4 a);\n"
                                  "subroutine (samplerFn) void f(inout vec4 a) { a = vec4(1); }\n"
                                  "subroutine uniform samplerFn s;\nout vec4 o;\n"
                                  "void main() { vec4 a = vec4(0); s(a); o = a; }\n"), GL_TRUE);
}

// std430 is for buffer blocks, and imageSize arrived in GLSL 4.30. glslang
// takes both when it writes SPIR-V, so they compiled.
GPU_TEST(misc_core, std430_uniforms_and_early_image_size_do_not_compile)
{
    CHECK_EQ_INT(fragmentCompiles("#version 430 core\nlayout(std430) uniform UBO { int x; };\n"
                                  "buffer SSBO { int y; };\nvoid main() { y = x; }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 430 core\nlayout(std430) buffer SSBO { int y; };\n"
                                  "void main() { y = 1; }\n"), GL_TRUE);
    CHECK_EQ_INT(fragmentCompiles("#version 420 core\nlayout(location = 0) out vec4 c;\n"
                                  "layout(binding = 0, rg16f) uniform image2D img;\n"
                                  "void main() { c = vec4(imageSize(img), 0, 1); }\n"), GL_FALSE);
    CHECK_EQ_INT(fragmentCompiles("#version 430 core\nlayout(location = 0) out vec4 c;\n"
                                  "layout(binding = 0, rg16f) uniform image2D img;\n"
                                  "void main() { c = vec4(imageSize(img), 0, 1); }\n"), GL_TRUE);
}

// Each stage needs a main. A vertex shader without one linked.
GPU_TEST(misc_core, a_stage_without_main_does_not_link)
{
    static const char *VS = "#version 460\nvoid niam() { gl_Position = vec4(0); }\n";
    static const char *FS = "#version 460\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    char log[1024] = { 0 };
    GLuint prog = mgl_build_program(VS, FS, log, sizeof log);

    CHECK_EQ_UINT(prog, 0);
    if (prog)
        glDeleteProgram(prog);

    // nor does a separable program holding just that stage
    static const char *NOMAIN_FS = "#version 310 es\nuniform highp vec4 u_color;\n"
                                   "layout(location = 0) out mediump vec4 o_color;\n"
                                   "void niaM(void) { o_color = u_color; }\n";
    static const char *MAIN_FS = "#version 310 es\nuniform highp vec4 u_color;\n"
                                 "layout(location = 0) out mediump vec4 o_color;\n"
                                 "void main(void) { o_color = u_color; }\n";
    GLuint sep = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &MAIN_FS), fs = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = -1;

    // the shader compiles once with a main, then again without one
    glShaderSource(fs, 1, &MAIN_FS, NULL);
    glCompileShader(fs);
    glAttachShader(sep, fs);
    glShaderSource(fs, 1, &NOMAIN_FS, NULL);
    glCompileShader(fs);
    glLinkProgram(sep);
    glGetProgramiv(sep, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);

    glDeleteShader(fs);
    glDeleteProgram(sep);
    mgl_drain_errors();
}

// A shader whose latest compile failed fails the link, whatever it compiled
// to before.
GPU_TEST(misc_core, a_failed_recompile_fails_the_link)
{
    static const char *GOOD = "#version 460\nout vec4 o;\nvoid main() { o = vec4(1); }\n";
    static const char *BAD = "#version 460\nout vec4 o;\nvoid main() { o = vec4(1) }\n";
    static const char *VS = "#version 460\nvoid main() { gl_Position = vec4(0); }\n";
    GLuint prog = glCreateProgram(), vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = -1;

    glShaderSource(vs, 1, &VS, NULL);
    glCompileShader(vs);
    glShaderSource(fs, 1, &GOOD, NULL);
    glCompileShader(fs);
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glShaderSource(fs, 1, &BAD, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_FALSE);

    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteProgram(prog);
    mgl_drain_errors();
}

/* ---------- glClipControl changes what is drawn ---------- */

static const char *CLIP_VS =
    "#version 450 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "uniform float z;\n"
    "uniform float lower;\n"
    "void main() { gl_Position = vec4(pos.x, lower > 0.5 ? pos.y * 0.5 - 0.5 : pos.y, z, 1.0); }\n";
static const char *CLIP_FS =
    "#version 450 core\n"
    "out vec4 c;\n"
    "void main() { c = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static GLfloat clipDepthAfter(GLuint prog, GLuint vao, GLenum mode, GLfloat z)
{
    GLfloat d = -1.0f;

    glClipControl(GL_LOWER_LEFT, mode);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUniform1f(glGetUniformLocation(prog, "z"), z);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glReadPixels(4, 4, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
    return d;
}

GPU_TEST(misc_core, clip_depth_mode_zero_to_one_keeps_z)
{
    MGLTestTarget t;
    GLuint vbo, vao, prog;
    char log[1024];

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 1)) { CHECK(0); return; }
    prog = mgl_build_program(CLIP_VS, CLIP_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "clip program did not build: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);

    GLfloat d = clipDepthAfter(prog, vao, GL_NEGATIVE_ONE_TO_ONE, 0.5f);
    CHECK_MSG(fabsf(d - 0.75f) < 1e-3f, "-1..1: depth %g, want 0.75", d);

    d = clipDepthAfter(prog, vao, GL_ZERO_TO_ONE, 0.5f);
    CHECK_MSG(fabsf(d - 0.5f) < 1e-3f, "0..1: depth %g, want 0.5", d);

    // below zero is outside the clip volume now, so nothing is drawn
    d = clipDepthAfter(prog, vao, GL_ZERO_TO_ONE, -0.5f);
    CHECK_MSG(fabsf(d - 1.0f) < 1e-3f, "0..1 at z -0.5: depth %g, want 1 (clipped)", d);

    d = clipDepthAfter(prog, vao, GL_NEGATIVE_ONE_TO_ONE, -0.5f);
    CHECK_MSG(fabsf(d - 0.25f) < 1e-3f, "back to -1..1: depth %g, want 0.25", d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(misc_core, clip_origin_upper_left_turns_the_picture_over)
{
    MGLTestTarget t;
    GLuint vbo, vao, prog;
    unsigned char *px, top[4], bottom[4];
    char log[1024];

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0)) { CHECK(0); return; }
    prog = mgl_build_program(CLIP_VS, CLIP_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "clip program did not build: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "lower"), 1.0f);

    // back faces are culled; flipping the origin must not make the quad one
    glEnable(GL_CULL_FACE);

    for (int upper = 0; upper < 2; upper++)
    {
        glClipControl(upper ? GL_UPPER_LEFT : GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        px = mgl_read_rgba8(&t);
        mgl_pixel_at(px, &t, 4, 6, top);
        mgl_pixel_at(px, &t, 4, 1, bottom);
        free(px);

        CHECK_MSG((upper ? top[1] : bottom[1]) == 255 && (upper ? bottom[1] : top[1]) == 0,
                  "origin %s: top green %d, bottom green %d", upper ? "upper-left" : "lower-left",
                  top[1], bottom[1]);
    }
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* ---------- polygon offset and polygon mode reach the draw ---------- */

static GLfloat offsetDepth(GLuint prog, GLboolean on, GLfloat units, GLfloat clamp)
{
    GLfloat d = -1.0f;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (on) glEnable(GL_POLYGON_OFFSET_FILL); else glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffsetClamp(0.0f, units, clamp);
    glUniform1f(glGetUniformLocation(prog, "z"), 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glReadPixels(4, 4, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
    return d;
}

GPU_TEST(misc_core, polygon_offset_moves_depth)
{
    MGLTestTarget t;
    GLuint vbo, vao, prog;
    char log[1024];

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 1)) { CHECK(0); return; }
    prog = mgl_build_program(CLIP_VS, CLIP_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "offset program did not build: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);

    GLfloat ref = offsetDepth(prog, GL_FALSE, 0.0f, 0.0f);
    GLfloat up = offsetDepth(prog, GL_TRUE, 5000.0f, 0.0f);
    GLfloat down = offsetDepth(prog, GL_TRUE, -5000.0f, 0.0f);
    GLfloat clamped = offsetDepth(prog, GL_TRUE, 5000.0f, 0.0001f);

    CHECK_MSG(fabsf(ref - 0.5f) < 1e-3f, "no offset: depth %g, want 0.5", ref);
    CHECK_MSG(up > ref + 1e-4f, "offset up: depth %g, reference %g", up, ref);
    CHECK_MSG(down < ref - 1e-4f, "offset down: depth %g, reference %g", down, ref);
    CHECK_MSG(clamped > ref && clamped < up, "clamped: depth %g, between %g and %g", clamped, ref, up);
    glDisable(GL_POLYGON_OFFSET_FILL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

GPU_TEST(misc_core, polygon_mode_line_leaves_the_middle_empty)
{
    MGLTestTarget t;
    GLuint vbo, vao, prog;
    unsigned char *px, mid[4];
    char log[1024];

    if (!mgl_target_create(&t, 16, 16, GL_RGBA8, 0)) { CHECK(0); return; }
    prog = mgl_build_program(CLIP_VS, CLIP_FS, log, sizeof log);
    CHECK_MSG(prog != 0, "line program did not build: %s", log);
    if (!prog) { mgl_target_destroy(&t); return; }

    vao = mgl_fullscreen_quad(&vbo);
    glUseProgram(prog);
    glUniform1f(glGetUniformLocation(prog, "lower"), 0.0f);

    glPolygonMode(GL_FRONT, GL_LINE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // a quad of two triangles shares a diagonal, so look off it
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 11, 4, mid);
    free(px);
    CHECK_MSG(mid[1] == 0, "line mode filled the inside (green %d)", mid[1]);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 11, 4, mid);
    free(px);
    CHECK_MSG(mid[1] == 255, "fill mode left the inside empty (green %d)", mid[1]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
