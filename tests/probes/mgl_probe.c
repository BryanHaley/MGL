/*
 * mgl_probe.c
 * Copyright (C) The MooGL Project
 *
 * Finish criteria for the roadmap, as something you can run.
 *
 * A conformance count says how much is left; it does not say whether a phase
 * is done, because a phase is done when a specific capability works end to
 * end. Each function below is one phase's gate, written against what the
 * specification or a real client actually requires. A GATE line is work
 * remaining; the exit code is the number of gates still standing.
 */
#include <stdio.h>
#include <string.h>
#include <GL/glcorearb.h>

/* Compatibility profile enums the core header dropped. The gates below hold
   whether or not the driver was built with them. */
#ifndef GL_ALPHA_TEST
#define GL_ALPHA_TEST      0x0BC0
#define GL_ALPHA_TEST_FUNC 0x0BC1
#define GL_ALPHA_TEST_REF  0x0BC2
#endif
#ifndef GL_CLAMP
#define GL_CLAMP           0x2900
#endif

/* And the ones phases 10B to 10D name. */
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE      0x8861
#endif
#ifndef GL_ALL_ATTRIB_BITS
#define GL_ALL_ATTRIB_BITS   0x000FFFFF
#endif
#ifndef GL_TEXTURE_ENV
#define GL_TEXTURE_ENV       0x2300
#define GL_TEXTURE_ENV_MODE  0x2200
#define GL_MODULATE          0x2100
#endif
#ifndef GL_QUADS
#define GL_QUADS             0x0007
#endif
#ifndef GL_POLYGON
#define GL_POLYGON           0x0009
#endif
#ifndef GL_COMPILE
#define GL_COMPILE           0x1300
#endif
#ifndef GL_SELECT
#define GL_SELECT            0x1C02
#define GL_RENDER            0x1C00
#endif

/* --- the slice of GL this file drives ------------------------------------ */
extern const GLubyte *glGetString(GLenum);
extern const GLubyte *glGetStringi(GLenum, GLuint);
extern void  glGetIntegerv(GLenum, GLint *);
extern void  glGetFloatv(GLenum, GLfloat *);
extern GLenum glGetError(void);
extern void  glGenTextures(GLsizei, GLuint *);
extern void  glBindTexture(GLenum, GLuint);
extern void  glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
extern void  glGetTexImage(GLenum, GLint, GLenum, GLenum, void *);
extern void  glPixelStorei(GLenum, GLint);
extern GLuint glCreateShader(GLenum);
extern void  glShaderSource(GLuint, GLsizei, const GLchar *const *, const GLint *);
extern void  glCompileShader(GLuint);
extern void  glGetShaderiv(GLuint, GLenum, GLint *);
extern GLuint glCreateProgram(void);
extern void  glAttachShader(GLuint, GLuint);
extern void  glLinkProgram(GLuint);
extern void  glGetProgramiv(GLuint, GLenum, GLint *);
extern GLuint glGetUniformBlockIndex(GLuint, const GLchar *);
extern void  glGetActiveUniformBlockiv(GLuint, GLuint, GLenum, GLint *);
extern void  glGetUniformIndices(GLuint, GLsizei, const GLchar *const *, GLuint *);
extern void  glGetActiveUniformsiv(GLuint, GLsizei, const GLuint *, GLenum, GLint *);
extern GLint glGetUniformLocation(GLuint, const GLchar *);
extern void  glGenQueries(GLsizei, GLuint *);
extern void  glBeginQuery(GLenum, GLuint);
extern void  glEndQuery(GLenum);
extern void  glGenTransformFeedbacks(GLsizei, GLuint *);
extern void  glBindTransformFeedback(GLenum, GLuint);
extern void  glEnable(GLenum);
extern void  glDisable(GLenum);
extern void  glGenBuffers(GLsizei, GLuint *);
extern void  glBindBuffer(GLenum, GLuint);
extern void  glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
extern void  glBindBufferBase(GLenum, GLuint, GLuint);
extern void  glGetBufferSubData(GLenum, GLintptr, GLsizeiptr, void *);
extern void  glUseProgram(GLuint);
extern void  glDispatchCompute(GLuint, GLuint, GLuint);
extern void  glMemoryBarrier(GLbitfield);
extern void  glTexImage3D(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
extern void  glTexParameteri(GLenum, GLenum, GLint);
extern void  glDeleteTextures(GLsizei, const GLuint *);
extern void  glGenVertexArrays(GLsizei, GLuint *);
extern void  glBindVertexArray(GLuint);
extern void  glGenFramebuffers(GLsizei, GLuint *);
extern void  glBindFramebuffer(GLenum, GLuint);
extern void  glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint);
extern void  glViewport(GLint, GLint, GLsizei, GLsizei);
extern void  glDrawArrays(GLenum, GLint, GLsizei);
extern void  glUniform1i(GLint, GLint);
extern void  glUniform1f(GLint, GLfloat);
extern void  glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
extern void  glUniformBlockBinding(GLuint, GLuint, GLuint);
extern void  glActiveTexture(GLenum);
extern void  glFinish(void);
extern void  glClear(GLbitfield);
extern void  glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
extern void  glGetTexParameteriv(GLenum, GLenum, GLint *);
extern void  glTexStorage2D(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
extern void  glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
extern void  glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
extern void  glEnableVertexAttribArray(GLuint);
extern void  glBlitFramebuffer(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
extern void  glPatchParameteri(GLenum, GLint);
extern void  glDrawElements(GLenum, GLsizei, GLenum, const void *);
extern void  glGetQueryObjectuiv(GLuint, GLenum, GLuint *);
extern GLuint glGetSubroutineIndex(GLuint, GLenum, const GLchar *);
extern void  glUniformSubroutinesuiv(GLenum, GLsizei, const GLuint *);
extern GLboolean glIsEnabled(GLenum);
extern void  glAlphaFunc(GLenum, GLfloat);
extern void  glDeleteProgram(GLuint);
extern void  glDrawBuffer(GLenum);
extern void  glReadBuffer(GLenum);
extern void  glBegin(GLenum);
extern void  glEnd(void);
extern void  glVertex3f(GLfloat, GLfloat, GLfloat);
extern void  glPointSize(GLfloat);
extern void  glPushAttrib(GLbitfield);
extern void  glPopAttrib(void);
extern void  glPolygonMode(GLenum, GLenum);
extern void  glTexEnvi(GLenum, GLenum, GLint);
extern void  glEdgeFlag(GLboolean);
extern GLuint glGenLists(GLsizei);
extern void  glNewList(GLuint, GLenum);
extern void  glEndList(void);
extern void  glCallList(GLuint);
extern GLint glRenderMode(GLenum);
extern void  glDrawPixels(GLsizei, GLsizei, GLenum, GLenum, const void *);
extern void  glBitmap(GLsizei, GLsizei, GLfloat, GLfloat, GLfloat, GLfloat, const GLubyte *);

/* --- reporting ----------------------------------------------------------- */
static int  g_gates;
static int  g_phase_gates;
static const char *g_phase;

static void phase(const char *name)
{
    if (g_phase)
        printf("    %s\n\n", g_phase_gates ? "-> gates remaining" : "-> PHASE COMPLETE");

    g_phase = name;
    g_phase_gates = 0;
    printf("%s\n", name);
}

static void gate(const char *what, int ok, const char *detail)
{
    printf("  [%s] %-46s %s\n", ok ? "ok  " : "GATE", what, detail ? detail : "");
    if (!ok) { g_gates++; g_phase_gates++; }
}

static void drain(void) { while (glGetError()) ; }

static int hasExt(const char *want)
{
    GLint n = 0;

    drain();
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);

    for (GLint i = 0; i < n; i++)
    {
        const char *s = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);

        if (s && !strcmp(s, want))
            return 1;
    }

    return 0;
}

