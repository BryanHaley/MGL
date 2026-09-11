set SDKROOT=`xcrun --show-sdk-path`

# SPIRV-Headers is a submodule; SPIRV-Tools is told where to find it
SPIRV_HEADERS="$(cd "$(dirname "$0")/../submodules/SPIRV-Headers" && pwd)"

cp ../MGL/include/MGLContext.h glfw/src
cp ../MGL/include/MGLRenderer.h glfw/src
cd SPIRV-Tools
mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DSPIRV-Headers_SOURCE_DIR="$SPIRV_HEADERS"
make -j 4
cd ../..
cd ../submodules/SPIRV-Cross
mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make -j 4
cd ../../../external
cd glslang
./update_glslang_sources.py
mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make -j 4
cd ../..
cd glfw
mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make -j 4 glfw
cd ../..
