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

// A size that is not positive is a bad value, and storage is set only once.
// A negative size raised INVALID_OPERATION; zero and a second call went through.
GPU_TEST(buffer_storage_map, immutable_rejects_negative_size)
{
    GLuint b = 0;

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);

    glBufferStorage(GL_ARRAY_BUFFER, -1, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
    glBufferStorage(GL_ARRAY_BUFFER, 0, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glBufferStorage(GL_ARRAY_BUFFER, 16, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glBufferStorage(GL_ARRAY_BUFFER, 16, NULL, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glNamedBufferStorage(b, 16, NULL, 0);
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
    // a range starts on GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, which is 256
    GLintptr offsets[1] = { 256 };
    GLsizeiptr sizes[1] = { 32 };
    GLint got_buf = -1;

    glGenBuffers(1, &b);
    glBindBuffer(GL_UNIFORM_BUFFER, b);
    glBufferData(GL_UNIFORM_BUFFER, 512, NULL, GL_STATIC_DRAW);
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

// With no buffer bound there is nothing to fill; that is an error, not silence.
GPU_TEST(buffer_storage_map, buffer_data_with_nothing_bound_is_an_error)
{
    GLubyte data[4] = { 0 };

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBufferData(GL_ARRAY_BUFFER, sizeof data, data, GL_STATIC_DRAW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

// A plain mapping blocks writes only where it covers, and a persistent one
// blocks none. Every write was refused while any mapping was open.
GPU_TEST(buffer_storage_map, writes_beside_a_mapping_go_through)
{
    GLuint b = 0;
    GLubyte data[16] = { 0 };

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferStorage(GL_ARRAY_BUFFER, 64, NULL,
                    GL_DYNAMIC_STORAGE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);

    glMapBufferRange(GL_ARRAY_BUFFER, 16, 16, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 16, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glNamedBufferSubData(b, 32, 16, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glBufferSubData(GL_ARRAY_BUFFER, 8, 16, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
    glUnmapBuffer(GL_ARRAY_BUFFER);

    glMapBufferRange(GL_ARRAY_BUFFER, 16, 16, GL_MAP_PERSISTENT_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
    glBufferSubData(GL_ARRAY_BUFFER, 8, 16, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glUnmapBuffer(GL_ARRAY_BUFFER);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

// Only names glGenBuffers handed out can be bound, and glBufferSubData with
// nothing bound is an error. Both went through quietly.
GPU_TEST(buffer_storage_map, only_generated_names_bind)
{
    GLubyte data[4] = { 0 };

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof data, data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffer(GL_ARRAY_BUFFER, 987654);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

// GL 4.6 section 6.3: a zero length is an invalid operation, not a bad value
GPU_TEST(buffer_storage_map, mapping_nothing_is_an_invalid_operation)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_DYNAMIC_DRAW);
    mgl_drain_errors();

    CHECK(glMapNamedBufferRange(b, 0, 0, GL_MAP_READ_BIT) == NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffer(GL_ARRAY_BUFFER, b);
    CHECK(glMapBufferRange(GL_ARRAY_BUFFER, 16, 0, GL_MAP_WRITE_BIT) == NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

// A flush offset counts from the start of the mapped range, not the buffer
GPU_TEST(buffer_storage_map, flush_offset_is_relative_to_the_mapping)
{
    GLuint b = 0, got[16] = { 0 }, zero[16] = { 0 };

    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof zero, zero, GL_DYNAMIC_DRAW);

    GLuint *p = (GLuint *)glMapBufferRange(GL_ARRAY_BUFFER, 16, 32, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    CHECK(p != NULL);
    if (p)
    {
        for (int i = 0; i < 8; i++)
            p[i] = 100u + (GLuint)i;
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, 32);
        CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof got, got);
    CHECK_MSG(got[3] == 0 && got[4] == 100 && got[11] == 107 && got[12] == 0,
              "buffer holds %u %u %u %u", got[3], got[4], got[11], got[12]);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);
}

// Updates between draws in one frame reach only the draws after them. Each
// draw fills one quarter with the colour its uniform block held at the time.
GPU_TEST(buffer_storage_map, updates_between_draws_keep_their_order)
{
    static const char *vs =
        "#version 330 core\nlayout(location = 0) in vec2 p;\nvoid main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 420 core\nlayout(std140, binding = 0) uniform C { vec4 c; vec4 pad[511]; };\n"
        "out vec4 o;\nvoid main() { o = c; }\n";
    static const GLfloat red[4] = { 1, 0, 0, 1 }, green[4] = { 0, 1, 0, 1 }, blue[4] = { 0, 0, 1, 1 };
    static const GLfloat white[4] = { 1, 1, 1, 1 };
    static GLfloat big[2][2048];
    MGLTestTarget t;
    GLuint prog, vbo, vao, ubo[2];
    unsigned char *px, q[4][4];
    char log[1024];

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "did not build: %s", log);
    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
        return;

    vao = mgl_fullscreen_quad(&vbo);
    glCreateBuffers(2, ubo);
    // big enough to be bound as a buffer rather than copied into the command
    memcpy(big[0], red, sizeof red);
    memcpy(big[1], blue, sizeof blue);
    glNamedBufferStorage(ubo[0], sizeof big[0], big[0], GL_DYNAMIC_STORAGE_BIT);
    glNamedBufferStorage(ubo[1], sizeof big[1], big[1], GL_DYNAMIC_STORAGE_BIT);
    glUseProgram(prog);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo[0]);
    glEnable(GL_SCISSOR_TEST);

    // red, then green by sub-data, then white by clear, then blue by copy
    glScissor(0, 0, 8, 8);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glNamedBufferSubData(ubo[0], 0, 16, green);
    glScissor(8, 0, 8, 8);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glClearNamedBufferSubData(ubo[0], GL_RGBA32F, 0, 16, GL_RGBA, GL_FLOAT, white);
    glScissor(0, 8, 8, 8);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glCopyNamedBufferSubData(ubo[1], ubo[0], 0, 0, 16);
    glScissor(8, 8, 8, 8);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDisable(GL_SCISSOR_TEST);

    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 4, 4, q[0]);
    mgl_pixel_at(px, &t, 12, 4, q[1]);
    mgl_pixel_at(px, &t, 4, 12, q[2]);
    mgl_pixel_at(px, &t, 12, 12, q[3]);
    free(px);

    CHECK_MSG(q[0][0] == 255 && q[0][1] == 0, "first draw read %d %d %d, want red", q[0][0], q[0][1], q[0][2]);
    CHECK_MSG(q[1][0] == 0 && q[1][1] == 255, "second draw read %d %d %d, want green", q[1][0], q[1][1], q[1][2]);
    CHECK_MSG(q[2][0] == 255 && q[2][1] == 255 && q[2][2] == 255, "third draw read %d %d %d, want white",
              q[2][0], q[2][1], q[2][2]);
    CHECK_MSG(q[3][0] == 0 && q[3][2] == 255, "fourth draw read %d %d %d, want blue", q[3][0], q[3][1], q[3][2]);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(2, ubo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}

// A storage buffer that is also the indirect buffer, rewritten whole before
// each draw: each draw keeps its own command and colour
GPU_TEST(buffer_storage_map, whole_rewrites_of_a_storage_buffer_keep_each_draw)
{
    static const char *vs =
        "#version 330 core\nlayout(location = 0) in vec2 p;\nvoid main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs =
        "#version 430 core\nlayout(std430, binding = 0) buffer B { vec4 c; };\n"
        "out vec4 o;\nvoid main() { o = c; }\n";
    // larger than the copy-on-write limit
    enum { SIZE = 512 * 1024 };
    static const GLfloat colors[4][4] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 1, 1, 1, 1 } };
    MGLTestTarget t;
    GLuint prog, vbo, vao, buf;
    unsigned char *px, q[4][4], *data;
    char log[1024];

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "did not build: %s", log);
    data = calloc(1, SIZE);
    if (!prog || !data || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
    {
        free(data);
        return;
    }

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    vao = mgl_fullscreen_quad(&vbo);
    glCreateBuffers(1, &buf);
    glNamedBufferData(buf, SIZE, data, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buf);
    glUseProgram(prog);
    glEnable(GL_SCISSOR_TEST);

    // colour at 0, a draw command at 256 that only draws in the last pass
    for (int k = 0; k < 4; k++)
    {
        GLuint cmd[4] = { k == 3 ? 6 : 0, 1, 0, 0 };

        memcpy(data, colors[k], sizeof colors[k]);
        memcpy(data + 256, cmd, sizeof cmd);
        glNamedBufferSubData(buf, 0, SIZE, data);
        glScissor((k & 1) * 8, (k >> 1) * 8, 8, 8);
        if (k < 3)
            glDrawArrays(GL_TRIANGLES, 0, 6);
        else
            glDrawArraysIndirect(GL_TRIANGLES, (const void *)256);
    }
    glDisable(GL_SCISSOR_TEST);

    px = mgl_read_rgba8(&t);
    for (int k = 0; k < 4; k++)
        mgl_pixel_at(px, &t, (k & 1) * 8 + 4, (k >> 1) * 8 + 4, q[k]);
    free(px);

    for (int k = 0; k < 4; k++)
        CHECK_MSG(q[k][0] == (colors[k][0] ? 255 : 0) && q[k][1] == (colors[k][1] ? 255 : 0) &&
                  q[k][2] == (colors[k][2] ? 255 : 0),
                  "draw %d read %d %d %d", k, q[k][0], q[k][1], q[k][2]);

    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &buf);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
    free(data);
}

// One vertex and index buffer refilled before each draw, the way UI
// renderers stream their lists; every draw keeps the geometry it was given
GPU_TEST(buffer_storage_map, refilled_vertex_and_index_buffers_keep_each_draw)
{
    static const char *vs =
        "#version 330 core\nlayout(location = 0) in vec2 p;\nvoid main() { gl_Position = vec4(p, 0.0, 1.0); }\n";
    static const char *fs = "#version 330 core\nout vec4 o;\nvoid main() { o = vec4(1.0); }\n";
    MGLTestTarget t;
    GLuint prog, vao, vbo, ibo;
    unsigned char *px, q[4][4];
    char log[1024];

    prog = mgl_build_program(vs, fs, log, sizeof log);
    CHECK_MSG(prog != 0, "did not build: %s", log);
    if (!prog || !mgl_target_create(&t, 16, 16, GL_RGBA8, 0))
        return;

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glCreateVertexArrays(1, &vao);
    glCreateBuffers(1, &vbo);
    glCreateBuffers(1, &ibo);
    glNamedBufferData(vbo, 4096, NULL, GL_STREAM_DRAW);
    glNamedBufferData(ibo, 1024, NULL, GL_STREAM_DRAW);
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, 2 * sizeof(GLfloat));
    glVertexArrayElementBuffer(vao, ibo);
    glEnableVertexArrayAttrib(vao, 0);
    glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(vao, 0, 0);
    glBindVertexArray(vao);
    glUseProgram(prog);

    // a quad in each quarter, in turn; the indices change order each time too
    for (int k = 0; k < 4; k++)
    {
        GLfloat x = (k & 1) ? 0.0f : -1.0f, y = (k & 2) ? 0.0f : -1.0f;
        GLfloat v[8] = { x, y, x + 1, y, x + 1, y + 1, x, y + 1 };
        GLushort idx[2][6] = { { 0, 1, 2, 0, 2, 3 }, { 3, 2, 0, 2, 1, 0 } };

        glNamedBufferSubData(vbo, 0, sizeof v, v);
        glNamedBufferSubData(ibo, 0, sizeof idx[k & 1], idx[k & 1]);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
    }

    px = mgl_read_rgba8(&t);
    mgl_pixel_at(px, &t, 4, 4, q[0]);
    mgl_pixel_at(px, &t, 12, 4, q[1]);
    mgl_pixel_at(px, &t, 4, 12, q[2]);
    mgl_pixel_at(px, &t, 12, 12, q[3]);
    free(px);

    for (int k = 0; k < 4; k++)
        CHECK_MSG(q[k][0] == 255, "quarter %d read %d, want the quad drawn there", k, q[k][0]);

    glUseProgram(0);
    glBindVertexArray(0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteProgram(prog);
    mgl_target_destroy(&t);
}
