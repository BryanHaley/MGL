/*
 * test_tex_sampler_params.c
 * MGL
 *
 * Texture and sampler parameter setters, including the integer variants.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- glCreateSamplers, glBindSamplers ---------- */

GPU_TEST(tex_sampler_params, create_and_bind_samplers)
{
    GLuint s[3] = { 0 };

    glCreateSamplers(3, s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(s[0] != 0 && s[1] != 0 && s[2] != 0);
    CHECK(s[0] != s[1] && s[1] != s[2] && s[0] != s[2]);

    CHECK_EQ_INT(glIsSampler(s[0]), GL_TRUE);

    // bind all three at once
    glBindSamplers(0, 3, s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 3; i++)
    {
        GLint bound = -1;
        glGetIntegeri_v(GL_SAMPLER_BINDING, i, &bound);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(bound, (GLint)s[i]);
    }

    // unbind with NULL
    glBindSamplers(0, 3, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 3; i++)
    {
        GLint bound = -2;
        glGetIntegeri_v(GL_SAMPLER_BINDING, i, &bound);
        CHECK_EQ_INT(bound, 0);
    }

    // first + count exceeds limit
    glBindSamplers(999999, 1, s);
    // spec: GL_INVALID_OPERATION when first+count exceeds TEXTURE_UNITS
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // negative count
    glBindSamplers(0, -1, s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // negative n for CreateSamplers
    glCreateSamplers(-1, s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteSamplers(3, s);
}

/* ---------- glSamplerParameterfv, glSamplerParameteriv ---------- */

GPU_TEST(tex_sampler_params, sampler_parameter_fv_and_iv)
{
    GLuint s = 0;

    glCreateSamplers(1, &s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set border color via fv
    glSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR,
                         (const GLfloat[]){ 0.25f, 0.5f, 0.75f, 1.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLfloat got[4] = { -1, -1, -1, -1 };
        glGetSamplerParameterfv(s, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 0.25f, 1e-5f);
        CHECK_NEAR(got[1], 0.50f, 1e-5f);
        CHECK_NEAR(got[2], 0.75f, 1e-5f);
        CHECK_NEAR(got[3], 1.00f, 1e-5f);
    }

    // set scalar via fv (one-element array)
    glSamplerParameterfv(s, GL_TEXTURE_MIN_LOD,
                         (const GLfloat[]){ -500.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLfloat got = 9999.0f;
        glGetSamplerParameterfv(s, GL_TEXTURE_MIN_LOD, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got, -500.0f, 1e-5f);
    }

    // set scalar via iv
    glSamplerParameteriv(s, GL_TEXTURE_MAX_LOD, (const GLint[]){ 500 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLint got = -1;
        glGetSamplerParameteriv(s, GL_TEXTURE_MAX_LOD, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(got, 500);
    }

    // set wrap mode via iv
    glSamplerParameteriv(s, GL_TEXTURE_WRAP_S, (const GLint[]){ GL_MIRRORED_REPEAT });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLint got = -1;
        glGetSamplerParameteriv(s, GL_TEXTURE_WRAP_S, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(got, GL_MIRRORED_REPEAT);
    }

    // invalid pname
    glSamplerParameterfv(s, 0x9999, (const GLfloat[]){ 0.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glSamplerParameteriv(s, 0x9999, (const GLint[]){ 0 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // invalid sampler name
    glSamplerParameterfv(999999, GL_TEXTURE_MIN_LOD, (const GLfloat[]){ 0.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteSamplers(1, &s);
}

/* ---------- glSamplerParameterIiv, glSamplerParameterIuiv ---------- */

GPU_TEST(tex_sampler_params, sampler_parameter_iiv_and_iuiv)
{
    GLuint s = 0;
    GLint  iiv[4]  = { -1, -2, -3, -4 };
    GLuint iuiv[4] = { 10, 20, 30, 40 };

    glCreateSamplers(1, &s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set border color via Iiv (signed integer)
    glSamplerParameterIiv(s, GL_TEXTURE_BORDER_COLOR, iiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLint got[4] = { 0 };
        glGetSamplerParameterIiv(s, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(got[0], -1);
        CHECK_EQ_INT(got[1], -2);
        CHECK_EQ_INT(got[2], -3);
        CHECK_EQ_INT(got[3], -4);
    }

    // set border color via Iuiv (unsigned integer)
    glSamplerParameterIuiv(s, GL_TEXTURE_BORDER_COLOR, iuiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLuint got[4] = { 0 };
        glGetSamplerParameterIuiv(s, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_UINT(got[0], 10u);
        CHECK_EQ_UINT(got[1], 20u);
        CHECK_EQ_UINT(got[2], 30u);
        CHECK_EQ_UINT(got[3], 40u);
    }

    // invalid pname for Iiv
    glSamplerParameterIiv(s, 0x9999, iiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // invalid pname for Iuiv
    glSamplerParameterIuiv(s, 0x9999, iuiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // invalid sampler name
    glSamplerParameterIiv(999999, GL_TEXTURE_BORDER_COLOR, iiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glSamplerParameterIuiv(999999, GL_TEXTURE_BORDER_COLOR, iuiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteSamplers(1, &s);
}

/* ---------- glTexParameterfv, glTexParameterIiv, glTexParameterIuiv ---------- */

GPU_TEST(tex_sampler_params, tex_parameter_bound_set_and_get)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set border color via glTexParameterfv
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                     (const GLfloat[]){ 0.1f, 0.2f, 0.3f, 0.4f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLfloat got[4] = { -1, -1, -1, -1 };
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 0.1f, 1e-5f);
        CHECK_NEAR(got[1], 0.2f, 1e-5f);
        CHECK_NEAR(got[2], 0.3f, 1e-5f);
        CHECK_NEAR(got[3], 0.4f, 1e-5f);
    }

    // set a scalar via glTexParameterfv (lod bias)
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS,
                     (const GLfloat[]){ 1.5f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLfloat got = 9999.0f;
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got, 1.5f, 1e-5f);
    }

    // set a scalar via glTexParameterfv (min filter, an integer enum)
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                     (const GLfloat[]){ (GLfloat)GL_LINEAR });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLint got = -1;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(got, GL_LINEAR);
    }

    // set border color via glTexParameterIiv
    glTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                      (const GLint[]){ -5, -6, -7, -8 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLint got[4] = { 0 };
        glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        // SPEC: should read back -5, -6, -7, -8
        // MGL's getTexParameterIiv is a stub that returns 0 -> will FAIL
        CHECK_EQ_INT(got[0], -5);
    }

    // set border color via glTexParameterIuiv
    glTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                       (const GLuint[]){ 50, 60, 70, 80 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLuint got[4] = { 0 };
        glGetTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        // SPEC: should read back 50, 60, 70, 80
        // MGL's getTexParameterIuiv is a stub that returns 0 -> will FAIL
        CHECK_EQ_UINT(got[0], 50u);
    }

    // errors: invalid target
    glTexParameterfv(0x9999, GL_TEXTURE_BORDER_COLOR,
                     (const GLfloat[]){ 0.0f, 0.0f, 0.0f, 0.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // errors: invalid pname
    glTexParameterfv(GL_TEXTURE_2D, 0x9999,
                     (const GLfloat[]){ 0.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // errors: no texture bound
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR,
                     (const GLfloat[]){ 0.0f, 0.0f, 0.0f, 0.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &t);
}

/* ---------- glTextureParameterf, glTextureParameterfv, glTextureParameteri,
             glTextureParameteriv  (direct state access) ---------- */

GPU_TEST(tex_sampler_params, texture_parameter_dsa_f_and_i)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set via glTextureParameterf (scalar float)
    glTextureParameterf(t, GL_TEXTURE_LOD_BIAS, 2.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set via glTextureParameteri (scalar int)
    glTextureParameteri(t, GL_TEXTURE_BASE_LEVEL, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // bind and verify
    glBindTexture(GL_TEXTURE_2D, t);

    {
        GLfloat got = 9999.0f;
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got, 2.0f, 1e-5f);
    }

    {
        GLint got = -1;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(got, 1);
    }

    // invalid texture name
    glTextureParameterf(999999, GL_TEXTURE_LOD_BIAS, 1.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glTextureParameteri(999999, GL_TEXTURE_BASE_LEVEL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &t);
}

GPU_TEST(tex_sampler_params, texture_parameter_dsa_fv_and_iv)
{
    GLuint t = 0;

    glGenTextures(1, &t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set border color via glTextureParameterfv
    glTextureParameterfv(t, GL_TEXTURE_BORDER_COLOR,
                         (const GLfloat[]){ 0.5f, 0.6f, 0.7f, 0.8f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set swizzle via glTextureParameteriv
    glTextureParameteriv(t, GL_TEXTURE_SWIZZLE_RGBA,
                         (const GLint[]){ GL_ZERO, GL_ONE, GL_RED, GL_GREEN });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindTexture(GL_TEXTURE_2D, t);

    {
        GLfloat got[4] = { -1, -1, -1, -1 };
        glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_NEAR(got[0], 0.5f, 1e-5f);
        CHECK_NEAR(got[1], 0.6f, 1e-5f);
        CHECK_NEAR(got[2], 0.7f, 1e-5f);
        CHECK_NEAR(got[3], 0.8f, 1e-5f);
    }

    {
        GLint got = -1;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(got, GL_ZERO);
    }

    {
        GLint got = -1;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, &got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(got, GL_ONE);
    }

    // invalid texture name
    glTextureParameterfv(999999, GL_TEXTURE_BORDER_COLOR,
                         (const GLfloat[]){ 0.0f, 0.0f, 0.0f, 0.0f });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glTextureParameteriv(999999, GL_TEXTURE_SWIZZLE_RGBA,
                         (const GLint[]){ GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &t);
}

/* ---------- glTextureParameterIiv, glTextureParameterIuiv ---------- */

GPU_TEST(tex_sampler_params, texture_parameter_dsa_iiv_and_iuiv)
{
    GLuint t = 0;
    GLint  iiv[4]  = { -10, -20, -30, -40 };
    GLuint iuiv[4] = { 100, 200, 300, 400 };

    glGenTextures(1, &t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set via glTextureParameterIiv
    glTextureParameterIiv(t, GL_TEXTURE_BORDER_COLOR, iiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // set via glTextureParameterIuiv
    glTextureParameterIuiv(t, GL_TEXTURE_BORDER_COLOR, iuiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBindTexture(GL_TEXTURE_2D, t);

    // Verify via glGetTexParameterIiv
    {
        GLint got[4] = { 0 };
        glGetTexParameterIiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        // SPEC: should read back -10, -20, -30, -40
        // MGL's getTexParameterIiv is a stub that returns 0 -> will FAIL
        CHECK_EQ_INT(got[0], -10);
    }

    // Verify via glGetTexParameterIuiv
    {
        GLuint got[4] = { 0 };
        glGetTexParameterIuiv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, got);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        // SPEC: should read back 100, 200, 300, 400
        // MGL's getTexParameterIuiv is a stub that returns 0 -> will FAIL
        CHECK_EQ_UINT(got[0], 100u);
    }

    // invalid pname
    glTextureParameterIiv(t, 0x9999, iiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glTextureParameterIuiv(t, 0x9999, iuiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // invalid texture name
    glTextureParameterIiv(999999, GL_TEXTURE_BORDER_COLOR, iiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glTextureParameterIuiv(999999, GL_TEXTURE_BORDER_COLOR, iuiv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteTextures(1, &t);
}
