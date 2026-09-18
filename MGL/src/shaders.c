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
 * shaders.c
 * MGL
 *
 */

#include <stdio.h>
#include <string.h>
#include <glslang_c_interface.h>
#include <glslang_c_shader_types.h>

#include "shaders.h"
#include <ctype.h>
#include "mgl_reflect.h"
#include "glm_context.h"
#include "mgl_log.h"

const glslang_resource_t* glslang_default_resource(void);

const char *getShaderTypeStr(GLuint type)
{
    static const char *types[] = {"VERTEX_SHADER", "FRAGMENT_SHADER",
        "GEOMETRY_SHADER", "TESS_CONTROL_SHADER", "TESS_EVALUATION_SHADER",
        "COMPUTE_SHADER", "MAX_SHADER_TYPES", NULL};

    if (type < 0 || type >= _MAX_SHADER_TYPES)
        return "UNKNOWN_SHADER_TYPE";

    return types[type];
};

GLuint glShaderTypeToGLMType(GLuint type)
{
    switch(type) {
        case GL_VERTEX_SHADER: return _VERTEX_SHADER;
        case GL_FRAGMENT_SHADER: return _FRAGMENT_SHADER;
        case GL_GEOMETRY_SHADER: return _GEOMETRY_SHADER;
        case GL_TESS_CONTROL_SHADER: return _TESS_CONTROL_SHADER;
        case GL_TESS_EVALUATION_SHADER: return _TESS_EVALUATION_SHADER;
        case GL_COMPUTE_SHADER: return _COMPUTE_SHADER;
        default:
            // CRITICAL FIX: Handle unknown shader types gracefully instead of crashing
            MGL_ERR("MGL ERROR: Unknown shader type 0x%x, defaulting to vertex shader\n", type);
            return _VERTEX_SHADER;
    }
}

glslang_stage_t getGLSLStage(GLuint type)
{
    switch(type) {
        case GL_VERTEX_SHADER: return GLSLANG_STAGE_VERTEX;
        case GL_FRAGMENT_SHADER: return GLSLANG_STAGE_FRAGMENT;
        case GL_GEOMETRY_SHADER: return GLSLANG_STAGE_GEOMETRY;
        case GL_TESS_CONTROL_SHADER: return GLSLANG_STAGE_TESSCONTROL;
        case GL_TESS_EVALUATION_SHADER: return GLSLANG_STAGE_TESSEVALUATION;
        case GL_COMPUTE_SHADER: return GLSLANG_STAGE_COMPUTE;
        default:
            // CRITICAL FIX: Handle unknown shader types gracefully instead of crashing
            MGL_ERR("MGL ERROR: Unknown GLSL shader type 0x%x, defaulting to vertex\n", type);
            return GLSLANG_STAGE_VERTEX;
    }

    return 0;
}

void initGLSLInput(GLMContext ctx, GLuint type, const char *src, glslang_input_t *input)
{
    input->language = GLSLANG_SOURCE_GLSL;
    input->stage = getGLSLStage(type);
    input->client = GLSLANG_CLIENT_OPENGL;
    input->target_language = GLSLANG_TARGET_SPV;
    input->target_language_version = GLSLANG_TARGET_SPV_1_0;

    /* Detect and upgrade GLSL version from source
     * GLSL 1.40 (OpenGL 3.1) shaders from virglrenderer need upgrading to 3.30
     * for glslang's SPIR-V compatibility with desktop OpenGL
     *
     * Default to 330 (minimum for SPIR-V) instead of 460 to be more permissive
     */
    int glsl_version = 330; /* Default to GLSL 3.30 - minimum for SPIR-V */
    int original_version = 330;
    /* GL_ARB_ES3_1_compatibility lets an ES shader be compiled by a desktop
     * context. Forcing it to the core profile makes the front end reject it. */
    bool es_profile = false;
    const char *version_str = strstr(src, "#version");
    if (version_str) {
        int scanned_version;
        if (sscanf(version_str, "#version %d", &scanned_version) == 1) {
            const char *eol = strchr(version_str, '\n');

            original_version = scanned_version;
            glsl_version = scanned_version;

            if (eol && memmem(version_str, (size_t)(eol - version_str), " es", 3))
                es_profile = true;

            /* Upgrade legacy GLSL versions to 330 minimum for SPIR-V */
            if (!es_profile && glsl_version < 330) {
                glsl_version = 330;
            }
        }
    }

    /* Set client_version to match GLSL version for SPIR-V targeting
     * This prevents "forced to be (450, core)" error when using GLSL 330 shaders
     * Must be set AFTER version detection above
     *
     * Note: glslang only exposes GLSLANG_TARGET_OPENGL_450, so we use that
     * as the SPIR-V target for all modern GLSL versions
     */
    if (glsl_version < 330) {
        /* Legacy GLSL - still target 450 for SPIR-V but shader will be upgraded */
        input->client_version = GLSLANG_TARGET_OPENGL_450;
    } else if (glsl_version == 330) {
        /* GLSL 3.30 shaders - target OpenGL 3.30 for SPIR-V */
        input->client_version = 330;  /* Use numeric value directly */
    } else {
        /* GLSL 4.00+ - target OpenGL 4.50 for SPIR-V */
        input->client_version = GLSLANG_TARGET_OPENGL_450;
    }

    /* For legacy GLSL versions, replace #version directive in source copy */
    static char *modified_src = NULL;
    static size_t modified_src_size = 0;

    if (original_version < 330 && !es_profile) {
        MGL_INFO("[MGL] Upgrading GLSL shader from version %d to %d\n",
                original_version, glsl_version);

        size_t src_len = strlen(src);
        if (src_len + 100 > modified_src_size) {
            modified_src_size = src_len + 100;
            free(modified_src);
            modified_src = (char *)malloc(modified_src_size);
        }

        if (modified_src) {
            strcpy(modified_src, src);

            /* Find and replace #version line */
            char *version_line = strstr(modified_src, "#version");
            if (!version_line) {
                MGL_INFO("[MGL] WARNING: #version not found in source\n");
                input->code = src;
            } else {
                char *newline = strchr(version_line, '\n');
                if (!newline) {
                    MGL_INFO("[MGL] WARNING: newline not found after #version\n");
                    input->code = src;
                } else {
                    char version_buf[64];
                    snprintf(version_buf, sizeof(version_buf), "#version %d core", glsl_version);
                    size_t old_len = newline - version_line;
                    size_t new_len = strlen(version_buf);

                    MGL_INFO("[MGL] Old version line length: %zu, new: %zu\n", old_len, new_len);
                    MGL_INFO("[MGL] Old line: %.*s\n", (int)old_len, version_line);

                    if (new_len <= old_len) {
                        /* Simple in-place replacement with space padding */
                        memset(version_line, ' ', old_len);
                        memcpy(version_line, version_buf, new_len);
                        MGL_INFO("[MGL] Replaced version line in source (in-place)\n");
                    } else {
                        /* Need to shift the rest of the source */
                        size_t rest_of_src = strlen(newline);
                        memmove(version_line + new_len, newline, rest_of_src + 1); /* +1 for null terminator */
                        memcpy(version_line, version_buf, new_len);
                        MGL_INFO("[MGL] Replaced version line with shift\n");
                        MGL_INFO("[MGL] New line: %.*s\n", (int)new_len, version_line);
                    }
                }
            }
            input->code = modified_src;
        } else {
            MGL_INFO("[MGL] ERROR: Failed to allocate modified_src\n");
            input->code = src;
        }
    } else {
        input->code = src;
    }

    input->default_version = glsl_version;
    input->default_profile = es_profile ? GLSLANG_ES_PROFILE : GLSLANG_CORE_PROFILE;
    //input->messages = 0xFFFF & ~GLSLANG_MSG_RELAXED_ERRORS_BIT;
    input->messages = GLSLANG_MSG_DEFAULT_BIT | GLSLANG_MSG_DEBUG_INFO_BIT | GLSLANG_MSG_RELAXED_ERRORS_BIT;
    input->resource = (const glslang_resource_t *)mglGlslangResource(ctx);

    input->force_default_version_and_profile = 1;
}

