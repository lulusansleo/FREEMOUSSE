add_library(dv_analysis analyzer.cpp fft.cpp beat_detector.cpp)
target_include_directories(dv_analysis PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dv_analysis PUBLIC FFTW3::FFTW3 Aubio::Aubio)
