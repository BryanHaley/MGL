#!/bin/bash
# Copyright (C) The MooGL Project
#
# Builds the MooGL SDK: dylibs, headers, a CMake package, an example and the
# licences, as dist/MooGL-SDK-<version>-macos-arm64.tar.gz.
#
#   tools/make_sdk.sh [version] [--no-deps]
#
# version is 0.1.0 unless given; a release passes its tag, and a leading "v" is
# dropped. It ends up in the SDK's name, the dylibs, GL_VERSION and the CMake
# package. --no-deps skips tools/build_deps.sh when
# the dependencies are already built. With dotnet installed, the macOS FSR 2
# C# package (tools/fsr2_csharp) goes in too.
#
# Signing, so macOS loads the downloaded libraries without complaint:
#   MOOGL_SIGN_IDENTITY   a "Developer ID Application: ..." identity in the keychain
# and then notarizing, with either
#   MOOGL_NOTARY_PROFILE  a profile saved with `xcrun notarytool store-credentials`
# or an App Store Connect API key:
#   MOOGL_NOTARY_KEY (path to the .p8), MOOGL_NOTARY_KEY_ID, MOOGL_NOTARY_ISSUER

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

version=""
deps=1
for arg in "$@"; do
    case "$arg" in
        --no-deps) deps=0 ;;
        *) version="$arg" ;;
    esac
done
version="${version:-0.1.0}"
version="${version#v}"

export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-13.0}"
build="$root/build/sdk"
name="MooGL-SDK-$version-macos-arm64"
out="$root/dist/$name"
fsr2="submodules/FidelityFX-FSR2-OpenGL/src/ffx-fsr2-api"

[ "$deps" = 1 ] && tools/build_deps.sh

# its own build folder, so the deployment target cannot mix with a dev build
make -j"$(sysctl -n hw.ncpu)" build_dir=build/sdk MOOGL_VERSION="$version" lib

# stock SDL2 builds too, but makes Apple's OpenGL contexts
otool -L "$build/libSDL2-2.0.0.dylib" | grep -q libmoogl ||
    { echo "libSDL2 is not linked to MooGL: submodules/SDL2 is not MooGL's SDL2 fork" >&2; exit 1; }

libs="libmoogl.dylib libglfw.dylib libSDL2-2.0.0.dylib libffx_fsr2_api.dylib"

if [ -n "${MOOGL_SIGN_IDENTITY:-}" ]; then
    for lib in $libs; do
        codesign --force --timestamp --options runtime --sign "$MOOGL_SIGN_IDENTITY" "$build/$lib"
    done
fi

# the Makefile points the repo-root dylib links at whatever it built last
for lib in libmoogl.dylib libmoogles.dylib; do
    [ -f "build/$lib" ] && ln -fs "build/$lib" .
done

rm -rf "$out"
mkdir -p "$out/lib/cmake/MooGL" "$out/include/GL" "$out/include/KHR" "$out/include/GLFW" \
    "$out/include/ffx-fsr2/gl" "$out/include/ffx-fsr2/shaders" "$out/include/ffx-fsr2/glad" \
    "$out/examples" "$out/licenses"

for lib in $libs; do
    cp "$build/$lib" "$out/lib/"
done
# programs built against a stock GLFW or SDL2 ask for them by these names
ln -s libglfw.dylib "$out/lib/libglfw.3.dylib"
ln -s libSDL2-2.0.0.dylib "$out/lib/libSDL2-2.0.dylib"
ln -s libSDL2-2.0.0.dylib "$out/lib/libSDL2.dylib"

cp include/GL/glcorearb.h "$out/include/GL/"
cp include/KHR/khrplatform.h "$out/include/KHR/"
cp include/MGLContext.h "$out/include/"
cp external/glfw/include/GLFW/glfw3.h external/glfw/include/GLFW/glfw3native.h "$out/include/GLFW/"
cp -R "$build/sdl2/install/include/SDL2" "$out/include/"
cp -R "$build/sdl2/install/lib/cmake/SDL2" "$out/lib/cmake/"
cp "$build/sdl2/install/lib/libSDL2main.a" "$out/lib/"

