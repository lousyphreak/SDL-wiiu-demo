cmake_minimum_required(VERSION 3.13)
project(sdl-demo CXX)

find_package(SDL3 REQUIRED CONFIG)

add_executable(sdl-demo
   main.cpp)

target_compile_features(sdl-demo PRIVATE cxx_std_17)
target_link_libraries(sdl-demo PRIVATE SDL3::SDL3)

wut_create_rpx(sdl-demo)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sdl-demo.rpx"
        DESTINATION "${CMAKE_INSTALL_PREFIX}")

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/content/"
        DESTINATION "${CMAKE_INSTALL_PREFIX}/content")
