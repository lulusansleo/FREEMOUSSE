#include "analysis/beat_detector.hpp"

#include "core/audio_state.hpp"

#include <aubio/aubio.h>

namespace dv {

struct BeatDetector::Impl {
    aubio_tempo_t* tempo{nullptr};
    fvec_t* input{nullptr};
    fvec_t* output{nullptr};
};

BeatDetector::BeatDetector() : m_impl(std::make_unique<Impl>()) {
    auto& d = *m_impl;
    d.input = new_fvec(kCaptureFrames);
    d.output = new_fvec(1);
    d.tempo = new_aubio_tempo("default", kFFTSize, kCaptureFrames, kSampleRate);
}

BeatDetector::~BeatDetector() {
    del_aubio_tempo(m_impl->tempo);
    del_fvec(m_impl->input);
    del_fvec(m_impl->output);
}

BeatResult BeatDetector::process(std::span<const float> samples) {
    auto& d = *m_impl;
    const auto n = std::min(samples.size(), static_cast<std::size_t>(kCaptureFrames));
    for (std::size_t i = 0; i < n; ++i)
        d.input->data[i] = samples[i];

    aubio_tempo_do(d.tempo, d.input, d.output);
    return {aubio_tempo_get_bpm(d.tempo), d.output->data[0] > 0.f};
}

} // namespace dv
