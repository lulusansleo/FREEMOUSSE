#include "analysis/analyzer.hpp"
#include "analysis/fft.hpp"
#include "analysis/beat_detector.hpp"
#include <cmath>
#include <thread>

namespace dv {

struct Analyzer::Impl {
    FFT          fft;
    BeatDetector beat;
    // Accumulation buffer: FFT wants kFFTSize samples but ring delivers
    // kCaptureFrames at a time — we stitch them together here.
    std::array<float, kFFTSize> accum{};
    int accumFill{0};
};

Analyzer::Analyzer(RingBuffer& ring, SharedAudioState& state)
    : m_ring(ring), m_state(state), m_impl(std::make_unique<Impl>()) {}

Analyzer::~Analyzer() { stop(); }

void Analyzer::start()
{
    m_running.store(true);
    m_thread = std::thread(&Analyzer::loop, this);
}

void Analyzer::stop()
{
    m_running.store(false);
    if (m_thread.joinable()) m_thread.join();
}

void Analyzer::loop()
{
    SampleChunk chunk;
    AudioState  out;
    auto& d = *m_impl;

    while (m_running.load()) {
        SampleChunk* front = m_ring.front();
        if (!front) { std::this_thread::yield(); continue; }
        chunk = *front;
        m_ring.pop();

        // RMS
        float sum = 0.f;
        for (float s : chunk) sum += s * s;
        out.rms = std::sqrt(sum / chunk.size());

        // Beat / BPM
        auto [bpm, onBeat] = d.beat.process(chunk);
        out.bpm    = bpm;
        out.onBeat = onBeat;

        // Accumulate into FFT window
        for (float s : chunk) {
            d.accum[d.accumFill++] = s;
            if (d.accumFill == kFFTSize) {
                d.fft.process(d.accum, out.spectrum);
                d.accumFill = 0;
            }
        }

        m_state.write(out);
    }
}

} // namespace dv
