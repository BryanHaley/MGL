/*
 * test_tier2.c
 * MGL
 *
 * Query objects, KHR_debug, the program interface query family and the
 * transform feedback object model.
 */

#include "mgl_test.h"
#include "harness.h"
#include <string.h>

static const char *VS_IO =
    "#version 460 core\n"
    "layout(location = 0) in vec2 pos;\n"
    "layout(location = 0) out vec4 v_col;\n"
    "layout(location = 0) uniform vec4 u_col;\n"
    "void main() { v_col = u_col; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *FS_IO =
    "#version 460 core\n"
    "layout(location = 0) in vec4 v_col;\n"
    "layout(location = 0) out vec4 frag;\n"
    "void main() { frag = v_col; }\n";

/* ---------- query objects ---------- */

GPU_TEST(query, gen_delete_and_is)
{
    GLuint q[3] = { 0 };

    glGenQueries(3, q);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(q[0] != 0 && q[1] != 0 && q[2] != 0);

    // names must be distinct
    CHECK(q[0] != q[1] && q[1] != q[2] && q[0] != q[2]);

    // a reserved name is not an object until it is used
    CHECK_EQ_INT(glIsQuery(q[0]), GL_FALSE);

    glBeginQuery(GL_SAMPLES_PASSED, q[0]);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(glIsQuery(q[0]), GL_TRUE);
    glEndQuery(GL_SAMPLES_PASSED);

    glDeleteQueries(3, q);
    CHECK_EQ_INT(glIsQuery(q[0]), GL_FALSE);
}

GPU_TEST(query, create_sets_the_target)
{
    GLuint q = 0;
    GLint v = -1;

    glCreateQueries(GL_SAMPLES_PASSED, 1, &q);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(glIsQuery(q), GL_TRUE);

    // wrong target on an object that already has one
    glBeginQuery(GL_PRIMITIVES_GENERATED, q);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &v);
    CHECK_EQ_INT(v, 0);

    glDeleteQueries(1, &q);
}

GPU_TEST(query, current_query_tracks_begin_and_end)
{
    GLuint q = 0;
    GLint v = -1;

    glGenQueries(1, &q);

    glBeginQuery(GL_SAMPLES_PASSED, q);
    glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &v);
    CHECK_EQ_INT(v, (GLint)q);

    glEndQuery(GL_SAMPLES_PASSED);
    glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &v);
    CHECK_EQ_INT(v, 0);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glDeleteQueries(1, &q);
}

GPU_TEST(query, result_is_unavailable_until_end)
{
    GLuint q = 0;
    GLuint avail = 99;

    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);

    // still running, so reading it is an error
    glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &avail);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glEndQuery(GL_SAMPLES_PASSED);

    glGetQueryObjectuiv(q, GL_QUERY_RESULT_AVAILABLE, &avail);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_UINT(avail, 1u);

    glDeleteQueries(1, &q);
}

GPU_TEST(query, rejects_nested_and_duplicate_begins)
{
    GLuint a = 0, b = 0;

    glGenQueries(1, &a);
    glGenQueries(1, &b);

    glBeginQuery(GL_SAMPLES_PASSED, a);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // another query on the same target
    glBeginQuery(GL_SAMPLES_PASSED, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glEndQuery(GL_SAMPLES_PASSED);

    // ending with nothing running
    glEndQuery(GL_SAMPLES_PASSED);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteQueries(1, &a);
    glDeleteQueries(1, &b);
}

GPU_TEST(query, timestamp_is_monotonic_and_rejects_begin)
{
    GLuint a = 0, b = 0;
    GLuint64 t0 = 0, t1 = 0;

    glGenQueries(1, &a);
    glGenQueries(1, &b);

    glBeginQuery(GL_TIMESTAMP, a);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glQueryCounter(a, GL_TIMESTAMP);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glQueryCounter(b, GL_TIMESTAMP);

    glGetQueryObjectui64v(a, GL_QUERY_RESULT, &t0);
    glGetQueryObjectui64v(b, GL_QUERY_RESULT, &t1);

    CHECK(t0 > 0);
    CHECK_MSG(t1 >= t0, "timestamps went backwards");

    glQueryCounter(a, GL_SAMPLES_PASSED);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteQueries(1, &a);
    glDeleteQueries(1, &b);
}

GPU_TEST(query, indexed_streams_are_range_checked)
{
    GLuint q = 0;
    GLint v = 0;

    glGenQueries(1, &q);

    glBeginQueryIndexed(GL_PRIMITIVES_GENERATED, 9999, q);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // occlusion has one stream only
    glBeginQueryIndexed(GL_SAMPLES_PASSED, 2, q);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBeginQueryIndexed(GL_PRIMITIVES_GENERATED, 2, q);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetQueryIndexediv(GL_PRIMITIVES_GENERATED, 2, GL_CURRENT_QUERY, &v);
    CHECK_EQ_INT(v, (GLint)q);

    glEndQueryIndexed(GL_PRIMITIVES_GENERATED, 2);
    glDeleteQueries(1, &q);
}

GPU_TEST(query, deleting_a_running_query_clears_it)
{
    GLuint q = 0;
    GLint v = -1;

    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glDeleteQueries(1, &q);

    glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &v);
    CHECK_EQ_INT(v, 0);

    // the target must be usable again
    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glEndQuery(GL_SAMPLES_PASSED);
    glDeleteQueries(1, &q);
}

