#!/bin/bash
# Copyright (C) The MooGL Project
#
# Builds BoyBaykiller's FSR 2 C# bindings (the FFX_FSR2 NuGet package) for
# macOS, with MGL's MetalFX FSR 2 libraries inside it.
#
# The published package is laid out for Windows, where wchar_t is 2 bytes.
# On macOS it is 4, so FfxResource and friends are bigger; macos.patch builds
# the name fields with 4-byte characters when FFX_WCHAR32 is defined.
#
# Output: build/nuget/FFX_FSR2.<version>-macos.nupkg. Needs `make fsr2` first.

set -euo pipefail

mgl="$(cd "$(dirname "$0")/../.." && pwd)"
here="$mgl/tools/fsr2_csharp"
work="$mgl/build/fsr2_csharp"
out="$mgl/build/nuget"
commit=12c709ba140c70d5e0f4ba8716b119ce78952292    # 1.0.3
version=1.0.3-macos

for lib in libffx_fsr2_api_x64.dylib libffx_fsr2_api_gl_x64.dylib; do
    [ -f "$mgl/build/$lib" ] || { echo "build/$lib is missing; run make fsr2" >&2; exit 1; }
done

if [ ! -d "$work/src" ]; then
    git clone -q https://github.com/BoyBaykiller/FidelityFX-FSR2-CSharpBindings.git "$work/src"
fi

git -C "$work/src" checkout -q --force "$commit"
git -C "$work/src" apply "$here/macos.patch"

mkdir -p "$work/natives" "$out"
cp "$mgl/build/libffx_fsr2_api_x64.dylib" "$mgl/build/libffx_fsr2_api_gl_x64.dylib" "$work/natives/"

dotnet pack "$work/src/FidelityFX-FSR2-CSharpBindings/FFX_FSR2.csproj" -c Release \
    -p:DefineConstants=FFX_WCHAR32 -p:Version="$version" -p:MacOSNatives="$work/natives" \
    -p:GeneratePackageOnBuild=false -o "$out"

# a rebuilt package with the same version must not come back from the cache
rm -rf "${NUGET_PACKAGES:-$HOME/.nuget/packages}/ffx_fsr2/$version"

echo "built $out/FFX_FSR2.$version.nupkg"
