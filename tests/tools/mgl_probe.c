/*
 * mgl_probe.c
 * MGL
 *
 * Reports what a GL entry point resolves to at runtime. GLFW's MGL backend
 * dlopens "libmoogle.dylib" by bare name and pulls entry points out with dlsym,
 * and libmoogle itself links the system OpenGL framework, so both a real macOS GL
 * and MGL's own symbols exist in the process. This says which one wins.
 */

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#define GL_VERSION        0x1F02
#define GL_RENDERER       0x1F01
#define GL_VENDOR         0x1F00
#define GL_MAJOR_VERSION  0x821B
#define GL_MINOR_VERSION  0x821C

typedef const unsigned char *(*GetStringFn)(unsigned int);
typedef void (*GetIntegervFn)(unsigned int, int *);

static void where(const char *what, void *sym)
{
    Dl_info info;

    if (!sym)
    {
        printf("  %-14s NOT FOUND\n", what);
        return;
    }

    if (dladdr(sym, &info) && info.dli_fname)
        printf("  %-14s %s\n", what, info.dli_fname);
    else
        printf("  %-14s %p (unknown image)\n", what, sym);
}

int main(void)
{
    void *h;
    GetStringFn get_string;
    GetIntegervFn get_integerv;
    int major = -1, minor = -1;

    setvbuf(stdout, NULL, _IONBF, 0);

    printf("\n--- dlopen, the way GLFW's MGL backend does it ---\n");

    h = dlopen("libmoogle.dylib", RTLD_LAZY | RTLD_LOCAL);
    printf("  dlopen         %s\n", h ? "ok" : dlerror());

    if (!h)
    {
        printf("\nGLFW cannot find libmoogle.dylib from this directory.\n"
               "Run from the repo root, where the build drops a libmoogle.dylib symlink.\n");
        return 1;
    }

    get_string   = (GetStringFn)dlsym(h, "glGetString");
    get_integerv = (GetIntegervFn)dlsym(h, "glGetIntegerv");

    printf("\n--- where the entry points come from ---\n");
    where("glGetString", (void *)get_string);
    where("glGetIntegerv", (void *)get_integerv);

    if (!get_string)
        return 1;

    printf("\n--- what they report ---\n");
    printf("  GL_VERSION     %s\n", (const char *)get_string(GL_VERSION));
    printf("  GL_RENDERER    %s\n", (const char *)get_string(GL_RENDERER));
    printf("  GL_VENDOR      %s\n", (const char *)get_string(GL_VENDOR));

    if (get_integerv)
    {
        get_integerv(GL_MAJOR_VERSION, &major);
        get_integerv(GL_MINOR_VERSION, &minor);
        printf("  MAJOR.MINOR    %d.%d\n", major, minor);
    }

    printf("\nGLFW parses GL_VERSION above and refuses the window if it is\n"
           "below the 4.6 the MGL backend requires.\n\n");

    return 0;
}
