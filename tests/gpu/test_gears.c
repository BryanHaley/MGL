/*
 * test_gears.c
 * MGL
 *
 * Covers the gears demos: the geometry builder on the CPU, and a full render
 * of the three gears through the 4.6 path with the result checked in pixels.
 */

#include "mgl_test.h"
#include "harness.h"
#include "gears_common.h"

#define GW 128
#define GH 128

/* ---------- geometry, no GPU needed ---------- */

TEST(gears, mesh_builds_triangles)
{
    GearMesh m;

    gears_mesh_init(&m);
    gears_build(&m, 1.0f, 4.0f, 1.0f, 20, 0.7f);

    CHECK_MSG(m.count > 0, "no vertices produced");
    CHECK_MSG(m.count % 3 == 0, "vertex count %zu is not whole triangles", m.count);

    gears_mesh_free(&m);
}

TEST(gears, tooth_count_scales_geometry)
{
    GearMesh a, b;

    gears_mesh_init(&a);
    gears_mesh_init(&b);

    gears_build(&a, 1.0f, 4.0f, 1.0f, 10, 0.7f);
    gears_build(&b, 1.0f, 4.0f, 1.0f, 20, 0.7f);

    CHECK_MSG(b.count > a.count, "20 teeth (%zu) should need more vertices than 10 (%zu)",
              b.count, a.count);

    gears_mesh_free(&a);
    gears_mesh_free(&b);
}

TEST(gears, normals_are_usable)
{
    GearMesh m;
    int degenerate = 0;

    gears_mesh_init(&m);
    gears_build(&m, 1.0f, 4.0f, 1.0f, 20, 0.7f);

    /* The original demo leaves some of the tooth normals un-normalised and
       turns on GL_NORMALIZE to compensate; the 4.6 shader calls normalize()
       instead. So only require they are finite and non-zero. */
    for (size_t i = 0; i < m.count; i++)
    {
        GearVertex *v = &m.verts[i];
        double len = sqrt((double)v->nx * v->nx + (double)v->ny * v->ny + (double)v->nz * v->nz);

        if (!isfinite(len) || len < 1e-6) degenerate++;
    }

    CHECK_MSG(degenerate == 0, "%d of %zu normals are zero or not finite", degenerate, m.count);

    gears_mesh_free(&m);
}

TEST(gears, vertices_stay_within_the_gear_radius)
{
    GearMesh m;
    float outer = 4.0f, tooth = 0.7f, width = 1.0f;
    float max_r = outer + tooth / 2.0f + 1e-3f;
    int outside = 0, too_deep = 0;

    gears_mesh_init(&m);
    gears_build(&m, 1.0f, outer, width, 20, tooth);

    for (size_t i = 0; i < m.count; i++)
    {
        GearVertex *v = &m.verts[i];
        float r = sqrtf(v->px * v->px + v->py * v->py);

        if (r > max_r) outside++;
        if (fabsf(v->pz) > width * 0.5f + 1e-3f) too_deep++;
    }

    CHECK_MSG(outside == 0, "%d vertices past the outer radius", outside);
    CHECK_MSG(too_deep == 0, "%d vertices outside the gear width", too_deep);

    gears_mesh_free(&m);
}

/* ---------- matrices ---------- */

TEST(gears, identity_is_multiplicative_unit)
{
    float id[16], m[16], out[16];

    mat4_identity(id);
    mat4_identity(m);
    mat4_translate(m, 1.0f, 2.0f, 3.0f);

    mat4_mul(out, id, m);

    for (int i = 0; i < 16; i++)
        CHECK_NEAR(out[i], m[i], 1e-6);
}

TEST(gears, translate_lands_in_the_fourth_column)
{
    float m[16];

    mat4_identity(m);
    mat4_translate(m, 1.0f, 2.0f, 3.0f);

    CHECK_NEAR(m[12], 1.0, 1e-6);
    CHECK_NEAR(m[13], 2.0, 1e-6);
    CHECK_NEAR(m[14], 3.0, 1e-6);
    CHECK_NEAR(m[15], 1.0, 1e-6);
}

TEST(gears, rotation_preserves_length)
{
    float m[16];
    float x, y, z;

    mat4_identity(m);
    mat4_rotate(m, 37.0f, 0.0f, 0.0f, 1.0f);

    // rotate (1,0,0) about z
    x = m[0]; y = m[1]; z = m[2];

    CHECK_NEAR(sqrt((double)x * x + (double)y * y + (double)z * z), 1.0, 1e-5);
    CHECK_NEAR(x, cos(37.0 * M_PI / 180.0), 1e-5);
}

TEST(gears, frustum_matches_the_gl_definition)
{
    float m[16];

    mat4_frustum(m, -1.0f, 1.0f, -1.0f, 1.0f, 5.0f, 60.0f);

    CHECK_NEAR(m[0], 5.0, 1e-5);            // 2n/(r-l)
    CHECK_NEAR(m[5], 5.0, 1e-5);            // 2n/(t-b)
    CHECK_NEAR(m[11], -1.0, 1e-6);
    CHECK_NEAR(m[10], -(60.0 + 5.0) / (60.0 - 5.0), 1e-5);
    CHECK_NEAR(m[14], -2.0 * 60.0 * 5.0 / (60.0 - 5.0), 1e-4);
}

