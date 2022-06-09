
message (STATUS "Check for Java")
find_package(Java REQUIRED)

if (JAVA_FOUND)
  message(STATUS "Found Java_JAVA_EXECUTABLE = ${Java_JAVA_EXECUTABLE}")
  message(STATUS "Found Java_JAVAC_EXECUTABLE = ${Java_JAVAC_EXECUTABLE}")
  message(STATUS "Found Java_JAVAH_EXECUTABLE = ${Java_JAVAH_EXECUTABLE}")
  message(STATUS "Found Java_JAVADOC_EXECUTABLE = ${Java_JAVADOC_EXECUTABLE}")
  message(STATUS "Found Java_VERSION_STRING = ${Java_VERSION_STRING}")
  message(STATUS "Found Java_VERSION = ${Java_VERSION}")
endif ()

include(UseJava)

find_package(JNI REQUIRED)
list (APPEND linked_jni_libs ${JNI_LIBRARIES})

file (GLOB JAVA_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.java")
add_jar(org_openslide
  SOURCES "${JAVA_SRC}"
  MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/MANIFEST.MF"
  GENERATE_NATIVE_HEADERS openslide_jni_headers
)

set (OPENSLIDE_JAVA openslide-jni)

add_library(${OPENSLIDE_JAVA} "${CMAKE_CURRENT_SOURCE_DIR}/src/openslide-jni.c")

target_include_directories(${OPENSLIDE_JAVA} PRIVATE ${JNI_INCLUDE_DIRS})
target_include_directories(${OPENSLIDE_JAVA} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/")
target_include_directories(${OPENSLIDE_JAVA} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")

target_link_directories(${OPENSLIDE_JAVA} PRIVATE ${JNI_INCLUDE_DIRS})

target_link_libraries(${OPENSLIDE_JAVA} PUBLIC ${openslidelib} ${linked_jni_libs})
target_link_libraries(${OPENSLIDE_JAVA} PRIVATE openslide_jni_headers)

install(TARGETS ${OPENSLIDE_JAVA}
  RUNTIME DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin/"
  LIBRARY DESTINATION "${CMAKE_CURRENT_LIST_DIR}/lib/"
  ARCHIVE DESTINATION "${CMAKE_CURRENT_LIST_DIR}/lib/"
  COMPONENT dev
)

install_jar(org_openslide
  DESTINATION "${CMAKE_CURRENT_LIST_DIR}/bin/"
)
