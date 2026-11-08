/*
 * test_fsr2.c
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
 * FSR 2's API on MetalFX (libffx_fsr2_api): AMD's helper values and
 * struct layout, a jittered moving scene upscaled 2x, sharpening, and the
 * reactive mask.
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#include "mgl_test.h"
#include "harness.h"

#include "ffx_fsr2.h"

// gl/ffx_fsr2_gl.h is C++ only
size_t ffxFsr2GetScratchMemorySizeGL(void);
FfxErrorCode ffxFsr2GetInterfaceGL(FfxFsr2Interface *outInterface, void *scratchBuffer,
                                   size_t scratchBufferSize, void *getProcAddress);
FfxResource ffxGetTextureResourceGL(GLuint textureGL, uint32_t width, uint32_t height,
                                    GLenum imgFormat, const wchar_t *name);

TEST(fsr2, helpers_and_layout_match_amd)
{
    uint32_t w = 0, h = 0;
    float x = 0, y = 0;

    CHECK(ffxFsr2GetUpscaleRatioFromQualityMode(FFX_FSR2_QUALITY_MODE_QUALITY) == 1.5f);
    CHECK(ffxFsr2GetUpscaleRatioFromQualityMode(FFX_FSR2_QUALITY_MODE_BALANCED) == 1.7f);
    CHECK(ffxFsr2GetUpscaleRatioFromQualityMode(FFX_FSR2_QUALITY_MODE_PERFORMANCE) == 2.0f);
    CHECK(ffxFsr2GetUpscaleRatioFromQualityMode(FFX_FSR2_QUALITY_MODE_ULTRA_PERFORMANCE) == 3.0f);

    CHECK_EQ_UINT(ffxFsr2GetRenderResolutionFromQualityMode(&w, &h, 1920, 1080, FFX_FSR2_QUALITY_MODE_QUALITY), FFX_OK);
    CHECK_EQ_UINT(w, 1280);
    CHECK_EQ_UINT(h, 720);
    CHECK_EQ_UINT(ffxFsr2GetRenderResolutionFromQualityMode(&w, &h, 1920, 1080, 0), FFX_ERROR_INVALID_ENUM);

    CHECK_EQ_INT(ffxFsr2GetJitterPhaseCount(1280, 1920), 18);

    // Halton 2,3 from index 1, centred
    CHECK_EQ_UINT(ffxFsr2GetJitterOffset(&x, &y, 0, 18), FFX_OK);
    CHECK(x == 0.0f && fabsf(y - (1.0f / 3.0f - 0.5f)) < 1e-6f);
    CHECK_EQ_UINT(ffxFsr2GetJitterOffset(&x, &y, 19, 18), FFX_OK);
    CHECK(x == -0.25f && fabsf(y - (2.0f / 3.0f - 0.5f)) < 1e-6f);
    CHECK_EQ_UINT(ffxFsr2GetJitterOffset(&x, &y, 0, 0), FFX_ERROR_INVALID_ARGUMENT);

    // macOS's own layout: wchar_t is 4 bytes here, 2 on Windows
    CHECK_EQ_INT(sizeof(FfxResource), 312);

    FfxResource r = ffxGetTextureResourceGL(7, 32, 16, GL_DEPTH_COMPONENT32F, NULL);
    CHECK(r.resource == (void *)7 && r.isDepth && r.description.width == 32);
    CHECK(!ffxFsr2ResourceIsNull(r));
}

/* ---------- a jittered scene upscaled 64 -> 128 ---------- */

enum { RW = 64, DW = 128, FRAMES = 48 };

static const float kMove[2] = { 0.37f, 0.21f };    // render pixels per frame
static float gPeriod;                              // stripe width, display pixels

static const char *kVs =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

// Each pixel centre sees the scene point it would with the camera offset by
// the jitter and the scene moved on by t frames
static const char *kFs =
    "#version 460 core\n"
    "layout(location = 0) out vec4 color;\n"
    "layout(location = 1) out vec2 velocity;\n"
    "uniform vec2 jitter;\n"
    "uniform vec2 shift;\n"
    "uniform vec2 move;\n"
    "uniform float period;\n"
    "float scene(vec2 u) {\n"
    "    if (length(u - 0.5) > 0.3) return 0.2;\n"
    "    return fract(u.x * 128.0 / period) < 0.5 ? 1.0 : 0.0;\n"
    "}\n"
    "void main() {\n"
    "    vec2 u = (gl_FragCoord.xy - jitter - shift) / 64.0;\n"
    "    color = vec4(vec3(scene(u)), 1.0);\n"
    "    velocity = move / 64.0;\n"
    "    gl_FragDepth = 0.5;\n"
    "}\n";

