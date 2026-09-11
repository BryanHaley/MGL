# SPIRV-Headers is the submodule and is header-only, so there is nothing to
# build. SPIRV-Tools is pointed at it by xcbuild-SPIRV-Tools.sh, and the Xcode
# header search path names it directly.
headers="$(cd "$(dirname "$0")/../submodules/SPIRV-Headers" && pwd)" || {
    echo "submodules/SPIRV-Headers is missing - run: git submodule update --init" >&2
    exit 1
}

echo "SPIRV-Headers: using $headers"
