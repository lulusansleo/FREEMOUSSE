#include "analysis/fft.hpp"

#include <algorithm>
#include <cmath>
#include <fftw3.h>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace dv {

struct FFT::Impl {
    std::vector<float> in;
    fftwf_complex* out{nullptr};
    fftwf_plan plan{nullptr};
    std::vector<float> window; // pre-computed Hann coefficients
};

FFT::FFT() : m_impl(std::make_unique<Impl>()) {
    auto& d = *m_impl;
    d.in.resize(kFFTSize);
    d.out = static_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * kSpectrumBins));
    if (!d.out)
        throw std::runtime_error("FFTW3: output buffer allocation failed");

    // FFTW_MEASURE: expensive one-time planning for fastest execution
    d.plan = fftwf_plan_dft_r2c_1d(kFFTSize, d.in.data(), d.out, FFTW_MEASURE);
    if (!d.plan)
        throw std::runtime_error("FFTW3: plan creation failed");

    // Hann window
    d.window.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        d.window[i] = 0.5f * (1.f - std::cos(2.f * std::numbers::pi_v<float> * i / (kFFTSize - 1)));
}

FFT::~FFT() {
    if (m_impl->plan)
        fftwf_destroy_plan(m_impl->plan);
    if (m_impl->out)
        fftwf_free(m_impl->out);
}

void FFT::process(std::span<const float> in, std::array<float, kSpectrumBins>& out) {
    auto& d = *m_impl;
    const auto n = std::min(in.size(), static_cast<std::size_t>(kFFTSize));
    for (std::size_t i = 0; i < n; ++i)
        d.in[i] = in[i] * d.window[i];
    for (std::size_t i = n; i < static_cast<std::size_t>(kFFTSize); ++i)
        d.in[i] = 0.0f;

    fftwf_execute(d.plan);

    for (int i = 0; i < kSpectrumBins; ++i) {
        const float re = d.out[i][0], im = d.out[i][1];
        out[i] = std::sqrt(re * re + im * im) / kFFTSize;
    }
}

} // namespace dv
