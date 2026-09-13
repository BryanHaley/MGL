/*
 * query.c
 * MGL
 *
 * Query objects. Occlusion counts are filled in by the renderer; timestamps
 * come from the host clock.
 */

#include <string.h>
#include <stdlib.h>
#include <mach/mach_time.h>

#include "glm_context.h"

Buffer *findBuffer(GLMContext ctx, GLuint buffer);
#include "buffers.h"

Query *newQuery(GLMContext ctx, GLuint name)
{
    Query *q = (Query *)calloc(1, sizeof(Query));

    ERROR_CHECK_RETURN_VALUE(q, GL_OUT_OF_MEMORY, NULL);

    q->name = name;

    insertHashElement(&ctx->state.query_table, name, q);

    return q;
}

Query *findQuery(GLMContext ctx, GLuint name)
{
    return (Query *)searchHashTable(&ctx->state.query_table, name);
}

bool isQueryName(GLMContext ctx, GLuint name)
{
    return searchHashTable(&ctx->state.query_table, name) != NULL;
}

static int queryTargetIndex(GLenum target)
{
    switch (target)
    {
        case GL_SAMPLES_PASSED:                         return _QUERY_SAMPLES_PASSED;
        case GL_ANY_SAMPLES_PASSED:                     return _QUERY_ANY_SAMPLES_PASSED;
        case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:        return _QUERY_ANY_SAMPLES_PASSED_CONSERVATIVE;
        case GL_PRIMITIVES_GENERATED:                   return _QUERY_PRIMITIVES_GENERATED;
        case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:  return _QUERY_TF_PRIMITIVES_WRITTEN;
        case GL_TRANSFORM_FEEDBACK_OVERFLOW:            return _QUERY_TF_OVERFLOW;
        case GL_TRANSFORM_FEEDBACK_STREAM_OVERFLOW:     return _QUERY_TF_STREAM_OVERFLOW;
        case GL_TIME_ELAPSED:                           return _QUERY_TIME_ELAPSED;
        case GL_TIMESTAMP:                              return _QUERY_TIMESTAMP;
    }

    return -1;
}

// only the primitive and feedback targets have more than one stream
static bool targetAllowsStream(int t, GLuint index)
{
    switch (t)
    {
        case _QUERY_PRIMITIVES_GENERATED:
        case _QUERY_TF_PRIMITIVES_WRITTEN:
        case _QUERY_TF_OVERFLOW:
        case _QUERY_TF_STREAM_OVERFLOW:
            return true;
    }

    return index == 0;
}

// The three targets Metal counts fragments for.
static bool isOcclusionTarget(GLenum target)
{
    return target == GL_SAMPLES_PASSED ||
           target == GL_ANY_SAMPLES_PASSED ||
           target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
}

static GLuint64 hostTimeNS(void)
{
    static mach_timebase_info_data_t timebase;

    if (timebase.denom == 0)
        mach_timebase_info(&timebase);

    return (GLuint64)(mach_absolute_time() * timebase.numer / timebase.denom);
}

void mglGenQueries(GLMContext ctx, GLsizei n, GLuint *ids)
{
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    if (n == 0)
        return;

    ERROR_CHECK_RETURN(ids, GL_INVALID_VALUE);

    // reserves names only; a name becomes an object on first use
    for (GLsizei i = 0; i < n; i++)
        ids[i] = getNewName(&ctx->state.query_table);
}

