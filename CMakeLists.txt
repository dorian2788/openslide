cmake_minimum_required (VERSION 3.10.2)
include(CMakeDependentOption)

set (BUILD_SHARED_LIBS "TRUE" CACHE BOOL "Create openslide as a shared library")

set(Openslide_MAJOR_VERSION 3)
set(Openslide_MINOR_VERSION 4)
set(Openslide_PATCH_VERSION 1)
set(Openslide_TWEAK_VERSION 0)

set(Openslide_VERSION ${Openslide_MAJOR_VERSION}.${Openslide_MINOR_VERSION}.${Openslide_PATCH_VERSION}.${Openslide_TWEAK_VERSION})
set (CMAKE_PROJECT_HOMEPAGE_URL "https://github.com/Nico-Curti/openslide")
set (CMAKE_PROJECT_DESCRIPTION "Whole Slide Image file reader")

message (STATUS "Openslide VERSION: ${Openslide_VERSION}")

option(CMAKE_VERBOSE_MAKEFILE "Create verbose makefile" OFF)
option(PYTHON_Openslide "Enable Python wrap compilation" OFF)
option(BUILD_TEST "Enable Testing compilation" OFF)
option(BUILD_DOCS "Enable Documentation build support" OFF)
option(BUILD_JAVA "Enable JAVA build support" OFF)
option(BUILD_JS   "Enable JavaScript build support" OFF)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
  message(STATUS "VCPKG found: $ENV{VCPKG_ROOT}")
  message(STATUS "Using VCPKG integration")
  message(STATUS "VCPKG_MANIFEST_FEATURES: ${VCPKG_MANIFEST_FEATURES}")
  if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
    message(STATUS "Setting default vcpkg target triplet to $ENV{VCPKG_DEFAULT_TRIPLET}")
    set(VCPKG_TARGET_TRIPLET $ENV{VCPKG_DEFAULT_TRIPLET})
  endif()
endif()

enable_language(C)
enable_language(CXX)

if (MSVC)
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /DOpenslide_VERSION=\"${Openslide_VERSION}\"")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /DOpenslide_VERSION=\"${Openslide_VERSION}\"")
else ()
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DOpenslide_VERSION=\"${Openslide_VERSION}\"")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DOpenslide_VERSION=\"${Openslide_VERSION}\"")
endif()

project(Openslide VERSION ${Openslide_VERSION})

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/" ${CMAKE_MODULE_PATH})

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
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Install prefix" FORCE)
endif()

set(INSTALL_BIN_DIR "${CMAKE_CURRENT_LIST_DIR}/bin" CACHE PATH "Path where exe and dll will be installed")
set(INSTALL_LIB_DIR "${CMAKE_CURRENT_LIST_DIR}/lib" CACHE PATH "Path where lib will be installed")

set(INSTALL_INCLUDE_DIR "include/openslide" CACHE PATH "Path where headers will be installed")
set(INSTALL_CMAKE_DIR   "share/openslide"   CACHE PATH "Path where cmake configs will be installed")

set(CMAKE_DEBUG_POSTFIX d)

# FIND required PACKAGES
find_package(TIFF REQUIRED)
include_directories(${TIFF_INCLUDE_DIRS})
link_directories(${TIFF_INCLUDE_DIRS})
list (APPEND linked_libs ${TIFF_LIBRARIES})

find_package(GLIB REQUIRED COMPONENTS gio gobject gthread)
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

set(Pixman_FIND_VERSION ON)
find_package( Pixman )

if (PIXMAN_VERSION VERSION_GREATER "0.38.0" AND PIXMAN_VERSION VERSION_LESS "0.38.4")
  message (WARNING "The detected PIXMAN version could lead to issues in the slide visualization "
                   "(ref. https://github.com/openslide/openslide/issues/291#issuecomment-722935212). "
                   "We sugges to provide a different PIXMAN version. "
                   "For this issue the versions 0.34, 0.36 and 0.40 of libpixman are all good. "
                   "Version 0.38 is the problematic one (0.38.0, 0.38.2, 0.38.4 are all affected). "
                   "Newer versions seems to be all good.")
endif ()