cp "$fsr2"/*.h "$out/include/ffx-fsr2/"
cp "$fsr2"/shaders/*.h "$out/include/ffx-fsr2/shaders/"
cp "$fsr2"/gl/ffx_fsr2_gl.h "$out/include/ffx-fsr2/gl/"
cp -R "$fsr2"/gl/external/glad/include/ "$out/include/ffx-fsr2/glad/"

cp tools/sdk/MooGLConfig.cmake "$out/lib/cmake/MooGL/"
cat > "$out/lib/cmake/MooGL/MooGLConfigVersion.cmake" <<EOF
set(PACKAGE_VERSION "$version")
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
EOF
cp -R examples/triangle examples/sdl2_triangle "$out/examples/"
rm -rf "$out"/examples/*/build

cp LICENSE "$out/licenses/MooGL.txt"
cp external/glfw/LICENSE.md "$out/licenses/GLFW.txt"
cp submodules/SPIRV-Cross/LICENSE "$out/licenses/SPIRV-Cross.txt"
cp submodules/SPIRV-Headers/LICENSE "$out/licenses/SPIRV-Headers.txt"
cp external/SPIRV-Tools/LICENSE "$out/licenses/SPIRV-Tools.txt"
cp submodules/glslang/LICENSE.txt "$out/licenses/glslang.txt"
cp submodules/FidelityFX-FSR2-OpenGL/LICENSE.txt "$out/licenses/FidelityFX-FSR2.txt"
cp submodules/SDL2/LICENSE.txt "$out/licenses/SDL2.txt"

if command -v dotnet >/dev/null; then
    BUILD_DIR="$build" tools/fsr2_csharp/build.sh
    mkdir -p "$out/nuget"
    cp "$build"/nuget/FFX_FSR2.*-macos.nupkg "$out/nuget/"
fi

cat > "$out/README.md" <<EOF
# MooGL SDK $version

OpenGL 4.6 core on Metal, for Apple silicon Macs on macOS $MACOSX_DEPLOYMENT_TARGET or later.

- \`lib/\`: libmoogl (OpenGL), libglfw and libSDL2 (built to make MooGL contexts), and
  the FSR 2 libraries on MetalFX
- \`include/\`: GL/glcorearb.h, GLFW, SDL2, MGLContext.h, and AMD's FSR 2 headers
- \`lib/cmake\`: \`find_package(MooGL)\` with MooGL::GL, MooGL::glfw and MooGL::FSR2, and
  \`find_package(SDL2)\`
- \`examples/\`: \`cmake -B build -DCMAKE_PREFIX_PATH=\$PWD/../.. && cmake --build build\`
- \`nuget/\`: the FFX_FSR2 C# package for macOS, if present

The full guide is at https://github.com/BryanHaley/MGL.
EOF

# the examples have to build against what was just packed
cc -O1 -o "$build/triangle" examples/triangle/main.c -I"$out/include" -L"$out/lib" -lglfw -lmoogl \
    -Wl,-rpath,"$out/lib"
cc -O1 -o "$build/sdl2_triangle" examples/sdl2_triangle/main.c -I"$out/include" -L"$out/lib" -lSDL2 -lmoogl \
    -Wl,-rpath,"$out/lib"

notary=()
if [ -n "${MOOGL_NOTARY_PROFILE:-}" ]; then
    notary=(--keychain-profile "$MOOGL_NOTARY_PROFILE")
elif [ -n "${MOOGL_NOTARY_KEY:-}" ]; then
    notary=(--key "$MOOGL_NOTARY_KEY" --key-id "$MOOGL_NOTARY_KEY_ID" --issuer "$MOOGL_NOTARY_ISSUER")
fi

if [ -n "${MOOGL_SIGN_IDENTITY:-}" ] && [ ${#notary[@]} -gt 0 ]; then
    rm -f "$build/notarize.zip"
    ditto -c -k --keepParent "$out" "$build/notarize.zip"
    xcrun notarytool submit "$build/notarize.zip" "${notary[@]}" --wait | tee "$build/notarize.log"
    grep -q "status: Accepted" "$build/notarize.log" || { echo "notarization failed" >&2; exit 1; }
else
    echo "not signed and notarized: macOS will quarantine these libraries when downloaded"
fi

mkdir -p dist
tar -C dist -czf "dist/$name.tar.gz" "$name"
echo "built dist/$name.tar.gz"