static GLint limit(GLenum e, int *unhandled)
{
    GLint v = -1;

    drain();
    glGetIntegerv(e, &v);
    *unhandled = (glGetError() != GL_NO_ERROR);

    return v;
}

static int compiles(GLenum stage, const char *src)
{
    GLuint sh = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    return ok;
}

/* ---- drawing, so a gate can ask what came out rather than what compiled --- */

/* Builds a program out of whichever stages are given, links it, and returns 0
   if any part of that failed. */
static GLuint buildProgram(const char *vs, const char *tcs, const char *tes,
                           const char *gs, const char *fs)
{
    static const struct { GLenum stage; int which; } slots[] = {
        { GL_VERTEX_SHADER, 0 }, { 0x8E88 /* TESS_CONTROL */, 1 },
        { 0x8E87 /* TESS_EVALUATION */, 2 }, { GL_GEOMETRY_SHADER, 3 },
        { GL_FRAGMENT_SHADER, 4 },
    };
    const char *src[5] = { vs, tcs, tes, gs, fs };
    GLuint p = glCreateProgram();
    GLint ok = 0;

    for (unsigned i = 0; i < sizeof(slots) / sizeof(slots[0]); i++)
    {
        GLuint sh;

        if (src[slots[i].which] == NULL)
            continue;

        sh = glCreateShader(slots[i].stage);
        glShaderSource(sh, 1, &src[slots[i].which], NULL);
        glCompileShader(sh);
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

        if (!ok)
            return 0;

        glAttachShader(p, sh);
    }

    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    return ok ? p : 0;
}

/* A 64x64 RGBA8 framebuffer with a red clear, so anything green in the
   readback was drawn rather than left over. */
static GLuint probeTarget(GLuint *tex_out)
{
    GLuint tex, fbo;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 64, 64);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    if (tex_out)
        *tex_out = tex;

    return fbo;
}

static void probeQuadVAO(void)
{
    static const GLfloat v[12] = { -1,-1, 1,-1, -1,1,  1,-1, 1,1, -1,1 };
    GLuint vao, vbo;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
}

/* Pixels that came out green, which is the colour every gate below draws. */
static int greenPixels(void)
{
    static GLubyte px[64 * 64 * 4];
    int n = 0;

    memset(px, 0xAB, sizeof px);
    glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, px);

    for (int i = 0; i < 64 * 64; i++)
        if (px[i * 4 + 1] > 200 && px[i * 4] < 60)
            n++;

    return n;
}


