/*
 * test_sync.c
 * MGL
 *
 * Sync objects (fences), memory barriers and texture barrier.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- fence sync: lifecycle ---------- */

GPU_TEST(sync, fence_create_and_query)
{
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK_MSG(s != NULL, "glFenceSync returned NULL for a valid call");
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // IsSync recognises it
    CHECK_EQ_INT(glIsSync(s), GL_TRUE);

    // Object type is always GL_SYNC_FENCE
    {
        GLsizei len = 0;
        GLint v = 0;
        glGetSynciv(s, GL_OBJECT_TYPE, 1, &len, &v);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(len, 1);
        CHECK_EQ_INT(v, GL_SYNC_FENCE);
    }

    // Sync condition is GPU_COMMANDS_COMPLETE
    {
        GLsizei len = 0;
        GLint v = 0;
        glGetSynciv(s, GL_SYNC_CONDITION, 1, &len, &v);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(v, GL_SYNC_GPU_COMMANDS_COMPLETE);
    }

    // Flags are always zero
    {
        GLsizei len = 0;
        GLint v = -1;
        glGetSynciv(s, GL_SYNC_FLAGS, 1, &len, &v);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_EQ_INT(v, 0);
    }

    // Sync status is either SIGNALED or UNSIGNALED
    {
        GLsizei len = 0;
        GLint v = -1;
        glGetSynciv(s, GL_SYNC_STATUS, 1, &len, &v);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_MSG(v == GL_SIGNALED || v == GL_UNSIGNALED,
                  "GL_SYNC_STATUS is 0x%x, expected SIGNALED or UNSIGNALED",
                  v);
    }

    glDeleteSync(s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // After delete, IsSync returns GL_FALSE
    CHECK_EQ_INT(glIsSync(s), GL_FALSE);
}

GPU_TEST(sync, fence_client_wait_and_server_wait)
{
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL);
    mgl_drain_errors();

    // Client wait with zero timeout and no flush bit
    // Returns ALREADY_SIGNALED, TIMEOUT_EXPIRED, or CONDITION_SATISFIED
    // Never WAIT_FAILED
    {
        GLenum status = glClientWaitSync(s, 0, 0);
        CHECK_MSG(status != GL_WAIT_FAILED,
                  "glClientWaitSync returned WAIT_FAILED for valid sync");
        CHECK_MSG(status == GL_ALREADY_SIGNALED ||
                  status == GL_CONDITION_SATISFIED ||
                  status == GL_TIMEOUT_EXPIRED,
                  "unexpected client wait status 0x%x", status);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    }

    // Server wait with GL_TIMEOUT_IGNORED is valid
    glWaitSync(s, 0, GL_TIMEOUT_IGNORED);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteSync(s);
}

