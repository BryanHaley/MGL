# SPIRV-Headers comes from the submodule, not a nested clone
SPIRV_HEADERS="$(cd "$(dirname "$0")/../submodules/SPIRV-Headers" && pwd)"

if [ ! -d "SPIRV-Tools" ]
then
    git clone https://github.com/KhronosGroup/SPIRV-Tools.git --depth 1
    cd SPIRV-Tools
else
    cd SPIRV-Tools
    git pull
fi

if [ ! -d "build" ]
then
    mkdir build
    cd build
    cmake .. -DSPIRV-Headers_SOURCE_DIR="$SPIRV_HEADERS"
else
    cd build
fi

make -j 4
