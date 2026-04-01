#include "core/app.hpp"

#include "analysis/analyzer.hpp"
#include "audio/audio_capture.hpp"
#include "audio/ring_buffer.hpp"
#include "core/audio_state.hpp"
#include "render/renderer.hpp"

#include <SDL.h>
#include <algorithm>
#include <aubio/aubio.h>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <portaudio.h>
#include <stdexcept>

namespace dv {

// Module-level singletons owned by App (one per process)
static RingBuffer s_ring{8};
static RingBuffer s_playbackRing{64};
static SharedAudioState s_state;

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static int selectOutputDevice(const std::string& query) {
    if (query.empty())
        return Pa_GetDefaultOutputDevice();

    const std::string needle = toLower(query);
    const int count = Pa_GetDeviceCount();
    if (count < 0)
        return paNoDevice;

    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info || info->maxOutputChannels <= 0)
            continue;
        if (toLower(info->name).find(needle) != std::string::npos)
            return i;
    }
    return paNoDevice;
}

static bool playFileWithSDLAudio(std::atomic<bool>& running, std::atomic<bool>& sourceDone) {
    if (std::getenv("PULSE_SERVER") && !std::getenv("SDL_AUDIODRIVER")) {
        // In WSLg, force SDL to prefer PulseAudio if available.
        setenv("SDL_AUDIODRIVER", "pulseaudio", 0);
    }

    if (SDL_AudioInit(nullptr) != 0) {
        std::cerr << "Warning: SDL audio init failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_AudioSpec want{};
    SDL_AudioSpec have{};
    want.freq = kSampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = static_cast<Uint16>(kCaptureFrames);
    want.callback = nullptr;

    const SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev == 0) {
        std::cerr << "Warning: SDL failed to open output device: " << SDL_GetError() << "\n";
        SDL_AudioQuit();
        return false;
    }

    SDL_PauseAudioDevice(dev, 0);
    std::array<float, kCaptureFrames * 2> stereo{};

    while (running.load() || !sourceDone.load() || !s_playbackRing.empty()) {
        SampleChunk* front = s_playbackRing.front();
        if (!front) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const SampleChunk chunk = *front;
        s_playbackRing.pop();

        for (std::size_t i = 0; i < chunk.size(); ++i) {
            const float s = chunk[i];
            stereo[i * 2] = s;
            stereo[i * 2 + 1] = s;
        }

        const Uint32 bytes = static_cast<Uint32>(kCaptureFrames * 2 * sizeof(float));
        if (SDL_QueueAudio(dev, stereo.data(), bytes) != 0) {
            std::cerr << "Warning: SDL_QueueAudio failed: " << SDL_GetError() << "\n";
            break;
        }

        // Keep queue bounded to roughly 0.5s to avoid excessive latency.
        const Uint32 maxQueued = static_cast<Uint32>(kSampleRate * 2 * sizeof(float) / 2);
        while (running.load() && SDL_GetQueuedAudioSize(dev) > maxQueued) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    while (running.load() && SDL_GetQueuedAudioSize(dev) > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    SDL_CloseAudioDevice(dev);
    SDL_AudioQuit();
    return true;
}

App::App(std::string audioFilePath, bool playAudio, std::string outputDeviceQuery)
    : m_audioFilePath(std::move(audioFilePath)), m_outputDeviceQuery(std::move(outputDeviceQuery)),
      m_playAudio(playAudio) {
    if (m_audioFilePath.empty()) {
        m_capture = std::make_unique<AudioCapture>([this](const float* buf, std::size_t n) {
            // RT callback path: convert interleaved stereo to mono and enqueue chunks.
            pushInterleavedStereo(buf, n);
        });
    }

    m_analyzer = std::make_unique<Analyzer>(s_ring, s_state);
    m_renderer = std::make_unique<Renderer>(s_state);
}

App::~App() {
    shutdownSubsystems();
}

int App::run() {
    try {
        m_fileSourceDone.store(false);

        m_analyzer->start();

        if (!m_audioFilePath.empty()) {
            startFileSource();
            if (m_playAudio)
                startFilePlayback();
            else if (!m_outputDeviceQuery.empty())
                std::cerr << "Warning: --output-device is ignored unless --play-audio is set\n";
        } else {
            if (m_playAudio) {
                std::cerr << "Warning: --play-audio is only supported with --audio-file\n";
            }
            if (!m_outputDeviceQuery.empty())
                std::cerr << "Warning: --output-device is only supported with --audio-file "
                             "--play-audio\n";
            m_capture->start();
        }

        m_renderer->run(m_running); // blocks until window closed / ESC
        shutdownSubsystems();
        if (m_fileSourceFailed.load())
            return 1;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        shutdownSubsystems();
        return 1;
    }
}

void App::dispatchFullChunk() noexcept {
    // File-mode producer can block to preserve timing and avoid drops.
    if (!m_audioFilePath.empty()) {
        s_ring.push(m_pendingChunk);
        if (m_playAudio)
            s_playbackRing.push(m_pendingChunk);
    } else {
        // Live input callback path should stay non-blocking.
        (void)s_ring.try_push(m_pendingChunk);
    }
}

void App::pushMonoSample(float sample) noexcept {
    m_pendingChunk[m_pendingFill++] = sample;
    if (m_pendingFill == kCaptureFrames) {
        dispatchFullChunk();
        m_pendingFill = 0;
    }
}

void App::pushInterleavedStereo(const float* buf, std::size_t frames) noexcept {
    for (std::size_t i = 0; i < frames; ++i) {
        const float l = buf[i * 2];
        const float r = buf[i * 2 + 1];
        pushMonoSample(0.5f * (l + r));
    }
}

void App::startFileSource() {
    m_fileThread = std::thread(&App::fileSourceLoop, this);
}

void App::stopFileSource() {
    if (m_fileThread.joinable())
        m_fileThread.join();
}

void App::startFilePlayback() {
    m_filePlaybackThread = std::thread(&App::filePlaybackLoop, this);
}

void App::stopFilePlayback() {
    if (m_filePlaybackThread.joinable())
        m_filePlaybackThread.join();
}

void App::fileSourceLoop() {
    aubio_source_t* src = new_aubio_source(m_audioFilePath.c_str(), kSampleRate, kCaptureFrames);
    if (!src) {
        std::cerr << "Fatal error: failed to open audio file: " << m_audioFilePath << "\n";
        m_fileSourceFailed.store(true);
        m_running.store(false);
        return;
    }

    fvec_t* input = new_fvec(kCaptureFrames);
    if (!input) {
        del_aubio_source(src);
        std::cerr << "Fatal error: failed to allocate aubio input buffer\n";
        m_fileSourceFailed.store(true);
        m_running.store(false);
        return;
    }

    while (m_running.load()) {
        uint_t read = 0;
        aubio_source_do(src, input, &read);
        if (read == 0)
            break;

        for (uint_t i = 0; i < read; ++i)
            pushMonoSample(input->data[i]);

        // Mimic real-time playback speed so visual playback duration
        // matches the audio file duration.
        const auto chunkDuration =
            std::chrono::microseconds(static_cast<long long>(read) * 1000000LL / kSampleRate);
        std::this_thread::sleep_for(chunkDuration);

        if (read < static_cast<uint_t>(kCaptureFrames))
            break;
    }

    // Flush a partial tail chunk so playback/analysis consume the final audio slice.
    if (m_pendingFill > 0) {
        for (std::size_t i = m_pendingFill; i < kCaptureFrames; ++i)
            m_pendingChunk[i] = 0.0f;
        dispatchFullChunk();
        m_pendingFill = 0;
    }

    del_fvec(input);
    del_aubio_source(src);

    m_fileSourceDone.store(true);

    // Without audible playback, EOF should still close the app.
    if (!m_playAudio)
        m_running.store(false);
}

void App::filePlaybackLoop() {
    if (Pa_Initialize() != paNoError) {
        std::cerr << "Warning: PortAudio output initialization failed; trying SDL audio backend\n";
        if (!playFileWithSDLAudio(m_running, m_fileSourceDone)) {
            std::cerr
                << "Warning: no usable audio playback backend; continuing without audio output\n";
        }
        if (m_fileSourceDone.load())
            m_running.store(false);
        return;
    }

    PaStream* stream = nullptr;
    bool streamOpen = false;

    do {
        PaStreamParameters out{};
        out.device = selectOutputDevice(m_outputDeviceQuery);
        if (out.device == paNoDevice) {
            if (m_outputDeviceQuery.empty()) {
                std::cerr << "Warning: no default PortAudio output device available; trying SDL "
                             "audio backend\n";
            } else {
                std::cerr << "Warning: no PortAudio output device matched '" << m_outputDeviceQuery
                          << "'; trying SDL audio backend\n";
            }
            (void)playFileWithSDLAudio(m_running, m_fileSourceDone);
            break;
        }

        const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(out.device);
        if (!devInfo) {
            std::cerr
                << "Warning: failed to query output device info; continuing without audio output\n";
            break;
        }

    out.channelCount = 2;
    out.sampleFormat = paFloat32;
    // Use the device's high output latency to give the audio server (Pulse/PipeWire)
    // more headroom and reduce the chance of underflowing when the system is busy.
    out.suggestedLatency = devInfo->defaultHighOutputLatency;

        if (!m_outputDeviceQuery.empty()) {
            std::cerr << "Info: using output device '" << devInfo->name << "'\n";
        }

    // Let PortAudio choose an appropriate internal buffer size instead of forcing
    // a small fixed frames-per-buffer. This helps on PipeWire/PulseAudio where
    // host buffer sizes can differ from the application's chunk size.
    PaError err = Pa_OpenStream(&stream, nullptr, &out, kSampleRate,
                    paFramesPerBufferUnspecified, paClipOff, nullptr, nullptr);
        if (err != paNoError) {
            std::cerr << "Warning: failed to open PortAudio output stream: " << Pa_GetErrorText(err)
                      << "; trying SDL audio backend\n";
            (void)playFileWithSDLAudio(m_running, m_fileSourceDone);
            break;
        }
        streamOpen = true;

        err = Pa_StartStream(stream);
        if (err != paNoError) {
            std::cerr << "Warning: failed to start PortAudio output stream: "
                      << Pa_GetErrorText(err) << "; trying SDL audio backend\n";
            (void)playFileWithSDLAudio(m_running, m_fileSourceDone);
            break;
        }

        std::array<float, kCaptureFrames * 2> stereo{};

        while (m_running.load() || !m_fileSourceDone.load() || !s_playbackRing.empty()) {
            SampleChunk* front = s_playbackRing.front();
            if (!front) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            const SampleChunk chunk = *front;
            s_playbackRing.pop();

            for (std::size_t i = 0; i < chunk.size(); ++i) {
                const float s = chunk[i];
                stereo[i * 2] = s;
                stereo[i * 2 + 1] = s;
            }

            const PaError writeErr = Pa_WriteStream(stream, stereo.data(), kCaptureFrames);
            if (writeErr != paNoError) {
                const char* txt = Pa_GetErrorText(writeErr);
                std::string errText = txt ? txt : "(unknown)";
                std::cerr << "Warning: audio playback write failed: " << errText << "\n";

                // If this was an output underflow try a brief sleep and continue —
                // underflows can be transient when the sink is suspended/waking or
                // when the system is briefly busy. For other errors fall back.
                if (errText.find("underflow") != std::string::npos ||
                    errText.find("Underflow") != std::string::npos) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                break;
            }
        }
    } while (false);

    if (streamOpen) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    Pa_Terminate();

    if (m_fileSourceDone.load())
        m_running.store(false);
}

void App::shutdownSubsystems() {
    m_running.store(false);
    if (m_capture)
        m_capture->stop();
    stopFileSource();
    stopFilePlayback();
    if (m_analyzer)
        m_analyzer->stop();
}

} // namespace dv
