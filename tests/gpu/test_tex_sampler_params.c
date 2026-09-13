/*
 * test_tex_sampler_params.c
 * MGL
 *
 * Texture and sampler parameter setters, including the integer variants.
 */

#include <string.h>
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

/* GL 3.3 allows GL_ZERO and GL_ONE as swizzle sources. Neither reached Metal,
   so a swizzle of GL_ONE handed back the original component instead of 1.0. */
static void set_swizzle(void)
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ZERO);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
}

static void swizzle_case(int after_first_use);

/* Metal fixes a texture's swizzle when the texture is made, so a swizzle set
   after it has already been sampled needs a new view of the same storage. */
GPU_TEST(tex_sampler_params, swizzle_set_after_first_use_takes_effect)
{
    swizzle_case(1);
}

GPU_TEST(tex_sampler_params, swizzle_accepts_zero_and_one)
{
    swizzle_case(0);
}

static void swizzle_case(int after_first_use)
{
    static const char *VS =
        "#version 460 core\n"
        "void main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));"
        "gl_Position=vec4(p[gl_VertexID],0,1);}\n";
    static const char *FS =
        "#version 460 core\n"
        "uniform sampler2D s;out vec4 o;void main(){o=texelFetch(s,ivec2(0),0);}\n";

    GLubyte px[4] = {0x10, 0x20, 0x30, 0x40};
    GLubyte out[4 * 4 * 4];
    GLuint tex = 0, rt = 0, fb = 0, vao = 0;
    GLint ok = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    if (!after_first_use)
        set_swizzle();

    glGenTextures(1, &rt);
    glBindTexture(GL_TEXTURE_2D, rt);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt, 0);

    GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint p = glCreateProgram();

    glShaderSource(v, 1, &VS, NULL); glCompileShader(v);
    glShaderSource(f, 1, &FS, NULL); glCompileShader(f);
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(GL_TRUE, ok);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(p);
    glUniform1i(glGetUniformLocation(p, "s"), 0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glViewport(0, 0, 4, 4);
    mgl_drain_errors();

    if (after_first_use)
    {
        glDrawArrays(GL_TRIANGLES, 0, 3);   /* sample once with the default */

        /* The flush is load-bearing, and that is a separate bug: a texture
           state change between two draws inside one render pass is not seen,
           because nothing re-binds the texture on the open encoder. */
        glFinish();

        set_swizzle();
    }

    glDrawArrays(GL_TRIANGLES, 0, 3);

    memset(out, 0xAB, sizeof out);
    glReadPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, out);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    CHECK_EQ_INT(0x30, out[0]);   /* R <- blue  */
    CHECK_EQ_INT(0x00, out[1]);   /* G <- zero  */
    CHECK_EQ_INT(0x10, out[2]);   /* B <- red   */
    CHECK_EQ_INT(0xFF, out[3]);   /* A <- one   */

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &rt);
    glDeleteTextures(1, &tex);
}
