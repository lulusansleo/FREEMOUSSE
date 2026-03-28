# DJ Visualizer

Real-time audio visualizer for live music / DJ performance.  
**Stack:** C++20 · PortAudio (ASIO) · FFTW3 · aubio · SDL2 · OpenGL 3.3

## Quick start

```bash
./scripts/install_deps.sh   # install system libraries
./scripts/build.sh          # configure + compile
./build/dj_visualizer       # run
./build/dj_visualizer --audio-file /path/to/track.wav  # file-driven mode
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

## Development workflow

### First-time setup
```bash
./scripts/install_deps.sh    # system libs
./scripts/setup_github.sh    # create GitHub repo, push branches, set rules
```

### Daily flow
```bash
./scripts/new_feature.sh feat/waveform-oscilloscope
# ... write code ...
./scripts/format.sh          # auto-format before committing
git add -p
git commit -m "feat(render): add waveform oscilloscope"
git push
# open PR → dev on GitHub; CI runs automatically
```

### Commit message format (enforced by hook)
```
feat(render): add waveform oscilloscope
fix(audio): handle missing default device
perf(fft): cache plan across frames
refactor(core): extract SharedAudioState
```

### Releasing
```bash
./scripts/release.sh v0.1.0
# Merges dev → main, tags, triggers CD → macOS .app artifact
```

## Branch rules

| Branch | Protected | Requires | Merge strategy |
|--------|-----------|----------|----------------|
| `main` | ✅ | All CI + 1 approval | Squash from `dev` only |
| `dev`  | ✅ | Linux CI + format/tidy + 1 approval | Squash from `feat/*` / `fix/*` |
| `feat/*` `fix/*` `perf/*` | — | — | Use `./scripts/new_feature.sh` |
