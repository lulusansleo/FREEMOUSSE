#include "audio/audio_capture.hpp"

#include "core/audio_state.hpp"

#include <portaudio.h>
#include <stdexcept>

namespace dv {

struct AudioCapture::Impl {
    PaStream* stream{nullptr};
    AudioCallback cb;
};

// This runs on the OS audio thread at real-time priority.
int AudioCapture::paCallback(const void* input, void* /*output*/, unsigned long frameCount,
                             const PaStreamCallbackTimeInfo* /*timeInfo*/,
                             PaStreamCallbackFlags /*flags*/, void* userData) {
    auto* self = static_cast<AudioCapture*>(userData);
    if (input && self->m_impl && self->m_impl->cb)
        self->m_impl->cb(static_cast<const float*>(input), frameCount);
    return paContinue;
}

AudioCapture::AudioCapture(AudioCallback cb) : m_impl(new Impl{nullptr, std::move(cb)}) {
    if (Pa_Initialize() != paNoError)
        throw std::runtime_error("PortAudio: initialization failed");
}

AudioCapture::~AudioCapture() {
    stop();
    Pa_Terminate();
    delete m_impl;
}

void AudioCapture::start() {
    PaStreamParameters p{};
    p.device = Pa_GetDefaultInputDevice();
    if (p.device == paNoDevice)
        throw std::runtime_error("PortAudio: no default input device available");

    const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(p.device);
    if (!devInfo)
        throw std::runtime_error("PortAudio: failed to query input device info");

    p.channelCount = kChannels;
    p.sampleFormat = paFloat32;
    p.suggestedLatency = devInfo->defaultLowInputLatency;

    PaError err = Pa_OpenStream(&m_impl->stream, &p, nullptr, kSampleRate, kCaptureFrames,
                                paClipOff, &AudioCapture::paCallback, this);
    if (err != paNoError)
        throw std::runtime_error(Pa_GetErrorText(err));

    Pa_StartStream(m_impl->stream);
    m_running.store(true);
}

void AudioCapture::stop() {
    if (!m_running.exchange(false))
        return;
    Pa_StopStream(m_impl->stream);
    Pa_CloseStream(m_impl->stream);
    m_impl->stream = nullptr;
}

} // namespace dv
