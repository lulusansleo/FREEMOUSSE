#include "core/app.hpp"

#include <iostream>
#include <portaudio.h>
#include <string>

namespace {

int listOutputDevices() {
    if (Pa_Initialize() != paNoError) {
        std::cerr << "Failed to initialize PortAudio for device listing\n";
        return 1;
    }

    const int defaultOut = Pa_GetDefaultOutputDevice();
    const int count = Pa_GetDeviceCount();
    if (count < 0) {
        std::cerr << "Failed to query PortAudio devices\n";
        Pa_Terminate();
        return 1;
    }

    std::cout << "Output devices:\n";
    int outputCount = 0;
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info || info->maxOutputChannels <= 0)
            continue;
        ++outputCount;
        std::cout << "  [" << i << "] " << info->name;
        if (i == defaultOut)
            std::cout << "  (default)";
        std::cout << "\n";
    }

    if (outputCount == 0) {
        std::cout << "  (none found)\n";
        std::cout << "PortAudio host APIs visible in this session:\n";
        const int apiCount = Pa_GetHostApiCount();
        for (int i = 0; i < apiCount; ++i) {
            const PaHostApiInfo* api = Pa_GetHostApiInfo(i);
            if (!api)
                continue;
            std::cout << "  - " << api->name << " (devices: " << api->deviceCount << ")\n";
        }
        std::cout << "Hint: if your headphones are connected, run this from your desktop/login "
                     "session (not a headless shell) so PulseAudio/PipeWire devices are visible.\n";
    }

    Pa_Terminate();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string audioFilePath;
    std::string outputDeviceQuery;
    bool playAudio = false;
    bool listDevices = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: ./build/dj_visualizer [--audio-file <path>] [--play-audio] "
                         "[--output-device <name>] [--list-output-devices]\n";
            return 0;
        }
        if (arg == "--list-output-devices") {
            listDevices = true;
            continue;
        }
        if (arg == "--audio-file") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --audio-file\n";
                return 2;
            }
            audioFilePath = argv[++i];
            continue;
        }
        if (arg == "--play-audio") {
            playAudio = true;
            continue;
        }
        if (arg == "--output-device") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --output-device\n";
                return 2;
            }
            outputDeviceQuery = argv[++i];
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        std::cerr << "Usage: ./build/dj_visualizer [--audio-file <path>] [--play-audio] "
                     "[--output-device <name>] [--list-output-devices]\n";
        return 2;
    }

    if (listDevices)
        return listOutputDevices();

    dv::App app(audioFilePath, playAudio, outputDeviceQuery);
    return app.run();
}
