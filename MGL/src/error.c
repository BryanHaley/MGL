/*
 * Copyright (C) Michael Larson on 1/6/2022
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
 * error.h
 * MGL
 *
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include "error.h"
#include "mgl_log.h"


GLenum  mglGetError(GLMContext ctx)
{
    GLenum err;

    err = ctx->state.error;

    if (err != GL_NO_ERROR)
        MGL_DEBUG("MGL: glGetError returning 0x%x\n", err);

    ctx->state.error = GL_NO_ERROR;

    return err;
}


void error_func(GLMContext ctx, const char *func, GLenum error)
{
    // GL keeps only the first error until glGetError clears it, so only report
    // that one; otherwise a bad call in a loop floods the log
    if (ctx->state.error)
        return;

    ctx->state.error = error;

    MGL_ERR("MGL GL Error in %s: 0x%x\n", func, error);

    /* Temporarily disabled to allow QEMU to continue despite errors */
    // if (ctx->assert_on_error)
    //     assert(0);
}

/* ---------- KHR_debug ---------- */

static GLboolean debugSourceValid(GLenum source)
{
    switch (source)
    {
        case GL_DEBUG_SOURCE_API:
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
        case GL_DEBUG_SOURCE_THIRD_PARTY:
        case GL_DEBUG_SOURCE_APPLICATION:
        case GL_DEBUG_SOURCE_OTHER:
            return GL_TRUE;
    }

    return GL_FALSE;
}

static GLboolean debugTypeValid(GLenum type)
{
    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        case GL_DEBUG_TYPE_PORTABILITY:
        case GL_DEBUG_TYPE_PERFORMANCE:
        case GL_DEBUG_TYPE_MARKER:
        case GL_DEBUG_TYPE_PUSH_GROUP:
        case GL_DEBUG_TYPE_POP_GROUP:
        case GL_DEBUG_TYPE_OTHER:
            return GL_TRUE;
    }

    return GL_FALSE;
}

static GLboolean debugSeverityValid(GLenum severity)
{
    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:
        case GL_DEBUG_SEVERITY_MEDIUM:
        case GL_DEBUG_SEVERITY_LOW:
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return GL_TRUE;
    }

    return GL_FALSE;
}

// a negative length means the string is NUL terminated
static GLsizei debugMeasure(const GLchar *buf, GLsizei length)
{
    if (length < 0)
        return (GLsizei)strlen(buf);

    return length;
}

void mglDebugEmit(GLMContext ctx, GLenum source, GLenum type, GLuint id, GLenum severity, const char *text)
{
    DebugState *debug = &ctx->state.debug;
    GLDEBUGPROC callback;
    DebugMessage *msg;
    GLsizei len;

    if (debug->messages_enabled == GL_FALSE)
        return;

    len = (GLsizei)strlen(text);

    if (len > MAX_DEBUG_MSG_LEN - 1)
        len = MAX_DEBUG_MSG_LEN - 1;

    callback = (GLDEBUGPROC)ctx->state.debug_callback;

    if (callback)
    {
        callback(source, type, id, severity, len, (const GLchar *)text, ctx->state.debug_user_param);

        return;
    }

    msg = &debug->messages[debug->head];
    msg->source = source;
    msg->type = type;
    msg->id = id;
    msg->severity = severity;
    msg->length = len;
    memcpy(msg->text, text, (size_t)len);
    msg->text[len] = '\0';

    debug->head = (debug->head + 1) % MAX_DEBUG_MESSAGES;

    if (debug->count < MAX_DEBUG_MESSAGES)
        debug->count++;
}

void mglDebugMessageCallback(GLMContext ctx, GLDEBUGPROC callback, const void *userParam)
{
    ctx->state.debug_callback = (void *)callback;
    ctx->state.debug_user_param = userParam;
}

