include(FetchContent)

if(NOT DEFINED MAJIMIX_LIBKSS_GIT_TAG)
  set(MAJIMIX_LIBKSS_GIT_TAG "1.2.1")
endif()

FetchContent_Declare(
  libkss
  GIT_REPOSITORY "https://github.com/digital-sound-antiques/libkss"
  GIT_TAG ${MAJIMIX_LIBKSS_GIT_TAG}
)

add_subdirectory(libkss)

#[[
FetchContent_Declare(
  PortAudio
  GIT_REPOSITORY "https://github.com/PortAudio/portaudio"
  GIT_TAG 147dd722548358763a8b649b3e4b41dfffbcfbb6 # V 19.7.0
)
add_subdirectory(portaudio)
]]