GPU_TEST(query, writes_a_result_into_a_buffer)
{
    GLuint q = 0, b = 0;
    GLuint readback = 0xABCD;

    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glEndQuery(GL_SAMPLES_PASSED);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);

    glGetQueryBufferObjectuiv(q, b, GL_QUERY_RESULT_AVAILABLE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof readback, &readback);
    CHECK_EQ_UINT(readback, 1u);

    // past the end
    glGetQueryBufferObjectuiv(q, b, GL_QUERY_RESULT, 1024);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
    glDeleteQueries(1, &q);
}

GPU_TEST(query, rejects_bad_arguments)
{
    GLuint q = 0;
    GLint v = 0;

    glGenQueries(-1, &q);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBeginQuery(0x9999, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetQueryiv(GL_SAMPLES_PASSED, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetQueryObjectiv(999999, GL_QUERY_RESULT, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- KHR_debug ---------- */

GPU_TEST(khr_debug, insert_then_read_back_in_order)
{
    GLenum src[4] = { 0 }, typ[4] = { 0 }, sev[4] = { 0 };
    GLuint ids[4] = { 0 };
    GLsizei lens[4] = { 0 };
    char log[512] = { 0 };
    GLuint got;

    // a non-debug context generates no messages until this is on
    glEnable(GL_DEBUG_OUTPUT);

    glDebugMessageCallback(NULL, NULL);

    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 11,
                         GL_DEBUG_SEVERITY_LOW, -1, "first");
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 22,
                         GL_DEBUG_SEVERITY_LOW, -1, "second");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    got = glGetDebugMessageLog(4, sizeof log, src, typ, ids, sev, lens, log);
    CHECK_EQ_UINT(got, 2u);

    // oldest first
    CHECK_EQ_UINT(ids[0], 11u);
    CHECK_EQ_UINT(ids[1], 22u);
    CHECK_STR_EQ(log, "first");
    CHECK_STR_EQ(log + lens[0], "second");
    CHECK_EQ_UINT(src[0], GL_DEBUG_SOURCE_APPLICATION);

    // the log is drained
    CHECK_EQ_UINT(glGetDebugMessageLog(4, sizeof log, src, typ, ids, sev, lens, log), 0u);
}

static int g_cb_seen;
static GLuint g_cb_last_id;
static GLuint g_cb_token = 0xFEED;
static int g_cb_user_ok;

static void debugCb(GLenum source, GLenum type, GLuint id, GLenum severity,
                    GLsizei length, const GLchar *message, const void *userParam)
{
    (void)source; (void)type; (void)severity; (void)length; (void)message;

    g_cb_seen++;
    g_cb_last_id = id;
    g_cb_user_ok = (userParam == &g_cb_token);
}

GPU_TEST(khr_debug, callback_bypasses_the_queue)
{
    GLenum src[2], typ[2], sev[2];
    GLuint ids[2];
    GLsizei lens[2];
    char log[256];

    // a non-debug context generates no messages until this is on
    glEnable(GL_DEBUG_OUTPUT);

    g_cb_seen = 0;
    g_cb_last_id = 0;
    g_cb_user_ok = 0;

    glDebugMessageCallback(debugCb, &g_cb_token);
    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 77,
                         GL_DEBUG_SEVERITY_NOTIFICATION, -1, "hello");

    CHECK_EQ_INT(g_cb_seen, 1);
    CHECK_EQ_UINT(g_cb_last_id, 77u);
    CHECK_MSG(g_cb_user_ok, "user param did not reach the callback");

    // with a callback installed nothing should have queued
    glDebugMessageCallback(NULL, NULL);
    CHECK_EQ_UINT(glGetDebugMessageLog(2, sizeof log, src, typ, ids, sev, lens, log), 0u);
}

GPU_TEST(khr_debug, groups_push_and_pop)
{
    GLenum src[4], typ[4], sev[4];
    GLuint ids[4];
    GLsizei lens[4];
    char log[512];

    // a non-debug context generates no messages until this is on
    glEnable(GL_DEBUG_OUTPUT);

    glDebugMessageCallback(NULL, NULL);

    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 5, -1, "group");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPopDebugGroup();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // one push message and one pop message
    CHECK_EQ_UINT(glGetDebugMessageLog(4, sizeof log, src, typ, ids, sev, lens, log), 2u);
    CHECK_EQ_UINT(typ[0], GL_DEBUG_TYPE_PUSH_GROUP);
    CHECK_EQ_UINT(typ[1], GL_DEBUG_TYPE_POP_GROUP);

    // popping an empty stack
    glPopDebugGroup();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_STACK_UNDERFLOW);
}

