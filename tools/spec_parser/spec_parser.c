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
 * parser.c
 * headerparser
 *
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include "ezxml.h"

#define string(_STR_)   (const char *)_STR_

enum {
    kEnum,
    KCommand
};

enum {
    kGLHeaders,         // gl.h
    kGLFuncs,           // gl.c
    kMGLHeaders,        // mgl.h
    kMGLFuncs,          // mgl.c
    kMGLDispatch,       // glm_dispatch.h
    kMGLDispatchInit    // glm_dispatch.c
};

typedef struct {
    int major, minor;
} GLVersion;

GLVersion *get_gl_versions(unsigned es_only)
{
    if (es_only)
    {
        static GLVersion gl_es_version[] = {{3,0}, {3,1}, {3,2}, {0,0}};
        
        return gl_es_version;
    }

    static GLVersion gl_versions[] = {{1,0}, {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {2,0}, {2,1}, {3,0}, {3,1}, {3,2}, {3,3}, {4,0}, {4,1}, {4,2}, {4,3}, {4,4}, {4,5}, {4,6}, {0,0}};
    
    return gl_versions;

}

char *insert_string(char *src, char *insert, size_t insert_at)
{
    char *ret, *ptr;
    size_t len_src, len_insert;

    assert(src);
    assert(insert);

    // if 0 this is a reverse strcat
    if (insert_at == 0)
    {
        size_t len;
        char *ptr;

        len = strlen(src) + strlen(insert) + 128;
        ptr = (char *)malloc(len);
        assert(ptr);

        strcpy(ptr, insert);
        strcat(ptr, src);

        return ptr;
    }

    len_src = strlen(src);
    assert(len_src);

    len_insert = strlen(insert);
    assert(len_insert);

    ret = (char *)malloc(len_src + len_insert + 128);
    assert(ret);

    ptr = ret;
    for(int i=0; i<insert_at; i++)
    {
        *ptr++ = *src++;
    }

    for(int i=0; i<len_insert; i++)
    {
        *ptr++ = *insert++;
    }

    while(*src)
    {
        *ptr++ = *src++;
    }

    *ptr = 0;

    return ret;
}

char *make_mgl_str(const char *str)
{
    char *new_str;
    size_t len;

    assert(str);

    len = strlen(str) + 128;
    new_str = (char *)malloc(len);
    assert(new_str);

    sprintf(new_str, "m%s", str);

    return new_str;
}

char *make_dispatch_str(const char *str)
{
    char *new_str, *ptr;
    size_t len;

    assert(str);

    len = strlen(str) + 128;
    new_str = (char *)malloc(len);
    assert(new_str);

    ptr = new_str;

    // jump over "gl"
    str = &str[2];

    *ptr++ = tolower(*str++);
    while(*str)
    {
        // fix odd case for 1D 2D 3D at end of string.. keep those uppercase
        if (isupper(*str) && (str[1] != 0))
        {
            *ptr++ = '_';
            *ptr++ = tolower(*str++);
        }
        else
        {
            *ptr++ = *str++;
        }
    }

    *ptr = 0;

    return new_str;
}

void print_type(ezxml_t type, FILE *fp_out)
{
    ezxml_t name;

    name = ezxml_child(type, "name");
    if (name)
    {
        char *new_str;

        new_str = insert_string(type->txt, name->txt, name->off);
        assert(new_str);

        fprintf(fp_out, "%s\n", new_str);

        free(new_str);
    }
    else
    {
        fprintf(fp_out, "%s\n", type->txt);
    }
}

void print_enum(ezxml_t enum_node, FILE *fp_out)
{
    const char *value, *name;

    value = ezxml_attr(enum_node, string("value"));
    name = ezxml_attr(enum_node, string("name"));

    assert(value);
    assert(name);

    fprintf(fp_out, "#define %-60s %s\n", name, value);
}

/* A command listed by more than one GL version is still one function. The
   walk below visits every <require> block, so remember what has been written
   and skip repeats. Reset for each file. */
static char *g_seen[4096];
static int   g_seen_count;

static void reset_seen(void)
{
    for (int i = 0; i < g_seen_count; i++)
        free(g_seen[i]);

    g_seen_count = 0;
}

static int already_emitted(const char *name)
{
    for (int i = 0; i < g_seen_count; i++)
        if (!strcmp(g_seen[i], name))
            return 1;

    if (g_seen_count < (int)(sizeof(g_seen) / sizeof(g_seen[0])))
        g_seen[g_seen_count++] = strdup(name);

    return 0;
}

/* The return type is split across the proto text and its <ptype> child:
   glGetString is "const  *" with "GLubyte" sitting at offset 6. Putting the
   two back together is the only way to keep the const and the star. */
static char *full_return_type(ezxml_t proto)
{
    ezxml_t ptype = ezxml_child(proto, string("ptype"));

    if (ptype)
        return insert_string(proto->txt, ptype->txt, ptype->off);

    return strdup(proto->txt ? proto->txt : "void ");
}

/* Where the macOS SDK and the Khronos registry disagree, the SDK wins for the
   exported entry points: those sit in the same translation unit as
   <OpenGL/gl3.h>, so a type the registry has since changed would be a
   conflicting declaration. Apple's gl3.h still spells these two
   internalformat parameters GLint.

   This applies to the exported glXxx surface only. The dispatch table and the
   mglXxx prototypes follow the registry, because that is what MGL's own
   implementations are written against. */
static const struct { const char *func; const char *param; const char *type; }
g_param_overrides[] = {
    { "glTexImage2DMultisample", "internalformat", "GLint " },
    { "glTexImage3DMultisample", "internalformat", "GLint " },
    { 0, 0, 0 }
};

static const char *param_type_override(const char *func, const char *param)
{
    if (!func || !param)
        return 0;

    for (int i = 0; g_param_overrides[i].func; i++)
        if (!strcmp(g_param_overrides[i].func, func) &&
            !strcmp(g_param_overrides[i].param, param))
            return g_param_overrides[i].type;

    return 0;
}

int proto_has_return_type(ezxml_t proto)
{
    ezxml_t ptype;

    assert(proto);
    assert(proto->txt);

    ptype = ezxml_child(proto, string("ptype"));

    if (ptype)
    {
        return 1;
    }

    /* no <ptype>, so the proto text is the whole type: "void " returns
       nothing, "void *" returns a pointer */
    if (strchr(proto->txt, '*'))
    {
        return 1;
    }

    return 0;
}

char *proto_return_type(ezxml_t proto)
{
    ezxml_t ptype;

    assert(proto);
    assert(proto->txt);

    ptype = ezxml_child(proto, string("ptype"));

    if (ptype)
    {
        return ptype->txt;
    }

    /* no <ptype>: the proto text is the type, e.g. "void *" */
    return proto->txt;
}

int get_param_count(ezxml_t command)
{
    ezxml_t param;
    int param_count = 0;

    for (param = ezxml_child(command, "param"); param; param = param->next)
    {
        param_count++;
    }

    return param_count;
}

void print_command(ezxml_t command, int mode, FILE *fp_out)
{
    ezxml_t proto, name;
    ezxml_t param, ptype;

    proto = ezxml_child(command, string("proto"));
    assert(proto);

    name = ezxml_child(proto, string("name"));

    const char *func_name = (name && name->txt) ? name->txt : 0;

    ptype = ezxml_child(proto, string("ptype"));

    int param_count = get_param_count(command);

    if (mode == kMGLDispatch)
    {
        char *dispatch_str;

        dispatch_str = make_dispatch_str(name->txt);

        char *ret = full_return_type(proto);

        fprintf(fp_out, "        %s(*%s)(GLMContext ctx", ret, dispatch_str);

        free(ret);
        free(dispatch_str);
    }
    else if (mode == kGLHeaders || mode == kGLFuncs)
    {
        char *str;

        char *ret = full_return_type(proto);

        fprintf(fp_out, "%s%s(", ret, name->txt);

        free(ret);
    }
    else if (mode == kMGLHeaders || mode == kMGLFuncs)
    {
        char *str, *mgl_func;

        mgl_func = make_mgl_str(name->txt);
        assert(mgl_func);

        char *ret = full_return_type(proto);

        fprintf(fp_out, "%s%s(GLMContext ctx", ret, mgl_func);

        free(ret);
        free(mgl_func);
    }
    else
    {
        assert(0);
    }

    if (param_count)
    {
        int first = (mode == kGLHeaders || mode == kGLFuncs);

        for (param = ezxml_child(command, "param"); param; param = param->next)
        {
            assert(param->off);

            ptype = ezxml_child(param, "ptype");
            name = ezxml_child(param, "name");

            assert(name);
            assert(name->txt);

            /* the separator goes in front, so a parameter whose type has no
               <ptype> child -- "const void *pixels" -- still gets one */
            if (!first)
            {
                fprintf(fp_out, ", ");
            }

            first = 0;

            const char *override = (mode == kGLHeaders || mode == kGLFuncs)
                                 ? param_type_override(func_name, name->txt) : 0;

            if (override)
            {
                fprintf(fp_out, "%s%s", override, name->txt);
            }
            else if (ptype)
            {
                char *str = insert_string(param->txt, ptype->txt, ptype->off);

                fprintf(fp_out, "%s%s", str, name->txt);

                free(str);
            }
            else
            {
                fprintf(fp_out, "%s%s", param->txt, name->txt);
            }
        }
    }

    if (mode == kGLHeaders || mode == kMGLHeaders || mode == kMGLDispatch)
    {
        fprintf(fp_out, ");\n");
    }
    else if (mode == kGLFuncs)
    {
        char *dispatch_name;

        name = ezxml_child(proto, string("name"));

        fprintf(fp_out, ")\n{\n");
        fprintf(fp_out, "    GLMContext ctx = GET_CONTEXT();\n\n");

        dispatch_name = make_dispatch_str(name->txt);

        if (proto_has_return_type(proto))
        {
            fprintf(fp_out, "    return ctx->dispatch.%s(ctx", dispatch_name);
        }
        else
        {
            fprintf(fp_out, "    ctx->dispatch.%s(ctx", dispatch_name);
        }

        free(dispatch_name);

        for (param = ezxml_child(command, "param"); param; param = param->next)
        {
            name = ezxml_child(param, "name");

            assert(name);
            assert(name->txt);

            fprintf(fp_out, ", %s", name->txt);
        }
        // end of function call
        fprintf(fp_out, ");\n");

        fprintf(fp_out, "}\n\n");
    }
    else if (mode == kMGLFuncs)
    {
        char *mgl_name;

        name = ezxml_child(proto, string("name"));

        fprintf(fp_out, ")\n{\n");

        mgl_name = make_mgl_str(name->txt);

        if (proto_has_return_type(proto))
        {
            char *ret_type;

            ret_type = proto_return_type(proto);

            if (!strcmp(ret_type, "GLuint"))
            {
                fprintf(fp_out, "\t%s ret = 0;\n\n", ret_type);
            }
            else if (!strcmp(ret_type, "GLubyte"))
            {
                fprintf(fp_out, "\t%s ret = 0;\n\n", ret_type);
            }
            else if (!strcmp(ret_type, "GLboolean"))
            {
                fprintf(fp_out, "\t%s ret = 0;\n\n", ret_type);
            }
            else if (!strcmp(ret_type, "GLenum"))
            {
                fprintf(fp_out, "\t%s ret = (GLenum)0;\n\n", ret_type);
            }
            else if (!strcmp(ret_type, "GLint"))
            {
                fprintf(fp_out, "\t%s ret = -1;\n\n", ret_type);
            }
            else if (!strcmp(ret_type, "GLhandleARB"))
            {
                fprintf(fp_out, "\t%s ret = NULL;\n\n", ret_type);
            }
            else if (!strcmp(ret_type, "GLintptr"))
            {
                fprintf(fp_out, "\t%s ret = 0;\n\n", ret_type);
            }
            else if (!strcmp(ret_type, "GLsync"))
            {
                fprintf(fp_out, "\t%s ret = NULL;\n\n", ret_type);
            }
            else if (!strcmp(ret_type, "GLvoid"))
            {
                fprintf(fp_out, "\t%s* ret = NULL;\n\n", ret_type);
            }
            else if (strchr(ret_type, '*'))
            {
                /* glMapBuffer and friends, whose type has no <ptype> child */
                fprintf(fp_out, "\t%sret = NULL;\n\n", ret_type);
            }
            else
            {
                assert(0);
            }

            fprintf(fp_out, "\t// Unimplemented function\n");
            fprintf(fp_out, "\tassert(0);\n");

            fprintf(fp_out, "\treturn ret;\n");
        }
        else
        {
            fprintf(fp_out, "\t// Unimplemented function\n");
            fprintf(fp_out, "\tassert(0);\n");
        }

        free(mgl_name);

        // end of function call
        fprintf(fp_out, "}\n\n");
    }
   else
    {
        assert(0);
    }
}

void print_init_dispatch_command(ezxml_t command, FILE *fp_out)
{
    ezxml_t proto, name;
    char *dispatch_name, *mgl_name;

    proto = ezxml_child(command, string("proto"));
    assert(proto);

    name = ezxml_child(proto, string("name"));

    dispatch_name = make_dispatch_str(name->txt);
    mgl_name = make_mgl_str(name->txt);

    fprintf(fp_out, "    ctx->dispatch.%s = %s;\n", dispatch_name, mgl_name);

    free(dispatch_name);
    free(mgl_name);
}

ezxml_t find_enum(ezxml_t registry, const char *str)
{
    ezxml_t nodes, node;

    for (nodes = ezxml_child(registry, "enums"); nodes; nodes = nodes->next)
    {
        for (node = ezxml_child(nodes, "enum"); node; node = node->next)
        {
            const char *name;

            name = ezxml_attr(node, "name");

            if (!strcmp(name, str))
            {
                return node;
            }
        }
    }

    return NULL;
}

ezxml_t find_command(ezxml_t registry, const char *str)
{
    ezxml_t nodes, node;

    for (nodes = ezxml_child(registry, "commands"); nodes; nodes = nodes->next)
    {
        for (node = ezxml_child(nodes, "command"); node; node = node->next)
        {
            ezxml_t proto, name;

            proto = ezxml_child(node, "proto");
            assert(proto);

            name = ezxml_child(proto, string("name"));
            assert(name);

            if (!strcmp(name->txt, str))
            {
                return node;
            }
        }
    }

    return NULL;
}

ezxml_t find_feature(ezxml_t registry, const char *feature, unsigned es_only)
{
    ezxml_t node;

    for (node = ezxml_child(registry, "feature"); node; node = node->next)
    {
        const char *api, *name;

        api = ezxml_attr(node, "api");
        name = ezxml_attr(node, "name");

        assert(api);
        assert(name);

        // search by api and version
        if (es_only == 0)
        {
            if (!strcmp(api, "gl") && !strcmp(name, feature))
            {
                return node;
            }
        }
        else
        {
            if (!strcmp(api, "gles2") && !strcmp(name, feature))
            {
                return node;
            }
        }
    }

    return NULL;
}

void print_features(ezxml_t registry, ezxml_t feature, int type, FILE *fout)
{
    ezxml_t require, node;

    for (require = ezxml_child(feature, "require"); require; require = require->next)
    {
        if (type == kEnum)
        {
            ezxml_t enum_node;

            for (node = ezxml_child(require, "enum"); node; node = node->next)
            {
                const char *name;

                name = ezxml_attr(node, "name");
                assert(name);

                enum_node = find_enum(registry, name);
                assert(enum_node);

                print_enum(enum_node, fout);
            }
        }
        else if (type == KCommand)
        {
            ezxml_t command_node;

            for (node = ezxml_child(require, "command"); node; node = node->next)
            {
                const char *name;

                name = ezxml_attr(node, "name");
                assert(name);

                command_node = find_command(registry, name);
                assert(command_node);

                print_command(command_node, kGLHeaders, fout);
            }
        }
        else
        {
            assert(0);
        }
    }
}

void print_required_features(ezxml_t registry, const char *version, int mode, FILE *fout, unsigned es_only)
{
    ezxml_t feature;

    feature = find_feature(registry, version, es_only);
    assert(feature);

    print_features(registry, feature, mode, fout);
}

void print_required(ezxml_t registry, FILE *fp_out, unsigned es_only)
{
    GLVersion *gl_versions = get_gl_versions(es_only);
    char version_str[64];

    for (int i=0; gl_versions[i].major !=0; i++)
    {
        if (es_only == 0)
        {
            sprintf(version_str, "GL_VERSION_%d_%d", gl_versions[i].major, gl_versions[i].minor);
        }
        else
        {
            sprintf(version_str, "GL_ES_VERSION_%d_%d", gl_versions[i].major, gl_versions[i].minor);
        }

        fprintf(fp_out, "\n\n#ifndef %s\n", version_str);
        fprintf(fp_out, "#define %s 1\n", version_str);
        fprintf(fp_out, "\n// %s enums\n\n", version_str);
        print_required_features(registry, string(version_str), kEnum, fp_out, es_only);
        fprintf(fp_out, "#endif // %s\n\n", version_str);
    }

    for (int i=0; gl_versions[i].major !=0; i++)
    {
        if (es_only == 0)
        {
            sprintf(version_str, "GL_VERSION_%d_%d", gl_versions[i].major, gl_versions[i].minor);
        }
        else
        {
            sprintf(version_str, "GL_ES_VERSION_%d_%d", gl_versions[i].major, gl_versions[i].minor);
        }

        fprintf(fp_out, "\n\n#ifndef %s\n", version_str);
        fprintf(fp_out, "#define %s 1\n", version_str);
        fprintf(fp_out, "\n// %s commands\n\n", version_str);
        print_required_features(registry, string(version_str), KCommand, fp_out, es_only);
        fprintf(fp_out, "#endif // %s\n\n", version_str);
    }
}

void print_required_feature_commands(ezxml_t registry, ezxml_t feature, int mode, FILE *fout)
{
    ezxml_t require, node;

    for (require = ezxml_child(feature, "require"); require; require = require->next)
    {
        ezxml_t command_node;

        for (node = ezxml_child(require, "command"); node; node = node->next)
        {
            const char *name;

            name = ezxml_attr(node, "name");
            assert(name);

            command_node = find_command(registry, name);
            assert(command_node);

            if (already_emitted(name))
                continue;

            if (mode == kMGLDispatchInit)
            {
                print_init_dispatch_command(command_node, fout);
            }
            else
            {
                print_command(command_node, mode, fout);
            }
        }
    }
}

void print_required_commands(ezxml_t registry, int mode, FILE *fout, unsigned es_only)
{
    GLVersion *gl_versions = get_gl_versions(es_only);
    char version_str[64];

    for (int i=0; gl_versions[i].major !=0; i++)
    {
        ezxml_t feature;

        if (es_only == 0)
        {
            sprintf(version_str, "GL_VERSION_%d_%d", gl_versions[i].major, gl_versions[i].minor);
        }
        else
        {
            sprintf(version_str, "GL_ES_VERSION_%d_%d", gl_versions[i].major, gl_versions[i].minor);
        }

        feature = find_feature(registry, version_str, es_only);
        assert(feature);

        print_required_feature_commands(registry, feature, mode, fout);
    }
}

/* Where the generated files land. The five the build compiles are written
   into the source tree; mgl.c and gl_core.h are reference output nothing
   consumes, so they stay in /tmp. */
static const char *g_src_dir = "/tmp";
static const char *g_include_dir = "/tmp";

static const char *out_path(const char *dir, const char *leaf)
{
    static char buf[8][1024];
    static int which;

    char *p = buf[which++ & 7];

    snprintf(p, sizeof(buf[0]), "%s/%s", dir, leaf);

    return p;
}

void print_about(FILE *fp_out, const char *filename)
{
    /* the leaf only, so the banner does not record where it was generated */
    const char *leaf = strrchr(filename, '/');

    fprintf(fp_out, "//\n// %s\n", leaf ? leaf + 1 : filename);
    fprintf(fp_out, "//\n// Autogenerated from gl.xml\n");
    fprintf(fp_out, "//\n// Mike Larson\n");
    fprintf(fp_out, "//\n// January 2026\n");
    fprintf(fp_out, "//\n\n");
}

void print_mgl_core_header(ezxml_t registry)
{
    FILE *fp_out;
    ezxml_t nodes, node;
    const char *filename = "/tmp/gl_core.h";

    //fp_out = fopen(filename, "w");
    fp_out = stdout;
    assert(fp_out);

    // print mgl_core.h header
    print_about(fp_out, filename);

    // print GL types
    for (nodes = ezxml_child(registry, "types"); nodes; nodes = nodes->next)
    {
        for (node = ezxml_child(nodes, "type"); node; node = node->next)
        {
            print_type(node, fp_out);
        }
    }

    // print all the required enums and prototypes
    print_required(registry, fp_out, 0);

    //fclose(fp_out);
}


/* What every generated entry-point file needs in front of it: the GL types,
   the context, and the lazy accessor the bodies below call. */
static void print_entry_point_prologue(FILE *fp_out)
{
    fprintf(fp_out, "#include \"glcorearb.h\"\n\n");
    fprintf(fp_out, "#include \"glm_context.h\"\n\n");
    fprintf(fp_out, "extern GLMContext _ctx;\n");
    fprintf(fp_out, "extern void mgl_lazy_init(void);\n\n");
    fprintf(fp_out, "#define GET_CONTEXT()   (mgl_lazy_init(), _ctx)\n\n");
}

void print_mgl_core_source(ezxml_t registry)
{
    FILE *fp_out;
    const char *filename = out_path(g_src_dir, "gl_core.c");

    fp_out = fopen(filename, "w");
    assert(fp_out);

    // print mgl_core.h header
    print_about(fp_out, filename);

    print_entry_point_prologue(fp_out);

    reset_seen();

    // print the required commands
    print_required_commands(registry, kGLFuncs, fp_out, 0);

    fclose(fp_out);
}

void print_mgl_es_source(ezxml_t registry)
{
    FILE *fp_out;
    const char *filename = out_path(g_src_dir, "gl_es.c");

    fp_out = fopen(filename, "w");
    assert(fp_out);

    // print mgl_core.h header
    print_about(fp_out, filename);

    print_entry_point_prologue(fp_out);

    reset_seen();

    // print the required commands
    print_required_commands(registry, kGLFuncs, fp_out, 1);

    fclose(fp_out);
}

void print_mgl_dispatch(ezxml_t registry)
{
    FILE *fp_out;
    const char *filename = out_path(g_include_dir, "glm_dispatch.h");

    fp_out = fopen(filename, "w");
    assert(fp_out);

    // print mgl_core.h header
    print_about(fp_out, filename);

    fprintf(fp_out, "#ifndef glm_dispatch_h\n");
    fprintf(fp_out, "#define glm_dispatch_h\n\n");
    fprintf(fp_out, "#include \"glcorearb.h\"\n");
    fprintf(fp_out, "#include \"gltypes.h\"\n\n");
    fprintf(fp_out, "typedef struct GLMContextRec_t *GLMContext;\n\n");
    fprintf(fp_out, "void init_dispatch(GLMContext ctx);\n\n");

    for (int version=0; version<2; version++)
    {
        if (version == 0)
        {
            fprintf(fp_out, "struct GLMDispatchTable {\n");
        }
        else
        {
            fprintf(fp_out, "\nstruct GLM_ES_DispatchTable {\n");
        }
        
        reset_seen();

        // print the required commands
        print_required_commands(registry, kMGLDispatch, fp_out, version);
        
        fprintf(fp_out, "};\n");
    }

    fprintf(fp_out, "\n\n#endif // #ifndef glm_dispatch_h\n");

    fclose(fp_out);
}

void print_mgl_init_dispatch(ezxml_t registry)
{
    FILE *fp_out;
    const char *filename = out_path(g_src_dir, "glm_dispatch.c");

    fp_out = fopen(filename, "w");
    assert(fp_out);

    // print mgl_core.h header
    print_about(fp_out, filename);

    fprintf(fp_out, "#include \"mgl.h\"\n\n");

    /* The pipeline-draw wrappers swap themselves over the table once it is
       filled in. Leaving this call out silently drops every wrapper, so it is
       generated rather than added by hand afterwards. */
    fprintf(fp_out, "void mglWrapPipelineDraws(GLMContext ctx);\n\n");

    for (int version=0; version<2; version++)
    {
        /* core and ES are two builds of the same driver, not two functions in
           one binary, so each defines init_dispatch behind its own guard */
        fprintf(fp_out, "#ifdef %s\n", version == 0 ? "MGL_GL_CORE" : "MGL_GL_ES");

        fprintf(fp_out, "void init_dispatch(GLMContext ctx)\n");

        fprintf(fp_out, "{\n");
        
        reset_seen();

        // print the required commands
        print_required_commands(registry, kMGLDispatchInit, fp_out, version);

        fprintf(fp_out, "\n    mglWrapPipelineDraws(ctx);\n");
        fprintf(fp_out, "}\n");
        fprintf(fp_out, "#endif\n\n");
    }
    
    fclose(fp_out);
}

void print_mgl_header(ezxml_t registry)
{
    FILE *fp_out;
    const char *filename = out_path(g_include_dir, "mgl.h");

    fp_out = fopen(filename, "w");
    assert(fp_out);

    // print mgl_core.h header
    print_about(fp_out, filename);

    fprintf(fp_out, "#ifndef mgl_h\n");
    fprintf(fp_out, "#define mgl_h\n\n");
    fprintf(fp_out, "#include <stdio.h>\n");
    fprintf(fp_out, "#include <stdlib.h>\n");
    fprintf(fp_out, "#include <strings.h>\n");
    fprintf(fp_out, "#include <assert.h>\n\n");
    fprintf(fp_out, "#include \"gltypes.h\"\n");
    fprintf(fp_out, "#include \"glcorearb.h\"\n");
    fprintf(fp_out, "#include \"glm_context.h\"\n\n");

    reset_seen();

    // print the required commands
    print_required_commands(registry, kMGLHeaders, fp_out, 0);

    /* ES adds a handful of entry points core does not have. The tally is not
       reset, so only those extras come out here. */
    fprintf(fp_out, "\n#ifdef MGL_GL_ES\n");
    print_required_commands(registry, kMGLHeaders, fp_out, 1);
    fprintf(fp_out, "#endif\n");

    fprintf(fp_out, "\n#endif /* mgl_h */\n");

    fclose(fp_out);
}

void print_mgl_functions(ezxml_t registry)
{
    FILE *fp_out;
    const char *filename = "/tmp/mgl.c";

    fp_out = fopen(filename, "w");
    assert(fp_out);

    // print mgl_core.h header
    print_about(fp_out, filename);

    fprintf(fp_out, "#include \"mgl.h\"\n\n");

    reset_seen();

    // print the required commands
    print_required_commands(registry, kMGLFuncs, fp_out, 0);

    fprintf(fp_out, "\n#ifdef MGL_GL_ES\n");
    print_required_commands(registry, kMGLFuncs, fp_out, 1);
    fprintf(fp_out, "#endif\n");

    fclose(fp_out);
}


int main(int argc, char **argv)
{
    ezxml_t registry;
    const char *xml = "gl.xml";

    if (argc > 1)
        xml = argv[1];

    if (argc > 2)
        g_src_dir = argv[2];

    if (argc > 3)
        g_include_dir = argv[3];

    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
    {
        fprintf(stderr, "usage: %s [gl.xml [src_dir [include_dir]]]\n", argv[0]);
        return 2;
    }

    registry = ezxml_parse_file(xml);

    if (registry == NULL)
    {
        fprintf(stderr, "%s: cannot read %s\n", argv[0], xml);
        return 1;
    }

    // mgl gl exported enums and prototypes
    print_mgl_core_header(registry);

    // mgl gl exported functions
    print_mgl_core_source(registry);

    // mgl es exported functions
    print_mgl_es_source(registry);

    // mgl dispatch table
    print_mgl_dispatch(registry);

    // mgl init dispatch table
    print_mgl_init_dispatch(registry);

    // mgl prototypes
    print_mgl_header(registry);

    // mgl functions
    print_mgl_functions(registry);

    ezxml_free(registry);

    return 0;
}
