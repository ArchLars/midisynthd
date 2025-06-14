cmake_minimum_required(VERSION 3.12)

project(midisynthd VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "Build unit tests" ON)
option(ENABLE_SYSTEMD "Enable systemd integration" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth)
pkg_check_modules(ALSA REQUIRED alsa)
find_library(MATH_LIB m)
find_library(RT_LIB rt)
find_package(Threads REQUIRED)

# Check for systemd (optional)
set(HAVE_SYSTEMD 0)
if(ENABLE_SYSTEMD)
    pkg_check_modules(SYSTEMD libsystemd)
    if(SYSTEMD_FOUND)
        set(HAVE_SYSTEMD 1)
        message(STATUS "systemd support: enabled")
    else()
        message(STATUS "systemd support: disabled (libsystemd not found)")
    endif()
else()
    message(STATUS "systemd support: disabled (ENABLE_SYSTEMD=OFF)")
endif()

include_directories(${CMAKE_SOURCE_DIR}/src)

# Main executable sources
set(SOURCES
    src/main.c
    src/config.c
    src/synth.c
    src/audio.c
    src/midi_alsa.c
    src/daemonize.c
)

# Create main executable
add_executable(midisynthd ${SOURCES})

target_include_directories(midisynthd PRIVATE
    ${FLUIDSYNTH_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
)

target_link_libraries(midisynthd
    ${FLUIDSYNTH_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${MATH_LIB}
    ${RT_LIB}
    Threads::Threads
)

if(HAVE_SYSTEMD)
    target_compile_definitions(midisynthd PRIVATE HAVE_SYSTEMD)
endif()

# Define HAVE_SYSTEMD for the source code
target_compile_definitions(midisynthd PRIVATE HAVE_SYSTEMD=${HAVE_SYSTEMD})

# Installation
install(TARGETS midisynthd
    RUNTIME DESTINATION bin
)

install(FILES config/midisynthd.conf
    DESTINATION etc
)

if(ENABLE_TESTS)
    enable_testing()
    pkg_check_modules(CMOCKA REQUIRED cmocka)
    add_subdirectory(tests)
endif()