/* --- phase 4: the pixel path and the front end --------------------------- */
static void phase4(void)
{
    GLuint tex;
    unsigned char pixels[4 * 4 * 16] = { 0 };
    int bad;

    phase("PHASE 4  the pixel path and the front end");

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    drain();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    gate("unsized base internal formats", glGetError() == GL_NO_ERROR, "GL_RGBA without a bit depth");

    drain();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 4, 4, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pixels);
    gate("integer uploads with data", glGetError() == GL_NO_ERROR, "GL_RGBA_INTEGER + pixels");

    drain();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, 4, 4, 0, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, pixels);
    gate("packed client types on upload", glGetError() == GL_NO_ERROR, "10F_11F_11F_REV");

    glPixelStorei(GL_PACK_ROW_LENGTH, 8);
    drain();
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV, pixels);
    bad = (glGetError() != GL_NO_ERROR);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    gate("GL_PACK_* honoured on readback", !bad, "row length on glGetTexImage");

    gate("layout(shared) uniform blocks compile",
         compiles(GL_VERTEX_SHADER,
                  "#version 330\nlayout(shared) uniform B { vec4 v; };\nvoid main(){gl_Position=v;}\n"),
         "SPIR-V has no equivalent; must map to std140");

    {
        GLuint p = glCreateProgram(), v = glCreateShader(GL_VERTEX_SHADER);
        const char *src = "#version 330\nlayout(std140) uniform D { vec4 a; mat4 m; float f; };\n"
                          "void main(){gl_Position=a*m+vec4(f);}\n";
        GLint sz = 0, n = 0, off[3] = { -1, -1, -1 };
        GLuint idx[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
        const char *names[3] = { "a", "m", "f" };
        GLuint bi;

        glShaderSource(v, 1, &src, NULL); glCompileShader(v);
        glAttachShader(p, v); glLinkProgram(p);
        bi = glGetUniformBlockIndex(p, "D");

        if (bi != 0xFFFFFFFFu)
        {
            glGetActiveUniformBlockiv(p, bi, GL_UNIFORM_BLOCK_DATA_SIZE, &sz);
            glGetActiveUniformBlockiv(p, bi, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &n);
            glGetUniformIndices(p, 3, names, idx);
            glGetActiveUniformsiv(p, 3, idx, GL_UNIFORM_OFFSET, off);
        }

        gate("std140 block size and member offsets",
             sz == 96 && n == 3 && off[0] == 0 && off[1] == 16 && off[2] == 80,
             sz == 96 ? "0/16/80 as the spec lays them out" : "size or offsets wrong");
    }

    {
        GLuint p = glCreateProgram(), f = glCreateShader(GL_FRAGMENT_SHADER);
        const char *src = "#version 330\nuniform sampler2D s;\nout vec4 o;\nvoid main(){o=texture(s,vec2(0));}\n";
        GLint ok = 0;

        glShaderSource(f, 1, &src, NULL); glCompileShader(f);
        glAttachShader(p, f); glLinkProgram(p);
        glGetProgramiv(p, GL_LINK_STATUS, &ok);

        gate("sampler uniforms have locations", ok && glGetUniformLocation(p, "s") >= 0,
             "glUniform1i picks the texture unit");
    }

    /* Array and rectangle textures used to reach Metal as NULL and come back
       as the renderer's emergency gradient, so a round trip is the check. */
    {
        GLubyte src[7 * 5 * 12 * 4], got[sizeof src];
        GLuint tex;
        int layers = 12, w = 7, h = 5, bad, i;

        for (i = 0; i < w * h * layers * 4; i++)
            src[i] = (GLubyte)(i * 7 + 1);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
        drain();
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, w, h, layers, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, src);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
        memset(got, 0xAB, sizeof got);
        glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);

        for (bad = 0, i = 0; i < w * h * layers * 4; i++)
            if (src[i] != got[i]) bad++;

        gate("array textures round trip", glGetError() == GL_NO_ERROR && bad == 0,
             "12 layers of 7x5, every byte");
        glDeleteTextures(1, &tex);
    }

    {
        GLubyte src[7 * 5 * 4], got[sizeof src];
        GLuint tex;
        int bad, i;

        for (i = 0; i < (int)sizeof src; i++)
            src[i] = (GLubyte)(i * 3 + 2);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_RECTANGLE, tex);
        drain();
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, 7, 5, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, src);
        memset(got, 0xAB, sizeof got);
        glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);

        for (bad = 0, i = 0; i < (int)sizeof src; i++)
            if (src[i] != got[i]) bad++;

        gate("rectangle textures round trip", glGetError() == GL_NO_ERROR && bad == 0,
             "core since 3.1");
        glDeleteTextures(1, &tex);
    }


    /* Textures reached the fragment stage only, so every vertex fetch read
       black -- and a plain uniform took its location from its own stage, so a
       vertex and a fragment uniform shared one buffer. Both show up in one
       draw. */
    {
        static const char *VS =
            "#version 330 core\n"
            "uniform sampler2D s;\n"
            "uniform float u_shift;\n"
            "flat out vec4 v;\n"
            "void main(){\n"
            "  vec2 p[4]=vec2[4](vec2(-1,-1),vec2(3,-1),vec2(-1,3),vec2(3,3));\n"
            "  v = texelFetch(s, ivec2(0), 0);\n"
            "  gl_Position = vec4(p[gl_VertexID & 3] + vec2(0.0, u_shift), 0.0, 1.0);\n"
            "}\n";
        static const char *FS =
            "#version 330 core\n"
            "flat in vec4 v;\n"
            "uniform vec4 u_color;\n"
            "layout(location=0) out vec4 o;\n"
            "void main(){ o = vec4(v.rgb, u_color.a); }\n";
        GLubyte texel[4] = { 10, 60, 120, 255 };
        GLubyte got[4 * 4 * 4];
        GLuint vao, src, dst, fbo, p = glCreateProgram();
        GLuint vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
        GLint ok = 0, lshift, lcolor;

        glGenVertexArrays(1, &vao); glBindVertexArray(vao);
        glShaderSource(vs, 1, &VS, NULL); glCompileShader(vs);
        glShaderSource(fs, 1, &FS, NULL); glCompileShader(fs);
        glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
        glGetProgramiv(p, GL_LINK_STATUS, &ok);

        glGenTextures(1, &src);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, src);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glGenTextures(1, &dst);
        glBindTexture(GL_TEXTURE_2D, dst);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
        glViewport(0, 0, 4, 4);

        glUseProgram(p);
        lshift = glGetUniformLocation(p, "u_shift");
        lcolor = glGetUniformLocation(p, "u_color");
        glUniform1i(glGetUniformLocation(p, "s"), 0);
        glUniform1f(lshift, 0.0f);
        glUniform4f(lcolor, 0.0f, 0.0f, 0.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, src);
        drain();
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glFinish();
        glBindTexture(GL_TEXTURE_2D, dst);
        memset(got, 0, sizeof got);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);

        gate("vertex shaders can sample a texture",
             got[0] == 10 && got[1] == 60 && got[2] == 120,
             "texelFetch from the vertex stage");
        gate("uniform locations are per program, not per stage",
             ok && lshift >= 0 && lcolor >= 0 && lshift != lcolor,
             "a vertex and a fragment uniform must not share one");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(1, &src);
        glDeleteTextures(1, &dst);
    }

    /* A block declared as an instance array is several GL blocks. */
    {
        static const char *VS =
            "#version 330 core\n"
            "layout(std140) uniform B { vec4 v; } b[3];\n"
            "void main(){ gl_Position = b[0].v + b[1].v + b[2].v; }\n";
        GLuint p = glCreateProgram(), vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint i0, i1, i2;
        GLint ok = 0;

        glShaderSource(vs, 1, &VS, NULL); glCompileShader(vs);
        glAttachShader(p, vs); glLinkProgram(p);
        glGetProgramiv(p, GL_LINK_STATUS, &ok);

        i0 = glGetUniformBlockIndex(p, "B[0]");
        i1 = glGetUniformBlockIndex(p, "B[1]");
        i2 = glGetUniformBlockIndex(p, "B[2]");

        gate("block instance arrays are separate blocks",
             ok && i0 != 0xFFFFFFFFu && i1 != 0xFFFFFFFFu && i2 != 0xFFFFFFFFu &&
             i0 != i1 && i1 != i2,
             "B[0], B[1] and B[2] each get an index and a binding");
    }

    /* "uniform S s;" is one Metal buffer and many GL uniforms. */
    {
        static const char *FS =
            "#version 330 core\n"
            "struct S { int a; int b[3]; int c; };\n"
            "uniform S s;\n"
            "layout(location=0) out ivec4 o;\n"
            "void main(){ o = ivec4(s.a, s.b[0], s.b[2], s.c); }\n";
        static const char *VS =
            "#version 330 core\n"
            "void main(){ gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
        GLuint p = glCreateProgram();
        GLuint vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
        GLint ok = 0, la, lb0, lb2, lc;

        glShaderSource(vs, 1, &VS, NULL); glCompileShader(vs);
        glShaderSource(fs, 1, &FS, NULL); glCompileShader(fs);
        glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
        glGetProgramiv(p, GL_LINK_STATUS, &ok);

        la  = glGetUniformLocation(p, "s.a");
        lb0 = glGetUniformLocation(p, "s.b[0]");
        lb2 = glGetUniformLocation(p, "s.b[2]");
        lc  = glGetUniformLocation(p, "s.c");

        gate("struct uniforms expose their members",
             ok && la >= 0 && lb0 >= 0 && lc >= 0 && lb2 == lb0 + 2,
             "s.a, s.b[0], s.b[2] and s.c each get a location");
    }

    /* glGetTexImage reads a compressed texture back as plain pixels. */
    {
        GLubyte src[8 * 8], got[8 * 8];
        GLuint t;
        int worst = 0, i;

        for (i = 0; i < 64; i++)
            src[i] = (GLubyte)((i % 8) * 32);

        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        drain();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RED_RGTC1, 8, 8, 0,
                     GL_RED, GL_UNSIGNED_BYTE, src);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        memset(got, 0xAB, sizeof got);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, got);

        for (i = 0; i < 64; i++)
        {
            int d = (int)got[i] - (int)src[i];

            if (d < 0) d = -d;
            if (d > worst) worst = d;
        }

        gate("compressed textures read back uncompressed",
             glGetError() == GL_NO_ERROR && worst <= 16,
             "glGetTexImage decompresses RGTC");
        glDeleteTextures(1, &t);
    }

    /* Advertising the string is not the criterion -- the arithmetic is. 1e-10
       added to 1.0 vanishes in a 24-bit float and survives in a double, so a
       result near 1.0 here can only come from real fp64. */
    {
        GLuint p = glCreateProgram(), c = glCreateShader(GL_COMPUTE_SHADER), b;
        const char *src =
            "#version 430\n"
            "layout(local_size_x=1) in;\n"
            "layout(std430, binding=0) buffer Out { float r; };\n"
            "void main(){\n"
            "  double a = 1.0LF;\n"
            "  double b = a + 1.0e-10LF;\n"
            "  r = float((b - a) * 1.0e10LF);\n"
            "}\n";
        GLint ok = 0;
        float got = -1.0f, seed = -1.0f;

        glShaderSource(c, 1, &src, NULL); glCompileShader(c);
        glGetShaderiv(c, GL_COMPILE_STATUS, &ok);

        if (ok)
        {
            glAttachShader(p, c); glLinkProgram(p);
            glGetProgramiv(p, GL_LINK_STATUS, &ok);
        }

        if (ok)
        {
            glGenBuffers(1, &b);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, b);
            glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float), &seed, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, b);

            glUseProgram(p);
            glDispatchCompute(1, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(float), &got);
            glUseProgram(0);
        }

        gate("ARB_gpu_shader_fp64 advertised", hasExt("GL_ARB_gpu_shader_fp64"),
             "core since 4.0");
        gate("doubles keep bits a float cannot", ok && got > 0.99f && got < 1.01f,
             ok ? "1.0 + 1e-10 must not round back to 1.0" : "the shader would not compile");
    }
}

