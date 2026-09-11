/*
 * test_query_xfb.c
 * MGL
 *
 * Query buffer object writes and the transform feedback object state machine:
 * begin / end / pause / resume, object creation, and state queries.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- transform feedback begin / end / pause / resume ---------- */

GPU_TEST(query_xfb, begin_end_pause_resume)
{
    GLuint t = 0;

    glGenTransformFeedbacks(1, &t);
    CHECK(t != 0);

    // begin with no xfb bound -> error
    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);

    // begin with a valid primitive mode
    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // double begin
    glBeginTransformFeedback(GL_POINTS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // pause
    glPauseTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // double pause
    glPauseTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // resume
    glResumeTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // resume when not paused
    glResumeTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // end while active
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // double end
    glEndTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // pause with nothing active
    glPauseTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // resume with nothing active
    glResumeTransformFeedback();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // begin with an invalid primitive mode
    // Spec: only GL_POINTS, GL_LINES, GL_TRIANGLES are valid.
    // MGL does not validate primitiveMode, so this will NOT produce
    // GL_INVALID_ENUM and the assertion below will FAIL.
    glBeginTransformFeedback(GL_TRIANGLE_STRIP);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // end it so the state machine is clean for deletion
    mgl_drain_errors();
    glEndTransformFeedback();
    mgl_drain_errors();

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteTransformFeedbacks(1, &t);
}

GPU_TEST(query_xfb, begin_with_lines_and_triangles)
{
    GLuint t = 0;

    glGenTransformFeedbacks(1, &t);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);

    // GL_LINES and GL_TRIANGLES are also valid per spec
    glBeginTransformFeedback(GL_LINES);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glEndTransformFeedback();

    glBeginTransformFeedback(GL_TRIANGLES);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glEndTransformFeedback();

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteTransformFeedbacks(1, &t);
}

/* ---------- glCreateTransformFeedbacks ---------- */

GPU_TEST(query_xfb, create_transform_feedbacks)
{
    GLuint ids[3] = { 0 };

    glCreateTransformFeedbacks(3, ids);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 3; i++)
    {
        CHECK_MSG(ids[i] != 0, "create xfb %d got name 0", i);
        CHECK_EQ_INT(glIsTransformFeedback(ids[i]), GL_TRUE);
    }

    // names must be distinct
    CHECK(ids[0] != ids[1] && ids[1] != ids[2] && ids[0] != ids[2]);

    // negative count
    glCreateTransformFeedbacks(-1, ids);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteTransformFeedbacks(3, ids);
}

/* ---------- glGetTransformFeedbackiv ---------- */

GPU_TEST(query_xfb, get_transform_feedbackiv)
{
    GLuint t = 0;
    GLint v = -1;

    // invalid xfb name
    glGetTransformFeedbackiv(9999, GL_TRANSFORM_FEEDBACK_PAUSED, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGenTransformFeedbacks(1, &t);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);

    // default state: not active, not paused
    glGetTransformFeedbackiv(t, GL_TRANSFORM_FEEDBACK_ACTIVE, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, GL_FALSE);

    glGetTransformFeedbackiv(t, GL_TRANSFORM_FEEDBACK_PAUSED, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(v, GL_FALSE);

    // after begin: active
    glBeginTransformFeedback(GL_POINTS);
    glGetTransformFeedbackiv(t, GL_TRANSFORM_FEEDBACK_ACTIVE, &v);
    CHECK_EQ_INT(v, GL_TRUE);

    // after pause: paused
    glPauseTransformFeedback();
    glGetTransformFeedbackiv(t, GL_TRANSFORM_FEEDBACK_PAUSED, &v);
    CHECK_EQ_INT(v, GL_TRUE);

    glResumeTransformFeedback();
    glEndTransformFeedback();

    // bad pname
    glGetTransformFeedbackiv(t, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    glDeleteTransformFeedbacks(1, &t);
}

/* ---------- glGetQueryObjecti64v ---------- */

GPU_TEST(query_xfb, get_query_object_i64v)
{
    GLuint q = 0;
    GLint64 result = -1;

    // invalid query id
    glGetQueryObjecti64v(9999, GL_QUERY_RESULT, &result);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glEndQuery(GL_SAMPLES_PASSED);

    // read result as 64-bit int
    glGetQueryObjecti64v(q, GL_QUERY_RESULT, &result);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // result is zero because nothing was drawn
    CHECK_EQ_INT((int)result, 0);

    // QUERY_RESULT_AVAILABLE
    GLint64 avail = 0;
    glGetQueryObjecti64v(q, GL_QUERY_RESULT_AVAILABLE, &avail);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT((int)avail, 1);

    // bad pname
    glGetQueryObjecti64v(q, 0x9999, &result);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    // NULL params
    glGetQueryObjecti64v(q, GL_QUERY_RESULT, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteQueries(1, &q);
}

/* ---------- glGetQueryBufferObjectiv / i64v / ui64v ---------- */

GPU_TEST(query_xfb, get_query_buffer_object)
{
    GLuint q = 0, b = 0;
    GLint ival = -1;
    GLint64 i64val = -1;
    GLuint64 ui64val = 0;

    glGenQueries(1, &q);
    glBeginQuery(GL_SAMPLES_PASSED, q);
    glEndQuery(GL_SAMPLES_PASSED);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);

    // glGetQueryBufferObjectiv writes a GLint
    glGetQueryBufferObjectiv(q, b, GL_QUERY_RESULT_AVAILABLE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof ival, &ival);
    CHECK_EQ_INT(ival, 1);

    // glGetQueryBufferObjecti64v writes a GLint64
    glGetQueryBufferObjecti64v(q, b, GL_QUERY_RESULT_AVAILABLE, 8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetBufferSubData(GL_ARRAY_BUFFER, 8, sizeof i64val, &i64val);
    CHECK_EQ_INT((int)i64val, 1);

    // glGetQueryBufferObjectui64v writes a GLuint64
    glGetQueryBufferObjectui64v(q, b, GL_QUERY_RESULT_AVAILABLE, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glGetBufferSubData(GL_ARRAY_BUFFER, 16, sizeof ui64val, &ui64val);
    CHECK_EQ_UINT(ui64val, 1u);

    // offset past the end
    glGetQueryBufferObjectiv(q, b, GL_QUERY_RESULT, 61);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glGetQueryBufferObjecti64v(q, b, GL_QUERY_RESULT, 60);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // invalid query
    glGetQueryBufferObjectiv(9999, b, GL_QUERY_RESULT_AVAILABLE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // invalid buffer
    glGetQueryBufferObjectiv(q, 9999, GL_QUERY_RESULT_AVAILABLE, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // negative offset
    glGetQueryBufferObjectiv(q, b, GL_QUERY_RESULT_AVAILABLE, -4);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // bad pname
    glGetQueryBufferObjectiv(q, b, 0x9999, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
    glDeleteQueries(1, &q);
}
