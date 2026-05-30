cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

project(majimix VERSION 0.5 DESCRIPTION "Simple audio mixer" LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

option(MAJIMIX_USE_BUNDLED_DEPS "Download and build codec and audio dependencies automatically" ON)
option(MAJIMIX_INSTALL_PKG_CONFIG "Install the pkg-config metadata on Unix platforms" ${UNIX})
option(MAJIMIX_ENABLE_WARNINGS "Enable compiler warnings for Majimix sources" ON)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(MAJIMIX_LIB_NAME "${PROJECT_NAME}_pa")
set(MAJIMIX_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
set(MAJIMIX_CONFIGURED_HEADER "${MAJIMIX_GENERATED_INCLUDE_DIR}/majimix/majimix.hpp")
set(MAJIMIX_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/Majimix")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/majimix.hpp.in"
    "${MAJIMIX_CONFIGURED_HEADER}"
    @ONLY
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MajimixDependencies.cmake")

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(${MAJIMIX_LIB_NAME} SHARED
    src/wave.cpp
    src/kss.cpp
    src/converters.cpp
    src/source_pcm.cpp
    src/source_vorbis.cpp
    src/mixer_buffer.cpp
    src/majimix.cpp
)

add_library(Majimix::pa ALIAS ${MAJIMIX_LIB_NAME})

target_compile_features(${MAJIMIX_LIB_NAME} PUBLIC cxx_std_17)
target_compile_definitions(${MAJIMIX_LIB_NAME}
    PRIVATE
        MAJIMIX_EXPORTS
        MAJIMIX_USE_FLOATING_POINT
    INTERFACE
        $<$<PLATFORM_ID:Windows>:MAJIMIX_IMPORTS>
)

if(MAJIMIX_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(${MAJIMIX_LIB_NAME} PRIVATE /W4)
    else()
        target_compile_options(${MAJIMIX_LIB_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

target_include_directories(${MAJIMIX_LIB_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${MAJIMIX_GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${libkss_SOURCE_DIR}/src"
        "${libkss_SOURCE_DIR}/modules"
)

target_link_libraries(${MAJIMIX_LIB_NAME}
    PRIVATE
        kss
        emu2149
        emu2212
        emu2413
        emu8950
        emu76489
        kmz80
        Threads::Threads
        Vorbis::vorbisfile
        PortAudio::portaudio
)

set_target_properties(${MAJIMIX_LIB_NAME} PROPERTIES
    EXPORT_NAME pa
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

install(TARGETS ${MAJIMIX_LIB_NAME}
    EXPORT MajimixTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES "${MAJIMIX_CONFIGURED_HEADER}"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/majimix"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MajimixConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MajimixConfig.cmake"
    INSTALL_DESTINATION "${MAJIMIX_CMAKE_INSTALL_DIR}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MajimixConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT MajimixTargets
    FILE MajimixTargets.cmake
    NAMESPACE Majimix::
    DESTINATION "${MAJIMIX_CMAKE_INSTALL_DIR}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MajimixConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MajimixConfigVersion.cmake"
    DESTINATION "${MAJIMIX_CMAKE_INSTALL_DIR}"
)

if(UNIX AND MAJIMIX_INSTALL_PKG_CONFIG)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/majimix.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${MAJIMIX_LIB_NAME}.pc"
        @ONLY
    )

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${MAJIMIX_LIB_NAME}.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )
endif()