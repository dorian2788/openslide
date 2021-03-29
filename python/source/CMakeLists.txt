# If the pyx file is a C++ file, we should specify that here.

set(PYC_SRC_DIR    ${PYC_DIR}/source       CACHE PATH "Path where find pyx files" )
set(PYC_INC_DIR    ${PYC_DIR}/lib          CACHE PATH "Path where find pxd files" )

set_source_files_properties( ${PYC_SRC_DIR}/openslide.pyx    PROPERTIES CYTHON_IS_CXX TRUE )

# Include *.pxd directory

include_directories(${PYC_INC_DIR})

# Multi-file cython modules do not appear to be working at the moment.

cython_add_module( pyopenslide ${PYC_SRC_DIR}/openslide.pyx ${OPENSLIDE_SRC})
set_target_properties(pyopenslide PROPERTIES OUTPUT_NAME "openslide")
target_link_libraries(pyopenslide ${linked_libs} ${openslidelib})
add_dependencies(pyopenslide ${openslidelib})

install(TARGETS pyopenslide                                           DESTINATION ${PYC_INC_DIR}/)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pyopenslide.cpp             DESTINATION ${PYC_SRC_DIR} RENAME openslide.cpp)

# setup installation

#set(SETUP_PY "${CMAKE_SOURCE_DIR}/setup.py")
#install(CODE "EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} ${SETUP_PY} develop --user)")
