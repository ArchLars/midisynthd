cmake_minimum_required(VERSION 3.12)

# Project definition
project(midisynthd
    VERSION 1.0.0
    DESCRIPTION "System-level MIDI Synthesizer Daemon for Linux"
    HOMEPAGE_URL "https://github.com/ArchLars/midisynthd"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type and compiler flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration")

# Enable position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Installation directories
include(GNUInstallDirs)
set(MIDISYNTHD_SYSCONFDIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}")
set(MIDISYNTHD_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}")
set(MIDISYNTHD_LIBDIR "${CMAKE_INSTALL_FULL_LIBDIR}")

# Configuration options
option(ENABLE_SYSTEMD "Enable systemd integration" ON)
option(ENABLE_TESTS "Build unit tests" ON)
option(ENABLE_STATIC_LINKING "Link libraries statically where possible" OFF)

# Find required packages
find_package(PkgConfig REQUIRED)

# FluidSynth - Required
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth>=2.0.0)
if(NOT FLUIDSYNTH_FOUND)
    message(FATAL_ERROR "FluidSynth library not found. Please install libfluidsynth-dev or fluidsynth-devel")
endif()

message(STATUS "Found FluidSynth: ${FLUIDSYNTH_VERSION}")

# ALSA - Required
pkg_check_modules(ALSA REQUIRED alsa>=1.0.16)
if(NOT ALSA_FOUND)
    message(FATAL_ERROR "ALSA library not found. Please install libasound2-dev or alsa-lib-devel")
endif()

message(STATUS "Found ALSA: ${ALSA_VERSION}")

# systemd - Optional
set(HAVE_SYSTEMD FALSE)
if(ENABLE_SYSTEMD)
    pkg_check_modules(SYSTEMD libsystemd)
    if(SYSTEMD_FOUND)
        set(HAVE_SYSTEMD TRUE)
        message(STATUS "Found systemd: ${SYSTEMD_VERSION}")
        
        # Get systemd directories for service file installation
        pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
        pkg_get_variable(SYSTEMD_USER_UNIT_DIR systemd systemduserunitdir)
        
        if(NOT SYSTEMD_SYSTEM_UNIT_DIR)
            set(SYSTEMD_SYSTEM_UNIT_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/systemd/system")
        endif()
        
        if(NOT SYSTEMD_USER_UNIT_DIR)
            set(SYSTEMD_USER_UNIT_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/systemd/user")
        endif()
    else()
        message(STATUS "systemd not found - building without systemd integration")
    endif()
endif()

# Check for math library (needed on some systems)
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    set(MATH_LIBRARIES ${MATH_LIBRARY})
endif()

# Check for pthread library
find_package(Threads REQUIRED)

# Check for realtime library (needed for some audio systems)
find_library(RT_LIBRARY rt)
if(RT_LIBRARY)
    set(RT_LIBRARIES ${RT_LIBRARY})
endif()

# Configuration header generation
configure_file(
    "${CMAKE_SOURCE_DIR}/config.h.in"
    "${CMAKE_BINARY_DIR}/config.h"
    @ONLY
)

# Include directories
include_directories(
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_BINARY_DIR}"
    ${FLUIDSYNTH_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
)

if(HAVE_SYSTEMD)
    include_directories(${SYSTEMD_INCLUDE_DIRS})
endif()

# Compiler definitions
add_definitions(-DPACKAGE_NAME="${PROJECT_NAME}")
add_definitions(-DPACKAGE_VERSION="${PROJECT_VERSION}")
add_definitions(-DCONFIG_SYSTEM_PATH="${MIDISYNTHD_SYSCONFDIR}/${PROJECT_NAME}.conf")
add_definitions(-DCONFIG_USER_PATH="/.config/${PROJECT_NAME}.conf")
add_definitions(-DCONFIG_DEFAULT_SOUNDFONT_PATH="${MIDISYNTHD_DATADIR}/soundfonts")
add_definitions(-DCONFIG_DEFAULT_CLIENT_NAME="${PROJECT_NAME}")

if(HAVE_SYSTEMD)
    add_definitions(-DHAVE_SYSTEMD=1)
endif()

# Source files
set(MIDISYNTHD_SOURCES
    src/main.c
    src/config.c
    src/synth.c
    src/midi_alsa.c
    src/audio.c
    src/daemonize.c
)

# Header files
set(MIDISYNTHD_HEADERS
    src/config.h
    src/synth.h
    src/midi_alsa.h
    src/audio.h
    src/daemonize.h
)

# Create the main executable
add_executable(${PROJECT_NAME} ${MIDISYNTHD_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${FLUIDSYNTH_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${MATH_LIBRARIES}
    ${RT_LIBRARIES}
    Threads::Threads
)

if(HAVE_SYSTEMD)
    target_link_libraries(${PROJECT_NAME} ${SYSTEMD_LIBRARIES})
endif()

# Add library directories
target_link_directories(${PROJECT_NAME} PRIVATE
    ${FLUIDSYNTH_LIBRARY_DIRS}
    ${ALSA_LIBRARY_DIRS}
)

if(HAVE_SYSTEMD)
    target_link_directories(${PROJECT_NAME} PRIVATE ${SYSTEMD_LIBRARY_DIRS})
endif()

# Compiler flags for libraries
target_compile_options(${PROJECT_NAME} PRIVATE
    ${FLUIDSYNTH_CFLAGS_OTHER}
    ${ALSA_CFLAGS_OTHER}
)

if(HAVE_SYSTEMD)
    target_compile_options(${PROJECT_NAME} PRIVATE ${SYSTEMD_CFLAGS_OTHER})
endif()

# Installation targets
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime
)

# Install configuration files
install(FILES config/${PROJECT_NAME}.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
    COMPONENT Configuration
)

install(FILES config/${PROJECT_NAME}.conf.example
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT Documentation
)

# Install systemd service files
if(HAVE_SYSTEMD)
    install(FILES systemd/${PROJECT_NAME}.service
        DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR}
        COMPONENT SystemdServices
    )
    
    install(FILES systemd/${PROJECT_NAME}-user.service
        DESTINATION ${SYSTEMD_USER_UNIT_DIR}
        COMPONENT SystemdServices
    )
endif()

# Install documentation
install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT Documentation
)

# Create directories for soundfonts (optional)
install(DIRECTORY DESTINATION ${MIDISYNTHD_DATADIR}/soundfonts
    COMPONENT Runtime
)

# Testing support
if(ENABLE_TESTS)
    enable_testing()
    
    # Check for test framework
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CMOCKA cmocka>=1.1.0)
    
    if(CMOCKA_FOUND)
        message(STATUS "Found CMocka: ${CMOCKA_VERSION}")
        
        # Add tests subdirectory
        add_subdirectory(tests)
    else()
        message(WARNING "CMocka not found - tests will not be built")
        message(STATUS "Install libcmocka-dev or cmocka-devel to enable testing")
    endif()