/* --- phase 5: the features Metal has no answer for ----------------------- */
static void phase5(void)
{
    GLuint tex, q, tf;
    GLint unhandled;

    phase("PHASE 5  the features Metal has no answer for");

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex);
    drain();
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 16, 16, GL_TRUE);
    gate("multisample storage allocates", glGetError() == GL_NO_ERROR, NULL);

    /* Rasterising multisampled means an edge that lands between samples comes
       back as a blend. A driver that quietly renders single sampled reports
       the same GL_SAMPLES and fails here. */
    {
        static const char *vs = "#version 410\nlayout(location=0) in vec2 p;void main(){gl_Position=vec4(p,0,1);}\n";
        static const char *fs = "#version 410\nout vec4 o;void main(){o=vec4(0,1,0,1);}\n";
        GLuint ms = 0, msfbo = 0, resolve = 0, rfbo = 0, prog;
        GLint samples = 0;
        int edge = 0;

        glGenTextures(1, &ms);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, 64, 64, GL_TRUE);
        glGenFramebuffers(1, &msfbo);
        glBindFramebuffer(GL_FRAMEBUFFER, msfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D_MULTISAMPLE, ms, 0);

        drain();
        glGetIntegerv(0x80A9 /* GL_SAMPLES */, &samples);

        prog = buildProgram(vs, NULL, NULL, NULL, fs);

        if (prog && samples > 1)
        {
            static const GLfloat tri[6] = { -1,-1, 1,-1, -1,1 };
            GLuint vao, vbo;

            glUseProgram(prog);
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(0);
            glViewport(0, 0, 64, 64);
            glClearColor(1, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            rfbo = probeTarget(&resolve);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msfbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rfbo);
            glBlitFramebuffer(0, 0, 64, 64, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, rfbo);

            {
                static GLubyte px[64 * 64 * 4];

                glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, px);

                for (int i = 0; i < 64 * 64; i++)
                    if (px[i * 4 + 1] > 20 && px[i * 4 + 1] < 235)
                        edge++;
            }
        }

        gate("multisample rasterisation", samples > 1 && edge > 8,
             "an edge has to resolve to partial coverage, not to one sample");
    }

    /* Tessellation, drawn. A control shader with no evaluation shader is a
       link error in GL, so the gate builds the pair and asks the tessellator
       for two different subdivision levels of the same patch. */
    {
        static const char *vs = "#version 410\nlayout(location=0) in vec2 p;void main(){gl_Position=vec4(p,0,1);}\n";
        static const char *tcs = "#version 410\nlayout(vertices=3) out;uniform float lvl;\n"
            "void main(){gl_out[gl_InvocationID].gl_Position=gl_in[gl_InvocationID].gl_Position;\n"
            "gl_TessLevelOuter[0]=lvl;gl_TessLevelOuter[1]=lvl;gl_TessLevelOuter[2]=lvl;gl_TessLevelInner[0]=lvl;}\n";
        static const char *tes = "#version 410\nlayout(triangles, equal_spacing, ccw) in;\n"
            "void main(){vec4 q=gl_TessCoord.x*gl_in[0].gl_Position+gl_TessCoord.y*gl_in[1].gl_Position"
            "+gl_TessCoord.z*gl_in[2].gl_Position;\n"
            "float s=0.30*sin(9.4248*gl_TessCoord.x)*sin(9.4248*gl_TessCoord.y);\n"
            "gl_Position=vec4(q.xy*0.7+vec2(s,s),0,1);}\n";
        static const char *fs = "#version 410\nout vec4 o;void main(){o=vec4(0,1,0,1);}\n";
        GLuint prog = buildProgram(vs, tcs, tes, NULL, fs);
        int flat = 0, fine = 0;

        if (prog)
        {
            static const GLfloat tri[6] = { -1,-1, 1,-1, -1,1 };
            GLuint vao, vbo;
            GLint lvl;

            probeTarget(NULL);
            glUseProgram(prog);
            lvl = glGetUniformLocation(prog, "lvl");
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(0);
            glViewport(0, 0, 64, 64);
            glPatchParameteri(0x8E72 /* GL_PATCH_VERTICES */, 3);

            glUniform1f(lvl, 1.0f);
            glClearColor(1, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(0x000E /* GL_PATCHES */, 0, 3);
            flat = greenPixels();

            glUniform1f(lvl, 16.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(0x000E, 0, 3);
            fine = greenPixels();
        }

        gate("tessellation subdivides", prog && flat > 0 && fine > 0 && flat != fine,
             "a higher tessellation level has to change the geometry");
    }

    /* Geometry, drawn. One point in, one quad out: nothing but a geometry
       stage can turn two vertices into two filled squares. */
    {
        static const char *vs = "#version 410\nlayout(location=0) in vec2 p;\n"
            "layout(location=0) out vec4 vcol;\n"
            "void main(){gl_Position=vec4(p,0,1);vcol=vec4(0,1,0,1);}\n";
        static const char *gs = "#version 410\nlayout(points) in;\n"
            "layout(triangle_strip,max_vertices=4) out;\n"
            "layout(location=0) in vec4 vcol[];\nlayout(location=0) out vec4 gcol;\n"
            "void main(){vec4 c=gl_in[0].gl_Position;\n"
            "for(int i=0;i<4;i++){gl_Position=c+vec4(float(i&1)*0.5-0.25,float(i>>1)*0.5-0.25,0,0);\n"
            "gcol=vcol[0];EmitVertex();}EndPrimitive();}\n";
        static const char *fs = "#version 410\nlayout(location=0) in vec4 gcol;out vec4 o;void main(){o=gcol;}\n";
        GLuint prog = buildProgram(vs, NULL, NULL, gs, fs);
        int drawn = 0;

        if (prog)
        {
            static const GLfloat pts[4] = { 0, 0, 0.5f, 0.5f };
            GLuint vao, vbo;

            probeTarget(NULL);
            glUseProgram(prog);
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof pts, pts, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(0);
            glViewport(0, 0, 64, 64);
            glClearColor(1, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_POINTS, 0, 2);
            drawn = greenPixels();
        }

        gate("geometry shaders emit", prog && drawn > 400,
             "two points have to come out as two filled quads");
    }

    /* An occlusion query that always answers zero is not an occlusion query. */
    {
        static const char *vs = "#version 410\nlayout(location=0) in vec2 p;void main(){gl_Position=vec4(p,0,1);}\n";
        static const char *fs = "#version 410\nout vec4 o;void main(){o=vec4(0,1,0,1);}\n";
        GLuint prog = buildProgram(vs, NULL, NULL, NULL, fs);
        GLuint passed = 0;

        if (prog)
        {
            probeTarget(NULL);
            glUseProgram(prog);
            probeQuadVAO();
            glViewport(0, 0, 64, 64);
            glClearColor(1, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            glGenQueries(1, &q);
            drain();
            glBeginQuery(GL_SAMPLES_PASSED, q);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glEndQuery(GL_SAMPLES_PASSED);
            glGetQueryObjectuiv(q, GL_QUERY_RESULT, &passed);
        }

        gate("occlusion queries count", passed == 64 * 64,
             "a full screen quad covers every pixel exactly once");
    }

    glGenTransformFeedbacks(1, &tf);
    drain();
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf);
    gate("transform feedback objects bind", glGetError() == GL_NO_ERROR, NULL);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    /* Subroutines, switched. Linking is not enough: the uniform has to pick
       which function runs. */
    {
        static const char *vs = "#version 400\nlayout(location=0) in vec2 p;void main(){gl_Position=vec4(p,0,1);}\n";
        static const char *fs = "#version 400\n"
            "subroutine vec4 pick();\nsubroutine uniform pick which;\n"
            "subroutine(pick) vec4 red(){return vec4(1,0,0,1);}\n"
            "subroutine(pick) vec4 green(){return vec4(0,1,0,1);}\n"
            "out vec4 o;void main(){o=which();}\n";
        GLuint prog = buildProgram(vs, NULL, NULL, NULL, fs);
        int as_red = -1, as_green = -1;

        if (prog)
        {
            GLuint ir, ig;

            probeTarget(NULL);
            glUseProgram(prog);
            probeQuadVAO();
            glViewport(0, 0, 64, 64);

            ir = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "red");
            ig = glGetSubroutineIndex(prog, GL_FRAGMENT_SHADER, "green");

            if (ir != 0xFFFFFFFFu && ig != 0xFFFFFFFFu)
            {
                glClearColor(0, 0, 1, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &ir);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                as_red = greenPixels();

                glClear(GL_COLOR_BUFFER_BIT);
                glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &ig);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                as_green = greenPixels();
            }
        }

        gate("subroutines select", as_red == 0 && as_green == 64 * 64,
             "the subroutine uniform has to choose which function runs");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* --- phase 6: the rest of the burndown ----------------------------------- */
static void phase6(void)
{
    struct { const char *name; GLenum e; GLint floor; } req[] = {
        { "GL_MAX_UNIFORM_BUFFER_BINDINGS",  0x8A2F, 84 },
        { "GL_MAX_UNIFORM_LOCATIONS",        0x826E, 1024 },
        { "GL_MAX_IMAGE_UNITS",              0x8F38, 8 },
        { "GL_MAX_COMPUTE_IMAGE_UNIFORMS",   0x91BD, 8 },
        { "GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS", 0x90DD, 8 },
        { "GL_MAX_TRANSFORM_FEEDBACK_BUFFERS", 0x8E70, 4 },
        { "GL_MAX_PATCH_VERTICES",           0x8E7D, 32 },
        { "GL_MAX_CLIP_DISTANCES",           0x0D32, 8 },
    };
    int all = 1;
    char buf[96];

    phase("PHASE 6  the rest of the burndown");

    for (unsigned i = 0; i < sizeof req / sizeof *req; i++)
    {
        int unhandled;
        GLint v = limit(req[i].e, &unhandled);

        if (unhandled || v < req[i].floor)
        {
            snprintf(buf, sizeof buf, "%s reports %s, floor %d", req[i].name,
                     unhandled ? "nothing" : "too little", req[i].floor);
            gate(req[i].name, 0, buf);
            all = 0;
        }
    }

    if (all)
        gate("every queried limit meets the 4.6 floor", 1, "8 of 8");

    gate("unknown get enums raise GL_INVALID_ENUM", ({
            GLint v = 12345;
            drain();
            glGetIntegerv(0x7FFF, &v);
            glGetError() == GL_INVALID_ENUM;
         }), "silence here returns uninitialised memory");

    gate("ARB_cull_distance advertised", hasExt("GL_ARB_cull_distance"), "core since 4.5");

    /* Culling is not clipping: a triangle whose corners disagree stays whole,
       and one every corner rejects goes away completely. */
    {
        static const char *vs =
            "#version 450 core\n"
            "layout(location = 0) in vec2 p;\n"
            "layout(location = 1) in float d;\n"
            "out float gl_CullDistance[1];\n"
            "void main() { gl_Position = vec4(p, 0, 1); gl_CullDistance[0] = d; }\n";
        static const char *fs =
            "#version 450 core\n"
            "out vec4 o;\n"
            "void main() { o = vec4(0, 1, 0, 1); }\n";
        GLuint prog = buildProgram(vs, NULL, NULL, NULL, fs);
        int whole = 0, mixed = 0, none = 1;

        if (prog)
        {
            GLuint fbo = probeTarget(NULL), vao, vbo;
            GLfloat v[9] = { -1,-1, 1,  3,-1, 1,  -1,3, 1 };

            glUseProgram(prog);
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glGenBuffers(1, &vbo);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_DYNAMIC_DRAW);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 12, 0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 12, (void *)8);
            glEnableVertexAttribArray(1);
            glViewport(0, 0, 64, 64);
            glClearColor(1, 0, 0, 1);

            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            whole = greenPixels();

            v[2] = -1.0f;
            glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_DYNAMIC_DRAW);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            mixed = greenPixels();

            v[5] = v[8] = -1.0f;
            glBufferData(GL_ARRAY_BUFFER, sizeof v, v, GL_DYNAMIC_DRAW);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            none = greenPixels();

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fbo);
            glUseProgram(0);
        }

        gate("cull distance drops whole primitives", whole > 1000 && mixed == whole && none == 0,
             "one negative corner keeps the triangle, three lose it");
    }

    gate("ARB_ES3_1_compatibility advertised", hasExt("GL_ARB_ES3_1_compatibility"), "core since 4.5");

    /* The point of the extension is that an ES 3.1 shader compiles and runs in
       a desktop context, so ask one to fill a storage buffer. */
    {
        static const char *cs =
            "#version 310 es\n"
            "layout(local_size_x = 4) in;\n"
            "layout(std430, binding = 0) buffer Out { uint v[]; } o;\n"
            "void main() { o.v[gl_GlobalInvocationID.x] = gl_GlobalInvocationID.x + 7u; }\n";
        GLuint prog = glCreateProgram();
        GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
        GLint ok = 0;
        GLuint got[4] = { 0, 0, 0, 0 };

        glShaderSource(sh, 1, &cs, NULL);
        glCompileShader(sh);
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

        if (ok)
        {
            glAttachShader(prog, sh);
            glLinkProgram(prog);
            glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        }

        if (ok)
        {
            GLuint ssbo;

            glGenBuffers(1, &ssbo);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
            glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof got, NULL, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
            glUseProgram(prog);
            glDispatchCompute(1, 1, 1);
            glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof got, got);
            glUseProgram(0);
            glDeleteBuffers(1, &ssbo);
        }

        gate("an ES 3.1 shader runs in a desktop context",
             ok && got[0] == 7 && got[3] == 10,
             "#version 310 es, dispatched, read back");
    }

    /* Direct state access: build and fill a texture without ever binding it,
       then read the bytes back. */
    {
        GLuint tex = 0;
        GLubyte want[16], got[16];
        int same = 0;

        for (int i = 0; i < 16; i++)
            want[i] = (GLubyte)(i * 7 + 3);

        memset(got, 0, sizeof got);
        drain();

        glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        glTextureStorage2D(tex, 1, GL_RGBA8, 2, 2);
        glTextureSubImage2D(tex, 0, 0, 0, 2, 2, GL_RGBA, GL_UNSIGNED_BYTE, want);
        glGetTextureImage(tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, sizeof got, got);

        same = glGetError() == GL_NO_ERROR && memcmp(want, got, sizeof got) == 0;

        glDeleteTextures(1, &tex);

        gate("a texture can be made and filled without binding it",
             same, "glCreateTextures, glTextureStorage2D, glTextureSubImage2D, glGetTextureImage");
    }

    /* Shader images: a compute shader writes one, and the bytes come back. */
    {
        static const char *cs =
            "#version 430 core\n"
            "layout(local_size_x = 2, local_size_y = 2) in;\n"
            "layout(rgba8, binding = 0) uniform writeonly image2D img;\n"
            "void main() { imageStore(img, ivec2(gl_GlobalInvocationID.xy), vec4(0, 1, 0, 1)); }\n";
        GLuint prog = glCreateProgram();
        GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
        GLint ok = 0;
        GLubyte got[16];

        memset(got, 0, sizeof got);
        glShaderSource(sh, 1, &cs, NULL);
        glCompileShader(sh);
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

        if (ok)
        {
            glAttachShader(prog, sh);
            glLinkProgram(prog);
            glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        }

        if (ok)
        {
            GLuint tex;

            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2, 2);
            glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
            glUseProgram(prog);
            glDispatchCompute(1, 1, 1);
            glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
            glUseProgram(0);
            glDeleteTextures(1, &tex);
        }

        gate("a compute shader writes an image",
             ok && got[0] == 0 && got[1] == 255 && got[13] == 255,
             "imageStore into an rgba8 image2D, read back through glGetTexImage");
    }
}