Shader *newShader(GLMContext ctx, GLenum type, GLuint shader)
{
    Shader *ptr;
    char shader_type_name[128];

    ptr = (Shader *)malloc(sizeof(Shader));
    // CRITICAL SECURITY FIX: Check malloc result instead of using assert()
    if (!ptr) {
        MGL_ERR("MGL SECURITY ERROR: Failed to allocate memory for shader\n");
        STATE(error) = GL_OUT_OF_MEMORY;
        return NULL;
    }

    bzero(ptr, sizeof(Shader));

    ptr->name = shader;
    ptr->type = type;
    ptr->glm_type = glShaderTypeToGLMType(type);

    snprintf(shader_type_name, sizeof(shader_type_name), "%s_%d", getShaderTypeStr(ptr->glm_type), shader);
    ptr->mtl_shader_type_name = strdup(shader_type_name);

    return ptr;
}

Shader *getShader(GLMContext ctx, GLenum type, GLuint shader)
{
    Shader *ptr;

    ptr = (Shader *)searchHashTable(&STATE(shader_table), shader);

    if (!ptr)
    {
        ptr = newShader(ctx, type, shader);

        insertHashElement(&STATE(shader_table), shader, ptr);
    }

    return ptr;
}

int isShader(GLMContext ctx, GLuint shader)
{
    Shader *ptr;

    ptr = (Shader *)searchHashTable(&STATE(shader_table), shader);

    if (ptr)
        return 1;

    return 0;
}

Shader *findShader(GLMContext ctx, GLuint shader)
{
    Shader *ptr;

    ptr = (Shader *)searchHashTable(&STATE(shader_table), shader);

    return ptr;
}

GLuint mglCreateShader(GLMContext ctx, GLenum type)
{
    GLuint shader;

    switch(type)
    {
        case GL_VERTEX_SHADER:
        case GL_FRAGMENT_SHADER:
        case GL_GEOMETRY_SHADER:
        case GL_COMPUTE_SHADER:
        case GL_TESS_CONTROL_SHADER:
        case GL_TESS_EVALUATION_SHADER:
            break;

        default:
            ERROR_RETURN_VALUE(GL_INVALID_ENUM, 0);
    }

    shader = getNewName(&STATE(shader_table));

    getShader(ctx, type, shader);

    return shader;
}

void mglFreeShader(GLMContext ctx, Shader *ptr)
{
    if (ptr->compiled_glsl_shader)
    {
        glslang_shader_delete(ptr->compiled_glsl_shader);
    }

    if (ptr->mtl_data.library)
    {
        ctx->mtl_funcs.mtlDeleteMTLObj(ctx, ptr->mtl_data.function);
        ctx->mtl_funcs.mtlDeleteMTLObj(ctx, ptr->mtl_data.library);
    }

    free((void *)ptr->mtl_shader_type_name);
    free((void *)ptr->src);
    free(ptr->pp_src);
    if (ptr->log) free(ptr->log);

    free(ptr);
}

