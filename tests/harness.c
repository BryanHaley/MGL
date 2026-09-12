/*
 * harness.c
 * MGL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "MGLContext.h"

extern void *CppCreateMGLRendererHeadless(void *glm_ctx);

static GLMContext g_ctx = NULL;
static int g_ready = 0;

int mgl_harness_init(void)
{
    if (g_ready)
        return 1;

    // tests trigger errors on purpose; set MGL_LOG_LEVEL to see them
    if (!getenv("MGL_LOG_LEVEL"))
        setenv("MGL_LOG_LEVEL", "0", 1);

    g_ctx = createGLMContext(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                             GL_DEPTH_COMPONENT, GL_FLOAT,
                             GL_STENCIL_INDEX8, GL_UNSIGNED_BYTE);
    if (!g_ctx)
    {
        fprintf(stderr, "harness: createGLMContext failed\n");
        return 0;
    }

    MGLsetCurrentContext(g_ctx);

    if (!CppCreateMGLRendererHeadless(g_ctx))
    {
        fprintf(stderr, "harness: headless renderer failed\n");
        return 0;
    }

    g_ready = 1;

    return 1;
}

void mgl_harness_shutdown(void)
{
    g_ready = 0;
    g_ctx = NULL;
}

void mgl_harness_reset(void)
{
    if (!g_ready) return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);

    glBlendFunc(GL_ONE, GL_ZERO);
    glBlendEquation(GL_FUNC_ADD);
    glBlendColor(0.0f, 0.0f, 0.0f, 0.0f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glClearStencil(0);
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    // A leftover row length silently widens every later readback, so the next
    // test writes past the buffer it sized for itself.
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
    glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);

    // constant attribute values are context state and persist across tests
    {
        GLint n = 0;

        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &n);

        if (n <= 0 || n > 64) n = 16;

        for (GLint i = 0; i < n; i++)
            glVertexAttrib4f((GLuint)i, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    glViewport(0, 0, 1, 1);
    glScissor(0, 0, 0, 0);
    glDepthRange(0.0, 1.0);

    mgl_drain_errors();
}

GLenum mgl_drain_errors(void)
{
    GLenum first = GL_NO_ERROR, e;
    int guard = 0;

    while ((e = glGetError()) != GL_NO_ERROR && guard++ < 64)
        if (first == GL_NO_ERROR) first = e;

    return first;
}

int mgl_target_create(MGLTestTarget *t, GLsizei w, GLsizei h,
                      GLenum color_internalformat, int want_depth_stencil)
{
    GLenum status;

    memset(t, 0, sizeof *t);
    t->width = w;
    t->height = h;

    glGenTextures(1, &t->color);
    glBindTexture(GL_TEXTURE_2D, t->color);
    glTexStorage2D(GL_TEXTURE_2D, 1, color_internalformat, w, h);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &t->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->color, 0);

    if (want_depth_stencil)
    {
        glGenTextures(1, &t->depth_stencil);
        glBindTexture(GL_TEXTURE_2D, t->depth_stencil);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, w, h);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D, t->depth_stencil, 0);
    }

    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "harness: fbo incomplete 0x%x\n", status);
        mgl_target_destroy(t);
        return 0;
    }

    glViewport(0, 0, w, h);

    return 1;
}

void mgl_target_destroy(MGLTestTarget *t)
{
    if (t->fbo)           glDeleteFramebuffers(1, &t->fbo);
    if (t->color)         glDeleteTextures(1, &t->color);
    if (t->depth_stencil) glDeleteTextures(1, &t->depth_stencil);

    memset(t, 0, sizeof *t);
}

void mgl_target_bind(const MGLTestTarget *t)
{
    glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
    glViewport(0, 0, t->width, t->height);
}

unsigned char *mgl_read_rgba8(const MGLTestTarget *t)
{
    size_t n = (size_t)t->width * (size_t)t->height * 4;
    unsigned char *px = (unsigned char *)malloc(n);

    if (!px) return NULL;

    memset(px, 0, n);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, t->fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glReadPixels(0, 0, t->width, t->height, GL_RGBA, GL_UNSIGNED_BYTE, px);

    return px;
}

void mgl_pixel_at(const unsigned char *px, const MGLTestTarget *t,
                  int x, int y, unsigned char out[4])
{
    const unsigned char *p = px + ((size_t)y * (size_t)t->width + (size_t)x) * 4;

    out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; out[3] = p[3];
}

static GLuint compile_stage(GLenum stage, const char *src, char *log, int log_size)
{
    GLuint s = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        if (log && log_size > 0) glGetShaderInfoLog(s, log_size, NULL, log);
        glDeleteShader(s);
        return 0;
    }

    return s;
}

static GLuint link_program(GLuint *stages, int n, char *log, int log_size)
{
    GLuint p = glCreateProgram();
    GLint ok = 0;

    for (int i = 0; i < n; i++) glAttachShader(p, stages[i]);

    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    for (int i = 0; i < n; i++) { glDetachShader(p, stages[i]); glDeleteShader(stages[i]); }

    if (!ok)
    {
        if (log && log_size > 0) glGetProgramInfoLog(p, log_size, NULL, log);
        glDeleteProgram(p);
        return 0;
    }

    return p;
}

GLuint mgl_build_program(const char *vs_src, const char *fs_src, char *log, int log_size)
{
    GLuint st[2];

    if (log && log_size) log[0] = 0;

    st[0] = compile_stage(GL_VERTEX_SHADER, vs_src, log, log_size);
    if (!st[0]) return 0;

    st[1] = compile_stage(GL_FRAGMENT_SHADER, fs_src, log, log_size);
    if (!st[1]) { glDeleteShader(st[0]); return 0; }

    return link_program(st, 2, log, log_size);
}

GLuint mgl_build_compute_program(const char *cs_src, char *log, int log_size)
{
    GLuint st[1];

    if (log && log_size) log[0] = 0;

    st[0] = compile_stage(GL_COMPUTE_SHADER, cs_src, log, log_size);
    if (!st[0]) return 0;

    return link_program(st, 1, log, log_size);
}

GLuint mgl_fullscreen_quad(GLuint *out_vbo)
{
    static const float verts[] = {
        -1.0f, -1.0f,   1.0f, -1.0f,   1.0f,  1.0f,
        -1.0f, -1.0f,   1.0f,  1.0f,  -1.0f,  1.0f,
    };
    GLuint vao = 0, vbo = 0;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL);

    if (out_vbo) *out_vbo = vbo;

    return vao;
}
