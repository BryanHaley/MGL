/*
 * Copyright (C) The Moogle Project
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
 * mgl_reflect.cpp
 * MGL
 *
 * The program interface queries want GL's view of a program: array names
 * ending in [0], struct members spelled out, blocks named by block name, and
 * a note of which stages use what. glslang's reflection already speaks that
 * language, so each stage is linked on its own and the results are merged.
 */

#include "mgl_reflect.h"

#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <glslang/Include/Types.h>
#include <glslang/MachineIndependent/localintermediate.h>
#include <glslang/Public/ShaderLang.h>

// glslang's C interface keeps the C++ shader as the first thing in its handle
struct CShaderHandle {
    glslang::TShader *shader;
};

namespace {

const int kReflectOptions = EShReflectionStrictArraySuffix | EShReflectionBasicArraySuffix |
                            EShReflectionIntermediateIO | EShReflectionSeparateBuffers |
                            EShReflectionAllBlockVariables | EShReflectionUnwrapIOBlocks;

// names MGL made up for itself
bool internalName(const std::string &name)
{
    bool prefix = (name.compare(0, 3, "mgl") == 0 || name.compare(0, 3, "Mgl") == 0) &&
                  name.size() > 3 && std::isupper((unsigned char)name[3]);

    return prefix || name.find("_mglsr") != std::string::npos;
}

MglResource blank()
{
    MglResource r;

    std::memset(&r, 0, sizeof(r));
    r.offset = -1;
    r.block_index = -1;
    r.array_stride = -1;
    r.matrix_stride = -1;
    r.top_level_size = 0;
    r.binding = -1;
    r.location = -1;
    r.atomic_buffer = -1;
    r.xfb_buffer = -1;
    r.xfb_offset = -1;

    return r;
}

// "Block[3]" -> 3, anything else -> 0
int elementOf(const std::string &name)
{
    if (name.empty() || name.back() != ']')
        return 0;

    size_t open = name.rfind('[');

    return open == std::string::npos ? 0 : std::atoi(name.c_str() + open + 1);
}

struct Kind {
    std::vector<std::string> names;
    std::vector<MglResource> res;
    std::map<std::string, int> index;

