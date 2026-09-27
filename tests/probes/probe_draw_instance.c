/*
 * probe_draw_instance.c
 * Copyright (C) The MooGL Project
 *
 * P6.1 diagnosis probe: what a shader actually sees for gl_InstanceID,
 * gl_BaseInstance and gl_DrawID under MGL's multi-draw indirect path.
 *
 * This is a diagnosis, not a conformance test. It draws into an offscreen
 * target with a vertex shader that records (gl_DrawID, gl_InstanceID,
 * gl_BaseInstance, gl_VertexID) for every invocation into an SSBO, then reads
 * the buffer back and prints what the shader saw. It then says what the GL
 * 4.6 core specification requires and whether MGL matched.
 *
 * What GL 4.6 requires (section 11.1.3.9, "Shader Inputs"):
 *
 *   gl_InstanceID   the instance counter within the draw, always starting at
 *                   zero and running to <instancecount> - 1. It does NOT
 *                   include <baseinstance>.
 *   gl_BaseInstance the value passed as <baseinstance> (0 when the command
 *                   has no such parameter).
 *   gl_DrawID       the zero-based index of the draw within the list a
 *                   MultiDraw* command processes; 0 for a non-multi draw.
 *
 * GL_ARB_base_instance, issue 1, puts the first rule beyond doubt:
 *   "Does <baseinstance> offset gl_InstanceID?
 *    RESOLVED: No. gl_InstanceID always starts from zero and counts up by one
 *    for each instance rendered."
 *
 * Metal's [[instance_id]] does the opposite: it runs from <baseInstance>
 * through <baseInstance> + <instanceCount> - 1. A GL-to-Metal translation has
 * to subtract gl_BaseInstance from it. This probe shows whether MGL does.
 *
 * Build:   make probe        (or: make build/probe_draw_instance)
 * Run:     build/probe_draw_instance
 * Exit code is the number of expectations that failed.
 */

#include <stdio.h>
#include <string.h>

#define GL_GLEXT_PROTOTYPES 1
#include "glcorearb.h"

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif
#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

#define W 16
#define H 16

/* The quad is six indices, and every instance runs all of them, so a draw
   with N instances makes N * 6 shader invocations. */
#define INDEX_COUNT 6
#define INSTANCES 3

/* one record per shader invocation */
#define REC_WORDS 4
#define REC_DRAWID 0
#define REC_INSTANCEID 1
#define REC_BASEINSTANCE 2
#define REC_VERTEXID 3
#define MAX_RECORDS 128

static int fails;

static void check(const char *what, int ok, const char *detail)
{
    printf("  [%s] %s%s%s\n", ok ? "ok  " : "FAIL", what,
           detail && *detail ? " -- " : "", detail ? detail : "");
    if (!ok)
        fails++;
}

/* The GL indirect command, exactly as the spec lays it out. */
typedef struct {
    unsigned int count;
    unsigned int instanceCount;
    unsigned int firstIndex;
    int          baseVertex;
    unsigned int baseInstance;
} DrawElementsIndirectCmd;

/*
 * The vertex shader records one four-word record per invocation. The slot is
 * claimed with an atomicAdd on a counter so the probe does not have to trust
 * any of the values it is trying to measure.
 */
static const char *VS_RECORD =
"#version 460 core\n"
"layout(location = 0) in vec2 pos;\n"
"layout(std430, binding = 0) buffer Records { uint words[]; };\n"
"layout(std430, binding = 1) buffer Counter { uint count; };\n"
"void main() {\n"
"    uint slot = atomicAdd(count, 1u);\n"
"    words[slot * 4 + 0] = uint(gl_DrawID);\n"
"    words[slot * 4 + 1] = uint(gl_InstanceID);\n"
"    words[slot * 4 + 2] = uint(gl_BaseInstance);\n"
"    words[slot * 4 + 3] = uint(gl_VertexID);\n"
"    gl_Position = vec4(pos, 0.0, 1.0);\n"
"}\n";

static const char *FS_SOLID =
"#version 460 core\n"
"layout(location = 0) out vec4 frag;\n"
"void main() { frag = vec4(1.0); }\n";

