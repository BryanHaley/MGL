/*
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * The spinning triangle again, with SDL2 for the window. Written the way any
 * SDL2 program asks for a 4.1 core context, and MooGL answers with 4.6.
 * --frames N closes after N frames, and fails if the triangle didn't draw.
 */

#include <GL/glcorearb.h>
#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *vertexSource =
    "#version 410 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec3 color;\n"
    "uniform float angle;\n"
    "out vec3 vColor;\n"
    "void main() {\n"
    "    mat2 spin = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));\n"
    "    gl_Position = vec4(spin * position, 0.0, 1.0);\n"
    "    vColor = color;\n"
    "}\n";

static const char *fragmentSource =
    "#version 410 core\n"
    "in vec3 vColor;\n"
    "out vec4 fragColor;\n"
    "void main() { fragColor = vec4(vColor, 1.0); }\n";

static GLuint compile(GLenum stage, const char *source)
{
    GLuint shader = glCreateShader(stage);
    GLint ok = 0;
    char log[1024];

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        glGetShaderInfoLog(shader, sizeof log, NULL, log);
        fprintf(stderr, "shader: %s\n", log);
        exit(1);
    }

    return shader;
}

int main(int argc, char **argv)
{
    int frames = argc > 2 && !strcmp(argv[1], "--frames") ? atoi(argv[2]) : 0;
    int status = 0;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

    SDL_Window *window = SDL_CreateWindow("MooGL SDL2 triangle", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_GLContext context = window ? SDL_GL_CreateContext(window) : NULL;

    if (!context) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);
    printf("%s, %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    const float vertices[] = {
         0.0f,  0.6f,  1.0f, 0.3f, 0.3f,
        -0.6f, -0.4f,  0.3f, 1.0f, 0.3f,
         0.6f, -0.4f,  0.3f, 0.3f, 1.0f,
    };

    GLuint buffer, vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(2 * sizeof(float)));

    GLuint program = glCreateProgram();
    glAttachShader(program, compile(GL_VERTEX_SHADER, vertexSource));
    glAttachShader(program, compile(GL_FRAGMENT_SHADER, fragmentSource));
    glLinkProgram(program);
    GLint angle = glGetUniformLocation(program, "angle");

    for (int frame = 0, running = 1; running && (!frames || frame < frames); frame++) {
        SDL_Event event;
        int width, height;

        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT)
                running = 0;

        SDL_GL_GetDrawableSize(window, &width, &height);
        glViewport(0, 0, width, height);

        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glUniform1f(angle, SDL_GetTicks() / 1000.0f);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // the centre of the window is inside the triangle at any angle
        if (frames && frame == frames - 1) {
            unsigned char pixel[4];
            glReadPixels(width / 2, height / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            if (pixel[0] < 60 && pixel[1] < 60 && pixel[2] < 60) {
                fprintf(stderr, "the triangle didn't draw: centre is %d %d %d\n", pixel[0], pixel[1], pixel[2]);
                status = 1;
            }
        }

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}