static float sceneAt(float ux, float uy)
{
    float dx = ux - 0.5f, dy = uy - 0.5f;

    if (sqrtf(dx * dx + dy * dy) > 0.3f) return 0.2f;

    float s = ux * 128.0f / gPeriod;
    return s - floorf(s) < 0.5f ? 1.0f : 0.0f;
}

typedef struct {
    GLuint color, velocity, depth, output, fbo, prog, vao, vbo;
    FfxFsr2Context fsr;
    void *scratch;
} Scene;

static GLuint tex2D(GLenum format, int w, int h)
{
    GLuint t = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &t);
    glTextureStorage2D(t, 1, format, w, h);
    return t;
}

static int sceneInit(Scene *s)
{
    char log[1024];

    memset(s, 0, sizeof *s);
    s->prog = mgl_build_program(kVs, kFs, log, sizeof log);

    if (!s->prog) {
        CHECK_MSG(0, "scene program: %s", log);
        return 0;
    }

    s->vao = mgl_fullscreen_quad(&s->vbo);
    s->color = tex2D(GL_RGBA16F, RW, RW);
    s->velocity = tex2D(GL_RG16F, RW, RW);
    s->depth = tex2D(GL_DEPTH_COMPONENT32F, RW, RW);
    s->output = tex2D(GL_RGBA16F, DW, DW);

    glCreateFramebuffers(1, &s->fbo);
    glNamedFramebufferTexture(s->fbo, GL_COLOR_ATTACHMENT0, s->color, 0);
    glNamedFramebufferTexture(s->fbo, GL_COLOR_ATTACHMENT1, s->velocity, 0);
    glNamedFramebufferTexture(s->fbo, GL_DEPTH_ATTACHMENT, s->depth, 0);
    const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glNamedFramebufferDrawBuffers(s->fbo, 2, bufs);

    FfxFsr2ContextDescription desc;
    memset(&desc, 0, sizeof desc);
    desc.flags = FFX_FSR2_ALLOW_NULL_DEVICE_AND_COMMAND_LIST;
    desc.maxRenderSize.width = desc.maxRenderSize.height = RW;
    desc.displaySize.width = desc.displaySize.height = DW;

    s->scratch = calloc(1, ffxFsr2GetScratchMemorySizeGL());
    CHECK_EQ_UINT(ffxFsr2GetInterfaceGL(&desc.callbacks, s->scratch, ffxFsr2GetScratchMemorySizeGL(), NULL), FFX_OK);
    CHECK_EQ_UINT(ffxFsr2ContextCreate(&s->fsr, &desc), FFX_OK);

    return mgl_drain_errors() == GL_NO_ERROR;
}

static void sceneFree(Scene *s)
{
    ffxFsr2ContextDestroy(&s->fsr);
    free(s->scratch);
    glDeleteFramebuffers(1, &s->fbo);
    GLuint t[4] = { s->color, s->velocity, s->depth, s->output };
    glDeleteTextures(4, t);
    glDeleteBuffers(1, &s->vbo);
    glDeleteVertexArrays(1, &s->vao);
    glDeleteProgram(s->prog);
}