void mglCreateQueries(GLMContext ctx, GLenum target, GLsizei n, GLuint *ids)
{
    ERROR_CHECK_RETURN(queryTargetIndex(target) >= 0, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    if (n == 0)
        return;

    ERROR_CHECK_RETURN(ids, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < n; i++)
    {
        GLuint name = getNewName(&ctx->state.query_table);
        Query *q = newQuery(ctx, name);

        if (q == NULL)
            return;

        q->target = target;
        ids[i] = name;
    }
}

void mglDeleteQueries(GLMContext ctx, GLsizei n, const GLuint *ids)
{
    ERROR_CHECK_RETURN(n >= 0, GL_INVALID_VALUE);

    if (n == 0)
        return;

    ERROR_CHECK_RETURN(ids, GL_INVALID_VALUE);

    for (GLsizei i = 0; i < n; i++)
    {
        Query *q = findQuery(ctx, ids[i]);

        if (q == NULL)
            continue;

        // a deleted query that was still running stops being current
        for (int t = 0; t < _MAX_QUERY_TARGETS; t++)
            for (int s = 0; s < MAX_QUERY_STREAMS; s++)
                if (ctx->state.active_query[t][s] == q)
                    ctx->state.active_query[t][s] = NULL;

        deleteHashElement(&ctx->state.query_table, ids[i]);
        free(q);
    }
}

GLboolean mglIsQuery(GLMContext ctx, GLuint id)
{
    return findQuery(ctx, id) ? GL_TRUE : GL_FALSE;
}

void mglBeginQueryIndexed(GLMContext ctx, GLenum target, GLuint index, GLuint id)
{
    int t = queryTargetIndex(target);
    Query *q;

    ERROR_CHECK_RETURN(t >= 0, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(target != GL_TIMESTAMP, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(id != 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(index < MAX_QUERY_STREAMS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(targetAllowsStream(t, index), GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ctx->state.active_query[t][index] == NULL, GL_INVALID_OPERATION);

    // the same object cannot be running twice
    for (int i = 0; i < _MAX_QUERY_TARGETS; i++)
        for (int j = 0; j < MAX_QUERY_STREAMS; j++)
            if (ctx->state.active_query[i][j] && ctx->state.active_query[i][j]->name == id)
                ERROR_RETURN(GL_INVALID_OPERATION);

    q = findQuery(ctx, id);

    if (q == NULL)
    {
        q = newQuery(ctx, id);

        if (q == NULL)
            return;
    }

    ERROR_CHECK_RETURN(q->target == 0 || q->target == target, GL_INVALID_OPERATION);

    q->target = target;
    q->index = index;
    q->active = GL_TRUE;
    q->have_result = GL_FALSE;
    q->result = 0;
    q->visibility_offset = -1;
    q->visibility_slots = 0;
    q->start_time = hostTimeNS();

    ctx->state.active_query[t][index] = q;

    // a render encoder may already be open, in which case counting starts now
    if (isOcclusionTarget(target) && ctx->mtl_funcs.mtlQueryBegin)
        ctx->mtl_funcs.mtlQueryBegin(ctx, q);
}

void mglEndQueryIndexed(GLMContext ctx, GLenum target, GLuint index)
{
    int t = queryTargetIndex(target);
    Query *q;

    ERROR_CHECK_RETURN(t >= 0, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(index < MAX_QUERY_STREAMS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(targetAllowsStream(t, index), GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ctx->state.active_query[t][index], GL_INVALID_OPERATION);

    q = ctx->state.active_query[t][index];

    if (target == GL_TIME_ELAPSED)
        q->result = hostTimeNS() - q->start_time;

    if (isOcclusionTarget(target) && ctx->mtl_funcs.mtlQueryEnd)
        ctx->mtl_funcs.mtlQueryEnd(ctx, q);

    q->active = GL_FALSE;
    q->have_result = GL_TRUE;

    ctx->state.active_query[t][index] = NULL;
}

void mglBeginQuery(GLMContext ctx, GLenum target, GLuint id)
{
    mglBeginQueryIndexed(ctx, target, 0, id);
}

void mglEndQuery(GLMContext ctx, GLenum target)
{
    mglEndQueryIndexed(ctx, target, 0);
}

void mglQueryCounter(GLMContext ctx, GLuint id, GLenum target)
{
    Query *q;

    ERROR_CHECK_RETURN(target == GL_TIMESTAMP, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(id != 0, GL_INVALID_VALUE);

    q = findQuery(ctx, id);

    if (q == NULL)
    {
        q = newQuery(ctx, id);

        if (q == NULL)
            return;
    }

    ERROR_CHECK_RETURN(q->active == GL_FALSE, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(q->target == 0 || q->target == target, GL_INVALID_OPERATION);

    q->target = target;
    q->result = hostTimeNS();
    q->have_result = GL_TRUE;
}

static void getQueryTargetiv(GLMContext ctx, GLenum target, GLuint index, GLenum pname, GLint *params)
{
    int t = queryTargetIndex(target);

    ERROR_CHECK_RETURN(t >= 0, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(index < MAX_QUERY_STREAMS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(targetAllowsStream(t, index), GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    switch (pname)
    {
        case GL_CURRENT_QUERY:
            *params = ctx->state.active_query[t][index] ?
                      (GLint)ctx->state.active_query[t][index]->name : 0;
            break;

        case GL_QUERY_COUNTER_BITS:
            *params = (target == GL_TIMESTAMP || target == GL_TIME_ELAPSED) ? 64 : 32;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetQueryiv(GLMContext ctx, GLenum target, GLenum pname, GLint *params)
{
    getQueryTargetiv(ctx, target, 0, pname, params);
}

void mglGetQueryIndexediv(GLMContext ctx, GLenum target, GLuint index, GLenum pname, GLint *params)
{
    getQueryTargetiv(ctx, target, index, pname, params);
}

static bool queryObjectValue(GLMContext ctx, GLuint id, GLenum pname, GLuint64 *out)
{
    Query *q = findQuery(ctx, id);

    ERROR_CHECK_RETURN_VALUE(q, GL_INVALID_OPERATION, false);
    ERROR_CHECK_RETURN_VALUE(q->active == GL_FALSE, GL_INVALID_OPERATION, false);

    switch (pname)
    {
        case GL_QUERY_RESULT:
        case GL_QUERY_RESULT_NO_WAIT:
            // an occlusion count is written by the GPU, so the answer is not
            // there until the work that wrote it has run
            if (isOcclusionTarget(q->target) && q->visibility_offset >= 0)
            {
                if (pname == GL_QUERY_RESULT)
                    ctx->mtl_funcs.mtlFlush(ctx, true);

                ctx->mtl_funcs.mtlQueryResult(ctx, q);
            }

            *out = q->result;

            if (q->target == GL_ANY_SAMPLES_PASSED ||
                q->target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE)
                *out = *out ? GL_TRUE : GL_FALSE;

            return true;

        case GL_QUERY_RESULT_AVAILABLE:
            *out = q->have_result ? 1 : 0;
            return true;
    }

    ERROR_RETURN_VALUE(GL_INVALID_ENUM, false);
}

void mglGetQueryObjectiv(GLMContext ctx, GLuint id, GLenum pname, GLint *params)
{
    GLuint64 value;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (queryObjectValue(ctx, id, pname, &value))
        *params = (GLint)value;
}

void mglGetQueryObjectuiv(GLMContext ctx, GLuint id, GLenum pname, GLuint *params)
{
    GLuint64 value;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (queryObjectValue(ctx, id, pname, &value))
        *params = (GLuint)value;
}

void mglGetQueryObjecti64v(GLMContext ctx, GLuint id, GLenum pname, GLint64 *params)
{
    GLuint64 value;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (queryObjectValue(ctx, id, pname, &value))
        *params = (GLint64)value;
}

void mglGetQueryObjectui64v(GLMContext ctx, GLuint id, GLenum pname, GLuint64 *params)
{
    GLuint64 value;

    ERROR_CHECK_RETURN(params, GL_INVALID_VALUE);

    if (queryObjectValue(ctx, id, pname, &value))
        *params = value;
}

static void queryIntoBuffer(GLMContext ctx, GLuint id, GLuint buffer, GLenum pname,
                            GLintptr offset, size_t value_size)
{
    Buffer *buf;
    GLuint64 value;

    ERROR_CHECK_RETURN(offset >= 0, GL_INVALID_VALUE);

    if (queryObjectValue(ctx, id, pname, &value) == false)
        return;

    buf = findBuffer(ctx, buffer);

    ERROR_CHECK_RETURN(buf, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(buf->data.buffer_data, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN((GLuint64)offset + value_size <= (GLuint64)buf->size, GL_INVALID_VALUE);

    memcpy((char *)buf->data.buffer_data + offset, &value, value_size);
}

void mglGetQueryBufferObjectiv(GLMContext ctx, GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    queryIntoBuffer(ctx, id, buffer, pname, offset, sizeof(GLint));
}

void mglGetQueryBufferObjectuiv(GLMContext ctx, GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    queryIntoBuffer(ctx, id, buffer, pname, offset, sizeof(GLuint));
}

void mglGetQueryBufferObjecti64v(GLMContext ctx, GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    queryIntoBuffer(ctx, id, buffer, pname, offset, sizeof(GLint64));
}

void mglGetQueryBufferObjectui64v(GLMContext ctx, GLuint id, GLuint buffer, GLenum pname, GLintptr offset)
{
    queryIntoBuffer(ctx, id, buffer, pname, offset, sizeof(GLuint64));
}
