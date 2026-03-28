find_path(FFTW3_INCLUDE_DIR fftw3.h)

# `fftwf_*` symbols require libfftw3f. Fall back to double precision only
# when single-precision is unavailable.
find_library(FFTW3F_LIBRARY NAMES fftw3f)
find_library(FFTW3_LIBRARY  NAMES fftw3)

set(FFTW3_EFFECTIVE_LIBRARY "${FFTW3F_LIBRARY}")
if(NOT FFTW3_EFFECTIVE_LIBRARY)
  set(FFTW3_EFFECTIVE_LIBRARY "${FFTW3_LIBRARY}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFTW3 DEFAULT_MSG FFTW3_EFFECTIVE_LIBRARY FFTW3_INCLUDE_DIR)
if(FFTW3_FOUND AND NOT TARGET FFTW3::FFTW3)
  add_library(FFTW3::FFTW3 UNKNOWN IMPORTED)
  set_target_properties(FFTW3::FFTW3 PROPERTIES
    IMPORTED_LOCATION "${FFTW3_EFFECTIVE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FFTW3_INCLUDE_DIR}")
endif()
