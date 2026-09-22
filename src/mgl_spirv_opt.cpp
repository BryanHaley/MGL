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
 * mgl_spirv_opt.cpp
 * MGL
 *
 */

#include <spirv-tools/optimizer.hpp>

#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" bool mglInlineSpirv(const unsigned int *words, size_t count,
                               unsigned int **out_words, size_t *out_count);

// Fold every called function into its caller. MGL's rewrites leave the
// application's own main as a separate function that the generated main calls,
// and SPIRV-Cross then has to hand that function every uniform it touches as a
// parameter -- which Metal refuses, because a uniform lives in constant address
// space and a plain reference parameter does not.
extern "C" bool mglInlineSpirv(const unsigned int *words, size_t count,
                               unsigned int **out_words, size_t *out_count)
{
    if (words == nullptr || count == 0 || out_words == nullptr || out_count == nullptr)
        return false;

    // Inlining makes Metal's compile several times slower, so it is kept to
    // the modules that need it: a helper function alongside main, and a plain
    // uniform that is not a texture or sampler, which is what SPIRV-Cross
    // hands across with the wrong address space.
    if (count > 5 && words[0] == 0x07230203)
    {
        std::vector<uint32_t> opaque;       // ids of image, sampler and sampled image types
        size_t functions = 0;
        bool loose_uniform = false;
        bool tess_stage = false;

        for (size_t i = 5; i < count;)
        {
            uint32_t wc = words[i] >> 16, op = words[i] & 0xffff;

            if (wc == 0 || i + wc > count)
                break;

            if ((op == 25 || op == 26 || op == 27) && wc >= 2)         // Image, Sampler, SampledImage
                opaque.push_back(words[i + 1]);
            else if ((op == 28 || op == 29) && wc >= 3)                // arrays of them
            {
                for (uint32_t t : opaque)
                    if (t == words[i + 2]) { opaque.push_back(words[i + 1]); break; }
            }
            else if (op == 32 && wc >= 4 && words[i + 2] == 0)         // pointer to UniformConstant
            {
                bool is_opaque = false;

                for (uint32_t t : opaque)
                    if (t == words[i + 3]) is_opaque = true;

                if (!is_opaque)
                    loose_uniform = true;
            }
            else if (op == 54)                                          // OpFunction
                functions++;
            else if (op == 15 && wc >= 2 && (words[i + 1] == 1 || words[i + 1] == 2))
                tess_stage = true;          // gl_in reaches a helper with the wrong constness

            i += wc;
        }

        if (functions < 2 || !(loose_uniform || tess_stage))
            return false;
    }

    spvtools::Optimizer opt(SPV_ENV_UNIVERSAL_1_5);

    opt.SetMessageConsumer([](spv_message_level_t, const char *, const spv_position_t &, const char *) {});

    opt.RegisterPass(spvtools::CreateInlineExhaustivePass());

    std::vector<uint32_t> result;

    if (!opt.Run(words, count, &result) || result.empty())
        return false;

    unsigned int *buf = (unsigned int *)malloc(result.size() * sizeof(unsigned int));

    if (buf == nullptr)
        return false;

    memcpy(buf, result.data(), result.size() * sizeof(unsigned int));

    *out_words = buf;
    *out_count = result.size();

    return true;
}

// ---------------------------------------------------------------------------
// Atomic counters
//
// SPIRV-Cross turns an atomic operation into Metal's atomic_fetch_add_explicit
// and friends, but only knows how to declare the thing being counted when it
// is a member of a storage buffer. A bare "uniform atomic_uint" comes out
// referenced and never declared, and the shader does not compile.
//
// So each counter is moved into a buffer block of uints, one block per GL
// atomic binding, at element offset / 4. The atomic operations are left alone,
// so they stay genuinely atomic on the GPU. The ids of the new blocks and their
// GL bindings are handed back, so reflection can file them as atomic counter
// buffers rather than ordinary storage buffers.
// ---------------------------------------------------------------------------

#include <map>

