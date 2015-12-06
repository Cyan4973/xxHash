cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)

project(xxhash)

set(XXHASH_LIB_VERSION "0.42.0")
set(XXHASH_LIB_SOVERSION "0")

add_library(xxhash SHARED ../xxhash.c)
set_target_properties(xxhash PROPERTIES COMPILE_DEFINITIONS "XXHASH_EXPORT"
                       VERSION "${XXHASH_LIB_VERSION}"
                       SOVERSION "${XXHASH_LIB_SOVERSION}")
set(install_libs xxhash)

set(BUILD_STATIC_LIBS ON CACHE BOOL "Set to ON to build static libraries")
if(BUILD_STATIC_LIBS)
  add_library(xxhashstatic ../xxhash.c)
  set_target_properties(xxhashstatic PROPERTIES OUTPUT_NAME xxhash)
  LIST(APPEND install_libs xxhashstatic)
endif(BUILD_STATIC_LIBS)


INSTALL(FILES ../xxhash.h DESTINATION include)
INSTALL(TARGETS ${install_libs} DESTINATION lib)