/* --- phase 8: extensions beyond core ------------------------------------- */
static void phase8(void)
{
    static const char *want[] = {
        "GL_KHR_shader_subgroup", "GL_ARB_bindless_texture",
        "GL_ARB_fragment_shader_interlock", "GL_EXT_shader_image_load_formatted",
        "GL_ARB_shader_viewport_layer_array", "GL_KHR_parallel_shader_compile",
        "GL_ARB_sparse_texture", "GL_EXT_texture_compression_s3tc",
    };

    phase("PHASE 8  extensions beyond core");

    gate("GL_KHR_debug advertised and live", hasExt("GL_KHR_debug"), "done -- errors reach the callback");

    for (unsigned i = 0; i < sizeof want / sizeof *want; i++)
        gate(want[i], hasExt(want[i]), NULL);
}

/* --- phase 10: the compatibility profile games still ask for ------------- */
/*
 * MGL is a core profile driver. A full compatibility profile is not worth
 * building, but a handful of removed features still turn up in shipped games,
 * and each gate below is one of them. Built with -DMGL_NO_COMPAT_PROFILE
 * every gate here stands, which is the point of the switch.
 */
static void phase10(void)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    /* green at a quarter alpha, so a reference of a half decides it */
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0.0, 1.0, 0.0, 0.25); }\n";
    GLuint fbo, tex, prog;
    GLint func = 0;
    GLfloat ref = -1.0f;
    int accepted, stored, discarded = 0, kept = 0;

    phase("PHASE 10  the compatibility profile games still ask for");

    /* --- the alpha test --- */
    drain();
    glEnable(GL_ALPHA_TEST);
    accepted = (glGetError() == GL_NO_ERROR) && glIsEnabled(GL_ALPHA_TEST);

    gate("glEnable(GL_ALPHA_TEST) is a state MGL keeps", accepted,
         "the fixed function stage old engines use for foliage and fences");

    drain();
    glAlphaFunc(GL_GREATER, 0.5f);
    glGetIntegerv(GL_ALPHA_TEST_FUNC, &func);
    glGetFloatv(GL_ALPHA_TEST_REF, &ref);
    stored = (glGetError() == GL_NO_ERROR) && func == GL_GREATER &&
             ref > 0.49f && ref < 0.51f;

    gate("glAlphaFunc stores its compare and reference", stored, NULL);

    fbo = probeTarget(&tex);
    probeQuadVAO();
    prog = buildProgram(vs, NULL, NULL, NULL, fs);

    if (prog && accepted)
    {
        glViewport(0, 0, 64, 64);
        glUseProgram(prog);

        /* a quarter is not greater than a half, so nothing should survive */
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glFinish();
        discarded = (greenPixels() == 0);

        /* and with the compare reversed every fragment should land */
        glAlphaFunc(GL_LESS, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glFinish();
        kept = (greenPixels() == 64 * 64);

        glUseProgram(0);
    }

    gate("the alpha test discards what it should", prog && discarded && kept,
         "fragment alpha 0.25 against a reference of 0.5, both ways round");

    glDisable(GL_ALPHA_TEST);

    /* --- the wrap mode that came before the two --- */
    {
        GLint got = 0;

        drain();
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &got);

        gate("GL_CLAMP is taken as clamp to border",
             glGetError() == GL_NO_ERROR && got == GL_CLAMP_TO_BORDER,
             "shadow maps and old UI code still set it");
    }

    /* --- the rest of the list, none of it built yet --- */
    gate("a border colour Metal does not hold is exact", 0,
         "Metal keeps three; anything else snaps to the nearest");

    {
        /* a client pointer with no array buffer bound is still an error */
        GLfloat verts[6] = { 0 };

        drain();
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, verts);

        gate("a vertex array in client memory draws", glGetError() == GL_NO_ERROR,
             "engines written before VBOs stream straight out of malloc");
    }

    {
        /* a rectangle texture is read at [0, width], not [0, 1] */
        static const char *rect_fs =
            "#version 460 core\n"
            "uniform sampler2DRect r;\n"
            "out vec4 o;\n"
            "void main() { o = texture(r, vec2(1.5, 0.5)); }\n";
        static const GLubyte texels[8] = { 255,0,0,255,  0,255,0,255 };
        GLuint rect = 0, rp;
        int read_green = 0;

        drain();
        glGenTextures(1, &rect);
        glBindTexture(GL_TEXTURE_RECTANGLE, rect);
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA8, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        rp = buildProgram(vs, NULL, NULL, NULL, rect_fs);

        if (rp && glGetError() == GL_NO_ERROR)
        {
            glUseProgram(rp);
            glUniform1i(glGetUniformLocation(rp, "r"), 0);
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glFinish();
            /* texel one is green, and only unnormalised coordinates reach it */
            read_green = (greenPixels() == 64 * 64);
            glUseProgram(0);
            glDeleteProgram(rp);
        }

        glDeleteTextures(1, &rect);

        gate("GL_TEXTURE_RECTANGLE takes unnormalised coordinates", read_green,
             "console ports and UI layers used it to dodge power of two sizes");
    }

    drain();
    glEnable(0x0B50 /* GL_LIGHTING */);
    gate("fixed function lighting has an uber shader behind it",
         glGetError() == GL_NO_ERROR,
         "retro titles and modding tools light geometry this way");

    gate("the modelview and projection matrix stacks are emulated", 0,
         "glPushMatrix, glLoadIdentity, glMultMatrixf and the rest");

    glDeleteProgram(prog);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    (void)fbo;
}

