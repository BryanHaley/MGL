/*
 * Copyright (C) The MooGL Project
 *
 * test_shared_contexts.c
 * MGL
 *
 * Contexts made to share (GL 4.6 chapter 5): buffers, textures,
 * renderbuffers, samplers, shaders, programs and syncs are common to them;
 * vertex arrays, framebuffers, transform feedback objects, program pipelines
 * and queries are each context's own.
 */

#include "mgl_test.h"
#include "harness.h"
#include "MGLContext.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define W 16
#define H 16

extern void *CppCreateMGLRendererHeadless(void *glm_ctx);

static GLMContext newContext(GLMContext share)
{
    GLMContext was = MGLgetCurrentContext();
    GLMContext c = createGLMContextShared(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, GL_DEPTH_COMPONENT, GL_FLOAT,
                                          GL_STENCIL_INDEX8, GL_UNSIGNED_BYTE, share);

    if (c)
    {
        MGLsetCurrentContext(c);
        CppCreateMGLRendererHeadless(c);
    }

    MGLsetCurrentContext(was);
    return c;
}

static const char *VS =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_GREEN =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = vec4(0, 1, 0, 1); }\n";

static const char *FS_TEX =
    "#version 460 core\n"
    "uniform sampler2D tex;\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = texture(tex, vec2(0.5)); }\n";

// green, after enough work that another queue can overtake it
static const char *FS_SLOW_GREEN =
    "#version 460 core\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() {\n"
    "    float a = gl_FragCoord.x;\n"
    "    for (int i = 0; i < 20000; i++) a = fract(sin(a) * 43758.5453 + float(i));\n"
    "    frag = vec4(0, 1, step(2.0, a), 1);\n"
    "}\n";

static void centre(const MGLTestTarget *t, unsigned char out[4])
{
    unsigned char *px = mgl_read_rgba8(t);

    memset(out, 0, 4);
    if (px)
        mgl_pixel_at(px, t, t->width / 2, t->height / 2, out);
    free(px);
}

// draws a full-target quad with prog into t, in the current context
static void drawQuad(const MGLTestTarget *t, GLuint prog)
{
    GLuint vao, vbo;

    vao = mgl_fullscreen_quad(&vbo);
    mgl_target_bind(t);
    glUseProgram(prog);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

GPU_TEST(shared_contexts, shared_objects_are_common_and_containers_are_not)
{
    GLMContext a = MGLgetCurrentContext(), b = newContext(a), c = newContext(a), lone = newContext(NULL);
    GLuint buf, tex, rbo, smp, prog, shader, vao, fbo;
    GLsync sync;

    CHECK(b && c && lone);
    if (!b || !c || !lone) return;

    glGenBuffers(1, &buf);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glGenSamplers(1, &smp);
    glBindSampler(0, smp);
    prog = glCreateProgram();
    shader = glCreateShader(GL_VERTEX_SHADER);
    sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glObjectLabel(GL_TEXTURE, tex, -1, "shared label");

    MGLsetCurrentContext(b);
    CHECK(glIsBuffer(buf) && glIsTexture(tex) && glIsRenderbuffer(rbo) && glIsSampler(smp));
    CHECK(glIsProgram(prog) && glIsShader(shader) && glIsSync(sync));

    char label[32] = { 0 };
    glGetObjectLabel(GL_TEXTURE, tex, sizeof label, NULL, label);
    CHECK_MSG(!strcmp(label, "shared label"), "label in the other context: '%s'", label);

    // a name made in one context is new in the other: shaders and programs
    // share one namespace, and both contexts draw from it
    GLuint more = glCreateProgram();
    CHECK(more != prog && more != shader);
    glDeleteProgram(more);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // another new context in the group has neither
    MGLsetCurrentContext(c);
    CHECK(!glIsVertexArray(vao) && !glIsFramebuffer(fbo));

    // a context that was not made to share sees none of it
    MGLsetCurrentContext(lone);
    CHECK(!glIsTexture(tex) && !glIsBuffer(buf) && !glIsProgram(prog) && !glIsSync(sync));

    MGLsetCurrentContext(b);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fbo);

    MGLsetCurrentContext(a);
    glDeleteBuffers(1, &buf);
    glDeleteTextures(1, &tex);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteSamplers(1, &smp);
    glDeleteProgram(prog);
    glDeleteShader(shader);
    glDeleteSync(sync);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    destroyGLMContext(b);
    destroyGLMContext(c);
    destroyGLMContext(lone);
    MGLsetCurrentContext(a);
}

