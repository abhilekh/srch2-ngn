FILE(GLOB BUILD_FILE sdk/build.xml)

MESSAGE("${CMAKE_CURRENT_SOURCE_DIR}")
MESSAGE("${SDK_FILES}")

IF(BUILD_STATIC)
  SET(DEBUG "true")
ELSE()
  SET(DEBUG "false")
ENDIF()

IF(ANDROID)
    # This does not work in cmake version 2.8.11, so a hack
    # FIND_PACKAGE(Java)

    SET(Java_JAVA_EXECUTABLE  "$ENV{JAVA_HOME}/bin/java")
    SET(Java_JAR_EXECUTABLE  "$ENV{JAVA_HOME}/bin/jar")
    SET(Java_JAVAC_EXECUTABLE "$ENV{JAVA_HOME}/bin/javac")
    SET(Java_JAVAH_EXECUTABLE "$ENV{JAVA_HOME}/bin/javah")
    SET(Java_JAVADOC_EXECUTABLE "$ENV{JAVA_HOME}/bin/javadoc")

    mark_as_advanced(
      Java_JAVA_EXECUTABLE
      Java_JAR_EXECUTABLE
      Java_JAVAC_EXECUTABLE
      Java_JAVAH_EXECUTABLE
      Java_JAVADOC_EXECUTABLE
      )
ELSE()
    FIND_PACKAGE(Java)
ENDIF()

ADD_CUSTOM_TARGET(ant_build ALL)
ADD_CUSTOM_COMMAND(TARGET ant_build
    COMMAND ant
    ARGS -buildfile ${BUILD_FILE}
    ARGS -Dsrcdir=com/srch2
    ARGS -Dtmpdir=${CMAKE_CURRENT_BINARY_DIR}
    ARGS -Doutfile=${CMAKE_CURRENT_BINARY_DIR}/Srch2SDK.jar
    ARGS -Ddebug=${DEBUG}
    )
