/*
 * probe_fsr2.c
 * Copyright (C) The MooGL Project
 *
 * Phase 9 finish criterion: can FidelityFX FSR 2.2.1's OpenGL backend
 * initialise and dispatch against MGL? Each check below is something
 * ffx_fsr2_gl.cpp actually does, in the order it does it.
 */
#include <stdio.h>
#include <string.h>
#include <GL/glcorearb.h>

extern const GLubyte *glGetString(GLenum);
extern const GLubyte *glGetStringi(GLenum, GLuint);
extern void glGetIntegerv(GLenum, GLint *);
extern GLenum glGetError(void);
extern void glCreateTextures(GLenum, GLsizei, GLuint *);
extern void glTextureStorage2D(GLuint, GLsizei, GLenum, GLsizei, GLsizei);
extern void glTextureView(GLuint, GLenum, GLuint, GLenum, GLuint, GLuint, GLuint, GLuint);
extern void glBindTextureUnit(GLuint, GLuint);
extern void glCreateBuffers(GLsizei, GLuint *);
extern void glNamedBufferStorage(GLuint, GLsizeiptr, const void *, GLbitfield);
extern void *glMapNamedBufferRange(GLuint, GLintptr, GLsizeiptr, GLbitfield);
extern GLboolean glUnmapNamedBuffer(GLuint);
extern void glCopyImageSubData(GLuint, GLenum, GLint, GLint, GLint, GLint,
                               GLuint, GLenum, GLint, GLint, GLint, GLint,
                               GLsizei, GLsizei, GLsizei);
extern void glClearTexImage(GLuint, GLint, GLenum, GLenum, const void *);
extern GLuint glCreateShader(GLenum);
extern void glShaderBinary(GLsizei, const GLuint *, GLenum, const void *, GLsizei);
extern void glSpecializeShader(GLuint, const GLchar *, GLuint, const GLuint *, const GLuint *);
extern void glGenTextures(GLsizei, GLuint *);
extern void glTextureSubImage2D(GLuint, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
extern void glBindTexture(GLenum, GLuint);
extern void glGetTexImage(GLenum, GLint, GLenum, GLenum, void *);

#ifndef GL_SUBGROUP_SIZE_KHR
#define GL_SUBGROUP_SIZE_KHR              0x9532
#define GL_SUBGROUP_SUPPORTED_STAGES_KHR  0x9533
#endif
#ifndef GL_SHADER_BINARY_FORMAT_SPIR_V
#define GL_SHADER_BINARY_FORMAT_SPIR_V    0x9551
#endif

static int fails;

static void check(const char *what, int ok, const char *detail)
{
    printf("  [%s] %-42s %s\n", ok ? "ok  " : "GATE", what, detail ? detail : "");
    if (!ok) fails++;
}

static int hasExtension(const char *want)
{
    GLint n = 0;

    while (glGetError()) ;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);

    for (GLint i = 0; i < n; i++)
    {
        const char *s = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);

        if (s && !strcmp(s, want))
            return 1;
    }

    return 0;
}

int main(void)
{
    char buf[64];
    GLuint tex = 0, view = 0, dst = 0, buffer = 0, sh = 0;
    GLint v = 0, formats = 0;
    void *mapped;

    printf("FSR 2.2.1 OpenGL backend readiness against %s\n\n", (const char *)glGetString(GL_VERSION));

    /* 1. the hard gate: CreateBackendContextGL returns an error without this */
    check("GL_KHR_shader_subgroup", hasExtension("GL_KHR_shader_subgroup"),
          "hard gate -- backend refuses to initialise");

    while (glGetError()) ;
    glGetIntegerv(GL_SUBGROUP_SIZE_KHR, &v);
    check("GL_SUBGROUP_SIZE_KHR queryable", glGetError() == GL_NO_ERROR, "wave lane count");

    /* 2. shaders arrive as SPIR-V, not GLSL */
    while (glGetError()) ;
    glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &formats);
    snprintf(buf, sizeof buf, "%d binary format(s)", formats);
    check("SPIR-V shader ingestion", formats > 0 && glGetError() == GL_NO_ERROR, buf);

    sh = glCreateShader(GL_COMPUTE_SHADER);
    while (glGetError()) ;
    glSpecializeShader(sh, "main", 0, NULL, NULL);
    snprintf(buf, sizeof buf, "err 0x%x", glGetError());
    check("glSpecializeShader present", 1, buf);

    /* 3. every resource is created with DSA */
    while (glGetError()) ;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA16F, 64, 64);
    check("glCreateTextures + glTextureStorage2D", glGetError() == GL_NO_ERROR, "DSA texture path");

    while (glGetError()) ;
    glBindTextureUnit(0, tex);
    check("glBindTextureUnit", glGetError() == GL_NO_ERROR, NULL);

    while (glGetError()) ;
    glCreateBuffers(1, &buffer);
    glNamedBufferStorage(buffer, 4096, NULL, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
    mapped = glMapNamedBufferRange(buffer, 0, 256, GL_MAP_WRITE_BIT);
    check("glCreateBuffers + storage + map range", mapped != NULL && glGetError() == GL_NO_ERROR,
          mapped ? "constant buffer upload" : "map returned NULL");
    if (mapped) glUnmapNamedBuffer(buffer);

    /* 4. aliasing views over mip levels -- an error code is not the question,
       whether the view actually shares storage is */
    {
        GLuint src;
        unsigned char put[4 * 4 * 4], got[4 * 4 * 4];

        memset(put, 0xA5, sizeof put);
        memset(got, 0, sizeof got);

        glCreateTextures(GL_TEXTURE_2D, 1, &src);
        glTextureStorage2D(src, 1, GL_RGBA8, 4, 4);
        glTextureSubImage2D(src, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, put);

        glGenTextures(1, &view);
        while (glGetError()) ;
        glTextureView(view, GL_TEXTURE_2D, src, GL_RGBA8, 0, 1, 0, 1);

        if (glGetError() == GL_NO_ERROR)
        {
            glBindTexture(GL_TEXTURE_2D, view);
            while (glGetError()) ;
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
        }

        check("glTextureView aliases its parent's storage",
              memcmp(put, got, sizeof put) == 0,
              memcmp(put, got, sizeof put) == 0 ? "wrote through the parent, read through the view"
                                                : "no error raised, but the view sees nothing");
    }

    /* 5. resource copies between passes */
    glCreateTextures(GL_TEXTURE_2D, 1, &dst);
    glTextureStorage2D(dst, 1, GL_RGBA16F, 64, 64);
    while (glGetError()) ;
    glCopyImageSubData(tex, GL_TEXTURE_2D, 0, 0, 0, 0, dst, GL_TEXTURE_2D, 0, 0, 0, 0, 64, 64, 1);
    check("glCopyImageSubData", glGetError() == GL_NO_ERROR, NULL);

    while (glGetError()) ;
    glClearTexImage(tex, 0, GL_RGBA, GL_FLOAT, NULL);
    check("glClearTexImage", glGetError() == GL_NO_ERROR, NULL);

    printf("\n%s: %d gate(s) block FSR 2\n", fails ? "NOT READY" : "READY", fails);

    return fails ? 1 : 0;
}