void mglDeleteShader(GLMContext ctx, GLuint shader)
{
    Shader *ptr;

    /* OpenGL spec: A value of 0 for shader will be silently ignored. */
    if (shader == 0) {
        return;
    }

    ptr = findShader(ctx, shader);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);

    ptr->delete_status = GL_TRUE;

    // A shader still attached to a program is only flagged: glIsShader keeps
    // saying yes and glShaderSource still works on it, until it is detached.
    if (ptr->refcount == 0)
    {
        deleteHashElement(&STATE(shader_table), shader);
        mglFreeShader(ctx, ptr);
    }
}

GLboolean mglIsShader(GLMContext ctx, GLuint shader)
{
    return isShader(ctx, shader);
}

void mglShaderSource(GLMContext ctx, GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length)
{
    size_t len;
    GLchar *src;
    Shader *ptr;

    ERROR_CHECK_RETURN(isShader(ctx, shader), GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(count >= 0, GL_INVALID_VALUE);

    ptr = findShader(ctx, shader);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);

    if (count>1)
    {
        // compute storage requirement
        len = 0;
        if (!length) {
            for(int i=0; i<count; i++)
            {
                len += strlen(string[i]);
            }
        }
        else {
            for(int i=0; i<count; i++)
            {
                len += length[i];
            }
        }   
        ERROR_CHECK_RETURN(len, GL_INVALID_VALUE);

        // allocate storage
        src = (GLchar *)malloc(len+1); // +1 for NULL
        ERROR_CHECK_RETURN(src, GL_OUT_OF_MEMORY);

        if (!length) {        
            // string[i] are null-terminated
            *src = 0;
            for(int i=0; i<count; ++i)
            {
                strlcat(src, string[i], len+1);
            }
            assert(strlen(src) == len);
        } else {
            // CRITICAL SECURITY FIX: Prevent buffer overflow in shader source concatenation
            // string[i] may not be null-terminated - we must validate bounds carefully
            size_t cum_len = 0;
            for(int i=0; i<count; ++i)
            {
                // CRITICAL: Check if adding this string would exceed buffer bounds
                if (cum_len + length[i] > (size_t)len) {
                    // SECURITY: Truncate safely instead of overflowing buffer
                    MGL_ERR("MGL SECURITY ERROR: Shader source concatenation would overflow buffer, truncating safely\n");
                    // Copy only what fits
                    size_t safe_copy_len = ((size_t)len > cum_len) ? ((size_t)len - cum_len) : 0;
                    if (safe_copy_len > 0) {
                        strncpy(&src[cum_len], string[i], safe_copy_len);
                    }
                    cum_len = len; // Force termination at end
                    break;
                }

                // CRITICAL: Validate source pointer and length before copy
                if (!string[i]) {
                    MGL_ERR("MGL SECURITY ERROR: NULL string pointer in shader source concatenation\n");
                    continue; // Skip this string
                }

                strncpy(&src[cum_len], string[i], length[i]);
                cum_len += length[i];
            }
            // CRITICAL: Ensure null termination regardless of truncation
            src[cum_len < (size_t)len ? cum_len : (size_t)len] = '\0';
        }
    }
    else
    {
        ERROR_CHECK_RETURN(string, GL_INVALID_VALUE);

        src = strdup(*string);
        len = strlen(src);

        ERROR_CHECK_RETURN(len, GL_INVALID_VALUE);
    }

    ptr->src_len = len;
    ptr->src = src;
    ptr->dirty_bits |= DIRTY_SHADER;
}

static bool isWord(const char *p, const char *start, const char *word)
{
    size_t n = strlen(word);

    return !strncmp(p, word, n) &&
           (p == start || (!isalnum((unsigned char)p[-1]) && p[-1] != '_')) &&
           !isalnum((unsigned char)p[n]) && p[n] != '_';
}

// Does the block after a layout(...) ask for offset or align? Those only
// work with std140 and std430, so a packed or shared block using them has to
// stay as written and fail.
static bool blockPlacesMembers(const char *layout, const char *after)
{
    for (const char *p = layout; p < after; p++)
        if (isWord(p, layout, "align"))
            return true;

    const char *p = after;

    while (*p && *p != '{' && *p != ';')
        p++;

    if (*p != '{')
        return false;

    const char *open = p;
    int depth = 0;

    for (; *p; p++)
    {
        if (*p == '{') depth++;
        else if (*p == '}' && --depth == 0) break;
        else if (isWord(p, open, "offset") || isWord(p, open, "align")) return true;
    }

    return false;
}

// SPIR-V has no notion of the shared or packed block layouts, so glslang
// refuses them outright. They are legal GLSL, and an app using them is meant
// to ask the API where each member sits -- which MGL answers -- so treat them
// as std140. All three words are six letters, so this rewrites in place.
static void rewriteBlockLayouts(char *src)
{
    char *p = src;

    while ((p = strstr(p, "layout")) != NULL)
    {
        char *q = p + 6;

        if (p != src && (isalnum((unsigned char)p[-1]) || p[-1] == '_'))
        {
            p = q;
            continue;
        }

        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r')
            q++;

        if (*q != '(')
        {
            p = q;
            continue;
        }

        char *open = q;
        char *words[8];
        int nwords = 0;
        int depth = 0;

        for (; *q; q++)
        {
            if (*q == '(') { depth++; continue; }
            if (*q == ')') { if (--depth == 0) { q++; break; } continue; }

            if (depth > 0 && nwords < 8 && (isWord(q, src, "shared") || isWord(q, src, "packed")))
                words[nwords++] = q;
        }

        if (nwords && !blockPlacesMembers(open, q))
            for (int i = 0; i < nwords; i++)
                memcpy(words[i], "std140", 6);

        p = q;
    }
}

