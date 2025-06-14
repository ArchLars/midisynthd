cmake_minimum_required(VERSION 3.16)

project(midisynthd
    VERSION 1.0.0
    DESCRIPTION "System-level MIDI synthesizer daemon for Linux"
    LANGUAGES C
)

# Set C standard and compiler flags
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Add common compiler warnings
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wformat=2
    -Wformat-security
    -Wnull-dereference
    -Wstack-protector
    -Wtrampolines
    -Walloca
    -Wvla
    -Warray-bounds=2
    -Wimplicit-fallthrough=3
    -Wtraditional-conversion
    -Wshift-overflow=2
    -Wcast-qual
    -Wstringop-overflow=4
)

# Enable position independent code for security
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Add security hardening flags
add_compile_options(-fstack-protector-strong)
add_link_options(-Wl,-z,relro,-z,now)

# Include GNUInstallDirs for standard installation directories
include(GNUInstallDirs)

# Find required dependencies
find_package(PkgConfig REQUIRED)

# FluidSynth - Required
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth>=2.0)
if(NOT FLUIDSYNTH_FOUND)
    message(FATAL_ERROR "FluidSynth library not found. Please install libfluidsynth-dev or fluidsynth-devel package.")
endif()

# ALSA - Required
pkg_check_modules(ALSA REQUIRED alsa>=1.0.16)
if(NOT ALSA_FOUND)
    message(FATAL_ERROR "ALSA library not found. Please install libasound2-dev or alsa-lib-devel package.")
endif()

# libconfig - Required for configuration parsing
pkg_check_modules(LIBCONFIG REQUIRED libconfig>=1.4)
if(NOT LIBCONFIG_FOUND)
    message(FATAL_ERROR "libconfig library not found. Please install libconfig-dev or libconfig-devel package.")
endif()

# systemd - Optional for systemd integration
pkg_check_modules(SYSTEMD libsystemd)
if(SYSTEMD_FOUND)
    message(STATUS "systemd support enabled")
    add_compile_definitions(HAVE_SYSTEMD=1)
else()
    message(STATUS "systemd not found - daemon will run without systemd integration")
endif()

# pthread - Required for threading
find_package(Threads REQUIRED)

# Math library
find_library(MATH_LIBRARY m)

# Create main executable
set(SOURCES
    src/main.c
    src/synth.c
    src/midi_alsa.c
    src/audio.c
    src/config.c
    src/daemonize.c
)

set(HEADERS
    src/synth.h
    src/midi_alsa.h
    src/audio.h
    src/config.h
    src/daemonize.h
)

add_executable(midisynthd ${SOURCES} ${HEADERS})

# Set target properties
set_target_properties(midisynthd PROPERTIES
    OUTPUT_NAME midisynthd
    VERSION ${PROJECT_VERSION}
)

# Include directories
target_include_directories(midisynthd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FLUIDSYNTH_INCLUDE_DIRS}
    ${ALSA_INCLUDE_DIRS}
    ${LIBCONFIG_INCLUDE_DIRS}
)

if(SYSTEMD_FOUND)
    target_include_directories(midisynthd PRIVATE ${SYSTEMD_INCLUDE_DIRS})
endif()

# Compiler definitions
target_compile_definitions(midisynthd PRIVATE
    _GNU_SOURCE
    _DEFAULT_SOURCE
    PACKAGE_NAME="${PROJECT_NAME}"
    PACKAGE_VERSION="${PROJECT_VERSION}"
    SYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}"
    SOUNDFONT_DIR="${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/soundfonts"
)

# Link libraries
target_link_libraries(midisynthd
    ${FLUIDSYNTH_LIBRARIES}
    ${ALSA_LIBRARIES}
    ${LIBCONFIG_LIBRARIES}
    Threads::Threads
)

if(SYSTEMD_FOUND)
    target_link_libraries(midisynthd ${SYSTEMD_LIBRARIES})
endif()

if(MATH_LIBRARY)
    target_link_libraries(midisynthd ${MATH_LIBRARY})
endif()

