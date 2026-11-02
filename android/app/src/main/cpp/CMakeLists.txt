cmake_minimum_required(VERSION 3.22.1)
project(gdcpp_player LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../engine)
file(GLOB ENGINE_SRC ${ENGINE_DIR}/src/*.cpp)

set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
add_library(native_app_glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${APP_GLUE_DIR})

add_library(gdcpp_player SHARED android_main.cpp ${ENGINE_SRC})
target_include_directories(gdcpp_player PRIVATE ${ENGINE_DIR}/include)
target_compile_definitions(gdcpp_player PRIVATE GDCPP_GLES=1)
target_compile_options(gdcpp_player PRIVATE -O2 -Wall)
set_target_properties(gdcpp_player PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")
target_link_libraries(gdcpp_player native_app_glue android EGL GLESv3 jnigraphics log)
