cmake_minimum_required(VERSION 3.13)
project(sdl-demo CXX)

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_mixer REQUIRED CONFIG)

add_executable(sdl-demo
   main.cpp
   audio_demo.cpp
   camera_demo.cpp
   input_demo.cpp
   render_utils.cpp
   scene_demo.cpp
   storage_demo.cpp
   time_demo.cpp)

target_compile_features(sdl-demo PRIVATE cxx_std_17)
target_link_libraries(sdl-demo PRIVATE SDL3_mixer::SDL3_mixer SDL3::SDL3)

wut_create_rpx(sdl-demo)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sdl-demo.rpx"
        DESTINATION "${CMAKE_INSTALL_PREFIX}")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/content/"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/content")
