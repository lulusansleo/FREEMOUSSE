add_library(dv_core app.cpp)
target_include_directories(dv_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dv_core PUBLIC dv_audio dv_analysis dv_render)
