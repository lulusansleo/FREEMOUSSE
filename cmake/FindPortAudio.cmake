find_path(PORTAUDIO_INCLUDE_DIR portaudio.h)
find_library(PORTAUDIO_LIBRARY NAMES portaudio)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PortAudio DEFAULT_MSG PORTAUDIO_LIBRARY PORTAUDIO_INCLUDE_DIR)
if(PortAudio_FOUND AND NOT TARGET PortAudio::PortAudio)
  add_library(PortAudio::PortAudio UNKNOWN IMPORTED)
  set_target_properties(PortAudio::PortAudio PROPERTIES
    IMPORTED_LOCATION "${PORTAUDIO_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${PORTAUDIO_INCLUDE_DIR}")
endif()
