#-include config.mk

SHELL := /bin/bash

# Find SDK path via xcode-select, backwards compatible with Xcode vers < 4.5
# on M1 monterey, comment out the following line
SDK_ROOT = $(shell xcrun --sdk macosx --show-sdk-path)

# SPIRV-Cross now comes from submodules/ -- current upstream plus the fp64
# emulation. The old external/ fork still builds: override these three on the
# make line to point back at it.
spirv_cross_include_path ?= ./submodules/SPIRV-Cross
spirv_cross_config_include_path ?= ./submodules/SPIRV-Cross
spirv_cross_lib_path ?= ./submodules/SPIRV-Cross/build
spirv_headers_path ?= ./submodules/SPIRV-Headers

spirv_tools_include_path ?= ./external/SPIRV-Tools/include
spirv_tools_path ?= ./external/SPIRV-Tools/build

glslang_include_path ?= ./submodules/glslang/glslang/Include


#glslang_path ?= glslang
#glslang_include_path ?= $(glslang_path)/build/include/glslang $(glslang_path)/glslang/Include
#glslang_lib_path ?= $(glslang_path)/build/glslang $(glslang_path)/build/OGLCompilersDLL $(glslang_path)/build/glslang/OSDependent/Unix $(glslang_path)/build/StandAlone $(glslang_path)/build/SPIRV

# build dirs
build_dir ?= build
build_core_dir := $(build_dir)/core
build_es_dir := $(build_dir)/es

CFLAGS += -Wall #-Wunused-parameter #-Wextra
CFLAGS += -gfull
CFLAGS += -O2
#CFLAGS += -00
# Disable AddressSanitizer for production - causes crashes when loaded via dlopen()
#CFLAGS += -fsanitize=address
#LIBS += -fsanitize=address
# The compatibility profile leftovers old games need are on by default.
# make MGL_NO_COMPAT=1 builds a driver that answers only for core.
ifdef MGL_NO_COMPAT
CFLAGS += -DMGL_NO_COMPAT_PROFILE
endif

CFLAGS += -arch $(shell uname -m)
LIBS += -arch $(shell uname -m)

LIBS += -F$(SDK_ROOT)/System/Library/Frameworks
LIBS += -framework Metal -framework OpenGL -framework Foundation

CFLAGS += -I$(spirv_cross_include_path)
CFLAGS += -I$(spirv_cross_config_include_path)
CFLAGS += -I$(spirv_tools_include_path)
CFLAGS += -I$(glslang_include_path)


CFLAGS += -Iinclude

# MooGL's version. Local builds are 0.1.0; a release passes the tag's.
MOOGL_VERSION ?= 0.1.0
moogl_version_number := $(shell echo '$(MOOGL_VERSION)' | sed 's/[^0-9.].*//')
CFLAGS += -I$(build_dir)/include
CFLAGS += -Iinclude/GL # "glcorearb.h"
CFLAGS += -DENABLE_OPT=0 -DSPIRV_CROSS_C_API_MSL=1 -DSPIRV_CROSS_C_API_GLSL=1 -DSPIRV_CROSS_C_API_CPP=1 -DSPIRV_CROSS_C_API_REFLECT=1

# GLFW configuration for shared library build
CFLAGS += -I./external/glfw/include -I./external/glfw/src
CXXFLAGS += -I./external/glfw/include -I./external/glfw/src

# macOS specific compile definitions for GLFW
CFLAGS += -D_COCOA -D_GLFW_COCOA
CXXFLAGS += -D_COCOA -D_GLFW_COCOA
CXXFLAGS += -I./external/SPIRV-Tools/include -Iinclude -std=c++17
CXXFLAGS += -I./submodules/glslang -Iinclude/GL

# GL_CORE SPECIFIC FLAGS
CFLAGS_GL_CORE := $(CFLAGS) -DMGL_GL_CORE
CXXFLAGS_GL_CORE := $(CXXFLAGS) -DMGL_GL_CORE

