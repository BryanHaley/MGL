/*
 * gears_gl46.c
 * MGL
 *
 * The gears demo rewritten against an OpenGL 4.6 core profile.
 *
 * Everything the original got from fixed function is done explicitly here:
 * the gear geometry goes into vertex buffers instead of display lists, the
 * matrix stack is replaced by matrices built on the CPU and sent as uniforms,
 * and the GL_LIGHT0 lighting model is written out as GLSL.
 *
 * Original gears demo copyright (C) 1999-2001 Brian Paul, MIT licence.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define GL_GLEXT_PROTOTYPES 1
#include "glcorearb.h"

#include <GLFW/glfw3.h>

#include "MGLContext.h"
#include "gears_common.h"

extern void *CppCreateMGLRendererHeadless(void *glm_ctx);

/* ---------- shaders ---------- */

static const char *VERTEX_SRC =
"#version 460 core\n"
"layout(location = 0) in vec3 in_pos;\n"
"layout(location = 1) in vec3 in_normal;\n"
"\n"
"layout(location = 0) uniform mat4 u_mvp;\n"
"layout(location = 1) uniform mat4 u_modelview;\n"
"layout(location = 2) uniform mat4 u_normal_matrix;\n"
"\n"
"layout(location = 0) out vec3 v_normal;\n"
"layout(location = 1) out vec3 v_eye_pos;\n"
"\n"
"void main()\n"
"{\n"
"    v_normal    = mat3(u_normal_matrix) * in_normal;\n"
"    v_eye_pos   = (u_modelview * vec4(in_pos, 1.0)).xyz;\n"
"    gl_Position = u_mvp * vec4(in_pos, 1.0);\n"
"}\n";

static const char *FRAGMENT_SRC =
"#version 460 core\n"
"layout(location = 0) in vec3 v_normal;\n"
"layout(location = 1) in vec3 v_eye_pos;\n"
"\n"
"layout(location = 3) uniform vec4 u_color;\n"
"layout(location = 4) uniform vec4 u_light_pos;\n"
"\n"
"layout(location = 0) out vec4 frag_colour;\n"
"\n"
"void main()\n"
"{\n"
"    vec3 N = normalize(v_normal);\n"
"    vec3 L = (u_light_pos.w == 0.0)\n"
"           ? normalize(u_light_pos.xyz)\n"
"           : normalize(u_light_pos.xyz - v_eye_pos);\n"
"\n"
"    float diffuse = max(dot(N, L), 0.0);\n"
"\n"
"    // 0.2 stands in for the fixed-function global ambient term\n"
"    vec3 c = u_color.rgb * (0.2 + diffuse);\n"
"\n"
"    frag_colour = vec4(min(c, vec3(1.0)), u_color.a);\n"
"}\n";

/* ---------- state ---------- */

typedef struct {
    GLuint vao;
    GLuint vbo;
    GLsizei count;
    float color[4];
} Gear;

static Gear   gears[3];
static GLuint program;

static float view_rotx = 20.0f, view_roty = 30.0f, view_rotz = 0.0f;
static float angle = 0.0f;

static float projection[16];

static int opt_frames  = 0;
static int opt_headless = 0;
static int opt_check   = 0;
static int opt_info    = 0;
static int opt_width   = 300;
static int opt_height  = 300;
static const char *opt_out = NULL;
static int opt_only   = -1;   /* draw just one gear, for debugging */

/* uniform locations, fixed by the layout qualifiers above */
enum { U_MVP = 0, U_MODELVIEW = 1, U_NORMAL_MATRIX = 2, U_COLOR = 3, U_LIGHT_POS = 4 };

/* ---------- gl helpers ---------- */

static GLuint compile(GLenum stage, const char *src)
{
    GLuint s = glCreateShader(stage);
    GLint ok = 0;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        char log[4096] = { 0 };
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "gears_gl46: %s shader failed:\n%s\n",
                stage == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }

    return s;
}

