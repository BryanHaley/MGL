/*
 * harness.h
 * MGL
 *
 * Headless GL context plus the bits GPU tests need to check rendered pixels.
 */

#ifndef mgl_harness_h
#define mgl_harness_h

#define GL_GLEXT_PROTOTYPES 1
#include "glcorearb.h"

#ifdef __cplusplus
extern "C" {
#endif

// Brings up one headless context for the whole run. 0 means no GPU available.
int  mgl_harness_init(void);
void mgl_harness_shutdown(void);

// Drops objects and returns GL state to defaults between tests.
void mgl_harness_reset(void);

// An offscreen colour (and optionally depth/stencil) target to draw into.
typedef struct {
    GLuint fbo;
    GLuint color;
    GLuint depth_stencil;
    GLsizei width;
    GLsizei height;
} MGLTestTarget;

int  mgl_target_create(MGLTestTarget *t, GLsizei w, GLsizei h,
                       GLenum color_internalformat, int want_depth_stencil);
void mgl_target_destroy(MGLTestTarget *t);
void mgl_target_bind(const MGLTestTarget *t);

// Reads the whole target as RGBA8, bottom-up like glReadPixels. Caller frees.
unsigned char *mgl_read_rgba8(const MGLTestTarget *t);

// x,y are GL coordinates (origin bottom-left).
void mgl_pixel_at(const unsigned char *px, const MGLTestTarget *t,
                  int x, int y, unsigned char out_rgba[4]);

// Compiles and links; returns 0 and fills log on failure.
GLuint mgl_build_program(const char *vs_src, const char *fs_src, char *log, int log_size);
GLuint mgl_build_compute_program(const char *cs_src, char *log, int log_size);

// Drains and returns the first GL error, or GL_NO_ERROR.
GLenum mgl_drain_errors(void);

// A VAO with one vec2 attribute at location 0 covering the viewport as 2 triangles.
GLuint mgl_fullscreen_quad(GLuint *out_vbo);

#ifdef __cplusplus
}
#endif

#endif /* mgl_harness_h */