# GL_ES SPECIFIC FLAGS
CFLAGS_GL_ES := $(CFLAGS) -DMGL_GL_ES
CXXFLAGS_GL_ES := $(CXXFLAGS) -DMGL_GL_ES

# Add CoreFoundation framework headers for GLFW Objective-C compilation
GLFW_FRAMEWORKS = -framework Cocoa -framework CoreFoundation -framework CoreGraphics \
                  -framework IOKit -framework Foundation -framework QuartzCore \
                  -framework Metal -framework OpenGL

# GLFW sources for shared library build - macOS specific configuration
GLFW_SRC_DIR = external/glfw/src
GLFW_C_SOURCES = $(GLFW_SRC_DIR)/context.c \
                $(GLFW_SRC_DIR)/init.c \
                $(GLFW_SRC_DIR)/input.c \
                $(GLFW_SRC_DIR)/monitor.c \
                $(GLFW_SRC_DIR)/vulkan.c \
                $(GLFW_SRC_DIR)/window.c \
                $(GLFW_SRC_DIR)/osmesa_context.c \
                $(GLFW_SRC_DIR)/egl_context.c \
                $(GLFW_SRC_DIR)/posix_thread.c \
                $(GLFW_SRC_DIR)/posix_module.c \
                $(GLFW_SRC_DIR)/cocoa_time.c \
                $(GLFW_SRC_DIR)/platform.c

GLFW_M_SOURCES = $(GLFW_SRC_DIR)/cocoa_init.m \
                $(GLFW_SRC_DIR)/cocoa_joystick.m \
                $(GLFW_SRC_DIR)/cocoa_monitor.m \
                $(GLFW_SRC_DIR)/cocoa_window.m \
                $(GLFW_SRC_DIR)/mgl_context.m

# Simplified GLFW object paths - use a flat structure for easier building
GLFW_BUILD_DIR = $(build_dir)/glfw
GLFW_C_OBJS = $(GLFW_C_SOURCES:$(GLFW_SRC_DIR)/%.c=$(GLFW_BUILD_DIR)/%.o)

GLFW_M_OBJS = $(GLFW_M_SOURCES:$(GLFW_SRC_DIR)/%.m=$(GLFW_BUILD_DIR)/%.o)
glfw_objs = $(GLFW_C_OBJS) $(GLFW_M_OBJS)

ifneq ($(SDK_ROOT),)
CFLAGS_GL_CORE += -isysroot $(SDK_ROOT)
CFLAGS_GL_ES += -isysroot $(SDK_ROOT)
endif

# SPIRV-Cross and glslang, as tools/build_deps.sh builds them
spirv_cross_libs := c msl glsl hlsl cpp reflect util core
LIBS += $(foreach l,$(spirv_cross_libs),$(spirv_cross_lib_path)/libspirv-cross-$(l).a)
glslang_lib_path := submodules/glslang/build/glslang
LIBS += $(glslang_lib_path)/libglslang.a $(glslang_lib_path)/libMachineIndependent.a \
        $(glslang_lib_path)/libGenericCodeGen.a $(glslang_lib_path)/libglslang-default-resource-limits.a
LIBS += -lc++

# SPIRV-Tools
LIBS += external/SPIRV-Tools/build/source/lint/libSPIRV-Tools-lint.a
LIBS += external/SPIRV-Tools/build/source/reduce/libSPIRV-Tools-reduce.a
LIBS += external/SPIRV-Tools/build/source/diff/libSPIRV-Tools-diff.a
LIBS += external/SPIRV-Tools/build/source/libSPIRV-Tools.a
LIBS += external/SPIRV-Tools/build/source/link/libSPIRV-Tools-link.a
LIBS += external/SPIRV-Tools/build/source/opt/libSPIRV-Tools-opt.a


# --
# no need to tweak after this line, hopefully

default: lib

