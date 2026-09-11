# SPIRV-Cross is the submodule, not a clone of its own: the Makefile builds and
# links that copy, and MGL's own patches live in it.
cd "$(dirname "$0")/../submodules/SPIRV-Cross"

if [ ! -d "build" ]
then
    mkdir build
    cd build
    cmake ..
else
    cd build
fi

make -j 4