// Mean error against the box-filtered scene; jsign and msign flip what is
// passed to FSR 2, to show the right sign is the one that resolves it
static float upscaleError(Scene *s, float period, bool moving, float jsign, float msign, bool sharpen)
{
    const int phases = ffxFsr2GetJitterPhaseCount(RW, DW);
    float *out = malloc(DW * DW * 4 * sizeof(float));
    double err = 0.0;
    int n = 0;

    glUseProgram(s->prog);
    glBindVertexArray(s->vao);
    glBindFramebuffer(GL_FRAMEBUFFER, s->fbo);
    glViewport(0, 0, RW, RW);
    const float mx = moving ? kMove[0] : 0.0f, my = moving ? kMove[1] : 0.0f;

    gPeriod = period;
    glUniform1f(glGetUniformLocation(s->prog, "period"), period);
    glUniform2f(glGetUniformLocation(s->prog, "move"), mx, my);

    for (int f = 0; f < FRAMES; f++) {
        float jx, jy;
        ffxFsr2GetJitterOffset(&jx, &jy, f, phases);

        glUniform2f(glGetUniformLocation(s->prog, "jitter"), jx, jy);
        glUniform2f(glGetUniformLocation(s->prog, "shift"), mx * f, my * f);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        FfxFsr2DispatchDescription d;
        memset(&d, 0, sizeof d);
        d.color = ffxGetTextureResourceGL(s->color, RW, RW, GL_RGBA16F, NULL);
        d.depth = ffxGetTextureResourceGL(s->depth, RW, RW, GL_DEPTH_COMPONENT32F, NULL);
        d.motionVectors = ffxGetTextureResourceGL(s->velocity, RW, RW, GL_RG16F, NULL);
        d.output = ffxGetTextureResourceGL(s->output, DW, DW, GL_RGBA16F, NULL);
        d.jitterOffset.x = jsign * jx;
        d.jitterOffset.y = jsign * jy;
        d.motionVectorScale.x = -msign * RW;
        d.motionVectorScale.y = -msign * RW;
        d.renderSize.width = d.renderSize.height = RW;
        d.enableSharpening = sharpen;
        d.sharpness = 0.5f;
        d.frameTimeDelta = 16.6f;
        d.preExposure = 1.0f;
        d.reset = f == 0;
        d.cameraNear = 0.1f;
        d.cameraFar = 100.0f;
        d.cameraFovAngleVertical = 1.0f;
        d.viewSpaceToMetersFactor = 1.0f;

        CHECK_EQ_UINT(ffxFsr2ContextDispatch(&s->fsr, &d), FFX_OK);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, s->output);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, out);

    const float sx = mx * (FRAMES - 1) / RW, sy = my * (FRAMES - 1) / RW;

    // away from the edges, which have no history to resolve from
    for (int y = 8; y < DW - 8; y++)
        for (int x = 8; x < DW - 8; x++) {
            float ref = 0.0f;

            for (int j = 0; j < 8; j++)
                for (int i = 0; i < 8; i++)
                    ref += sceneAt((x + (i + 0.5f) / 8.0f) / DW - sx, (y + (j + 0.5f) / 8.0f) / DW - sy);

            float v = out[(y * DW + x) * 4];
            err += isfinite(v) ? fabsf(v - ref / 64.0f) : 1.0f;
            n++;
        }

    free(out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    return (float)(err / n);
}

GPU_TEST(fsr2, upscales_a_jittered_scene)
{
    Scene s;

    if (!sceneInit(&s)) {
        sceneFree(&s);
        return;
    }

    // stripes finer than a render pixel only resolve with the jitter right
    float still = upscaleError(&s, 3.0f, false, 1.0f, 1.0f, false);
    float flippedJitter = upscaleError(&s, 3.0f, false, -1.0f, 1.0f, false);
    float sharpened = upscaleError(&s, 3.0f, false, 1.0f, 1.0f, true);

    // a moving scene stays sharp only with the motion vectors right
    float moving = upscaleError(&s, 8.0f, true, 1.0f, 1.0f, false);
    float flippedMotion = upscaleError(&s, 8.0f, true, 1.0f, -1.0f, false);

    printf("    fsr2 error: still %.4f, jitter flipped %.4f, sharpened %.4f, moving %.4f, motion flipped %.4f\n",
           still, flippedJitter, sharpened, moving, flippedMotion);

    CHECK_MSG(still < 0.1f, "still scene error %.4f", still);
    CHECK_MSG(still * 1.5f < flippedJitter, "jitter sign: right %.4f, flipped %.4f", still, flippedJitter);
    CHECK_MSG(sharpened < 0.1f, "sharpened error %.4f", sharpened);
    CHECK_MSG(moving < 0.07f, "moving scene error %.4f", moving);
    CHECK_MSG(moving * 1.5f < flippedMotion, "motion sign: right %.4f, flipped %.4f", moving, flippedMotion);

    sceneFree(&s);
}

