cmake_minimum_required(VERSION 3.20)
project(sdl2_triangle C)

# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/MooGL-SDK
find_package(MooGL REQUIRED)
find_package(SDL2 REQUIRED CONFIG)

add_executable(sdl2_triangle main.c)
target_link_libraries(sdl2_triangle PRIVATE SDL2::SDL2 MooGL::GL)

set_target_properties(sdl2_triangle PROPERTIES BUILD_RPATH "@executable_path;${MooGL_LIBRARY_DIR}")