    // the resource by this name, added if new
    int find(const std::string &name, bool *added)
    {
        auto it = index.find(name);

        *added = it == index.end();

        if (!*added)
            return it->second;

        int at = (int)res.size();

        index[name] = at;
        names.push_back(name);
        res.push_back(blank());

        return at;
    }
};

// std140 lines a matrix's vectors up on 16 bytes; std430 lets a two-component
// vector sit on 8
void matrixLayout(const glslang::TType &member, const glslang::TType *block, MglResource &r)
{
    if (!member.isMatrix())
    {
        r.matrix_stride = 0;
        r.row_major = 0;
        return;
    }

    glslang::TLayoutMatrix m = member.getQualifier().layoutMatrix;

    if (m == glslang::ElmNone && block)
        m = block->getQualifier().layoutMatrix;

    bool row = m == glslang::ElmRowMajor;
    bool std430 = block && block->getQualifier().layoutPacking == glslang::ElpStd430;
    int n = row ? member.getMatrixCols() : member.getMatrixRows();
    int scalar = member.getBasicType() == glslang::EbtDouble ? 8 : 4;
    int stride = std430 ? (n == 2 ? 2 * scalar : 4 * scalar) : 4 * scalar;

    if (!std430 && stride < 16)
        stride = 16;

    r.matrix_stride = stride;
    r.row_major = row ? 1 : 0;
}

void addBlocks(Kind &kind, glslang::TProgram &prog, bool storage, int stage, std::vector<int> &local)
{
    int n = storage ? prog.getNumBufferBlocks() : prog.getNumUniformBlocks();

    local.assign(n, -1);

    for (int i = 0; i < n; i++)
    {
        const glslang::TObjectReflection &o = storage ? prog.getBufferBlock(i) : prog.getUniformBlock(i);

        if (internalName(o.name))
            continue;

        bool added;
        int at = kind.find(o.name, &added);
        MglResource &r = kind.res[at];

        if (added)
        {
            int binding = o.getBinding();

            r.binding = binding >= 0 ? binding + elementOf(o.name) : -1;
            r.data_size = o.size;
        }

        r.stages |= 1u << stage;
        local[i] = at;
    }
}

void addVariables(Kind &kind, glslang::TProgram &prog, bool storage, int stage,
                  const std::vector<int> &local)
{
    int n = storage ? prog.getNumBufferVariables() : prog.getNumUniformVariables();

    for (int i = 0; i < n; i++)
    {
        const glslang::TObjectReflection &o = storage ? prog.getBufferVariable(i) : prog.getUniform(i);
        const glslang::TType *type = o.getType();

        if (internalName(o.name) || type == nullptr)
            continue;

        bool in_block = o.index >= 0 && o.index < (int)local.size();

        if (in_block && local[o.index] < 0)
            continue;

        bool added;
        int at = kind.find(o.name, &added);
        MglResource &r = kind.res[at];

        if (added)
        {
            r.type = (GLenum)o.glDefineType;
            r.array_size = o.size;

            if (in_block)
            {
                const glslang::TObjectReflection &b = storage ? prog.getBufferBlock(o.index)
                                                              : prog.getUniformBlock(o.index);

                r.block_index = local[o.index];
                r.offset = o.offset;
                r.array_stride = o.arrayStride;
                matrixLayout(*type, b.getType(), r);

                if (storage)
                {
                    r.top_level_size = o.topLevelArraySize > 0 ? o.topLevelArraySize : 1;
                    r.top_level_stride = o.topLevelArrayStride;

                    // a runtime-sized array has no size of its own
                    if (type->isArray() && (type->isUnsizedArray() || type->isImplicitlySizedArray()))
                        r.array_size = 0;
                }
            }
            else if (r.type == GL_UNSIGNED_INT_ATOMIC_COUNTER)
            {
                const glslang::TQualifier &q = type->getQualifier();

                r.binding = q.hasBinding() ? (int)q.layoutBinding : 0;
                r.offset = q.hasOffset() ? q.layoutOffset : 0;
                r.array_stride = type->isArray() ? 4 : 0;
                r.matrix_stride = 0;
            }
        }

        // a block member counts as used wherever its block is
        r.stages |= in_block ? 1u << stage : (GLuint)o.stages;
    }
}

// true when the word appears in the source on its own
bool mentions(const char *src, const char *word)
{
    size_t n = std::strlen(word);

    if (src == nullptr)
        return true;

    for (const char *at = std::strstr(src, word); at; at = std::strstr(at + 1, word))
    {
        bool left = at == src || !(std::isalnum((unsigned char)at[-1]) || at[-1] == '_');
        bool right = !(std::isalnum((unsigned char)at[n]) || at[n] == '_');

        if (left && right)
            return true;
    }

    return false;
}

// glslang lists every member of the built-in gl_PerVertex block; GL lists the
// ones the shader writes or reads
bool unusedBuiltin(const std::string &name, const char *src)
{
    static const char *members[] = { "gl_PointSize", "gl_ClipDistance", "gl_CullDistance", "gl_Position" };

    for (const char *m : members)
        if (name.find(m) != std::string::npos)
            return !mentions(src, m);

    return false;
}

void addPipe(Kind &kind, glslang::TProgram &prog, bool outputs, int stage, const char *src)
{
    int n = outputs ? prog.getNumPipeOutputs() : prog.getNumPipeInputs();

    for (int i = 0; i < n; i++)
    {
        const glslang::TObjectReflection &o = outputs ? prog.getPipeOutput(i) : prog.getPipeInput(i);
        const glslang::TType *type = o.getType();

        if (internalName(o.name) || type == nullptr || unusedBuiltin(o.name, src))
            continue;

        std::string name = o.name;

        // the per-vertex array of a geometry or tessellation stage is not part
        // of the name GL reports
        bool per_vertex = !type->getQualifier().patch &&
                          ((!outputs && (stage == EShLangTessControl || stage == EShLangTessEvaluation ||
                                         stage == EShLangGeometry)) ||
                           (outputs && stage == EShLangTessControl));
        size_t sub = name.find("[0]");

        if (per_vertex && type->isArray() && sub != std::string::npos &&
            name.find('.') == std::string::npos)
            name.erase(sub, 3);

        bool added;
        int at = kind.find(name, &added);
        MglResource &r = kind.res[at];

        if (added)
        {
            const glslang::TQualifier &q = type->getQualifier();

            r.type = (GLenum)o.glDefineType;
            r.array_size = o.size > 0 ? o.size : 1;

            if (per_vertex && sub != std::string::npos && name.find("[0]") == std::string::npos)
                r.array_size = 1;

            r.location = q.hasLocation() ? (int)q.layoutLocation : -1;
            r.location_index = q.hasIndex() ? (int)q.layoutIndex : 0;
            r.component = q.hasComponent() ? (int)q.layoutComponent : 0;
            r.per_patch = q.patch ? 1 : 0;
            r.xfb_buffer = q.hasXfbBuffer() ? (int)q.layoutXfbBuffer : -1;
            r.xfb_offset = q.hasXfbOffset() ? (int)q.layoutXfbOffset : -1;
        }

        r.stages |= 1u << stage;
    }
}

void emit(Kind &kind, MglResourceTable *out, int k)
{
    out->count[k] = (GLint)kind.res.size();
    out->list[k] = (MglResource *)std::calloc(kind.res.size() ? kind.res.size() : 1, sizeof(MglResource));

    for (size_t i = 0; i < kind.res.size(); i++)
    {
        out->list[k][i] = kind.res[i];
        out->list[k][i].name = strdup(kind.names[i].c_str());
    }
}

// every variable of a block or buffer, in index order
void listMembers(MglResourceTable *out, int block_kind, int var_kind)
{
    for (GLint b = 0; b < out->count[block_kind]; b++)
    {
        MglResource &block = out->list[block_kind][b];
        std::vector<GLint> members;

        for (GLint v = 0; v < out->count[var_kind]; v++)
        {
            MglResource &var = out->list[var_kind][v];
            bool mine = block_kind == MGL_RES_ATOMIC_BUFFER ? var.atomic_buffer == b : var.block_index == b;

            if (mine)
                members.push_back(v);
        }

        block.num_active = (GLint)members.size();
        block.active = (GLint *)std::calloc(members.size() ? members.size() : 1, sizeof(GLint));

        for (size_t m = 0; m < members.size(); m++)
            block.active[m] = members[m];
    }
}

}

