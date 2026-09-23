/*
 * test_buffer_storage_map.c
 * MGL
 *
 * Immutable buffer storage (glBufferStorage), buffer clearing
 * (glClearBufferData, glClearBufferSubData), mapping (glMapBuffer,
 * glFlushMappedBufferRange) and multi-bind (glBindBuffersBase,
 * glBindBuffersRange).
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- glBufferStorage ---------- */

GPU_TEST(buffer_storage_map, immutable_storage_flags)
{
    GLuint b = 0;
    GLint immutable = -1, flags = -1, size = -1;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);

    glBufferStorage(GL_ARRAY_BUFFER, 256, NULL, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_IMMUTABLE_STORAGE, &immutable);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(immutable, GL_TRUE);

    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_STORAGE_FLAGS, &flags);
    CHECK_EQ_INT(flags, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
    CHECK_EQ_INT(size, 256);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, immutable_data_round_trip)
{
    GLuint b = 0;
    GLubyte src[16] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00 };
    GLubyte dst[16] = { 0 };

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);

    glBufferStorage(GL_ARRAY_BUFFER, sizeof src, src, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof dst, dst);
    for (int i = 0; i < 16; i++)
        CHECK_EQ_UINT(dst[i], src[i]);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, immutable_rejects_subdata_without_dynamic_bit)
{
    GLuint b = 0;
    GLubyte data[16] = { 0 };

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);

    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL, GL_MAP_READ_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glBufferSubData(GL_ARRAY_BUFFER, 0, 16, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}
