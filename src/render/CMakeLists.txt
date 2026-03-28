add_library(dv_render renderer.cpp shader.cpp)
target_include_directories(dv_render PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(dv_render PUBLIC OpenGL::GL SDL2::SDL2 glm::glm)