GPU_TEST(fsr2, keeps_hdr_values)
{
    Scene s;

    if (!sceneInit(&s)) {
        sceneFree(&s);
        return;
    }

    ffxFsr2ContextDestroy(&s.fsr);

    FfxFsr2ContextDescription desc;
    memset(&desc, 0, sizeof desc);
    desc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_AUTO_EXPOSURE;
    desc.maxRenderSize.width = desc.maxRenderSize.height = RW;
    desc.displaySize.width = desc.displaySize.height = DW;
    ffxFsr2GetInterfaceGL(&desc.callbacks, s.scratch, ffxFsr2GetScratchMemorySizeGL(), NULL);
    CHECK_EQ_UINT(ffxFsr2ContextCreate(&s.fsr, &desc), FFX_OK);

    const float bright[4] = { 20.0f, 5.0f, 0.5f, 1.0f }, zero[4] = { 0, 0, 0, 0 }, half = 0.5f;
    float *out = malloc(DW * DW * 4 * sizeof(float));

    glClearTexImage(s.color, 0, GL_RGBA, GL_FLOAT, bright);
    glClearTexImage(s.velocity, 0, GL_RG, GL_FLOAT, zero);
    glClearTexImage(s.depth, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &half);

    for (int f = 0; f < 8; f++) {
        FfxFsr2DispatchDescription d;
        memset(&d, 0, sizeof d);
        d.color = ffxGetTextureResourceGL(s.color, RW, RW, GL_RGBA16F, NULL);
        d.depth = ffxGetTextureResourceGL(s.depth, RW, RW, GL_DEPTH_COMPONENT32F, NULL);
        d.motionVectors = ffxGetTextureResourceGL(s.velocity, RW, RW, GL_RG16F, NULL);
        d.output = ffxGetTextureResourceGL(s.output, DW, DW, GL_RGBA16F, NULL);
        ffxFsr2GetJitterOffset(&d.jitterOffset.x, &d.jitterOffset.y, f, 32);
        d.motionVectorScale.x = d.motionVectorScale.y = -RW;
        d.renderSize.width = d.renderSize.height = RW;
        d.enableSharpening = f == 7;
        d.sharpness = 0.5f;
        d.preExposure = 1.0f;
        d.reset = f == 0;
        CHECK_EQ_UINT(ffxFsr2ContextDispatch(&s.fsr, &d), FFX_OK);
    }

    glBindTexture(GL_TEXTURE_2D, s.output);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, out);

    const float *c = out + (DW / 2 * DW + DW / 2) * 4;
    printf("    fsr2 hdr: %f %f %f\n", c[0], c[1], c[2]);
    CHECK_MSG(fabsf(c[0] - 20.0f) < 0.5f && fabsf(c[1] - 5.0f) < 0.2f && fabsf(c[2] - 0.5f) < 0.05f,
              "HDR colour came back as %f %f %f", c[0], c[1], c[2]);

    free(out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    sceneFree(&s);
}

GPU_TEST(fsr2, generates_a_reactive_mask)
{
    Scene s;

    if (!sceneInit(&s)) {
        sceneFree(&s);
        return;
    }

    GLuint opaque = tex2D(GL_RGBA16F, RW, RW), mask = tex2D(GL_R16F, RW, RW);
    const float black[4] = { 0, 0, 0, 1 }, grey[4] = { 0.25f, 0.5f, 0.125f, 1 };
    float back[RW * RW];

    glClearTexImage(opaque, 0, GL_RGBA, GL_FLOAT, black);
    glClearTexImage(s.color, 0, GL_RGBA, GL_FLOAT, grey);

    FfxFsr2GenerateReactiveDescription g;
    memset(&g, 0, sizeof g);
    g.commandList = (void *)1;
    g.colorOpaqueOnly = ffxGetTextureResourceGL(opaque, RW, RW, GL_RGBA16F, NULL);
    g.colorPreUpscale = ffxGetTextureResourceGL(s.color, RW, RW, GL_RGBA16F, NULL);
    g.outReactive = ffxGetTextureResourceGL(mask, RW, RW, GL_R16F, NULL);
    g.renderSize.width = g.renderSize.height = RW;
    g.scale = 1.5f;
    g.flags = 8;    // largest channel

    CHECK_EQ_UINT(ffxFsr2ContextGenerateReactiveMask(&s.fsr, &g), FFX_OK);

    glBindTexture(GL_TEXTURE_2D, mask);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, back);
    CHECK_MSG(fabsf(back[0] - 0.75f) < 0.01f && fabsf(back[RW * RW - 1] - 0.75f) < 0.01f,
              "reactive %f %f, wanted 0.75", back[0], back[RW * RW - 1]);

    // the threshold makes it binary
    g.flags = 8 | 4;
    g.cutoffThreshold = 0.8f;
    g.binaryValue = 0.9f;
    CHECK_EQ_UINT(ffxFsr2ContextGenerateReactiveMask(&s.fsr, &g), FFX_OK);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, back);
    CHECK_MSG(back[0] == 0.0f, "below the cutoff: %f", back[0]);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    GLuint t[2] = { opaque, mask };
    glDeleteTextures(2, t);
    sceneFree(&s);
}
