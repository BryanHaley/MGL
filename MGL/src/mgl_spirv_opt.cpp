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
