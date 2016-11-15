cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)

project(xxhash)

set(XXHASH_LIB_VERSION "0.42.0")
set(XXHASH_LIB_SOVERSION "0")

option(BUILD_SHARED_LIBS "Set to ON to build shared libraries" ON)
option(BUILD_STATIC_LIBS "Set to ON to build static libraries" ON)

if(BUILD_SHARED_LIBS)
  add_library(xxhash SHARED ../xxhash.c)
  set_target_properties(xxhash PROPERTIES COMPILE_DEFINITIONS "XXHASH_EXPORT"
                         VERSION "${XXHASH_LIB_VERSION}"
                         SOVERSION "${XXHASH_LIB_SOVERSION}")
  LIST(APPEND install_libs xxhash)
endif(BUILD_SHARED_LIBS)

if(BUILD_STATIC_LIBS)
  add_library(xxhashstatic ../xxhash.c)
  set_target_properties(xxhashstatic PROPERTIES OUTPUT_NAME xxhash)
  LIST(APPEND install_libs xxhashstatic)
endif(BUILD_STATIC_LIBS)


INSTALL(FILES ../xxhash.h DESTINATION include)
INSTALL(
    TARGETS ${install_libs}
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
