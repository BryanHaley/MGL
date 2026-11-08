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
 * A spinning triangle in OpenGL 4.6 core on MooGL: GLFW for the window,
 * direct state access for the buffers, and GL called directly.
 */

#include <GL/glcorearb.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *vertexSource =
    "#version 460 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec3 color;\n"
    "layout(location = 0) uniform float angle;\n"
    "out vec3 vColor;\n"
    "void main() {\n"
    "    mat2 spin = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));\n"
    "    gl_Position = vec4(spin * position, 0.0, 1.0);\n"
    "    vColor = color;\n"
    "}\n";

static const char *fragmentSource =
    "#version 460 core\n"
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
    // --frames N closes the window after N frames, and fails if the triangle didn't draw
    int frames = argc > 2 && !strcmp(argv[1], "--frames") ? atoi(argv[2]) : 0;
    int status = 0;

    if (!glfwInit())
        return 1;

    // MooGL offers OpenGL 4.6 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(640, 480, "MooGL triangle", NULL, NULL);

    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    printf("%s, %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    const float vertices[] = {
        //  x      y      r     g     b
         0.0f,  0.6f,  1.0f, 0.3f, 0.3f,
        -0.6f, -0.4f,  0.3f, 1.0f, 0.3f,
         0.6f, -0.4f,  0.3f, 0.3f, 1.0f,
    };

    GLuint buffer, vao;
    glCreateBuffers(1, &buffer);
    glNamedBufferStorage(buffer, sizeof vertices, vertices, 0);

    glCreateVertexArrays(1, &vao);
    glVertexArrayVertexBuffer(vao, 0, buffer, 0, 5 * sizeof(float));
    glEnableVertexArrayAttrib(vao, 0);
    glEnableVertexArrayAttrib(vao, 1);
    glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
    glVertexArrayAttribBinding(vao, 0, 0);
    glVertexArrayAttribBinding(vao, 1, 0);

    GLuint program = glCreateProgram();
    glAttachShader(program, compile(GL_VERTEX_SHADER, vertexSource));
    glAttachShader(program, compile(GL_FRAGMENT_SHADER, fragmentSource));
    glLinkProgram(program);

    for (int frame = 0; !glfwWindowShouldClose(window) && (!frames || frame < frames); frame++) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glUniform1f(0, (float)glfwGetTime());
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

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return status;
}