GPU_TEST(khr_debug, rejects_bad_arguments)
{
    GLuint ids[1] = { 1 };

    glDebugMessageInsert(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_MARKER, 0,
                         GL_DEBUG_SEVERITY_LOW, -1, "not allowed from the API source");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDebugMessageControl(0x9999, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, -1, NULL, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // naming ids needs a specific source and type
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 1, ids, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glObjectLabel(0x9999, 1, -1, "x");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glObjectLabel(GL_BUFFER, 1, -1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- program interface query ---------- */

GPU_TEST(prog_iface, counts_inputs_and_uniforms)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    GLint n = -1;

    CHECK_MSG(p != 0, "link failed: %s", err);
    if (!p) return;

    glGetProgramInterfaceiv(p, GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES, &n);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(n >= 1, "expected at least one program input, got %d", n);

    glGetProgramInterfaceiv(p, GL_UNIFORM, GL_ACTIVE_RESOURCES, &n);
    CHECK_MSG(n >= 1, "expected at least one uniform, got %d", n);

    glGetProgramInterfaceiv(p, GL_UNIFORM, GL_MAX_NAME_LENGTH, &n);
    CHECK(n > 0);

    glGetProgramInterfaceiv(p, 0x9999, GL_ACTIVE_RESOURCES, &n);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteProgram(p);
}

GPU_TEST(prog_iface, index_and_name_round_trip)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    GLuint idx;
    char name[128] = { 0 };
    GLsizei len = 0;

    CHECK(p != 0);
    if (!p) return;

    idx = glGetProgramResourceIndex(p, GL_UNIFORM, "u_col");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(idx != GL_INVALID_INDEX, "u_col not found in the uniform interface");

    if (idx != GL_INVALID_INDEX)
    {
        glGetProgramResourceName(p, GL_UNIFORM, idx, sizeof name, &len, name);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_STR_EQ(name, "u_col");
        CHECK_EQ_INT(len, 5);
    }

    CHECK_EQ_UINT(glGetProgramResourceIndex(p, GL_UNIFORM, "nope"), GL_INVALID_INDEX);

    glDeleteProgram(p);
}

GPU_TEST(prog_iface, resourceiv_reports_properties)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    const GLenum props[] = { GL_NAME_LENGTH, GL_LOCATION, GL_ARRAY_SIZE };
    GLint vals[3] = { -9, -9, -9 };
    GLsizei written = 0;
    GLuint idx;

    CHECK(p != 0);
    if (!p) return;

    idx = glGetProgramResourceIndex(p, GL_UNIFORM, "u_col");

    if (idx != GL_INVALID_INDEX)
    {
        glGetProgramResourceiv(p, GL_UNIFORM, idx, 3, props, 3, &written, vals);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(written, 3);
        CHECK_EQ_INT(vals[0], 6);      // "u_col" plus the NUL
        CHECK_EQ_INT(vals[2], 1);
    }

    glGetProgramResourceiv(p, GL_UNIFORM, 9999, 3, props, 3, &written, vals);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

GPU_TEST(prog_iface, frag_data_location_is_found)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);

    CHECK(p != 0);
    if (!p) return;

    CHECK_EQ_INT(glGetFragDataLocation(p, "frag"), 0);
    CHECK_EQ_INT(glGetFragDataLocation(p, "nope"), -1);
    CHECK_EQ_INT(glGetFragDataIndex(p, "frag"), 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgram(p);
}

