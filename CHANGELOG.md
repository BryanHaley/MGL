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