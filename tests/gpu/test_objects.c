/*
 * test_objects.c
 * MGL
 *
 * Object name allocation and the hash tables behind it. Program pipelines and
 * transform feedbacks had no table at all, so the first Gen call spun forever
 * inside insertHashElement; these pin that down.
 */

#include "mgl_test.h"
#include "harness.h"

/* ---------- the two tables that were never initialised ---------- */

GPU_TEST(objects, gen_program_pipelines_returns_usable_names)
{
    GLuint p[4] = { 99, 99, 99, 99 };

    glGenProgramPipelines(4, p);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 4; i++)
        CHECK_MSG(p[i] != 0, "pipeline %d got name 0, which GL reserves", i);

    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            CHECK_MSG(p[i] != p[j], "pipelines %d and %d share name %u", i, j, p[i]);

    glDeleteProgramPipelines(4, p);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(objects, bind_program_pipeline_roundtrips)
{
    GLuint p = 0;

    glGenProgramPipelines(1, &p);
    if (!p) SKIP("no pipeline name");

    glBindProgramPipeline(p);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(glIsProgramPipeline(p) == GL_TRUE, "pipeline %u not reported as one", p);

    glBindProgramPipeline(0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteProgramPipelines(1, &p);
    CHECK_MSG(glIsProgramPipeline(p) == GL_FALSE, "pipeline %u still live after delete", p);
}

GPU_TEST(objects, gen_transform_feedbacks_returns_usable_names)
{
    GLuint t[4] = { 99, 99, 99, 99 };

    glGenTransformFeedbacks(4, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    for (int i = 0; i < 4; i++)
        CHECK_MSG(t[i] != 0, "transform feedback %d got name 0", i);

    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            CHECK_MSG(t[i] != t[j], "feedbacks %d and %d share name %u", i, j, t[i]);

    glDeleteTransformFeedbacks(4, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(objects, bind_transform_feedback_roundtrips)
{
    GLuint t = 0;

    glGenTransformFeedbacks(1, &t);
    if (!t) SKIP("no transform feedback name");

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, t);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
    CHECK_MSG(glIsTransformFeedback(t) == GL_TRUE, "feedback %u not reported as one", t);

    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    glDeleteTransformFeedbacks(1, &t);
}

/* Growing an empty table used to be impossible, because the loop doubled a
   zero. A high explicit name forces that growth path. */
GPU_TEST(objects, high_object_names_grow_the_table)
{
    GLuint p = 0;

    glGenProgramPipelines(1, &p);
    if (!p) SKIP("no pipeline name");

    glBindProgramPipeline(p);
    glBindProgramPipeline(0);
    glDeleteProgramPipelines(1, &p);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

/* ---------- name allocation across every object type ---------- */

GPU_TEST(objects, no_object_type_hands_out_name_zero)
{
    GLuint buf = 0, tex = 0, vao = 0, fbo = 0, rbo = 0, smp = 0;

    glGenBuffers(1, &buf);
    glGenTextures(1, &tex);
    glGenVertexArrays(1, &vao);
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);
    glGenSamplers(1, &smp);

    CHECK_MSG(buf != 0, "glGenBuffers returned 0");
    CHECK_MSG(tex != 0, "glGenTextures returned 0");
    CHECK_MSG(vao != 0, "glGenVertexArrays returned 0");
    CHECK_MSG(fbo != 0, "glGenFramebuffers returned 0");
    CHECK_MSG(rbo != 0, "glGenRenderbuffers returned 0");
    CHECK_MSG(smp != 0, "glGenSamplers returned 0");

    glDeleteBuffers(1, &buf);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fbo);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteSamplers(1, &smp);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(objects, generated_names_are_unique_in_bulk)
{
    enum { N = 64 };
    GLuint b[N];
    int dupes = 0, zeros = 0;

    glGenBuffers(N, b);

    for (int i = 0; i < N; i++)
    {
        if (b[i] == 0) zeros++;

        for (int j = i + 1; j < N; j++)
            if (b[i] == b[j]) dupes++;
    }

    CHECK_MSG(zeros == 0, "%d of %d buffer names were 0", zeros, N);
    CHECK_MSG(dupes == 0, "%d duplicate buffer names in %d", dupes, N);

    glDeleteBuffers(N, b);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(objects, deleting_zero_is_silently_ignored)
{
    GLuint zero = 0;

    // GL says a name of 0 is ignored by every Delete call, not an error
    glDeleteBuffers(1, &zero);
    glDeleteTextures(1, &zero);
    glDeleteFramebuffers(1, &zero);
    glDeleteProgramPipelines(1, &zero);
    glDeleteTransformFeedbacks(1, &zero);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(objects, gen_with_zero_count_is_a_noop)
{
    GLuint sentinel = 0xABCD;

    glGenBuffers(0, &sentinel);
    glGenProgramPipelines(0, &sentinel);
    glGenTransformFeedbacks(0, &sentinel);

    CHECK_EQ_UINT(sentinel, 0xABCD);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(objects, negative_gen_count_errors)
{
    GLuint one = 0;

    glGenBuffers(-1, &one);
    CHECK_EQ_UINT(mgl_drain_errors(), GL_INVALID_VALUE);
}

GPU_TEST(objects, is_queries_reject_unknown_names)
{
    // never generated, so none of these should claim the name
    CHECK(glIsBuffer(4242) == GL_FALSE);
    CHECK(glIsTexture(4242) == GL_FALSE);
    CHECK(glIsFramebuffer(4242) == GL_FALSE);
    CHECK(glIsProgramPipeline(4242) == GL_FALSE);
    CHECK(glIsTransformFeedback(4242) == GL_FALSE);

    mgl_drain_errors();
}

/* ---------- deleted names come back ---------- */

GPU_TEST(objects, deleted_names_are_reused)
{
    // Without a free list every create/delete cycle pushed the next name up,
    // and the hash table grew with it for as long as the app ran. A game that
    // churns objects per frame reached tens of thousands of names in seconds.
    GLuint peak = 0;

    for (int round = 0; round < 50; round++)
    {
        GLuint b[20];

        glGenBuffers(20, b);

        for (int i = 0; i < 20; i++)
        {
            glBindBuffer(GL_ARRAY_BUFFER, b[i]);
            glBufferData(GL_ARRAY_BUFFER, 64, NULL, GL_STATIC_DRAW);

            if (b[i] > peak)
                peak = b[i];
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(20, b);
    }

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);

    // 1000 buffers came and went, but only 20 were ever alive at once
    CHECK_MSG(peak < 200, "names ran to %u for 20 live buffers, so nothing is being reused", peak);
}

GPU_TEST(objects, reused_names_never_collide_with_live_objects)
{
    GLuint live[64];

    glGenBuffers(64, live);

    for (int i = 0; i < 64; i++)
    {
        glBindBuffer(GL_ARRAY_BUFFER, live[i]);
        glBufferData(GL_ARRAY_BUFFER, 16, NULL, GL_STATIC_DRAW);
    }

    for (int i = 0; i < 64; i++)
        for (int j = i + 1; j < 64; j++)
            CHECK_MSG(live[i] != live[j], "live buffers %d and %d share name %u", i, j, live[i]);

    // free one, then bring the same name back by binding it directly -- a
    // later Gen must not hand that name out while it is in use again
    GLuint victim = live[7];

    glDeleteBuffers(1, &victim);
    glBindBuffer(GL_ARRAY_BUFFER, victim);
    glBufferData(GL_ARRAY_BUFFER, 16, NULL, GL_STATIC_DRAW);

    GLuint fresh[8];
    glGenBuffers(8, fresh);

    for (int i = 0; i < 8; i++)
        CHECK_MSG(fresh[i] != victim, "name %u was handed out while still in use", victim);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(8, fresh);
    glDeleteBuffers(64, live);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}

GPU_TEST(objects, reused_name_still_holds_data)
{
    GLuint a = 0, b = 0;
    float in[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    float out[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    glGenBuffers(1, &a);
    glBindBuffer(GL_ARRAY_BUFFER, a);
    glBufferData(GL_ARRAY_BUFFER, sizeof(in), in, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &a);

    // whatever name this gets, most likely the one just freed, it must be a
    // clean buffer and not carry anything over from the old object
    glGenBuffers(1, &b);
    glBindBuffer(GL_ARRAY_BUFFER, b);
    glBufferData(GL_ARRAY_BUFFER, sizeof(in), in, GL_STATIC_DRAW);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(out), out);

    for (int i = 0; i < 4; i++)
        CHECK_NEAR(out[i], in[i], 0.0f);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(1, &b);

    CHECK_EQ_UINT(mgl_drain_errors(), GL_NO_ERROR);
}