GPU_TEST(prog_iface, queries_do_not_create_programs)
{
    GLint n = 0;

    // a read-only query on an unknown name must not manufacture an object
    glGetProgramInterfaceiv(999123, GL_UNIFORM, GL_ACTIVE_RESOURCES, &n);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    CHECK_EQ_INT(glIsProgram(999123), GL_FALSE);
}

GPU_TEST(prog_iface, subroutine_counts_are_zero)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    GLint n = -1;

    CHECK(p != 0);
    if (!p) return;

    glGetProgramStageiv(p, GL_VERTEX_SHADER, GL_ACTIVE_SUBROUTINES, &n);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(n, 0);

    CHECK_EQ_UINT(glGetSubroutineIndex(p, GL_VERTEX_SHADER, "anything"), GL_INVALID_INDEX);
    CHECK_EQ_INT(glGetSubroutineUniformLocation(p, GL_VERTEX_SHADER, "anything"), -1);

    glDeleteProgram(p);
}

/* ---------- uniform value getters ---------- */

GPU_TEST(uniform_get, typed_readback_round_trips)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    GLint loc;
    GLfloat f[4] = { 0 };
    GLdouble d[4] = { 0 };

    CHECK(p != 0);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_col");
    glProgramUniform4f(p, loc, 0.25f, 0.5f, 0.75f, 1.0f);

    glGetUniformfv(p, loc, f);
    CHECK_NEAR(f[0], 0.25f, 0.001f);
    CHECK_NEAR(f[3], 1.0f,  0.001f);

    glGetUniformdv(p, loc, d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR((float)d[1], 0.5f, 0.001f);

    glDeleteProgram(p);
}

