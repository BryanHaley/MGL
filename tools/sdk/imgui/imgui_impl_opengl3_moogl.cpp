/*
 * Copyright (C) The MooGL Project
 *
 * imgui_impl_opengl3_moogl.cpp
 * MGL
 *
 * Dear ImGui's OpenGL 3 backend, calling MooGL directly. Its own loader
 * opens Apple's OpenGL framework on macOS.
 */

#include <GL/glcorearb.h>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include "imgui_impl_opengl3.cpp"
