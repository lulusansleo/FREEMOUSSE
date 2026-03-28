# DJ Visualizer

Real-time audio visualizer for live music / DJ performance.  
**Stack:** C++20 · PortAudio (ASIO) · FFTW3 · aubio · SDL2 · OpenGL 3.3

## Quick start

```bash
./scripts/install_deps.sh   # install system libraries
./scripts/build.sh          # configure + compile
./build/dj_visualizer       # run
```

## Architecture

```
[RT Audio Thread]   PortAudio callback → SPSCQueue ring buffer
[Main Thread]       ring buffer → Analyzer (FFTW + aubio) → SharedAudioState
[Main Thread]       SDL2 event loop → read SharedAudioState → OpenGL draw
```

## First things to implement

1. `src/core/app.cpp` — stereo→mono mix + push to ring buffer in the audio callback
2. `src/render/renderer.cpp` — `drawFrame()`: build bar geometry from `spectrum[]`
3. Add beat-reactive effects using `s.onBeat` and `s.bpm` in `drawFrame()`

## Adding a new visual mode

1. Create `include/render/my_mode.hpp` + `src/render/my_mode.cpp`
2. Implement `void draw(const AudioState&)`
3. Call it from `Renderer::drawFrame()`

## Upgrading to 3D

- Add a depth buffer + perspective matrix to `Renderer`
- Write new shader pair in `shaders/3d/`
- The entire audio pipeline is unchanged