if (MSVC)
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /DCMAKE_BUILD /DG_DISABLE_SINGLE_INCLUDES")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} /DCMAKE_BUILD /DG_DISABLE_SINGLE_INCLUDES")
else ()
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DCMAKE_BUILD -DG_DISABLE_SINGLE_INCLUDES")
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -DCMAKE_BUILD -DG_DISABLE_SINGLE_INCLUDES")
endif ()

set(ADDITIONAL_CXX_FLAGS "-Wall -Wunused-result -Wunknown-pragmas -Wfatal-errors -Wdeprecated-declarations -Wwrite-strings")
set(ADDITIONAL_C_FLAGS "-Wall -Wunused-result -Wunknown-pragmas -Wfatal-errors -Wdeprecated-declarations -Wwrite-strings")

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
endif()


# Make relative paths absolute (needed later on)
foreach (p LIB BIN INCLUDE CMAKE)
  set (var INSTALL_${p}_DIR)
  if (NOT IS_ABSOLUTE "${${var}}")
    set (FULLPATH_${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h"
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/openslide.pc.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/openslide.pc" @ONLY)
message(STATUS "Pkg-config generated")

set (openslidelib openslide)

###########################################################################
# Set configuration variables for compatibility with the master project
include(CheckSymbolExists)
include(CheckIncludeFile)

# Check for macro SEEK_SET
check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_include_file("fcntl.h" HAVE_FCNTL)
check_include_file("stdio.h" HAVE_FSEEKO)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("memory.h" HAVE_MEMORY_H)
check_symbol_exists(proc_pidfdinfo "libproc.h" HAVE_PROC_PIDFDINFO)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file(uintptr_t "stdint.h" HAVE_UINTPTR_T)
check_include_file("unistd.h" HAVE_UNISTD_H)
pkg_check_modules(VALGRIND valgrind)
set(HAVE_VALGRIND ${VALGRIND_FOUND})
set(PACKAGE_BUGREPORT "openslide-users@lists.andrew.cmu.edu")
check_symbol_exists(_wfopen, "stdio.h" HAVE__WFOPEN)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h" @ONLY)

###########################################################################

#look for all *.h files in src folder
file(GLOB OPENSLIDE_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

#look for all *.c files in src folder
file(GLOB OPENSLIDE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM OPENSLIDE_SRC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/make-tables.c"
)

if ( PYTHON_Openslide )

  include( UseCython )
  find_package (Python REQUIRED COMPONENTS Interpreter)
  find_package(NumPy REQUIRED)
  include_directories(${NumPy_INCLUDE_DIRS})
  add_definitions (-DNPY_NO_DEPRECATED_API)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/python/source")

  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/python/lib")
  file(GLOB DEEPZOOM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/python/lib/*.c")
  file(GLOB DEEPZOOM_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/python/lib/*.h")
  list(APPEND OPENSLIDE_SRC ${DEEPZOOM_SRC})
  list(APPEND OPENSLIDE_HEADER ${DEEPZOOM_HEADER})

endif ()

if (BUILD_DOCS)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/doc/")
endif ()

if ( BUILD_TEST )
  if ( MSVC )
    message(WARNING "Windows support for the testing is currently disabled. We are working on it!")
  else ()
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
  endif ()
endif ()

if ( BUILD_JAVA )
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/java/")
endif ()

if ( BUILD_JS )
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/js/")
endif ()

add_library(${openslidelib} SHARED ${OPENSLIDE_SRC} ${OPENSLIDE_HEADER})
set_target_properties(${openslidelib} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(${openslidelib}
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}>
)
target_link_libraries(${openslidelib} PUBLIC ${linked_libs})
target_compile_definitions(${openslidelib} PRIVATE -D_OPENSLIDE_BUILDING_DLL)

set_target_properties(${openslidelib} PROPERTIES PUBLIC_HEADER "${OPENSLIDE_HEADER}")

install(TARGETS ${openslidelib} EXPORT OpenslideTargets
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
file(RELATIVE_PATH REL_INCLUDE_DIR "${FULLPATH_INSTALL_CMAKE_DIR}" "${FULLPATH_INSTALL_INCLUDE_DIR}")
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
