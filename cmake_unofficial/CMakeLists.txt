cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6)

project(xxhash)

set(XXHASH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

file(STRINGS "${XXHASH_DIR}/xxhash.h" XXHASH_VERSION_MAJOR REGEX "^#define XXH_VERSION_MAJOR +([0-9]+) *$")
string(REGEX REPLACE "^#define XXH_VERSION_MAJOR +([0-9]+) *$" "\\1" XXHASH_VERSION_MAJOR "${XXHASH_VERSION_MAJOR}")
file(STRINGS "${XXHASH_DIR}/xxhash.h" XXHASH_VERSION_MINOR REGEX "^#define XXH_VERSION_MINOR +([0-9]+) *$")
string(REGEX REPLACE "^#define XXH_VERSION_MINOR +([0-9]+) *$" "\\1" XXHASH_VERSION_MINOR "${XXHASH_VERSION_MINOR}")
file(STRINGS "${XXHASH_DIR}/xxhash.h" XXHASH_VERSION_RELEASE REGEX "^#define XXH_VERSION_RELEASE +([0-9]+) *$")
string(REGEX REPLACE "^#define XXH_VERSION_RELEASE +([0-9]+) *$" "\\1" XXHASH_VERSION_RELEASE "${XXHASH_VERSION_RELEASE}")
set(XXHASH_VERSION_STRING "${XXHASH_VERSION_MAJOR}.${XXHASH_VERSION_MINOR}.${XXHASH_VERSION_RELEASE}")
set(XXHASH_LIB_VERSION ${XXHASH_VERSION_STRING})
set(XXHASH_LIB_SOVERSION "${XXHASH_VERSION_MAJOR}")
mark_as_advanced(XXHASH_VERSION_MAJOR XXHASH_VERSION_MINOR XXHASH_VERSION_RELEASE XXHASH_VERSION_STRING XXHASH_LIB_VERSION XXHASH_LIB_SOVERSION)

option(BUILD_XXHSUM "Build the xxhsum binary" ON)
option(BUILD_SHARED_LIBS "Build shared library" ON)

# Make CMake's RPATH handling not be insane. This suff has cmake set rpaths appropriately for
# where things end up in the install tree. For some reason that's not the default:
# https://cmake.org/Wiki/CMake_RPATH_handling
SET(CMAKE_SKIP_BUILD_RPATH FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# Where we search for shared libraries
SET(CMAKE_INSTALL_RPATH "./lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

add_library(xxhash ../xxhash.c)
set_target_properties(xxhash PROPERTIES COMPILE_DEFINITIONS "XXHASH_EXPORT"
                       VERSION "${XXHASH_LIB_VERSION}"
                       SOVERSION "${XXHASH_LIB_SOVERSION}")

if (BUILD_XXHSUM)
    add_executable(xxhsum ../xxhsum.c)
    target_link_libraries(xxhsum xxhash)
endif()

INSTALL(FILES ../xxhash.h DESTINATION include)
INSTALL(
    TARGETS xxhash xxhsum
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