extern "C" bool mglReflectProgram(void *const *shaders, const char *const *sources, int stage_count,
                                  MglResourceTable *out)
{
    Kind kinds[MGL_RES_KINDS];
    int first = -1, last = -1, capture = -1;

    std::memset(out, 0, sizeof(*out));

    // GL's pipeline order is glslang's stage order, compute aside
    for (int s = 0; s < stage_count; s++)
    {
        if (shaders[s] == nullptr)
            continue;

        if (first < 0)
            first = s;

        last = s;

        if (s != EShLangFragment && s != EShLangCompute)
            capture = s;
    }

    if (first < 0)
        return false;

    for (int s = 0; s < stage_count; s++)
    {
        if (shaders[s] == nullptr)
            continue;

        glslang::TShader *shader = ((CShaderHandle *)shaders[s])->shader;
        glslang::TProgram prog;

        if (shader == nullptr)
            continue;

        prog.addShader(shader);

        if (!prog.link(EShMsgDefault) || !prog.buildReflection(kReflectOptions))
            continue;

        std::vector<int> ub, sb;

        addBlocks(kinds[MGL_RES_UNIFORM_BLOCK], prog, false, s, ub);
        addBlocks(kinds[MGL_RES_STORAGE_BLOCK], prog, true, s, sb);
        addVariables(kinds[MGL_RES_UNIFORM], prog, false, s, ub);
        addVariables(kinds[MGL_RES_BUFFER_VARIABLE], prog, true, s, sb);

        if (s == first)
            addPipe(kinds[MGL_RES_PROGRAM_INPUT], prog, false, s, sources[s]);

        if (s == last)
            addPipe(kinds[MGL_RES_PROGRAM_OUTPUT], prog, true, s, sources[s]);

        // what transform feedback records counts as used, even if nothing reads it
        if (s == capture)
        {
            glslang::TProgram all;

            all.addShader(shader);

            if (all.link(EShMsgDefault) && all.buildReflection(kReflectOptions | EShReflectionAllIOVariables))
                addPipe(kinds[MGL_RES_CAPTURE_SOURCE], all, true, s, nullptr);
        }
    }

    // the buffers the capture source's xfb qualifiers name, in buffer order
    {
        Kind &src = kinds[MGL_RES_CAPTURE_SOURCE];
        std::map<int, std::vector<int>> by_buffer;

        for (size_t v = 0; v < src.res.size(); v++)
            if (src.res[v].xfb_offset >= 0)
                by_buffer[src.res[v].xfb_buffer >= 0 ? src.res[v].xfb_buffer : 0].push_back((int)v);

        for (auto &entry : by_buffer)
        {
            MglResource buffer = blank();

            buffer.binding = entry.first;
            kinds[MGL_RES_XFB_BUFFER].res.push_back(buffer);
            kinds[MGL_RES_XFB_BUFFER].names.push_back(std::string());
        }
    }

    // atomic counters live in buffers named only by their binding
    {
        Kind &uniforms = kinds[MGL_RES_UNIFORM];
        std::map<int, std::vector<int>> by_binding;

        for (size_t u = 0; u < uniforms.res.size(); u++)
            if (uniforms.res[u].type == GL_UNSIGNED_INT_ATOMIC_COUNTER)
                by_binding[uniforms.res[u].binding].push_back((int)u);

        for (auto &entry : by_binding)
        {
            MglResource buffer = blank();
            int at = (int)kinds[MGL_RES_ATOMIC_BUFFER].res.size();

            buffer.binding = entry.first;

            for (int u : entry.second)
            {
                MglResource &r = uniforms.res[u];
                int end = r.offset + 4 * (r.array_size > 0 ? r.array_size : 1);

                if (end > buffer.data_size)
                    buffer.data_size = end;

                buffer.stages |= r.stages;
                r.atomic_buffer = at;
            }

            kinds[MGL_RES_ATOMIC_BUFFER].res.push_back(buffer);
            kinds[MGL_RES_ATOMIC_BUFFER].names.push_back(std::string());
        }
    }

    for (int k = 0; k < MGL_RES_KINDS; k++)
        emit(kinds[k], out, k);

    listMembers(out, MGL_RES_UNIFORM_BLOCK, MGL_RES_UNIFORM);
    listMembers(out, MGL_RES_STORAGE_BLOCK, MGL_RES_BUFFER_VARIABLE);
    listMembers(out, MGL_RES_ATOMIC_BUFFER, MGL_RES_UNIFORM);

    return true;
}