endif()

# Uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
endif()

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "ArchLars")
set(CPACK_PACKAGE_CONTACT "ArchLars")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Debian packaging
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfluidsynth3 (>= 2.0.0), libasound2 (>= 1.0.16)")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fluid-soundfont-gm")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# RPM packaging
set(CPACK_RPM_PACKAGE_REQUIRES "fluidsynth >= 2.0.0, alsa-lib >= 1.0.16")
set(CPACK_RPM_PACKAGE_SUGGESTS "fluid-soundfont-gm")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_LICENSE "LGPL-2.1")

if(HAVE_SYSTEMD)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, libsystemd0")
    set(CPACK_RPM_PACKAGE_REQUIRES "${CPACK_RPM_PACKAGE_REQUIRES}, systemd-libs")
endif()

include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Project:              ${PROJECT_NAME} ${PROJECT_VERSION}")
message(STATUS "  Build type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix:       ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  C compiler:           ${CMAKE_C_COMPILER}")
message(STATUS "  C flags:              ${CMAKE_C_FLAGS}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "  C flags (Debug):      ${CMAKE_C_FLAGS_DEBUG}")
else()
    message(STATUS "  C flags (Release):    ${CMAKE_C_FLAGS_RELEASE}")
endif()
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  FluidSynth:           ${FLUIDSYNTH_VERSION}")
message(STATUS "  ALSA:                 ${ALSA_VERSION}")
if(HAVE_SYSTEMD)
    message(STATUS "  systemd:              ${SYSTEMD_VERSION}")
else()
    message(STATUS "  systemd:              disabled")
endif()
message(STATUS "")
message(STATUS "Features:")
message(STATUS "  systemd integration:  ${ENABLE_SYSTEMD}")
message(STATUS "  Unit tests:           ${ENABLE_TESTS}")
message(STATUS "  Static linking:       ${ENABLE_STATIC_LINKING}")
message(STATUS "")
message(STATUS "Installation paths:")
message(STATUS "  Binaries:             ${CMAKE_INSTALL_FULL_BINDIR}")
message(STATUS "  Config files:         ${CMAKE_INSTALL_FULL_SYSCONFDIR}")
message(STATUS "  Documentation:        ${CMAKE_INSTALL_FULL_DOCDIR}")
if(HAVE_SYSTEMD)
    message(STATUS "  System units:         ${SYSTEMD_SYSTEM_UNIT_DIR}")
    message(STATUS "  User units:           ${SYSTEMD_USER_UNIT_DIR}")
endif()
message(STATUS "")
