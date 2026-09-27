/*
 * test_shaders.c
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
 * glGetShaderiv has to answer the ARB_gl_spirv state query. The CTS gl_spirv
 * cases call glShaderBinary and then ask whether the shader is a SPIR-V
 * binary; an INVALID_ENUM here kills every one of them before the test proper
 * begins. GL_SPIR_V_BINARY_ARB and the core GL_SPIR_V_BINARY are the same
 * token (0x9552), so both spellings must be accepted.
 *
 * These need a live context for the shader table, but no rendering.
 */

#include "mgl_test.h"
#include "harness.h"

/* The GLSL path is what a shader has before anything else happens to it. */
GPU_TEST(shader_query, spir_v_binary_is_false_for_a_source_shader)
{
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    GLint state = -1;

    CHECK(shader != 0);

    glGetShaderiv(shader, GL_SPIR_V_BINARY, &state);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);
    CHECK_EQ_INT(state, GL_FALSE);

    /* the ARB spelling has to give the same answer */
    state = -1;
    glGetShaderiv(shader, GL_SPIR_V_BINARY_ARB, &state);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);
    CHECK_EQ_INT(state, GL_FALSE);

    glDeleteShader(shader);
}

/*
 * A SPIR-V module is a stream of 32-bit words that starts with a magic
 * number; mglShaderBinary checks exactly that. A one-word module is enough to
 * put the shader into the SPIR-V-binary state, which is all this test looks
 * at.
 */
GPU_TEST(shader_query, spir_v_binary_is_true_after_shader_binary)
{
    static const unsigned int module[] = { 0x07230203u };
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    GLint state = -1;

    CHECK(shader != 0);

    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V,
                   module, (GLsizei)sizeof module);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);

    glGetShaderiv(shader, GL_SPIR_V_BINARY, &state);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);
    CHECK_EQ_INT(state, GL_TRUE);

    /* the ARB spelling has to give the same answer */
    state = -1;
    glGetShaderiv(shader, GL_SPIR_V_BINARY_ARB, &state);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);
    CHECK_EQ_INT(state, GL_TRUE);

    glDeleteShader(shader);
}

/*
 * ARB_gl_spirv 7.1: "If <shader> was previously associated with a SPIR-V
 * module ... that association is broken. Upon successful completion of this
 * command the SPIR_V_BINARY_ARB state of <shader> is set to FALSE." The CTS
 * positive test checks this in its step 4.
 */
GPU_TEST(shader_query, shader_source_breaks_the_spirv_association)
{
    static const unsigned int module[] = { 0x07230203u };
    static const char *src =
        "#version 460\n"
        "void main() { gl_Position = vec4(0.0); }\n";
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    GLint state = -1;

    CHECK(shader != 0);

    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V,
                   module, (GLsizei)sizeof module);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);

    glGetShaderiv(shader, GL_SPIR_V_BINARY, &state);
    CHECK_EQ_INT(state, GL_TRUE);

    glShaderSource(shader, 1, &src, NULL);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);

    state = -1;
    glGetShaderiv(shader, GL_SPIR_V_BINARY, &state);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);
    CHECK_EQ_INT(state, GL_FALSE);

    glDeleteShader(shader);
}

/*
 * ARB_gl_spirv adds to CompileShader: "An INVALID_OPERATION error is
 * generated if the SPIR_V_BINARY_ARB state of <shader> is TRUE." The CTS
 * error-verification test checks this first.
 */
GPU_TEST(shader_query, compiling_a_spirv_shader_is_refused)
{
    static const unsigned int module[] = { 0x07230203u };
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);

    CHECK(shader != 0);

    glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V,
                   module, (GLsizei)sizeof module);
    CHECK_EQ_INT(glGetError(), GL_NO_ERROR);

    glCompileShader(shader);
    CHECK_EQ_INT(glGetError(), GL_INVALID_OPERATION);

    glDeleteShader(shader);
}