static GLuint build_program(void)
{
    GLuint vs = compile(GL_VERTEX_SHADER, VERTEX_SRC);
    GLuint fs = 0, p = 0;
    GLint ok = 0;

    if (!vs) return 0;

    fs = compile(GL_FRAGMENT_SHADER, FRAGMENT_SRC);
    if (!fs) { glDeleteShader(vs); return 0; }

    p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    if (!ok)
    {
        char log[4096] = { 0 };
        glGetProgramInfoLog(p, sizeof log, NULL, log);
        fprintf(stderr, "gears_gl46: link failed:\n%s\n", log);
        glDeleteProgram(p);
        p = 0;
    }

    glDetachShader(p, vs);
    glDetachShader(p, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return p;
}

static void make_gear(Gear *g, float inner, float outer, float width,
                      int teeth, float tooth_depth,
                      float r, float gr, float b)
{
    GearMesh mesh;

    gears_mesh_init(&mesh);
    gears_build(&mesh, inner, outer, width, teeth, tooth_depth);

    g->count = (GLsizei)mesh.count;
    g->color[0] = r; g->color[1] = gr; g->color[2] = b; g->color[3] = 1.0f;

    glGenVertexArrays(1, &g->vao);
    glBindVertexArray(g->vao);

    glGenBuffers(1, &g->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(mesh.count * sizeof(GearVertex)),
                 mesh.verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GearVertex),
                          (const void *)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GearVertex),
                          (const void *)(3 * sizeof(float)));

    glBindVertexArray(0);
    gears_mesh_free(&mesh);
}

static void init_scene(void)
{
    make_gear(&gears[0], 1.0f, 4.0f, 1.0f, 20, 0.7f, 0.8f, 0.1f, 0.0f);
    make_gear(&gears[1], 0.5f, 2.0f, 2.0f, 10, 0.7f, 0.0f, 0.8f, 0.2f);
    make_gear(&gears[2], 1.3f, 2.0f, 0.5f, 10, 0.7f, 0.2f, 0.2f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

static void reshape(int width, int height)
{
    float h = (float)height / (float)width;

    glViewport(0, 0, width, height);
    mat4_frustum(projection, -1.0f, 1.0f, -h, h, 5.0f, 60.0f);
}

/* The original stacked these with glPushMatrix; here the same product is built
   directly: T(0,0,-40) * Rx * Ry * Rz * T(gear) * Rz(spin). */
static void draw_gear(const Gear *g, float tx, float ty, float spin)
{
    float modelview[16], mvp[16], normal_matrix[16];

    mat4_identity(modelview);
    mat4_translate(modelview, 0.0f, 0.0f, -40.0f);
    mat4_rotate(modelview, view_rotx, 1.0f, 0.0f, 0.0f);
    mat4_rotate(modelview, view_roty, 0.0f, 1.0f, 0.0f);
    mat4_rotate(modelview, view_rotz, 0.0f, 0.0f, 1.0f);

    mat4_translate(modelview, tx, ty, 0.0f);
    mat4_rotate(modelview, spin, 0.0f, 0.0f, 1.0f);

    mat4_mul(mvp, projection, modelview);
    mat4_normal_matrix(normal_matrix, modelview);

    glUniformMatrix4fv(U_MVP, 1, GL_FALSE, mvp);
    glUniformMatrix4fv(U_MODELVIEW, 1, GL_FALSE, modelview);
    glUniformMatrix4fv(U_NORMAL_MATRIX, 1, GL_FALSE, normal_matrix);
    glUniform4fv(U_COLOR, 1, g->color);

    glBindVertexArray(g->vao);
    glDrawArrays(GL_TRIANGLES, 0, g->count);
}

static void draw_scene(void)
{
    static const float light_pos[4] = { 5.0f, 5.0f, 10.0f, 0.0f };

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program);
    glUniform4fv(U_LIGHT_POS, 1, light_pos);

    if (opt_only < 0 || opt_only == 0) draw_gear(&gears[0], -3.0f, -2.0f, angle);
    if (opt_only < 0 || opt_only == 1) draw_gear(&gears[1],  3.1f, -2.0f, -2.0f * angle - 9.0f);
    if (opt_only < 0 || opt_only == 2) draw_gear(&gears[2], -3.1f,  4.2f, -2.0f * angle - 25.0f);

    glBindVertexArray(0);
}

/* ---------- output + verification ---------- */

static int write_tga(const char *path, const unsigned char *rgba, int w, int h)
{
    FILE *f = fopen(path, "wb");
    unsigned char hdr[18] = { 0 };

    if (!f) return 0;

    hdr[2]  = 2;                     /* uncompressed true colour */
    hdr[12] = (unsigned char)(w & 0xFF);
    hdr[13] = (unsigned char)((w >> 8) & 0xFF);
    hdr[14] = (unsigned char)(h & 0xFF);
    hdr[15] = (unsigned char)((h >> 8) & 0xFF);
    hdr[16] = 24;

    fwrite(hdr, 1, sizeof hdr, f);

    /* glReadPixels already hands back bottom-up, which is TGA's default order */
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            const unsigned char *p = rgba + ((size_t)y * w + x) * 4;
            unsigned char bgr[3] = { p[2], p[1], p[0] };
            fwrite(bgr, 1, 3, f);
        }
    }

    fclose(f);

    return 1;
}

