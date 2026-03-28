#!/usr/bin/env bash
# Install all system dependencies for dj_visualizer
set -e
OS="$(uname)"

if [[ "$OS" == "Darwin" ]]; then
    brew install portaudio fftw aubio sdl2 clang-format
elif [[ -f /etc/debian_version ]]; then
    sudo apt update

    # Ubuntu/Debian typically expose PortAudio headers via portaudio19-dev.
    # Keep compatibility with distros that provide libportaudio-dev.
    # `apt-cache show` can succeed even when package has no install candidate,
    # so parse only the `Candidate:` field from `apt-cache policy`.
    PORTAUDIO_CANDIDATE="$(apt-cache policy libportaudio-dev 2>/dev/null | awk '/Candidate:/ { print $2; exit }')"
    if [[ -n "$PORTAUDIO_CANDIDATE" && "$PORTAUDIO_CANDIDATE" != "(none)" ]]; then
        PORTAUDIO_DEV_PKG="libportaudio-dev"
    else
        PORTAUDIO_DEV_PKG="portaudio19-dev"
    fi

    sudo apt install -y \
        "$PORTAUDIO_DEV_PKG" libfftw3-dev libaubio-dev \
        libsdl2-dev libgl-dev cmake ninja-build clang-format
else
    echo "Unsupported OS — install portaudio, fftw3, aubio, sdl2 manually."
    exit 1
fi

echo ""
echo "Done. Now run: ./scripts/build.sh"
