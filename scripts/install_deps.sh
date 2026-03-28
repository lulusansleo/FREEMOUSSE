#!/usr/bin/env bash
# Install all system dependencies for dj_visualizer
set -e
OS="$(uname)"

if [[ "$OS" == "Darwin" ]]; then
    brew install portaudio fftw aubio sdl2
elif [[ -f /etc/debian_version ]]; then
    sudo apt update
    sudo apt install -y \
        libportaudio-dev libfftw3-dev libaubio-dev \
        libsdl2-dev libgl-dev cmake ninja-build
else
    echo "Unsupported OS — install portaudio, fftw3, aubio, sdl2 manually."
    exit 1
fi

echo ""
echo "Done. Now run: ./scripts/build.sh"
