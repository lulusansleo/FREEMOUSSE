#pragma once
#include "core/audio_state.hpp"
#include <memory>
#include <span>

namespace dv {

// RAII wrapper around an FFTW3 single-precision real→complex plan.
// Create once at startup (planning is slow), call process() every frame.
class FFT {
public:
    FFT();
    ~FFT();
    // Applies Hann window internally; writes magnitudes to out[].
    void process(std::span<const float>            in,
                 std::array<float, kSpectrumBins>& out);
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace dv