GPU_TEST(buffer_storage_map, immutable_rejects_bad_flags)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);

    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL, 0xDEAD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, immutable_rejects_negative_size)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);

    glBufferStorage(GL_ARRAY_BUFFER, -1, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

/* ---------- glClearBufferData ---------- */

GPU_TEST(buffer_storage_map, clear_buffer_data_fills_entire_buffer)
{
    GLuint b = 0;
    GLubyte init[16];
    GLubyte dst[16];
    GLuint clear_val = 0x00;

    memset(init, 0xFF, sizeof init);
    memset(dst, 0x00, sizeof dst);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof init, init, GL_STATIC_DRAW);

    glClearBufferData(GL_ARRAY_BUFFER, GL_R8, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &clear_val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof dst, dst);
    for (int i = 0; i < 16; i++)
        CHECK_EQ_UINT(dst[i], 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, clear_buffer_data_rejects_bad_target)
{
    glClearBufferData(0x9999, GL_R8, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(buffer_storage_map, clear_buffer_data_rejects_no_buffer)
{
    GLuint clear_val = 0;

    glClearBufferData(GL_ARRAY_BUFFER, GL_R8, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &clear_val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- glClearBufferSubData ---------- */

GPU_TEST(buffer_storage_map, clear_buffer_subdata_fills_sub_range)
{
    GLuint b = 0;
    GLubyte init[32];
    GLubyte dst[32];
    GLuint clear_val = 0x00;

    memset(init, 0xFF, sizeof init);
    memset(dst, 0x00, sizeof dst);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof init, init, GL_STATIC_DRAW);

    glClearBufferSubData(GL_ARRAY_BUFFER, GL_R8, 8, 8, GL_RED_INTEGER,
                         GL_UNSIGNED_BYTE, &clear_val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof dst, dst);
    CHECK_EQ_UINT(dst[0], 0xFF);
    CHECK_EQ_UINT(dst[7], 0xFF);
    CHECK_EQ_UINT(dst[8], 0x00);
    CHECK_EQ_UINT(dst[15], 0x00);
    CHECK_EQ_UINT(dst[16], 0xFF);
    CHECK_EQ_UINT(dst[31], 0xFF);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, clear_buffer_subdata_rejects_bad_target)
{
    glClearBufferSubData(0x9999, GL_R8, 0, 16, GL_RED_INTEGER,
                         GL_UNSIGNED_BYTE, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- glMapBuffer ---------- */

GPU_TEST(buffer_storage_map, map_buffer_write_then_read_back)
{
    GLuint b = 0;
    GLubyte *ptr = NULL;
    GLubyte expected[32];
    GLubyte dst[32];

    for (int i = 0; i < 32; i++) expected[i] = (GLubyte)(i * 17);

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferStorage(GL_ARRAY_BUFFER, 32, NULL, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    ptr = (GLubyte *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    CHECK_MSG(ptr != NULL, "glMapBuffer returned NULL");
    if (!ptr) { glDeleteBuffers(1, &b); return; }

    memcpy(ptr, expected, 32);

    CHECK_EQ_INT(glUnmapBuffer(GL_ARRAY_BUFFER), GL_TRUE);

    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof dst, dst);
    for (int i = 0; i < 32; i++)
        CHECK_EQ_UINT(dst[i], expected[i]);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, map_buffer_rejects_already_mapped)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUnmapBuffer(GL_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, map_buffer_rejects_bad_access)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL, GL_MAP_READ_BIT);

    glMapBuffer(GL_ARRAY_BUFFER, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, map_buffer_rejects_no_buffer)
{
    void *p = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
    CHECK(p == NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    (void)p;
}

GPU_TEST(buffer_storage_map, map_buffer_rejects_bad_target)
{
    void *p = glMapBuffer(0x9999, GL_READ_ONLY);
    CHECK(p == NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
    (void)p;
}

/* ---------- glFlushMappedBufferRange ---------- */

GPU_TEST(buffer_storage_map, flush_mapped_range_requires_explicit_bit)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUnmapBuffer(GL_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, flush_mapped_range_checks_mapping_bounds)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL,
                    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glUnmapBuffer(GL_ARRAY_BUFFER);

    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, flush_mapped_range_rejects_negative_offset)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    glFlushMappedBufferRange(GL_ARRAY_BUFFER, -1, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, flush_mapped_range_rejects_negative_length)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, -1);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, flush_mapped_range_rejects_bad_target)
{
    glFlushMappedBufferRange(0x9999, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

/* ---------- glBindBuffersBase ---------- */

GPU_TEST(buffer_storage_map, bind_buffers_base_sets_indexed_bindings)
{
    GLuint b[2] = { 0 };
    GLint got = -1;

    glGenBuffers(2, b);
    glBindBuffer(GL_UNIFORM_BUFFER, b[0]);
    glBufferData(GL_UNIFORM_BUFFER, 128, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, b[1]);
    glBufferData(GL_UNIFORM_BUFFER, 256, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBuffersBase(GL_UNIFORM_BUFFER, 0, 2, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &got);
    CHECK_EQ_INT(got, (GLint)b[0]);

    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 1, &got);
    CHECK_EQ_INT(got, (GLint)b[1]);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glDeleteBuffers(2, b);
}

GPU_TEST(buffer_storage_map, bind_buffers_base_null_unbinds)
{
    GLuint b = 0;
    GLint got = -1;

    glGenBuffers(1, &b);
    glBindBuffer(GL_UNIFORM_BUFFER, b);
    glBufferData(GL_UNIFORM_BUFFER, 64, NULL, GL_STATIC_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, b);
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &got);
    CHECK_EQ_INT(got, (GLint)b);

    glBindBuffersBase(GL_UNIFORM_BUFFER, 0, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &got);
    CHECK_EQ_INT(got, 0);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, bind_buffers_base_rejects_bad_target)
{
    glBindBuffersBase(GL_ARRAY_BUFFER, 0, 1, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(buffer_storage_map, bind_buffers_base_rejects_bad_buffer_name)
{
    GLuint bad_name = 999123;

    glBindBuffersBase(GL_UNIFORM_BUFFER, 0, 1, &bad_name);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- glBindBuffersRange ---------- */

GPU_TEST(buffer_storage_map, bind_buffers_range_sets_indexed_bindings)
{
    GLuint b = 0;
    GLintptr offsets[1] = { 8 };
    GLsizeiptr sizes[1] = { 32 };
    GLint got_buf = -1;

    glGenBuffers(1, &b);
    glBindBuffer(GL_UNIFORM_BUFFER, b);
    glBufferData(GL_UNIFORM_BUFFER, 128, NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindBuffersRange(GL_UNIFORM_BUFFER, 0, 1, &b, offsets, sizes);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &got_buf);
    CHECK_EQ_INT(got_buf, (GLint)b);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, bind_buffers_range_null_unbinds)
{
    GLuint b = 0;
    GLint got = -1;

    glGenBuffers(1, &b);
    glBindBuffer(GL_UNIFORM_BUFFER, b);
    glBufferData(GL_UNIFORM_BUFFER, 64, NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, b);

    glBindBuffersRange(GL_UNIFORM_BUFFER, 0, 1, NULL, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &got);
    CHECK_EQ_INT(got, 0);

    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_storage_map, bind_buffers_range_rejects_bad_target)
{
    glBindBuffersRange(GL_ARRAY_BUFFER, 0, 1, NULL, NULL, NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);
}

GPU_TEST(buffer_storage_map, bind_buffers_range_rejects_bad_buffer_name)
{
    GLuint bad_name = 999123;
    GLintptr off = 0;
    GLsizeiptr sz = 16;

    glBindBuffersRange(GL_UNIFORM_BUFFER, 0, 1, &bad_name, &off, &sz);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- unmapping a persistent mapping ---------- */

GPU_TEST(buffer_storage_map, persistent_mapping_unmaps_cleanly)
{
    GLuint b = 0;
    unsigned char *p;
    GLint still_mapped = -1;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);

    glBufferStorage(GL_ARRAY_BUFFER, 256, NULL,
                    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    if (mgl_drain_errors() != GL_NO_ERROR) {
        glDeleteBuffers(1, &b);
        SKIP("persistent storage unavailable");
    }

    p = (unsigned char *)glMapBufferRange(GL_ARRAY_BUFFER, 0, 256,
                                          GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK(p != NULL);

    // the call under test: a mapped buffer is exactly what unmap is for
    CHECK_EQ_INT(glUnmapBuffer(GL_ARRAY_BUFFER), GL_TRUE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED, &still_mapped);
    CHECK_EQ_INT(still_mapped, GL_FALSE);

    // and a second unmap is the error case
    CHECK_EQ_INT(glUnmapBuffer(GL_ARRAY_BUFFER), GL_FALSE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

/* ---------- a persistent pointer survives the buffer reaching the GPU ---------- */

// The Metal side of a buffer used to be made by copying its memory into a new
// MTLBuffer and freeing the original, so a persistent mapping taken before the
// first GPU use pointed at freed pages afterwards.
GPU_TEST(buffer_storage_map, a_persistent_pointer_still_reads_after_a_gpu_copy)
{
    static const GLuint reference[2] = { 3, 1415927 };
    GLuint src = 0, dst = 0;
    GLuint *mapped;
    GLsync sync;

    glCreateBuffers(1, &src);
    glNamedBufferData(src, sizeof reference, reference, GL_STATIC_COPY);

    glCreateBuffers(1, &dst);
    glNamedBufferStorage(dst, sizeof reference, NULL,
                         GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    mapped = (GLuint *)glMapNamedBufferRange(dst, 0, sizeof reference,
                                             GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    CHECK(mapped != NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glCopyNamedBufferSubData(src, dst, 0, 0, sizeof reference);
    sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
    glDeleteSync(sync);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    if (mapped)
        CHECK_MSG(mapped[0] == reference[0] && mapped[1] == reference[1],
                  "read %u %u through the mapping, want %u %u",
                  mapped[0], mapped[1], reference[0], reference[1]);

    glUnmapNamedBuffer(dst);
    glDeleteBuffers(1, &src);
    glDeleteBuffers(1, &dst);
}
