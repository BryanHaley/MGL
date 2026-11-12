/*
 * Copyright (C) The MooGL Project
 *
 * test_imgui_viewports.cpp
 * MGL
 *
 * Dear ImGui with multi-viewports: a panel placed outside the main window
 * gets its own window, whose context shares the font texture, buffers and
 * program with the main one. Checks the panel's text is drawn there.
 * Built once for ImGui's GLFW backend and once, with MGL_TEST_SDL2, for its
 * SDL2 backend.
 */

#include <GL/glcorearb.h>

#ifdef MGL_TEST_SDL2
#include <SDL.h>
#include "imgui_impl_sdl2.h"
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#endif

#include "imgui.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;

static void check(bool ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

// Text is white, everything else in the panel is darker
static int brightPixels(void)
{
    GLint vp[4];
    int bright = 0;

    glGetIntegerv(GL_VIEWPORT, vp);
    unsigned char *px = (unsigned char *)malloc((size_t)vp[2] * vp[3] * 4);
    glReadPixels(0, 0, vp[2], vp[3], GL_RGBA, GL_UNSIGNED_BYTE, px);

    for (int i = 0; i < vp[2] * vp[3]; i++)
        if (px[i * 4] > 200 && px[i * 4 + 1] > 200 && px[i * 4 + 2] > 200)
            bright++;

    free(px);
    return bright;
}

#ifdef MGL_TEST_SDL2
static SDL_Window *window;
static SDL_GLContext context;

static bool openWindow(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return false;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    window = SDL_CreateWindow("MooGL ImGui viewports (SDL2)", 100, 100, 400, 300,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    context = window ? SDL_GL_CreateContext(window) : NULL;
    if (!context)
        return false;

    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);
    return ImGui::CreateContext() && ImGui_ImplSDL2_InitForOpenGL(window, context);
}

static bool pollEvents(void)
{
    SDL_Event e;
    bool open = true;

    while (SDL_PollEvent(&e))
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        open &= e.type != SDL_QUIT;
    }

    return open;
}

static void windowRect(int *x, int *y, int *w, int *h)
{
    SDL_GetWindowPosition(window, x, y);
    SDL_GetWindowSize(window, w, h);
}

static void drawableSize(int *w, int *h) { SDL_GL_GetDrawableSize(window, w, h); }
static void platformNewFrame(void) { ImGui_ImplSDL2_NewFrame(); }
static void makeMainCurrent(void) { SDL_GL_MakeCurrent(window, context); }
static void swapMain(void) { SDL_GL_SwapWindow(window); }
static void platformShutdown(void) { ImGui_ImplSDL2_Shutdown(); }

static void closeWindow(void)
{
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
#else
static GLFWwindow *window;

static bool openWindow(void)
{
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    window = glfwCreateWindow(400, 300, "MooGL ImGui viewports (GLFW)", NULL, NULL);
    if (!window)
        return false;

    glfwSetWindowPos(window, 100, 100);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    return ImGui::CreateContext() && ImGui_ImplGlfw_InitForOpenGL(window, true);
}

static bool pollEvents(void)
{
    glfwPollEvents();
    return !glfwWindowShouldClose(window);
}

static void windowRect(int *x, int *y, int *w, int *h)
{
    glfwGetWindowPos(window, x, y);
    glfwGetWindowSize(window, w, h);
}

static void drawableSize(int *w, int *h) { glfwGetFramebufferSize(window, w, h); }
static void platformNewFrame(void) { ImGui_ImplGlfw_NewFrame(); }
static void makeMainCurrent(void) { glfwMakeContextCurrent(window); }
static void swapMain(void) { glfwSwapBuffers(window); }
static void platformShutdown(void) { ImGui_ImplGlfw_Shutdown(); }

static void closeWindow(void)
{
    glfwDestroyWindow(window);
    glfwTerminate();
}
#endif

int main(int argc, char **argv)
{
    int frames = 30;

    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = atoi(argv[++i]);

    bool opened = openWindow();

    check(opened, "a main window");
    if (!opened)
        return 1;

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui_ImplOpenGL3_Init("#version 410");

    int outside = 0, drawn = 0;
    bool errors = false;

    for (int frame = 0; frame < frames && pollEvents(); frame++)
    {
        bool last = frame == frames - 1;
        int x, y, w, h;

        windowRect(&x, &y, &w, &h);

        ImGui_ImplOpenGL3_NewFrame();
        platformNewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2((float)x + 20, (float)y + 20));
        ImGui::Begin("Inside");
        ImGui::Text("Main window, frame %d", frame);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2((float)(x + w + 40), (float)y));
        ImGui::SetNextWindowSize(ImVec2(260, 160));
        ImGui::Begin("Outside");
        for (int i = 0; i < 6; i++)
            ImGui::Text("Shared context line %d", i);
        ImGui::End();

        ImGui::Render();

        int fw, fh;
        drawableSize(&fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        errors |= glGetError() != GL_NO_ERROR;

        ImGui::UpdatePlatformWindows();

        if (!last)
        {
            ImGui::RenderPlatformWindowsDefault();
        }
        else
        {
            // What RenderPlatformWindowsDefault does, with a read before the swap
            ImGuiPlatformIO &pio = ImGui::GetPlatformIO();

            for (int i = 1; i < pio.Viewports.Size; i++)
            {
                ImGuiViewport *vp = pio.Viewports[i];

                if (vp->Flags & ImGuiViewportFlags_IsMinimized)
                    continue;

                outside++;
                pio.Platform_RenderWindow(vp, NULL);
                pio.Renderer_RenderWindow(vp, NULL);
                errors |= glGetError() != GL_NO_ERROR;
                if (brightPixels() > 50)
                    drawn++;
                pio.Platform_SwapBuffers(vp, NULL);
            }
        }

        makeMainCurrent();
        swapMain();
    }

    check(outside == 1, "the outside panel has its own window and context");
    check(outside > 0 && drawn == outside, "its text is drawn with the shared font texture");
    check(!errors, "no GL errors in either context");

    ImGui_ImplOpenGL3_Shutdown();
    platformShutdown();
    ImGui::DestroyContext();

    makeMainCurrent();
    check(glGetError() == GL_NO_ERROR, "no GL errors after closing the panel's context");

    closeWindow();

    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
