## MooGL 0.2.0

### Added
- Shared contexts. Contexts created to share with another one now have the same
  textures, buffers, renderbuffers, samplers, programs, shaders and fences. Each
  context still has its own vertex arrays, framebuffers and transform feedback
  objects, as OpenGL requires. Before, asking for a shared context failed.
  - Works with GLFW's `share` window argument and SDL2's
    `SDL_GL_SHARE_WITH_CURRENT_CONTEXT`, and from C with `createGLMContextShared`.
  - Each thread has its own current context, so contexts on different threads can
    load and draw at the same time.
  - An object deleted in one context stays alive while another context still uses
    it, and is freed once none does.
  - Fences work across contexts: `glWaitSync` makes one context's GPU work wait for
    another's, and `glClientWaitSync` honours its timeout.
- Dear ImGui in the SDK: `libimgui.a` (docking branch, with its GLFW, SDL2 and
  OpenGL 3 backends) and a `MooGL::imgui` CMake target. Stock ImGui loads Apple's
  OpenGL on a Mac; this build draws with MooGL. Two examples, one for GLFW and one
  for SDL2, open ImGui's demo with multi-viewports, so windows dragged out of the
  main one get their own shared context.

## MooGL 0.1.1

### Faster
- SuperTuxKart is about 3× faster (52 → ~160 fps on its built-in benchmark).
  Normalized 10:10:10:2 vertex attributes are now read by Metal directly instead
  of being converted on the CPU for every draw.
- IDKEngine is about 14% faster (183 → 209 fps at 1600×800).
- Buffer updates no longer stall waiting for the GPU. A partial update of a buffer
  the GPU is still using is copied on the GPU in order, and read-backs only wait
  when the GPU wrote the buffer.
- Deleting buffers, textures and other objects no longer interrupts rendering.
- One render pass per frame instead of three: `glClear` becomes the next pass's
  clear, and swap no longer starts an empty pass or waits for the next drawable early.
- Setting state to the value it already has (`glEnable`, depth, cull and so on)
  costs nothing, and viewport, scissor, depth and cull changes no longer rebuild
  the render pipeline.
- Setting a sampler or uniform-block binding no longer rebuilds the program's
  Metal shaders.
- With primitive restart on, triangle and line lists are only reworked on the CPU
  when they actually contain the restart index.
- Vertex formats Metal can't read are converted once per buffer change instead of
  on every draw.

### Fixed
- Tightly packed 10:10:10:2 vertex attributes (stride 0) were read 16 bytes apart
  instead of 4.
- Drawing with no program bound could crash.

### Added
- `MGLget` can read driver counters (`MGL_STAT_*` in `MGLContext.h`): pipelines
  built, GPU waits, render passes, buffer rebinds, CPU index expansions, program
  rebuilds and attribute conversions.