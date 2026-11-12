/*
 * Copyright (C) The MooGL Project
 *
 * test_sdl_share.c
 * MGL
 *
 * SDL_GL_SHARE_WITH_CURRENT_CONTEXT through MooGL's SDL2: the second context
 * sees the first one's objects.
 */

#include <GL/glcorearb.h>
#include <SDL2/SDL.h>

#include <stdio.h>

static int fails;

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 77;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window *window = SDL_CreateWindow("MooGL SDL share test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    SDL_GLContext first = window ? SDL_GL_CreateContext(window) : NULL;

    check(first != NULL, "a context");
    if (!first)
        return 1;

    GLuint tex, vao;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GLContext second = SDL_GL_CreateContext(window);

    check(second != NULL, "a second context sharing with the first");
    if (second)
    {
        SDL_GL_MakeCurrent(window, second);
        check(glIsTexture(tex) && !glIsVertexArray(vao), "it shares the texture and not the vertex array");
        SDL_GL_MakeCurrent(window, first);
        SDL_GL_DeleteContext(second);
    }

    check(glIsTexture(tex), "the texture outlives the second context");
    check(glGetError() == GL_NO_ERROR, "no GL errors");

    SDL_GL_DeleteContext(first);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
