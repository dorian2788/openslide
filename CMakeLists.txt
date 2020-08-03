cmake_minimum_required (VERSION 3.14.3)

set(Openslide_MAJOR_VERSION 3)
set(Openslide_MINOR_VERSION 4)
set(Openslide_PATCH_VERSION 1)
set(Openslide_TWEAK_VERSION 0)

set(Openslide_VERSION ${Openslide_MAJOR_VERSION}.${Openslide_MINOR_VERSION}.${Openslide_PATCH_VERSION}.${Openslide_TWEAK_VERSION})

if (MSVC)
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /DOpenslide_VERSION=\"${Openslide_VERSION}\"")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /DOpenslide_VERSION=\"${Openslide_VERSION}\"")
else () 
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DOpenslide_VERSION=\"${Openslide_VERSION}\"")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DOpenslide_VERSION=\"${Openslide_VERSION}\"")
endif()

option(CMAKE_VERBOSE_MAKEFILE "Create verbose makefile" OFF)
option(ENABLE_VCPKG_INTEGRATION "Enable VCPKG integration" ON)

if(ENABLE_VCPKG_INTEGRATION AND DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
  message(STATUS "VCPKG found: $ENV{VCPKG_ROOT}")
  message(STATUS "Using VCPKG integration")
endif()

project(Openslide VERSION ${Openslide_VERSION})

if(WIN32 AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(USE_INTEGRATED_LIBS "TRUE" CACHE BOOL "Use libs distributed with this repo")
else()
  set(USE_INTEGRATED_LIBS "FALSE" CACHE BOOL "Use libs distributed with this repo")
endif()

enable_language(C)
enable_language(CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/" ${CMAKE_MODULE_PATH})

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Install prefix" FORCE)
endif()

set(INSTALL_BIN_DIR      "${CMAKE_CURRENT_LIST_DIR}/bin"                 CACHE PATH "Path where exe and dll will be installed")
set(INSTALL_LIB_DIR      "${CMAKE_CURRENT_LIST_DIR}/lib"                 CACHE PATH "Path where lib will be installed")
set(INSTALL_INCLUDE_DIR  "${CMAKE_CURRENT_LIST_DIR}/include/openslide"   CACHE PATH "Path where headers will be installed")
set(INSTALL_CMAKE_DIR    "${CMAKE_CURRENT_LIST_DIR}/share/openslide"     CACHE PATH "Path where cmake configs will be installed")

set(CMAKE_DEBUG_POSTFIX d)

# FIND required PACKAGES
find_package(TIFF REQUIRED)
include_directories(${TIFF_INCLUDE_DIRS})
link_directories(${TIFF_INCLUDE_DIRS})
list (APPEND linked_libs ${TIFF_LIBRARIES})

find_package(GLIB REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_INCLUDE_DIRS})
list (APPEND linked_libs ${GLIB_LIBRARIES})

find_package(Cairo REQUIRED)
include_directories(${Cairo_INCLUDE_DIRS})
link_directories(${Cairo_INCLUDE_DIRS})
list (APPEND linked_libs ${Cairo_LIBRARIES})

find_package(GDK-PixBuf REQUIRED)
include_directories(${GDK-PixBuf_INCLUDE_DIRS})
link_directories(${GDK-PixBuf_INCLUDE_DIRS})
list (APPEND linked_libs ${GDK-PixBuf_LIBRARIES})

find_package(JPEG REQUIRED)
include_directories(${JPEG_INCLUDE_DIRS})
link_directories(${JPEG_INCLUDE_DIRS})
list (APPEND linked_libs ${JPEG_LIBRARY})

find_package(OpenJPEG REQUIRED)
# This is a temporary solution since I have only libopenjpeg2 installed
if (MSVC)
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /DHAVE_OPENJPEG2")
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /DHAVE_OPENJPEG2")
else () 
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DHAVE_OPENJPEG2")
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DHAVE_OPENJPEG2")
endif()
include_directories(${OpenJPEG_INCLUDE_DIR})
link_directories(${OpenJPEG_INCLUDE_DIR})
list (APPEND linked_libs ${OpenJPEG_LIBRARY})

find_package(SQLite3 REQUIRED)
include_directories(${SQLite3_INCLUDE_DIRS})
link_directories(${SQLite3_INCLUDE_DIRS})
list (APPEND linked_libs ${SQLite3_LIBRARIES})

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
link_directories(${LIBXML2_INCLUDE_DIR})
list (APPEND linked_libs ${LIBXML2_LIBRARIES})

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})
link_directories(${ZLIB_INCLUDE_DIR})
list (APPEND linked_libs ${ZLIB_LIBRARIES})

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIR})
link_directories(${PNG_INCLUDE_DIR})
list (APPEND linked_libs ${PNG_LIBRARIES})

