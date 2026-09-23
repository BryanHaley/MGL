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