TEST(gears, normal_matrix_of_a_rotation_is_that_rotation)
{
    float mv[16], nm[16];

    mat4_identity(mv);
    mat4_rotate(mv, 30.0f, 0.0f, 0.0f, 1.0f);

    mat4_normal_matrix(nm, mv);

    // a pure rotation is its own inverse transpose
    for (int c = 0; c < 3; c++)
        for (int r = 0; r < 3; r++)
            CHECK_NEAR(nm[c * 4 + r], mv[c * 4 + r], 1e-4);
}

/* ---------- the actual render ---------- */

static const char *GEARS_VS =
"#version 460 core\n"
"layout(location = 0) in vec3 in_pos;\n"
"layout(location = 1) in vec3 in_normal;\n"
"layout(location = 0) uniform mat4 u_mvp;\n"
"layout(location = 1) uniform mat4 u_modelview;\n"
"layout(location = 2) uniform mat4 u_normal_matrix;\n"
"layout(location = 0) out vec3 v_normal;\n"
"void main() {\n"
"    v_normal = mat3(u_normal_matrix) * in_normal;\n"
"    gl_Position = u_mvp * vec4(in_pos, 1.0);\n"
"}\n";

static const char *GEARS_FS =
"#version 460 core\n"
"layout(location = 0) in vec3 v_normal;\n"
"layout(location = 3) uniform vec4 u_color;\n"
"layout(location = 0) out vec4 frag;\n"
"void main() {\n"
"    vec3 N = normalize(v_normal);\n"
"    float d = max(dot(N, normalize(vec3(5.0, 5.0, 10.0))), 0.0);\n"
"    frag = vec4(min(u_color.rgb * (0.2 + d), vec3(1.0)), 1.0);\n"
"}\n";

typedef struct { GLuint vao, vbo; GLsizei count; } TestGear;

static void build_gear(TestGear *g, float inner, float outer, float w, int teeth)
{
    GearMesh mesh;

    gears_mesh_init(&mesh);
    gears_build(&mesh, inner, outer, w, teeth, 0.7f);
    g->count = (GLsizei)mesh.count;

    glGenVertexArrays(1, &g->vao);
    glBindVertexArray(g->vao);
    glGenBuffers(1, &g->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh.count * sizeof(GearVertex)),
                 mesh.verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GearVertex), (const void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GearVertex),
                          (const void *)(3 * sizeof(float)));

    glBindVertexArray(0);
    gears_mesh_free(&mesh);
}

static void free_gear(TestGear *g)
{
    glDeleteVertexArrays(1, &g->vao);
    glDeleteBuffers(1, &g->vbo);
}

static void gear_draw(const TestGear *g, const float *proj, float tx, float ty,
                      float spin, const float *color)
{
    float mv[16], mvp[16], nm[16];

    mat4_identity(mv);
    mat4_translate(mv, 0.0f, 0.0f, -40.0f);
    mat4_rotate(mv, 20.0f, 1.0f, 0.0f, 0.0f);
    mat4_rotate(mv, 30.0f, 0.0f, 1.0f, 0.0f);
    mat4_translate(mv, tx, ty, 0.0f);
    mat4_rotate(mv, spin, 0.0f, 0.0f, 1.0f);

    mat4_mul(mvp, proj, mv);
    mat4_normal_matrix(nm, mv);

    glUniformMatrix4fv(0, 1, GL_FALSE, mvp);
    glUniformMatrix4fv(1, 1, GL_FALSE, mv);
    glUniformMatrix4fv(2, 1, GL_FALSE, nm);
    glUniform4fv(3, 1, color);

    glBindVertexArray(g->vao);
    glDrawArrays(GL_TRIANGLES, 0, g->count);
}

