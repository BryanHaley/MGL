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

/* ---------- a zero-sized viewport is legal ---------- */

GPU_TEST(cts_findings, viewport_accepts_a_zero_size)
{
    // GL 4.6 section 13.6.1: only a negative width or height is an error.
    // MGL demanded a positive one, so gluStateReset threw on a headless
    // render target and took every CTS run with it after the first test.
    GLint vp[4] = {0};

    mgl_drain_errors();

    glViewport(0, 0, 0, 0);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetIntegerv(GL_VIEWPORT, vp);
    CHECK_EQ_INT(0, vp[2]);
    CHECK_EQ_INT(0, vp[3]);

    glViewport(0, 0, -1, 4);
    CHECK_EQ_UINT(GL_INVALID_VALUE, glGetError());

    glViewport(0, 0, 4, -1);
    CHECK_EQ_UINT(GL_INVALID_VALUE, glGetError());

    glViewport(0, 0, 64, 64);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());
}

/* ---------- glDrawBuffer takes GL_BACK ---------- */

GPU_TEST(cts_findings, draw_buffer_accepts_back)
{
    // GL_BACK is what a double-buffered default framebuffer draws to. It was
    // missing from the switch while glReadBuffer had it, so the ordinary case
    // raised GL_INVALID_ENUM.
    mgl_drain_errors();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glDrawBuffer(GL_BACK);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDrawBuffer(GL_FRONT);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDrawBuffer(GL_NONE);
    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glDrawBuffer(0x1234);
    CHECK_EQ_UINT(GL_INVALID_ENUM, glGetError());

    glDrawBuffer(GL_BACK);
    mgl_drain_errors();
}

/* ---------- a shader that will not build is not a GL error ---------- */

GPU_TEST(cts_findings, failed_compile_sets_status_not_error)
{
    // GL 4.6 section 7.1: glCompileShader reports failure through
    // GL_COMPILE_STATUS and the info log. Raising GL_INVALID_OPERATION as well
    // made every negative CTS test fail, because they check glGetError first.
    GLuint s = glCreateShader(GL_VERTEX_SHADER);
    const char *bad = "#version 460 core\nthis is not glsl\n";
    GLint ok = 1;

    mgl_drain_errors();

    glShaderSource(s, 1, &bad, NULL);
    glCompileShader(s);

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    CHECK_EQ_INT(GL_FALSE, ok);

    glDeleteShader(s);
}

GPU_TEST(cts_findings, failed_link_sets_status_not_error)
{
    // Section 7.3 says the same for glLinkProgram. 2,580 enhanced_layouts
    // cases turned on this one behaviour.
    GLuint p = glCreateProgram();
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    const char *src =
        "#version 460 core\n"
        "layout(std140) uniform B { layout(offset = 3) vec4 v; } b;\n"
        "void main() { gl_Position = b.v; }\n";
    GLint ok = 1;

    mgl_drain_errors();

    glShaderSource(v, 1, &src, NULL);
    glCompileShader(v);
    glAttachShader(p, v);
    glLinkProgram(p);

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(GL_FALSE, ok);

    glDeleteShader(v);
    glDeleteProgram(p);
}

/* A program that does build still has to link cleanly and raise nothing. */
GPU_TEST(cts_findings, successful_link_raises_nothing)
{
    GLuint p = glCreateProgram();
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    const char *vs = "#version 460 core\nvoid main(){gl_Position=vec4(0,0,0,1);}\n";
    const char *fs = "#version 460 core\nout vec4 o;void main(){o=vec4(1);}\n";
    GLint ok = 0;

    mgl_drain_errors();

    glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
    glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);

    CHECK_EQ_UINT(GL_NO_ERROR, glGetError());

    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    CHECK_EQ_INT(GL_TRUE, ok);

    glDeleteShader(v);
    glDeleteShader(f);
    glDeleteProgram(p);
}

/* ---------- compile errors that five-stage linking used to hide ---------- */

static GLint compiles(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    GLint ok = GL_FALSE;

    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    glDeleteShader(s);
    return ok;
}

GPU_TEST(cts_findings, output_location_past_the_limit_fails)
{
    GLint comps = 0;
    char src[512];

    glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS, &comps);

    const char *fmt =
        "#version 440 core\n"
        "layout(location = %d) out vec4 v;\n"
        "void main() { v = vec4(1.0); gl_Position = vec4(0.0); }\n";

    snprintf(src, sizeof src, fmt, comps / 4 - 1);
    CHECK_EQ_INT(compiles(GL_VERTEX_SHADER, src), GL_TRUE);

    snprintf(src, sizeof src, fmt, comps / 4);
    CHECK_EQ_INT(compiles(GL_VERTEX_SHADER, src), GL_FALSE);

    // a mat4 takes four locations, so the last one it can start at is limit - 4
    snprintf(src, sizeof src,
             "#version 440 core\n"
             "layout(location = %d) out mat4 m;\n"
             "void main() { m = mat4(1.0); gl_Position = vec4(0.0); }\n", comps / 4 - 3);
    CHECK_EQ_INT(compiles(GL_VERTEX_SHADER, src), GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(cts_findings, packed_block_cannot_place_members)
{
    // offset and align only work with std140 and std430
    CHECK_EQ_INT(compiles(GL_FRAGMENT_SHADER,
        "#version 440 core\n"
        "layout(packed) uniform Block { layout(offset = 16) vec4 b; } blk;\n"
        "out vec4 c;\n"
        "void main() { c = blk.b; }\n"), GL_FALSE);

    CHECK_EQ_INT(compiles(GL_FRAGMENT_SHADER,
        "#version 440 core\n"
        "layout(shared) uniform Block { vec4 b; } blk;\n"
        "out vec4 c;\n"
        "void main() { c = blk.b; }\n"), GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}
