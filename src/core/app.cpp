#include "core/app.hpp"

#include "analysis/analyzer.hpp"
#include "audio/audio_capture.hpp"
#include "audio/ring_buffer.hpp"
#include "core/audio_state.hpp"
#include "render/renderer.hpp"

#include <aubio/aubio.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace dv {

// Module-level singletons owned by App (one per process)
static RingBuffer s_ring{8};
static SharedAudioState s_state;

App::App(std::string audioFilePath) : m_audioFilePath(std::move(audioFilePath)) {
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
        } else {
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

void App::shutdownSubsystems() {
    m_running.store(false);
    if (m_capture)
        m_capture->stop();
    stopFileSource();
    if (m_analyzer)
        m_analyzer->stop();
}

} // namespace dv
