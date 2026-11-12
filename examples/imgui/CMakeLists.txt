cmake_minimum_required(VERSION 3.20)
project(imgui_demo CXX)

# cmake -B build -DCMAKE_PREFIX_PATH=/path/to/MooGL-SDK
find_package(MooGL REQUIRED)

add_executable(imgui_demo main.cpp)
target_compile_features(imgui_demo PRIVATE cxx_std_17)
target_link_libraries(imgui_demo PRIVATE MooGL::imgui MooGL::glfw)

# find the MooGL dylibs next to the program, then in the SDK
set_target_properties(imgui_demo PROPERTIES BUILD_RPATH "@executable_path;${MooGL_LIBRARY_DIR}")
