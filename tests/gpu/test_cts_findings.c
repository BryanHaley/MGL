/*
 * test_cts_findings.c
 * MGL
 *
 * Regressions for bugs the Khronos CTS found that this suite did not. Each one
 * gated a large group of CTS tests, so they are worth pinning here where they
 * are cheap to run.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- glDeleteShader defers while the shader is attached ---------- */

GPU_TEST(cts_findings, delete_shader_defers_while_attached)
{
    // GL 4.6 section 7.2: a shader attached to a program is flagged for
    // deletion, not deleted. The CTS uses create/attach/delete/source/compile
    // as its ordinary idiom, so destroying it early failed the link every time.
    GLuint p = glCreateProgram();
    GLuint s = glCreateShader(GL_VERTEX_SHADER);
    const char *src =
        "#version 460 core\n"
        "void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";

    glAttachShader(p, s);
    glDeleteShader(s);

    CHECK_EQ_INT(glIsShader(s), GL_TRUE);

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    {
        GLint compiled = GL_FALSE;
        glGetShaderiv(s, GL_COMPILE_STATUS, &compiled);
        CHECK_EQ_INT(compiled, GL_TRUE);
    }

    glDeleteProgram(p);
}

GPU_TEST(cts_findings, deleted_shader_name_dies_on_detach)
{
    GLuint p = glCreateProgram();
    GLuint s = glCreateShader(GL_VERTEX_SHADER);

    glAttachShader(p, s);
    glDeleteShader(s);
    CHECK_EQ_INT(glIsShader(s), GL_TRUE);      // still attached

    glDetachShader(p, s);
    CHECK_EQ_INT(glIsShader(s), GL_FALSE);     // last reference gone
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
}

GPU_TEST(cts_findings, delete_shader_not_attached_dies_at_once)
{
    GLuint s = glCreateShader(GL_FRAGMENT_SHADER);

    CHECK_EQ_INT(glIsShader(s), GL_TRUE);
    glDeleteShader(s);
    CHECK_EQ_INT(glIsShader(s), GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- clip distances through the non-indexed enables ---------- */

GPU_TEST(cts_findings, clip_distance_enables_without_the_indexed_call)
{
    // glEnable(CLIP_DISTANCEi) is the same state as glEnablei(CLIP_DISTANCE0, i).
    // Only the indexed form worked, so every CTS clipping test took an
    // INVALID_ENUM on the first line.
    GLint maxcd = 0;
    int i;

    glGetIntegerv(GL_MAX_CLIP_DISTANCES, &maxcd);
    CHECK(maxcd >= 8);

    for (i = 0; i < 8; i++)
    {
        glEnable((GLenum)(GL_CLIP_DISTANCE0 + i));
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(glIsEnabled((GLenum)(GL_CLIP_DISTANCE0 + i)), GL_TRUE);
    }

    for (i = 0; i < 8; i++)
    {
        glDisable((GLenum)(GL_CLIP_DISTANCE0 + i));
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(glIsEnabled((GLenum)(GL_CLIP_DISTANCE0 + i)), GL_FALSE);
    }
}

GPU_TEST(cts_findings, clip_distance_agrees_with_the_indexed_call)
{
    glEnable(GL_CLIP_DISTANCE3);
    CHECK_EQ_INT(glIsEnabledi(GL_CLIP_DISTANCE0, 3), GL_TRUE);

    glDisablei(GL_CLIP_DISTANCE0, 3);
    CHECK_EQ_INT(glIsEnabled(GL_CLIP_DISTANCE3), GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}
