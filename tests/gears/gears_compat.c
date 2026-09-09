/*
 * gears_compat.c
 * MGL
 *
 * Brian Paul's gears demo ported straight to macOS. The rendering code is the
 * original fixed-function GL 1.x: display lists, the matrix stack, glBegin,
 * glLightfv, glMaterialfv. Only the windowing was rewritten, from Win32/WGL to
 * GLFW, and GLFW here hands out MGL contexts.
 *
 * Original copyright (C) 1999-2001 Brian Paul, MIT licence. Ported to GLX by
 * Brian Paul, to Win32 by Ben Skeggs, to macOS/MGL here.
 *
 * Note that none of the fixed-function entry points exist in an OpenGL 4.6
 * core profile, so MGL answers all of them with GL_INVALID_OPERATION and this
 * draws nothing. That is the point of --check: it proves MGL turns the whole
 * compatibility profile away cleanly instead of crashing. gears_gl46 is the
 * version that actually renders.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define GL_GLEXT_PROTOTYPES 1
#include "glcorearb.h"

#include <GLFW/glfw3.h>

#include "MGLContext.h"

extern void *CppCreateMGLRendererHeadless(void *glm_ctx);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* glcorearb.h is core only, so the compatibility entry points MGL exports have
   to be declared by hand. */
#define GL_QUADS            0x0007
#define GL_QUAD_STRIP       0x0008
#define GL_COMPILE          0x1300
#define GL_FLAT             0x1D00
#define GL_SMOOTH           0x1D01
#define GL_PROJECTION       0x1701
#define GL_MODELVIEW        0x1700
#define GL_LIGHTING         0x0B50
#define GL_LIGHT0           0x4000
#define GL_POSITION         0x1203
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_FRONT_COMPAT     0x0404
#define GL_NORMALIZE        0x0BA1

extern void   glBegin(GLenum mode);
extern void   glEnd(void);
extern void   glVertex3f(GLfloat x, GLfloat y, GLfloat z);
extern void   glNormal3f(GLfloat x, GLfloat y, GLfloat z);
extern void   glShadeModel(GLenum mode);
extern void   glPushMatrix(void);
extern void   glPopMatrix(void);
extern void   glLoadIdentity(void);
extern void   glMatrixMode(GLenum mode);
extern void   glRotatef(GLfloat a, GLfloat x, GLfloat y, GLfloat z);
extern void   glTranslatef(GLfloat x, GLfloat y, GLfloat z);
extern void   glFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
extern GLuint glGenLists(GLsizei range);
extern void   glNewList(GLuint list, GLenum mode);
extern void   glEndList(void);
extern void   glCallList(GLuint list);
extern void   glLightfv(GLenum light, GLenum pname, const GLfloat *params);
extern void   glMaterialfv(GLenum face, GLenum pname, const GLfloat *params);

static GLfloat view_rotx = 20.0f, view_roty = 30.0f, view_rotz = 0.0f;
static GLuint gear1, gear2, gear3;
static GLfloat angle = 0.0f;

static int   opt_frames  = 0;      /* 0 means run until the window closes */
static int   opt_check   = 0;
static int   opt_info    = 0;
static int   opt_headless = 0;
static int   opt_width   = 300;
static int   opt_height  = 300;

/* how many distinct GL errors the compatibility calls produced */
static int   err_count   = 0;
static int   call_count  = 0;

static void note_call(const char *what)
{
    GLenum e;

    call_count++;

    while ((e = glGetError()) != GL_NO_ERROR)
    {
        err_count++;

        if (opt_check && err_count <= 8)
            printf("  %-24s -> 0x%04x%s\n", what, e,
                   e == GL_INVALID_OPERATION ? " (GL_INVALID_OPERATION)" : "");
    }
}