// Things glslang refuses that GL accepts, straightened out before it sees them.
// Returns NULL when there is nothing to change.
static char *normalizeFrontEnd(const char *src, GLenum type)
{
    size_t n = strlen(src);
    char *out = (char *)malloc(n + 1);
    bool changed = false;
    size_t o = 0;

    if (out == NULL)
        return NULL;

    // a line continuation written with a DOS line ending is still one
    for (size_t i = 0; i < n; i++)
    {
        if (src[i] == '\\' && src[i + 1] == '\r' && src[i + 2] == '\n')
        {
            out[o++] = '\\';
            i++;
            changed = true;
            continue;
        }
        out[o++] = src[i];
    }
    out[o] = 0;

    // Extensions core in 4.6 that glslang does not know by name. Subroutines
    // MGL rewrites away itself; the others are plain core GLSL by now.
    static const char *core_names[] = {
        "GL_ARB_shader_subroutine",
        "GL_ARB_arrays_of_arrays",
        "GL_ARB_texture_query_levels",
    };

    for (char *p = out; (p = strstr(p, "#extension")) != NULL; )
    {
        char *e = strchr(p, '\n');

        if (e == NULL)
            e = p + strlen(p);

        for (size_t k = 0; k < sizeof(core_names) / sizeof(core_names[0]); k++)
            if (memmem(p, (size_t)(e - p), core_names[k], strlen(core_names[k])))
            {
                memset(p, ' ', (size_t)(e - p));
                changed = true;
                break;
            }

        p = e;
    }

    // "invariant" on an input changes nothing, and GL lets any stage but the
    // vertex stage write it. glslang refuses it from 4.20 on, so it goes.
    if (type != GL_VERTEX_SHADER && strstr(out, "invariant"))
    {
        bool in_block = false;
        char *stmt = out;

        for (char *p = out; *p; p++)
        {
            if (*p != ';' && *p != '{' && *p != '}')
                continue;

            // the statement is [stmt, p)
            bool is_in = false;

            for (char *q = stmt; q < p; q++)
            {
                if ((q == out || !isalnum((unsigned char)q[-1])) && q[0] == 'i' && q[1] == 'n' &&
                    !isalnum((unsigned char)q[2]) && q[2] != '_' && (q == out || q[-1] != '_'))
                {
                    is_in = true;
                    break;
                }
            }

            if (*p == '{')
                in_block = is_in;

            if (in_block || is_in)
            {
                for (char *q = stmt; q + 9 <= p; q++)
                {
                    if (!strncmp(q, "invariant", 9) &&
                        (q == out || !(isalnum((unsigned char)q[-1]) || q[-1] == '_')) &&
                        !(isalnum((unsigned char)q[9]) || q[9] == '_'))
                    {
                        memset(q, ' ', 9);
                        changed = true;
                    }
                }
            }

            if (*p == '}')
                in_block = false;

            stmt = p + 1;
        }
    }

    if (!changed)
    {
        free(out);
        return NULL;
    }

    return out;
}

// glslang does not carry GL_ARB_cull_distance, so a shader that asks for it by
// extension is raised to the version that has cull distance in core. The name
// itself becomes one of ours, because GLSL will not let anyone define a macro
// that starts with GL_.
static char *rewriteCullExtension(const char *src)
{
    static const char *want = "GL_ARB_cull_distance";
    const char *ver, *nl, *body;
    char *out;
    size_t len;

    if (strstr(src, want) == NULL)
        return NULL;

    ver = strstr(src, "#version");

    if (ver == NULL)
        return NULL;

    nl = strchr(ver, '\n');

    if (nl == NULL)
        return NULL;

    body = nl + 1;
    len = strlen(src) + 128;
    out = (char *)malloc(len);

    if (out == NULL)
        return NULL;

    snprintf(out, len, "%.*s#version 450 core\n#define MG_ARB_cull_distance 1\n%s",
             (int)(ver - src), src, body);

    // the extension is core at this version, so asking for it again is an error
    for (char *p = out; (p = strstr(p, "#extension")) != NULL; )
    {
        char *e = strchr(p, '\n');

        if (e == NULL)
            e = p + strlen(p);

        if (memmem(p, (size_t)(e - p), want, strlen(want)))
            memset(p, ' ', (size_t)(e - p));

        p = e;
    }

    // whatever is left is the shader testing for the name, so point it at ours
    for (char *p = out; (p = strstr(p, want)) != NULL; p += strlen(want))
    {
        p[0] = 'M';
        p[1] = 'G';
    }

    return out;
}

// glslang drops gl_NumSamples when it is generating SPIR-V, because Vulkan has
// no such built-in -- but it is core GLSL and the CTS leans on it. Rename it to
// a plain uniform of the same length, which MGL writes at draw time.
// Returns a new string when something changed, NULL when nothing did.

