/*
 * test_basic.c
 * MGL
 *
 * Context, strings, clears and readback against a real Metal device.
 */

#include "mgl_test.h"
#include "harness.h"

#define TARGET_W 64
#define TARGET_H 64

/* ---------- context and strings ---------- */

GPU_TEST(ctx, reports_46_core)
{
    const char *ver = (const char *)glGetString(GL_VERSION);
    const char *ren = (const char *)glGetString(GL_RENDERER);
    const char *ven = (const char *)glGetString(GL_VENDOR);
    const char *sl  = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);

    CHECK(ver && strstr(ver, "4.6"));
    CHECK(ren && *ren);
    CHECK(ven && *ven);
    CHECK(sl && *sl);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(ctx, major_minor_version)
{
    GLint major = 0, minor = 0;

    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    CHECK_EQ_INT(major, 4);
    CHECK_EQ_INT(minor, 6);
}

GPU_TEST(ctx, extension_enumeration_is_consistent)
{
    GLint n = 0;

    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    CHECK(n > 0);

    for (GLint i = 0; i < n; i++)
    {
        const char *e = (const char *)glGetStringi(GL_EXTENSIONS, i);
        CHECK_MSG(e && strncmp(e, "GL_", 3) == 0, "extension %d malformed", i);
    }

    // out of range must error, not crash
    glGetStringi(GL_EXTENSIONS, (GLuint)n);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(ctx, getstring_extensions_is_invalid_in_core)
{
    const GLubyte *s = glGetString(GL_EXTENSIONS);

    CHECK(s == NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(ctx, bad_getstring_enum_errors)
{
    glGetString(0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(ctx, common_limits_are_sane)
{
    GLint v = 0;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &v);
    CHECK_MSG(v >= 1024, "MAX_TEXTURE_SIZE = %d", v);

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &v);
    CHECK_MSG(v >= 16, "MAX_VERTEX_ATTRIBS = %d", v);

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &v);
    CHECK_MSG(v >= 4, "MAX_DRAW_BUFFERS = %d", v);

    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &v);
    CHECK_MSG(v >= 4, "MAX_COLOR_ATTACHMENTS = %d", v);
}

/* ---------- error machinery ---------- */

GPU_TEST(errors, geterror_clears_after_read)
{
    glGetString(0x9999);                        // force an error
    CHECK_EQ_UINT(glGetError(), GL_INVALID_ENUM);
    CHECK_EQ_UINT(glGetError(), GL_NO_ERROR);   // must reset
}

// MGL exports these even though glcorearb.h does not declare them
extern void   glBegin(GLenum mode);
extern void   glLoadIdentity(void);
extern void   glMatrixMode(GLenum mode);
extern GLuint glGenLists(GLsizei range);
#define MGL_GL_MODELVIEW 0x1700

GPU_TEST(errors, compatibility_calls_error_not_crash)
{
    // these are not in 4.6 core; they must set an error rather than abort
    glBegin(GL_TRIANGLES);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glLoadIdentity();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glMatrixMode(MGL_GL_MODELVIEW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    CHECK_EQ_UINT(glGenLists(1), 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- state setters that used to crash ---------- */

GPU_TEST(state, polygon_offset_roundtrips)
{
    GLfloat f = 0.0f, u = 0.0f;

    glPolygonOffset(1.5f, -2.5f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &f);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &u);

    CHECK_NEAR(f, 1.5, 1e-6);
    CHECK_NEAR(u, -2.5, 1e-6);
}

GPU_TEST(state, blend_func_separate_accepts_valid_factors)
{
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_SUBTRACT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(state, blend_func_rejects_bad_enum)
{
    glBlendFuncSeparate(0x9999, GL_ONE, GL_ONE, GL_ONE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBlendEquationSeparate(0x9999, GL_FUNC_ADD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(state, blend_func_accepts_src_alpha_saturate)
{
    // valid in 4.6 but was missing from the validator
    glBlendFunc(GL_SRC_ALPHA_SATURATE, GL_ONE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(state, point_parameter_validates)
{
    glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, 2.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE, -1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glPointParameteri(0x9999, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(state, enable_disable_roundtrip)
{
    static const GLenum caps[] = {
        GL_DEPTH_TEST, GL_STENCIL_TEST, GL_BLEND, GL_CULL_FACE, GL_SCISSOR_TEST,
    };

    for (unsigned i = 0; i < sizeof caps / sizeof caps[0]; i++)
    {
        glEnable(caps[i]);
        CHECK_MSG(glIsEnabled(caps[i]) == GL_TRUE, "enable cap 0x%x", caps[i]);

        glDisable(caps[i]);
        CHECK_MSG(glIsEnabled(caps[i]) == GL_FALSE, "disable cap 0x%x", caps[i]);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(state, internalformat_query)
{
    GLint supported = 0;

    glGetInternalformativ(GL_TEXTURE_2D, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 1, &supported);
    CHECK_EQ_INT(supported, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetInternalformativ(0x9999, GL_RGBA8, GL_INTERNALFORMAT_SUPPORTED, 1, &supported);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(state, get_integer64)
{
    GLint64 v = 0;

    glGetInteger64v(GL_MAX_ELEMENT_INDEX, &v);
    CHECK(v > 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- offscreen target + readback ---------- */

GPU_TEST(fbo, creates_complete_target)
{
    MGLTestTarget t;

    if (!mgl_target_create(&t, TARGET_W, TARGET_H, GL_RGBA8, 0))
        SKIP("could not create RGBA8 target");

    CHECK_EQ_UINT(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    mgl_target_destroy(&t);
}

GPU_TEST(clear, solid_color_reads_back)
{
    MGLTestTarget t;
    unsigned char *px;
    unsigned char c[4];

    if (!mgl_target_create(&t, TARGET_W, TARGET_H, GL_RGBA8, 0))
        SKIP("no RGBA8 target");

    mgl_target_bind(&t);
    glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (!px) { mgl_target_destroy(&t); SKIP("readback allocation failed"); }

    mgl_pixel_at(px, &t, TARGET_W / 2, TARGET_H / 2, c);

    // 0.25*255=63.75->64, 0.5*255=127.5->128, 0.75*255=191.25->191
    CHECK_MSG(abs((int)c[0] - 64) <= 1, "R = %d, want ~64", c[0]);
    CHECK_MSG(abs((int)c[1] - 128) <= 1, "G = %d, want ~128", c[1]);
    CHECK_MSG(abs((int)c[2] - 191) <= 1, "B = %d, want ~191", c[2]);
    CHECK_MSG(c[3] == 255, "A = %d, want 255", c[3]);

    free(px);
    mgl_target_destroy(&t);
}

GPU_TEST(clear, black_and_white)
{
    MGLTestTarget t;
    unsigned char *px, c[4];

    if (!mgl_target_create(&t, 16, 16, GL_RGBA8, 0)) SKIP("no target");

    mgl_target_bind(&t);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 8, 8, c);
    CHECK_EQ_UINT(c[0], 0); CHECK_EQ_UINT(c[1], 0); CHECK_EQ_UINT(c[2], 0);
    free(px);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 8, 8, c);
    CHECK_EQ_UINT(c[0], 255); CHECK_EQ_UINT(c[1], 255); CHECK_EQ_UINT(c[2], 255);
    free(px);

    mgl_target_destroy(&t);
}

GPU_TEST(clear, scissor_limits_the_clear)
{
    MGLTestTarget t;
    unsigned char *px, inside[4], outside[4];

    if (!mgl_target_create(&t, 32, 32, GL_RGBA8, 0)) SKIP("no target");

    mgl_target_bind(&t);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, 16, 16);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    px = mgl_read_rgba8(&t);
    if (!px) { mgl_target_destroy(&t); SKIP("readback failed"); }

    mgl_pixel_at(px, &t, 4, 4, inside);      // inside scissor
    mgl_pixel_at(px, &t, 28, 28, outside);   // outside scissor

    CHECK_MSG(inside[0] > 200, "inside scissor R = %d, want red", inside[0]);
    CHECK_MSG(outside[0] < 50, "outside scissor R = %d, want black", outside[0]);

    free(px);
    mgl_target_destroy(&t);
}

GPU_TEST(readpixels, subrect_matches_full_read)
{
    MGLTestTarget t;
    unsigned char full[32 * 32 * 4];
    unsigned char sub[8 * 8 * 4];

    if (!mgl_target_create(&t, 32, 32, GL_RGBA8, 0)) SKIP("no target");

    mgl_target_bind(&t);
    glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, full);
    glReadPixels(8, 8, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, sub);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // uniform colour, so any sub pixel must equal any full pixel
    for (int i = 0; i < 4; i++)
        CHECK_EQ_UINT(sub[i], full[i]);

    mgl_target_destroy(&t);
}

GPU_TEST(readpixels, rejects_bad_format_type_pair)
{
    unsigned char buf[64];

    // 5_6_5 only pairs with RGB
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, buf);
    CHECK(mgl_drain_errors() != GL_NO_ERROR);
}

GPU_TEST(readpixels, negative_size_errors)
{
    unsigned char buf[64];

    glReadPixels(0, 0, -1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(readpixels, float_target_roundtrip)
{
    MGLTestTarget t;
    float px[16 * 16 * 4];

    if (!mgl_target_create(&t, 16, 16, GL_RGBA32F, 0))
        SKIP("RGBA32F target unsupported");

    mgl_target_bind(&t);
    glClearColor(0.125f, 0.25f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 16, 16, GL_RGBA, GL_FLOAT, px);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    CHECK_NEAR(px[0], 0.125, 1e-5);
    CHECK_NEAR(px[1], 0.25, 1e-5);
    CHECK_NEAR(px[2], 0.5, 1e-5);
    CHECK_NEAR(px[3], 1.0, 1e-5);

    mgl_target_destroy(&t);
}
