/*
 * test_state_indexed.c
 * MGL
 *
 * Indexed and separate state: per-buffer blending, viewport/scissor arrays,
 * stencil separation, clip distances, and miscellaneous single-value state
 * entry points that had no coverage.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- per-buffer blending ---------- */

GPU_TEST(state_indexed, blend_equationi_sets_one_buffer)
{
    glBlendEquationi(0, GL_FUNC_ADD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBlendEquationi(2, GL_FUNC_SUBTRACT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // invalid mode
    glBlendEquationi(0, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // buf >= MAX_COLOR_ATTACHMENTS
    glBlendEquationi(999, GL_FUNC_ADD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(state_indexed, blend_equation_separatei_sets_one_buffer)
{
    glBlendEquationSeparatei(1, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // bad RGB mode
    glBlendEquationSeparatei(1, 0x9999, GL_FUNC_ADD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // bad Alpha mode
    glBlendEquationSeparatei(1, GL_FUNC_ADD, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // out of range buffer
    glBlendEquationSeparatei(999, GL_FUNC_ADD, GL_FUNC_ADD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(state_indexed, blend_func_separatei_sets_one_buffer)
{
    glBlendFuncSeparatei(2, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                         GL_ONE, GL_ZERO);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // bad srcRGB
    glBlendFuncSeparatei(0, 0x9999, GL_ONE, GL_ZERO, GL_ZERO);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // out of range buffer
    glBlendFuncSeparatei(999, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- depth clear and range ---------- */

GPU_TEST(state_indexed, clear_depthf_round_trips)
{
    GLfloat v;

    // default is 1.0
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &v);
    CHECK_NEAR(v, 1.0f, 1e-6f);

    glClearDepthf(0.375f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &v);
    CHECK_NEAR(v, 0.375f, 1e-6f);
}

GPU_TEST(state_indexed, depth_rangef_round_trips)
{
    GLdouble vals[2];

    // default near=0, far=1
    glGetDoublev(GL_DEPTH_RANGE, vals);
    CHECK_NEAR(vals[0], 0.0, 1e-6);

    glDepthRangef(0.25f, 0.75f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetDoublev(GL_DEPTH_RANGE, vals);
    CHECK_NEAR(vals[0], 0.25, 1e-6);
    CHECK_NEAR(vals[1], 0.75, 1e-6);
}

GPU_TEST(state_indexed, depth_range_arrayv_sets_multiple)
{
    GLdouble v0[2], v1[2];
    const GLdouble data[] = { 0.1, 0.5, 0.6, 0.9 };

    glDepthRangeArrayv(0, 2, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetDoublei_v(GL_DEPTH_RANGE, 0, v0);
    glGetDoublei_v(GL_DEPTH_RANGE, 1, v1);
    CHECK_NEAR(v0[0], 0.1, 1e-6);
    CHECK_NEAR(v0[1], 0.5, 1e-6);
    CHECK_NEAR(v1[0], 0.6, 1e-6);
    CHECK_NEAR(v1[1], 0.9, 1e-6);

    // out of range first
    glDepthRangeArrayv(99, 1, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // count wraps past MAX_VIEWPORTS
    glDepthRangeArrayv(14, 4, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // count < 0
    glDepthRangeArrayv(0, -1, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // null pointer
    glDepthRangeArrayv(0, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- color mask indexed ---------- */

GPU_TEST(state_indexed, color_maski_affects_only_the_indexed_buffer)
{
    GLboolean mask[4];

    // reset to full write
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // disable writes on buffer 1; per spec buffer 0 must be unaffected
    glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // read buffer 0 — spec says it must still be (T,T,T,T)
    glGetBooleanv(GL_COLOR_WRITEMASK, mask);
    CHECK(mask[0] == GL_TRUE);
    CHECK(mask[1] == GL_TRUE);
    CHECK(mask[2] == GL_TRUE);
    CHECK(mask[3] == GL_TRUE);
}

/* ---------- clip distance enable/query ---------- */

GPU_TEST(state_indexed, is_enabledi_clip_distances)
{
    GLboolean v;

    // start disabled
    v = glIsEnabledi(GL_CLIP_DISTANCE0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, GL_FALSE);

    glEnablei(GL_CLIP_DISTANCE0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    v = glIsEnabledi(GL_CLIP_DISTANCE0, 0);
    CHECK_EQ_INT(v, GL_TRUE);

    glDisablei(GL_CLIP_DISTANCE0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    v = glIsEnabledi(GL_CLIP_DISTANCE0, 0);
    CHECK_EQ_INT(v, GL_FALSE);

    // index out of range
    glIsEnabledi(GL_CLIP_DISTANCE0, 9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // invalid target
    glIsEnabledi(0x9999, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- graphics reset status ---------- */

GPU_TEST(state_indexed, graphics_reset_status_is_no_error)
{
    CHECK_EQ_UINT(glGetGraphicsResetStatus(), GL_NO_ERROR);
}

/* ---------- multisample sample position ---------- */

GPU_TEST(state_indexed, get_multisamplefv_returns_sample_position)
{
    GLfloat pos[2] = { -1, -1 };

    glGetMultisamplefv(GL_SAMPLE_POSITION, 0, pos);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // sample position must be in [0,1]
    CHECK(pos[0] >= 0.0f && pos[0] <= 1.0f);
    CHECK(pos[1] >= 0.0f && pos[1] <= 1.0f);

    // invalid pname
    glGetMultisamplefv(0x9999, 0, pos);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- pixel store float ---------- */

GPU_TEST(state_indexed, pixel_storef_accepts_valid_pnames)
{
    glPixelStoref(GL_PACK_SWAP_BYTES, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPixelStoref(GL_PACK_SWAP_BYTES, 0.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPixelStoref(GL_PACK_ROW_LENGTH, 64.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPixelStoref(GL_PACK_ALIGNMENT, 4.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // invalid pname
    glPixelStoref(0x9999, 0.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // negative param -> GL_INVALID_VALUE (glPixelStoref truncates to int)
    glPixelStoref(GL_PACK_ROW_LENGTH, -1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- point parameters ---------- */

GPU_TEST(state_indexed, point_parameterfv_sets_threshold)
{
    glPointParameterfv(GL_POINT_FADE_THRESHOLD_SIZE, (const GLfloat[]){ 2.5f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // negative threshold
    glPointParameterfv(GL_POINT_FADE_THRESHOLD_SIZE, (const GLfloat[]){ -1.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // GL_POINT_SPRITE_COORD_ORIGIN
    glPointParameterfv(GL_POINT_SPRITE_COORD_ORIGIN, (const GLfloat[]){ (GLfloat)GL_LOWER_LEFT });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPointParameterfv(GL_POINT_SPRITE_COORD_ORIGIN, (const GLfloat[]){ (GLfloat)GL_UPPER_LEFT });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // bad origin value
    glPointParameterfv(GL_POINT_SPRITE_COORD_ORIGIN, (const GLfloat[]){ 999.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // invalid pname
    glPointParameterfv(0x9999, (const GLfloat[]){ 0.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // null pointer
    glPointParameterfv(GL_POINT_FADE_THRESHOLD_SIZE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(state_indexed, point_parameteriv_sets_origin)
{
    glPointParameteriv(GL_POINT_SPRITE_COORD_ORIGIN, (const GLint[]){ GL_LOWER_LEFT });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // bad origin
    glPointParameteriv(GL_POINT_SPRITE_COORD_ORIGIN, (const GLint[]){ 0x9999 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // invalid pname
    glPointParameteriv(0x9999, (const GLint[]){ 0 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // null pointer
    glPointParameteriv(GL_POINT_FADE_THRESHOLD_SIZE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- polygon offset clamp ---------- */

GPU_TEST(state_indexed, polygon_offset_clamp_stores_all_three)
{
    GLfloat factor, units;

    glPolygonOffsetClamp(2.0f, 4.0f, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // factor and units are readable via the standard getters
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &factor);
    CHECK_NEAR(factor, 2.0f, 1e-6f);

    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &units);
    CHECK_NEAR(units, 4.0f, 1e-6f);

    // clamp has no getter, but a second call with different values is valid
    glPolygonOffsetClamp(-1.0f, -2.0f, 0.5f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- stencil separate ---------- */

GPU_TEST(state_indexed, stencil_func_separate_front_and_back)
{
    GLint front_func, back_func;
    GLint front_ref, back_ref;
    GLint front_mask, back_mask;

    // reset defaults
    glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xFFFFFFFFu);
    glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xFFFFFFFFu);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set front only
    glStencilFuncSeparate(GL_FRONT, GL_LESS, 5, 0xAAu);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegerv(GL_STENCIL_FUNC, &front_func);
    CHECK_EQ_INT(front_func, GL_LESS);
    glGetIntegerv(GL_STENCIL_REF, &front_ref);
    CHECK_EQ_INT(front_ref, 5);
    glGetIntegerv(GL_STENCIL_VALUE_MASK, &front_mask);
    CHECK_EQ_INT(front_mask, 0xAA);

    // back must be unchanged
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &back_func);
    CHECK_EQ_INT(back_func, GL_ALWAYS);
    glGetIntegerv(GL_STENCIL_BACK_REF, &back_ref);
    CHECK_EQ_INT(back_ref, 0);
    glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &back_mask);
    // 0xFFFFFFFF read as GLint via glGetIntegerv is -1; compare as unsigned
    CHECK_EQ_UINT((GLuint)back_mask, 0xFFFFFFFFu);

    // set back
    glStencilFuncSeparate(GL_BACK, GL_GEQUAL, 9, 0xFFu);
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &back_func);
    CHECK_EQ_INT(back_func, GL_GEQUAL);

    // set both
    glStencilFuncSeparate(GL_FRONT_AND_BACK, GL_EQUAL, 7, 0x77u);
    glGetIntegerv(GL_STENCIL_FUNC, &front_func);
    CHECK_EQ_INT(front_func, GL_EQUAL);
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &back_func);
    CHECK_EQ_INT(back_func, GL_EQUAL);

    // invalid face
    glStencilFuncSeparate(0x9999, GL_ALWAYS, 0, 0xFFFFFFFFu);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // invalid func
    glStencilFuncSeparate(GL_FRONT, 0x9999, 0, 0xFFFFFFFFu);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(state_indexed, stencil_mask_separate_front_and_back)
{
    GLint front_mask, back_mask;

    glStencilMaskSeparate(GL_FRONT, 0xAAu);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegerv(GL_STENCIL_WRITEMASK, &front_mask);
    CHECK_EQ_INT(front_mask, 0xAA);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &back_mask);
    CHECK_EQ_INT(back_mask, 0xFFFFFFFFu);  // default

    glStencilMaskSeparate(GL_BACK, 0x55u);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &back_mask);
    CHECK_EQ_INT(back_mask, 0x55);

    glStencilMaskSeparate(GL_FRONT_AND_BACK, 0xFFu);
    glGetIntegerv(GL_STENCIL_WRITEMASK, &front_mask);
    CHECK_EQ_INT(front_mask, 0xFF);
    glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &back_mask);
    CHECK_EQ_INT(back_mask, 0xFF);

    // invalid face
    glStencilMaskSeparate(0x9999, 0xFFu);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- indexed viewport / scissor arrays ---------- */

GPU_TEST(state_indexed, viewport_indexedfv_sets_one)
{
    GLfloat v[4] = { 99, 99, 99, 99 };

    glViewportIndexedfv(2, (const GLfloat[]){ 10.0f, 20.0f, 128.0f, 64.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetFloati_v(GL_VIEWPORT, 2, v);
    CHECK_NEAR(v[0], 10.0f, 1e-5f);
    CHECK_NEAR(v[1], 20.0f, 1e-5f);
    CHECK_NEAR(v[2], 128.0f, 1e-5f);
    CHECK_NEAR(v[3], 64.0f, 1e-5f);

    // viewport 0 must be unaffected
    glGetFloati_v(GL_VIEWPORT, 0, v);
    CHECK_NEAR(v[2], 256.0f, 1e-5f);  // default width

    // index out of range
    glViewportIndexedfv(99, (const GLfloat[]){ 0, 0, 64, 64 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // negative width
    glViewportIndexedfv(0, (const GLfloat[]){ 0, 0, -1, 64 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // null pointer
    glViewportIndexedfv(0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(state_indexed, scissor_arrayv_sets_multiple)
{
    GLint v[4] = { 99, 99, 99, 99 };
    const GLint data[] = { 0, 0, 100, 100, 10, 10, 50, 50 };

    glScissorArrayv(0, 2, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegeri_v(GL_SCISSOR_BOX, 0, v);
    CHECK_EQ_INT(v[0], 0);
    CHECK_EQ_INT(v[1], 0);
    CHECK_EQ_INT(v[2], 100);
    CHECK_EQ_INT(v[3], 100);

    glGetIntegeri_v(GL_SCISSOR_BOX, 1, v);
    CHECK_EQ_INT(v[0], 10);
    CHECK_EQ_INT(v[1], 10);
    CHECK_EQ_INT(v[2], 50);
    CHECK_EQ_INT(v[3], 50);

    // out of range first
    glScissorArrayv(99, 1, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // count wraps past MAX_VIEWPORTS
    glScissorArrayv(15, 2, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // negative width in data
    glScissorArrayv(0, 1, (const GLint[]){ 0, 0, -1, 64 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // null pointer
    glScissorArrayv(0, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(state_indexed, scissor_indexedv_sets_one)
{
    GLint v[4];

    glScissorIndexedv(3, (const GLint[]){ 5, 10, 200, 150 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegeri_v(GL_SCISSOR_BOX, 3, v);
    CHECK_EQ_INT(v[0], 5);
    CHECK_EQ_INT(v[1], 10);
    CHECK_EQ_INT(v[2], 200);
    CHECK_EQ_INT(v[3], 150);

    // index out of range
    glScissorIndexedv(99, (const GLint[]){ 0, 0, 64, 64 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // negative width
    glScissorIndexedv(0, (const GLint[]){ 0, 0, -1, 64 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // null pointer
    glScissorIndexedv(0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}