GPU_TEST(uniform_get, n_forms_reject_a_short_buffer)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    GLint loc;
    GLfloat f[4] = { 0 };
    GLdouble d[4] = { 0 };

    CHECK(p != 0);
    if (!p) return;

    loc = glGetUniformLocation(p, "u_col");
    glProgramUniform4f(p, loc, 1, 2, 3, 4);

    glGetnUniformfv(p, loc, sizeof f, f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_NEAR(f[2], 3.0f, 0.001f);

    // four floats do not fit in eight bytes
    glGetnUniformfv(p, loc, 8, f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // four doubles need 32 bytes, not 16
    glGetnUniformdv(p, loc, 16, d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGetnUniformdv(p, loc, sizeof d, d);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetnUniformfv(p, loc, -1, f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

/* ---------- transform feedback objects ---------- */

GPU_TEST(xfb, object_lifecycle_and_binding)
{
    GLuint t = 0;

    glGenTransformFeedbacks(1, &t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(t != 0);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(glIsTransformFeedback(t), GL_TRUE);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteTransformFeedbacks(1, &t);
}

GPU_TEST(xfb, buffer_bindings_round_trip)
{
    GLuint t = 0, b = 0;
    GLint v = -1;
    GLint64 v64 = -1;

    glGenTransformFeedbacks(1, &t);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 256, NULL, GL_STATIC_DRAW);

    glTransformFeedbackBufferBase(t, 0, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTransformFeedbacki_v(t, GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, 0, &v);
    CHECK_EQ_INT(v, (GLint)b);

    glTransformFeedbackBufferRange(t, 1, b, 16, 64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetTransformFeedbacki64_v(t, GL_TRANSFORM_FEEDBACK_BUFFER_START, 1, &v64);
    CHECK_EQ_INT((GLint)v64, 16);
    glGetTransformFeedbacki64_v(t, GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, 1, &v64);
    CHECK_EQ_INT((GLint)v64, 64);

    // unaligned offset, past the end, bad index
    glTransformFeedbackBufferRange(t, 0, b, 3, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glTransformFeedbackBufferRange(t, 0, b, 0, 4096);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glTransformFeedbackBufferBase(t, 99, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
    glDeleteTransformFeedbacks(1, &t);
}

GPU_TEST(xfb, varyings_are_copied_not_referenced)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    GLuint t = 0;
    char name0[32], name1[32];
    const char *names[2];
    char out[64] = { 0 };
    GLsizei len = 0, size = 0;
    GLenum type = 0;

    CHECK(p != 0);
    if (!p) return;

    glGenTransformFeedbacks(1, &t);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);

    strcpy(name0, "v_col");
    strcpy(name1, "v_other");
    names[0] = name0;
    names[1] = name1;

    glTransformFeedbackVaryings(p, 2, names, GL_INTERLEAVED_ATTRIBS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // scribble over the caller's buffers; the copies must survive
    memset(name0, 'X', sizeof name0);
    memset(name1, 'X', sizeof name1);

    glGetTransformFeedbackVarying(p, 0, sizeof out, &len, &size, &type, out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_STR_EQ(out, "v_col");
    CHECK_EQ_INT(size, 1);

    glGetTransformFeedbackVarying(p, 1, sizeof out, &len, &size, &type, out);
    CHECK_STR_EQ(out, "v_other");

    glGetTransformFeedbackVarying(p, 5, sizeof out, &len, &size, &type, out);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteTransformFeedbacks(1, &t);
    glDeleteProgram(p);
}

GPU_TEST(xfb, varyings_reject_bad_arguments)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    const char *one[1] = { "v_col" };

    CHECK(p != 0);
    if (!p) return;

    glTransformFeedbackVaryings(p, 1, one, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glTransformFeedbackVaryings(p, -1, one, GL_INTERLEAVED_ATTRIBS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glTransformFeedbackVaryings(999123, 1, one, GL_INTERLEAVED_ATTRIBS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(p);
}

GPU_TEST(xfb, draw_validates_then_reports_unsupported)
{
    GLuint t = 0;

    glGenTransformFeedbacks(1, &t);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);

    // bad enum is caught before the unsupported report
    glDrawTransformFeedback(0x9999, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDrawTransformFeedbackInstanced(GL_TRIANGLES, t, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDrawTransformFeedback(GL_TRIANGLES, 999123);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // a well formed call still cannot run
    glDrawTransformFeedback(GL_TRIANGLES, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteTransformFeedbacks(1, &t);
}

/* ---------- misc state ---------- */

GPU_TEST(misc_state, clip_control_round_trips)
{
    GLint v = 0;

    glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClipControl(0x9999, GL_ZERO_TO_ONE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glClipControl(GL_LOWER_LEFT, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glClipControl(GL_LOWER_LEFT, GL_NEGATIVE_ONE_TO_ONE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    (void)v;
}

GPU_TEST(misc_state, sample_mask_and_shading)
{
    glMinSampleShading(0.5f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // out of range values clamp rather than error
    glMinSampleShading(-2.0f);
    glMinSampleShading(7.0f);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glSampleMaski(0, 0xFFFFFFFFu);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glSampleMaski(1, 0xFFu);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(misc_state, clamp_color_and_restart_index)
{
    glClampColor(GL_CLAMP_READ_COLOR, GL_FIXED_ONLY);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClampColor(0x9999, GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glClampColor(GL_CLAMP_READ_COLOR, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glPrimitiveRestartIndex(0xFFFF);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(misc_state, conditional_render_pairs_up)
{
    GLuint q = 0;

    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glEndQuery(GL_SAMPLES_PASSED);

    glBeginConditionalRender(q, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBeginConditionalRender(999123, GL_QUERY_WAIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBeginConditionalRender(q, GL_QUERY_WAIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // already inside one
    glBeginConditionalRender(q, GL_QUERY_WAIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glEndConditionalRender();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glEndConditionalRender();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteQueries(1, &q);
}

GPU_TEST(misc_state, patch_parameters_are_stored)
{
    GLfloat inner[2] = { 2.0f, 3.0f };
    GLfloat outer[4] = { 1.0f, 2.0f, 3.0f, 4.0f };

    glPatchParameteri(GL_PATCH_VERTICES, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPatchParameteri(GL_PATCH_VERTICES, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glPatchParameteri(0x9999, 3);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, inner);
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outer);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glPatchParameterfv(0x9999, outer);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(misc_state, readn_pixels_bounds_the_buffer)
{
    MGLTestTarget t;
    unsigned char small[8];
    unsigned char big[4 * 4 * 4];

    if (!mgl_target_create(&t, 4, 4, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&t);

    glReadnPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, sizeof small, small);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glReadnPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, sizeof big, big);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glReadnPixels(0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, -1, big);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    mgl_target_destroy(&t);
}

GPU_TEST(misc_state, validate_program_reports_status)
{
    char err[1024] = { 0 };
    GLuint good = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    GLuint empty = glCreateProgram();
    GLint v = -1;

    CHECK(good != 0);
    if (!good) return;

    glValidateProgram(good);
    glGetProgramiv(good, GL_VALIDATE_STATUS, &v);
    CHECK_EQ_INT(v, GL_TRUE);

    glValidateProgram(empty);
    glGetProgramiv(empty, GL_VALIDATE_STATUS, &v);
    CHECK_EQ_INT(v, GL_FALSE);

    glValidateProgram(999123);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteProgram(good);
    glDeleteProgram(empty);
}

GPU_TEST(misc_state, program_pipeline_queries)
{
    GLuint pp = 0;
    GLint v = -1;

    glGenProgramPipelines(1, &pp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(pp != 0);

    glBindProgramPipeline(pp);

    glValidateProgramPipeline(pp);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetProgramPipelineiv(pp, GL_VALIDATE_STATUS, &v);
    CHECK_EQ_INT(v, GL_FALSE);          // nothing attached

    glGetProgramPipelineiv(pp, GL_VERTEX_SHADER, &v);
    CHECK_EQ_INT(v, 0);

    glGetProgramPipelineiv(pp, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glGetProgramPipelineiv(999123, GL_VALIDATE_STATUS, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glActiveShaderProgram(pp, 999123);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindProgramPipeline(0);
    glDeleteProgramPipelines(1, &pp);
}

GPU_TEST(misc_state, buffer_parameter_64_matches_32)
{
    GLuint b = 0;
    GLint v32 = 0;
    GLint64 v64 = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 1024, NULL, GL_STATIC_DRAW);

    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &v32);
    glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &v64);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT((GLint)v64, v32);
    CHECK_EQ_INT(v32, 1024);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

/* ---------- ARB_gl_spirv and the remaining loose ends ---------- */

// a minimal valid SPIR-V header; MGL only checks the magic and word alignment
static const GLuint SPIRV_STUB[] = {
    0x07230203u, 0x00010000u, 0x00080001u, 0x00000001u, 0x00000000u
};

GPU_TEST(spirv, shader_binary_accepts_a_module)
{
    GLuint s = glCreateShader(GL_VERTEX_SHADER);

    CHECK(s != 0);

    glShaderBinary(1, &s, GL_SHADER_BINARY_FORMAT_SPIR_V, SPIRV_STUB, sizeof SPIRV_STUB);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteShader(s);
}

GPU_TEST(spirv, shader_binary_rejects_bad_input)
{
    GLuint s = glCreateShader(GL_VERTEX_SHADER);
    GLuint bad_magic[] = { 0xDEADBEEFu, 0u, 0u, 0u };
    GLuint nope = 999123;

    glShaderBinary(1, &s, GL_NONE, SPIRV_STUB, sizeof SPIRV_STUB);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glShaderBinary(-1, &s, GL_SHADER_BINARY_FORMAT_SPIR_V, SPIRV_STUB, sizeof SPIRV_STUB);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // not a whole number of 32 bit words
    glShaderBinary(1, &s, GL_SHADER_BINARY_FORMAT_SPIR_V, SPIRV_STUB, 7);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glShaderBinary(1, &s, GL_SHADER_BINARY_FORMAT_SPIR_V, bad_magic, sizeof bad_magic);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glShaderBinary(1, &nope, GL_SHADER_BINARY_FORMAT_SPIR_V, SPIRV_STUB, sizeof SPIRV_STUB);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // a zero length buffer must not be read
    glShaderBinary(1, &s, GL_SHADER_BINARY_FORMAT_SPIR_V, SPIRV_STUB, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteShader(s);
}

GPU_TEST(spirv, specialize_needs_a_binary_and_runs_once)
{
    GLuint s = glCreateShader(GL_VERTEX_SHADER);
    GLint ok = -1;

    // no binary loaded yet
    glSpecializeShader(s, "main", 0, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glShaderBinary(1, &s, GL_SHADER_BINARY_FORMAT_SPIR_V, SPIRV_STUB, sizeof SPIRV_STUB);

    glSpecializeShader(s, NULL, 0, NULL, NULL);   // NULL entry point means main
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    CHECK_EQ_INT(ok, GL_TRUE);

    // specializing twice
    glSpecializeShader(s, "main", 0, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glSpecializeShader(999123, "main", 0, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteShader(s);
}

GPU_TEST(spirv, specialize_checks_the_constant_arrays)
{
    GLuint s = glCreateShader(GL_VERTEX_SHADER);

    glShaderBinary(1, &s, GL_SHADER_BINARY_FORMAT_SPIR_V, SPIRV_STUB, sizeof SPIRV_STUB);

    glSpecializeShader(s, "main", 2, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteShader(s);
}

GPU_TEST(spirv, program_parameteri_and_binary)
{
    char err[1024] = { 0 };
    GLuint p = mgl_build_program(VS_IO, FS_IO, err, sizeof err);
    GLsizei len = 99;
    GLenum fmt = 0;
    char buf[64];
    GLint v = -1;

    CHECK(p != 0);
    if (!p) return;

    glProgramParameteri(p, GL_PROGRAM_SEPARABLE, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glProgramParameteri(p, GL_PROGRAM_SEPARABLE, 7);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glProgramParameteri(p, 0x9999, GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // no binary formats are exposed
    glGetProgramBinary(p, sizeof buf, &len, &fmt, buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    CHECK_EQ_INT(len, 0);

    glProgramBinary(p, 0x1234, buf, sizeof buf);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // an unrecognised binary leaves the program unlinked
    glGetProgramiv(p, GL_LINK_STATUS, &v);
    CHECK_EQ_INT(v, GL_FALSE);

    glDeleteProgram(p);
}

GPU_TEST(clear_buffer, integer_clears_validate)
{
    MGLTestTarget t;
    GLint iv[4] = { 1, 2, 3, 4 };
    GLuint uv[4] = { 1, 2, 3, 4 };

    if (!mgl_target_create(&t, 8, 8, GL_RGBA8, 0)) { CHECK(0); return; }
    mgl_target_bind(&t);

    glClearBufferiv(GL_COLOR, 0, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClearBufferuiv(GL_COLOR, 0, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glClearBufferiv(GL_STENCIL, 0, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // uiv has no stencil form
    glClearBufferuiv(GL_STENCIL, 0, uv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glClearBufferiv(GL_DEPTH, 0, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glClearBufferiv(GL_COLOR, 9999, iv);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glClearBufferiv(GL_COLOR, 0, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // the float form used to index the attachment array unchecked
    glClearBufferfv(GL_COLOR, 9999, (const GLfloat[]){ 0, 0, 0, 1 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    mgl_target_destroy(&t);
}

GPU_TEST(clear_buffer, indirect_count_draws_validate)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, b);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, 256, NULL, GL_STATIC_DRAW);

    glMultiDrawArraysIndirectCount(0x9999, NULL, 0, 1, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, -4, 1, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // the parameter offset must be 4 byte aligned
    glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 3, 1, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 0, -1, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glMultiDrawElementsIndirectCount(GL_TRIANGLES, GL_FLOAT, NULL, 0, 1, 20);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // well formed, but there is no parameter buffer target yet
    glMultiDrawArraysIndirectCount(GL_TRIANGLES, NULL, 0, 1, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glDeleteBuffers(1, &b);
}