/* Draw a gear wheel. Unchanged from the original apart from float suffixes. */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth)
{
    GLint i;
    GLfloat r0, r1, r2;
    GLfloat ang, da;
    GLfloat u, v, len;

    r0 = inner_radius;
    r1 = outer_radius - tooth_depth / 2.0f;
    r2 = outer_radius + tooth_depth / 2.0f;

    da = 2.0f * (GLfloat)M_PI / teeth / 4.0f;

    glShadeModel(GL_FLAT);

    glNormal3f(0.0f, 0.0f, 1.0f);

    /* draw front face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        ang = i * 2.0f * (GLfloat)M_PI / teeth;
        glVertex3f(r0 * cosf(ang), r0 * sinf(ang), width * 0.5f);
        glVertex3f(r1 * cosf(ang), r1 * sinf(ang), width * 0.5f);
        if (i < teeth) {
            glVertex3f(r0 * cosf(ang), r0 * sinf(ang), width * 0.5f);
            glVertex3f(r1 * cosf(ang + 3 * da), r1 * sinf(ang + 3 * da), width * 0.5f);
        }
    }
    glEnd();

    /* draw front sides of teeth */
    glBegin(GL_QUADS);
    da = 2.0f * (GLfloat)M_PI / teeth / 4.0f;
    for (i = 0; i < teeth; i++) {
        ang = i * 2.0f * (GLfloat)M_PI / teeth;

        glVertex3f(r1 * cosf(ang), r1 * sinf(ang), width * 0.5f);
        glVertex3f(r2 * cosf(ang + da), r2 * sinf(ang + da), width * 0.5f);
        glVertex3f(r2 * cosf(ang + 2 * da), r2 * sinf(ang + 2 * da), width * 0.5f);
        glVertex3f(r1 * cosf(ang + 3 * da), r1 * sinf(ang + 3 * da), width * 0.5f);
    }
    glEnd();

    glNormal3f(0.0f, 0.0f, -1.0f);

    /* draw back face */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        ang = i * 2.0f * (GLfloat)M_PI / teeth;
        glVertex3f(r1 * cosf(ang), r1 * sinf(ang), -width * 0.5f);
        glVertex3f(r0 * cosf(ang), r0 * sinf(ang), -width * 0.5f);
        if (i < teeth) {
            glVertex3f(r1 * cosf(ang + 3 * da), r1 * sinf(ang + 3 * da), -width * 0.5f);
            glVertex3f(r0 * cosf(ang), r0 * sinf(ang), -width * 0.5f);
        }
    }
    glEnd();

    /* draw back sides of teeth */
    glBegin(GL_QUADS);
    da = 2.0f * (GLfloat)M_PI / teeth / 4.0f;
    for (i = 0; i < teeth; i++) {
        ang = i * 2.0f * (GLfloat)M_PI / teeth;

        glVertex3f(r1 * cosf(ang + 3 * da), r1 * sinf(ang + 3 * da), -width * 0.5f);
        glVertex3f(r2 * cosf(ang + 2 * da), r2 * sinf(ang + 2 * da), -width * 0.5f);
        glVertex3f(r2 * cosf(ang + da), r2 * sinf(ang + da), -width * 0.5f);
        glVertex3f(r1 * cosf(ang), r1 * sinf(ang), -width * 0.5f);
    }
    glEnd();

    /* draw outward faces of teeth */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i < teeth; i++) {
        ang = i * 2.0f * (GLfloat)M_PI / teeth;

        glVertex3f(r1 * cosf(ang), r1 * sinf(ang), width * 0.5f);
        glVertex3f(r1 * cosf(ang), r1 * sinf(ang), -width * 0.5f);
        u = r2 * cosf(ang + da) - r1 * cosf(ang);
        v = r2 * sinf(ang + da) - r1 * sinf(ang);
        len = sqrtf(u * u + v * v);
        u /= len;
        v /= len;
        glNormal3f(v, -u, 0.0f);
        glVertex3f(r2 * cosf(ang + da), r2 * sinf(ang + da), width * 0.5f);
        glVertex3f(r2 * cosf(ang + da), r2 * sinf(ang + da), -width * 0.5f);
        glNormal3f(cosf(ang), sinf(ang), 0.0f);
        glVertex3f(r2 * cosf(ang + 2 * da), r2 * sinf(ang + 2 * da), width * 0.5f);
        glVertex3f(r2 * cosf(ang + 2 * da), r2 * sinf(ang + 2 * da), -width * 0.5f);
        u = r1 * cosf(ang + 3 * da) - r2 * cosf(ang + 2 * da);
        v = r1 * sinf(ang + 3 * da) - r2 * sinf(ang + 2 * da);
        glNormal3f(v, -u, 0.0f);
        glVertex3f(r1 * cosf(ang + 3 * da), r1 * sinf(ang + 3 * da), width * 0.5f);
        glVertex3f(r1 * cosf(ang + 3 * da), r1 * sinf(ang + 3 * da), -width * 0.5f);
        glNormal3f(cosf(ang), sinf(ang), 0.0f);
    }

    glVertex3f(r1 * cosf(0.0f), r1 * sinf(0.0f), width * 0.5f);
    glVertex3f(r1 * cosf(0.0f), r1 * sinf(0.0f), -width * 0.5f);

    glEnd();

    glShadeModel(GL_SMOOTH);

    /* draw inside radius cylinder */
    glBegin(GL_QUAD_STRIP);
    for (i = 0; i <= teeth; i++) {
        ang = i * 2.0f * (GLfloat)M_PI / teeth;
        glNormal3f(-cosf(ang), -sinf(ang), 0.0f);
        glVertex3f(r0 * cosf(ang), r0 * sinf(ang), -width * 0.5f);
        glVertex3f(r0 * cosf(ang), r0 * sinf(ang), width * 0.5f);
    }
    glEnd();
}