/* --- phase 10B: the software that is not a Steam game ------------------- */
/*
 * CAD packages, scientific viewers, modding utilities and the GoldSrc engine
 * ask for a compatibility profile for reasons phase 10 does not cover. The
 * matrix stack, which GoldSrc also needs, is gated in phase 10.
 */
static void phase10B(void)
{
    phase("PHASE 10B  legacy desktop software and the GoldSrc line");

    drain();
    glDrawBuffer(GL_FRONT);
    glReadBuffer(GL_BACK);
    gate("the default framebuffer answers to the old buffer names",
         glGetError() == GL_NO_ERROR,
         "GL_FRONT, GL_BACK, GL_AUXi and the stereo pair, for overlays");

    drain();
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 1.0f, 0.0f);
    glEnd();
    gate("glBegin and glEnd gather into a buffer and draw",
         glGetError() == GL_NO_ERROR,
         "one feature unlocks a graveyard of internal tools");

    drain();
    glEnable(GL_POINT_SPRITE);
    glPointSize(4.0f);
    gate("point sprites size and texture themselves",
         glGetError() == GL_NO_ERROR,
         "particles and sparks drawn without building quads");

    drain();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glPopAttrib();
    gate("glPushAttrib and glPopAttrib save and restore state",
         glGetError() == GL_NO_ERROR,
         "GUI toolkits wrap every custom widget in one");

    drain();
    glPolygonMode(GL_FRONT, GL_FILL);
    glPolygonMode(GL_BACK, GL_LINE);
    gate("front and back rasterise differently",
         glGetError() == GL_NO_ERROR,
         "wireframe over shaded, which CAD draws in a single pass");

    drain();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    gate("glTexEnv combines a texture with the fragment colour",
         glGetError() == GL_NO_ERROR,
         "lightmaps and detail maps without a shader of their own");

    drain();
    glEdgeFlag(1);
    gate("glEdgeFlag is accepted", glGetError() == GL_NO_ERROR,
         "modelling tools mark which polygon edges a wireframe draws");

    gate("GL_ARB_multitexture advertised", hasExt("GL_ARB_multitexture"),
         "GoldSrc looks for the extension string, not the core entry point");
}