# Add library directories
target_link_directories(midisynthd PRIVATE
    ${FLUIDSYNTH_LIBRARY_DIRS}
    ${ALSA_LIBRARY_DIRS}
    ${LIBCONFIG_LIBRARY_DIRS}
)

if(SYSTEMD_FOUND)
    target_link_directories(midisynthd PRIVATE ${SYSTEMD_LIBRARY_DIRS})
endif()

# Installation targets
install(TARGETS midisynthd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install configuration files
install(FILES config/midisynthd.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}
    COMPONENT config
)

install(FILES config/midisynthd.conf.example
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT documentation
)

# Soundfont directory (users need to install soundfonts separately)
# Recommended: install soundfont-fluid package or download from
# https://archive.org/download/fluidr3-gm-gs/FluidR3_GM_GS.sf2
install(DIRECTORY DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}/soundfonts
    COMPONENT soundfonts
)

# Install systemd service files if systemd is available
if(SYSTEMD_FOUND)
    # Get systemd directories
    pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemdsystemunitdir)
    pkg_get_variable(SYSTEMD_USER_UNIT_DIR systemd systemduserunitdir)
    
    if(SYSTEMD_SYSTEM_UNIT_DIR)
        install(FILES systemd/midisynthd.service
            DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR}
            COMPONENT systemd
        )
    endif()
    
    if(SYSTEMD_USER_UNIT_DIR)
        install(FILES systemd/midisynthd-user.service
            DESTINATION ${SYSTEMD_USER_UNIT_DIR}
            COMPONENT systemd
        )
    endif()
else()
    # Fallback to standard locations if pkg-config doesn't provide them
    install(FILES systemd/midisynthd.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system
        COMPONENT systemd
    )
    
    install(FILES systemd/midisynthd-user.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user
        COMPONENT systemd
    )
endif()

# Install license and documentation
install(FILES LICENSE README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT documentation
)

# Optional: Install public headers if developing a library interface
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/midisynthd.h")
    install(FILES include/midisynthd.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT development
    )
endif()

# Create uninstall target
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

# Testing support
option(BUILD_TESTING "Build the testing tree" ON)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "ArchLars")
set(CPACK_PACKAGE_CONTACT "ArchLars")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# DEB specific settings
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libfluidsynth3 (>= 2.0), libasound2 (>= 1.0.16), libconfig9 (>= 1.4)")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fluid-soundfont-gm | soundfont-fluid")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# RPM specific settings
set(CPACK_RPM_PACKAGE_REQUIRES "fluidsynth >= 2.0, alsa-lib >= 1.0.16, libconfig >= 1.4")
set(CPACK_RPM_PACKAGE_SUGGESTS "fluid-soundfont-gm")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_LICENSE "LGPL-2.1")

include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "midisynthd ${PROJECT_VERSION} build configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  FluidSynth: ${FLUIDSYNTH_VERSION} (${FLUIDSYNTH_LIBRARIES})")
message(STATUS "  ALSA: ${ALSA_VERSION} (${ALSA_LIBRARIES})")
message(STATUS "  libconfig: ${LIBCONFIG_VERSION} (${LIBCONFIG_LIBRARIES})")
if(SYSTEMD_FOUND)
    message(STATUS "  systemd: ${SYSTEMD_VERSION} (${SYSTEMD_LIBRARIES})")
else()
    message(STATUS "  systemd: not found")
endif()
message(STATUS "")
message(STATUS "Installation directories:")
message(STATUS "  Binary: ${CMAKE_INSTALL_FULL_BINDIR}")
message(STATUS "  Config: ${CMAKE_INSTALL_FULL_SYSCONFDIR}")
message(STATUS "  Soundfonts: ${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/soundfonts (directory only - install soundfonts separately)")
message(STATUS "  Documentation: ${CMAKE_INSTALL_FULL_DOCDIR}")
if(SYSTEMD_FOUND AND SYSTEMD_SYSTEM_UNIT_DIR)
    message(STATUS "  Systemd units: ${SYSTEMD_SYSTEM_UNIT_DIR}")
endif()
message(STATUS "")
message(STATUS "NOTE: Soundfonts not included - install soundfont-fluid package")
message(STATUS "      or download from: https://archive.org/download/fluidr3-gm-gs/")
