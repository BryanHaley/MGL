cmake_minimum_required(VERSION 3.20)
project(triangle C)

# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/MooGL-SDK
find_package(MooGL REQUIRED)

add_executable(triangle main.c)
target_link_libraries(triangle PRIVATE MooGL::glfw)

# find the MooGL dylibs next to the program, then in the SDK
set_target_properties(triangle PROPERTIES BUILD_RPATH "@executable_path;${MooGL_LIBRARY_DIR}")