static char *rewriteNumSamples(const char *src)
{
    static const char gl_name[] = "gl_NumSamples";
    static const char my_name[] = MGL_NUM_SAMPLES_NAME;
    static const char decl[] = "\nuniform int " MGL_NUM_SAMPLES_NAME ";\n";
    const size_t n = sizeof(gl_name) - 1;
    const char *p;
    char *out, *q;
    size_t hits = 0;

    for (p = strstr(src, gl_name); p; p = strstr(p + n, gl_name))
    {
        char before = (p == src) ? ' ' : p[-1];

        if (!isalnum((unsigned char)before) && before != '_' &&
            !isalnum((unsigned char)p[n]) && p[n] != '_')
            hits++;
    }

    if (hits == 0)
        return NULL;

    out = (char *)malloc(strlen(src) + sizeof(decl) + 1);

    if (out == NULL)
        return NULL;

    strcpy(out, src);

    for (q = strstr(out, gl_name); q; q = strstr(q + n, gl_name))
    {
        char before = (q == out) ? ' ' : q[-1];

        if (!isalnum((unsigned char)before) && before != '_' &&
            !isalnum((unsigned char)q[n]) && q[n] != '_')
            memcpy(q, my_name, n);
    }

    // the declaration goes after #version, which the preprocessor keeps first
    {
        char *ins = strstr(out, "#version");

        if (ins)
        {
            ins = strchr(ins, '\n');
            ins = ins ? ins + 1 : out;
        }
        else
        {
            ins = out;
        }

        memmove(ins + sizeof(decl) - 1, ins, strlen(ins) + 1);
        memcpy(ins, decl, sizeof(decl) - 1);
    }

    return out;
}

// ---------------------------------------------------------------------------
// Preprocessor rules glslang lets slide
//
// glslang only warns about a handful of things the GLSL specification calls
// errors, and a conformant compiler has to refuse them. These are checked on
// the source exactly as the application gave it.
// ---------------------------------------------------------------------------

static bool ppIdentStart(char c) { return isalpha((unsigned char)c) || c == '_'; }
static bool ppIdentChar(char c)  { return isalnum((unsigned char)c) || c == '_'; }

// The expression after #if or #elif has to be one expression. "1 foobar" is two.
// Parentheses are not balanced here: a macro may open one that the line closes.
static bool ppExpressionIsWellFormed(const char *p)
{
    bool want_operand = true;

    for (;;)
    {
        while (*p == ' ' || *p == '\t') p++;

        if (*p == 0)
            break;

        if (ppIdentStart(*p))
        {
            const char *start = p;

            while (ppIdentChar(*p)) p++;

            if (!want_operand)
                return false;

            if (p - start == 7 && !strncmp(start, "defined", 7))
            {
                while (*p == ' ' || *p == '\t') p++;

                bool paren = (*p == '(');

                if (paren) p++;
                while (*p == ' ' || *p == '\t') p++;
                if (!ppIdentStart(*p)) return false;
                while (ppIdentChar(*p)) p++;
                while (*p == ' ' || *p == '\t') p++;
                if (paren && *p++ != ')') return false;
            }
            else
            {
                // a function-like macro call is one operand, arguments and all
                const char *q = p;

                while (*q == ' ' || *q == '\t') q++;

                if (*q == '(')
                {
                    int d = 0;

                    for (p = q; *p; p++)
                    {
                        if (*p == '(') d++;
                        else if (*p == ')' && --d == 0) { p++; break; }
                    }
                }
            }

            want_operand = false;
            continue;
        }

        if (isdigit((unsigned char)*p))
        {
            if (!want_operand)
                return false;

            while (ppIdentChar(*p) || *p == '.') p++;
            want_operand = false;
            continue;
        }

        if (*p == '(')
        {
            if (!want_operand) return false;
            p++;
            continue;
        }

        if (*p == ')')
        {
            if (want_operand) return false;
            p++;
            continue;
        }

        // the unary operators may start an operand
        if (want_operand && (*p == '!' || *p == '~' || *p == '-' || *p == '+'))
        {
            p++;
            continue;
        }

        if (strchr("+-*/%<>=!&|^?:", *p))
        {
            if (want_operand)
                return false;

            p++;
            while (*p && strchr("<>=&|", *p)) p++;
            want_operand = true;
            continue;
        }

        // anything else is left for glslang to judge
        p++;
    }

    return !want_operand;
}

static bool ppKnownVersion(int v, bool es)
{
    static const int desktop[] = { 110, 120, 130, 140, 150, 330, 400, 410, 420, 430, 440, 450, 460 };

    if (es || v == 100)
        return v == 100 || v == 300 || v == 310 || v == 320;

    for (size_t i = 0; i < sizeof(desktop) / sizeof(desktop[0]); i++)
        if (desktop[i] == v)
            return true;

    return false;
}