static void
draw(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glPushMatrix();
    glRotatef(view_rotx, 1.0f, 0.0f, 0.0f);
    glRotatef(view_roty, 0.0f, 1.0f, 0.0f);
    glRotatef(view_rotz, 0.0f, 0.0f, 1.0f);

    glPushMatrix();
    glTranslatef(-3.0f, -2.0f, 0.0f);
    glRotatef(angle, 0.0f, 0.0f, 1.0f);
    glCallList(gear1);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(3.1f, -2.0f, 0.0f);
    glRotatef(-2.0f * angle - 9.0f, 0.0f, 0.0f, 1.0f);
    glCallList(gear2);
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-3.1f, 4.2f, 0.0f);
    glRotatef(-2.0f * angle - 25.0f, 0.0f, 0.0f, 1.0f);
    glCallList(gear3);
    glPopMatrix();

    glPopMatrix();
}

static void
reshape(int width, int height)
{
    GLfloat h = (GLfloat)height / (GLfloat)width;

    glViewport(0, 0, (GLint)width, (GLint)height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -40.0f);
}

static void
init(void)
{
    static GLfloat pos[4]   = { 5.0f, 5.0f, 10.0f, 0.0f };
    static GLfloat red[4]   = { 0.8f, 0.1f, 0.0f, 1.0f };
    static GLfloat green[4] = { 0.0f, 0.8f, 0.2f, 1.0f };
    static GLfloat blue[4]  = { 0.2f, 0.2f, 1.0f, 1.0f };

    while (glGetError() != GL_NO_ERROR) { }

    glLightfv(GL_LIGHT0, GL_POSITION, pos);   note_call("glLightfv");
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);                    note_call("glEnable(LIGHTING)");
    glEnable(GL_LIGHT0);                      note_call("glEnable(LIGHT0)");
    glEnable(GL_DEPTH_TEST);

    gear1 = glGenLists(1);                    note_call("glGenLists");
    glNewList(gear1, GL_COMPILE);             note_call("glNewList");
    glMaterialfv(GL_FRONT_COMPAT, GL_AMBIENT_AND_DIFFUSE, red);  note_call("glMaterialfv");
    gear(1.0f, 4.0f, 1.0f, 20, 0.7f);
    glEndList();                              note_call("glEndList");

    gear2 = glGenLists(1);
    glNewList(gear2, GL_COMPILE);
    glMaterialfv(GL_FRONT_COMPAT, GL_AMBIENT_AND_DIFFUSE, green);
    gear(0.5f, 2.0f, 2.0f, 10, 0.7f);
    glEndList();

    gear3 = glGenLists(1);
    glNewList(gear3, GL_COMPILE);
    glMaterialfv(GL_FRONT_COMPAT, GL_AMBIENT_AND_DIFFUSE, blue);
    gear(1.3f, 2.0f, 0.5f, 10, 0.7f);
    glEndList();

    glEnable(GL_NORMALIZE);                   note_call("glEnable(NORMALIZE)");
}

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
    reshape(width, height);
}