namespace {

enum : uint32_t {
    OpName = 5, OpEntryPoint = 15, OpTypeInt = 21, OpTypeArray = 28,
    OpTypeRuntimeArray = 29, OpTypeStruct = 30, OpTypePointer = 32,
    OpConstant = 43, OpFunction = 54, OpFunctionCall = 57, OpVariable = 59,
    OpAccessChain = 65, OpInBoundsAccessChain = 66, OpDecorate = 71,
    OpMemberDecorate = 72, OpDecorationGroup = 73, OpGroupDecorate = 74,
    OpGroupMemberDecorate = 75, OpCopyObject = 83, OpIAdd = 128, OpIMul = 132,
    OpAtomicStore = 228, OpAtomicFlagClear = 319,
    DecBlock = 2, DecBufferBlock = 3, DecArrayStride = 6, DecBinding = 33,
    DecDescriptorSet = 34, DecOffset = 35,
    SCUniform = 2, SCAtomicCounter = 10,
};

const uint32_t kAtomicSet = 5;      // a descriptor set nothing else uses

using Inst = std::vector<uint32_t>;

bool isAtomicOp(uint32_t op)
{
    return (op >= 227 && op <= 242) || op == 318 || op == OpAtomicFlagClear ||
           op == 5614 || op == 5615 || op == 6035;
}

bool isAnnotation(uint32_t op)
{
    return op == OpDecorate || op == OpMemberDecorate || op == OpDecorationGroup ||
           op == OpGroupDecorate || op == OpGroupMemberDecorate || op == 332 ||
           op == 1178 || op == 1179;
}

Inst makeString(uint32_t op, uint32_t target, const std::string &s)
{
    Inst in = {0, target};
    size_t words = s.size() / 4 + 1;
    size_t at = in.size();
    in.resize(at + words, 0);
    memcpy(&in[at], s.c_str(), s.size());
    in[0] = ((uint32_t)in.size() << 16) | op;
    return in;
}

Inst make(uint32_t op, std::initializer_list<uint32_t> operands)
{
    Inst in = {0};
    in.insert(in.end(), operands);
    in[0] = ((uint32_t)in.size() << 16) | op;
    return in;
}

struct Counter {
    uint32_t binding = 0, offset = 0;
    std::vector<uint32_t> dims;     // array lengths, outermost first; empty for a scalar
};

} // namespace

extern "C" int mglLowerAtomicCounters(const unsigned int *words, size_t count,
                                      unsigned int **out_words, size_t *out_count,
                                      unsigned int *block_ids, unsigned int *block_bindings,
                                      int max_blocks);