extern "C" void mglFreeResourceTable(MglResourceTable *table)
{
    if (table == nullptr)
        return;

    for (int k = 0; k < MGL_RES_KINDS; k++)
    {
        for (GLint i = 0; i < table->count[k]; i++)
        {
            std::free(table->list[k][i].name);
            std::free(table->list[k][i].active);
        }

        std::free(table->list[k]);
    }

    std::memset(table, 0, sizeof(*table));
}

namespace {

int scalarBytes(const glslang::TType &t)
{
    return t.getBasicType() == glslang::EbtDouble ? 8 : 4;
}

int bytesOf(const glslang::TType &t);

// struct members sit on their own scalar size, one after another
int structBytes(const glslang::TType &t)
{
    int off = 0, worst = 4;

    for (const glslang::TTypeLoc &m : *t.getStruct())
    {
        int align = scalarBytes(*m.type);

        off = (off + align - 1) / align * align + bytesOf(*m.type);

        if (align > worst)
            worst = align;
    }

    return off;
}

int bytesOf(const glslang::TType &t)
{
    if (t.isArray())
    {
        glslang::TType elem(t, 0);

        return t.getOuterArraySize() * bytesOf(elem);
    }

    if (t.isStruct())
        return structBytes(t);

    int n = t.isMatrix() ? t.getMatrixCols() * t.getMatrixRows() : t.getVectorSize();

    return n * scalarBytes(t);
}

char kindOf(const glslang::TType &t)
{
    switch (t.getBasicType())
    {
        case glslang::EbtDouble: return 'd';
        case glslang::EbtInt:    return 'i';
        case glslang::EbtUint:   return 'u';
        case glslang::EbtBool:   return 'b';
        default:                 return 'f';
    }
}

void flatten(const glslang::TType &t, const std::string &expr, int offset, int buffer,
             std::vector<MglXfbItem> &out)
{
    if (t.isArray())
    {
        glslang::TType elem(t, 0);
        int step = bytesOf(elem);

        for (int i = 0; i < t.getOuterArraySize(); i++)
            flatten(elem, expr + "[" + std::to_string(i) + "]", offset + i * step, buffer, out);

        return;
    }

    if (t.isStruct())
    {
        int off = offset;

        for (const glslang::TTypeLoc &m : *t.getStruct())
        {
            int align = scalarBytes(*m.type);

            off = (off + align - 1) / align * align;
            flatten(*m.type, expr + "." + m.type->getFieldName().c_str(), off, buffer, out);
            off += bytesOf(*m.type);
        }

        return;
    }

    MglXfbItem item;

    std::memset(&item, 0, sizeof(item));
    std::snprintf(item.expr, sizeof(item.expr), "%s", expr.c_str());
    item.buffer = buffer;
    item.offset = offset;
    item.kind = kindOf(t);
    item.components = t.isMatrix() ? t.getMatrixCols() * t.getMatrixRows() : t.getVectorSize();
    item.rows = t.isMatrix() ? t.getMatrixRows() : 0;
    out.push_back(item);
}

}

