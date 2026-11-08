#!/bin/bash
# Copyright (C) The MooGL Project
#
# Fetches and builds what MooGL links against, from a fresh clone:
#   submodules/SPIRV-Cross, SPIRV-Headers, glslang, FidelityFX-FSR2-OpenGL, SDL2
#     (SDL2 itself is built by make, after libmoogl)
#   external/SPIRV-Tools (cloned at a pinned commit)
#   external/glfw (in the tree)
# The CTS and IDKEngine submodules are left alone; they are only for testing.
#
# MACOSX_DEPLOYMENT_TARGET sets the oldest macOS the build runs on (default 13.0).

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
jobs="$(sysctl -n hw.ncpu)"
spirv_tools_commit=5262baed1ceceb131175be564547d2b11a76be84

cmake_build() {
    local src="$1" dir="$2"; shift 2
    cmake -S "$src" -B "$dir" -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" -DCMAKE_OSX_ARCHITECTURES=arm64 "$@"
    cmake --build "$dir" -j"$jobs"
}

git submodule update --init submodules/SPIRV-Cross submodules/SPIRV-Headers \
    submodules/glslang submodules/FidelityFX-FSR2-OpenGL submodules/SDL2

if [ ! -d external/SPIRV-Tools/.git ]; then
    git clone https://github.com/KhronosGroup/SPIRV-Tools.git external/SPIRV-Tools
fi
git -C external/SPIRV-Tools fetch -q origin "$spirv_tools_commit" || true
git -C external/SPIRV-Tools checkout -q "$spirv_tools_commit"

cmake_build external/SPIRV-Tools external/SPIRV-Tools/build \
    -DSPIRV-Headers_SOURCE_DIR="$root/submodules/SPIRV-Headers" -DSPIRV_SKIP_TESTS=ON -DSPIRV_WERROR=OFF

cmake_build submodules/SPIRV-Cross submodules/SPIRV-Cross/build -DSPIRV_CROSS_ENABLE_TESTS=OFF

cmake_build submodules/glslang submodules/glslang/build -DENABLE_OPT=0 -DGLSLANG_TESTS=OFF

cmake_build external/glfw external/glfw/build -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF \
    -DGLFW_BUILD_DOCS=OFF -DBUILD_SHARED_LIBS=OFF

echo "dependencies built for macOS $MACOSX_DEPLOYMENT_TARGET and later"
