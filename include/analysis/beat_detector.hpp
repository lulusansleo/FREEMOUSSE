#pragma once
#include <memory>
#include <span>

namespace dv {

struct BeatResult {
    float bpm{0.f};
    bool  onBeat{false};
};

// Wraps aubio_tempo_t. Feed mono float samples, get BPM + onset events.
class BeatDetector {
public:
    BeatDetector();
    ~BeatDetector();
    BeatResult process(std::span<const float> samples);
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace dv