void mglDebugMessageControl(GLMContext ctx, GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled)
{
    ERROR_CHECK_RETURN(source == GL_DONT_CARE || debugSourceValid(source), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(type == GL_DONT_CARE || debugTypeValid(type), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(severity == GL_DONT_CARE || debugSeverityValid(severity), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(count == 0 || ids, GL_INVALID_VALUE);

    if (count > 0)
    {
        ERROR_CHECK_RETURN(source != GL_DONT_CARE && type != GL_DONT_CARE, GL_INVALID_OPERATION);
        ERROR_CHECK_RETURN(severity == GL_DONT_CARE, GL_INVALID_OPERATION);
    }

    ctx->state.debug.messages_enabled = enabled;
}

void mglDebugMessageInsert(GLMContext ctx, GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf)
{
    char text[MAX_DEBUG_MSG_LEN];
    GLsizei len;

    ERROR_CHECK_RETURN(source == GL_DEBUG_SOURCE_APPLICATION ||
                       source == GL_DEBUG_SOURCE_THIRD_PARTY, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(debugTypeValid(type), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(debugSeverityValid(severity), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(buf, GL_INVALID_VALUE);

    len = debugMeasure(buf, length);
    ERROR_CHECK_RETURN(len < MAX_DEBUG_MSG_LEN, GL_INVALID_VALUE);

    memcpy(text, buf, (size_t)len);
    text[len] = '\0';

    mglDebugEmit(ctx, source, type, id, severity, text);
}

GLuint mglGetDebugMessageLog(GLMContext ctx, GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog)
{
    DebugState *debug = &ctx->state.debug;
    GLuint written = 0;
    GLsizei remaining = bufSize;

    ERROR_CHECK_RETURN_VALUE(bufSize >= 0 || messageLog == NULL, GL_INVALID_VALUE, 0);

    while (written < count && debug->count > 0)
    {
        // head is where the next write goes, so the oldest sits count back
        GLuint tail = (debug->head + MAX_DEBUG_MESSAGES - debug->count) % MAX_DEBUG_MESSAGES;
        DebugMessage *msg = &debug->messages[tail];
        GLsizei len = msg->length + 1;

        if (messageLog && len > remaining)
            break;

        if (sources)    sources[written] = msg->source;
        if (types)      types[written] = msg->type;
        if (ids)        ids[written] = msg->id;
        if (severities) severities[written] = msg->severity;
        if (lengths)    lengths[written] = len;

        if (messageLog)
        {
            memcpy(messageLog, msg->text, (size_t)len);
            messageLog += len;
            remaining -= len;
        }

        debug->count--;
        written++;
    }

    return written;
}

void mglPushDebugGroup(GLMContext ctx, GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
    DebugState *debug = &ctx->state.debug;
    char text[MAX_DEBUG_MSG_LEN];
    GLsizei len;

    ERROR_CHECK_RETURN(source == GL_DEBUG_SOURCE_APPLICATION ||
                       source == GL_DEBUG_SOURCE_THIRD_PARTY, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(message, GL_INVALID_VALUE);

    len = debugMeasure(message, length);
    ERROR_CHECK_RETURN(len < MAX_DEBUG_MSG_LEN, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(debug->group_depth < MAX_DEBUG_GROUPS, GL_STACK_OVERFLOW);

    memcpy(text, message, (size_t)len);
    text[len] = '\0';

    debug->groups[debug->group_depth].source = source;
    debug->groups[debug->group_depth].type = GL_DEBUG_TYPE_PUSH_GROUP;
    debug->groups[debug->group_depth].id = id;
    debug->groups[debug->group_depth].severity = GL_DEBUG_SEVERITY_NOTIFICATION;
    debug->groups[debug->group_depth].length = len;
    memcpy(debug->groups[debug->group_depth].text, text, (size_t)len + 1);
    debug->group_depth++;

    mglDebugEmit(ctx, source, GL_DEBUG_TYPE_PUSH_GROUP, id, GL_DEBUG_SEVERITY_NOTIFICATION, text);
}

void mglPopDebugGroup(GLMContext ctx)
{
    DebugState *debug = &ctx->state.debug;
    DebugMessage *group;

    ERROR_CHECK_RETURN(debug->group_depth > 0, GL_STACK_UNDERFLOW);

    debug->group_depth--;
    group = &debug->groups[debug->group_depth];

    mglDebugEmit(ctx, group->source, GL_DEBUG_TYPE_POP_GROUP, group->id,
                 GL_DEBUG_SEVERITY_NOTIFICATION, group->text);
}

void mglObjectLabel(GLMContext ctx, GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
    GLsizei len;

    switch (identifier)
    {
        case GL_BUFFER:
        case GL_SHADER:
        case GL_PROGRAM:
        case GL_TEXTURE:
        case GL_VERTEX_ARRAY:
        case GL_QUERY:
        case GL_PROGRAM_PIPELINE:
        case GL_TRANSFORM_FEEDBACK:
        case GL_SAMPLER:
        case GL_FRAMEBUFFER:
        case GL_RENDERBUFFER:
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }

    // a NULL label removes any label, so it is not an error
    if (label == NULL)
        return;

    len = debugMeasure(label, length);
    ERROR_CHECK_RETURN(len < MAX_OBJECT_LABEL, GL_INVALID_VALUE);
}

void mglObjectPtrLabel(GLMContext ctx, const void *ptr, GLsizei length, const GLchar *label)
{
    GLsizei len;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);

    if (label == NULL)
        return;

    len = debugMeasure(label, length);
    ERROR_CHECK_RETURN(len < MAX_OBJECT_LABEL, GL_INVALID_VALUE);
}