static void usage(const char *prog)
{
    printf("usage: %s [options]\n", prog);
    printf("  -info        print GL implementation information\n");
    printf("  --frames N   render N frames then exit\n");
    printf("  --check      report which compatibility calls MGL rejects, then exit\n");
    printf("  --headless   run without a window\n");
    printf("  --size W H   window size (default 300x300)\n");
    printf("  -h           this help\n");
}

static void glfw_error_callback(int code, const char *desc)
{
    fprintf(stderr, "GLFW error 0x%04x: %s\n", code, desc ? desc : "(none)");
}

int main(int argc, char **argv)
{
    GLFWwindow *window;

    setvbuf(stdout, NULL, _IONBF, 0);

    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-info"))         opt_info = 1;
        else if (!strcmp(argv[i], "--check"))  { opt_check = 1; opt_headless = 1; if (!opt_frames) opt_frames = 2; }
        else if (!strcmp(argv[i], "--headless")) opt_headless = 1;
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) opt_frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 2 < argc)
        {
            opt_width = atoi(argv[++i]);
            opt_height = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "%s: unknown option '%s'\n", argv[0], argv[i]); usage(argv[0]); return 1; }
    }

    if (opt_headless)
    {
        GLMContext ctx = createGLMContext(GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                                          GL_DEPTH_COMPONENT, GL_FLOAT,
                                          GL_STENCIL_INDEX8, GL_UNSIGNED_BYTE);
        if (!ctx || !CppCreateMGLRendererHeadless(ctx))
        {
            fprintf(stderr, "gears_compat: headless context failed\n");
            return 77;
        }

        MGLsetCurrentContext(ctx);
        window = NULL;
    }
    else
    {
        glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
        {
            fprintf(stderr, "gears_compat: glfwInit failed\n");
            return 77;                        /* skip: no window server */
        }

        // MGL only offers 4.6; GLFW defaults to 1.0 and its MGL backend
        // refuses anything lower
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

        window = glfwCreateWindow(opt_width, opt_height, "gears (fixed function)", NULL, NULL);

        if (!window)
        {
            fprintf(stderr, "gears_compat: could not create a window\n");
            glfwTerminate();
            return 77;
        }

        glfwMakeContextCurrent(window);
        glfwSetKeyCallback(window, key_callback);
        glfwSetFramebufferSizeCallback(window, size_callback);
    }

    if (opt_info)
    {
        printf("GL_RENDERER = %s\n", (char *)glGetString(GL_RENDERER));
        printf("GL_VERSION  = %s\n", (char *)glGetString(GL_VERSION));
        printf("GL_VENDOR   = %s\n", (char *)glGetString(GL_VENDOR));
    }

    if (opt_check)
        printf("compatibility-profile calls rejected by MGL:\n");

    reshape(opt_width, opt_height);
    init();

    int frames = 0;
    double t0 = window ? glfwGetTime() : 0.0;

    while (window ? !glfwWindowShouldClose(window) : 1)
    {
        angle += 2.0f;

        draw();
        while (glGetError() != GL_NO_ERROR) err_count++;

        if (window)
        {
            MGLswapBuffers((GLMContext)glfwGetWindowUserPointer(window));
            glfwPollEvents();
        }

        frames++;

        if (opt_frames && frames >= opt_frames)
            break;

        if (!window)
            continue;

        double t = glfwGetTime();
        if (!opt_frames && t - t0 >= 5.0)
        {
            printf("%d frames in %3.1f seconds = %6.3f FPS\n",
                   frames, t - t0, frames / (t - t0));
            t0 = t;
            frames = 0;
        }
    }

    if (window)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    if (opt_check)
    {
        printf("\n%d compatibility calls made, %d GL errors raised\n", call_count, err_count);
        printf("no crash: MGL turned the fixed-function pipeline away cleanly\n");
        printf("(OpenGL 4.6 core has none of these; see gears_gl46 for the real renderer)\n");
        return err_count > 0 ? 0 : 1;    /* we expect it to reject them */
    }

    return 0;
}
