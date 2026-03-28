find_path(AUBIO_INCLUDE_DIR aubio/aubio.h)
find_library(AUBIO_LIBRARY NAMES aubio)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Aubio DEFAULT_MSG AUBIO_LIBRARY AUBIO_INCLUDE_DIR)
if(Aubio_FOUND AND NOT TARGET Aubio::Aubio)
  add_library(Aubio::Aubio UNKNOWN IMPORTED)
  set_target_properties(Aubio::Aubio PROPERTIES
    IMPORTED_LOCATION "${AUBIO_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${AUBIO_INCLUDE_DIR}")
endif()
