add_library(dv_audio audio_capture.cpp)
target_include_directories(dv_audio PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dv_audio PUBLIC PortAudio::PortAudio SPSCQueue)