extern "C" int mglLowerAtomicCounters(const unsigned int *words, size_t count,
                                      unsigned int **out_words, size_t *out_count,
                                      unsigned int *block_ids, unsigned int *block_bindings,
                                      int max_blocks)
{
    if (words == nullptr || count < 5 || words[0] != 0x07230203)
        return 0;

    std::vector<Inst> ins;
    for (size_t i = 5; i < count;)
    {
        uint32_t wc = words[i] >> 16;
        if (wc == 0 || i + wc > count)
            return 0;
        ins.emplace_back(words + i, words + i + wc);
        i += wc;
    }

    std::map<uint32_t, uint32_t> constValue;             // OpConstant id -> low word
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> arrays;   // id -> (elem, length const)
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> ptrs;     // id -> (storage class, pointee)
    std::map<uint32_t, Counter> counters;                // atomic variable id -> where it lives
    uint32_t uintType = 0;

    for (auto &in : ins)
    {
        uint32_t op = in[0] & 0xffff;
        if (op == OpTypeInt && in.size() >= 4 && in[2] == 32 && in[3] == 0 && !uintType)
            uintType = in[1];
        else if (op == OpConstant && in.size() >= 4)
            constValue[in[2]] = in[3];
        else if (op == OpTypeArray && in.size() >= 4)
            arrays[in[1]] = {in[2], in[3]};
        else if (op == OpTypePointer && in.size() >= 4)
            ptrs[in[1]] = {in[2], in[3]};
        else if (op == OpVariable && in.size() >= 4 && in[3] == SCAtomicCounter)
        {
            Counter c;
            uint32_t t = ptrs.count(in[1]) ? ptrs[in[1]].second : 0;
            while (arrays.count(t))
            {
                if (!constValue.count(arrays[t].second))
                    return 0;       // a specialisation-sized array is not handled
                c.dims.push_back(constValue[arrays[t].second]);
                t = arrays[t].first;
            }
            counters[in[2]] = c;
        }
    }

    if (counters.empty() || !uintType)
        return 0;

    for (auto &in : ins)
    {
        uint32_t op = in[0] & 0xffff;
        if (op == OpDecorate && in.size() >= 4 && counters.count(in[1]))
        {
            if (in[2] == DecBinding) counters[in[1]].binding = in[3];
            if (in[2] == DecOffset)  counters[in[1]].offset = in[3];
        }
    }

    uint32_t bound = words[3];
    auto fresh = [&bound]() { return bound++; };

    // one block per GL binding, sized by nothing: a runtime array of uints
    std::map<uint32_t, uint32_t> blockVar;                // GL binding -> variable id
    std::vector<Inst> newTypes, newDecor, newNames;

    uint32_t rta = fresh(), uintPtr = fresh(), c0 = fresh();
    newTypes.push_back(make(OpTypeRuntimeArray, {rta, uintType}));
    newTypes.push_back(make(OpTypePointer, {uintPtr, SCUniform, uintType}));
    newTypes.push_back(make(OpConstant, {uintType, c0, 0}));
    newDecor.push_back(make(OpDecorate, {rta, DecArrayStride, 4}));

    for (auto &kv : counters)
    {
        uint32_t b = kv.second.binding;
        if (blockVar.count(b))
            continue;
        if ((int)blockVar.size() >= max_blocks)
            return 0;

        uint32_t st = fresh(), stPtr = fresh(), var = fresh();
        newTypes.push_back(make(OpTypeStruct, {st, rta}));
        newTypes.push_back(make(OpTypePointer, {stPtr, SCUniform, st}));
        newTypes.push_back(make(OpVariable, {stPtr, var, SCUniform}));
        newDecor.push_back(make(OpDecorate, {st, DecBufferBlock}));
        newDecor.push_back(make(OpMemberDecorate, {st, 0, DecOffset, 0}));
        newDecor.push_back(make(OpDecorate, {var, DecDescriptorSet, kAtomicSet}));
        newDecor.push_back(make(OpDecorate, {var, DecBinding, b}));
        newNames.push_back(makeString(OpName, st, "MglAtomicCounters" + std::to_string(b)));
        newNames.push_back(makeString(OpName, var, "mglAtomic" + std::to_string(b)));
        blockVar[b] = var;
    }

    std::map<uint32_t, uint32_t> constCache;
    auto uintConst = [&](uint32_t v) {
        if (v == 0) return c0;
        if (!constCache.count(v))
        {
            uint32_t id = fresh();
            newTypes.push_back(make(OpConstant, {uintType, id, v}));
            constCache[v] = id;
        }
        return constCache[v];
    };

    std::vector<Inst> out;
    bool inFunction = false;

    for (auto &in : ins)
    {
        uint32_t op = in[0] & 0xffff;

        if (op == OpFunction)
            inFunction = true;

        // the counters themselves, and everything that names them, go away
        if (op == OpVariable && in.size() >= 4 && counters.count(in[2]))
            continue;
        if ((op == OpDecorate || op == OpName) && in.size() >= 2 && counters.count(in[1]))
            continue;

        if (op == OpEntryPoint)
        {
            Inst e;
            for (size_t i = 0; i < in.size(); i++)
                if (i < 3 || !counters.count(in[i]))
                    e.push_back(in[i]);
            e[0] = ((uint32_t)e.size() << 16) | op;
            out.push_back(e);
            continue;
        }

        if (!inFunction)
        {
            // a pointer to one counter now points into a buffer block
            if (op == OpTypePointer && in.size() >= 4 && in[2] == SCAtomicCounter && in[3] == uintType)
            {
                Inst p = in;
                p[2] = SCUniform;
                out.push_back(p);
                continue;
            }
            out.push_back(in);
            continue;
        }

        // an element of a counter array: flatten the indices onto the block
        if ((op == OpAccessChain || op == OpInBoundsAccessChain) && in.size() >= 5 && counters.count(in[3]))
        {
            Counter &c = counters[in[3]];
            size_t nidx = in.size() - 4;
            if (nidx != c.dims.size())
                return 0;           // a partial chain into an array of arrays

            uint32_t lin = uintConst(c.offset / 4);
            for (size_t k = 0; k < nidx; k++)
            {
                uint32_t stride = 1;
                for (size_t j = k + 1; j < c.dims.size(); j++)
                    stride *= c.dims[j];

                uint32_t term = in[4 + k];
                if (stride != 1)
                {
                    uint32_t mul = fresh();
                    out.push_back(make(OpIMul, {uintType, mul, term, uintConst(stride)}));
                    term = mul;
                }
                uint32_t sum = fresh();
                out.push_back(make(OpIAdd, {uintType, sum, lin, term}));
                lin = sum;
            }
            out.push_back(make(OpAccessChain, {in[1], in[2], blockVar[c.binding], c0, lin}));
            continue;
        }

        // a scalar counter used directly: point at its element first
        std::vector<size_t> slots;
        if (isAtomicOp(op))
            slots.push_back((op == OpAtomicStore || op == OpAtomicFlagClear) ? 1 : 3);
        else if (op == OpFunctionCall)
            for (size_t i = 4; i < in.size(); i++) slots.push_back(i);
        else if (op == OpCopyObject)
            slots.push_back(3);

        Inst copy = in;
        for (size_t s : slots)
        {
            if (s >= copy.size() || !counters.count(copy[s]))
                continue;
            Counter &c = counters[copy[s]];
            if (!c.dims.empty())
                return 0;           // a whole array handed on is not handled
            uint32_t p = fresh();
            out.push_back(make(OpAccessChain, {uintPtr, p, blockVar[c.binding], c0, uintConst(c.offset / 4)}));
            copy[s] = p;
        }
        out.push_back(copy);
    }

    // splice the new declarations into their sections
    auto firstOf = [&out](bool (*pred)(uint32_t)) {
        for (size_t i = 0; i < out.size(); i++)
            if (pred(out[i][0] & 0xffff)) return i;
        return out.size();
    };
    size_t fn = firstOf([](uint32_t o) { return o == OpFunction; });
    out.insert(out.begin() + fn, newTypes.begin(), newTypes.end());

    size_t lastAnno = 0;
    bool anyAnno = false;
    for (size_t i = 0; i < out.size(); i++)
        if (isAnnotation(out[i][0] & 0xffff)) { lastAnno = i; anyAnno = true; }
    size_t decoAt;
    if (anyAnno)
        decoAt = lastAnno + 1;
    else
        decoAt = firstOf([](uint32_t o) { return o == OpTypeInt || o == 19 || o == 20 || o == 22 || o == OpTypePointer || o == 33; });
    out.insert(out.begin() + decoAt, newDecor.begin(), newDecor.end());

    size_t nameAt = firstOf([](uint32_t o) { return isAnnotation(o); });
    out.insert(out.begin() + nameAt, newNames.begin(), newNames.end());

    std::vector<uint32_t> result(words, words + 5);
    result[3] = bound;
    for (auto &in : out)
        result.insert(result.end(), in.begin(), in.end());

    unsigned int *buf = (unsigned int *)malloc(result.size() * sizeof(unsigned int));
    if (buf == nullptr)
        return 0;
    memcpy(buf, result.data(), result.size() * sizeof(unsigned int));
    *out_words = buf;
    *out_count = result.size();

    int n = 0;
    for (auto &kv : blockVar)
    {
        block_bindings[n] = kv.first;
        block_ids[n] = kv.second;
        n++;
    }
    return n;
}

