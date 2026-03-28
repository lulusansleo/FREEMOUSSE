#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# create_issues.sh — seed GitHub issues for all code TODOs
# Usage: ./scripts/create_issues.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

info()    { echo "  ✓ $*"; }
skip()    { echo "  ↷ $*"; }
section() { echo ""; echo "▸ $*"; }

# ── Detect repo ───────────────────────────────────────────────────────────
REPO_SLUG=$(gh repo view --json nameWithOwner --jq '.nameWithOwner')
info "Repo: $REPO_SLUG"

# ── 1. Labels ─────────────────────────────────────────────────────────────
section "Creating labels"

create_label() {
  local name="$1" color="$2" desc="$3"
  if gh label list --repo "$REPO_SLUG" | grep -q "^$name"; then
    skip "label '$name' already exists"
  else
    gh label create "$name" --color "$color" --description "$desc" --repo "$REPO_SLUG"
    info "label: $name"
  fi
}

# Type
create_label "feat"     "7057ff" "New feature"
create_label "fix"      "d73a4a" "Bug fix"
create_label "perf"     "e4e669" "Performance improvement"
create_label "chore"    "cfd3d7" "Maintenance task"

# Domain
create_label "audio"    "f9a825" "Audio capture / RT thread"
create_label "analysis" "ab47bc" "FFT, beat detection, features"
create_label "render"   "26a69a" "OpenGL / SDL2 rendering"
create_label "core"     "546e7a" "App wiring, shared state"

# Priority
create_label "p0-blocker" "b60205" "Blocks a milestone"
create_label "p1-high"    "e99695" "Do soon"
create_label "p2-normal"  "f9d0c4" "Normal priority"

# ── 2. Milestones ─────────────────────────────────────────────────────────
section "Creating milestones"

create_milestone() {
  local title="$1" desc="$2"
  if gh api "repos/$REPO_SLUG/milestones" --jq '.[].title' | grep -qF "$title"; then
    skip "milestone '$title' already exists"
  else
    gh api --method POST "repos/$REPO_SLUG/milestones" \
      --input - << EOF
{"title": "$title", "description": "$desc"}
EOF
    info "milestone: $title"
  fi
}

create_milestone "v0.1 — First pixels" \
  "Audio flows into the ring buffer, FFT runs, spectrum bars appear on screen"

create_milestone "v0.2 — Beat reactive" \
  "Beat flash effects, log-scale spectrum, waveform oscilloscope"



# ── 3. Issues ─────────────────────────────────────────────────────────────
section "Creating issues"

create_issue() {
  local title="$1" body="$2" labels="$3" milestone="$4"
  if gh issue list --repo "$REPO_SLUG" --state all --search "\"$title\"" \
       --json title --jq '.[].title' | grep -qF "$title"; then
    skip "issue already exists: $title"
    return
  fi

  gh issue create \
    --repo "$REPO_SLUG" \
    --title "$title" \
    --body "$body" \
    --label "$labels" \
    --milestone "$milestone"
  info "issue: $title"
}

# ── v0.1 — src/core/app.cpp TODO ──────────────────────────────────────────

create_issue \
  "feat(audio): stereo→mono mix and ring buffer push in audio callback" \
  "## Location
