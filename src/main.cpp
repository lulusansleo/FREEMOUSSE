#include "core/app.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string audioFilePath;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: ./build/dj_visualizer [--audio-file <path>]\n";
            return 0;
        }
        if (arg == "--audio-file") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --audio-file\n";
                return 2;
            }
            audioFilePath = argv[++i];
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n";
        std::cerr << "Usage: ./build/dj_visualizer [--audio-file <path>]\n";
        return 2;
    }

    dv::App app(audioFilePath);
    return app.run();
}
