cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)

project(xxhash)

set(XXHASH_LIB_VERSION "0.42.0")
set(XXHASH_LIB_SOVERSION "0")

add_library(xxhash ../xxhash.c)
set_target_properties(xxhash PROPERTIES COMPILE_DEFINITIONS "XXHASH_EXPORT"
                       VERSION "${XXHASH_LIB_VERSION}"
                       SOVERSION "${XXHASH_LIB_SOVERSION}")

INSTALL(FILES ../xxhash.h DESTINATION include)
INSTALL(
    TARGETS xxhash
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
