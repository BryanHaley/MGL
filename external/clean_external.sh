# SPIRV-Cross and SPIRV-Headers are submodules; handled by git submodule
cd SPIRV-Tools
cd build
make clean
cd ../..
cd glslang
./update_glslang_sources.py
cd build
make clean
cd ../..
cd glfw
cd build
make clean
cd ../..
