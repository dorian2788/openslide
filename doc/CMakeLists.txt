
# build C++ APIs with Doxygen

find_package(Doxygen REQUIRED)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile"
  @ONLY
)

# note the option ALL which allows to build the docs together with the application
add_custom_target(doc_doxygen ALL
  COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Generating C++ API documentation with Doxygen"
  VERBATIM
)
