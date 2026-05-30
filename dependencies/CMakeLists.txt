include(FetchContent)

macro(majimix_fetchcontent_makeavailable_noinstall dependency_name)
  set(_majimix_saved_skip_install_rules "${CMAKE_SKIP_INSTALL_RULES}")
  set(CMAKE_SKIP_INSTALL_RULES ON)
  FetchContent_MakeAvailable(${dependency_name})
  set(CMAKE_SKIP_INSTALL_RULES "${_majimix_saved_skip_install_rules}")
endmacro()

FetchContent_Declare(
  libkss
  GIT_REPOSITORY "https://github.com/digital-sound-antiques/libkss.git"
  GIT_TAG "751cd56267ba4eaca6b0e1b45f9184473620aebe"
  GIT_PROGRESS TRUE
  GIT_SUBMODULES_RECURSE TRUE
)

majimix_fetchcontent_makeavailable_noinstall(libkss)
file(WRITE "${libkss_BINARY_DIR}/cmake_install.cmake" "")