// NULL when the source is fine, otherwise the reason it is not.
static const char *ppStrictError(const char *src)
{
    static const char *const kVersionFirst = "#version must come before anything else in the shader";
    static const char *const kVersionBad   = "#version names a GLSL version that does not exist";
    static const char *const kStringify    = "GLSL has no # stringification operator";
    static const char *const kExtraTokens  = "unexpected tokens after a preprocessor directive";
    static const char *const kBadExpr      = "malformed #if or #elif expression";

    size_t n = strlen(src);
    char *buf = (char *)malloc(n + 1);
    const char *err = NULL;
    bool seen = false;

    if (buf == NULL)
        return NULL;

    // comments become spaces, and a backslash-newline joins the lines
    size_t o = 0;

    for (size_t i = 0; i < n; i++)
    {
        if (src[i] == '/' && src[i + 1] == '/')
        {
            while (i < n && src[i] != '\n') i++;
            if (i < n) buf[o++] = '\n';
        }
        else if (src[i] == '/' && src[i + 1] == '*')
        {
            i += 2;
            while (i < n && !(src[i] == '*' && src[i + 1] == '/'))
            {
                if (src[i] == '\n') buf[o++] = '\n';
                i++;
            }
            i++;
            buf[o++] = ' ';
        }
        else if (src[i] == '\\' && src[i + 1] == '\n')
        {
            i++;
        }
        else
        {
            buf[o++] = src[i];
        }
    }
    buf[o] = 0;

    for (char *line = buf; line && *line && !err;)
    {
        char *next = strchr(line, '\n');

        if (next) *next++ = 0;

        char *p = line;

        while (*p == ' ' || *p == '\t' || *p == '\r') p++;

        if (*p == 0)
        {
            line = next;
            continue;
        }

        if (*p != '#')
        {
            seen = true;
            line = next;
            continue;
        }

        p++;
        while (*p == ' ' || *p == '\t') p++;

        char name[16] = "";
        size_t k = 0;

        while (ppIdentChar(*p) && k < sizeof(name) - 1) name[k++] = *p++;
        name[k] = 0;

        char *rest = p;
        while (*rest == ' ' || *rest == '\t' || *rest == '\r') rest++;

        if (!strcmp(name, "version"))
        {
            if (seen)
                err = kVersionFirst;
            else
            {
                int v = atoi(rest);
                bool es = strstr(rest, "es") != NULL;

                if (!isdigit((unsigned char)*rest) || !ppKnownVersion(v, es))
                    err = kVersionBad;
            }
        }
        else if (!strcmp(name, "define"))
        {
            char *q = rest;

            while (ppIdentChar(*q)) q++;

            if (*q == '(')
                while (*q && *q != ')') q++;

            for (; *q && !err; q++)
                if (*q == '#' && q[1] != '#' && (q == rest || q[-1] != '#'))
                    err = kStringify;
        }
        else if (!strcmp(name, "else") || !strcmp(name, "endif"))
        {
            if (*rest && *rest != '\r')
                err = kExtraTokens;
        }
        else if (!strcmp(name, "if") || !strcmp(name, "elif"))
        {
            if (!ppExpressionIsWellFormed(rest))
                err = kBadExpr;
        }

        seen = true;
        line = next;
    }

    free(buf);

    return err;
}

