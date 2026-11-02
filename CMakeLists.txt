cmake_minimum_required(VERSION 3.16)
project(gdcpp VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Núcleo do engine (portável, sem dependências). O gl_renderer.cpp só compila no Android (GDCPP_GLES).
file(GLOB ENGINE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/engine/src/*.cpp)
add_library(gdcpp_core STATIC ${ENGINE_SRC})
target_include_directories(gdcpp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/engine/include)

enable_testing()
add_executable(gdcpp_tests tests/test_engine.cpp)
target_link_libraries(gdcpp_tests gdcpp_core)
add_test(NAME engine COMMAND gdcpp_tests ${CMAKE_CURRENT_SOURCE_DIR}/android/app/src/main/assets/game/project.json)
