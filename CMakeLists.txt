cmake_minimum_required(VERSION 3.12)

project(midisynthd VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "Build unit tests" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth)
pkg_check_modules(ALSA REQUIRED alsa)
find_library(MATH_LIB m)
find_library(RT_LIB rt)
find_package(Threads REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/src)


if(ENABLE_TESTS)
    enable_testing()
    pkg_check_modules(CMOCKA REQUIRED cmocka)
    add_subdirectory(tests)
endif()
