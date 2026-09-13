/*
 * transform_feedback.c
 * MGL
 *
 * Object model, bindings and queries. Metal has no capture stage, so the draw
 * calls that replay captured vertices validate and then report failure.
 */

#include <string.h>
#include <stdlib.h>

#include "glm_context.h"

Buffer *findBuffer(GLMContext ctx, GLuint buffer);
Program *findProgram(GLMContext ctx, GLuint program);
#include "buffers.h"
#include "programs.h"

TransformFeedback *findTransformFeedback(GLMContext ctx, GLuint name);

// 0 means whatever is bound
static TransformFeedback *xfbForName(GLMContext ctx, GLuint xfb)
{
    if (xfb == 0)
        return ctx->state.transform_feedback;

    return findTransformFeedback(ctx, xfb);
}

static GLboolean isPrimitiveMode(GLenum mode)
{
    switch (mode)
    {
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
        case GL_LINES_ADJACENCY:
        case GL_LINE_STRIP_ADJACENCY:
        case GL_TRIANGLES_ADJACENCY:
        case GL_TRIANGLE_STRIP_ADJACENCY:
        case GL_PATCHES:
            return GL_TRUE;
    }

    return GL_FALSE;
}

static void freeVaryings(TransformFeedback *xfb)
{
    if (xfb->varyings)
    {
        for (GLsizei i = 0; i < xfb->varying_count; i++)
            free(xfb->varyings[i]);

        free(xfb->varyings);
        xfb->varyings = NULL;
    }

    xfb->varying_count = 0;
}

