
set(TEST_DIR  ${CMAKE_SOURCE_DIR}/test  CACHE PATH "Path where find test files" )

file(GLOB COMMON_HEADERS "${CMAKE_SOURCE_DIR}/common/*.h")
file(GLOB COMMON_SRC "${CMAKE_SOURCE_DIR}/common/*.c")
set (CONFIG_FILE "${CMAKE_SOURCE_DIR}/config.h")

include_directories(${CMAKE_SOURCE_DIR}/common/)
# The root directory is necessary for the config file
include_directories(${CMAKE_SOURCE_DIR}/)

add_custom_target(testing)

add_executable( extended ${COMMON_HEADERS} ${COMMON_SRC} ${TEST_DIR}/extended.c )
target_link_libraries(extended ${linked_libs} ${openslidelib})

add_executable( mosaic ${COMMON_HEADERS} ${COMMON_SRC} ${TEST_DIR}/mosaic.c )
target_link_libraries(mosaic ${linked_libs} ${openslidelib})

add_executable( parallel ${COMMON_HEADERS} ${COMMON_SRC} ${TEST_DIR}/parallel.c )
target_link_libraries(parallel ${linked_libs} ${openslidelib})

add_executable( profile ${COMMON_HEADERS} ${COMMON_SRC} ${TEST_DIR}/profile.c )
target_link_libraries(profile ${linked_libs} ${openslidelib})

add_executable( query ${COMMON_HEADERS} ${COMMON_SRC} ${TEST_DIR}/query.c )
target_link_libraries(query ${linked_libs} ${openslidelib})

add_executable( symlink ${COMMON_HEADERS} ${COMMON_SRC} ${TEST_DIR}/symlink.c )
target_link_libraries(symlink ${linked_libs} ${openslidelib})

add_executable( test ${COMMON_HEADERS} ${COMMON_SRC} ${TEST_DIR}/test.c )
target_link_libraries(test ${linked_libs} ${openslidelib})

add_executable( try_open ${COMMON_HEADERS} ${COMMON_SRC} ${TEST_DIR}/try_open.c )
target_link_libraries(try_open ${linked_libs} ${openslidelib})


add_dependencies(testing extended mosaic parallel profile query symlink test try_open)

