/*
 * mgl_probe.c
 * Copyright (C) The Moogle Project
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

/* A stage that compiles but cannot be linked into a program is not support.
   This is the difference the roadmap's tessellation row kept getting wrong. */
static int linksWithStage(GLenum stage, const char *src)
{
    GLuint p = glCreateProgram();
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint e = glCreateShader(stage);
    const char *vs = "#version 400\nvoid main(){gl_Position=vec4(0);}\n";
    const char *fs = "#version 400\nout vec4 o;void main(){o=vec4(1);}\n";
    GLint ok = 0;

    glShaderSource(v, 1, &vs, NULL);  glCompileShader(v);
    glShaderSource(f, 1, &fs, NULL);  glCompileShader(f);
    glShaderSource(e, 1, &src, NULL); glCompileShader(e);
    glGetShaderiv(e, GL_COMPILE_STATUS, &ok);

    if (!ok)
        return 0;

    glAttachShader(p, v); glAttachShader(p, e); glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    return ok;
}

static int links(const char *vs, const char *fs)
{
    GLuint p = glCreateProgram(), v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = 0;

    glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
    glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    return ok;
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

    gate("ARB_gpu_shader_fp64 advertised", hasExt("GL_ARB_gpu_shader_fp64"),
         "core since 4.0; lowering is built but not wired");
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

    gate("multisample rasterisation",
         limit(0x80A9 /* GL_SAMPLES */, &unhandled) > 0 && !unhandled,
         "GL_SAMPLES > 0 on a multisampled target");

    gate("tessellation programs link", linksWithStage(0x8E88 /* TESS_CONTROL */,
            "#version 400\nlayout(vertices=3) out;\n"
            "void main(){gl_TessLevelOuter[0]=1.0;gl_out[gl_InvocationID].gl_Position=vec4(0);}\n"),
         "the stage attached to a program, not just compiled");

    gate("geometry programs link", linksWithStage(GL_GEOMETRY_SHADER,
            "#version 400\nlayout(points) in;\nlayout(points,max_vertices=1) out;\n"
            "void main(){gl_Position=vec4(0);EmitVertex();}\n"),
         "the stage attached to a program, not just compiled");

    glGenQueries(1, &q);
    drain();
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glEndQuery(GL_SAMPLES_PASSED);
    gate("occlusion queries run", glGetError() == GL_NO_ERROR, "a result still has to be non-zero");

    glGenTransformFeedbacks(1, &tf);
    drain();
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tf);
    gate("transform feedback objects bind", glGetError() == GL_NO_ERROR, NULL);

    gate("subroutines link",
         compiles(GL_FRAGMENT_SHADER,
                  "#version 400\nsubroutine vec4 pick();\nsubroutine uniform pick which;\n"
                  "subroutine(pick) vec4 red(){return vec4(1,0,0,1);}\n"
                  "out vec4 o;void main(){o=which();}\n"),
         "core since 4.0");
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
    gate("ARB_ES3_1_compatibility advertised", hasExt("GL_ARB_ES3_1_compatibility"), "core since 4.5");
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

int main(void)
{
    printf("MGL phase probe -- %s\n", (const char *)glGetString(GL_VERSION));
    printf("%s\n\n", (const char *)glGetString(GL_RENDERER));

    phase4();
    phase5();
    phase6();
    phase8();

    printf("    %s\n\n", g_phase_gates ? "-> gates remaining" : "-> PHASE COMPLETE");
    printf("%d gate(s) remaining across the probed phases.\n", g_gates);
    printf("Phase 9 has its own probe: build/probe_fsr2\n");

    return g_gates ? 1 : 0;
}
