/*
 * test_window.c
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
 * What only shows with a real window, through MooGL's GLFW: the context
 * version a request gets, the default viewport, the window framebuffer's
 * size and stencil, and state kept from one frame to the next.
 */

#include <GL/glcorearb.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char *what)
{
    printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

static const char *vs =
    "#version 330 core\n"
    "void main() {\n"
    "    vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID & 2) * 2.0 - 1.0);\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "}\n";

static const char *fs =
    "#version 330 core\n"
    "out vec4 color;\n"
    "void main() { color = vec4(0.0, 1.0, 0.0, 1.0); }\n";

static GLuint program(void)
{
    GLuint p = glCreateProgram();
    const char *src[2] = { vs, fs };
    GLenum stage[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };

    for (int i = 0; i < 2; i++) {
        GLuint s = glCreateShader(stage[i]);
        glShaderSource(s, 1, &src[i], NULL);
        glCompileShader(s);
        glAttachShader(p, s);
    }
    glLinkProgram(p);
    return p;
}

static void pixel(int x, int y, unsigned char out[4])
{
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

static int green(const unsigned char p[4]) { return p[0] < 30 && p[1] > 220 && p[2] < 30; }
static int red(const unsigned char p[4])   { return p[0] > 220 && p[1] < 30 && p[2] < 30; }

int main(void)
{
    unsigned char a[4], b[4];
    int w, h;

    if (!glfwInit())
        return 77;

    // what most Mac code asks for; MooGL answers with 4.6
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(320, 240, "MooGL window test", NULL, NULL);
    check(window != NULL, "a 3.3 core request makes a context");
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwGetFramebufferSize(window, &w, &h);
    printf("  framebuffer %dx%d, %s\n", w, h, glGetString(GL_VERSION));
    check(!strncmp((const char *)glGetString(GL_VERSION), "4.6", 3), "and gets 4.6");

    GLuint prog = program();
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(prog);

    // no glViewport call: the viewport starts as the whole framebuffer, in pixels
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    pixel(w - 1, h - 1, a);
    pixel(0, 0, b);
    check(green(a) && green(b), "a draw with the default viewport covers the whole framebuffer");
    glfwSwapBuffers(window);

    // GLFW asks for 8 stencil bits by default; mark the left half and draw where it is set
    glViewport(0, 0, w, h);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, w / 2, h);
    glClearStencil(1);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 1, 0xff);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_STENCIL_TEST);
    pixel(w / 4, h / 2, a);
    pixel(3 * w / 4, h / 2, b);
    check(green(a) && red(b), "the window framebuffer has a stencil buffer");
    glfwSwapBuffers(window);

    // a scissor box set in one frame still holds in the next
    glScissor(0, 0, w / 2, h);
    glfwSwapBuffers(window);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    pixel(w / 4, h / 2, a);
    pixel(3 * w / 4, h / 2, b);
    check(green(a) && red(b), "the scissor box carries over to the next frame");
    glfwSwapBuffers(window);

    check(glGetError() == GL_NO_ERROR, "no GL errors");

    // MooGL contexts can't share objects yet, so asking to is refused, not ignored
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *shared = glfwCreateWindow(64, 64, "shared", NULL, window);
    check(shared == NULL, "a share request is refused");
    if (shared)
        glfwDestroyWindow(shared);

    glfwTerminate();
    printf("%s\n", fails ? "FAILED" : "passed");
    return fails ? 1 : 0;
}
