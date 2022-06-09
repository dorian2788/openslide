# If the pyx file is a C++ file, we should specify that here.

set_source_files_properties( "${CMAKE_CURRENT_SOURCE_DIR}/openslide.pyx" PROPERTIES CYTHON_IS_CXX TRUE )
set_source_files_properties( "${CMAKE_CURRENT_SOURCE_DIR}/deepzoom.pyx"  PROPERTIES CYTHON_IS_CXX TRUE )

# Include *.pxd directory

include_directories("${CMAKE_SOURCE_DIR}/python/lib")

# Multi-file cython modules do not appear to be working at the moment.

cython_add_module( pyopenslide "${CMAKE_CURRENT_SOURCE_DIR}/openslide.pyx" )
set_target_properties(pyopenslide PROPERTIES OUTPUT_NAME "openslide")
target_link_libraries(pyopenslide ${linked_libs} ${openslidelib})
add_dependencies(pyopenslide ${openslidelib})

cython_add_module( pydeepzoom "${CMAKE_CURRENT_SOURCE_DIR}/deepzoom.pyx" )
set_target_properties(pydeepzoom PROPERTIES OUTPUT_NAME "deepzoom")
target_link_libraries(pydeepzoom ${linked_libs} ${openslidelib})
add_dependencies(pydeepzoom ${openslidelib})


# Installation of targets
install(TARGETS pyopenslide                                 DESTINATION "${CMAKE_SOURCE_DIR}/python/lib")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pyopenslide.cpp" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}" RENAME openslide.cpp)

install(TARGETS pydeepzoom                                  DESTINATION "${CMAKE_SOURCE_DIR}/python/lib")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pydeepzoom.cpp"  DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}" RENAME deepzoom.cpp)

# setup installation

configure_file(
  "${CMAKE_SOURCE_DIR}/setup.py.in"
  "${CMAKE_SOURCE_DIR}/setup_.py" @ONLY)
configure_file(
  "${CMAKE_SOURCE_DIR}/python/__version__.py.in"
  "${CMAKE_SOURCE_DIR}/python/__version__.py" @ONLY)
configure_file(
  "${CMAKE_SOURCE_DIR}/setup.cfg.in"
  "${CMAKE_SOURCE_DIR}/setup.cfg" @ONLY)

# NOTE: the current installation is performed in development mode
add_custom_target(setup_Openslide ALL
                  DEPENDS pyopenslide pydeepzoom
                  COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_SOURCE_DIR}/setup_.py" develop --user
                  #COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_SOURCE_DIR}/setup_.py" install --user
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  COMMENT "Setup Python installation")