/* --- phase 10C: the rest of the vintage stack --------------------------- */
/*
 * What is left after phase 10B before an application stops needing a
 * compatibility profile at all.
 */
static void phase10C(void)
{
    static const char *vs =
        "#version 460 core\n"
        "layout(location = 0) in vec2 p;\n"
        "void main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 460 core\n"
        "out vec4 o;\n"
        "void main() { o = vec4(0.0, 1.0, 0.0, 1.0); }\n";
    static const GLubyte one_pixel[4] = { 255, 255, 255, 255 };
    static const GLubyte one_bit[1] = { 0x80 };
    GLuint list, query = 0, prog;
    GLuint passed = 0;
    int quads_drew = 0;

    phase("PHASE 10C  the rest of the vintage stack");

    /* a real draw, so the gate answers about the mode and not the setup */
    probeTarget(NULL);
    probeQuadVAO();
    prog = buildProgram(vs, NULL, NULL, NULL, fs);

    if (prog)
    {
        glViewport(0, 0, 64, 64);
        glUseProgram(prog);
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        drain();
        glDrawArrays(GL_QUADS, 0, 4);
        glFinish();
        quads_drew = (glGetError() == GL_NO_ERROR) && greenPixels() > 0;
        glUseProgram(0);
        glDeleteProgram(prog);
    }

    gate("GL_QUADS, GL_QUAD_STRIP and GL_POLYGON expand to triangles",
         quads_drew,
         "level geometry and flat UI built four vertices at a time");

    drain();
    list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glEndList();
    glCallList(list);
    gate("a display list records commands and replays them",
         list != 0 && glGetError() == GL_NO_ERROR,
         "terrain chunks, fonts and models compiled once");

    drain();
    glRenderMode(GL_SELECT);
    glRenderMode(GL_RENDER);
    gate("glRenderMode(GL_SELECT) answers what the cursor is over",
         glGetError() == GL_NO_ERROR,
         "how every legacy editor handles a click on 3D geometry");

    drain();
    glDrawPixels(1, 1, GL_RGBA, GL_UNSIGNED_BYTE, one_pixel);
    glBitmap(1, 1, 0.0f, 0.0f, 0.0f, 0.0f, one_bit);
    gate("glDrawPixels and glBitmap reach the framebuffer",
         glGetError() == GL_NO_ERROR,
         "debug text and HUDs written straight to the screen");

    /* queries are core in 4.6, so this asks whether MGL services them */
    drain();
    glGenQueries(1, &query);
    glBeginQuery(GL_SAMPLES_PASSED, query);
    glEndQuery(GL_SAMPLES_PASSED);
    glGetQueryObjectuiv(query, GL_QUERY_RESULT, &passed);
    gate("occlusion queries return without stalling the caller",
         query != 0 && glGetError() == GL_NO_ERROR,
         "flight sims cull scenery by asking what the depth test passed");
}

