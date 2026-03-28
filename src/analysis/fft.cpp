#include "analysis/fft.hpp"
#include <fftw3.h>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace dv {

struct FFT::Impl {
    std::vector<float>         in;
    std::vector<fftwf_complex> out;
    fftwf_plan                 plan;
    std::vector<float>         window; // pre-computed Hann coefficients
};

FFT::FFT() : m_impl(std::make_unique<Impl>())
{
    auto& d = *m_impl;
    d.in.resize(kFFTSize);
    d.out.resize(kSpectrumBins);

    // FFTW_MEASURE: expensive one-time planning for fastest execution
    d.plan = fftwf_plan_dft_r2c_1d(
        kFFTSize, d.in.data(), d.out.data(), FFTW_MEASURE);
    if (!d.plan) throw std::runtime_error("FFTW3: plan creation failed");

    // Hann window
    d.window.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
        d.window[i] = 0.5f * (1.f - std::cos(
            2.f * std::numbers::pi_v<float> * i / (kFFTSize - 1)));
}

FFT::~FFT() { fftwf_destroy_plan(m_impl->plan); }

void FFT::process(std::span<const float>            in,
                  std::array<float, kSpectrumBins>& out)
{
    auto& d = *m_impl;
    const auto n = std::min(in.size(), static_cast<std::size_t>(kFFTSize));
    for (std::size_t i = 0; i < n; ++i)
        d.in[i] = in[i] * d.window[i];

    fftwf_execute(d.plan);

    for (int i = 0; i < kSpectrumBins; ++i) {
        const float re = d.out[i][0], im = d.out[i][1];
        out[i] = std::sqrt(re * re + im * im) / kFFTSize;
    }
}

} // namespace dv