void mglCompileShader(GLMContext ctx, GLuint shader)
{
    Shader *ptr;
    glslang_input_t glsl_input;
    glslang_shader_t *glsl_shader;
    int err;

    ERROR_CHECK_RETURN(isShader(ctx, shader), GL_INVALID_VALUE);

    ptr = findShader(ctx, shader);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_OPERATION);

    // no source was ever accepted for this shader, so there is nothing to compile
    if (ptr->src == NULL)
    {
        if (ptr->log) free(ptr->log);
        ptr->log = strdup("shader has no source");
        return;
    }

    // GL 4.6 section 7.1: a shader that will not compile sets COMPILE_STATUS
    // false and fills the info log. It does not raise a GL error.
    {
        const char *strict = ppStrictError(ptr->src);

        if (strict)
        {
            free(ptr->log);
            ptr->log = strdup(strict);
            return;
        }
    }

    ctx->error_suppress++;

    char *normal = normalizeFrontEnd(ptr->src, ptr->type);
    char *raised = rewriteCullExtension(normal ? normal : ptr->src);

    if (normal && raised == NULL)
    {
        raised = normal;
        normal = NULL;
    }

    free(normal);
    const char *front = raised ? raised : ptr->src;

    // glslang will not take the subroutine keyword when it targets SPIR-V, so
    // the source is rewritten into plain GLSL before it ever sees it.
    char *desub = mglRewriteSubroutines(front, &ptr->subroutines);

    if (ptr->subroutines.error)
    {
        free(ptr->log);
        ptr->log = strdup(ptr->subroutines.error);
        free(desub);
        free(raised);
        ctx->error_suppress--;
        return;
    }

    if (desub)
    {
        free(raised);
        raised = NULL;
        front = desub;
    }

    initGLSLInput(ctx, ptr->type, front, &glsl_input);

    glsl_shader = glslang_shader_create(&glsl_input);
    if (glsl_shader == NULL)
    {
        // CRITICAL FIX: Handle shader creation failure gracefully instead of crashing
        MGL_ERR("MGL ERROR: Failed to create GLSL shader for type 0x%x\n", ptr->type);

        // Set error state for the shader - only set log message
        if (!ptr->log) {
            ptr->log = strdup("GLSL shader creation failed - insufficient memory or unsupported shader type");
        }
        free(desub);
        free(raised);
        ctx->error_suppress--;
        return;
    }

    if (ptr->log)
    {
        free(ptr->log);
        ptr->log = NULL;
    }

    /* SPIR-V wants every varying to carry an explicit location, but GL never
     * required that at any version, so let glslang hand out the missing ones.
     * Locations the shader does declare are left alone. */
    int options = GLSLANG_SHADER_VULKAN_RULES_RELAXED | GLSLANG_SHADER_AUTO_MAP_LOCATIONS |
                  GLSLANG_SHADER_AUTO_MAP_BINDINGS;

    glslang_shader_set_options(glsl_shader, options);

    err = glslang_shader_preprocess(glsl_shader, &glsl_input);

    if (err)
    {
        const char *pp = glslang_shader_get_preprocessed_code(glsl_shader);

        if (pp)
        {
            char *fixed = strdup(pp);

            if (fixed)
            {
                rewriteBlockLayouts(fixed);

                if (ptr->glm_type == _FRAGMENT_SHADER)
                {
                    char *numbered = rewriteNumSamples(fixed);

                    if (numbered)
                    {
                        free(fixed);
                        fixed = numbered;
                    }
                }

                glslang_shader_set_preprocessed_code(glsl_shader, fixed);

                free(ptr->pp_src);
                ptr->pp_src = fixed;
                fixed = NULL;
            }
        }
    }

    if (!err)
    {
        // PROPER FIX: Enhanced error logging with proper formatting
        const char *preprocessed = glslang_shader_get_preprocessed_code(glsl_shader);
        const char *info_log = glslang_shader_get_info_log(glsl_shader);
        const char *debug_log = glslang_shader_get_info_debug_log(glsl_shader);

        MGL_INFO("MGL SHADER ERROR: glslang_shader_preprocess failed with error: %d\n", err);
        MGL_INFO("MGL SHADER ERROR: Shader type: %s\n", getShaderTypeStr(ptr->glm_type));
        MGL_INFO("MGL SHADER ERROR: Preprocessed code:\n%s\n", preprocessed ? preprocessed : "(null)");
        MGL_INFO("MGL SHADER ERROR: Info log:\n%s\n", info_log ? info_log : "(null)");
        MGL_INFO("MGL SHADER ERROR: Debug log:\n%s\n", debug_log ? debug_log : "(null)");

        size_t len;

        len = 1024;
        len += strlen(glslang_shader_get_preprocessed_code(glsl_shader));
        len += strlen(glslang_shader_get_info_log(glsl_shader));
        len += strlen(glslang_shader_get_info_debug_log(glsl_shader));

        ptr->log = (char *)malloc(len);

        ptr->log[0] = 0;

        snprintf(ptr->log, len,
                "glslang_shader_preprocess failed err: %d\n"
                "glslang_shader_get_preprocessed_code:\n%s\n"
                "glslang_shader_get_info_log:%s\n"
                "glslang_shader_get_info_debug_log:\n%s\n",
                err,
                glslang_shader_get_preprocessed_code(glsl_shader),
                glslang_shader_get_info_log(glsl_shader),
                glslang_shader_get_info_debug_log(glsl_shader));

        free(desub);
        free(raised);
        ctx->error_suppress--;
        return;
    }

    err = glslang_shader_parse(glsl_shader, &glsl_input);
    if (!err)
    {
        // PROPER FIX: Enhanced parse error logging
        const char *preprocessed = glslang_shader_get_preprocessed_code(glsl_shader);
        const char *info_log = glslang_shader_get_info_log(glsl_shader);
        const char *debug_log = glslang_shader_get_info_debug_log(glsl_shader);

        MGL_INFO("MGL SHADER ERROR: glslang_shader_parse failed with error: %d\n", err);
        MGL_INFO("MGL SHADER ERROR: Shader type: %s\n", getShaderTypeStr(ptr->glm_type));
        MGL_INFO("MGL SHADER ERROR: Preprocessed code:\n%s\n", preprocessed ? preprocessed : "(null)");
        MGL_INFO("MGL SHADER ERROR: Info log:\n%s\n", info_log ? info_log : "(null)");
        MGL_INFO("MGL SHADER ERROR: Debug log:\n%s\n", debug_log ? debug_log : "(null)");

        size_t len;

        len = 1024;
        len += strlen(glslang_shader_get_preprocessed_code(glsl_shader));
        len += strlen(glslang_shader_get_info_log(glsl_shader));
        len += strlen(glslang_shader_get_info_debug_log(glsl_shader));

        ptr->log = (char *)malloc(len);

        ptr->log[0] = 0;

        snprintf(ptr->log, len,
                "glslang_shader_preprocess failed err: %d\n"
                "glslang_shader_get_preprocessed_code:\n%s\n"
                "glslang_shader_get_info_log:%s\n"
                "glslang_shader_get_info_debug_log:\n%s\n",
                err,
                glslang_shader_get_preprocessed_code(glsl_shader),
                glslang_shader_get_info_log(glsl_shader),
                glslang_shader_get_info_debug_log(glsl_shader));

        free(desub);
        free(raised);
        ctx->error_suppress--;
        return;
    }

    int max_in = 0, max_out = 0;

    switch (ptr->glm_type)
    {
        case _VERTEX_SHADER:
            max_out = ctx->state.var.max_vertex_output_components / 4;
            break;
        case _TESS_CONTROL_SHADER:
            max_in = ctx->state.var.max_tess_control_input_components / 4;
            max_out = ctx->state.var.max_tess_control_output_components / 4;
            break;
        case _TESS_EVALUATION_SHADER:
            max_in = ctx->state.var.max_tess_evaluation_input_components / 4;
            max_out = ctx->state.var.max_tess_evaluation_output_components / 4;
            break;
        case _GEOMETRY_SHADER:
            max_in = ctx->state.var.max_geometry_input_components / 4;
            max_out = ctx->state.var.max_geometry_output_components / 4;
            break;
        case _FRAGMENT_SHADER:
            max_in = ctx->state.var.max_fragment_input_components / 4;
            break;
        default:
            break;
    }

    char where[256];

    if (!mglVaryingLocationsFit(glsl_shader, max_in, max_out, where, sizeof where))
    {
        ptr->log = strdup(where);
        glslang_shader_delete(glsl_shader);
        free(desub);
        free(raised);
        ctx->error_suppress--;
        return;
    }

    if (ptr->compiled_glsl_shader) {
        ptr->dirty_bits |= DIRTY_SHADER;
    }

    ptr->compiled_glsl_shader = glsl_shader;
    free(desub);
    free(raised);
    ctx->error_suppress--;
}