# --- generated sources ---------------------------------------------------
# These five come out of tools/spec_parser and are not checked in. They are
# listed by hand rather than picked up by the wildcard below, because on a
# clean tree they do not exist yet when make expands it.
gen_src_dir := src
gen_inc_dir := include

generated_srcs := $(gen_src_dir)/gl_core.c $(gen_src_dir)/gl_es.c $(gen_src_dir)/glm_dispatch.c
generated_hdrs := $(gen_inc_dir)/glm_dispatch.h $(gen_inc_dir)/mgl.h
generated := $(generated_srcs) $(generated_hdrs)

registry_xml := tools/spec_parser/gl.xml
spec_parser := $(build_dir)/spec_parser

# mgl
#mgl_srcs_c := $(wildcard src/*.c)
mgl_srcs_c := $(filter-out $(generated_srcs), $(wildcard src/*.c))
mgl_srcs_c += $(gen_src_dir)/glm_dispatch.c

mgl_srcs_cpp := $(wildcard src/*.cpp)

mgl_srcs_objc := $(wildcard src/*.m)

mgl_core_c := src/gl_core.c
mgl_es_c := src/gl_es.c

mgl_core_obj := $(mgl_core_c:.c=.o)
mgl_core_obj := $(addprefix $(build_core_dir)/,$(mgl_core_obj))

mgl_es_obj := $(mgl_es_c:.c=.o)
mgl_es_obj := $(addprefix $(build_es_dir)/,$(mgl_es_obj))

# core objs
mgl_core_objs := $(mgl_srcs_c:.c=.o) $(mgl_srcs_cpp:.cpp=.o)
mgl_core_objs := $(addprefix $(build_core_dir)/,$(mgl_core_objs))

mgl_core_objs := $(mgl_srcs_c:.c=.o) $(mgl_srcs_cpp:.cpp=.o)
mgl_core_objs := $(addprefix $(build_core_dir)/,$(mgl_core_objs))

mgl_core_arc_objs := $(mgl_srcs_objc:.m=.o)
mgl_core_arc_objs := $(addprefix $(build_core_dir)/arc/,$(mgl_core_arc_objs))

# es objs
mgl_es_objs := $(mgl_srcs_c:.c=.o) $(mgl_srcs_cpp:.cpp=.o)
mgl_es_objs := $(addprefix $(build_es_dir)/,$(mgl_es_objs))

mgl_es_objs := $(mgl_srcs_c:.c=.o) $(mgl_srcs_cpp:.cpp=.o)
mgl_es_objs := $(addprefix $(build_es_dir)/,$(mgl_es_objs))

mgl_es_arc_objs := $(mgl_srcs_objc:.m=.o)
mgl_es_arc_objs := $(addprefix $(build_es_dir)/arc/,$(mgl_es_arc_objs))


# --- generating them -----------------------------------------------------
# The registry copy lives in the tree so a build needs no network. Refresh it
# from external/OpenGL-Registry when Khronos publishes a new one.
$(spec_parser): tools/spec_parser/spec_parser.c tools/enum_parser/ezxml.c
	@mkdir -p $(dir $@)
	$(CC) -O2 -o $@ $^ -Itools/enum_parser -w

# One run writes all five, so the work hangs off a stamp rather than off each
# file; the per-file rule only re-runs it when a file has gone missing.
gen_stamp := $(build_dir)/.generated

$(gen_stamp): $(spec_parser) $(registry_xml)
	@mkdir -p $(dir $@)
	$(spec_parser) $(registry_xml) $(gen_src_dir) $(gen_inc_dir)
	@touch $@

$(generated): $(gen_stamp)
	@test -f $@ || $(spec_parser) $(registry_xml) $(gen_src_dir) $(gen_inc_dir)

generate: $(generated)

# these must match the object lists actually built, or header edits
# silently leave stale objects with mismatched struct layouts
deps += $(mgl_core_objs:.o=.d)
deps += $(mgl_core_arc_objs:.o=.d)
deps += $(mgl_core_obj:.o=.d)
deps += $(mgl_es_objs:.o=.d)
deps += $(mgl_es_arc_objs:.o=.d)
deps += $(mgl_es_obj:.o=.d)
deps += $(glfw_objs:.o=.d)


mgl_lib := $(build_dir)/libmoogl.dylib
mgl_es_lib := $(build_dir)/libmoogles.dylib

# a rebuilt SPIRV-Cross has to relink the driver, or it keeps the old one
spirv_cross_archives := $(wildcard submodules/SPIRV-Cross/build/libspirv-cross-*.a)

mgl_toolchain_obj := $(build_dir)/src/mgl_toolchain.o
mgl_toolchain_lib := $(build_dir)/libmoogl_toolchain.a

# rewritten only when the version changes, so get.c rebuilds only then
version_h := $(build_dir)/include/moogl_version.h

$(version_h): FORCE
	@mkdir -p $(dir $@)
	@echo '#define MOOGL_VERSION "$(MOOGL_VERSION)"' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

$(build_core_dir)/src/get.o $(build_es_dir)/src/get.o: $(version_h)

FORCE:

$(mgl_lib): $(mgl_core_objs) $(mgl_core_arc_objs) $(mgl_core_obj) $(spirv_cross_archives)
	@mkdir -p $(dir $@)
	$(CC) -D$(CFLAGS_GL_CORE) -dynamiclib -o $@ $(filter %.o,$^) $(LIBS) \
		-install_name @rpath/libmoogl.dylib -current_version $(moogl_version_number)
	# loading dynamic library requires this
	ln -fs $(mgl_lib) .

$(mgl_es_lib): $(mgl_es_objs) $(mgl_es_arc_objs) $(mgl_es_obj) $(spirv_cross_archives)
	@mkdir -p $(dir $@)
	$(CC) -D$(CFLAGS_GL_ES) -dynamiclib -o $@ $(filter %.o,$^) $(LIBS) \
		-install_name @rpath/libmoogles.dylib
	# loading dynamic library requires this
	ln -fs $(mgl_es_lib) .


$(mgl_toolchain_lib): $(mgl_toolchain_obj)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

# GLFW's own build, made by tools/build_deps.sh; rebuilt when its sources change
external/glfw/build/src/libglfw3.a: $(wildcard external/glfw/src/*.c external/glfw/src/*.m external/glfw/src/*.h)
	cmake --build external/glfw/build

# Build GLFW shared library from pre-built static library
$(build_dir)/libglfw.dylib: external/glfw/build/src/libglfw3.a $(mgl_lib)
	@echo "Creating GLFW shared library from static library..."
	@mkdir -p $(dir $@)
	$(CC) -shared -fPIC -dynamiclib \
		-Wl,-force_load,$(word 1,$^) \
		-L$(build_dir) -lmoogl \
		-o $@ \
		$(GLFW_FRAMEWORKS) \
		-Wl,-rpath,@loader_path \
		-install_name @rpath/libglfw.dylib
	@echo "✅ GLFW shared library built: $@"
	@echo "This enables compatibility with Minecraft mods and Prism Launcher"


# FSR 2's API on MetalFX: all 14 functions, AMD's core and GL ones, in one library
fsr2_api_dir := submodules/FidelityFX-FSR2-OpenGL/src/ffx-fsr2-api
fsr2_lib := $(build_dir)/libffx_fsr2_api.dylib

$(fsr2_lib): src/fsr2/ffx_fsr2_metalfx.mm include/MGLContext.h $(mgl_lib)
	$(CXX) -std=c++17 -O2 -fobjc-arc -DFFX_GCC -Iinclude -I$(fsr2_api_dir) \
		-I$(fsr2_api_dir)/gl/external/glad/include -dynamiclib -o $@ $< \
		-L$(build_dir) -lmoogl -framework Metal -framework MetalFX -framework Foundation \
		-Wl,-rpath,@loader_path -install_name @rpath/libffx_fsr2_api.dylib -current_version $(moogl_version_number)

fsr2: $(fsr2_lib)

# SDL2 with a MooGL backend in place of NSOpenGL (submodules/SDL2, branch mgl),
# installed into build/sdl2/install for its headers and CMake package
sdl2_dir := submodules/SDL2
sdl2_install := $(build_dir)/sdl2/install
sdl2_lib := $(build_dir)/libSDL2-2.0.0.dylib

$(sdl2_lib): $(mgl_lib) $(wildcard $(sdl2_dir)/src/video/cocoa/*.m) $(sdl2_dir)/CMakeLists.txt
	cmake -S $(sdl2_dir) -B $(build_dir)/sdl2 -Wno-dev -Wno-deprecated -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_C_FLAGS=-w -DCMAKE_OBJC_FLAGS=-w \
		-DSDL_STATIC=OFF -DSDL_TEST=OFF -DCMAKE_SKIP_BUILD_RPATH=ON \
		-DMOOGL_LIBRARY=$(abspath $(mgl_lib)) -DCMAKE_INSTALL_PREFIX=$(abspath $(sdl2_install))
	cmake --build $(build_dir)/sdl2 --target install
	cp $(sdl2_install)/lib/libSDL2-2.0.0.dylib $@

sdl2: $(sdl2_lib)

# specific rules

lib: $(mgl_lib) $(mgl_es_lib) $(build_dir)/libglfw.dylib $(fsr2_lib) $(sdl2_lib)

toolchain: $(mgl_toolchain_lib)

# --- automated test suite ---
test_dir := tests
gears_dir := $(test_dir)/gears
test_build_dir := $(build_dir)/tests
test_srcs := $(wildcard $(test_dir)/*.c) $(wildcard $(test_dir)/unit/*.c) $(wildcard $(test_dir)/gpu/*.c)
# gears_common is shared with the suite; the two demos have their own main()
test_srcs += $(gears_dir)/gears_common.c
test_objs := $(patsubst $(test_dir)/%.c,$(test_build_dir)/%.o,$(test_srcs))
test_exe  := $(build_dir)/mgl_tests
deps += $(test_objs:.o=.d)

TEST_CFLAGS := -Wall -g -O1 -arch $(shell uname -m) -std=c11 \
  -Iinclude -Iinclude/GL -I$(test_dir) -I$(gears_dir) -DMGL_GL_CORE $(if $(MGL_NO_COMPAT),-DMGL_NO_COMPAT_PROFILE) \
  -I$(glslang_include_path) -I./submodules/SPIRV-Cross
ifneq ($(SDK_ROOT),)
TEST_CFLAGS += -isysroot $(SDK_ROOT)
endif

$(test_build_dir)/%.o: $(test_dir)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD $(TEST_CFLAGS) -c $< -o $@

$(test_build_dir)/gpu/test_fsr2.o: TEST_CFLAGS += -DFFX_GCC -I$(fsr2_api_dir)

$(test_exe): $(test_objs) $(mgl_lib) $(fsr2_lib)
	@mkdir -p $(dir $@)
	$(CC) -arch $(shell uname -m) -o $@ $(test_objs) \
	  -L$(build_dir) -lmoogl -lffx_fsr2_api -Wl,-rpath,@executable_path -Wl,-rpath,$(CURDIR)/$(build_dir) \
	  -framework Foundation -framework Metal -framework Cocoa -framework QuartzCore

# --- gears demos: two ports of the classic gears, both runnable as tests ---
gears_build_dir := $(build_dir)/gears
gears_common_obj := $(gears_build_dir)/gears_common.o
gears_compat_exe := $(build_dir)/gears_compat
gears_gl46_exe   := $(build_dir)/gears_gl46
deps += $(gears_common_obj:.o=.d)

GEARS_CFLAGS := $(TEST_CFLAGS) -I$(gears_dir) -I./external/glfw/include
# Link GLFW statically. As a dylib it is resolved by leaf name, so a
# DYLD_LIBRARY_PATH or a Homebrew libglfw silently replaces the MGL-aware
# build with a stock one that has no MGL backend.
glfw_static := external/glfw/build/src/libglfw3.a
GEARS_LIBS := $(glfw_static) -L$(build_dir) -lmoogl -Wl,-rpath,@executable_path -Wl,-rpath,$(CURDIR)/$(build_dir) \
  -framework Cocoa -framework Foundation -framework Metal -framework QuartzCore -framework IOKit

$(gears_build_dir)/%.o: $(gears_dir)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD $(GEARS_CFLAGS) -c $< -o $@

$(gears_compat_exe): $(gears_build_dir)/gears_compat.o $(mgl_lib) $(build_dir)/libglfw.dylib
	$(CC) -arch $(shell uname -m) -o $@ $< $(GEARS_LIBS)

$(gears_gl46_exe): $(gears_build_dir)/gears_gl46.o $(gears_common_obj) $(mgl_lib) $(build_dir)/libglfw.dylib
	$(CC) -arch $(shell uname -m) -o $@ $(gears_build_dir)/gears_gl46.o $(gears_common_obj) $(GEARS_LIBS)

gears: $(gears_compat_exe) $(gears_gl46_exe)

# Needs a logged-in session: these open real windows, which the suite does not
window_libs := -L$(build_dir) -lmoogl -Wl,-rpath,$(CURDIR)/$(build_dir)

$(build_dir)/test_window: tests/window/test_window.c $(mgl_lib) $(build_dir)/libglfw.dylib
	$(CC) -o $@ $< -Iinclude -Iexternal/glfw/include -L$(build_dir) -lglfw $(window_libs)

$(build_dir)/triangle: examples/triangle/main.c $(mgl_lib) $(build_dir)/libglfw.dylib
	$(CC) -o $@ $< -Iinclude -Iexternal/glfw/include -L$(build_dir) -lglfw $(window_libs)

$(build_dir)/sdl2_triangle: examples/sdl2_triangle/main.c $(mgl_lib) $(sdl2_lib)
	$(CC) -o $@ $< -Iinclude -I$(sdl2_install)/include -L$(build_dir) -lSDL2-2.0.0 $(window_libs)

window-test: $(build_dir)/test_window $(build_dir)/triangle $(build_dir)/sdl2_triangle
	$(build_dir)/test_window
	$(build_dir)/triangle --frames 60
	$(build_dir)/sdl2_triangle --frames 60

gears-test: gears
	@echo "== straight port (fixed function) =="
	@$(gears_compat_exe) --check
	@echo ""
	@echo "== OpenGL 4.6 port =="
	@$(gears_gl46_exe) --check --frames 4 --size 256 256 --out $(build_dir)/gears_gl46.tga

probe_srcs := $(wildcard $(test_dir)/probes/*.c)
probe_exes := $(patsubst $(test_dir)/probes/%.c,$(build_dir)/%,$(probe_srcs))

$(build_dir)/%: $(test_dir)/probes/%.c $(mgl_lib)
	$(CC) $(CFLAGS) -o $@ $< -L$(build_dir) -lmoogl -Wl,-rpath,$(abspath $(build_dir))

# Roadmap finish criteria you can run. A phase is done when its gates are clear.
probe: $(probe_exes)
	@for p in $(probe_exes); do echo; $$p || true; done

tests: $(test_exe)

test: $(test_exe)
	$(test_exe)

# --- conformance ---------------------------------------------------------
# glcts drops TestResults.qpa in whatever directory it is run from, so the
# log path is passed explicitly to keep it out of the repo root.
# Override the case filter: make cts CTS_CASE='KHR-GL46.clear_tex_image.*'
#
# The surface size is not optional. Leave it off and deqp sizes the FBO to
# GL_MAX_RENDERBUFFER_SIZE squared -- a 16384x16384, one gigabyte target that
# no conformance run uses. Tests that allocate a few surfaces that big then
# die of bad_alloc and take the rest of their group with them. 64x64 is what
# the Khronos runner uses for GL 4.6, so it is what we measure against.
cts_dir := tests/results
cts_bin := submodules/VK-GL-CTS/build/external/openglcts/modules/glcts
CTS_CASE ?= KHR-GL46.*
CTS_W ?= 64
CTS_H ?= 64

cts: $(mgl_lib)
	@mkdir -p $(cts_dir)
	MGL_LIBRARY_PATH=$(abspath $(mgl_lib)) caffeinate -dimsu $(cts_bin) \
	  --deqp-case='$(CTS_CASE)' \
	  --deqp-log-filename=$(abspath $(cts_dir))/TestResults.qpa \
	  --deqp-surface-type=fbo \
	  --deqp-surface-width=$(CTS_W) --deqp-surface-height=$(CTS_H) \
	  --deqp-watchdog=disable

dbg: $(test_exe)
	lldb -o run $(test_exe)


# generic rules

#
# core build
#
$(build_core_dir)/%.o: %.c | $(generated)
	@mkdir -p $(dir $@)
	$(CC) -MMD $(CFLAGS_GL_CORE) -c $< -o $@

#-std=gnu17 
$(build_core_dir)/%.o: %.cpp | $(generated)
	@mkdir -p $(dir $@)
	$(CXX) -MMD $(CXXFLAGS_GL_CORE) -c $< -o $@

#-std=c++14
$(build_core_dir)/arc/%.o: %.m | $(generated)
	@mkdir -p $(dir $@)
	clang -fobjc-arc -fmodules -MMD $(CFLAGS_GL_CORE) \
		-c $< -o $@

$(build_core_dir)/%.o: %.m | $(generated)
	@mkdir -p $(dir $@)
	clang -fmodules -MMD $(CFLAGS_GL_CORE) -c $< -o $@


#
# es build
#
$(build_es_dir)/%.o: %.c | $(generated)
	@mkdir -p $(dir $@)
	$(CC) -MMD $(CFLAGS_GL_ES) -c $< -o $@

#-std=gnu17
$(build_es_dir)/%.o: %.cpp | $(generated)
	@mkdir -p $(dir $@)
	$(CXX) -MMD $(CXXFLAGS_GL_ES) -c $< -o $@

#-std=c++14
$(build_es_dir)/arc/%.o: %.m | $(generated)
	@mkdir -p $(dir $@)
	clang -fobjc-arc -fmodules -MMD $(CFLAGS_GL_ES) \
		-c $< -o $@

$(build_dir)/%.o: %.m | $(generated)
	@mkdir -p $(dir $@)
	clang -fmodules -MMD $(CXXFLAGS_GL_ES) -c $< -o $@




# GLFW-specific build rules with simplified flat directory structure
$(GLFW_BUILD_DIR)/%.o: $(GLFW_SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -MMD $(CFLAGS) -c $< -o $@

$(GLFW_BUILD_DIR)/%.o: $(GLFW_SRC_DIR)/%.m
	@mkdir -p $(dir $@)
	clang -fno-objc-arc -fmodules -MMD $(CFLAGS) -c $< -o $@

# everything in the build folder except the CTS results kept in cts-work
clean:
	if [ -d $(build_dir) ]; then find $(build_dir) -mindepth 1 -maxdepth 1 ! -name cts-work -exec rm -rf {} +; fi
	rm -f $(generated)
	rm -f libmoogl.dylib
	rm -f libmoogles.dylib
	rm -f libglfw.dylib

.PHONY: default test tests cts gears gears-test window-test dbg lib fsr2 sdl2 clean generate FORCE

-include $(deps)
