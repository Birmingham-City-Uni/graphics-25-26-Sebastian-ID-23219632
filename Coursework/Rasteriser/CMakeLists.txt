cmake_minimum_required(VERSION 3.27.1)

project(Rasteriser)

add_subdirectory(3rdParty)
include_directories(3rdParty/lodepng)

add_executable(Rasteriser
    Rasteriser.cpp)

target_link_libraries(Rasteriser
    lodepng
)

target_compile_features(Rasteriser PRIVATE cxx_std_17)