void mglGetShaderiv(GLMContext ctx, GLuint shader, GLenum pname, GLint *params)
{
    Shader *ptr;

    ptr = findShader(ctx, shader);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);

    switch(pname)
    {
        case GL_SHADER_TYPE:
            switch(ptr->glm_type)
            {
                case _VERTEX_SHADER: *params = GL_VERTEX_SHADER; break;
                case _FRAGMENT_SHADER: *params = GL_FRAGMENT_SHADER; break;
                case _GEOMETRY_SHADER: *params = GL_GEOMETRY_SHADER; break;
                case _COMPUTE_SHADER: *params = GL_COMPUTE_SHADER; break;
                case _TESS_CONTROL_SHADER: *params = GL_TESS_CONTROL_SHADER; break;
                case _TESS_EVALUATION_SHADER: *params = GL_TESS_EVALUATION_SHADER; break;
                default:
                    // CRITICAL FIX: Handle unknown shader types gracefully instead of crashing
                    MGL_ERR("MGL ERROR: Unknown internal shader type %d, defaulting to vertex\n", ptr->glm_type);
                    *params = GL_VERTEX_SHADER;
            }
            break;

        case GL_DELETE_STATUS:
            *params = ptr->delete_status;
            break;

        case GL_COMPILE_STATUS:
            if (ptr->log)
            {
                *params = GL_FALSE;
            }
            else
            {
                *params = GL_TRUE;
            }
            break;

        case GL_INFO_LOG_LENGTH:
            *params = ptr->log ? (GLint)strlen(ptr->log) + 1 : 0;
            break;

        case GL_SHADER_SOURCE_LENGTH:
            *params = (GLint)ptr->src_len;
            break;

        default:
            ERROR_RETURN(GL_INVALID_ENUM);
            break;
    }
}

void mglGetShaderInfoLog(GLMContext ctx, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)
{
    Shader *ptr;

    ptr = findShader(ctx, shader);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);

    if (ptr->log)
    {
        if (length)
        {
            *length = (GLsizei)strlen(ptr->log);
        }

        if (infoLog)
        {
            if (bufSize >= strlen(ptr->log))
            {
                memcpy(infoLog, ptr->log, strlen(ptr->log));
            }
        }
    }
}

void mglGetShaderSource(GLMContext ctx, GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)
{
    Shader *ptr;

    ptr = findShader(ctx, shader);

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(bufSize >= 0, GL_INVALID_VALUE);

    // GL 4.6 section 7.1: at most bufSize characters are written, terminator
    // included, and length is what was written without it. A shader that was
    // never given source has none, which is an empty string and not a crash --
    // this used to size the copy with strlen(ptr->log), and a shader with no
    // log at all is the common case.
    GLsizei n = 0;

    if (ptr->src && bufSize > 0)
    {
        n = (GLsizei)ptr->src_len;

        if (n > bufSize - 1)
            n = bufSize - 1;
    }

    if (source && bufSize > 0)
    {
        if (n)
            memcpy(source, ptr->src, (size_t)n);

        source[n] = '\0';
    }

    if (length)
        *length = n;
}

/* ---------- ARB_gl_spirv ---------- */

void mglShaderBinary(GLMContext ctx, GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length)
{
    GLuint magic;

    ERROR_CHECK_RETURN(binaryFormat == GL_SHADER_BINARY_FORMAT_SPIR_V, GL_INVALID_ENUM);
    ERROR_CHECK_RETURN(count >= 0 && length >= 0, GL_INVALID_VALUE);

    if (count == 0)
        return;

    ERROR_CHECK_RETURN(shaders, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(binary, GL_INVALID_VALUE);

    // a SPIR-V module is a stream of 32 bit words starting with a magic number
    ERROR_CHECK_RETURN(length >= 4 && (length % 4) == 0, GL_INVALID_VALUE);

    memcpy(&magic, binary, sizeof magic);
    ERROR_CHECK_RETURN(magic == 0x07230203u || magic == 0x03022307u, GL_INVALID_VALUE);

    // check every name before touching any of them
    for (GLsizei i = 0; i < count; i++)
        ERROR_CHECK_RETURN(findShader(ctx, shaders[i]), GL_INVALID_VALUE);

    for (GLsizei i = 0; i < count; i++)
    {
        Shader *ptr = findShader(ctx, shaders[i]);
        void *copy = malloc((size_t)length);

        ERROR_CHECK_RETURN(copy, GL_OUT_OF_MEMORY);

        memcpy(copy, binary, (size_t)length);

        free(ptr->spirv_binary);
        ptr->spirv_binary = copy;
        ptr->spirv_binary_length = length;
        ptr->specialized = GL_FALSE;
    }
}

void mglSpecializeShader(GLMContext ctx, GLuint shader, const GLchar *pEntryPoint, GLuint numSpecializationConstants, const GLuint *pConstantIndex, const GLuint *pConstantValue)
{
    Shader *ptr = findShader(ctx, shader);
    char *ep;

    ERROR_CHECK_RETURN(ptr, GL_INVALID_VALUE);
    ERROR_CHECK_RETURN(ptr->spirv_binary, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(ptr->specialized == GL_FALSE, GL_INVALID_OPERATION);
    ERROR_CHECK_RETURN(numSpecializationConstants == 0 ||
                       (pConstantIndex && pConstantValue), GL_INVALID_VALUE);

    ep = strdup(pEntryPoint ? pEntryPoint : "main");
    ERROR_CHECK_RETURN(ep, GL_OUT_OF_MEMORY);

    free((void *)ptr->entry_point);
    ptr->entry_point = ep;
    ptr->specialized = GL_TRUE;

    // specialization constants are not rewritten yet; the module is used as is
    free(ptr->log);
    ptr->log = NULL;
}
