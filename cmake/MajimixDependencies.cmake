include_guard(GLOBAL)

include(FetchContent)

macro(majimix_fetchcontent_makeavailable_noinstall dependency_name)
  set(_majimix_saved_skip_install_rules "${CMAKE_SKIP_INSTALL_RULES}")
  set(CMAKE_SKIP_INSTALL_RULES ON)
  FetchContent_MakeAvailable(${dependency_name})
  set(CMAKE_SKIP_INSTALL_RULES "${_majimix_saved_skip_install_rules}")
endmacro()

function(majimix_create_pkg_config_interface canonical_target pkg_config_target wrapper_target)
  if(TARGET ${canonical_target})
    return()
  endif()

  add_library(${wrapper_target} INTERFACE)
  target_link_libraries(${wrapper_target} INTERFACE ${pkg_config_target})
  add_library(${canonical_target} ALIAS ${wrapper_target})
endfunction()

include("${PROJECT_SOURCE_DIR}/dependencies/CMakeLists.txt")

if(MAJIMIX_USE_BUNDLED_DEPS)
  message(STATUS "Majimix: using bundled Ogg, Vorbis and PortAudio dependencies")

  FetchContent_Declare(
    ogg
    URL "https://github.com/xiph/ogg/archive/refs/tags/v1.3.6.tar.gz"
    URL_HASH "SHA256=95b643da661155d79db9de2fca55daed3a8d491039829def246aacb3d9201c81"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  set(_majimix_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
  set(_majimix_saved_install_docs "${INSTALL_DOCS}")
  set(_majimix_saved_install_pkg_config_module "${INSTALL_PKG_CONFIG_MODULE}")
  set(_majimix_saved_install_cmake_package_module "${INSTALL_CMAKE_PACKAGE_MODULE}")
  set(BUILD_SHARED_LIBS OFF)
  set(INSTALL_DOCS OFF)
  set(INSTALL_PKG_CONFIG_MODULE OFF)
  set(INSTALL_CMAKE_PACKAGE_MODULE OFF)
  majimix_fetchcontent_makeavailable_noinstall(ogg)
  file(WRITE "${ogg_BINARY_DIR}/cmake_install.cmake" "")
  set(Ogg_DIR "${ogg_BINARY_DIR}")
  set(BUILD_SHARED_LIBS "${_majimix_saved_build_shared_libs}")
  set(INSTALL_DOCS "${_majimix_saved_install_docs}")
  set(INSTALL_PKG_CONFIG_MODULE "${_majimix_saved_install_pkg_config_module}")
  set(INSTALL_CMAKE_PACKAGE_MODULE "${_majimix_saved_install_cmake_package_module}")

  if(TARGET ogg AND NOT TARGET Ogg::ogg)
    add_library(Ogg::ogg ALIAS ogg)
  endif()

  FetchContent_Declare(
    vorbis
    URL "https://github.com/xiph/vorbis/archive/refs/tags/v1.3.7.tar.gz"
    URL_HASH "SHA256=270c76933d0934e42c5ee0a54a36280e2d87af1de3cc3e584806357e237afd13"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  set(_majimix_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
  set(_majimix_saved_install_cmake_package_module "${INSTALL_CMAKE_PACKAGE_MODULE}")
  set(_majimix_saved_cmake_prefix_path "${CMAKE_PREFIX_PATH}")
  set(_majimix_saved_prefer_config "${CMAKE_FIND_PACKAGE_PREFER_CONFIG}")
  set(BUILD_SHARED_LIBS OFF)
  set(INSTALL_CMAKE_PACKAGE_MODULE OFF)
  list(PREPEND CMAKE_PREFIX_PATH "${ogg_BINARY_DIR}")
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)
  majimix_fetchcontent_makeavailable_noinstall(vorbis)
  file(WRITE "${vorbis_BINARY_DIR}/cmake_install.cmake" "")
  set(BUILD_SHARED_LIBS "${_majimix_saved_build_shared_libs}")
  set(INSTALL_CMAKE_PACKAGE_MODULE "${_majimix_saved_install_cmake_package_module}")
  set(CMAKE_PREFIX_PATH "${_majimix_saved_cmake_prefix_path}")
  set(CMAKE_FIND_PACKAGE_PREFER_CONFIG "${_majimix_saved_prefer_config}")

  if(TARGET vorbis AND NOT TARGET Vorbis::vorbis)
    add_library(Vorbis::vorbis ALIAS vorbis)
  endif()

  if(TARGET vorbisfile AND NOT TARGET Vorbis::vorbisfile)
    add_library(Vorbis::vorbisfile ALIAS vorbisfile)
  endif()

  FetchContent_Declare(
    portaudio
    URL "https://github.com/PortAudio/portaudio/archive/refs/tags/v19.7.0.tar.gz"
    URL_HASH "SHA256=5af29ba58bbdbb7bbcefaaecc77ec8fc413f0db6f4c4e286c40c3e1b83174fa0"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  set(_majimix_saved_pa_build_static "${PA_BUILD_STATIC}")
  set(_majimix_saved_pa_build_shared "${PA_BUILD_SHARED}")
  set(_majimix_saved_pa_build_tests "${PA_BUILD_TESTS}")
  set(_majimix_saved_pa_build_examples "${PA_BUILD_EXAMPLES}")
  set(_majimix_saved_pa_use_asio "${PA_USE_ASIO}")
  set(_majimix_saved_pa_use_jack "${PA_USE_JACK}")
  set(_majimix_saved_pa_disable_install "${PA_DISABLE_INSTALL}")
  set(PA_BUILD_STATIC ON)
  set(PA_BUILD_SHARED OFF)
  set(PA_BUILD_TESTS OFF)
  set(PA_BUILD_EXAMPLES OFF)
  set(PA_USE_ASIO OFF)
  set(PA_USE_JACK OFF)
  set(PA_DISABLE_INSTALL ON)
  majimix_fetchcontent_makeavailable_noinstall(portaudio)
  file(WRITE "${portaudio_BINARY_DIR}/cmake_install.cmake" "")
  set(PA_BUILD_STATIC "${_majimix_saved_pa_build_static}")
  set(PA_BUILD_SHARED "${_majimix_saved_pa_build_shared}")
  set(PA_BUILD_TESTS "${_majimix_saved_pa_build_tests}")
  set(PA_BUILD_EXAMPLES "${_majimix_saved_pa_build_examples}")
  set(PA_USE_ASIO "${_majimix_saved_pa_use_asio}")
  set(PA_USE_JACK "${_majimix_saved_pa_use_jack}")
  set(PA_DISABLE_INSTALL "${_majimix_saved_pa_disable_install}")

  if(TARGET portaudio_static AND NOT TARGET PortAudio::portaudio)
    add_library(PortAudio::portaudio ALIAS portaudio_static)
  elseif(TARGET portaudio AND NOT TARGET PortAudio::portaudio)
    add_library(PortAudio::portaudio ALIAS portaudio)
  endif()
else()
  message(STATUS "Majimix: using system Ogg, Vorbis and PortAudio dependencies")

  find_package(Ogg QUIET CONFIG)
  if(NOT TARGET Ogg::ogg)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OGG REQUIRED IMPORTED_TARGET ogg)
    majimix_create_pkg_config_interface(Ogg::ogg PkgConfig::OGG majimix_system_ogg)
  endif()

  find_package(Vorbis QUIET CONFIG)
  if(NOT TARGET Vorbis::vorbis)
    if(NOT PKG_CONFIG_FOUND)
      find_package(PkgConfig REQUIRED)
    endif()
    pkg_check_modules(VORBIS REQUIRED IMPORTED_TARGET vorbis)
    majimix_create_pkg_config_interface(Vorbis::vorbis PkgConfig::VORBIS majimix_system_vorbis)
  endif()

  if(NOT TARGET Vorbis::vorbisfile)
    if(NOT PKG_CONFIG_FOUND)
      find_package(PkgConfig REQUIRED)
    endif()
    pkg_check_modules(VORBISFILE REQUIRED IMPORTED_TARGET vorbisfile)
    majimix_create_pkg_config_interface(Vorbis::vorbisfile PkgConfig::VORBISFILE majimix_system_vorbisfile)
  endif()

  find_package(PortAudio QUIET CONFIG)
  if(TARGET PortAudio::portaudio)
    return()
  endif()

  if(TARGET portaudio AND NOT TARGET PortAudio::portaudio)
    add_library(PortAudio::portaudio ALIAS portaudio)
  elseif(TARGET portaudio_static AND NOT TARGET PortAudio::portaudio)
    add_library(PortAudio::portaudio ALIAS portaudio_static)
  else()
    if(NOT PKG_CONFIG_FOUND)
      find_package(PkgConfig REQUIRED)
    endif()
    pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)
    majimix_create_pkg_config_interface(PortAudio::portaudio PkgConfig::PORTAUDIO majimix_system_portaudio)
  endif()
endif()