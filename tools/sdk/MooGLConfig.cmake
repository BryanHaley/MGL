# Copyright (C) The MooGL Project
#
# find_package(MooGL) for the MooGL SDK. Targets:
#   MooGL::GL    OpenGL 4.6 (libmoogl), with GL/glcorearb.h
#   MooGL::glfw  GLFW built to make MooGL contexts; links MooGL::GL
#   MooGL::FSR2  AMD FSR 2's API on MetalFX, with AMD's headers
#   MooGL::imgui Dear ImGui (docking) with its GLFW, SDL2 and OpenGL 3
#                backends, the last one drawing with MooGL. Link MooGL::glfw
#                or SDL2::SDL2 alongside it for the platform backend you use.
# MooGL_LIBRARY_DIR is the folder holding the dylibs.

get_filename_component(MooGL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(MooGL_LIBRARY_DIR "${MooGL_ROOT}/lib")
set(MooGL_INCLUDE_DIR "${MooGL_ROOT}/include")

if(NOT TARGET MooGL::GL)
    add_library(MooGL::GL SHARED IMPORTED)
    set_target_properties(MooGL::GL PROPERTIES
        IMPORTED_LOCATION "${MooGL_LIBRARY_DIR}/libmoogl.dylib"
        IMPORTED_SONAME "@rpath/libmoogl.dylib"
        INTERFACE_INCLUDE_DIRECTORIES "${MooGL_INCLUDE_DIR}")

    add_library(MooGL::glfw SHARED IMPORTED)
    set_target_properties(MooGL::glfw PROPERTIES
        IMPORTED_LOCATION "${MooGL_LIBRARY_DIR}/libglfw.dylib"
        IMPORTED_SONAME "@rpath/libglfw.dylib"
        INTERFACE_INCLUDE_DIRECTORIES "${MooGL_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES MooGL::GL)

    add_library(MooGL::FSR2 SHARED IMPORTED)
    set_target_properties(MooGL::FSR2 PROPERTIES
        IMPORTED_LOCATION "${MooGL_LIBRARY_DIR}/libffx_fsr2_api.dylib"
        IMPORTED_SONAME "@rpath/libffx_fsr2_api.dylib"
        INTERFACE_INCLUDE_DIRECTORIES "${MooGL_INCLUDE_DIR}/ffx-fsr2;${MooGL_INCLUDE_DIR}/ffx-fsr2/glad"
        INTERFACE_COMPILE_DEFINITIONS FFX_GCC
        INTERFACE_LINK_LIBRARIES MooGL::GL)

    add_library(MooGL::imgui STATIC IMPORTED)
    set_target_properties(MooGL::imgui PROPERTIES
        IMPORTED_LOCATION "${MooGL_LIBRARY_DIR}/libimgui.a"
        INTERFACE_INCLUDE_DIRECTORIES "${MooGL_INCLUDE_DIR}/imgui"
        INTERFACE_LINK_LIBRARIES MooGL::GL)
endif()

set(MooGL_FOUND TRUE)
