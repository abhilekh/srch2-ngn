# define the sources for lib-srch2.
#Inside the source directory, to make a header file invisible to build environment, name it in "_${name}.h" format. For example, this applies to test headers like _TrieTest.h

FILE(GLOB SRCH2_LIB_SRC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp" "index/*.cpp" "ranking/*.cpp" "operation/*.cpp" "operation/physical_plan/*.cpp" "postprocessing/*.cpp" "record/*.cpp" "util/*.cpp" "analyzer/*.cpp" "query/*.cpp" "geo/*.cpp" "../../thirdparty/snappy-1.0.4/*.cc" "serialization/*.cpp" "highlighter/*.cpp")

IF(BUILD_JNI)
    IF(ANDROID)
        # This won't find arm architure on say amd arch
        file(TO_CMAKE_PATH "$ENV{JAVA_HOME_ARM}" _JAVA_HOME)
        SET(JNI_FOUND TRUE)
        SET(JNI_INC_DIR "${_JAVA_HOME}/include")
        SET(JNI_LIB "${_JAVA_HOME}/jre/lib/arm")
        SET(JAVA_AWT_LIBRARY "${JNI_LIB}/libjawt.so")
        SET(JAVA_JVM_LIBRARY "${JNI_LIB}/client/libjvm.so")
        SET(JAVA_INCLUDE_PATH "${JNI_INC_DIR}/jni.h")
        SET(JAVA_INCLUDE_PATH2 "${JNI_INC_DIR}/linux/jni_md.h" )
        SET(JAVA_AWT_INCLUDE_PATH "${JNI_INC_DIR}/jawt.h" )

        mark_as_advanced(
          JAVA_AWT_LIBRARY
          JAVA_JVM_LIBRARY
          JAVA_AWT_INCLUDE_PATH
          JAVA_INCLUDE_PATH
          JAVA_INCLUDE_PATH2
        )

        set(JNI_LIBRARIES
          ${JAVA_AWT_LIBRARY}
          ${JAVA_JVM_LIBRARY}
        )

        set(JNI_INCLUDE_DIRS
          ${JAVA_INCLUDE_PATH}
          ${JAVA_INCLUDE_PATH2}
          ${JAVA_AWT_INCLUDE_PATH}
        )
    ELSE()
        find_package(JNI)
    ENDIF()
    FILE(GLOB JNI_FILE RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "../java/jni/*.cpp")
    SET(SRCH2_LIB_SRC ${SRCH2_LIB_SRC} ${JNI_FILE})
    INCLUDE_DIRECTORIES(${JNI_INCLUDE_DIRS})
    SET(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${JNI_LIBARIES})
ENDIF()

# additional includes
INCLUDE_DIRECTORIES(. "${CMAKE_BINARY_DIR}/include" ${Boost_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/thirdparty/json/jsoncpp-src/include)


# generating a shared library if needed.
IF (BUILD_STATIC OR BUILD_SERVER OR ANDROID_SERVER)
  ADD_LIBRARY(srch2_core STATIC ${SRCH2_LIB_SRC} )
ELSE (BUILD_STATIC)
  ADD_LIBRARY(srch2_core SHARED ${SRCH2_LIB_SRC} )
ENDIF()

IF(ENABLE_PROFILER)
    ADD_DEFINITIONS(-DENABLE_PROFILER)
ENDIF()

SET_TARGET_PROPERTIES(srch2_core PROPERTIES COMPILE_FLAGS -fPIC)

IF(ANDROID)
    TARGET_LINK_LIBRARIES(srch2_core ${Boost_LIBRARIES} ${CMAKE_REQUIRED_LIBRARIES})
ELSE()
    TARGET_LINK_LIBRARIES(srch2_core ${Boost_LIBRARIES} ${crypto_LIBRARY} ${CMAKE_REQUIRED_LIBRARIES})
ENDIF()

# install
#INSTALL(TARGETS srch2_core DESTINATION lib)