GPU_TEST(sync, fence_rejects_bad_condition)
{
    // Invalid condition: must set GL_INVALID_ENUM and return NULL
    GLsync s = glFenceSync(0xDEAD, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
    CHECK_MSG(s == NULL,
              "glFenceSync with invalid condition must return NULL, "
              "per GL_INVALID_ENUM spec");
}

GPU_TEST(sync, fence_rejects_nonzero_flags)
{
    // Non-zero flags: must set GL_INVALID_VALUE and return NULL
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    CHECK_MSG(s == NULL,
              "glFenceSync with non-zero flags must return NULL, "
              "per GL_INVALID_VALUE spec");
}

GPU_TEST(sync, is_sync_returns_false_for_null)
{
    // glIsSync on NULL returns GL_FALSE, no error
    CHECK_EQ_INT(glIsSync(NULL), GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(sync, is_sync_returns_false_for_deleted_sync)
{
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL);
    mgl_drain_errors();

    glDeleteSync(s);
    mgl_drain_errors();

    CHECK_EQ_INT(glIsSync(s), GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(sync, delete_sync_accepts_null)
{
    // The spec says glDeleteSync silently ignores a value of zero.
    // GLsync is a pointer type, so zero is NULL.
    glDeleteSync(NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(sync, delete_sync_rejects_invalid_sync)
{
    // Create and delete a sync, then delete it again — the second
    // delete gets an invalid (non-zero, non-NULL) name and must
    // set GL_INVALID_VALUE.
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL);
    mgl_drain_errors();
    glDeleteSync(s);
    mgl_drain_errors();

    glDeleteSync(s);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- glClientWaitSync ---------- */

GPU_TEST(sync, client_wait_rejects_bad_flags)
{
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL);
    mgl_drain_errors();

    // Flags with bits outside GL_SYNC_FLUSH_COMMANDS_BIT:
    // must set GL_INVALID_VALUE and return GL_WAIT_FAILED
    GLenum status = glClientWaitSync(s, 0xFFFF, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    CHECK_MSG(status == GL_WAIT_FAILED,
              "glClientWaitSync with bad flags must return "
              "GL_WAIT_FAILED, got 0x%x", status);

    glDeleteSync(s);
}

GPU_TEST(sync, client_wait_rejects_invalid_sync)
{
    // An invalid sync must set GL_INVALID_VALUE and return GL_WAIT_FAILED
    GLenum status = glClientWaitSync(NULL, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    CHECK_MSG(status == GL_WAIT_FAILED,
              "glClientWaitSync(NULL) must return "
              "GL_WAIT_FAILED, got 0x%x", status);
}

/* ---------- glWaitSync ---------- */

GPU_TEST(sync, server_wait_rejects_nonzero_flags)
{
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL);
    mgl_drain_errors();

    // Non-zero flags: must set GL_INVALID_VALUE
    glWaitSync(s, 1, GL_TIMEOUT_IGNORED);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteSync(s);
}

GPU_TEST(sync, server_wait_rejects_bad_timeout)
{
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL);
    mgl_drain_errors();

    // timeout must be GL_TIMEOUT_IGNORED
    glWaitSync(s, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteSync(s);
}

GPU_TEST(sync, server_wait_rejects_invalid_sync)
{
    glWaitSync(NULL, 0, GL_TIMEOUT_IGNORED);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- glGetSynciv ---------- */

GPU_TEST(sync, get_synciv_rejects_invalid_sync)
{
    GLint v = 0;
    glGetSynciv(NULL, GL_OBJECT_TYPE, 1, NULL, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(sync, get_synciv_rejects_bad_pname)
{
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GLint v = 0;
    CHECK(s != NULL);
    mgl_drain_errors();

    glGetSynciv(s, 0xDEAD, 1, NULL, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteSync(s);
}

GPU_TEST(sync, get_synciv_rejects_negative_count)
{
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    GLint v = 0;
    CHECK(s != NULL);
    mgl_drain_errors();

    // Per OpenGL 4.6 spec §6.1.11, bufSize < 0 sets GL_INVALID_VALUE.
    // bufSize = 0 is valid (no values written, length set to 0).
    glGetSynciv(s, GL_OBJECT_TYPE, -1, NULL, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteSync(s);
}

/* ---------- glFinish / glFlush ---------- */

GPU_TEST(sync, finish_and_flush_are_observable)
{
    // Create a fence, flush to push it to the GPU, finish to wait,
    // then confirm the fence is already signaled — proving both
    // glFlush and glFinish did something.
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL);
    mgl_drain_errors();

    glFlush();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glFinish();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // After finish all commands including the fence are done,
    // so the sync must be signaled.
    {
        GLenum status = glClientWaitSync(s, 0, 0);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        CHECK_MSG(status == GL_ALREADY_SIGNALED,
                  "after glFinish, fence should be ALREADY_SIGNALED, "
                  "got 0x%x", status);
    }

    glDeleteSync(s);
}

GPU_TEST(sync, flush_with_sync_flush_bit)
{
    // ClientWaitSync with GL_SYNC_FLUSH_COMMANDS_BIT must flush
    // implicitly and not produce an error.
    GLsync s = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    CHECK(s != NULL);
    mgl_drain_errors();

    GLenum status = glClientWaitSync(s, GL_SYNC_FLUSH_COMMANDS_BIT,
                                     (GLuint64)1000000000ULL); // 1 sec
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(status != GL_WAIT_FAILED,
              "client wait with FLUSH_COMMANDS_BIT must not fail");
    CHECK_MSG(status != GL_TIMEOUT_EXPIRED,
              "1 second should be enough for a fence on a headless "
              "context; got TIMEOUT_EXPIRED. This may be a driver issue.");

    glDeleteSync(s);
}

/* ---------- glMemoryBarrier ---------- */

GPU_TEST(sync, memory_barrier_accepts_valid_bits)
{
    // Zero is valid
    glMemoryBarrier(0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Every individual valid bit must be accepted
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_ELEMENT_ARRAY_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_UNIFORM_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_PIXEL_BUFFER_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_TRANSFORM_FEEDBACK_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Combined valid bits
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
                    GL_UNIFORM_BARRIER_BIT |
                    GL_SHADER_STORAGE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // GL_ALL_BARRIER_BITS must be accepted
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Per spec, CLIENT_MAPPED_BUFFER and QUERY_BUFFER barriers are
    // also valid for glMemoryBarrier
    glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrier(GL_QUERY_BUFFER_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(sync, memory_barrier_rejects_undefined_bits)
{
    // An undefined bit must set GL_INVALID_VALUE
    glMemoryBarrier(0x10000000);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(sync, memory_barrier_by_region_accepts_valid_bits)
{
    // Zero is valid
    glMemoryBarrierByRegion(0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Each valid bit for ByRegion
    glMemoryBarrierByRegion(GL_ATOMIC_COUNTER_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrierByRegion(GL_FRAMEBUFFER_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrierByRegion(GL_SHADER_STORAGE_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrierByRegion(GL_TEXTURE_FETCH_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMemoryBarrierByRegion(GL_UNIFORM_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // Combined
    glMemoryBarrierByRegion(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                            GL_UNIFORM_BARRIER_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // GL_ALL_BARRIER_BITS covers the by-region subset
    glMemoryBarrierByRegion(GL_ALL_BARRIER_BITS);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(sync, memory_barrier_by_region_rejects_undefined_bits)
{
    // An undefined bit must set GL_INVALID_VALUE
    glMemoryBarrierByRegion(0x10000000);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

/* ---------- glTextureBarrier ---------- */

GPU_TEST(sync, texture_barrier)
{
    // glTextureBarrier has no errors per the spec.
    glTextureBarrier();
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}