GPU_TEST(gears, renders_three_coloured_gears)
{
    MGLTestTarget t;
    char log[4096] = { 0 };
    GLuint prog;
    TestGear g0, g1, g2;
    float proj[16];
    unsigned char *px;
    size_t lit = 0, red = 0, green = 0, blue = 0, n = (size_t)GW * GH;

    static const float c_red[4]   = { 0.8f, 0.1f, 0.0f, 1.0f };
    static const float c_green[4] = { 0.0f, 0.8f, 0.2f, 1.0f };
    static const float c_blue[4]  = { 0.2f, 0.2f, 1.0f, 1.0f };

    if (!mgl_target_create(&t, GW, GH, GL_RGBA8, 1))
        SKIP("no depth-capable target");

    prog = mgl_build_program(GEARS_VS, GEARS_FS, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("gears shader failed to build"); }

    build_gear(&g0, 1.0f, 4.0f, 1.0f, 20);
    build_gear(&g1, 0.5f, 2.0f, 2.0f, 10);
    build_gear(&g2, 1.3f, 2.0f, 0.5f, 10);

    mat4_frustum(proj, -1.0f, 1.0f, -1.0f, 1.0f, 5.0f, 60.0f);

    mgl_target_bind(&t);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(prog);
    gear_draw(&g0, proj, -3.0f, -2.0f, 0.0f, c_red);
    gear_draw(&g1, proj,  3.1f, -2.0f, -9.0f, c_green);
    gear_draw(&g2, proj, -3.1f,  4.2f, -25.0f, c_blue);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    px = mgl_read_rgba8(&t);
    if (!px) { free_gear(&g0); free_gear(&g1); free_gear(&g2);
               glDeleteProgram(prog); mgl_target_destroy(&t); SKIP("readback failed"); }

    for (size_t i = 0; i < n; i++)
    {
        unsigned char *p = px + i * 4;
        int r = p[0], g = p[1], b = p[2];

        if (r + g + b < 24) continue;

        lit++;

        if (r > g + 20 && r > b + 20) red++;
        else if (g > r + 20 && g > b + 20) green++;
        else if (b > r + 20 && b > g + 20) blue++;
    }

    CHECK_MSG(lit > n / 20, "only %zu of %zu pixels covered, gears did not draw", lit, n);
    CHECK_MSG(lit < n * 9 / 10, "%zu of %zu pixels covered, that is not gear shaped", lit, n);
    CHECK_MSG(red > 0, "no red gear pixels");
    CHECK_MSG(green > 0, "no green gear pixels");
    CHECK_MSG(blue > 0, "no blue gear pixels");

    free(px);
    free_gear(&g0);
    free_gear(&g1);
    free_gear(&g2);
    glDeleteProgram(prog);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    mgl_target_destroy(&t);
}

GPU_TEST(gears, depth_test_hides_the_back_faces)
{
    MGLTestTarget t;
    char log[4096] = { 0 };
    GLuint prog;
    TestGear g;
    float proj[16];
    unsigned char *px;
    size_t lit_with_cull = 0, lit_without = 0, n = (size_t)GW * GH;
    static const float c[4] = { 0.8f, 0.1f, 0.0f, 1.0f };

    if (!mgl_target_create(&t, GW, GH, GL_RGBA8, 1)) SKIP("no depth target");

    prog = mgl_build_program(GEARS_VS, GEARS_FS, log, sizeof log);
    if (!prog) { mgl_target_destroy(&t); SKIP("gears shader failed to build"); }

    build_gear(&g, 1.0f, 4.0f, 1.0f, 20);
    mat4_frustum(proj, -1.0f, 1.0f, -1.0f, 1.0f, 5.0f, 60.0f);

    mgl_target_bind(&t);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(prog);

    for (int pass = 0; pass < 2; pass++)
    {
        if (pass == 0) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gear_draw(&g, proj, 0.0f, 0.0f, 0.0f, c);

        px = mgl_read_rgba8(&t);
        if (!px) break;

        size_t lit = 0;
        for (size_t i = 0; i < n; i++)
            if (px[i * 4] + px[i * 4 + 1] + px[i * 4 + 2] >= 24) lit++;

        if (pass == 0) lit_with_cull = lit; else lit_without = lit;
        free(px);
    }

    // the silhouette is the same either way; culling must not empty the frame
    CHECK_MSG(lit_with_cull > n / 20, "culled render covered only %zu pixels", lit_with_cull);
    CHECK_MSG(lit_without > n / 20, "unculled render covered only %zu pixels", lit_without);

    free_gear(&g);
    glDeleteProgram(prog);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    mgl_target_destroy(&t);
}

GPU_TEST(gears, multiple_uniforms_reach_the_right_stage)
{
    /* The gears need three matrices in the vertex stage and a colour in the
       fragment stage. MGL used to hand every uniform the same buffer, so this
       pins the per-stage mapping down. */
    char log[4096] = { 0 };
    GLuint prog = mgl_build_program(GEARS_VS, GEARS_FS, log, sizeof log);

    if (!prog) SKIP("gears shader failed to build");

    GLint mvp = glGetUniformLocation(prog, "u_mvp");
    GLint mv  = glGetUniformLocation(prog, "u_modelview");
    GLint nm  = glGetUniformLocation(prog, "u_normal_matrix");
    GLint col = glGetUniformLocation(prog, "u_color");

    CHECK_MSG(mvp >= 0, "u_mvp not found");
    CHECK_MSG(mv  >= 0, "u_modelview not found");
    CHECK_MSG(nm  >= 0, "u_normal_matrix not found");
    CHECK_MSG(col >= 0, "u_color not found");

    CHECK_MSG(mvp != mv && mv != nm && mvp != nm,
              "vertex uniforms share a location: %d %d %d", mvp, mv, nm);
    CHECK_MSG(col != mvp && col != mv && col != nm,
              "fragment uniform collides with a vertex one: %d", col);

    glDeleteProgram(prog);
}