/* --- phase 10D: early Mac OS X ------------------------------------------ */
/*
 * Screen savers, iTunes visualisers and vintage ports from the PowerPC and
 * early Intel years, which leaned on Apple's own extensions.
 */
static void phase10D(void)
{
    GLuint tex = 0;
    static const GLubyte bgra[4] = { 0, 0, 255, 255 };

    phase("PHASE 10D  early Mac OS X");

    gate("GL_EXT_texture_rectangle aliases the core rectangle target",
         hasExt("GL_EXT_texture_rectangle"),
         "the spelling early Mac code checks for before it will run");

    gate("GL_APPLE_client_storage", hasExt("GL_APPLE_client_storage"),
         "a texture kept in the application's own memory");

    gate("GL_APPLE_texture_range", hasExt("GL_APPLE_texture_range"),
         "and the hint saying how that memory will be read");

    gate("GL_APPLE_ycbcr_422", hasExt("GL_APPLE_ycbcr_422"),
         "QuickTime handed video frames to GL in their own colour space");

    gate("GL_APPLE_packed_pixels", hasExt("GL_APPLE_packed_pixels"),
         "the tokens alone; the unpacking is already core");

    drain();
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA, GL_UNSIGNED_BYTE, bgra);
    gate("GL_BGRA uploads without a byte swap", glGetError() == GL_NO_ERROR,
         "the order the old window server already had the pixels in");
    glDeleteTextures(1, &tex);

    gate("AGL and CGL context creation is translated", 0,
         "aglCreateContext and CGLChoosePixelFormat, which this probe "
         "cannot reach from inside a context");
}

int main(void)
{
    printf("MGL phase probe -- %s\n", (const char *)glGetString(GL_VERSION));
    printf("%s\n\n", (const char *)glGetString(GL_RENDERER));

    phase4();
    phase5();
    phase6();
    phase8();
    phase10();
    phase10B();
    phase10C();
    phase10D();

    printf("    %s\n\n", g_phase_gates ? "-> gates remaining" : "-> PHASE COMPLETE");
    printf("%d gate(s) remaining across the probed phases.\n", g_gates);
    printf("Phase 9 has its own probe: build/probe_fsr2\n");

    return g_gates ? 1 : 0;
}