void mglTransformFeedbackVaryings(GLMContext ctx, GLuint program, GLsizei count, const GLchar *const*varyings, GLenum bufferMode)
{
    Program *prog = findProgram(ctx, program);
    TransformFeedback *xfb;
    char **names = NULL;

    ERROR_CHECK_RETURN(prog, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(count == 0 || varyings, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufferMode == GL_INTERLEAVED_ATTRIBS ||
                       bufferMode == GL_SEPARATE_ATTRIBS, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(bufferMode != GL_SEPARATE_ATTRIBS || count <= MAX_TF_BUFFERS, GL_INVALID_VALUE);

    if (count > 0)
    {
        names = (char **)calloc((size_t)count, sizeof(char *));
        ERROR_CHECK_RETURN(names, GL_OUT_OF_MEMORY);

        for (GLsizei i = 0; i < count; i++)
        {
            // the caller may free its strings the moment this returns
            names[i] = varyings[i] ? strdup(varyings[i]) : NULL;

            if (names[i] == NULL)
            {
                while (i > 0)
                    free(names[--i]);

                free(names);

                ERROR_RETURN(GL_OUT_OF_MEMORY);
            }
        }
    }

    // GL 4.6 section 11.1.2.1: these belong to the program object and do not
    // take effect until it is linked again. MGL used to hang them on whatever
    // transform feedback object happened to be bound, so a program that set
    // its varyings before binding one lost them.
    for (GLsizei i = 0; i < prog->xfb_varying_count; i++)
        free(prog->xfb_varyings[i]);

    free(prog->xfb_varyings);

    prog->xfb_varyings = names;
    prog->xfb_varying_count = count;
    prog->xfb_buffer_mode = bufferMode;
}

void mglGetTransformFeedbackVarying(GLMContext ctx, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name)
{
    Program *prog = findProgram(ctx, program);
    GLsizei n;

    ERROR_CHECK_RETURN(prog, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(index < (GLuint)prog->xfb_varying_count, GL_INVALID_VALUE);

    if (size)
        *size = 1;

    // the linker does not keep the GLSL type of a captured varying
    if (type)
        *type = GL_NONE;

    if (length)
        *length = 0;

    if (name == NULL || bufSize == 0)
        return;

    n = (GLsizei)strlen(prog->xfb_varyings[index]);

    if (n > bufSize - 1)
        n = bufSize - 1;

    memcpy(name, prog->xfb_varyings[index], (size_t)n);
    name[n] = '\0';

    if (length)
        *length = n;
}

static void bindXfbBuffer(GLMContext ctx, GLuint xfb, GLuint index, GLuint buffer,
                          GLintptr offset, GLsizeiptr size, bool ranged)
{
    TransformFeedback *t = findTransformFeedback(ctx, xfb);
    Buffer *buf;

    ERROR_CHECK_RETURN(t, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(index < MAX_TF_BUFFERS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(t->active == GL_FALSE, GL_INVALID_OPERATION);

    if (buffer == 0)
    {
        memset(&t->buffers[index], 0, sizeof(BufferBaseTarget));

        return;
    }

    buf = findBuffer(ctx, buffer);
    ERROR_CHECK_RETURN(buf, GL_INVALID_OPERATION);

    if (ranged)
    {
        ERROR_CHECK_RETURN(offset >= 0, GL_INVALID_VALUE);
        ERROR_CHECK_RETURN(size > 0, GL_INVALID_VALUE);
        ERROR_CHECK_RETURN((offset & 3) == 0, GL_INVALID_VALUE);
        ERROR_CHECK_RETURN(offset + size <= (GLintptr)buf->size, GL_INVALID_VALUE);
    }
    else
    {
        offset = 0;
        size = 0;
    }

    t->buffers[index].buffer = buffer;
    t->buffers[index].offset = offset;
    t->buffers[index].size = size;
    t->buffers[index].buf = buf;
}

void mglTransformFeedbackBufferBase(GLMContext ctx, GLuint xfb, GLuint index, GLuint buffer)
{
    bindXfbBuffer(ctx, xfb, index, buffer, 0, 0, false);
}

void mglTransformFeedbackBufferRange(GLMContext ctx, GLuint xfb, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    bindXfbBuffer(ctx, xfb, index, buffer, offset, size, true);
}

void mglGetTransformFeedbackiv(GLMContext ctx, GLuint xfb, GLenum pname, GLint *param)
{
    TransformFeedback *t = xfbForName(ctx, xfb);

    ERROR_CHECK_RETURN(t, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    switch (pname)
    {
        case GL_TRANSFORM_FEEDBACK_PAUSED:
            *param = t->paused;
            break;

        case GL_TRANSFORM_FEEDBACK_ACTIVE:
            *param = t->active;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

void mglGetTransformFeedbacki_v(GLMContext ctx, GLuint xfb, GLenum pname, GLuint index, GLint *param)
{
    TransformFeedback *t = xfbForName(ctx, xfb);

    ERROR_CHECK_RETURN(t, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(index < MAX_TF_BUFFERS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(pname == GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, GL_INVALID_ENUM);

    *param = (GLint)t->buffers[index].buffer;
}

void mglGetTransformFeedbacki64_v(GLMContext ctx, GLuint xfb, GLenum pname, GLuint index, GLint64 *param)
{
    TransformFeedback *t = xfbForName(ctx, xfb);

    ERROR_CHECK_RETURN(t, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(index < MAX_TF_BUFFERS, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(param, GL_INVALID_VALUE);

    switch (pname)
    {
        case GL_TRANSFORM_FEEDBACK_BUFFER_START:
            *param = (GLint64)t->buffers[index].offset;
            break;

        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
            *param = (GLint64)t->buffers[index].size;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
    }
}

static void drawTransformFeedbackCommon(GLMContext ctx, GLenum mode, GLuint id, GLuint stream, GLsizei instancecount)
{
    TransformFeedback *t;

    ERROR_CHECK_RETURN(isPrimitiveMode(mode), GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(instancecount >= 0, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(stream < MAX_TF_BUFFERS, GL_INVALID_VALUE);

    t = findTransformFeedback(ctx, id);

    ERROR_CHECK_RETURN(t, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(t->active == GL_FALSE, GL_INVALID_OPERATION);

    // nothing was ever captured, so there is nothing to replay
    ERROR_RETURN(GL_INVALID_OPERATION);
}

void mglDrawTransformFeedback(GLMContext ctx, GLenum mode, GLuint id)
{
    drawTransformFeedbackCommon(ctx, mode, id, 0, 1);
}

void mglDrawTransformFeedbackInstanced(GLMContext ctx, GLenum mode, GLuint id, GLsizei instancecount)
{
    drawTransformFeedbackCommon(ctx, mode, id, 0, instancecount);
}

void mglDrawTransformFeedbackStream(GLMContext ctx, GLenum mode, GLuint id, GLuint stream)
{
    drawTransformFeedbackCommon(ctx, mode, id, stream, 1);
}

void mglDrawTransformFeedbackStreamInstanced(GLMContext ctx, GLenum mode, GLuint id, GLuint stream, GLsizei instancecount)
{
    drawTransformFeedbackCommon(ctx, mode, id, stream, instancecount);
}