static GLuint build_program(char *log, int log_size)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint p = glCreateProgram();
    GLint ok = 0;

    glShaderSource(vs, 1, &VS_RECORD, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(vs, log_size, NULL, log);
        return 0;
    }

    glShaderSource(fs, 1, &FS_SOLID, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(fs, log_size, NULL, log);
        return 0;
    }

    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        glGetProgramInfoLog(p, log_size, NULL, log);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return p;
}

/* Reads back the records and prints them as a table. Returns how many were
   written (capped at max_words / REC_WORDS). */
static unsigned int dump_records(GLuint recbuf, GLuint counter,
                                 unsigned int *out, unsigned int max_words)
{
    unsigned int count = 0;
    unsigned int cap = max_words / REC_WORDS;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counter);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof count, &count);

    if (count > cap)
        count = cap;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, recbuf);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                       (GLsizeiptr)(count * REC_WORDS * sizeof(unsigned int)), out);

    printf("    %-6s %-11s %-13s %-9s\n", "DrawID", "InstanceID", "BaseInstance", "VertexID");
    for (unsigned int i = 0; i < count; i++)
        printf("    %-6u %-11u %-13u %-9u\n",
               out[i * REC_WORDS + REC_DRAWID],
               out[i * REC_WORDS + REC_INSTANCEID],
               out[i * REC_WORDS + REC_BASEINSTANCE],
               out[i * REC_WORDS + REC_VERTEXID]);

    return count;
}

/* Are the records for one draw exactly {instanceID in [0,instanceCount)} for
   every vertex, with the given base instance and draw id? */
static int draw_matches(const unsigned int *rec, unsigned int n,
                        unsigned int want_drawid, unsigned int want_base,
                        unsigned int instance_count, unsigned int vertex_count,
                        char *why, int why_size)
{
    unsigned int seen_instance[64];
    unsigned int seen = 0, matching = 0;

    memset(seen_instance, 0, sizeof seen_instance);

    for (unsigned int i = 0; i < n; i++) {
        const unsigned int *r = &rec[i * REC_WORDS];

        if (r[REC_BASEINSTANCE] != want_base)
            continue;

        matching++;

        if (r[REC_DRAWID] != want_drawid) {
            snprintf(why, why_size, "gl_DrawID = %u, expected %u", r[REC_DRAWID], want_drawid);
            return 0;
        }

        if (r[REC_INSTANCEID] >= instance_count) {
            snprintf(why, why_size,
                     "gl_InstanceID = %u, outside [0,%u) -- base instance leaked in",
                     r[REC_INSTANCEID], instance_count);
            return 0;
        }

        if (seen < 64)
            seen_instance[seen++] = r[REC_INSTANCEID];
    }

    if (matching != instance_count * vertex_count) {
        snprintf(why, why_size, "%u invocations with baseInstance %u, expected %u",
                 matching, want_base, instance_count * vertex_count);
        return 0;
    }

    /* every instance value in [0,instance_count) must have been seen */
    for (unsigned int want = 0; want < instance_count; want++) {
        int found = 0;

        for (unsigned int s = 0; s < seen; s++)
            if (seen_instance[s] == want)
                found = 1;

        if (!found) {
            snprintf(why, why_size, "gl_InstanceID = %u was never produced", want);
            return 0;
        }
    }

    return 1;
}

/* Clears the counter and the records before a draw. */
static void reset_records(GLuint recbuf, GLuint counter)
{
    unsigned int zero = 0;
    unsigned int clear[MAX_RECORDS * REC_WORDS];

    memset(clear, 0, sizeof clear);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counter);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof zero, &zero);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, recbuf);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof clear, clear);
}

