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

static bool playFileWithSDLAudio(const std::string& audioFilePath, std::atomic<bool>& running) {
    if (std::getenv("PULSE_SERVER") && !std::getenv("SDL_AUDIODRIVER")) {
        // In WSLg, force SDL to prefer PulseAudio if available.
        setenv("SDL_AUDIODRIVER", "pulseaudio", 0);
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
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
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    aubio_source_t* src = new_aubio_source(audioFilePath.c_str(), kSampleRate, kCaptureFrames);
    if (!src) {
        std::cerr << "Warning: failed to open audio file for SDL playback: " << audioFilePath
                  << "\n";
        SDL_CloseAudioDevice(dev);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    fvec_t* input = new_fvec(kCaptureFrames);
    if (!input) {
        std::cerr << "Warning: failed to allocate SDL playback buffer\n";
        del_aubio_source(src);
        SDL_CloseAudioDevice(dev);
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_PauseAudioDevice(dev, 0);
    std::array<float, kCaptureFrames * 2> stereo{};

    while (running.load()) {
        uint_t read = 0;
        aubio_source_do(src, input, &read);
        if (read == 0)
            break;

        for (uint_t i = 0; i < read; ++i) {
            const float s = input->data[i];
            stereo[i * 2] = s;
            stereo[i * 2 + 1] = s;
        }
        for (uint_t i = read; i < static_cast<uint_t>(kCaptureFrames); ++i) {
            stereo[i * 2] = 0.0f;
            stereo[i * 2 + 1] = 0.0f;
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

        if (read < static_cast<uint_t>(kCaptureFrames))
            break;
    }

    while (running.load() && SDL_GetQueuedAudioSize(dev) > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    del_fvec(input);
    del_aubio_source(src);
    SDL_CloseAudioDevice(dev);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
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

void App::pushMonoSample(float sample) noexcept {
    m_pendingChunk[m_pendingFill++] = sample;
    if (m_pendingFill == kCaptureFrames) {
        // Avoid blocking the producer thread; drop if queue is temporarily full.
        (void)s_ring.try_push(m_pendingChunk);
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

    del_fvec(input);
    del_aubio_source(src);

    // Stop the app once playback reaches EOF.
    m_running.store(false);
}

void App::filePlaybackLoop() {
    if (Pa_Initialize() != paNoError) {
        std::cerr << "Warning: PortAudio output initialization failed; trying SDL audio backend\n";
        if (!playFileWithSDLAudio(m_audioFilePath, m_running)) {
            std::cerr
                << "Warning: no usable audio playback backend; continuing without audio output\n";
        }
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
            (void)playFileWithSDLAudio(m_audioFilePath, m_running);
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
        out.suggestedLatency = devInfo->defaultLowOutputLatency;

        if (!m_outputDeviceQuery.empty()) {
            std::cerr << "Info: using output device '" << devInfo->name << "'\n";
        }

        PaError err = Pa_OpenStream(&stream, nullptr, &out, kSampleRate, kCaptureFrames, paClipOff,
                                    nullptr, nullptr);
        if (err != paNoError) {
            std::cerr << "Warning: failed to open PortAudio output stream: " << Pa_GetErrorText(err)
                      << "; trying SDL audio backend\n";
            (void)playFileWithSDLAudio(m_audioFilePath, m_running);
            break;
        }
        streamOpen = true;

        err = Pa_StartStream(stream);
        if (err != paNoError) {
            std::cerr << "Warning: failed to start PortAudio output stream: "
                      << Pa_GetErrorText(err) << "; trying SDL audio backend\n";
            (void)playFileWithSDLAudio(m_audioFilePath, m_running);
            break;
        }

        aubio_source_t* src =
            new_aubio_source(m_audioFilePath.c_str(), kSampleRate, kCaptureFrames);
        if (!src) {
            std::cerr << "Warning: failed to open audio file for playback: " << m_audioFilePath
                      << "\n";
            break;
        }

        fvec_t* input = new_fvec(kCaptureFrames);
        if (!input) {
            del_aubio_source(src);
            std::cerr << "Warning: failed to allocate playback buffer\n";
            break;
        }

        std::array<float, kCaptureFrames * 2> stereo{};

        while (m_running.load()) {
            uint_t read = 0;
            aubio_source_do(src, input, &read);
            if (read == 0)
                break;

            for (uint_t i = 0; i < read; ++i) {
                const float s = input->data[i];
                stereo[i * 2] = s;
                stereo[i * 2 + 1] = s;
            }
            for (uint_t i = read; i < static_cast<uint_t>(kCaptureFrames); ++i) {
                stereo[i * 2] = 0.0f;
                stereo[i * 2 + 1] = 0.0f;
            }

            const PaError writeErr = Pa_WriteStream(stream, stereo.data(), kCaptureFrames);
            if (writeErr != paNoError) {
                std::cerr << "Warning: audio playback write failed: " << Pa_GetErrorText(writeErr)
                          << "\n";
                break;
            }

            if (read < static_cast<uint_t>(kCaptureFrames))
                break;
        }

        del_fvec(input);
        del_aubio_source(src);
    } while (false);

    if (streamOpen) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
    }
    Pa_Terminate();
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