extern "C" int mglXfbLayout(void *shader, MglXfbItem *items, int max_items, GLint *stride_bytes, int max_buffers)
{
    for (int b = 0; b < max_buffers; b++)
        stride_bytes[b] = 0;

    if (shader == nullptr || ((CShaderHandle *)shader)->shader == nullptr)
        return 0;

    const glslang::TIntermediate *interm = ((CShaderHandle *)shader)->shader->getIntermediate();

    if (interm == nullptr || interm->getTreeRoot() == nullptr)
        return 0;

    glslang::TIntermAggregate *root = interm->getTreeRoot()->getAsAggregate();

    if (root == nullptr)
        return 0;

    glslang::TIntermAggregate *linker = nullptr;

    for (TIntermNode *n : root->getSequence())
    {
        glslang::TIntermAggregate *a = n->getAsAggregate();

        if (a && a->getOp() == glslang::EOpLinkerObjects)
            linker = a;
    }

    if (linker == nullptr)
        return 0;

    std::vector<MglXfbItem> found;
    std::vector<int> ends(max_buffers, 0);
    bool doubles = false;

    for (TIntermNode *n : linker->getSequence())
    {
        glslang::TIntermSymbol *sym = n->getAsSymbolNode();

        if (sym == nullptr || sym->getQualifier().storage != glslang::EvqVaryingOut)
            continue;

        const glslang::TType &type = sym->getType();
        const glslang::TQualifier &q = type.getQualifier();
        std::string name = sym->getName().c_str();
        size_t before = found.size();

        if (type.getBasicType() == glslang::EbtBlock)
        {
            if (type.isArray())
                continue;

            bool anonymous = name.compare(0, 5, "anon@") == 0;

            for (const glslang::TTypeLoc &m : *type.getStruct())
            {
                const glslang::TQualifier &mq = m.type->getQualifier();

                if (!mq.hasXfbOffset())
                    continue;

                std::string field = m.type->getFieldName().c_str();
                int buffer = mq.hasXfbBuffer() ? (int)mq.layoutXfbBuffer
                           : q.hasXfbBuffer() ? (int)q.layoutXfbBuffer : 0;

                flatten(*m.type, anonymous ? field : name + "." + field, (int)mq.layoutXfbOffset, buffer, found);
            }
        }
        else if (q.hasXfbOffset())
        {
            flatten(type, name, (int)q.layoutXfbOffset, q.hasXfbBuffer() ? (int)q.layoutXfbBuffer : 0, found);
        }

        for (size_t i = before; i < found.size(); i++)
        {
            MglXfbItem &it = found[i];
            int size = it.components * (it.kind == 'd' ? 8 : 4);

            if (it.buffer < 0 || it.buffer >= max_buffers)
                return -1;

            if (it.offset + size > ends[it.buffer])
                ends[it.buffer] = it.offset + size;

            doubles = doubles || it.kind == 'd';
        }
    }

    if ((int)found.size() > max_items)
        return -1;

    for (int b = 0; b < max_buffers; b++)
    {
        unsigned declared = interm->getXfbStride(b);

        if (declared != glslang::TQualifier::layoutXfbStrideEnd)
            stride_bytes[b] = (GLint)declared;
        else if (ends[b])
            stride_bytes[b] = doubles ? (ends[b] + 7) / 8 * 8 : (ends[b] + 3) / 4 * 4;
    }

    for (size_t i = 0; i < found.size(); i++)
        items[i] = found[i];

    return (int)found.size();
}

