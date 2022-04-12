cmake_minimum_required(VERSION 3.13)
project(majimix LANGUAGES CXX C VERSION 0.5 DESCRIPTION "Simple audio mixer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -O3")
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -O3 -Wall")

add_definitions(-DMAJIMIX_EXPORTS)
# uses floating point to resample
add_definitions(-DMAJIMIX_USE_FLOATING_POINT)

# Set a default build type if none was specified
set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()


# configure majimix header file
configure_file(
    majimix.hpp.in
    ${CMAKE_SOURCE_DIR}/src/majimix.hpp
)

set(MAJIMIX_LIB_NAME ${PROJECT_NAME}_pa)

add_library(${MAJIMIX_LIB_NAME} SHARED
  src/wave.cpp
  src/kss.cpp
  src/converters.cpp
  src/source_pcm.cpp
  src/source_vorbis.cpp
  src/mixer_buffer.cpp
  src/majimix.cpp
)


# DEPENDENCIES
# ------------

include(FindPkgConfig)

# Vorbisfile
pkg_search_module(VORBISFILE vorbisfile>=1.3 REQUIRED)
if(${VORBISFILE_FOUND})
    message(STATUS "Vobisfile library found")
    message(STATUS "    include dir : ${VORBISFILE_INCLUDEDIR}")
    message(STATUS "           name : ${VORBISFILE_LIBRARIES}")
    message(STATUS "          files : ${VORBISFILE_LINK_LIBRARIES}")
    message(STATUS "        version : ${VORBISFILE_VERSION}")
    
    target_include_directories(${MAJIMIX_LIB_NAME} PRIVATE ${VORBISFILE_INCLUDEDIR})
endif()

# portaudio
pkg_check_modules(PORTAUDIO portaudio-2.0 REQUIRED)
if(${PORTAUDIO_FOUND})
    message(STATUS "PortAudio library found")
    message(STATUS "    include dir : ${PORTAUDIO_INCLUDEDIR}")
    message(STATUS "           name : ${PORTAUDIO_LIBRARIES}")
    message(STATUS "          files : ${PORTAUDIO_LINK_LIBRARIES}")
    message(STATUS "        version : ${PORTAUDIO_VERSION}")
    
    target_include_directories(${MAJIMIX_LIB_NAME} PRIVATE ${PORTAUDIO_INCLUDEDIR})
endif()

# threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# libkss
add_subdirectory(dependencies)
target_include_directories(${MAJIMIX_LIB_NAME} PRIVATE ${libkss_SOURCE_DIR}/src ${libkss_SOURCE_DIR}/modules)


# compile / link options
# ----------------------
target_compile_options(kss PRIVATE -fPIC)
target_compile_options(emu2149 PRIVATE -fPIC)
target_compile_options(emu2212 PRIVATE -fPIC)
target_compile_options(emu2413 PRIVATE -fPIC)
target_compile_options(emu8950 PRIVATE -fPIC)
target_compile_options(emu76489 PRIVATE -fPIC)
target_compile_options(kmz80 PRIVATE -fPIC)


# link
# ----
target_link_libraries(${MAJIMIX_LIB_NAME} kss emu2149 emu2212 emu2413 emu8950 emu76489 kmz80)
target_link_libraries(${MAJIMIX_LIB_NAME} Threads::Threads ${VORBISFILE_LIBRARIES} ${PORTAUDIO_LIBRARIES})


# install
# -------
include(GNUInstallDirs)

# header 
set_target_properties(${MAJIMIX_LIB_NAME} PROPERTIES PUBLIC_HEADER src/majimix.hpp)
# Set the version property.
set_target_properties(${MAJIMIX_LIB_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
# Set the shared object version property to the project's major version.
set_target_properties(${MAJIMIX_LIB_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})

install(TARGETS ${MAJIMIX_LIB_NAME}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${MAJIMIX_LIB_NAME}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

# Create the pkg-config file from the template.
configure_file(${PROJECT_NAME}.pc.in ${MAJIMIX_LIB_NAME}.pc @ONLY)

# Set pkg-config file to install.
install(FILES ${CMAKE_BINARY_DIR}/${MAJIMIX_LIB_NAME}.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)



# option(BUILD_TESTING "" ON)
# # test 
# # ----
# if(${BUILD_TESTING})
#       message(STATUS "prepare test")
#       enable_testing()
#       add_executable(test_dc src/test_dc.cpp)
#       target_compile_options(test_dc PUBLIC "$<$<CONFIG:RELEASE>:${-O3}>")
#       target_link_options(test_dc PUBLIC "$<$<CONFIG:RELEASE>:${-s}>")
#       target_link_libraries(test_dc ${MAJIMIX_LIB_NAME})

#       add_executable(test_ogg src/test_ogg.cpp)
#       target_compile_options(test_ogg PUBLIC "$<$<CONFIG:RELEASE>:${-O3}>")
#       target_link_options(test_ogg PUBLIC "$<$<CONFIG:RELEASE>:${-s}>")
#       target_link_libraries(test_ogg ${MAJIMIX_LIB_NAME})

#       set(RES_FILES 
#       u8-22050-stereo.wav
#       alaw-22050-stereo.wav
#       ulaw-22050-stereo.wav
#       s16-22050-stereo.wav
#       s24-22050-stereo.wav
#       s32-22050-stereo.wav
#       f32-22050-stereo.wav
#       f64-22050-stereo.wav
#       "Excellent !.wav"
#       55239520-D.ogg
#       )
#       FOREACH(rf ${RES_FILES})
#         add_custom_command(
#               TARGET  test_dc POST_BUILD
#               COMMAND ${CMAKE_COMMAND} -E copy
#                       ${CMAKE_SOURCE_DIR}/resources/${rf}
#                       ${CMAKE_CURRENT_BINARY_DIR}/resources/${rf})
#       ENDFOREACH()

#       #target_link_libraries(${MAJIMIX_LIB_TEST} ${MAJIMIX_LIB_NAME})
#     #   add_test(NAME ${MAJIMIX_LIB_TEST} COMMAND ${MAJIMIX_LIB_TEST})
# endif()