// A texture filled in one context and a program linked there draw in the other
GPU_TEST(shared_contexts, a_texture_and_program_from_one_context_draw_in_another)
{
    static const unsigned char red[16] = { 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255 };
    GLMContext a = MGLgetCurrentContext(), b = newContext(a);
    char log[1024];
    unsigned char c[4];
    GLuint tex, prog;
    MGLTestTarget t;

    CHECK(b);
    if (!b) return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    prog = mgl_build_program(VS, FS_TEX, log, sizeof log);
    CHECK(prog);
    glFinish();

    MGLsetCurrentContext(b);
    mgl_target_create(&t, W, H, GL_RGBA8, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    drawQuad(&t, prog);
    centre(&t, c);
    CHECK_MSG(c[0] > 250 && c[1] < 5, "other context drew %d,%d,%d, want red", c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    mgl_target_destroy(&t);

    MGLsetCurrentContext(a);
    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
    destroyGLMContext(b);
    MGLsetCurrentContext(a);
}

// One context renders into a shared texture; another waits on its fence and
// samples what it drew
GPU_TEST(shared_contexts, a_fence_orders_rendering_between_contexts)
{
    GLMContext a = MGLgetCurrentContext(), b = newContext(a);
    char log[1024];
    unsigned char c[4];
    GLuint green, sampler;
    MGLTestTarget src, dst;

    CHECK(b);
    if (!b) return;

    green = mgl_build_program(VS, FS_SLOW_GREEN, log, sizeof log);
    sampler = mgl_build_program(VS, FS_TEX, log, sizeof log);
    mgl_target_create(&src, 256, 256, GL_RGBA8, 0);
    glFinish();
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(&src, green);

    GLsync done = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();

    MGLsetCurrentContext(b);
    glWaitSync(done, 0, GL_TIMEOUT_IGNORED);
    mgl_target_create(&dst, W, H, GL_RGBA8, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src.color);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    drawQuad(&dst, sampler);
    centre(&dst, c);
    CHECK_MSG(c[1] > 250 && c[2] < 5, "sampled %d,%d,%d, want what the other context drew", c[0], c[1], c[2]);

    GLenum r = glClientWaitSync(done, 0, 1000000000ull);
    CHECK(r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED);
    GLint status = 0;
    glGetSynciv(done, GL_SYNC_STATUS, 1, NULL, &status);
    CHECK_EQ_INT(status, GL_SIGNALED);
    glDeleteSync(done);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    mgl_target_destroy(&dst);

    MGLsetCurrentContext(a);
    mgl_target_destroy(&src);
    glDeleteProgram(green);
    glDeleteProgram(sampler);
    destroyGLMContext(b);
    MGLsetCurrentContext(a);
}

// A fence that has not been reached times out instead of reporting done
GPU_TEST(shared_contexts, a_client_wait_on_a_fence_not_yet_reached_times_out)
{
    GLMContext a = MGLgetCurrentContext(), b = newContext(a);

    CHECK(b);
    if (!b) return;

    // made in b, whose work has not been sent: a can't see it go off
    MGLsetCurrentContext(b);
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    MGLsetCurrentContext(a);
    CHECK_EQ_UINT(glClientWaitSync(s, 0, 0), GL_TIMEOUT_EXPIRED);

    MGLsetCurrentContext(b);
    glFlush();
    MGLsetCurrentContext(a);
    GLenum r = glClientWaitSync(s, 0, 5000000000ull);
    CHECK(r == GL_CONDITION_SATISFIED || r == GL_ALREADY_SIGNALED);
    glDeleteSync(s);

    destroyGLMContext(b);
    MGLsetCurrentContext(a);
}

// Deleting objects another context is drawing with leaves them working
// there; only the names go
GPU_TEST(shared_contexts, deleting_in_one_context_keeps_what_another_uses)
{
    static const unsigned char red[16] = { 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255 };
    GLMContext a = MGLgetCurrentContext(), b = newContext(a);
    char log[1024];
    unsigned char c[4];
    GLuint tex, smp, prog, vao, vbo;
    MGLTestTarget t;

    CHECK(b);
    if (!b) return;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red);
    glGenSamplers(1, &smp);
    glSamplerParameteri(smp, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    prog = mgl_build_program(VS, FS_TEX, log, sizeof log);
    glFinish();

    MGLsetCurrentContext(b);
    mgl_target_create(&t, W, H, GL_RGBA8, 0);
    vao = mgl_fullscreen_quad(&vbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindSampler(0, smp);
    glUseProgram(prog);

    MGLsetCurrentContext(a);
    glDeleteTextures(1, &tex);
    glDeleteSamplers(1, &smp);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    CHECK(!glIsTexture(tex) && !glIsSampler(smp) && !glIsBuffer(vbo));

    MGLsetCurrentContext(b);
    CHECK(!glIsTexture(tex));
    mgl_target_bind(&t);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    centre(&t, c);
    CHECK_MSG(c[0] > 250 && c[1] < 5, "drew %d,%d,%d with deleted objects, want red", c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // letting go of them frees them
    glUseProgram(0);
    glBindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteVertexArrays(1, &vao);
    mgl_target_destroy(&t);

    destroyGLMContext(b);
    MGLsetCurrentContext(a);
}

// Shared objects outlive the context that made them
GPU_TEST(shared_contexts, destroying_a_context_keeps_the_shared_objects)
{
    static const unsigned char red[16] = { 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255 };
    GLMContext a = MGLgetCurrentContext(), b = newContext(a);
    char log[1024];
    unsigned char c[4];
    GLuint tex, prog;
    MGLTestTarget t;

    CHECK(b);
    if (!b) return;

    MGLsetCurrentContext(b);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, red);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    prog = mgl_build_program(VS, FS_TEX, log, sizeof log);
    glUseProgram(prog);
    glFinish();
    destroyGLMContext(b);

    MGLsetCurrentContext(a);
    CHECK(glIsTexture(tex) && glIsProgram(prog));
    mgl_target_create(&t, W, H, GL_RGBA8, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    drawQuad(&t, prog);
    centre(&t, c);
    CHECK_MSG(c[0] > 250, "drew %d,%d,%d, want red", c[0], c[1], c[2]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTextures(1, &tex);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

/* ---------- two threads ---------- */

#define LOADS 40

typedef struct {
    GLMContext ctx;
    GLuint buffers[LOADS], textures[LOADS], programs[4];
    int current_ok;
} Loader;

// what a loading thread does: make objects, fill them, compile programs
static void *loader(void *arg)
{
    Loader *l = (Loader *)arg;
    unsigned char px[64];
    char log[1024];

    memset(px, 0x80, sizeof px);
    MGLsetCurrentContext(l->ctx);
    l->current_ok = MGLgetCurrentContext() == l->ctx;

    for (int i = 0; i < LOADS; i++)
    {
        GLuint spare;

        glGenBuffers(1, &l->buffers[i]);
        glBindBuffer(GL_ARRAY_BUFFER, l->buffers[i]);
        glBufferData(GL_ARRAY_BUFFER, 4096, px, GL_STATIC_DRAW);

        glGenTextures(1, &l->textures[i]);
        glBindTexture(GL_TEXTURE_2D, l->textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);

        // churn names, so both threads recycle them at once
        glGenBuffers(1, &spare);
        glDeleteBuffers(1, &spare);

        if (i % 10 == 0)
            l->programs[i / 10] = mgl_build_program(VS, FS_GREEN, log, sizeof log);
    }

    glFinish();
    MGLsetCurrentContext(NULL);
    return NULL;
}

GPU_TEST(shared_contexts, contexts_on_two_threads_load_at_once)
{
    GLMContext a = MGLgetCurrentContext();
    Loader l[2];
    pthread_t th[2];

    memset(l, 0, sizeof l);
    l[0].ctx = newContext(a);
    l[1].ctx = newContext(a);
    CHECK(l[0].ctx && l[1].ctx);
    if (!l[0].ctx || !l[1].ctx) return;

    for (int i = 0; i < 2; i++)
        pthread_create(&th[i], NULL, loader, &l[i]);
    for (int i = 0; i < 2; i++)
        pthread_join(th[i], NULL);

    // each thread had its own current context, and this one kept its own
    CHECK(l[0].current_ok && l[1].current_ok);
    CHECK(MGLgetCurrentContext() == a);

    // every name is distinct, and all of it is here
    int clash = 0, missing = 0;

    for (int i = 0; i < LOADS; i++)
        for (int j = 0; j < LOADS; j++)
        {
            clash += l[0].buffers[i] == l[1].buffers[j];
            clash += l[0].textures[i] == l[1].textures[j];
        }

    for (int t = 0; t < 2; t++)
    {
        for (int i = 0; i < LOADS; i++)
            missing += !glIsBuffer(l[t].buffers[i]) + !glIsTexture(l[t].textures[i]);
        for (int i = 0; i < 4; i++)
            missing += !glIsProgram(l[t].programs[i]);
    }

    CHECK_EQ_INT(clash, 0);
    CHECK_EQ_INT(missing, 0);

    // and what one loader made draws here
    char log[1024];
    unsigned char c[4];
    MGLTestTarget t;

    (void)log;
    mgl_target_create(&t, W, H, GL_RGBA8, 0);
    drawQuad(&t, l[1].programs[3]);
    centre(&t, c);
    CHECK_MSG(c[1] > 250, "drew %d,%d,%d, want green", c[0], c[1], c[2]);
    mgl_target_destroy(&t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int k = 0; k < 2; k++)
    {
        glDeleteBuffers(LOADS, l[k].buffers);
        glDeleteTextures(LOADS, l[k].textures);
        for (int i = 0; i < 4; i++)
            glDeleteProgram(l[k].programs[i]);
        destroyGLMContext(l[k].ctx);
    }

    MGLsetCurrentContext(a);
}