\`src/core/app.cpp\` — the \`AudioCapture\` lambda, marked \`// TODO\`

## What to implement
The PortAudio callback receives interleaved stereo float32 samples (\`kChannels = 2\`).
It needs to:
1. Average L+R pairs into a mono \`SampleChunk\`
2. Push the chunk into \`s_ring\` via \`try_push\` (non-blocking — drop if full)

\`\`\`cpp
m_capture = std::make_unique<AudioCapture>([](const float* buf, std::size_t n)
{
    SampleChunk chunk;
    for (std::size_t i = 0; i < n; ++i)
        chunk[i] = (buf[i * 2] + buf[i * 2 + 1]) * 0.5f;
    s_ring.try_push(chunk);
});
\`\`\`

## Constraints (RT thread rules — must not be broken)
- [ ] No \`malloc\` / \`new\` / \`delete\`
- [ ] No mutexes or locks
- [ ] No file I/O or logging
- [ ] No blocking calls of any kind

## Acceptance criteria
- [ ] Bars visibly respond to audio after this is done
- [ ] No PortAudio underrun warnings in stderr at 512-frame buffer size
- [ ] Compiles clean with \`-Wall -Wextra\`" \
  "feat,audio,p0-blocker" \
  "v0.1 — First pixels"

# ── v0.1 — src/render/renderer.cpp TODO ───────────────────────────────────

create_issue \
  "feat(render): build bar geometry from spectrum[] in drawFrame()" \
  "## Location
\`src/render/renderer.cpp\` — \`Renderer::drawFrame()\`, marked \`// TODO\`

## What to implement
Read \`AudioState::spectrum[kSpectrumBins]\` and draw one quad per bin as a vertical bar.

### Geometry layout
Each bin \`i\` maps to one rectangle made of 2 triangles (6 vertices):
\`\`\`
x_left  = (float(i)       / kSpectrumBins) * 2.f - 1.f   // clip space [-1..1]
x_right = (float(i + 1)   / kSpectrumBins) * 2.f - 1.f
height  = s.spectrum[i]                                    // already [0..1]
\`\`\`

### VBO upload
\`\`\`cpp
std::vector<float> verts;
verts.reserve(kSpectrumBins * 6 * 2);
for (int i = 0; i < kSpectrumBins; ++i) {
    float xl = (float(i)     / kSpectrumBins) * 2.f - 1.f;
    float xr = (float(i + 1) / kSpectrumBins) * 2.f - 1.f;
    float h  = s.spectrum[i] * 2.f - 1.f; // map [0..1] → [-1..1]
    // triangle 1
    verts.insert(verts.end(), { xl, -1.f,  xr, -1.f,  xl, h });
    // triangle 2
    verts.insert(verts.end(), { xr, -1.f,  xr,  h,    xl, h });
}
glBufferSubData(GL_ARRAY_BUFFER, 0,
                verts.size() * sizeof(float), verts.data());
\`\`\`

Then uncomment:
\`\`\`cpp
glDrawArrays(GL_TRIANGLES, 0, kSpectrumBins * 6);
\`\`\`

## Acceptance criteria
- [ ] Bars are visible and span the full window width
- [ ] Bar heights visibly react to music in real time
- [ ] Frame rate stays above 60 fps at 1280×720
- [ ] No GL errors reported (add a debug check with \`glGetError()\` in Debug builds)" \
  "feat,render,p0-blocker" \
  "v0.1 — First pixels"

# ── v0.2 — follow-on work that unblocks good visuals ─────────────────────

create_issue \
  "feat(analysis): log-scale the spectrum before writing to AudioState" \
  "## Location
\`src/analysis/analyzer.cpp\` — \`Analyzer::loop()\`, after the FFT call

## Why
Raw FFT magnitudes are linear. Bass frequencies are orders of magnitude louder than treble,
so the first few bars dominate and the rest are invisible. Human hearing is logarithmic —
dB scaling fixes this.

## What to implement
After \`d.fft.process(d.accum, out.spectrum)\`, apply:
\`\`\`cpp
constexpr float kDbFloor = -80.f;
for (float& v : out.spectrum) {
    float db = 20.f * std::log10(v + 1e-9f);
    v = std::clamp((db - kDbFloor) / -kDbFloor, 0.f, 1.f);
}
\`\`\`

## Acceptance criteria
- [ ] Bass, mid, and treble bars are all visually active at typical DJ volumes
- [ ] No bins are permanently saturated at 1.0 during normal playback
- [ ] \`kDbFloor\` extracted as a named constant (future: move to config file)" \
  "feat,analysis,p1-high" \
  "v0.2 — Beat reactive"

create_issue \
  "feat(render): beat-reactive flash effect using AudioState::onBeat" \
  "## Location
\`src/render/renderer.cpp\` — \`Renderer::drawFrame()\`, after the bar draw

## What to implement
When \`s.onBeat == true\`, trigger a one-frame visual reaction. Suggested approach —
additive brightness overlay:
\`\`\`cpp
if (s.onBeat) {
    // Draw a full-screen quad with additive blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // draw white quad at ~0.3 alpha
    glDisable(GL_BLEND);
}
\`\`\`
Or simpler: multiply all bar heights by 1.4 for the beat frame only.

## Acceptance criteria
- [ ] Flash is clearly visible at BPMs 90–145
- [ ] Effect lasts no more than one frame (no multi-frame decay needed yet)
- [ ] Does not cause a measurable frame time spike (verify with \`SDL_GetTicks\`)" \
  "feat,render,p1-high" \
  "v0.2 — Beat reactive"

create_issue \
  "fix(audio): handle Pa_GetDefaultInputDevice() returning paNoDevice" \
  "## Location
\`src/audio/audio_capture.cpp\` — \`AudioCapture::start()\`

## Problem
If no audio input device is available, \`Pa_GetDefaultInputDevice()\` returns \`paNoDevice\` (-1).
The next line calls \`Pa_GetDeviceInfo(-1)\`, which is undefined behaviour and typically crashes.

## Fix
\`\`\`cpp
p.device = Pa_GetDefaultInputDevice();
if (p.device == paNoDevice)
    throw std::runtime_error(
        \"No audio input device found — plug in your audio interface and retry.\");
\`\`\`

Optionally enumerate available devices before throwing:
\`\`\`cpp
for (int i = 0; i < Pa_GetDeviceCount(); ++i) {
    const auto* info = Pa_GetDeviceInfo(i);
    if (info->maxInputChannels > 0)
        fprintf(stderr, \"  [%d] %s\\n\", i, info->name);
}
\`\`\`

## Acceptance criteria
- [ ] Clean error message printed when no device is found
- [ ] No crash or UB on machines without an audio interface
- [ ] Works correctly when a USB audio interface is hot-plugged after launch (future: not required now)" \
  "fix,audio,p1-high" \
  "v0.2 — Beat reactive"

# ── Done ──────────────────────────────────────────────────────────────────
echo ""
echo "┌──────────────────────────────────────────────────────────┐"
echo "│  Done! Issues, labels, and milestones created.           │"
echo "│                                                          │"
printf "│  %-56s│\n" "  https://github.com/$REPO_SLUG/issues"
echo "└──────────────────────────────────────────────────────────┘"
