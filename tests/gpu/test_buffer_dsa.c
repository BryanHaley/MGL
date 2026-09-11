/*
 * test_buffer_dsa.c
 * MGL
 *
 * Direct State Access (DSA) buffer operations.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- glCreateBuffers ---------- */

GPU_TEST(buffer_dsa, create_buffers_returns_usable_names)
{
    GLuint b[4] = { 0 };

    glCreateBuffers(4, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // names must be non-zero and distinct
    for (int i = 0; i < 4; i++)
        CHECK_MSG(b[i] != 0, "create buffers[%d] returned 0", i);

    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            CHECK_MSG(b[i] != b[j], "create buffers %d and %d share name %u", i, j, b[i]);

    glDeleteBuffers(4, b);
}

GPU_TEST(buffer_dsa, create_buffers_rejects_negative_n)
{
    GLuint b = 0;

    glCreateBuffers(-1, &b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(buffer_dsa, create_buffers_with_zero_count_is_noop)
{
    GLuint sentinel = 0xABCD;

    glCreateBuffers(0, &sentinel);
    CHECK_EQ_UINT(sentinel, 0xABCD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(buffer_dsa, create_buffers_creates_unbound_objects)
{
    GLuint b = 0;
    GLint size = -1;

    glCreateBuffers(1, &b);
    CHECK(b != 0);

    // the buffer exists even without binding it anywhere
    CHECK_EQ_INT(glIsBuffer(b), GL_TRUE);

    // it has no size yet; giving it one via the DSA form
    glNamedBufferData(b, 256, NULL, GL_STATIC_DRAW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedBufferParameteriv(b, GL_BUFFER_SIZE, &size);
    CHECK_EQ_INT(size, 256);

    glDeleteBuffers(1, &b);
}

/* ---------- glNamedBufferData ---------- */

GPU_TEST(buffer_dsa, named_buffer_data_writes_and_reads_back)
{
    GLuint b = 0;
    float src[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float got[4] = { 0 };

    glCreateBuffers(1, &b);
    glNamedBufferData(b, sizeof src, src, GL_STATIC_DRAW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedBufferSubData(b, 0, sizeof got, got);
    for (int i = 0; i < 4; i++)
        CHECK_NEAR(got[i], src[i], 0.0f);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, named_buffer_data_rejects_unknown_name)
{
    glNamedBufferData(9999, 64, NULL, GL_STATIC_DRAW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(buffer_dsa, named_buffer_data_rejects_bad_usage)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, named_buffer_data_rejects_negative_size)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, -1, NULL, GL_STATIC_DRAW);
    // spec says GL_INVALID_VALUE for negative size
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, named_buffer_data_fails_on_immutable_buffer)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferStorage(b, 64, NULL, GL_DYNAMIC_STORAGE_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glNamedBufferData(b, 32, NULL, GL_STATIC_DRAW);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteBuffers(1, &b);
}

/* ---------- glNamedBufferSubData ---------- */

GPU_TEST(buffer_dsa, named_buffer_sub_data_partial_update)
{
    GLuint b = 0;
    int init[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int patch[3] = { 99, 100, 101 };
    int got[8] = { 0 };

    glCreateBuffers(1, &b);
    glNamedBufferData(b, sizeof init, init, GL_STATIC_DRAW);

    glNamedBufferSubData(b, 3 * sizeof(int), sizeof patch, patch);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedBufferSubData(b, 0, sizeof got, got);
    CHECK_EQ_INT(got[0], 0);
    CHECK_EQ_INT(got[1], 1);
    CHECK_EQ_INT(got[2], 2);
    CHECK_EQ_INT(got[3], 99);
    CHECK_EQ_INT(got[4], 100);
    CHECK_EQ_INT(got[5], 101);
    CHECK_EQ_INT(got[6], 6);
    CHECK_EQ_INT(got[7], 7);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, named_buffer_sub_data_rejects_bad_args)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    // unknown buffer
    glNamedBufferSubData(9999, 0, 16, (const int[]){ 0 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // negative offset
    glNamedBufferSubData(b, -1, 16, (const int[]){ 0 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // out of range
    glNamedBufferSubData(b, 32, 64, (const int[]){ 0 });
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
}

/* ---------- glNamedBufferStorage ---------- */

GPU_TEST(buffer_dsa, named_buffer_storage_creates_immutable_buffer)
{
    GLuint b = 0;
    float src[4] = { 10, 20, 30, 40 };
    float got[4] = { 0 };
    GLint immutable = -1;

    glCreateBuffers(1, &b);
    glNamedBufferStorage(b, sizeof src, src, GL_MAP_READ_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedBufferParameteriv(b, GL_BUFFER_IMMUTABLE_STORAGE, &immutable);
    CHECK_EQ_INT(immutable, GL_TRUE);

    glGetNamedBufferSubData(b, 0, sizeof got, got);
    for (int i = 0; i < 4; i++)
        CHECK_NEAR(got[i], src[i], 0.0f);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, named_buffer_storage_rejects_bad_flags)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferStorage(b, 64, NULL, 0xDEAD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, named_buffer_storage_rejects_negative_size)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferStorage(b, -1, NULL, GL_MAP_READ_BIT);
    // spec says GL_INVALID_VALUE for negative size
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, named_buffer_storage_rejects_unknown_name)
{
    glNamedBufferStorage(9999, 64, NULL, GL_MAP_READ_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- glCopyNamedBufferSubData ---------- */

GPU_TEST(buffer_dsa, copy_named_buffer_sub_data)
{
    GLuint src = 0, dst = 0;
    int init[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int got[4] = { 0 };

    glCreateBuffers(1, &src);
    glNamedBufferData(src, sizeof init, init, GL_STATIC_DRAW);

    glCreateBuffers(1, &dst);
    glNamedBufferData(dst, 64, NULL, GL_STATIC_DRAW);

    glCopyNamedBufferSubData(src, dst, 0, 0, 32);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedBufferSubData(dst, 0, sizeof got, got);
    CHECK_EQ_INT(got[0], 0);
    CHECK_EQ_INT(got[1], 1);
    CHECK_EQ_INT(got[2], 2);
    CHECK_EQ_INT(got[3], 3);

    glDeleteBuffers(1, &src);
    glDeleteBuffers(1, &dst);
}

GPU_TEST(buffer_dsa, copy_named_buffer_sub_data_rejects_bad_args)
{
    GLuint a = 0, b = 0;

    glCreateBuffers(1, &a);
    glNamedBufferData(a, 64, NULL, GL_STATIC_DRAW);
    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    // unknown source
    glCopyNamedBufferSubData(9999, b, 0, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // unknown dest
    glCopyNamedBufferSubData(a, 9999, 0, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // copy with zero size is valid
    glCopyNamedBufferSubData(a, b, 0, 0, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteBuffers(1, &a);
    glDeleteBuffers(1, &b);
}

/* ---------- glClearNamedBufferData / glClearNamedBufferSubData ---------- */

GPU_TEST(buffer_dsa, clear_named_buffer_data)
{
    GLuint b = 0;
    int init[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int got[8] = { 0 };
    const int clear_val = 0xFF;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, sizeof init, init, GL_STATIC_DRAW);

    // clear the whole buffer as GL_R32UI with a single uint value
    glClearNamedBufferData(b, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &clear_val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedBufferSubData(b, 0, sizeof got, got);
    CHECK_EQ_INT(got[0], 0xFF);
    CHECK_EQ_INT(got[7], 0xFF);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, clear_named_buffer_sub_data)
{
    GLuint b = 0;
    int init[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    int got[8] = { 0 };
    const int clear_val = 0xAA;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, sizeof init, init, GL_STATIC_DRAW);

    // clear elements 2-5 (bytes 8-23) as R32UI
    glClearNamedBufferSubData(b, GL_R32UI, 2 * sizeof(int), 4 * sizeof(int),
                              GL_RED_INTEGER, GL_UNSIGNED_INT, &clear_val);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glGetNamedBufferSubData(b, 0, sizeof got, got);
    CHECK_EQ_INT(got[0], 0);
    CHECK_EQ_INT(got[1], 1);
    CHECK_EQ_INT(got[2], 0xAA);
    CHECK_EQ_INT(got[3], 0xAA);
    CHECK_EQ_INT(got[4], 0xAA);
    CHECK_EQ_INT(got[5], 0xAA);
    CHECK_EQ_INT(got[6], 6);
    CHECK_EQ_INT(got[7], 7);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, clear_named_buffer_rejects_bad_format)
{
    GLuint b = 0;
    const int v = 0;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    glClearNamedBufferData(b, GL_R32UI, 0x9999, GL_UNSIGNED_INT, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, clear_named_buffer_rejects_unknown_name)
{
    const int v = 0;

    glClearNamedBufferData(9999, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- glMapNamedBuffer ---------- */

GPU_TEST(buffer_dsa, map_named_buffer_read_write)
{
    GLuint b = 0;
    float src[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float got[4] = { 0 };
    void *p;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, sizeof src, src, GL_STATIC_DRAW);

    p = glMapNamedBuffer(b, GL_READ_ONLY);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    if (!p) { glDeleteBuffers(1, &b); SKIP("map returned null"); }

    memcpy(got, p, sizeof got);
    glUnmapNamedBuffer(b);

    for (int i = 0; i < 4; i++)
        CHECK_NEAR(got[i], src[i], 0.0f);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, map_named_buffer_rejects_bad_access)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    glMapNamedBuffer(b, 0x9999);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_ENUM);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, map_named_buffer_rejects_unknown_name)
{
    void *p = glMapNamedBuffer(9999, GL_READ_ONLY);
    CHECK(p == NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

GPU_TEST(buffer_dsa, map_named_buffer_rejects_already_mapped)
{
    GLuint b = 0;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    glMapNamedBuffer(b, GL_READ_WRITE);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glMapNamedBuffer(b, GL_READ_ONLY);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUnmapNamedBuffer(b);
    glDeleteBuffers(1, &b);
}

/* ---------- glFlushMappedNamedBufferRange ---------- */

GPU_TEST(buffer_dsa, flush_mapped_named_buffer_range)
{
    GLuint b = 0;
    float data[4] = { 0 };
    void *p;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, sizeof data, NULL, GL_STATIC_DRAW);

    p = glMapNamedBufferRange(b, 0, sizeof data,
                              GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    if (!p) { glDeleteBuffers(1, &b); SKIP("map range returned null"); }

    memcpy(p, (const float[]){ 5.0f, 6.0f, 7.0f, 8.0f }, sizeof data);

    glFlushMappedNamedBufferRange(b, 0, sizeof data);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glUnmapNamedBuffer(b);

    glGetNamedBufferSubData(b, 0, sizeof data, data);
    CHECK_NEAR(data[0], 5.0f, 0.0f);
    CHECK_NEAR(data[3], 8.0f, 0.0f);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, flush_mapped_named_buffer_rejects_bad_args)
{
    GLuint b = 0;
    void *p;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    // unknown name
    glFlushMappedNamedBufferRange(9999, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // not mapped yet
    glFlushMappedNamedBufferRange(b, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    // map without GL_MAP_FLUSH_EXPLICIT_BIT
    p = glMapNamedBufferRange(b, 0, 64, GL_MAP_WRITE_BIT);
    CHECK(p != NULL);
    glFlushMappedNamedBufferRange(b, 0, 16);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glUnmapNamedBuffer(b);
    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, flush_mapped_named_buffer_validates_range)
{
    GLuint b = 0;
    void *p;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    p = glMapNamedBufferRange(b, 8, 32,
                              GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    CHECK(p != NULL);

    // flush past the mapped region
    glFlushMappedNamedBufferRange(b, 0, 48);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    // negative offset
    glFlushMappedNamedBufferRange(b, -1, 8);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);

    glUnmapNamedBuffer(b);
    glDeleteBuffers(1, &b);
}

/* ---------- glGetNamedBufferParameteriv ---------- */

GPU_TEST(buffer_dsa, get_named_buffer_parameteriv)
{
    GLuint b = 0;
    GLint size = -1, usage = -1, mapped = -1;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 512, NULL, GL_DYNAMIC_DRAW);

    glGetNamedBufferParameteriv(b, GL_BUFFER_SIZE, &size);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT(size, 512);

    glGetNamedBufferParameteriv(b, GL_BUFFER_USAGE, &usage);
    CHECK_EQ_INT(usage, GL_DYNAMIC_DRAW);

    glGetNamedBufferParameteriv(b, GL_BUFFER_MAPPED, &mapped);
    CHECK_EQ_INT(mapped, GL_FALSE);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, get_named_buffer_parameteriv_rejects_bad_pname)
{
    GLuint b = 0;
    GLint v = -1;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    glGetNamedBufferParameteriv(b, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, get_named_buffer_parameteriv_rejects_unknown_name)
{
    GLint v = -1;

    glGetNamedBufferParameteriv(9999, GL_BUFFER_SIZE, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- glGetNamedBufferParameteri64v ---------- */

GPU_TEST(buffer_dsa, get_named_buffer_parameteri64v)
{
    GLuint b = 0;
    GLint64 size = -1, immutable = -1;

    glCreateBuffers(1, &b);
    glNamedBufferStorage(b, 256, NULL, GL_MAP_READ_BIT);

    glGetNamedBufferParameteri64v(b, GL_BUFFER_SIZE, &size);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_EQ_INT((GLint)size, 256);

    glGetNamedBufferParameteri64v(b, GL_BUFFER_IMMUTABLE_STORAGE, &immutable);
    CHECK_EQ_INT((GLint)immutable, GL_TRUE);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, get_named_buffer_parameteri64v_rejects_bad_pname)
{
    GLuint b = 0;
    GLint64 v = -1;

    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    glGetNamedBufferParameteri64v(b, 0x9999, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);

    glDeleteBuffers(1, &b);
}

GPU_TEST(buffer_dsa, get_named_buffer_parameteri64v_rejects_unknown_name)
{
    GLint64 v = -1;

    glGetNamedBufferParameteri64v(9999, GL_BUFFER_SIZE, &v);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_OPERATION);
}

/* ---------- combined DSA lifecycle ---------- */

GPU_TEST(buffer_dsa, full_dsa_lifecycle_without_bind)
{
    GLuint b = 0;
    float data[4] = { 0 };
    void *p;

    // Create, populate, read back, map, unmap, all without glBindBuffer
    glCreateBuffers(1, &b);
    glNamedBufferData(b, 64, NULL, GL_STATIC_DRAW);

    glNamedBufferSubData(b, 0, sizeof data, (const float[]){ 1, 2, 3, 4 });
    glGetNamedBufferSubData(b, 0, sizeof data, data);
    CHECK_NEAR(data[0], 1.0f, 0.0f);
    CHECK_NEAR(data[3], 4.0f, 0.0f);

    p = glMapNamedBufferRange(b, 0, 64, GL_MAP_READ_BIT);
    CHECK(p != NULL);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    glUnmapNamedBuffer(b);

    glDeleteBuffers(1, &b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}