/* Confirms something gear-shaped actually rendered: enough of the frame is
   covered, and all three gear colours are present. */
static int check_frame(const unsigned char *px, int w, int h)
{
    size_t n = (size_t)w * h;
    size_t lit = 0, reddish = 0, greenish = 0, bluish = 0;

    for (size_t i = 0; i < n; i++)
    {
        const unsigned char *p = px + i * 4;
        int r = p[0], g = p[1], b = p[2];

        if (r + g + b < 24) continue;      /* background */

        lit++;

        if (r > g + 20 && r > b + 20) reddish++;
        else if (g > r + 20 && g > b + 20) greenish++;
        else if (b > r + 20 && b > g + 20) bluish++;
    }

    double cover = (double)lit / (double)n;

    printf("coverage %.1f%%  red=%zu green=%zu blue=%zu\n",
           cover * 100.0, reddish, greenish, bluish);

    int ok = 1;

    if (cover < 0.05 || cover > 0.95)
    {
        fprintf(stderr, "FAIL: coverage %.1f%% is not plausible for the gears\n", cover * 100.0);
        ok = 0;
    }

    if (!reddish)  { fprintf(stderr, "FAIL: no red gear pixels\n");   ok = 0; }
    if (!greenish) { fprintf(stderr, "FAIL: no green gear pixels\n"); ok = 0; }
    if (!bluish)   { fprintf(stderr, "FAIL: no blue gear pixels\n");  ok = 0; }

    return ok;
}

static int drain_errors(const char *where)
{
    GLenum e;
    int n = 0;

    while ((e = glGetError()) != GL_NO_ERROR)
    {
        fprintf(stderr, "gears_gl46: GL error 0x%04x at %s\n", e, where);
        n++;
        if (n > 8) break;
    }

    return n;
}

/* ---------- headless ---------- */

static int run_headless(void)
{
    GLMContext ctx;
    GLuint fbo = 0, color = 0, depth = 0;
    unsigned char *px;
    int frames = opt_frames ? opt_frames : 1;
    int rc = 0;

    ctx = createGLMContext(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                           GL_DEPTH_COMPONENT, GL_FLOAT,
                           GL_STENCIL_INDEX8, GL_UNSIGNED_BYTE);
    if (!ctx)
    {
        fprintf(stderr, "gears_gl46: createGLMContext failed\n");
        return 77;
    }

    MGLsetCurrentContext(ctx);

    if (!CppCreateMGLRendererHeadless(ctx))
    {
        fprintf(stderr, "gears_gl46: headless renderer failed\n");
        return 77;
    }

    if (opt_info)
    {
        printf("GL_RENDERER = %s\n", (char *)glGetString(GL_RENDERER));
        printf("GL_VERSION  = %s\n", (char *)glGetString(GL_VERSION));
        printf("GL_VENDOR   = %s\n", (char *)glGetString(GL_VENDOR));
    }

    glGenTextures(1, &color);
    glBindTexture(GL_TEXTURE_2D, color);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, opt_width, opt_height);

    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, opt_width, opt_height);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "gears_gl46: offscreen framebuffer incomplete\n");
        return 1;
    }

    program = build_program();
    if (!program)
        return 1;

    init_scene();
    reshape(opt_width, opt_height);

    for (int i = 0; i < frames; i++)
    {
        angle += 2.0f;
        draw_scene();
    }

    if (drain_errors("headless render"))
        rc = 1;

    px = (unsigned char *)calloc((size_t)opt_width * opt_height, 4);
    if (!px) return 1;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, opt_width, opt_height, GL_RGBA, GL_UNSIGNED_BYTE, px);

    if (drain_errors("glReadPixels"))
        rc = 1;

    if (opt_out)
    {
        if (write_tga(opt_out, px, opt_width, opt_height))
            printf("wrote %s\n", opt_out);
        else
            fprintf(stderr, "gears_gl46: could not write %s\n", opt_out);
    }

    if (opt_check && !check_frame(px, opt_width, opt_height))
        rc = 1;

    free(px);

    return rc;
}