find_package(GLIB REQUIRED gio gobject gthread)
include_directories(${GLIB_INCLUDE_DIR})
link_directories(${GLIB_INCLUDE_DIR})
list (APPEND linked_libs ${GLIB_LIBRARIES} ${GLIB_GOBJECT_LIBRARIES} ${GLIB_GIO_LIBRARIES} ${GLIB_GTHREAD_LIBRARIES})


if (MSVC)
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /DCMAKE_BUILD /DG_DISABLE_SINGLE_INCLUDES")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /DCMAKE_BUILD /DG_DISABLE_SINGLE_INCLUDES")
else ()
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DCMAKE_BUILD -DG_DISABLE_SINGLE_INCLUDES")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DCMAKE_BUILD -DG_DISABLE_SINGLE_INCLUDES")
endif ()

set(ADDITIONAL_CXX_FLAGS "-Wall -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -Wno-deprecated-declarations -Wno-write-strings")
set(ADDITIONAL_C_FLAGS "-Wall -Wno-unused-result -Wno-unknown-pragmas -Wfatal-errors -Wno-deprecated-declarations -Wno-write-strings")

if(MSVC)
  set(ADDITIONAL_CXX_FLAGS "/wd4013 /wd4018 /wd4028 /wd4047 /wd4068 /wd4090 /wd4101 /wd4113 /wd4133 /wd4190 /wd4244 /wd4267 /wd4305 /wd4477 /wd4996 /wd4819 /fp:fast")
  set(ADDITIONAL_C_FLAGS "/wd4013 /wd4018 /wd4028 /wd4047 /wd4068 /wd4090 /wd4101 /wd4113 /wd4133 /wd4190 /wd4244 /wd4267 /wd4305 /wd4477 /wd4996 /wd4819 /fp:fast")
  set(CMAKE_CXX_FLAGS "${ADDITIONAL_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${ADDITIONAL_C_FLAGS} ${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "/O2" "/Ox" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
  string(REGEX REPLACE "/O2" "/Ox" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
endif()

if(CMAKE_COMPILER_IS_GNUCC OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${ADDITIONAL_CXX_FLAGS} ${CMAKE_CXX_FLAGS}")
  set(CMAKE_C_FLAGS "${ADDITIONAL_C_FLAGS} ${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "-O0" "-Og" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
  string(REGEX REPLACE "-O3" "-O3" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
  string(REGEX REPLACE "-O0" "-Og" CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
  string(REGEX REPLACE "-O3" "-O3" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
endif()

set(Openslide_INSTALL_INCLUDE_DIR ${INSTALL_INCLUDE_DIR})

configure_file(
  "${CMAKE_CURRENT_LIST_DIR}/src/version.h.in"
  "${CMAKE_CURRENT_LIST_DIR}/src/version.h"
)
#set variables for compatibility with configure.ac
set (prefix ${CMAKE_INSTALL_PREFIX})
set (libdir ${CMAKE_INSTALL_PREFIX}/lib)
set (includedir ${CMAKE_INSTALL_PREFIX}/include)
set (VERSION ${Openslide_VERSION})
configure_file(${CMAKE_SOURCE_DIR}/openslide.pc.in ${CMAKE_SOURCE_DIR}/openslide.pc @ONLY)
message(STATUS "Pkg-config generated")

set (openslidelib openslide)

#look for all *.h files in src folder
file(GLOB headers "${CMAKE_CURRENT_LIST_DIR}/src/*.h")

#look for all *.c files in src folder
file(GLOB sources "${CMAKE_CURRENT_LIST_DIR}/src/*.c")
list(REMOVE_ITEM sources
  ${CMAKE_CURRENT_LIST_DIR}/src/make-tables.c
)

add_library(${openslidelib} ${sources} ${headers})
set_target_properties(${openslidelib} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${openslidelib} PRIVATE ${linked_libs})
target_compile_definitions(${openslidelib} PRIVATE -D_OPENSLIDE_BUILDING_DLL)

install(TARGETS openslide EXPORT OpenslideTargets
  RUNTIME DESTINATION "${INSTALL_BIN_DIR}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
  PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}"
  COMPONENT dev
)

install(EXPORT OpenslideTargets
  FILE OpenslideTargets.cmake
  NAMESPACE Openslide::
  DESTINATION "${INSTALL_CMAKE_DIR}"
)

# Export the package for use from the build-tree (this registers the build-tree with a global CMake-registry)
export(PACKAGE Openslide)

# Create the OpenslideConfig.cmake
# First of all we compute the relative path between the cmake config file and the include path
file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}" "${INSTALL_INCLUDE_DIR}")
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")
configure_file(OpenslideConfig.cmake.in "${PROJECT_BINARY_DIR}/OpenslideConfig.cmake" @ONLY)
set(CONF_INCLUDE_DIRS "\${Openslide_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file(OpenslideConfig.cmake.in "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/OpenslideConfig.cmake" @ONLY)

# Create the OpenslideConfigVersion.cmake
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/OpenslideConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/OpenslideConfig.cmake"
  "${PROJECT_BINARY_DIR}/OpenslideConfigVersion.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}"
)
