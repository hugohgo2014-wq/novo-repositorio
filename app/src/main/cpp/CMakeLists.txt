cmake_minimum_required(VERSION 3.22.1)
project(nebula360 LANGUAGES CXX)

add_library(nebula360 SHARED spherical_injector.cpp)
target_compile_features(nebula360 PRIVATE cxx_std_17)
target_compile_options(nebula360 PRIVATE -Wall -Wextra -Werror=return-type)
find_library(log-lib log)
target_link_libraries(nebula360 ${log-lib})