int main(void)
{
    GLuint prog, vao, vbo, ibo, recbuf, counter, indirect = 0;
    GLuint fbo, tex;
    char log[2048] = "";
    unsigned int records[MAX_RECORDS * REC_WORDS];
    unsigned int count;
    char why[160];

    /* vertex data: a quad, indexed */
    static const float verts[] = {
        -1.0f, -1.0f,   1.0f, -1.0f,   -1.0f, 1.0f,   1.0f, 1.0f
    };
    static const unsigned int indices[] = { 0, 1, 2, 2, 1, 3 };

    printf("MGL draw-instance / draw-id probe\n");
    printf("  renderer: %s\n\n", (const char *)glGetString(GL_RENDERER));

    /* offscreen target */
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, W, H);

    prog = build_program(log, sizeof log);
    if (!prog) {
        printf("  shader build failed: %s\n", log);
        return 1;
    }

    /* VAO with the attribute and the element array */
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices, GL_STATIC_DRAW);

    /* the record buffer and its invocation counter */
    glGenBuffers(1, &recbuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, recbuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof records, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, recbuf);

    glGenBuffers(1, &counter);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counter);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, counter);

    glUseProgram(prog);

    /* ------------------------------------------------------------------ */
    /* 1. a single instanced draw with a non-zero base instance           */
    /* ------------------------------------------------------------------ */
    printf("1. glDrawElementsInstancedBaseInstance, %d instances, baseInstance = 7\n",
           INSTANCES);
    printf("   GL 4.6 expects gl_InstanceID in {0,1,2} and gl_BaseInstance = 7\n");

    reset_records(recbuf, counter);

    glDrawElementsInstancedBaseInstance(GL_TRIANGLES, INDEX_COUNT, GL_UNSIGNED_INT,
                                        NULL, INSTANCES, 7);
    glFinish();

    count = dump_records(recbuf, counter, records, sizeof records / sizeof records[0]);

    check("18 invocations recorded (3 instances x 6 vertices)",
          count == INSTANCES * INDEX_COUNT, NULL);

    memset(why, 0, sizeof why);
    check("gl_InstanceID starts at 0, gl_BaseInstance = 7",
          count == INSTANCES * INDEX_COUNT &&
          draw_matches(records, count, 0, 7, INSTANCES, INDEX_COUNT, why, sizeof why),
          why);

    /* ------------------------------------------------------------------ */
    /* 2. multi-draw indirect, two commands, base instances 5 and 10      */
    /* ------------------------------------------------------------------ */
    printf("\n2. glMultiDrawElementsIndirect, 2 commands, baseInstance 5 and 10\n");
    printf("   GL 4.6 expects draw 0: gl_DrawID=0, gl_InstanceID in {0,1,2}, gl_BaseInstance=5\n");
    printf("                draw 1: gl_DrawID=1, gl_InstanceID in {0,1,2}, gl_BaseInstance=10\n");

    {
        DrawElementsIndirectCmd cmds[2];

        memset(cmds, 0, sizeof cmds);
        cmds[0].count = INDEX_COUNT; cmds[0].instanceCount = INSTANCES; cmds[0].firstIndex = 0;
        cmds[0].baseVertex = 0; cmds[0].baseInstance = 5;
        cmds[1].count = INDEX_COUNT; cmds[1].instanceCount = INSTANCES; cmds[1].firstIndex = 0;
        cmds[1].baseVertex = 0; cmds[1].baseInstance = 10;

        glGenBuffers(1, &indirect);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof cmds, cmds, GL_STATIC_DRAW);

        reset_records(recbuf, counter);

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, NULL, 2,
                                    (GLsizei)sizeof(DrawElementsIndirectCmd));
        glFinish();

        count = dump_records(recbuf, counter, records, sizeof records / sizeof records[0]);

        check("36 invocations recorded across both draws (2 x 3 instances x 6 vertices)",
              count == 2 * INSTANCES * INDEX_COUNT, NULL);

        memset(why, 0, sizeof why);
        check("draw 0: gl_DrawID = 0, gl_InstanceID in {0,1,2}, gl_BaseInstance = 5",
              count == 2 * INSTANCES * INDEX_COUNT &&
              draw_matches(records, count, 0, 5, INSTANCES, INDEX_COUNT, why, sizeof why),
              why);

        memset(why, 0, sizeof why);
        check("draw 1: gl_DrawID = 1, gl_InstanceID in {0,1,2}, gl_BaseInstance = 10",
              count == 2 * INSTANCES * INDEX_COUNT &&
              draw_matches(records, count, 1, 10, INSTANCES, INDEX_COUNT, why, sizeof why),
              why);
    }

    printf("\n%d expectation(s) not met\n", fails);

    glBindVertexArray(0);
    if (indirect)
        glDeleteBuffers(1, &indirect);
    glDeleteBuffers(1, &counter);
    glDeleteBuffers(1, &recbuf);
    glDeleteBuffers(1, &ibo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);

    return fails;
}
