if(TARGET Ogg::ogg)
  set(Ogg_FOUND TRUE)
  set(OGG_FOUND TRUE)
  return()
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_OGG QUIET ogg)
endif()

find_path(OGG_INCLUDE_DIR
  NAMES ogg/ogg.h
  HINTS ${PC_OGG_INCLUDEDIR} ${PC_OGG_INCLUDE_DIRS}
)

find_library(OGG_LIBRARY
  NAMES ogg
  HINTS ${PC_OGG_LIBDIR} ${PC_OGG_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ogg
  REQUIRED_VARS OGG_LIBRARY OGG_INCLUDE_DIR
  VERSION_VAR PC_OGG_VERSION
)

if(Ogg_FOUND AND NOT TARGET Ogg::ogg)
  add_library(Ogg::ogg UNKNOWN IMPORTED)
  set_target_properties(Ogg::ogg PROPERTIES
    IMPORTED_LOCATION "${OGG_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${OGG_INCLUDE_DIR}"
  )
endif()