extern "C" int mglCountAtomicCounters(const unsigned int *words, size_t count);

// How many atomic counters a module declares, an array counting one per
// element. The per-stage limits are checked against this.
extern "C" int mglCountAtomicCounters(const unsigned int *words, size_t count)
{
    if (words == nullptr || count < 5 || words[0] != 0x07230203)
        return 0;

    std::map<uint32_t, uint32_t> constValue;
    std::map<uint32_t, uint32_t> arrayLength;           // array type -> element count
    std::map<uint32_t, uint32_t> counterPtr;            // AtomicCounter pointer -> pointee
    int total = 0;

    for (size_t i = 5; i < count;)
    {
        uint32_t wc = words[i] >> 16, op = words[i] & 0xffff;

        if (wc == 0 || i + wc > count)
            break;

        if (op == 43 && wc >= 4)                                    // OpConstant
            constValue[words[i + 2]] = words[i + 3];
        else if (op == 28 && wc >= 4)                               // OpTypeArray
            arrayLength[words[i + 1]] = constValue.count(words[i + 3]) ? constValue[words[i + 3]] : 1;
        else if (op == 32 && wc >= 4 && words[i + 2] == 10)         // pointer to AtomicCounter
            counterPtr[words[i + 1]] = words[i + 3];
        else if (op == 59 && wc >= 4 && words[i + 3] == 10)         // OpVariable in AtomicCounter
        {
            uint32_t pointee = counterPtr.count(words[i + 1]) ? counterPtr[words[i + 1]] : 0;

            total += arrayLength.count(pointee) ? (int)arrayLength[pointee] : 1;
        }

        i += wc;
    }

    return total;
}
