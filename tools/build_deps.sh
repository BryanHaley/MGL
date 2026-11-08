#!/bin/bash
# Copyright (C) The MooGL Project
#
# Fetches and builds what MooGL links against, from a fresh clone:
#   submodules/SPIRV-Tools, SPIRV-Cross, SPIRV-Headers, glslang, FidelityFX-FSR2-OpenGL, glfw, SDL2
#     (SDL2 itself is built by make, after libmoogl)
# The CTS and IDKEngine submodules are left alone; they are only for testing.
#
# MACOSX_DEPLOYMENT_TARGET sets the oldest macOS the build runs on (default 13.0).

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
jobs="$(sysctl -n hw.ncpu)"

# Third-party code: its own warnings are not ours to fix, so they are kept quiet
cmake_build() {
    local src="$1" dir="$2"; shift 2
    CFLAGS=-w CXXFLAGS=-w cmake -S "$src" -B "$dir" -Wno-dev -Wno-deprecated -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" -DCMAKE_OSX_ARCHITECTURES=arm64 "$@"
    cmake --build "$dir" -j"$jobs"
}

git submodule update --init submodules/SPIRV-Tools submodules/SPIRV-Cross submodules/SPIRV-Headers \
    submodules/glslang submodules/FidelityFX-FSR2-OpenGL submodules/glfw submodules/SDL2

cmake_build submodules/SPIRV-Tools submodules/SPIRV-Tools/build \
    -DSPIRV-Headers_SOURCE_DIR="$root/submodules/SPIRV-Headers" -DSPIRV_SKIP_TESTS=ON -DSPIRV_WERROR=OFF \
    -DSPIRV_SKIP_EXECUTABLES=ON

cmake_build submodules/SPIRV-Cross submodules/SPIRV-Cross/build -DSPIRV_CROSS_ENABLE_TESTS=OFF -DSPIRV_CROSS_CLI=OFF

cmake_build submodules/glslang submodules/glslang/build -DENABLE_OPT=0 -DGLSLANG_TESTS=OFF -DENABLE_HLSL=OFF \
    -DENABLE_GLSLANG_BINARIES=OFF

cmake_build submodules/glfw submodules/glfw/build -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF \
    -DGLFW_BUILD_DOCS=OFF -DBUILD_SHARED_LIBS=OFF

echo "dependencies built for macOS $MACOSX_DEPLOYMENT_TARGET and later"
