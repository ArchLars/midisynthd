cmake_minimum_required(VERSION 3.12)

project(midisynthd VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(ENABLE_TESTS "Build unit tests" ON)
option(ENABLE_SYSTEMD "Enable systemd integration" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth)
pkg_check_modules(ALSA REQUIRED alsa)
pkg_check_modules(JACK jack)
find_library(MATH_LIB m)
find_library(RT_LIB rt)
find_package(Threads REQUIRED)
set(HAVE_JACK 0)
if(JACK_FOUND)
    set(HAVE_JACK 1)
    message(STATUS "JACK support: enabled")
else()
    message(STATUS "JACK support: disabled (jack not found)")
endif()

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
if(HAVE_JACK)
    list(APPEND SOURCES src/midi_jack.c)
endif()

# Create main executable
add_executable(midisynthd ${SOURCES})

target_include_directories(midisynthd PRIVATE
    ${FLUIDSYNTH_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
)
if(HAVE_JACK)
    target_include_directories(midisynthd PRIVATE ${JACK_INCLUDE_DIRS})
endif()

target_link_libraries(midisynthd
    ${FLUIDSYNTH_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${MATH_LIB}
    ${RT_LIB}
    Threads::Threads
)
if(HAVE_JACK)
    target_link_libraries(midisynthd ${JACK_LIBRARIES})
endif()

if(HAVE_SYSTEMD)
    target_compile_definitions(midisynthd PRIVATE HAVE_SYSTEMD)
    target_link_libraries(midisynthd ${SYSTEMD_LIBRARIES})
endif()

# Define feature macros
target_compile_definitions(midisynthd PRIVATE HAVE_SYSTEMD=${HAVE_SYSTEMD} HAVE_JACK=${HAVE_JACK})

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