static bool locationFits(const glslang::TType &t, int location, int max, EShLanguage stage,
                         const char *name, const char *dir, char *msg, size_t n)
{
    glslang::TType element(t, 0);
    int size = (t.isArray() && t.getQualifier().isArrayedIo(stage))
                   ? glslang::TIntermediate::computeTypeLocationSize(element, stage)
                   : glslang::TIntermediate::computeTypeLocationSize(t, stage);

    if (location + size <= max)
        return true;

    std::snprintf(msg, n, "ERROR: %s %s: location %d is past the last %s location (%d)\n",
                  dir, name, location, dir, max - 1);
    return false;
}

extern "C" bool mglVaryingLocationsFit(void *shader, int max_in, int max_out, char *msg, size_t n)
{
    if (shader == nullptr || ((CShaderHandle *)shader)->shader == nullptr)
        return true;

    const glslang::TIntermediate *interm = ((CShaderHandle *)shader)->shader->getIntermediate();

    if (interm == nullptr || interm->getTreeRoot() == nullptr)
        return true;

    glslang::TIntermAggregate *root = interm->getTreeRoot()->getAsAggregate();

    if (root == nullptr)
        return true;

    EShLanguage stage = interm->getStage();

    for (TIntermNode *n0 : root->getSequence())
    {
        glslang::TIntermAggregate *a = n0->getAsAggregate();

        if (a == nullptr || a->getOp() != glslang::EOpLinkerObjects)
            continue;

        for (TIntermNode *node : a->getSequence())
        {
            glslang::TIntermSymbol *sym = node->getAsSymbolNode();

            if (sym == nullptr)
                continue;

            const glslang::TType &t = sym->getType();
            const glslang::TQualifier &q = t.getQualifier();
            int max;
            const char *dir;

            if (q.storage == glslang::EvqVaryingIn && stage != EShLangVertex)
                max = max_in, dir = "input";
            else if (q.storage == glslang::EvqVaryingOut && stage != EShLangFragment)
                max = max_out, dir = "output";
            else
                continue;

            if (max <= 0 || q.builtIn != glslang::EbvNone)
                continue;

            if (q.hasLocation())
            {
                if (!locationFits(t, q.layoutLocation, max, stage, sym->getName().c_str(), dir, msg, n))
                    return false;
            }
            else if (t.isStruct() && t.getBasicType() == glslang::EbtBlock)
            {
                for (const glslang::TTypeLoc &m : *t.getStruct())
                {
                    const glslang::TQualifier &mq = m.type->getQualifier();

                    if (mq.hasLocation() &&
                        !locationFits(*m.type, mq.layoutLocation, max, stage,
                                      m.type->getFieldName().c_str(), dir, msg, n))
                        return false;
                }
            }
        }
    }

    return true;
}
