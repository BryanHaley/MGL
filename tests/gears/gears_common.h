/*
 * gears_common.h
 * MGL
 *
 * Gear geometry and matrix maths shared by the two gears demos.
 *
 * The original glxgears builds gears with glBegin/glVertex inside display
 * lists. Neither exists in a 4.6 core profile, so the same emission order is
 * captured into a vertex array here and the quads are split into triangles.
 * Keeping the emitter shaped like the old immediate-mode calls makes the two
 * versions easy to compare side by side.
 */

#ifndef gears_common_h
#define gears_common_h

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float px, py, pz;
    float nx, ny, nz;
} GearVertex;

typedef struct {
    GearVertex *verts;
    size_t count;
    size_t cap;

    // scratch for the primitive currently being emitted
    GearVertex *scratch;
    size_t scratch_count;
    size_t scratch_cap;
    int mode;
    float cur_nx, cur_ny, cur_nz;
} GearMesh;

// mirrors the GL enums the original used, without needing a GL header here
#define GEARS_QUADS       0
#define GEARS_QUAD_STRIP  1

void gears_mesh_init(GearMesh *m);
void gears_mesh_free(GearMesh *m);

void gears_begin(GearMesh *m, int mode);
void gears_normal(GearMesh *m, float x, float y, float z);
void gears_vertex(GearMesh *m, float x, float y, float z);
void gears_end(GearMesh *m);

// Same parameters as the original gear() so the three gears match the demo.
void gears_build(GearMesh *m, float inner_radius, float outer_radius,
                 float width, int teeth, float tooth_depth);

/* ---- 4x4 matrices, column major like OpenGL ---- */

void mat4_identity(float *m);
void mat4_copy(float *dst, const float *src);
void mat4_mul(float *out, const float *a, const float *b);      // out = a * b
void mat4_frustum(float *m, float l, float r, float b, float t, float n, float f);
void mat4_translate(float *m, float x, float y, float z);       // m = m * T
void mat4_rotate(float *m, float deg, float x, float y, float z); // m = m * R

// upper-left 3x3 inverse transpose, written as a mat4 so it uploads simply
void mat4_normal_matrix(float *out, const float *modelview);

#ifdef __cplusplus
}
#endif

#endif /* gears_common_h */
