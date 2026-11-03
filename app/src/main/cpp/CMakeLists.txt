cmake_minimum_required(VERSION 3.22.1)
project(nebula LANGUAGES CXX)

add_library(nebula SHARED native-lib.cpp)
target_compile_features(nebula PRIVATE cxx_std_20)
target_compile_options(nebula PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter -ffast-math)
target_link_libraries(nebula GLESv3 EGL android log)