/* ---------- windowed ---------- */

static void key_callback(GLFWwindow *w, int key, int sc, int action, int mods)
{
    (void)sc; (void)mods;

    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;

    switch (key)
    {
        case GLFW_KEY_LEFT:   view_roty += 5.0f; break;
        case GLFW_KEY_RIGHT:  view_roty -= 5.0f; break;
        case GLFW_KEY_UP:     view_rotx += 5.0f; break;
        case GLFW_KEY_DOWN:   view_rotx -= 5.0f; break;
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); break;
    }
}

static void size_callback(GLFWwindow *w, int width, int height)
{
    (void)w;
    if (width > 0 && height > 0) reshape(width, height);
}

static void glfw_error_callback(int code, const char *desc)
{
    fprintf(stderr, "GLFW error 0x%04x: %s\n", code, desc ? desc : "(none)");
}

static int run_windowed(void)
{
    GLFWwindow *window;
    int frames = 0;
    double t0;

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
    {
        fprintf(stderr, "gears_gl46: glfwInit failed\n");
        return 77;
    }

    // MGL only offers 4.6; GLFW defaults to 1.0 and its MGL backend
    // refuses anything lower
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

    window = glfwCreateWindow(opt_width, opt_height, "gears (OpenGL 4.6)", NULL, NULL);

    if (!window)
    {
        fprintf(stderr, "gears_gl46: could not create a window\n");
        glfwTerminate();
        return 77;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetFramebufferSizeCallback(window, size_callback);

    if (opt_info)
    {
        printf("GL_RENDERER = %s\n", (char *)glGetString(GL_RENDERER));
        printf("GL_VERSION  = %s\n", (char *)glGetString(GL_VERSION));
        printf("GL_VENDOR   = %s\n", (char *)glGetString(GL_VENDOR));
    }

    program = build_program();
    if (!program)
    {
        glfwTerminate();
        return 1;
    }

    init_scene();
    reshape(opt_width, opt_height);

    t0 = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        angle += 2.0f;

        draw_scene();

        MGLswapBuffers((GLMContext)glfwGetWindowUserPointer(window));
        glfwPollEvents();

        frames++;

        if (opt_frames && frames >= opt_frames)
            break;

        double t = glfwGetTime();
        if (!opt_frames && t - t0 >= 5.0)
        {
            printf("%d frames in %3.1f seconds = %6.3f FPS\n",
                   frames, t - t0, frames / (t - t0));
            t0 = t;
            frames = 0;
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

static void usage(const char *prog)
{
    printf("usage: %s [options]\n", prog);
    printf("  -info         print GL implementation information\n");
    printf("  --frames N    render N frames then exit\n");
    printf("  --headless    render offscreen, no window needed\n");
    printf("  --check       headless render, then verify the gears appeared\n");
    printf("  --out FILE    write the last frame as a TGA\n");
    printf("  --only N      draw only gear N (0,1,2), for debugging\n");
    printf("  --size W H    resolution (default 300x300)\n");
    printf("  -h            this help\n");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-info"))            opt_info = 1;
        else if (!strcmp(argv[i], "--headless"))  opt_headless = 1;
        else if (!strcmp(argv[i], "--check"))     { opt_check = 1; opt_headless = 1; }
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) opt_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--only") && i + 1 < argc) opt_only = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) { opt_out = argv[++i]; opt_headless = 1; }
        else if (!strcmp(argv[i], "--size") && i + 2 < argc)
        {
            opt_width  = atoi(argv[++i]);
            opt_height = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[i]); usage(argv[0]); return 1; }
    }

    return opt_headless ? run_headless() : run_windowed();
}
